#!/usr/bin/env python3
"""Upload only a verified APK; never print provider secrets or update another app."""
import argparse
from datetime import datetime, timezone
import hashlib
import http.client
import json
import os
from pathlib import Path
import re
import secrets
import sys
import time

PACKAGE = "com.ayoub5550.falloforan"
KEY_PATTERN = re.compile(r"[A-Za-z0-9_-]{5,128}")


def verified_apk(apk, verification):
    report = json.loads(verification.read_text())
    if report.get("passed") is not True or report.get("package") != PACKAGE:
        raise ValueError("APK verification did not pass for Fall of Oran")
    if report.get("missing_assets") != [] or not report.get("required_entries"):
        raise ValueError("Cooked-content verification is missing or incomplete")
    digest = hashlib.sha256()
    with apk.open("rb") as source:
        for chunk in iter(lambda: source.read(1024**2), b""):
            digest.update(chunk)
    if digest.hexdigest() != report.get("sha256"):
        raise ValueError("APK changed after verification; verify this exact file again")
    return report


def validate_key(key):
    if not isinstance(key, str) or not KEY_PATTERN.fullmatch(key):
        raise ValueError("Invalid Appetize public key")
    return key


def decode_response(response):
    # API responses may contain privateKey, account data or signed URLs.
    # Never include their raw contents in diagnostics.
    body = response.read(2 * 1024**2 + 1)
    if not 200 <= response.status < 300:
        raise RuntimeError(f"Appetize HTTP {response.status}; no automatic retry was made")
    if len(body) > 2 * 1024**2:
        raise RuntimeError("Unexpectedly large Appetize response")
    try:
        result = json.loads(body)
    except (ValueError, UnicodeError):
        raise RuntimeError("Appetize returned an invalid JSON response") from None
    if not isinstance(result, dict):
        raise RuntimeError("Unexpected Appetize response structure")
    return result


def fetch_app(token, key):
    validate_key(key)
    connection = http.client.HTTPSConnection("api.appetize.io", timeout=60)
    try:
        connection.request("GET", f"/v1/apps/{key}", headers={
            "X-API-KEY": token, "Accept": "application/json",
        })
        return decode_response(connection.getresponse())
    finally:
        connection.close()


def check_remote_app(data):
    if data.get("platform") != "android" or data.get("bundle") != PACKAGE:
        raise ValueError("Appetize target is not this Fall of Oran Android app; refusing update")


def upload_file(token, apk, note, key=""):
    boundary = "fo-" + secrets.token_hex(20)
    fields = {"platform": "android", "note": note, "timeout": "120", "disabled": "false"}
    prefix = b""
    for name, value in fields.items():
        prefix += (
            f"--{boundary}\r\nContent-Disposition: form-data; name=\"{name}\"\r\n\r\n"
            f"{value}\r\n"
        ).encode("utf-8")
    prefix += (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="file"; filename="FallOfOran-arm64.apk"\r\n'
        "Content-Type: application/vnd.android.package-archive\r\n\r\n"
    ).encode("ascii")
    suffix = f"\r\n--{boundary}--\r\n".encode("ascii")
    connection = http.client.HTTPSConnection("api.appetize.io", timeout=120)
    deadline = time.monotonic() + 600
    try:
        connection.putrequest("POST", "/v1/apps" + (f"/{validate_key(key)}" if key else ""))
        connection.putheader("X-API-KEY", token)
        connection.putheader("Accept", "application/json")
        connection.putheader("Content-Type", f"multipart/form-data; boundary={boundary}")
        connection.putheader("Content-Length", str(len(prefix) + apk.stat().st_size + len(suffix)))
        connection.endheaders()
        connection.send(prefix)
        with apk.open("rb") as source:
            for chunk in iter(lambda: source.read(1024**2), b""):
                if time.monotonic() > deadline:
                    raise TimeoutError("Upload safety deadline exceeded")
                connection.send(chunk)
        connection.send(suffix)
        return decode_response(connection.getresponse())
    finally:
        connection.close()


def upload_verified(apk, verification, token, note, key, result_path):
    if not token or "\n" in token or "\r" in token:
        raise ValueError("APPETIZE_TOKEN missing or invalid — ask the owner for an API key")
    report = verified_apk(apk, verification)
    if key:
        check_remote_app(fetch_app(token, key))
    response = upload_file(token, apk, note, key)
    public_key = validate_key(response.get("publicKey"))
    if key and public_key != key:
        raise RuntimeError("Unexpected app identifier after update; inspect the account before retrying")
    # Persist the public ID immediately, so a failed follow-up GET does not cause
    # another POST and a duplicate app. Do not persist privateKey or raw response.
    result = {
        "uploaded": True, "device_tested": False, "remote_bundle_verified": False,
        "public_key": public_key, "play_url": f"https://appetize.io/app/{public_key}",
        "apk_sha256": report["sha256"],
        "created_utc": datetime.now(timezone.utc).isoformat(),
    }
    result_path.write_text(json.dumps(result, indent=2) + "\n")
    print("publicKey :", public_key, flush=True)
    print("play URL  :", result["play_url"], flush=True)
    try:
        check_remote_app(fetch_app(token, public_key))
    except (OSError, ValueError, RuntimeError, http.client.HTTPException):
        print("Upload returned an app ID, but remote bundle verification failed. "
              "Inspect this ID before retrying; do not create a duplicate.", file=sys.stderr)
        return 5
    result["remote_bundle_verified"] = True
    result_path.write_text(json.dumps(result, indent=2) + "\n")
    print("Status: uploaded, NOT play-tested. Verify installation, launch, rendering and controls.")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("note", nargs="?", default="Fall of Oran — device testing pending")
    args = parser.parse_args()
    apk = args.apk.resolve()
    verification = Path(os.environ.get(
        "APPETIZE_VERIFICATION", str(apk.parent.parent / "verification/verification.json")))
    result_path = Path(os.environ.get(
        "APPETIZE_UPLOAD_RESULT", str(verification.parent / "appetize_upload.json")))
    try:
        return upload_verified(
            apk, verification, os.environ.get("APPETIZE_TOKEN", ""),
            args.note, os.environ.get("APPETIZE_APP_KEY", ""), result_path,
        )
    except (OSError, ValueError, RuntimeError, http.client.HTTPException) as error:
        # Never echo a network exception, response body, request or credential.
        if isinstance(error, (ValueError, RuntimeError)):
            print(str(error), file=sys.stderr)
        else:
            print("Upload did not finish. Check APK/verification files or the account before retrying.",
                  file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
