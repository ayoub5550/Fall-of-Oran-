# Checkpoint readability and enemy-role candidate

This continues the draft gameplay branch; the preserved touch baseline is
unchanged. Not a claim that the full game is finished or phone-approved.

## Changes

- Original checkpoint meshes now use their GLB vertex colors via a dedicated
  lit material, including a small 0.22 color contribution to emissive. The
  earlier import had no material reading those colors. Local control-face
  geometry improves recognition without raising global exposure.
- Additive `Tools/finish_checkpoint_assets.py` assigns the material. Explicit
  runtime box components provide collision around all five checkpoint meshes,
  avoiding editor-only subsystem dependence. Imported Kenney materials remain.
- Puzzle tripod moves to the side of the interaction approach. The generator
  checks use distance even for direct API calls. Screenshot-tour framing looks
  down toward the generator rather than above it; this is NOT a gameplay camera
  or input test.
- Existing police zombie silhouette is a heavy: 160 base HP, 140 cm/s chase,
  30 damage, 1.9 s cooldown, 0.65 s windup. Other walkers/runners retain 100 HP,
  190/330 cm/s, 20 damage and 1.3 s cooldown, now with 0.45 s windup.
- Melee damage resolves after windup only if target remains within 170 cm and
  has a clear visibility trace. Cover or retreat can avoid the strike. No new
  animation or enemy mesh; no navmesh/pathfinding claim.
- Checkpoint note explains the heavy enemy and retreat/cover response.

## Reproducible test

`-game -nullrhi -FOLevel=0 -FOEncounterTest` runs a dedicated synthetic actor
integration test and exits with status. It supplies fuel events and teleports
the player, then uses the real interaction component/API. It sweeps the player
capsule along the near approach, verifies one-time wave spawning, advances
generator Tick by explicit deltas to check pause/resume/18 s threshold, and
checks heavy stats plus delayed hit, dodge and obstruction rules.

It does NOT establish an 18-second wall-clock encounter, ordinary touch input,
full-level traversal, perceived animation timing, combat balance or device FPS.
Existing campaign selftests still use DebugSolve. Build, test, visual and APK
results are recorded separately below after execution.

## Executed results (2026-09-07)

- Material commandlet completed successfully after fixing unnamed vertex-color
  output pins and removing an unavailable editor-only collision subsystem.
  Runtime bounds components are used instead, not an unverified collision bake.
- Linux Development editor build: passed (106 actions).
- Dedicated encounter test: all 17 named checks passed, process exit 0.
- Tooling tests: 22 passed.
- Campaign selftests: levels 0, 1 and 2 passed with restart/HUD checks.
- Software Vulkan screenshot-tour render exited 0; actual frame inspected:
  generator olive housing, vents and top/exhaust are now discernible, unlike
  the previous almost-black housing. The barrel is visible to its right.
  Player/tripod still overlap part of the machine; the surrounding street is
  very dark and this is NOT final art acceptance. Crate/bench/barrier are not
  all established by this single view.
- Android archive results: pending at this checkpoint.
