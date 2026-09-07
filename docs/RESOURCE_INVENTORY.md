# Resource inventory and checkpoint additions

Audited 2026-09-07. This is a targeted inventory of relevant accessible repositories,
not an exhaustive inventory of the internet or every branch of every repository.

## Existing owner resources

| Repository/source inspected | Found | Decision |
| --- | --- | --- |
| Fall-of-Oran- current Content and Tools/import_assets.py | Mixamo hero and four zombie meshes/animations, five cars, traffic lights/signs/streetlights, PBR surfaces/audio | Keep current rigs; no blind reimport of the destructive full importer. Existing licenses need per-source review before standalone redistribution. |
| inside-copy- main recursive tree and CREDITS.md | Quaternius humanoids/animation library/nature; Poly Haven concrete barriers, crates, utility box, pipe/fence/facade kits and other props | Strong reuse candidates. Credits identify CC0 for these sections; glTF dependencies and original provider terms must be checked before copying an individual asset. No copies added in this batch. |
| dzombies- main recursive tree | Three character glTFs; Zombie_Arm, Zombie_Basic, Zombie_Chubby, Zombie_Ribcage | Candidate silhouettes only. Retargeting/material costs and precise source/license need checking; NOT integrated as functioning new enemies. |
| animation-project main recursive tree and CREDITS.md | Credits for Poly Haven street/industrial/forest assets and Mixamo rigs; no glb/gltf/fbx/obj/blend/zip files found by this filtered tree query | Use its documented provider leads, not a claim that the files are available in that tree. |
| my-gpu and my-computer main recursive trees | No matching 3D model/archive files in filtered query | No resource imported. |

Trees reported `truncated=false`. Other branches and externally stored downloads
were not exhaustively inspected.

## External source verified and downloaded

- Provider page: https://kenney.nl/assets/survival-kit (direct HTTP 200).
- Archive: https://kenney.nl/media/pages/assets/survival-kit/4065a8185b-1712149243/kenney_survival-kit.zip
- Downloaded archive `License.txt`: Survival Kit 2.0, Kenney, CC0; personal,
  educational and commercial use permitted. Credit appreciated, not required.
- Selected only barrel, box-large and workbench GLBs plus referenced colormap.
  Original license is preserved in `RawAssets/Checkpoint/Kenney/License.txt`.
- Quaternius/Poly Pizza search results were leads only in this batch, not imported
  or treated as independent license verification.

## Original generated 3D resources

`Tools/generate_checkpoint_props.py` creates editable low-poly GLB geometry:

- Checkpoint generator: frame, housing, ventilation slats, control panel, feet
  and exhaust; 228 source triangles.
- Checkpoint barrier: base, raised body and alternating hazard panels;
  120 source triangles.

These are actual meshes, not AI-generated pictures presented as 3D models.
They are authored procedurally from primitives, not copied from reference media.
Palette follows the existing generator/hazard materials. Models are prototypes,
not finished photorealistic art.

## Integration and limits

Additive importer: `Tools/import_checkpoint_props.py`; assets isolated under
`/Game/Props/Checkpoint`. It fails if a GLB does not produce exactly one combined
static mesh. Source meshes, provenance, hashes and licenses are kept for rebuilds.

Generator uses its new mesh; a maintenance bench, crate, barrel and low barrier
are placed near it. Procedural cars/rubble exclude puzzle approach bands.
Props are not new inventory mechanics or enemy behaviours.

Pending: visually inspect scale/materials, collision and reachability in rendered
gameplay; check mobile draw calls, texture memory and device frame pacing.
No real-device or quality sign-off from import/build success. No new APK yet.
Do not merge or overwrite the preserved baseline for this asset experiment.

### Executed validation and visual blocker

- All five GLBs imported as one combined static mesh each. Imported triangle
  counts: generator 228, barrier 120, barrel 410, crate 124, bench 236.
  Source barrel has 412 triangles; import reports 410 after processing.
- Linux editor build passed; the modified first-level headless selftest passed.
  No missing-prop or generator-fallback warning appeared in that test.
- A software-Vulkan screenshot-tour render exited successfully and its actual
  frame was inspected. This is teleported/invulnerable proof rendering, NOT a
  playthrough or Android validation.
- **Visual blocker:** generator close-up is too dark and partly obscured by the
  player; the roof/nearby silhouette is not cleanly readable. Material response,
  viewing angle and work-light placement need correction before acceptance.
  Do not treat this shot as proof of polished art or reachable interaction.
- Other props were imported and referenced in code, but are not all visible in
  that single frame. Their scale/collision/materials are still unverified visually.
