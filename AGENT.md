# Fall of Oran — guide for AI agents (and humans)

> **Engine decision (owner: Ayoub, 2026-09-06): this game is developed on Unreal Engine 5.8 (C++), targeting Android.**
> The old Godot build lives in `legacy-godot/` and is frozen. Do not add Godot code. Do not propose an engine switch
> unless the owner asks. Everything below is what you need to work on the project autonomously.

## 0. TL;DR for a new agent
1. Read this file, then `docs/ARCHITECTURE.md` (code layers) and `docs/GDD_AR.md` (game design: rules, numbers, levels, roadmap — keep it in sync with the code in the same commit). Skim `Source/FallOfOran/Core/FOTypes.h` — every gameplay concept is a struct there.
2. Build the editor target, run the headless smoke test, then package Android (§4). **Never ship an APK without §4.4 (pak check).**
3. Content is *data-driven*: to add a level or puzzle you edit `Core/FOLevelRegistry.cpp`, not gameplay classes.
4. Push to `main` early and often — commit and push code changes as soon as they are written, **without waiting for a compile or smoke test** (owner's standing rule since 2026-09-06; the old "only push after it compiles" rule is retired). Mark unverified commits with a `[WIP]`/`untested` note in the message and verify/fix in follow-up commits. Tag releases `vX.Y` only after §4.2 + §4.4 pass.
5. Phone truth beats sandbox truth: the owner tests on a real Android device; sandbox renders (CPU Vulkan) are only a sanity check.
6. **Cloud play-testing route to try = Appetize.io.** There is no usable local emulator here (no KVM/GPU). **Ask the owner for an Appetize.io API key** (appetize.io → account → API Token), then upload the checked APK with `Tools/appetize_upload.sh <apk> "vX.Y"` (§4.5). A successful upload is NOT proof the game runs: verify installation, launch, graphics and controls in an actual session before claiming playability. Never commit or print the key.

## 1. Project facts
| | |
|---|---|
| Engine | Unreal Engine **5.8.2**, private fork `ayoub5550/UnrealEngine` (branch `release`), built from source |
| Language | C++ only (no Blueprints, no UMG; HUD is Slate in code). Runtime-built Enhanced Input. |
| Target | Android arm64, ES3.1 + Vulkan, minSdk 26, `com.ayoub5550.falloforan`, ASTC cook flavour |
| Map | `Content/Maps/Oran.umap` is an **empty** map; `AFOWorldBuilder` builds the whole level at BeginPlay from code |
| Assets | Mixamo characters/animations (`Content/Chars`), CC0 props/textures (`Content/Props`, `Content/Textures`), OGG audio (`Content/Audio`), Noto Kufi Arabic font (`Content/Fonts`) |
| Rendering work | Blender renders (cinematics/marketing) go to **SheepIt** (owner decision); the game itself renders on the player's phone |
| Repo | `github.com/ayoub5550/Fall-of-Oran-` — UE project at root, Godot in `legacy-godot/`, proof shots in `Screenshots/` |

## 2. Source layout (`Source/FallOfOran/`)
```
FallOfOran.{h,cpp,Build.cs}   module; LogFO category; ModuleDirectory is an include root → #include "Core/FOTypes.h"
Core/
  FOTypes.h            ALL data structs/enums: FFOLevelDef, FFOObjectiveDef, FFOPuzzleDef, FFOItemSpawn, FFOGameEvent, FFOProgress
  FOLevelRegistry.*    the campaign (ordered FFOLevelDef list) — EDIT THIS to add/tune levels
  FOGameInstance.*     app-lifetime state: progress + save game (slot "fo_progress"), brightness, -FOLevel=N override
FOGameMode.*           per-level flow (Menu→Playing→Dead|Won), owns Mission + HUD, ReportEvent() is the single gameplay-event entry
Mission/
  FOMissionComponent.* stages → objectives; hints; OnMissionComplete
  FOObjective.*        one objective (Collect / Kill / SolvePuzzle / Reach) — subclass for new rules
Components/
  FOHealthComponent.*  hp for anything
  FOWeaponComponent.*  hitscan gun: mag/reserve, cooldown, reload, distance-scaled aim assist, delegates for FX
  FOInteractable.h     IFOInteractable interface (CanInteract / GetPrompt / Interact)
  FOInteractionComponent.*  finds the best interactable in front of the player (6-7 Hz)
Puzzles/
  FOPuzzleBase.*       AFOPuzzleBase (Solve/Fail, geometry helpers) + AFOPuzzlePart (a clickable piece)
  FOBreakerPuzzle.*    press N coloured breakers in a seeded order (order printed on notes)
  FOKeypadPuzzle.*     numeric code; digits on notes; HUD keypad via GameMode::OpenKeypad
  FONote.*             readable note; first read → ItemCollected("note")
FOCharacter.*          hero: input, camera, animation, movement feel; numbers live in the components
FOZombie.*             steering AI (no navmesh), variants 0..3, runner flag, HpMul per level
FOPickup.*             fuel / medkit / ammo, reports ItemCollected(Tag)
FOWorldBuilder.*       procedural street + lighting preset + spawns items/puzzles/notes/zombies from FFOLevelDef
FOHud.*                Slate HUD: health, objectives, hint banner, note panel, interact button, keypad, menu + level chooser, brightness
```

## 3. How to…
**Add a level** — append an `FFOLevelDef` in `FOLevelRegistry.cpp::BuildCampaign()`: seed, street length, zombie count/runner chance/HpMul,
lighting preset, `Items` (fuel/health/ammo/notes with street coordinates: X along the street 0..StreetLength, Y across −550..550, Z 40–60),
`Puzzles` (Yaw: the puzzle front is its local −X; Yaw −90 faces +Y, Yaw 0 faces the approaching player), and `Stages` (sequential; objectives inside a stage are parallel). Test with `-FOLevel=<index>`.

**Add an objective type** — add an enum value in `EFOObjectiveType`, handle it in `UFOObjective::Matches`, emit the matching `FFOGameEvent` from gameplay code via `GameMode->ReportEvent`.

**Add a puzzle type** — subclass `AFOPuzzleBase` in `Puzzles/`, build geometry in `Setup()`, call `Solve()` / `Fail(msg)`, return note text from `GetHintText(i)`; add an `EFOPuzzleType` value and the `switch` case in `AFOWorldBuilder::SpawnPuzzles`. Anything clickable is an `AFOPuzzlePart` (or implements `IFOInteractable`).

**Tune feel** — movement/camera constants are public members at the top of `AFOCharacter` (WalkSpeed, SprintSpeed, InputSmoothing, BodyTurnSpeed, FOVs); weapon numbers in `FFOWeaponStats`; zombie numbers are `static constexpr` in `FOZombie.h`.

**Change lighting** — per-level `FFOLightingPreset`. Never set manual exposure in a PostProcess volume (see §6). Players have a brightness multiplier (menu / top of HUD) persisted in the save.

## 4. Build, test, package (sandbox recipe — Linux, no root, no GPU)
Engine at `/work/repos/unrealengine` (see `AI-AGENT-BUILD.md` there). Project at `/work/repos/fall-of-oran-ue5`.
Keep sandbox UBT settings in `Engine/Saved/UnrealBuildTool/BuildConfiguration.xml`, not only in the home directory (which may disappear after an environment replacement):
```xml
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
  <BuildConfiguration>
    <bAllowUBAExecutor>false</bAllowUBAExecutor>
    <MaxParallelActions>16</MaxParallelActions>
  </BuildConfiguration>
</Configuration>
```
Do not use the old `<ParallelExecutor><MaxProcessorCount>` block: this engine's schema rejects it.
An absent exit marker does not prove the build is running: check the actual build process too. Resume interrupted builds incrementally, without cleaning existing artifacts.
```bash
# 4.1 compile editor module (~1.5 min incremental)
PATH=/work/temp/fakebin:$PATH Engine/Build/BatchFiles/Linux/Build.sh FallOfOranEditor Linux Development -project=$PROJ/FallOfOran.uproject
# 4.2 logic tests, sequential, no rendering. UE_ROOT defaults to /work/repos/unrealengine.
# Checks both process exit status and game log markers; never kills unrelated editor processes.
UE_ROOT=/work/repos/unrealengine "$PROJ/Tools/selftest.sh"
UE_ROOT=/work/repos/unrealengine "$PROJ/Tools/smoke_test.sh" 1
# Self-test injects gameplay events / DebugSolve, tests the real exit overlap,
# restarts the level once and verifies the old HUD was released. It is NOT a touch-input or graphics test.
# Logs + summary.json: Saved/Automation/<timestamp>/. Override with --log-dir; cold-start timeout: --timeout 900.
# 4.3 proof screenshots on CPU Vulkan (~4 min per frame; -vulkandebug is mandatory or it SIGSEGVs at LoadMap)
LP_NUM_THREADS=1 UnrealEditor-Cmd $PROJ/FallOfOran.uproject -game -vulkan -AllowCPUDevices -featureleveles31 -RenderOffscreen -norhithread -vulkandebug -FOShots -FOShotMax=4 -FOLevel=1 -dpcvars=r.PSOPrecaching=0
#     → Saved/Shots/shot00..03.png (menu, street, mid-street, first puzzle). Kill the process afterwards (it does not always exit).
# Tooling unit tests (mock archives/processes; not real gameplay or APK validation):
python3 -m unittest discover -s "$PROJ/Tools/tests" -v
# 4.4 Android APK. Duration depends on the existing tool/engine/shader cache.
# JDK/SDK/NDK defaults and overrides in Tools/package_android.sh; preserves UAT exit status.
# Packaging shares the editor test lock and refuses to cook while another editor is running.
UE_ROOT=/work/repos/unrealengine "$PROJ/Tools/package_android.sh" "$OUT"
# Logs, PID and final exit status: Saved/Automation/Android-<timestamp>/.
# Building UnrealEditor alone does not build ShaderCompileWorker or UnrealPak; UAT builds those tools as needed.
#     THEN VERIFY THE PAK before sending anything to the owner:
python3 "$PROJ/Tools/verify_android_apk.py" "$OUT/Android_ASTC/FallOfOran-arm64.apk" --output "$OUT/verification"
# Checks ZIP/OBB integrity, package/version, arm64 native library, ALL project assets, and Arabic TTF.
# Manual inspection fallback must extract .ucas DATA too, not only the .utoc index:
unzip -o $OUT/Android_ASTC/FallOfOran-arm64.apk assets/main.obb.png -d /tmp/chk && unzip -o /tmp/chk/assets/main.obb.png '*.utoc' '*.ucas' '*.pak' -d /tmp/chk
UnrealPak -List /tmp/chk/FallOfOran/Content/Paks/FallOfOran-Android_ASTC.utoc | grep -c 'Zombies\|Anims\|Props'   # must be > 0 (was 0 in v1.6)
```
Runtime flags: `-FOLevel=N` (0-based), `-FOShots` (+`-FOShotMax=N`) screenshot tour.

### 4.5 Play-test the APK in the browser (Appetize.io)
There is no usable local emulator in the current sandbox (no `/dev/kvm`, no GPU, no root). The owner's **Appetize.io** API is accessible; actual compatibility with this APK still needs verification:
1. Ask the owner in chat: «أحتاج مفتاح Appetize.io API (الحساب → API Token) لتجربة اللعبة». Store it in a file with mode 600 outside the repo (e.g. `/work/secrets/appetize_token.txt`). It is a secret: never commit, log, or echo it, and remind the owner to rotate it when the session is over.
2. After §4.4 (pak check passed): `APPETIZE_TOKEN=$(cat /work/secrets/appetize_token.txt) Tools/appetize_upload.sh $OUT/Android_ASTC/FallOfOran-arm64.apk "v2.1 - short note"`
   The uploader requires `$OUT/verification/verification.json` and verifies the APK SHA-256 before any upload. Set `APPETIZE_VERIFICATION=<verification.json>` if the checker output is elsewhere. It never prints raw API responses/private keys and saves safe public metadata next to the verification report.
   → prints `publicKey` + play URL. Set `APPETIZE_APP_KEY=<publicKey>` on later uploads to update this same Fall of Oran app instead of piling up new ones. Check the existing app's bundle ID first: this account also contains another game, so never overwrite an unrelated app.
   The uploader enforces that bundle check for updates. On a network/follow-up error, inspect the saved app ID/account before retrying; do not blindly create another app.
3. Open the returned play URL yourself for a smoke run (menu → level 1 → first puzzle). Report installation, launch, rendering, controls and any failure separately. Share the URL with its verified status; do not describe an uploaded but untested app as playable.
4. Limits: the APK is arm64-only. Emulator architecture, ARM translation, Android version, graphics features and account entitlements can affect compatibility. Check the provider's current supported-device information and test the actual APK; neither API access nor a device name guarantees Unreal will run. Keep sessions short within the owner's plan and do not buy an upgrade without approval. Cloud-device FPS is not a real-phone performance benchmark; the owner's physical-device verdict still wins.
Do not try Samsung Remote Test Lab, BrowserStack, Genymotion or Redfinger from the sandbox: their signups need CAPTCHA/SMS/verified e-mail and all failed (2026-09).

## 5. Conventions
- Prefix `FO`; one class per file; `Core/`, `Mission/`, `Components/`, `Puzzles/` subfolders; headers document *why*.
- Gameplay code never talks to the mission directly: emit `FFOGameEvent` through `AFOGameMode::ReportEvent`.
- Numbers belong in structs/components, not in Tick bodies. Arabic strings are UTF-8 literals via `TEXT("…")`.
- Assets loaded by path (`LoadObject`) are only cooked because `DefaultGame.ini` has `+DirectoriesToAlwaysCook=(Path="/Game")` — keep it.
- Commit small steps and push to `main` early, even before compile, per §0.4; mark unverified changes `[WIP]`/`untested`. Messages in English; tag `vX.Y` only after §4.2 + §4.4 pass; never commit credentials
  (`SecurityToken` in DefaultEngine.ini is auto-regenerated; keys/passwords stay out of the repo).

## 6. Known pitfalls (learned the hard way)
- **v1.6 empty world on phone**: zombies/anims/props were not cooked (loaded by path, unreferenced). Fix = DirectoriesToAlwaysCook + §4.4 check.
- **v1.5 black screen**: PostProcess `AEM_Manual` + `AutoExposureBias` with `r.DefaultFeature.AutoExposure=False` on mobile → black frame. Never override exposure in PP; tune light intensities.
- **Overexposure on phone**: real devices render far brighter than the sandbox CPU driver. Keep `BaseMoon≈2`, `BaseSky≈1.2`; let the player scale (brightness buttons).
- **T-pose**: load `UAnimSequence` in `BeginPlay`, never in constructors (CDO time). `PlayAnim` must not early-out before the first clip (`bAnimStarted`). Skeletal materials need `used_with_skeletal_mesh`.
- Static components must be created before `RegisterComponent`. `-ExecCmds="quit"` doesn't quit a cooked game — kill the PID. Never run two editor instances at once.
- Unreal Remote / live viewport streaming from the sandbox is impossible (no GPU, no inbound network); the feedback loop is APK → Appetize.io (§4.5, needs the owner's API key) → owner's phone video.
