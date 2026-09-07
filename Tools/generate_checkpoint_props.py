"""Original low-poly checkpoint meshes. Run with: uv run --with trimesh python Tools/generate_checkpoint_props.py.
No reference meshes or images are copied. GLB uses metres, Y up.
"""
from pathlib import Path
import json
import trimesh

OUT = Path(__file__).resolve().parents[1] / "RawAssets/Checkpoint/Original"
OUT.mkdir(parents=True, exist_ok=True)
stats = {}

def box(scene, name, size, pos, color):
    m = trimesh.creation.box(extents=size)
    m.apply_translation(pos)
    m.visual.face_colors = color
    scene.add_geometry(m, node_name=name)

def save(scene, name):
    (OUT / (name + ".glb")).write_bytes(scene.export(file_type="glb"))
    stats[name] = {"triangles":sum(len(m.faces) for m in scene.geometry.values()),
                   "parts":len(scene.geometry), "source":"original procedural geometry"}

# Palette derives from the existing generator's olive/charcoal/yellow materials.
s=trimesh.Scene()
box(s,"frame",(1.3,.14,1.9),(0,.12,0),(25,25,25,255))
box(s,"engine",(1.08,.8,1.55),(0,.59,0),(77,71,20,255))
box(s,"top",(1.2,.12,1.75),(0,1.05,0),(22,22,22,255))
for i in range(9):
    box(s,f"vent{i}",(.02,.04,1.05),(-.551,.34+i*.07,0),(12,12,12,255))
box(s,"control_panel",(.04,.35,.5),(-.58,.75,.35),(30,32,34,255))
box(s,"status",(.045,.08,.1),(-.605,.83,.35),(230,191,26,255))
box(s,"exhaust",(.12,.65,.12),(.35,1.34,-.5),(35,35,35,255))
for x in [-.48,.48]:
    for z in [-.65,.65]:
        box(s,f"foot{x}{z}",(.18,.2,.25),(x,.10,z),(18,18,18,255))
save(s,"SM_CheckpointGenerator")

s=trimesh.Scene()
box(s,"base",(.65,.2,2.4),(0,.1,0),(110,107,102,255))
box(s,"body",(.35,.65,2.3),(0,.525,0),(130,125,117,255))
for i in range(8):
    box(s,f"hazard{i}",(.012,.22,.21),(-.182,.65,-.99+i*.28),
        (230,191,26,255) if i%2 else (22,22,22,255))
save(s,"SM_CheckpointBarrier")
(OUT/"manifest.json").write_text(json.dumps(stats,indent=2)+"\n")
print(json.dumps(stats))
