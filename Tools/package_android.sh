#!/usr/bin/env bash
# Usage: Tools/package_android.sh [archive-directory]
# Packaging is NOT release validation: run selftest.sh and verify the APK's pak
# contents before sharing. Appetize still requires a real installation/play test.
set -euo pipefail
PROJECT_ROOT="$(dirname "$(dirname "$(realpath "$0")")")"
UE_ROOT="${UE_ROOT:-/work/repos/unrealengine}"
OUT_DIR="$(realpath -m "${1:-$PROJECT_ROOT/Saved/Builds/v2.1}")"
LOG_DIR="${FO_BUILD_LOG_DIR:-$PROJECT_ROOT/Saved/Automation/Android-$(date -u +%Y%m%dT%H%M%SZ)}"
LOG_DIR="$(realpath -m "$LOG_DIR")"
export JAVA_HOME="${JAVA_HOME:-/work/android/jdk}"
export ANDROID_HOME="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-/work/android/sdk}}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export NDKROOT="${NDKROOT:-$ANDROID_HOME/ndk/27.2.12479018}"
export ANDROID_NDK_ROOT="$NDKROOT"
export PATH="$JAVA_HOME/bin:$PATH"
if [[ -d /work/temp/fakebin ]]; then export PATH="/work/temp/fakebin:$PATH"; fi
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export uebp_LogFolder="$LOG_DIR/AutomationTool"

[[ -f "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" ]] || { echo "UE_ROOT does not contain RunUAT.sh" >&2; exit 2; }
[[ -x "$JAVA_HOME/bin/java" && -d "$ANDROID_HOME/platforms" && -d "$NDKROOT" ]] || {
  echo "JDK / Android SDK / NDK missing; check JAVA_HOME, ANDROID_HOME and NDKROOT" >&2; exit 2;
}
mkdir -p "$LOG_DIR" "$OUT_DIR"
# Cooking launches an editor. Share the same engine-level lock as the test runner
# and refuse overlap rather than terminating another task's editor.
mkdir -p "$UE_ROOT/Engine/Saved"
exec 9>>"$UE_ROOT/Engine/Saved/FallOfOranTests.lock"
flock -n 9 || { echo "Another test/cook holds the editor lock; finish it first." >&2; exit 2; }
python3 - "$PROJECT_ROOT" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, str(Path(sys.argv[1]) / "Tools"))
from run_headless_tests import active_editors
editors = active_editors()
if editors:
    raise SystemExit(f"An editor is already running (PIDs {editors}); finish it before cooking.")
PY
printf '%s\n' "$$" > "$LOG_DIR/build.pid"
printf 'STARTED=%s\n' "$(date -Is)" > "$LOG_DIR/build.status"
echo "Android ASTC Development archive: $OUT_DIR"
echo "Build log: $LOG_DIR/build.log"
set +e
bash "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  "-project=$PROJECT_ROOT/FallOfOran.uproject" \
  -platform=Android -cookflavor=ASTC -clientconfig=Development \
  -build -cook -stage -package -pak -archive "-archivedirectory=$OUT_DIR" \
  -nop4 -utf8output -unattended -NoUBA -ddc=NoZenLocalFallback \
  "-ubtargs=-NoUBA -MaxParallelActions=${FO_BUILD_JOBS:-16} -WaitMutex" 2>&1 | tee "$LOG_DIR/build.log"
codes=("${PIPESTATUS[@]}")
result="${codes[0]}"
if [[ "$result" -eq 0 && "${codes[1]}" -ne 0 ]]; then result="${codes[1]}"; fi
printf 'EXIT=%s\nFINISHED=%s\n' "$result" "$(date -Is)" >> "$LOG_DIR/build.status"
echo "EXIT=$result" | tee -a "$LOG_DIR/build.log"
if [[ "$result" -eq 0 ]]; then
  echo "Package built. Pak-content check and device testing are still required."
fi
exit "$result"
