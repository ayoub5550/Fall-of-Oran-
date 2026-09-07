"""Original low-poly survival props. Run with:
    uv run --with trimesh python Tools/generate_survival_props.py

No reference meshes, textures or images are copied; all geometry is procedural.
GLB output uses metres, Y up, front facing -X, vertex/face colours only.
Palette follows brand/DESIGN.md (olive #4d4714, charcoal #161616, hazard #e6bf1a).
"""
from pathlib import Path
import hashlib
import json

import numpy as np
import trimesh

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "RawAssets/Survival/Original"
OUT.mkdir(parents=True, exist_ok=True)

OLIVE = (77, 71, 20, 255)
CHARCOAL = (22, 22, 22, 255)
HAZARD = (230, 191, 26, 255)


def blend(a, b, weight):
    """Byte RGB interpolation: round((1-weight)*a + weight*b), opaque."""
    return tuple(round((1 - weight) * x + weight * y)
                 for x, y in zip(a[:3], b[:3])) + (255,)


def tint(color, weight):
    """Computed white tint of an existing brand token, not a new palette token."""
    return blend(color, (255, 255, 255, 255), weight)


# All supporting colours derive explicitly from the three brand tokens above.
STEEL = tint(CHARCOAL, .38)
LIGHT_STEEL = tint(CHARCOAL, .56)
CANVAS = tint(OLIVE, .40)
SAND = tint(OLIVE, .48)
WHITE = tint(CHARCOAL, .86)
GLASS = blend(CHARCOAL, OLIVE, .30)
WOOD = blend(CHARCOAL, OLIVE, .70)

stats = {}


def add(scene, name, mesh, color):
    mesh.visual.face_colors = color
    scene.add_geometry(mesh, node_name=name)


def box(scene, name, size, pos, color):
    m = trimesh.creation.box(extents=size)
    m.apply_translation(pos)
    add(scene, name, m, color)


def cyl(scene, name, radius, height, pos, color, sections=10, axis="y"):
    m = trimesh.creation.cylinder(radius=radius, height=height, sections=sections)
    # trimesh cylinders are built along +Z; rotate to the requested axis.
    if axis == "x":
        m.apply_transform(trimesh.transformations.rotation_matrix(1.5707963, [0, 1, 0]))
    elif axis == "y":
        m.apply_transform(trimesh.transformations.rotation_matrix(1.5707963, [1, 0, 0]))
    m.apply_translation(pos)
    add(scene, name, m, color)


def save(scene, name):
    path = OUT / (name + ".glb")
    data = scene.export(file_type="glb")
    path.write_bytes(data)
    # Measure the actual serialized file, not the pre-export in-memory scene.
    exported = trimesh.load(path, force="scene", process=False)
    b = exported.bounds
    triangles = sum(len(m.faces) for m in exported.geometry.values())
    assert np.isfinite(b).all() and abs(b[0, 1]) < 1e-6
    assert 0 < triangles < 1500
    for m in exported.geometry.values():
        assert np.isfinite(m.vertices).all()
        assert (m.area_faces > 1e-12).all()
        assert m.is_watertight and m.is_winding_consistent
        assert m.visual.kind == "vertex"
        assert len(m.visual.vertex_colors) == len(m.vertices)
    stats[name] = {
        "file": str(path.relative_to(ROOT)),
        "triangles": int(triangles),
        "parts": len(exported.geometry),
        "bounds_min_m": [round(float(v), 4) for v in b[0]],
        "bounds_max_m": [round(float(v), 4) for v in b[1]],
        "size_m": [round(float(v), 4) for v in (b[1] - b[0])],
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "source": "original procedural geometry",
        "validation": "reloaded GLB: finite, grounded, nondegenerate, watertight parts, vertex colours, <1500 triangles",
    }


# --- 1. Portable radio mast: wide tripod feet, slim lattice tower, dish + whip.
s = trimesh.Scene()
box(s, "pad", (0.9, 0.08, 0.9), (0, 0.04, 0), CHARCOAL)
for i, (x, z) in enumerate([(-0.35, -0.35), (0.35, -0.35), (0.35, 0.35), (-0.35, 0.35)]):
    box(s, f"foot{i}", (0.3, 0.06, 0.3), (x, 0.11, z), OLIVE)
    box(s, f"stay{i}", (0.05, 0.9, 0.05), (x * 0.75, 0.55, z * 0.75), STEEL)
for i in range(7):
    y = 0.2 + i * 0.34
    box(s, f"lattice{i}", (0.16, 0.3, 0.16), (0, y + 0.15, 0), LIGHT_STEEL)
    box(s, f"rung{i}", (0.26, 0.04, 0.26), (0, y, 0), CHARCOAL)
box(s, "mast_top", (0.1, 0.5, 0.1), (0, 2.83, 0), LIGHT_STEEL)
cyl(s, "dish", 0.32, 0.07, (-0.2, 2.35, 0), WHITE, sections=12, axis="x")
box(s, "dish_arm", (0.22, 0.05, 0.05), (-0.09, 2.35, 0), CHARCOAL)
box(s, "beacon_lamp", (0.09, 0.12, 0.09), (0, 3.13, 0), HAZARD)
box(s, "radio_box", (0.3, 0.42, 0.5), (-0.3, 0.35, 0), OLIVE)
box(s, "radio_face", (0.04, 0.22, 0.34), (-0.46, 0.4, 0), CHARCOAL)
save(s, "SM_SurvivalRadioMast")

# --- 2. Supply locker: tall twin-door cabinet with louvres and latch.
s = trimesh.Scene()
box(s, "base", (0.62, 0.09, 0.52), (0, 0.045, 0), CHARCOAL)
box(s, "body", (0.56, 1.72, 0.46), (0, 0.95, 0), OLIVE)
box(s, "top_cap", (0.64, 0.07, 0.54), (0, 1.845, 0), CHARCOAL)
for i, z in enumerate([-0.115, 0.115]):
    box(s, f"door{i}", (0.04, 1.6, 0.21), (-0.29, 0.95, z), tint(OLIVE, .08))
    for j in range(4):
        box(s, f"louvre{i}_{j}", (0.02, 0.03, 0.17), (-0.315, 1.42 + j * 0.09, z), CHARCOAL)
box(s, "latch", (0.06, 0.3, 0.05), (-0.32, 0.95, 0), LIGHT_STEEL)
box(s, "handle", (0.1, 0.05, 0.05), (-0.36, 0.86, 0), HAZARD)
box(s, "label", (0.02, 0.16, 0.26), (-0.313, 1.15, 0), HAZARD)
save(s, "SM_SurvivalSupplyLocker")

# --- 3. Medical station: waist-high counter, glass upper, neutral capsule.
s = trimesh.Scene()
box(s, "plinth", (0.55, 0.12, 1.2), (0, 0.06, 0), CHARCOAL)
box(s, "counter_body", (0.5, 0.78, 1.14), (0, 0.51, 0), WHITE)
for i in range(3):
    box(s, f"drawer{i}", (0.03, 0.2, 1.0), (-0.26, 0.24 + i * 0.25, 0), tint(CHARCOAL, .74))
    box(s, f"drawer_pull{i}", (0.05, 0.04, 0.3), (-0.29, 0.24 + i * 0.25, 0), LIGHT_STEEL)
box(s, "worktop", (0.58, 0.06, 1.22), (0, 0.93, 0), STEEL)
box(s, "upper_back", (0.12, 0.72, 1.1), (0.2, 1.32, 0), WHITE)
for i, z in enumerate([-0.27, 0.27]):
    box(s, f"upper_case{i}", (0.34, 0.68, 0.5), (0.03, 1.32, z), tint(CHARCOAL, .80))
    box(s, f"glass{i}", (0.02, 0.5, 0.4), (-0.15, 1.34, z), GLASS)
box(s, "shelf", (0.3, 0.03, 1.02), (0.03, 1.32, 0), LIGHT_STEEL)
box(s, "medicine_plaque", (0.035, 0.24, 0.46), (-0.175, 1.35, 0), WHITE)
# Horizontal rounded capsule, olive/yellow halves; no humanitarian emblem.
for label, z, color in [("olive", -.12, OLIVE), ("yellow", .12, HAZARD)]:
    cyl(s, f"capsule_end_{label}", .07, .018, (-.201, 1.35, z),
        color, sections=12, axis="x")
    box(s, f"capsule_half_{label}", (.018, .14, .12),
        (-.201, 1.35, z / 2), color)
box(s, "iv_pole", (0.05, 1.1, 0.05), (-0.1, 1.5, 0.63), LIGHT_STEEL)
box(s, "iv_bag", (0.05, 0.22, 0.14), (-0.1, 1.94, 0.63), tint(OLIVE, .80))
save(s, "SM_SurvivalMedStation")

# --- 4. Sandbag barricade: staggered stacked bags, low and long.
s = trimesh.Scene()
rows = [(0.0, 7, 0.0), (0.16, 6, 0.22), (0.32, 5, 0.44), (0.48, 3, 0.9)]
for r, (y, count, zoff) in enumerate(rows):
    depth = 0.42 - r * 0.05
    span = 2.6 - r * 0.18
    for i in range(count):
        z = -span / 2 + (i + 0.5) * (span / count)
        box(s, f"bag{r}_{i}", (depth, 0.155, span / count * 0.94),
            ((r * 0.02), y + 0.085, z + (0.03 if i % 2 else -0.03)),
            tint(OLIVE, .48 - r * .05 + (.03 if i % 2 else -.02)))
box(s, "post_l", (0.09, 0.95, 0.09), (-0.14, 0.475, -1.36), CHARCOAL)
box(s, "post_r", (0.09, 0.95, 0.09), (-0.14, 0.475, 1.36), CHARCOAL)
box(s, "rail", (0.06, 0.07, 2.72), (-0.14, 0.9, 0), HAZARD)
save(s, "SM_SurvivalSandbagBarricade")

# --- 5. Evacuation beacon: slim tripod pole with strobe head + solar fin.
s = trimesh.Scene()
box(s, "base_plate", (0.52, 0.07, 0.52), (0, 0.035, 0), CHARCOAL)
for i, (x, z) in enumerate([(-0.2, 0), (0.14, -0.18), (0.14, 0.18)]):
    box(s, f"leg{i}", (0.1, 0.55, 0.1), (x, 0.32, z), OLIVE)
cyl(s, "pole", 0.055, 2.1, (0, 1.6, 0), LIGHT_STEEL, sections=8)
for i in range(3):
    box(s, f"band{i}", (0.13, 0.09, 0.13), (0, 0.95 + i * 0.45, 0), HAZARD)
box(s, "battery", (0.22, 0.34, 0.3), (0.02, 0.78, 0), OLIVE)
cyl(s, "strobe", 0.15, 0.3, (0, 2.78, 0), HAZARD, sections=12)
cyl(s, "strobe_cap", 0.16, 0.06, (0, 2.96, 0), CHARCOAL, sections=12)
box(s, "solar_fin", (0.5, 0.04, 0.34), (-0.22, 2.5, 0), GLASS)
box(s, "solar_arm", (0.16, 0.05, 0.06), (-0.06, 2.5, 0), CHARCOAL)
box(s, "sign", (0.03, 0.24, 0.34), (-0.09, 2.2, 0), WHITE)
save(s, "SM_SurvivalEvacBeacon")

# --- 6. Market stall awning: low striped canopy on four thin poles.
s = trimesh.Scene()
for i, (x, z) in enumerate([(-0.8, -1.1), (0.8, -1.1), (0.8, 1.1), (-0.8, 1.1)]):
    box(s, f"pole{i}", (0.07, 2.0, 0.07), (x, 1.0, z), WOOD)
    box(s, f"shoe{i}", (0.16, 0.05, 0.16), (x, 0.025, z), CHARCOAL)
box(s, "beam_front", (0.08, 0.09, 2.3), (-0.8, 2.02, 0), WOOD)
box(s, "beam_back", (0.08, 0.09, 2.3), (0.8, 2.12, 0), WOOD)
canopy = trimesh.creation.box(extents=(1.78, 0.05, 2.3))
canopy.apply_transform(trimesh.transformations.rotation_matrix(-0.055, [0, 0, 1]))
canopy.apply_translation((0, 2.09, 0))
add(s, "canopy", canopy, CANVAS)
for i in range(6):
    z = -0.96 + i * 0.384
    stripe = trimesh.creation.box(extents=(1.78, 0.02, 0.19))
    stripe.apply_transform(trimesh.transformations.rotation_matrix(-0.055, [0, 0, 1]))
    stripe.apply_translation((0, 2.125, z))
    add(s, f"stripe{i}", stripe, OLIVE if i % 2 else WHITE)
box(s, "valance", (0.04, 0.22, 2.3), (-0.86, 1.87, 0), CANVAS)
box(s, "counter", (0.5, 0.06, 2.0), (-0.45, 0.92, 0), tint(OLIVE, .22))
box(s, "counter_leg_l", (0.06, 0.9, 0.06), (-0.45, 0.45, -0.9), WOOD)
box(s, "counter_leg_r", (0.06, 0.9, 0.06), (-0.45, 0.45, 0.9), WOOD)
for i in range(4):
    box(s, f"crate{i}", (0.3, 0.26, 0.3), (0.45, 0.13 + (i // 2) * 0.27, -0.5 + (i % 2) * 1.0), SAND)
save(s, "SM_SurvivalMarketStall")

(OUT / "manifest.json").write_text(json.dumps(stats, indent=2) + "\n")

# Keep the provenance document synchronized with final reloaded exports.
notes = {
    "SM_SurvivalRadioMast": ("Portable mast with four-foot pad, segmented tower, dish and radio box.",
                            "Lower-body box only: 0.9 × 1.0 × 0.9 m; do not collide tower/dish."),
    "SM_SurvivalSupplyLocker": ("Twin-door locker with louvres, latch and hazard label.",
                               "Full bounds box, offset to the bounds centre."),
    "SM_SurvivalMedStation": ("Drawer counter, upper cases, IV pole and olive/yellow capsule symbol.",
                             "Counter box: 0.6 × 1.0 × 1.25 m; upper cases non-colliding."),
    "SM_SurvivalSandbagBarricade": ("Four staggered rows of block-stylized bags, posts and hazard rail.",
                                  "Full bounds box: 0.42 × 0.95 × 2.81 m."),
    "SM_SurvivalEvacBeacon": ("Three-leg beacon, banded pole, solar fin and strobe.",
                            "No collision recommended; optional base box 0.5 × 0.7 × 0.5 m."),
    "SM_SurvivalMarketStall": ("Four-pole low striped awning, counter and crates.",
                             "No canopy collision; optional counter box 0.5 × 0.98 × 2.0 m at x=-0.45."),
}
lines = [
    "# Survival Prop Asset Sources", "",
    "Generated by `Tools/generate_survival_props.py`; all meshes are original procedural",
    "geometry. No copied meshes, images or textures; no network-sourced art assets.", "",
    "## Regenerate and validate", "",
    "```bash", "uv run --with trimesh python Tools/generate_survival_props.py", "```", "",
    "Regenerates six GLBs, `RawAssets/Survival/Original/manifest.json` and this document.",
    "Statistics below are measured after reloading each final GLB. Assertions check",
    "finite coordinates, y=0 grounding, nondegenerate triangles, watertight consistently",
    "wound individual parts, vertex colours and fewer than 1500 triangles.", "",
    "## Conventions", "",
    "- GLB metres, Y up, front -X; sizes below are X × Y × Z, not Unreal axes.",
    "- Ground pivot at y=0. Bounds may be asymmetric around x/z origin.",
    "- Boxes and low-sided cylinders; intersecting closed parts, not one manifold union.",
    "- Opaque vertex colours; no textures. Parent supplies vertex-colour material.",
    "- No authored collision. Parent imports and supplies explicit box collision.",
    "- No Unreal import or engine-side visual validation performed by this generator.", "",
    "## Palette provenance", "",
    "Base tokens from `/work/brand/DESIGN.md`: olive #4d4714, charcoal #161616,",
    "hazard yellow #e6bf1a (also used by `Tools/generate_checkpoint_props.py`).",
    "All other colours are computed derivatives, not invented brand tokens.",
    "`blend(a,b,w) = round((1-w)*a + w*b)` per RGB channel; `tint(a,w)` blends",
    "toward RGB white (255,255,255). Every colour has alpha 255.", "",
    "| Use | Derivation |", "|---|---|",
    "| Steel / light steel | charcoal tints 0.38 / 0.56 |",
    "| Canvas / sand | olive tints 0.40 / 0.48 |",
    "| White paint | charcoal tint 0.86 |",
    "| Glass / wood | charcoal→olive blends 0.30 / 0.70 |",
    "| Locker doors | olive tint 0.08 |",
    "| Drawers / upper cases | charcoal tints 0.74 / 0.80 |",
    "| IV bag / counter | olive tints 0.80 / 0.22 |",
    "| Bags row r, column i | olive tint 0.48−0.05r+(0.03 if odd i else −0.02) |",
    "| Capsule / canopy stripe | olive and hazard / olive and white-paint derivative |", "",
    "The medicine marker is a horizontal rounded olive/yellow capsule, not a red cross.", "",
    "## Final exports", "",
    "All paths below are under `RawAssets/Survival/Original/`.", "",
    "| File | Triangles | Parts | Size X × Y × Z (m) | Bytes |",
    "|---|---:|---:|---|---:|",
]
for name, item in stats.items():
    size = " × ".join(str(v) for v in item["size_m"])
    lines.append(f"| `{name}.glb` | {item['triangles']} | {item['parts']} | {size} | {item['bytes']} |")
for name, item in stats.items():
    description, collision = notes[name]
    lines.extend(["", f"### {name}", "", description, "",
                  f"- File: `{item['file']}`",
                  f"- Bounds min (m): `{item['bounds_min_m']}`",
                  f"- Bounds max (m): `{item['bounds_max_m']}`",
                  f"- SHA-256: `{item['sha256']}`",
                  f"- Collision (sizes X × Y × Z): {collision}",
                  "- Source: original procedural geometry."])
lines.extend(["", "## Provenance and dependencies", "",
              "No third-party mesh or texture licensing obligations introduced.",
              "Generator dependency: trimesh (MIT); NumPy is its numerical dependency.",
              "Existing third-party repository assets retain their separate provenance.",
              "Generated geometry is original project work; this document does not",
              "assign a new distribution licence to the project.", ""])
(ROOT / "docs/SURVIVAL_ASSET_SOURCES.md").write_text("\n".join(lines))
print(json.dumps(stats, indent=2))
