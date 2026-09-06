#!/usr/bin/env python3
"""Sequential Linux logic tests. A clean exit AND expected markers are required.

NullRHI does not test graphics or touch input. Self-test uses synthetic mission
events and the puzzles' DebugSolve hooks, not a human play-through.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time


def active_editors():
    """Refuse overlap; never use broad pkill against another task's process."""
    editors = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            executable = (entry / "exe").resolve().name
            if executable in {"UnrealEditor", "UnrealEditor-Cmd"}:
                editors.append(int(entry.name))
        except OSError:
            continue
    return editors


def stop_owned_process(process):
    if process.poll() is not None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
    except ProcessLookupError:
        process.wait()


def run_level(binary, project, level, mode, output, timeout):
    stdout_file = output / f"{mode}_L{level}.log"
    engine_log = output / f"{mode}_L{level}.engine.log"
    user_dir = output / f"user_L{level}"
    user_dir.mkdir(parents=True, exist_ok=True)
    # An early launch failure must not reuse PASS markers from a previous run.
    engine_log.write_text("", encoding="utf-8")
    args = [
        str(binary), str(project), "-game", "-nullrhi", "-nosound",
        "-unattended", "-nop4", "-stdout", "-FullStdOutLogOutput",
        "-forcelogflush", "-ddc=NoZenLocalFallback", f"-FOLevel={level}",
        f"-abslog={engine_log}", f"-UserDir={user_dir}",
    ]
    args += ["-FOSelfTest", "-FOSelfTestRestart"] if mode == "selftest" else ["-FOShots", "-FOShotMax=2"]
    env = dict(os.environ, LP_NUM_THREADS="1")
    started = time.monotonic()
    timed_out = False
    with stdout_file.open("w", encoding="utf-8") as stream:
        process = subprocess.Popen(
            args, cwd=project.parent, stdout=stream, stderr=subprocess.STDOUT,
            env=env, start_new_session=True,
        )
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            stop_owned_process(process)
        except BaseException:
            stop_owned_process(process)
            raise
        stream.write(f"\nTEST_PROCESS_EXIT={process.returncode}\nTIMEOUT={timed_out}\n")

    text = stdout_file.read_text(encoding="utf-8", errors="replace")
    if engine_log.exists():
        text += "\n" + engine_log.read_text(encoding="utf-8", errors="replace")
    required = [
        "GameInstance: 3 levels", "Building level", "Mission: stage 1/",
        f"SELFTEST PASS: level {level + 1} won" if mode == "selftest" else "Shot 1 requested",
    ]
    if level == 1:
        required.append("Puzzle 'breakers' spawned")
    elif level == 2:
        required.append("Puzzle 'gatecode' spawned")
    if mode == "selftest":
        required += [
            "SELFTEST EXIT OVERLAP:", "SELFTEST RESTART:",
            "SELFTEST HUD CLEANUP PASS:",
        ]
    missing = [marker for marker in required if marker not in text]
    errors = re.findall(
        r"^.*(?:SELFTEST FAIL|Fatal error:|Assertion failed:|Unhandled Exception|"
        r"Error: appError called|Error: === Critical error:).*$",
        text, flags=re.MULTILINE,
    )
    passed = process.returncode == 0 and not timed_out and not missing and not errors
    return {
        "level_index": level, "mode": mode, "passed": passed,
        "exit_code": process.returncode, "timed_out": timed_out,
        "elapsed_seconds": round(time.monotonic() - started, 2),
        "missing_markers": missing, "critical_errors": list(dict.fromkeys(errors))[:20],
        "log": str(stdout_file), "engine_log": str(engine_log),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("selftest", "smoke"), default="selftest")
    parser.add_argument("--levels", nargs="+", type=int, default=[0, 1, 2])
    parser.add_argument("--engine-root", type=Path,
                        default=Path(os.environ.get("UE_ROOT", "/work/repos/unrealengine")))
    parser.add_argument("--project", type=Path,
                        default=Path(__file__).resolve().parents[1] / "FallOfOran.uproject")
    parser.add_argument("--log-dir", type=Path)
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()
    if args.timeout <= 0 or any(level < 0 or level > 2 for level in args.levels):
        parser.error("timeout must be positive; current campaign levels are 0, 1, 2")
    binary = args.engine_root.resolve() / "Engine/Binaries/Linux/UnrealEditor-Cmd"
    project = args.project.resolve()
    if not binary.is_file() or not project.is_file():
        parser.error("editor or project not found; set UE_ROOT/--engine-root and --project")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    output = (args.log_dir or project.parent / "Saved/Automation" / stamp).resolve()
    output.mkdir(parents=True, exist_ok=True)
    lock_path = args.engine_root.resolve() / "Engine/Saved/FallOfOranTests.lock"
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error("another Fall of Oran test run holds the editor lock")
        editors = active_editors()
        if editors:
            parser.error(f"an editor is already running (PIDs {editors}); finish it first")
        results = []
        for level in args.levels:
            result = run_level(binary, project, level, args.mode, output, args.timeout)
            results.append(result)
            summary = {
                "created_utc": stamp, "scope": "headless logic, not graphics or touch",
                "results": results, "passed": len(results) == len(args.levels)
                and all(item["passed"] for item in results),
            }
            (output / "summary.json").write_text(
                json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8",
            )
            print(json.dumps(result, ensure_ascii=False), flush=True)
        print(f"Summary: {output / 'summary.json'}", flush=True)
    return 0 if all(result["passed"] for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
