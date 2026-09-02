# AGENT.md — Oran: The Fall (وهران: السقوط)

Documentation for AI agents (and humans) continuing development of this game.
Written by Viktor (AI employee) after building v1.0 → v1.1. Read this fully before touching code.

## 1. What this is

A **third-person zombie survival-horror slice** set in Oran, Algeria, styled after **Resident Evil 4 Remake** (over-the-shoulder camera, dark streets, scarce ammo).
Engine: **Godot 4.3**, mobile renderer. Primary target: **Android phone** — the owner (Ayoub, GitHub `ayoub5550`) uses the Godot editor **on an Android phone, no PC**. All UI text is **Arabic**.

Delivery workflow agreed with the owner: build the full project, ZIP it (`shutil.make_archive`, `zip` CLI may be absent), send it in Slack DM; he imports `project.godot` on the phone. Keep the ZIP around ~100 MB.

## 2. Project layout

```
project.godot        # Godot 4.3, mobile renderer, landscape
scenes/main.tscn     # single scene; almost everything is built in code
scripts/main.gd      # ~600 lines: world gen, game states, HUD, spawning, mission
scripts/player.gd    # ~370 lines: movement, camera, shooting, health/ammo
scripts/zombie.gd    # ~200 lines: AI, outfits, LOD, death
audio/               # procedural WAVs (generated with numpy)
assets/              # GLB models + PNG/JPG textures (~98 MB)
concept_art/         # AI-generated concept images (style reference)
screenshots/         # verification screenshots
README_AR.md         # Arabic player-facing readme
```

## 3. Game design (current)

- **Mission**: collect 3 fuel cans (⛽) → port gate unlocks → reach gate → win. Each fuel pickup spawns 2 zombies behind the player.
- **Combat**: pistol, mag 12 / reserve 36, auto-reload 1.3 s, `GUN_DAMAGE 40`, zombie hp 100 (3 shots). **Aim assist**: camera ray first; on miss, cone check `clampf(1.9 / maxf(d, 1.0), 0.12, 0.35)` picks the **nearest** living zombie in group `"zombies"` within `GUN_RANGE 45`.
- **Pickups**: medkit +40 hp ×3, ammo +12 ×4.
- **HUD**: health bar, ammo `12 / 36`, kills ☠, fuel ⛽ `0/3`, Arabic objective label, crosshair (5 px ColorRect, `PRESET_CENTER`, `MOUSE_FILTER_IGNORE`), vignette. Touch fire button is the Arabic word **«نار»** — see pitfalls §7.
- **States** (`main.gd` enum `GameState`): 0 MENU, 1 PLAYING, 2 DEAD, 3 WON. Key API: `_start_game()`, `_win()`, `kills`, `fuel`, `FUEL_NEEDED`.

## 4. Code architecture facts (verified — do not guess)

- The player is **built in code** in `main._spawn_player()` and stored as `main.player`. There is **no named "Player" node** in the scene tree.
- `player.gd`: property is **`health`** (not `hp`), reserve ammo is **`reserve`** (not `ammo_reserve`). API: `take_damage(amount)`, `add_ammo(n)`, `add_health(n)`, `try_shoot()`, `pickup_flash()`; signals `died`, `damaged`, `shot_fired`.
- `zombie.gd`: damage entry point is **`take_hit(damage: float)`** (not `take_damage`). Death emits `killed`, plays fall+fade tween + `zombie_die.wav`.
- Props are placed with `main._prop(file, pos, roty_deg, target_h)`: loads a GLB, scales by merged AABB to `target_h`, puts feet on ground, `Node3D`, **no collision**.
- Performance LOD in `zombie.gd`: beyond 45 m — animation paused, velocity zeroed, early return.
- Player movement uses acceleration smoothing: `accel := 1.0 - exp(-9.0 * delta)` lerp on x/z velocity.

## 5. Art pipeline

- **Character models**: GLB, low-poly rigged (KayKit/Quaternius style). Animations: zombie `Zombie|Zombie{Bite,Crawl,Idle,Run,Walk}`; hero `Human Armature|{Idle,Walk,Run,Death,Punch,Jump,...}`. Blend with `anim.play(name, 0.25)`. **Do not trust the AABB of unposed skinned meshes.**
- **Outfits via band textures**: character UVs sample a **32-row horizontal palette strip**. Rows ≈ 0–6 skin, 6–11 eyes, 11–17 hair, 17–23 shirt, 23–32 pants (zombie: 28–32 shoes). Generate 128×128 PNGs with numpy (4 px per row) — see `assets/zombie_worker.png` (hi-vis), `zombie_cop.png`, `zombie_civilian.png`, `zombie_medic.png`, `hero_tex.png`. `zombie.gd` picks a random outfit at spawn and tints near-white; hero material albedo stays `Color(1,1,1)`.
- **Environment textures**: 2K JPG from ambientCG — direct URL `https://ambientcg.com/get?file={Name}_2K-JPG.zip`. Triplanar `StandardMaterial3D` needs no UVs.
- **Props**: Quaternius packs (Google Drive, `uvx gdown --folder URL`; Drive rate-limits). FBX → GLB with FBX2glTF (`--binary`).
- **Audio**: all SFX are numpy-generated WAVs in `audio/`.

## 6. Testing & tooling (headless, no GPU needed except screenshots)

- Headless Godot Linux binary: download `Godot_v4.3-stable_linux.x86_64`.
- After deleting `.godot/`, you **must** run `--headless --import .` (~4 min). "Null instance" errors on scene load = imports missing.
- Logic tests: a `SceneTree` script run with `-s test.gd --path .` — instantiate `main.tscn`, call `_start_game()`, assert states/kills/health/win. A known-good test flow is in the history: menu 0 → playing 1 → 3× `take_hit(40)` = 1 kill → `take_damage(25)` → `add_ammo(12)` → `fuel = 3; _win()` → state 3.
- Screenshots: `xvfb-run -a godot --rendering-driver opengl3 -s shots.gd --path .` then `get_viewport().get_texture().get_image().save_png(...)`.
- Run long headless jobs with **output redirected to a file** — Godot can hang writing to a dead pipe.
- Filter log noise: `grep -v "Parameter\|null instance\|servers/rendering\|drivers/dummy"`.

## 7. Pitfalls (learned the hard way)

1. **Emoji in Godot UI**: 🔫 etc. render as tofu with the default font. Use Arabic text («نار») for buttons; ☠ ⛽ happen to render fine.
2. Use `_input`, not `_unhandled_input`, for taps that must work over UI.
3. GDScript `:=` type inference fails on Variant loop variables — annotate.
4. Owner's phone-only workflow: never require command line, plugins, MCP, or PC-only steps from him.
5. Keep every repo file < 100 MB (GitHub hard limit); the whole assets dir is fine as-is.
6. The owner writes **Arabic** — reply and document player-facing text in Arabic.

## 8. Roadmap ideas (not committed)

- More districts of Oran (Sidi El Houari alleys, the port, Santa Cruz fort — see `concept_art/`).
- Melee weapon + knife parry (RE4-style), weapon variety (shotgun), inventory/attaché case.
- Zombie variety: crawler (anim exists: `Zombie|ZombieCrawl`), runner, armored cop.
- Save system, chapters, boss fight at the port gate.
- Real Arabic voice lines (TTS), music layers by threat level.
- Mobile perf: bake light, pool zombies, reduce shadow casters.

— Viktor, 2026-09-02
