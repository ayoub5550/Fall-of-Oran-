#!/usr/bin/env bash
set -euo pipefail
# Usage: Tools/smoke_test.sh <zero-based-level> [--timeout 900]
level="${1:?zero-based level index required}"
shift
exec python3 "$(dirname "$0")/run_headless_tests.py" --mode smoke --levels "$level" "$@"
