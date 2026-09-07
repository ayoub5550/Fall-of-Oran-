# Reference-led gameplay priorities

Research date: 2026-09-07. Requested by the owner.

## Evidence and access

Direct MobyGames browsing stopped at Cloudflare security verification, including
one proxy retry. No account login was performed. The references below were read
through web-search indexed excerpts, not through successfully opened pages.
Search extracts can mix descriptions and user reviews; only the descriptive
mechanics listed below are used, not review opinions or ratings.
No copyrighted artwork, models, code, music or screenshots were imported.

| Reference | What the indexed excerpt supports | Our application, not a claim about the reference |
| --- | --- | --- |
| [Resident Evil 4](https://www.mobygames.com/game/16373/resident-evil-4/) | Over-shoulder aiming; enemies coordinate to corner the player; treasure and merchant weapon upgrades. | Design a small encounter with two approaches and a retreat route. Reward exploration with one meaningful weapon upgrade, rather than more undifferentiated pickups. |
| [Dead Space](https://www.mobygames.com/game/37332/dead-space/) | Repair missions tied to the ship's condition; attacks on body parts change enemy behaviour; secondary fire and equipment upgrades. | Tie every objective to a visible world change. Give enemy types different tactical roles before adding more enemy skins or health. Do not implement dismemberment merely to copy it. |
| [The Walking Zombie II](https://www.mobygames.com/game/136115/the-walking-zombie-ii/) | Official promotional blurb lists story/side quests, survivors, skills/perks, equipment and traders. This is promotional material, not independently verified gameplay. | Add one optional survivor task with a tangible reward and a later consequence. Do not promise an RPG-sized campaign or reuse the blurb's quantity claims. |

## Work already in the candidate, before this research

`2593c7f` / PR #3 adds the generator defence, story notes, campaign handoff,
moving-fire locomotion preservation and aim-assist fixes. Those changes predate
the MobyGames research and must not be attributed to it retroactively.
See `GAMEPLAY_SLICE_2026_09_07.md` for validation limits.

## Next development order (proposed, not implemented)

1. **Make one section worth playing.** Rework the checkpoint into a readable
   approach → interaction → pressure → escape sequence, with cover, two enemy
   entry routes and an unobstructed retreat. Keep world changes visible: generator
   runs, checkpoint lights change, barrier opens. Hand-validate procedural prop
   placement so the objective and supplies cannot be blocked.
2. **Make enemy decisions matter.** Introduce a slow blocker and a flanking
   runner with distinct silhouettes and sound cues. A new skin with identical
   behaviour does not count as new gameplay. Balance first on a real phone.
3. **Add a purposeful optional detour.** A stranded survivor requests medical
   supplies; helping rewards a weapon upgrade or supplies for the next section.
   Choice must have a persisted outcome; do not consume the player's only
   critical heal or gate the main path on the side quest.
4. **Connect the campaign beyond menu flow.** Carry chosen upgrades and a
   bounded resource loadout between levels, with death/retry snapshots and save
   compatibility. Current handoff only removes a duplicate start tap; it does
   not yet preserve a campaign inventory.
5. **Only then add another area.** A new district should change the encounter
   pattern, not repeat the same long street with a different sky colour.

## Acceptance criteria

These are proposed targets, not measured results:

- One complete 5–10 minute first-session slice through ordinary phone controls.
- New player can explain the immediate goal and why it matters to the escape.
- Meaningful changes in activity: approach/explore, interact, defend, escape.
- No mandatory objective hidden behind random props, unexplained backtracking,
  duplicate start prompts, or progress lost at a chapter boundary.
- Test moving + aiming + firing simultaneously; record actual device frame
  pacing and animation discontinuities. Do not substitute software-rendered
  footage or a headless PASS for phone performance/feel.

Scope deliberately excludes multiplayer, open-world streaming, a large weapon
catalogue and new engine work until the small slice passes.
