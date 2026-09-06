#!/usr/bin/env python3
"""Fail closed on incomplete Android archives; never equate packaging with play-testing."""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tempfile
import zipfile


def safe_extract(archive, entry, root):
    path = PurePosixPath(entry.filename)
    if path.is_absolute() or ".." in path.parts or "\\" in entry.filename:
        raise ValueError(f"Unsafe archive path: {entry.filename}")
    if entry.file_size > 8 * 1024**3:
        raise ValueError(f"Unexpectedly large archive entry: {entry.filename}")
    target = root.joinpath(*path.parts)
    target.parent.mkdir(parents=True, exist_ok=True)
    with archive.open(entry) as source, target.open("wb") as destination:
        shutil.copyfileobj(source, destination, length=1024**2)
    return target


def extract_containers(archive, root):
    paths = []
    for entry in archive.infolist():
        if not entry.is_dir() and Path(entry.filename).suffix.lower() in {".pak", ".utoc", ".ucas"}:
            paths.append(safe_extract(archive, entry, root))
    return paths


def run_checked(command, log):
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        raise RuntimeError(f"{Path(command[0]).name} failed ({result.returncode}); see {log.name}")
    return log.read_text(encoding="utf-8", errors="replace")


def normalize_content_path(value):
    normalized = value.replace("\\", "/").strip().strip('"')
    marker = "FallOfOran/Content/"
    at = normalized.lower().find(marker.lower())
    return normalized[at + len(marker):].lower() if at >= 0 else None


def required_game_assets(project):
    # Every /Game literal with an object name in runtime C++ must be packaged.
    # Computed paths are additionally covered by checking all source uassets.
    required = {"maps/oran.umap", "fonts/fokufi.ttf"}
    for path in (project / "Source/FallOfOran").rglob("*.cpp"):
        for literal in re.findall(r'"(/Game/[^"]+)"', path.read_text(encoding="utf-8")):
            if "%" not in literal and "." in literal:
                package = literal.split(".", 1)[0]
                required.add(package[len("/Game/"):].lower() + ".uasset")
    for path in (project / "Content").rglob("*"):
        if path.suffix.lower() in {".uasset", ".umap"}:
            relative = path.relative_to(project / "Content").as_posix().lower()
            if not relative.startswith(("developers/", "collections/")):
                required.add(relative)
    return required


def verify(args, report):
    apk = args.apk.resolve()
    project = args.project.resolve()
    unrealpak = args.engine_root.resolve() / "Engine/Binaries/Linux/UnrealPak"
    if not apk.is_file() or not unrealpak.is_file():
        raise ValueError("APK or UnrealPak missing")
    digest = hashlib.sha256()
    with apk.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024**2), b""):
            digest.update(chunk)
    report.update(apk=apk.name, apk_bytes=apk.stat().st_size, sha256=digest.hexdigest())

    badging = run_checked([str(args.aapt), "dump", "badging", str(apk)], args.output / "aapt.log")
    package = re.search(r"^package: name='([^']+)' versionCode='([^']+)' versionName='([^']+)'",
                        badging, re.MULTILINE)
    if not package:
        raise ValueError("Android package metadata not found")
    report["package"], report["version_code"], report["version_name"] = package.groups()
    if package.group(1) != "com.ayoub5550.falloforan":
        raise ValueError("Wrong Android package; refusing to upload a different app")
    if package.group(2) != args.version_code or package.group(3) != args.version_name:
        raise ValueError("APK version does not match requested release")

    with tempfile.TemporaryDirectory(prefix="apk-check-", dir=args.output) as directory:
        root = Path(directory)
        with zipfile.ZipFile(apk) as archive:
            bad_member = archive.testzip()
            if bad_member:
                raise ValueError(f"APK CRC failure: {bad_member}")
            names = archive.namelist()
            native = "lib/arm64-v8a/libUnreal.so"
            if native not in names or archive.getinfo(native).file_size == 0:
                raise ValueError("Missing arm64 Unreal native library")
            report["abis"] = sorted({name.split("/")[1] for name in names
                                     if name.startswith("lib/") and name.endswith(".so")})
            if "assets/main.obb.png" in names:
                obb = safe_extract(archive, archive.getinfo("assets/main.obb.png"), root)
                with zipfile.ZipFile(obb) as payload:
                    bad_member = payload.testzip()
                    if bad_member:
                        raise ValueError(f"Embedded OBB CRC failure: {bad_member}")
                    containers = extract_containers(payload, root / "payload")
            else:
                containers = extract_containers(archive, root / "payload")

        # global.utoc carries script-object metadata, not an indexed file tree;
        # asking UnrealPak to -List it can fail despite a valid game container.
        indexed = [path for path in containers if path.suffix in {".utoc", ".pak"}
                   and path.name.lower() != "global.utoc"]
        if not indexed:
            raise ValueError("No embedded pak/IoStore containers; a self-contained APK is required")
        listing = set()
        report["containers"] = []
        for index, container in enumerate(sorted(indexed)):
            if container.suffix == ".utoc" and not container.with_suffix(".ucas").is_file():
                raise ValueError(f"Missing .ucas data for {container.name}")
            csv_path = args.output / f"container_{index}.csv"
            csv_path.unlink(missing_ok=True)
            log = args.output / f"container_{index}.log"
            text = run_checked([
                str(unrealpak), str(container), "-List", "-extracttomountpoint", f"-csv={csv_path}",
            ], log)
            entries = set()
            if csv_path.exists():
                with csv_path.open(encoding="utf-8-sig", newline="") as stream:
                    reader = csv.reader(stream, skipinitialspace=True)
                    for row in reader:
                        if row:
                            path = normalize_content_path(row[0])
                            if path:
                                entries.add(path)
            for value in re.findall(r'"([^"\n]+)" offset:', text):
                path = normalize_content_path(value)
                if path:
                    entries.add(path)
            listing.update(entries)
            report["containers"].append({"name": container.name, "game_entries": len(entries)})

        required = required_game_assets(project)
        missing = sorted(required - listing)
        report.update(required_entries=len(required), packaged_game_entries=len(listing),
                      missing_assets=missing)
        (args.output / "packaged_game_assets.txt").write_text("\n".join(sorted(listing)) + "\n")
        if missing:
            raise ValueError(f"{len(missing)} required assets absent from packaged content")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--engine-root", type=Path, default=Path(os.environ.get("UE_ROOT", "/work/repos/unrealengine")))
    parser.add_argument("--aapt", type=Path, default=Path("/work/android/sdk/build-tools/36.1.0/aapt"))
    parser.add_argument("--version-code", default="21")
    parser.add_argument("--version-name", default="2.1")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "passed": False, "scope": "archive integrity, arm64 library, package/version, cooked content",
        "device_tested": False,
    }
    try:
        verify(args, report)
        report["passed"] = True
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile) as error:
        report["error"] = str(error)
    (args.output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
