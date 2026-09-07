# Cohesive gameplay candidate

Owner feedback: the game is not smooth/cohesive and lacks enough content.
This is a first limited gameplay slice, not a claim of resolving that feedback.
The delivered Android baseline is preserved separately at
`v2.1-touch-baseline-20260907` / `26e57a5`.

## Changes

- Boulevard: collect fuel, interact with checkpoint generator, defend within
  9 metres during its 18-second startup, then escape toward the power station.
  Startup pauses outside the radius and resumes without resetting.
  The interaction is unavailable until the fuel objective is complete.
- Generator starts one encounter of three additional pursuers; two additional
  ammo pickups and a health pickup are nearby. Existing fuel pickup ambushes
  remain. Difficulty and supply balance still need playtesting.
- Two optional notes link the checkpoint, power station and port.
- Continue from a completed level starts the next level directly after loading,
  without requiring a second menu-start tap. Results screen and map load remain.
  This is not seamless world streaming.
- Moving fire preserves locomotion rather than replacing it with a stationary
  full-body shot clip. Stationary fire still plays that clip.
  Proper upper/lower-body animation layering is not implemented.
- Aim-assist cone now actually tapers across weapon range; target selection
  checks line of sight rather than selecting zombies through obstacles.

## Validation scope

Linux Development editor build succeeded; 22 tooling tests passed.
Headless campaign regression status is recorded in the PR.
Generator `DebugSolve` is explicitly restricted to selftests and bypasses
the timed defence. Existing selftest success therefore does NOT establish
that the defence timer, interaction reachability or encounter balance works.

## Required acceptance checks before another APK

1. Complete the new boulevard through ordinary controls without synthetic
   events or invulnerability: fuel → generator → defence → exit.
2. Verify generator cannot start early or spawn its ambush twice; progress pauses
   outside the radius, resumes inside, and never advances after player death.
3. Verify the generator and its supplies are reachable among procedural props.
4. Finish level one and continue to level two: no extra menu-start tap.
   Retry and manual level selection must still show the intended menu.
5. Shoot while walking/stopping; test a visible target versus one behind a solid
   car/wall. Review remaining single-node animation popping.
6. Real Android multitouch, readability, frame pacing and encounter difficulty.
   No FPS improvement is claimed from this patch.

No new Android package, main merge, or gameplay-quality sign-off accompanies
this candidate. It is deliberately separate from the preserved APK baseline.
