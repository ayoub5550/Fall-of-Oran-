"""Additive material repair for the five checkpoint assets.

Run in Unreal's Python commandlet AFTER import_checkpoint_props.py.
Original meshes store their palette as vertex colors, not imported materials.
Collision is provided by explicit runtime box components in C++.
"""
import json
from pathlib import Path
import unreal as u

eal = u.EditorAssetLibrary
mel = u.MaterialEditingLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
folder = "/Game/Props/Checkpoint"
name = "M_CheckpointVertex"
mat = eal.load_asset(folder + "/" + name) if eal.does_asset_exist(folder + "/" + name) else None
if mat is None:
    mat = tools.create_asset(name, folder, u.Material, u.MaterialFactoryNew())
mel.delete_all_material_expressions(mat)
vc = mel.create_material_expression(mat, u.MaterialExpressionVertexColor, -500, 0)
assert mel.connect_material_property(vc, "", u.MaterialProperty.MP_BASE_COLOR)
floor = mel.create_material_expression(mat, u.MaterialExpressionMultiply, -250, 150)
floor.set_editor_property("const_b", 0.22)
assert mel.connect_material_expressions(vc, "", floor, "A")
assert mel.connect_material_property(floor, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
rough = mel.create_material_expression(mat, u.MaterialExpressionConstant, -200, 350)
rough.set_editor_property("r", 0.85)
assert mel.connect_material_property(rough, "", u.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_loaded_asset(mat, only_if_is_dirty=False)
report = []
for name in ("SM_CheckpointGenerator", "SM_CheckpointBarrier", "SM_SupplyBarrel",
             "SM_SupplyCrate", "SM_MaintenanceBench"):
    mesh = eal.load_asset(f"{folder}/{name}/{name}")
    assert isinstance(mesh, u.StaticMesh), name
    if name in ("SM_CheckpointGenerator", "SM_CheckpointBarrier"):
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, mat)
    # Collision uses explicit runtime UBoxComponents; editor subsystems are
    # unavailable in Python commandlets. Do not silently claim collision baking.
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    report.append({"mesh": name, "collision": "runtime box component",
                   "vertex_material": name.startswith("SM_Checkpoint")})
(Path(u.Paths.project_saved_dir()) / "checkpoint-finish.json").write_text(
    json.dumps({"passed": True, "assets": report}, indent=2))
