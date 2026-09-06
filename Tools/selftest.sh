#!/usr/bin/env bash
set -euo pipefail
# A failed/timed-out test makes this script fail. Never kill unrelated editor instances.
exec python3 "$(dirname "$0")/run_headless_tests.py" --mode selftest "$@"
