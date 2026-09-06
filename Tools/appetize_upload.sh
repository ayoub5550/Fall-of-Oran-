#!/bin/bash
# Upload (or update) the Fall of Oran APK on Appetize.io and print a browser play link.
#
# Usage:  Tools/appetize_upload.sh <path/to/FallOfOran-arm64.apk> [note]
# Env:    APPETIZE_TOKEN   – API token (ask the OWNER for it; never commit it, never echo it)
#         APPETIZE_APP_KEY – optional publicKey of an existing app → updates it instead of creating a new one
#
# Owner rule: any AI agent that wants to play-test the game must ASK THE OWNER for an
# Appetize.io API key (account → "API Token"), upload the APK with this script and send
# the owner the returned publicURL. Read the token from a file with mode 600, e.g.
#   APPETIZE_TOKEN=$(cat /work/secrets/appetize_token.txt) Tools/appetize_upload.sh out/FallOfOran-arm64.apk "v2.1"
set -euo pipefail
APK="${1:?apk path required}"; NOTE="${2:-$(date -u +%Y-%m-%dT%H:%MZ)}"
[ -f "$APK" ] || { echo "APK not found: $APK" >&2; exit 1; }
[ -n "${APPETIZE_TOKEN:-}" ] || { echo "APPETIZE_TOKEN is empty — ask the owner for an Appetize.io API key" >&2; exit 2; }
URL="https://api.appetize.io/v1/apps${APPETIZE_APP_KEY:+/$APPETIZE_APP_KEY}"
RESP=$(curl -sS -u "$APPETIZE_TOKEN:" "$URL" \
  -F "file=@$APK" -F "platform=android" -F "note=$NOTE" \
  -F "timeout=120" -F "disabled=false")
python3 - "$RESP" <<'PY'
import json, sys
try:
    d = json.loads(sys.argv[1])
except Exception:
    print("Unexpected response:", sys.argv[1][:500]); sys.exit(3)
if "publicKey" not in d:
    print("Upload failed:", json.dumps(d)[:500]); sys.exit(4)
print("publicKey :", d["publicKey"])
print("play URL  :", d.get("publicURL") or f"https://appetize.io/app/{d['publicKey']}")
print("manage URL:", d.get("appURL", ""))
print("Status: uploaded, NOT play-tested. Verify arm64 installation, launch, rendering and controls in an actual session before claiming compatibility.")
PY
