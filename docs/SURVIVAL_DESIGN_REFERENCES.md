# Replayable modes: references and scope

Research: 2026-09-07. Owner requested deeper game-design research, more modes
and generated models. References inform mechanics, not copied media or code.

## Verified references

1. [Mike Booth / Valve, The AI Systems of Left 4 Dead (2009)](https://cdn.akamai.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf).
   Research helper downloaded the official PDF and read its converted full text.
   It describes structured unpredictability and alternating build-up, peaks and
   relaxation. Transfer: bounded spawn budgets and explicit recovery intervals,
   not random enemies everywhere. We are NOT implementing Valve's adaptive
   Director, navmesh population or multiplayer.
2. [Alan Wilson / Tripwire interview, Killing Floor 2 (GamingBolt, 2015)](https://gamingbolt.com/killing-floor-2-interview-organizing-slaughter-and-encouraging-mayhem).
   Article body fetched directly and reread by the parent. Wilson describes
   escalating pressure followed by relative calm to recover. Transfer: wave
   completion, visible intermission/resupply and a finite five-wave victory.
3. [Infinity Ward, Welcome to DMZ (2022)](https://www.infinityward.com/news/2022/11/warzone-2-0-launch-update--welcome-to-dmz/).
   Official body fetched directly and reread by the parent. Looting alone is
   not sufficient: players must extract to receive rewards. Transfer: supplies
   plus physical extraction before a timer, not a kill quota reskinned as another
   mode. We are NOT implementing its stash, insurance, multiplayer or open world.

The two mode concepts were selected before the research completed; the research
then supported recovery pacing and distinct completion rules. Do not attribute
all preceding code or prior generator work to these sources.

## Our bounded design, not claims about the reference games

- Campaign remains its existing three levels.
- Survival: finite waves with escalating role composition, a hard live-enemy
  budget, resupply between waves, clear victory/failure and retry.
- Supply Run: bounded time, individually counted supply pickups and a physical
  extraction requirement; collecting everything alone must not win.
- Both have explicit menu selection and must not unlock campaign levels or
  overwrite campaign bests.
- Six original lightweight props give the compact challenge area landmarks:
  radio mast, supply locker, medical cabinet, sandbags, evacuation beacon and
  market awning. Props need readable placement and purpose; number of assets
  alone is not a quality criterion.

Acceptance includes actual-input menu/start/retry checks and visible scene
inspection in addition to synthetic rules/regression tests. Phone controls and
frame pacing still require a compatible Android device. No FPS or full-playthrough
claim follows from successful compilation or headless test markers.

Excluded from this increment: online co-op, adaptive director, enemy pooling,
permanent economy, new weapon rigs, voiced NPC quests and open-world streaming.
