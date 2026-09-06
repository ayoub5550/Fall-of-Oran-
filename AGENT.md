# Fall of Oran — guide for AI agents (and humans)

> **Engine decision (owner: Ayoub, 2026-09-06): this game is developed on Unreal Engine 5.8 (C++), targeting Android.**
> The old Godot build lives in `legacy-godot/` and is frozen. Do not add Godot code. Do not propose an engine switch
> unless the owner asks. Everything below is what you need to work on the project autonomously.

## 0. TL;DR for a new agent
1. Read this file, then `docs/ARCHITECTURE.md` (code layers) and `docs/GDD_AR.md` (game design: rules, numbers, levels, roadmap — keep it in sync with the code in the same commit). Skim `Source/FallOfOran/Core/FOTypes.h` — every gameplay concept is a struct there.
2. Build the editor target, run the headless smoke test, then package Android (§4). **Never ship an APK without §4.4 (pak check).**
3. Content is *data-driven*: to add a level or puzzle you edit `Core/FOLevelRegistry.cpp`, not gameplay classes.
4. Push every fix to `main` as soon as it compiles and the smoke test passes (owner's standing rule). Tag releases `vX.Y`.
5. Phone truth beats sandbox truth: the owner tests on a real Android device; sandbox renders (CPU Vulkan) are only a sanity check.
6. **Play-testing from the sandbox = Appetize.io.** You cannot run an emulator here (no KVM/GPU). To actually play the APK, **ask the owner for an Appetize.io API key** (appetize.io → account → API Token), then run `Tools/appetize_upload.sh <apk> "vX.Y"` and send him the play link (§4.5). Never commit or print the key.

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
```bash
# 4.1 compile editor module (~1.5 min incremental)
PATH=/work/temp/fakebin:$PATH Engine/Build/BatchFiles/Linux/Build.sh FallOfOranEditor Linux Development -project=$PROJ/FallOfOran.uproject
# 4.2 logic smoke test, one level, no rendering (~1 min). Expect: "GameInstance: 3 levels", "Building level", "Mission: stage 1/…", "Puzzle '…' spawned"
UnrealEditor-Cmd $PROJ/FallOfOran.uproject -game -nullrhi -nosound -unattended -log -FOLevel=1 -FOShots -FOShotMax=2
# 4.3 proof screenshots on CPU Vulkan (~4 min per frame; -vulkandebug is mandatory or it SIGSEGVs at LoadMap)
LP_NUM_THREADS=1 UnrealEditor-Cmd $PROJ/FallOfOran.uproject -game -vulkan -AllowCPUDevices -featureleveles31 -RenderOffscreen -norhithread -vulkandebug -FOShots -FOShotMax=4 -FOLevel=1 -dpcvars=r.PSOPrecaching=0
#     → Saved/Shots/shot00..03.png (menu, street, mid-street, first puzzle). Kill the process afterwards (it does not always exit).
# 4.4 Android APK (~5 min incremental, ~40 min clean). JDK/SDK/NDK env as in Tools/package_android.sh
RunUAT.sh BuildCookRun -project=$PROJ/FallOfOran.uproject -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -package -pak -archive -archivedirectory=$OUT -nop4 -utf8output -unattended -NoUBA -NoUBALocal -ddc=NoZenLocalFallback
#     THEN VERIFY THE PAK before sending anything to the owner:
unzip -o $OUT/Android_ASTC/FallOfOran-arm64.apk assets/main.obb.png -d /tmp/chk && unzip -o /tmp/chk/assets/main.obb.png '*.utoc' -d /tmp/chk
UnrealPak -List /tmp/chk/FallOfOran/Content/Paks/FallOfOran-Android_ASTC.utoc | grep -c 'Zombies\|Anims\|Props'   # must be > 0 (was 0 in v1.6)
```
Runtime flags: `-FOLevel=N` (0-based), `-FOShots` (+`-FOShotMax=N`) screenshot tour.

### 4.5 Play-test the APK in the browser (Appetize.io)
Local emulators are impossible in the sandbox (no `/dev/kvm`, no GPU, no root). The working route is the owner's **Appetize.io** account:
1. Ask the owner in chat: «أحتاج مفتاح Appetize.io API (الحساب → API Token) لتجربة اللعبة». Store it in a file with mode 600 outside the repo (e.g. `/work/secrets/appetize_token.txt`). It is a secret: never commit, log, or echo it, and remind the owner to rotate it when the session is over.
2. After §4.4 (pak check passed): `APPETIZE_TOKEN=$(cat /work/secrets/appetize_token.txt) Tools/appetize_upload.sh $OUT/Android_ASTC/FallOfOran-arm64.apk "v2.1 - short note"`
   → prints `publicKey` + play URL. Set `APPETIZE_APP_KEY=<publicKey>` on later uploads to update the same app instead of piling up new ones.
3. Send the owner the play URL; open it yourself in the browser tool for a smoke run (menu → level 1 → first puzzle) and take screenshots for the report.
4. Limits: Appetize Android devices are x86 emulators — our arm64 APK runs via ARM translation on Android 11+ (choose a Pixel / Galaxy, Android 12+), so expect low FPS; judge logic/UI, not performance. Free-tier minutes are capped — keep sessions short. Real-phone verdict from the owner still wins.
Do not try Samsung Remote Test Lab, BrowserStack, Genymotion or Redfinger from the sandbox: their signups need CAPTCHA/SMS/verified e-mail and all failed (2026-09).

## 5. Conventions
- Prefix `FO`; one class per file; `Core/`, `Mission/`, `Components/`, `Puzzles/` subfolders; headers document *why*.
- Gameplay code never talks to the mission directly: emit `FFOGameEvent` through `AFOGameMode::ReportEvent`.
- Numbers belong in structs/components, not in Tick bodies. Arabic strings are UTF-8 literals via `TEXT("…")`.
- Assets loaded by path (`LoadObject`) are only cooked because `DefaultGame.ini` has `+DirectoriesToAlwaysCook=(Path="/Game")` — keep it.
- Commit as small, compiling steps; message in English; tag `vX.Y` when an APK is handed to the owner; never commit credentials
  (`SecurityToken` in DefaultEngine.ini is auto-regenerated; keys/passwords stay out of the repo).

## 6. Known pitfalls (learned the hard way)
- **v1.6 empty world on phone**: zombies/anims/props were not cooked (loaded by path, unreferenced). Fix = DirectoriesToAlwaysCook + §4.4 check.
- **v1.5 black screen**: PostProcess `AEM_Manual` + `AutoExposureBias` with `r.DefaultFeature.AutoExposure=False` on mobile → black frame. Never override exposure in PP; tune light intensities.
- **Overexposure on phone**: real devices render far brighter than the sandbox CPU driver. Keep `BaseMoon≈2`, `BaseSky≈1.2`; let the player scale (brightness buttons).
- **T-pose**: load `UAnimSequence` in `BeginPlay`, never in constructors (CDO time). `PlayAnim` must not early-out before the first clip (`bAnimStarted`). Skeletal materials need `used_with_skeletal_mesh`.
- Static components must be created before `RegisterComponent`. `-ExecCmds="quit"` doesn't quit a cooked game — kill the PID. Never run two editor instances at once.
- Unreal Remote / live viewport streaming from the sandbox is impossible (no GPU, no inbound network); the feedback loop is APK → Appetize.io (§4.5, needs the owner's API key) → owner's phone video.
