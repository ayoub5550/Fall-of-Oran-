# v2.1 Development validation — 2026-09-06

## Verdict

**Build, headless regression and archived-content checks passed. Playability is not established.**
The exact APK below installed/launched on two Appetize Android sessions, but both stopped at the Unreal splash with a graphics-capability dialog, before the game menu. It is a **test build**, not a production/Google Play release.

## Artifact identity

| Field | Value |
|---|---|
| APK | `FallOfOran-arm64.apk` |
| Package | `com.ayoub5550.falloforan` |
| App version | `2.1` / Android version code `21` |
| ABI / cook | `arm64-v8a` / Android ASTC |
| Configuration / signature | Development / Android Debug, APK Signature Scheme v2 verified |
| Size | `188488099` bytes (188.5 decimal MB) |
| SHA-256 | `ca1ff404c5a4f07c5026737c25ab29e54909efbb965c3e490289bf225885820d` |
| Minimum / target / compile SDK | `26` / `34` / `36` |
| Game build checkout | `c5c4e518a81df2fad0200323630052061f31383b` |
| Last gameplay-source change | `642eb8d` (subsequent changes before packaging were tools/docs) |
| Engine | UE `5.8.2`, source revision `265a24302`, branch `release` |

The first native `Build.sh` APK contained no cooked game content and was **not distributed**.
Only the later complete BuildCookRun archive is described here.

## Checks actually performed

| Check | Observed result |
|---|---|
| Incremental UnrealEditor build | `Succeeded`, exit 0 |
| FallOfOranEditor + Android arm64 builds | `Succeeded`, exit 0 |
| Strengthened self-tests, levels 1–3 | 3/3 passed, exit 0, all required markers present |
| Smoke tests, levels 1–3 | 3/3 passed, exit 0 |
| Tooling unit tests | 22 passed; mocked processes/archives/network, not device tests |
| UAT build/cook/stage/package/archive | `BUILD SUCCESSFUL`, exit 0, finished 17:02:34 UTC |
| APK/embedded OBB integrity | CRC checks passed |
| Cooked-content listing | 206/206 required entries; 332 project entries across pak/IoStore |
| Content coverage | Source assets, runtime `/Game` literals, `Maps/Oran.umap`, Arabic `Fonts/FOKufi.ttf` |
| Native library | Nonempty `lib/arm64-v8a/libUnreal.so` |
| APK signature / ZIP alignment | `apksigner verify` and `zipalign -c -P 16 -v 4` passed |
| Appetize upload | Separate Fall of Oran app created; remote bundle, version and ABI checked |

Headless tests used **NullRHI**, not a real graphics device. They exercised the real exit-trigger overlap, including unlocking while already inside the exit, winning, actual OpenLevel restart and release of the previous Slate HUD. Collection/kills and puzzle completion otherwise used synthetic events / DebugSolve. They do **not** prove note/pickup touch interactions, graphics, audio, lighting readability, or frame rate.

Archive/signature/ZIP-alignment success likewise does not prove that the APK runs on any particular device or that the build meets store policies.

## Actual Appetize session results

| Device configuration | Evidence | Result |
|---|---|---|
| Pixel 6 / Android 13.0 | Provider app-launch timestamp 17:06:28.712 UTC; sampled browser recording | Unreal startup graphics dialog; no game menu |
| Pixel 9 Pro / Android 16.0 | Provider app-launch timestamp 17:10:05.914 UTC; sampled browser recording | Same dialog; no game menu |

Exact on-screen text:

> **Unable to run on this device!**
>
> This device only supports OpenGL ES 2/3/3.1 which is not supported, only supports ES 3.2+

This matches `Engine/Source/Runtime/OpenGLDrv/Private/Android/AndroidOpenGL.cpp::PlatformInitOpenGL` in the engine revision above. The method checks OpenGL major version 3 and minor version at least 2, despite the configuration/shader naming `bBuildForES31` / ES3_1.

**What is established:** the tested APK reached native engine startup, chose its OpenGL path, and did not pass that path's capability check. **What is not established:** the underlying emulator GPU/driver details, why Vulkan was not selected, whether other Appetize configurations work, or how a compatible physical phone performs.

The initial embedded JS SDK attempt did not reach a device: the account displayed an embed-plan restriction and required authenticated access for debug mode. The ordinary play page worked without those features, exposing the separate graphics issue. No account upgrade was purchased. Both actual device sessions were closed and the provider's session records confirmed closure.

No device debug log was available for these non-debug sessions (the attachment endpoint returned 404). Recordings are sampled screenshots without audio; their playback speed is **not a game FPS measurement**.

## Next acceptance work

1. Install this checked APK on a compatible physical arm64 Android phone (usable Unreal-compatible Vulkan or OpenGL ES 3.2+), or a provider that demonstrates those capabilities for this binary.
2. Test menu → level selection → movement/camera/fire/sprint → pickups/notes → both puzzles → win/restart/save. Record lighting readability and actual-device frame rate separately.
3. If investigating Appetize further, obtain GPU/Vulkan startup logs through authorized debug access or provider support. Do not remove capability guards, claim a paid tier fixes graphics, or spend on an upgrade without owner approval.
4. Keep the physical-device validation pending; a successful upload or changed Android/device label alone is not a fix.

The separate engine Java-version parser change remains an **unmerged draft PR**. Its isolated parser cases passed, but the active engine built Android successfully without it; it is not an explanation or fix for this graphics failure.
