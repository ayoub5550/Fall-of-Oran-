# Touch routing candidate — 2026-09-07

## Report

Owner reports that installed-game buttons do not respond and screen dragging
only rotates the camera. Device model, Android version and exact installed
APK are not yet confirmed.

## Candidate changes

- Use GameAndUI input routing and disable permanent mouse capture so Slate
  controls get first refusal; unhandled input remains available to gameplay.
- Make full-screen HUD containers self-hit-test-invisible while playing.
  Buttons remain hit-testable. The virtual joystick is a lower-Z sibling, so
  returning Unhandled from a full-screen HUD target does not route to it.
- Retain menu/death/win overlay hit testing.
- The branch includes the earlier desktop fire-to-start fallback from PR #1.

## Executed validation

- Linux Development editor target built successfully.
- Tooling unit tests: 22 passed. These do not test C++ touch routing.
- Real rendered runtime, software Vulkan mobile feature level, `-faketouches`:
  XTest pointer presses/drags are converted by Slate into simulated touch.
  No keyboard gameplay actions, mission injection, or screenshot-tour mode.
- Menu brightness changed from 100% to 125% through its button.
- Tapping the menu dismissed it.
- Fire button reduced magazine count (12 → 11, later 11 → 10 → 9).
- Sprint button toggled its active/inactive appearance. Sprint speed was not
  measured independently.
- Left virtual stick reacted to dragging and the character moved forward.
- Right virtual stick reacted to dragging and changed the view.

The recording is genuine, realtime, silent software rendering. This is not
Android device validation or an FPS benchmark.

## Open checks

- Interact button near a valid note/puzzle, keypad input, death/retry/win.
- Simultaneous multi-finger movement, aiming and shooting on Android.
- Mouse/controller desktop regressions with the new input mode.
- Default right joystick visually overlaps the action-button region: layout
  needs further review even though the tested button presses now respond.
- Rebuild/package Android and verify the new archive before distributing it.
  The previously distributed APK does NOT contain these changes.

Candidate remains a draft until real-device confirmation and regression checks.
