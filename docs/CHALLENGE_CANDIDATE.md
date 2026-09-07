# Challenge expansion candidate — 2026-09-07

Development branch: `feat/cohesive-gameplay`; review in PR #3, based on the
separately preserved touch baseline. Not a main/release merge.

## Implemented

- Campaign remains three levels.
- Survival: five waves (4/6/8/10/12 enemies), simultaneous caps 3–7,
  inter-wave resupply; actual observed deaths advance the wave.
- Supply Run: four distinct supply crates, 210-second clock, five-live-enemy
  ambient cap; collection plus reaching extraction required.
- Separate challenge personal-best slot; menu mode cycling and same-mode retry.
- Six original generated environment meshes imported into Unreal and used in
  the compact challenge arena. No new character meshes in this increment.
- Challenge arenas exclude inherited random cars/debris/checkpoint obstacles;
  common Oran buildings, ground, lighting and street furniture remain.

Design references: [research](SURVIVAL_DESIGN_REFERENCES.md),
[rules](CHALLENGE_MODES.md), [asset provenance](SURVIVAL_ASSET_SOURCES.md).

## Verified evidence

1. Linux editor build succeeded: 94 actions, 828.18 seconds. Subsequent
   HUD-only wrap/glyph fix compiled in 2 actions, 41.89 seconds.
2. `-FOChallengeTest`: **58 synthetic integration checks passed**.
   Includes bounded progression, death versus disappearance, spoof-event
   rejection, timer/failure, retry state and isolated save results. Run with
   an isolated user directory: this test deliberately writes synthetic
   personal-best records and must not target a player's regular save.
3. Existing generator encounter checks passed; 22 Python tooling tests passed;
   all three campaign selftests passed. These are not playthroughs.
4. Actual rendered Linux software-Vulkan session with `-faketouches`,
   960×540: clicked menu Campaign→Survival, started it, moved using the
   on-screen stick, and fired through the on-screen button (12→10 ammo).
   First wave displayed live cap 3.
5. Supply Run opened with its rules correctly wrapped after the HUD fix.
   Actual screen inputs started the run, moved, and fired (12→10).
   Timer expired at 0/4 supplies; failure screen appeared. Clicking retry,
   then start, reset ammo to 12/36 and restarted the clock (observed 3:21
   after elapsed seconds), still in Supply Run.
6. A real-input Supply Run recording is 45.017 seconds, 6,526,048 bytes,
   H.264, 960×540. Full decoding completed without errors. Silent, software
   rendering; captured video rate is not game FPS. No teleport/invulnerability
   screenshot mode used for this recording.

## Important limits

- No completed ordinary-input challenge victory, physical extraction-pad
  completion, or all-four-crate reachability verification yet. Extraction
  success is synthetic event-path evidence, not input evidence.
- No new Android device/Appetize session, multitouch or measured phone FPS.
  The earlier Appetize ES 3.2+ blocker remains unresolved.
- This is first-pass tuning and prototype art: dark buildings, a very bright
  green exit panel, rough/reflective enemy materials, and some missing HUD
  symbol glyphs remain. Menu navigation arrows and brightness minus were fixed.
- Basic direct-steering enemy AI, not navigation-mesh pathfinding. Cover can
  impede enemies; wide lanes are not proof of robust navigation.
- Collision is hand-authored and not a complete visual/capsule certification
  of each prop. Medical station/beacon/stall now use original model heights.
- Game remains version 2.1/code21 and debug-signed. Package verification and
  signing identity are recorded separately after packaging, not inferred from
  this source build.

Do not call this a polished release or proof that the user's phone problem is fixed.

## Packaged Android candidate

- Source: `f1a3401`; full ASTC Development BuildCookRun succeeded in 502.32 s.
- `Fall-of-Oran-challenge-candidate.apk`: **188,767,287 bytes**.
- SHA256: `e808a0b10a84715cecbaae3dcc3757ac1418fe58c49d5fa2127b578b75dbbd8b`.
- Archive verification 2026-09-07T18:56:40Z: package/version/arm64 passed,
  224 required assets present among 364 game entries, none missing.
- APK Signature Scheme v2 and 16 KB page alignment verified.
- Debug certificate SHA256:
  `32cf66cf3d5ee72b9ca4e7f337d36e11a5c2c9c7b7a349cef1e36e76ec80b8f0`.
  This differs from the preceding checkpoint APK signer. Installing as an
  update may fail; uninstalling can delete progress. Stable release signing
  is still unresolved.
- Initial signature verification command lacked Java on PATH (exit 127).
  Repeated against the same APK with the installed JDK on PATH: passed.
- No Android/device/multitouch/FPS claim follows from these packaging checks.
