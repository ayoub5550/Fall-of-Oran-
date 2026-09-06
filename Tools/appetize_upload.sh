#!/bin/bash
# Upload (or update) the Fall of Oran APK on Appetize.io and print a browser play link.
#
# Usage:  Tools/appetize_upload.sh <path/to/FallOfOran-arm64.apk> [note]
# Env:    APPETIZE_TOKEN   – API token (ask the OWNER for it; never commit it, never echo it)
#         APPETIZE_APP_KEY – optional publicKey of an existing app → updates it instead of creating a new one
#         APPETIZE_VERIFICATION – verification.json for this exact APK
#                               (default: <archive>/verification/verification.json)
#
# Owner rule: any AI agent that wants to play-test the game must ASK THE OWNER for an
# Appetize.io API key (account → "API Token"), upload the APK with this script and send
# the owner the returned publicURL. Read the token from a file with mode 600, e.g.
#   APPETIZE_TOKEN=$(cat /work/secrets/appetize_token.txt) Tools/appetize_upload.sh out/FallOfOran-arm64.apk "v2.1"
set -euo pipefail
exec python3 "$(dirname "$(realpath "$0")")/appetize_upload.py" "$@"
