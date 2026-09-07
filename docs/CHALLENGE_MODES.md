# Challenge modes — Survival & Supply Run (2026-09-07)

Two **selectable, bounded** game modes that sit next to the unchanged 3-stage campaign.
The campaign (`FFOLevelRegistry`, `UFOMissionComponent`, `-FOSelfTest`, `-FOEncounterTest`) is untouched:
challenge modes are a separate flow with their own rules engine, their own compact arena and their own save slot.

Status: **written, not yet compiled or run** by the author of this document. The parent thread owns the
build/cook/test run. Nothing here is a claim about on-device behaviour.

## 1. Flow selection

`UFOGameInstance::FlowMode()` → `EFOFlowMode { Campaign, Survival, SupplyRun }` (session state; it is
deliberately *not* written into the campaign save slot).

| Where | Control |
|---|---|
| Menu overlay | `◄ الطور` / `الطور ►` buttons cycle Campaign → Survival → Supply Run → Campaign (`AFOGameMode::SelectRelativeMode`) |
| Menu overlay | The level chooser `◄ السابق / التالي ►` is now visible **only in campaign mode** |
| Death / win screen | `إعادة المحاولة` (retry, same mode) and `القائمة` (return to the menu without advancing anything) |
| Command line | `-FOMode=campaign\|survival\|supply` (dev/CI) |
| Command line | `-FOChallengeTest` runs the synthetic challenge checks and exits |

`AFOGameMode::ReturnToMenu()` reloads the current map with `bContinueIntoLevel = false`, so the player
lands on the menu. A challenge win **never** calls `UFOGameInstance::OnLevelCompleted`, so it can never
unlock a campaign level or overwrite campaign bests.

## 2. Survival — "الصمود: خمس موجات" (`survival_five`)

* Five bounded waves, defined in `Core/FOChallengeRegistry.cpp` (`FFOSurvivalWave`).
* Escalation: 4 → 6 → 8 → 10 → 12 enemies, live cap 3 → 7, runner chance 0 → 0.55,
  heavy (police silhouette) chance 0 → 0.40, HP ×1.0 → ×1.35, spawn interval 1.6 s → 0.9 s.
* **Wave progression counts actually-dead spawned enemies.** `UFOChallengeComponent::PruneEnemies()`
  observes `AFOZombie::bDying` on the enemies it spawned itself. A `ZombieKilled` *event* cannot advance a
  wave, and an enemy that disappears without dying (external `Destroy`, teardown) is counted as *lost*:
  the wave budget is restored and one replacement spawns instead of crediting a kill.
* A wave clears only when `spawned == wave size && confirmed dead == wave size && live == 0`.
* Between waves: an explicit **12–14 s resupply window** (intense wave → calm recovery). The window
  destroys leftovers, re-drops ammo (30 rounds) / medkit (55 hp) crates at all six resupply points and
  tops the player up by 24 reserve rounds.
* Victory: the fifth wave cleared. Failure: player death.

## 3. Supply Run — "خط الإمداد" (`supply_run`)

* Collect **4 crates tagged `supply`** (`EFOItem::Supply`), then **physically reach the extraction pad**
  before a **210 s** deadline. Collecting all four alone does *not* win.
* The crates use the tag `supply`, never `fuel`: the campaign's "fuel can clatter → ambush" rule in
  `AFOGameMode::ReportEvent` is additionally guarded with `!IsChallenge()`, so it cannot be reused as a
  hidden challenge spawner.
* Extraction is `AFOWorldBuilder::ExitTrigger` (in front of the existing gate, dressed with an evac pad
  and beacon). `AFOGameMode::IsExitUsable()` is mode-aware, and `AFOWorldBuilder::Tick` re-checks the
  overlap, so collecting the last crate while already standing on the pad finishes the run.
* Ambient pressure: at most 5 roamers alive, respawning every ~7 s, always outside the fair-spawn radius.
* Failure: the clock reaching 0 (even while holding every crate) or player death.

## 4. Fairness / robustness rules (all enforced in code)

* **Live cap** per wave (`MaxLive`) and for ambient roamers (`AmbientLive`).
* **Fair spawns**: never closer than `SafeSpawnRadius` (1100 / 1200 cm) to the player, always inside the
  arena bounds, and rejected if a pawn-sized capsule would overlap blocking geometry (no pathfinding →
  an enemy stuck in cover would stall a wave). If no fair point is found this attempt, the spawn is
  skipped and retried 0.5 s later — the player is never spawned on.
* **No instant completion**: `FFOChallengeDef::IsValid()` rejects empty wave lists, missing crates or a
  non-positive timer; an invalid definition *fails* the run (and logs) instead of completing it.
  `UFOMissionComponent` is never started in challenge modes, so the "empty mission completes instantly"
  path cannot fire.
* **Retry** re-enters via `Restart()` (map reload) and `UFOChallengeComponent::Start()` fully resets
  waves, kills, spawn history and destroys anything a previous run left behind.

## 5. Compact arena (`AFOWorldBuilder::BuildChallengeArena`)

Challenge levels are synthesized by `FFOChallengeRegistry::MakeLevelDef()` into a normal `FFOLevelDef`
with `bChallengeArena = true`, `ZombieCount = 0` (the component owns every spawn), no stages and a short
street: **30 m** (survival) / **40 m** (supply run) instead of the campaign's 80–100 m.

Authored dressing: three staggered sandbag rows (short segments, wide gaps — no continuous cover that
could seal a route), a resupply hub (locker + med station), a radio-mast landmark, two market stalls and
the extraction pad + beacon.

**Asset hook (parent):** the six generated props are referenced by six `static const TCHAR*` paths at the
top of `BuildChallengeArena()` and nowhere else:
`/Game/Props/Survival/SM_SurvivalRadioMast`, `…SupplyLocker`, `…MedStation`, `…SandbagBarricade`,
`…EvacBeacon`, `…MarketStall` (each `/<Name>/<Name>.<Name>`). If a name changes, edit those six lines.
Every prop is optional: a missing mesh logs a warning and primitive fallback geometry keeps the identical
gameplay layout. Imported props get their own mesh collision disabled (`NoCollide`) and explicit
`Blocker()` boxes instead, so the market awning stays walk-through (poles only).

## 6. Isolated challenge records

Separate save slot `fo_challenge` (`UFOChallengeSaveGame`, `FFOChallengeProgress` / `FFOChallengeRecord`):
attempts, best wave, best kills, best clear time, best remaining time, cleared flag. Loading drops
records for unknown ids and clamps out-of-range/NaN values. The campaign slot `fo_progress` is never
touched by a challenge, in either direction.

## 7. HUD

* In-game: mode + arena name (top right), a wave/live-enemy line for Survival, a `⏱ mm:ss — إمداد n/4`
  line for Supply Run, and the objective block (wave progress, resupply countdown, crates, extraction
  lock state, remaining time).
* Menu: mode name, mode rules text and the isolated personal best for the selected challenge.

## 8. Tests

`-FOChallengeTest` (`Source/FallOfOran/FOChallengeValidation.cpp`, `AFOGameMode::RunChallengeValidation`)
— **explicit synthetic integration checks**, not a playthrough and not an input/graphics test. It runs on
whatever map is loaded, builds its own `UFOChallengeComponent` instances with small test definitions and
asserts, among others:

1. registry: two modes, bounded 5-wave escalation, live cap ≤ wave size, resupply window on every
   non-final wave, timed collect target with enough crates, no `fuel` tag reuse, campaign registry still 3 levels;
2. an empty wave list fails and can never self-complete;
3. wave progression: spoofed `ZombieKilled` events change nothing; live cap holds; spawns are inside the
   arena and outside the safe radius; externally destroyed enemies are *not* credited as kills; the wave
   clears on real `Die()` deaths; the resupply drop happens once per break; final wave → victory;
4. player death fails the run and tears down spawned enemies;
5. supply run: extraction locked early, `fuel` does not count, full collection opens extraction,
   collecting alone does not win, reaching extraction wins before the deadline;
6. deadline expiry fails even while holding every crate; the clock stops at 0; ambient cap holds;
7. save isolation: challenge results leave `UnlockedLevels`, `SelectedLevel` and campaign bests unchanged,
   write only the separate slot, and reload clamped;
8. mode cycling semantics both directions, campaign exposes no challenge definition, and the live
   challenge component stays idle in campaign flow.

Existing suites are unchanged: `-FOSelfTest` (campaign stages; it now logs `SELFTEST SKIP` and exits if a
challenge flow is active) and `-FOEncounterTest`.

## 9. Known gaps / risks

* Nothing here has been compiled or run yet (no build/cook was performed by the author).
* Extraction is validated through the `ExitReached` event path in the synthetic test; the physical
  overlap + touch input on the pad still needs a device/harness check.
* Wave difficulty and the resupply amounts are first-pass tuning numbers, not playtested.
* The arena prop layout assumes the planned prop dimensions (mast ≈319 cm, locker ≈188 cm); collision
  boxes are authored by hand and may need nudging once the meshes are visible in a render.
* Mode selection is session state: quitting the app returns to campaign mode.
