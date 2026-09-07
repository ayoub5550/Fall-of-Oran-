"""Import original survival GLBs additively; preserve campaign/map/materials."""
import json
from pathlib import Path
import unreal as u

p = Path(u.Paths.project_dir())
eal = u.EditorAssetLibrary
at = u.AssetToolsHelpers.get_asset_tools()
report = {"passed": False, "assets": []}
result_file = p / "Saved/survival-import.json"
try:
    material = eal.load_asset("/Game/Props/Checkpoint/M_CheckpointVertex")
    assert material is not None, "run finish_checkpoint_assets.py first"
    sources = sorted((p / "RawAssets/Survival/Original").glob("*.glb"))
    assert len(sources) == 6, f"Expected six original meshes, got {len(sources)}"
    for src in sources:
        name = src.stem
        dest = "/Game/Props/Survival/" + name
        pipe = u.InterchangeGenericAssetsPipeline()
        meshes = pipe.get_editor_property("mesh_pipeline")
        meshes.set_editor_property("combine_static_meshes_behavior", u.InterchangeCombineStaticMeshesBehavior.ALL)
        meshes.set_editor_property("import_static_meshes", True)
        meshes.set_editor_property("import_skeletal_meshes", False)
        pipe.get_editor_property("common_meshes_properties").set_editor_property(
            "force_all_mesh_as_type", u.InterchangeForceMeshType.IFMT_STATIC_MESH)
        pipe.get_editor_property("animation_pipeline").set_editor_property("import_animations", False)
        overrides = u.InterchangePipelineStackOverride()
        overrides.add_pipeline(pipe)
        task = u.AssetImportTask()
        for k, v in dict(filename=str(src), destination_path=dest, automated=True,
                         replace_existing=True, save=True, options=overrides).items():
            task.set_editor_property(k, v)
        at.import_asset_tasks([task])
        found = [eal.load_asset(a) for a in eal.list_assets(dest, recursive=True, include_folder=False)]
        found = [a for a in found if isinstance(a, u.StaticMesh)]
        assert len(found) == 1, f"{name}: must be one combined mesh"
        mesh = found[0]
        want = dest + "/" + name
        if mesh.get_path_name().split(".")[0] != want:
            assert eal.rename_asset(mesh.get_path_name(), want), name
        for index in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(index, material)
        eal.save_loaded_asset(mesh, only_if_is_dirty=False)
        report["assets"].append({"name": name, "asset": want,
                                  "triangles": mesh.get_num_triangles(0)})
    report["passed"] = True
except Exception as error:
    report["error"] = str(error)
    raise
finally:
    result_file.write_text(json.dumps(report, indent=2))
