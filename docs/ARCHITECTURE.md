# Architecture

Fall of Oran follows the pattern used by most large UE titles (Lyra-style separation, without the plugin overhead):
**data → systems → actors → presentation**. Nothing in a lower layer knows about a higher one.

```
┌──────────────────────────── data (Core/FOTypes.h, FOLevelRegistry.cpp) ────────────────────────────┐
│ FFOLevelDef { seed, street, lighting, Items[], Puzzles[], Stages[ Objectives[] ] }                  │
└───────────────┬────────────────────────────────────────────────────────────────────────────────────┘
                │ UFOGameInstance::CurrentLevel()   (progress + save game, survives map loads)
                ▼
┌──────────── AFOGameMode (one per loaded map) ───────────────────────────────────────────────────────┐
│ state machine Menu→Playing→Dead|Won   ReportEvent(FFOGameEvent) ──► UFOMissionComponent            │
│ owns: World (AFOWorldBuilder), Mission, Hud (SFOHud), keypad UI state, hints/notes                  │
└───────┬──────────────────────────────────────────────────┬──────────────────────────────────────────┘
        │ BuildWorld(LevelDef)                             │ Start(LevelDef) / HandleEvent
        ▼                                                  ▼
 AFOWorldBuilder                                    UFOMissionComponent
   geometry (seeded) · lighting preset                 stages[] → UFOObjective[] (Collect/Kill/SolvePuzzle/Reach)
   spawns: AFOPickup · AFONote · AFOPuzzleBase* · AFOZombie    OnHint / OnMissionComplete
        │
        ▼ actors emit events upward only through GameMode->ReportEvent
 AFOCharacter ──► UFOHealthComponent / UFOWeaponComponent / UFOInteractionComponent
 AFOZombie    ──► TakeHit → Die → ZombieKilled
 AFOPickup    ──► ItemCollected(tag)
 AFOPuzzleBase──► Solve() → PuzzleSolved(id)   Fail() → GameMode->OnPuzzleFailed (penalty zombies)
 AFONote      ──► NoteRead + ItemCollected("note")
        │
        ▼ presentation reads state every frame
 SFOHud (Slate): health, objectives (Mission text), hint banner, note panel, interact button, keypad, menu/level chooser, brightness
```

## Event flow example — level 3 keypad
1. Player walks to a glowing `AFONote` → `UFOInteractionComponent` shows "اقرأ الملاحظة" → tap → `AFONote::Interact`
   → `GameMode->ShowNote("الرقم الثاني من شيفرة البوابة: 7")` + `ReportEvent(ItemCollected,"note")`.
2. `UFOMissionComponent` advances the `Collect note 4` objective; at 4/4 the stage completes → next stage
   (`SolvePuzzle gatecode`) becomes active → its `StartHint` is shown.
3. `AFOKeypadPuzzle::CanInteract` is now true (it asks `GameMode->IsPuzzleActive("gatecode")`). Interact →
   `GameMode->OpenKeypad(this)` → HUD shows the keypad; digits go to `KeypadPress`; on length reached → `Submit`.
4. Right code → `Solve()` → `ReportEvent(PuzzleSolved,"gatecode")` → stage 3 (`Reach`) → exit trigger accepted →
   `Win()` → `UFOGameInstance::OnLevelCompleted` unlocks level 4 (if any) and saves.

## Why these choices
- **Code-defined levels** instead of hand-built maps: the whole game is reproducible headless (no editor GUI in CI),
  and levels are diffable text. When a designer joins, the same structs can be wrapped in a `UDataAsset`.
- **Event bus through the GameMode** instead of actors poking each other: objectives/puzzles can be added without
  touching zombies, pickups or the HUD.
- **Components for numbers**: health/weapon logic is testable and reusable (a second weapon or a destructible door
  is a new `FFOWeaponStats` / another `UFOHealthComponent`).
- **Slate HUD in C++**: no UMG assets to cook, Arabic shaping via HarfBuzz with a shipped TTF, trivial to diff.

## Save game
`UFOSaveGame` (slot `fo_progress`) holds `FFOProgress`: unlocked levels, selected level, best kills/time per level,
brightness. Reset by deleting the slot (or `UFOGameInstance::ResetProgress`).
