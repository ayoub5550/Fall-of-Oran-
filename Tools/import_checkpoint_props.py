"""Additive import only: never rerun the full destructive map/material importer."""
import unreal, os, json
from pathlib import Path
p=Path(unreal.Paths.project_dir())
at=unreal.AssetToolsHelpers.get_asset_tools()
eal=unreal.EditorAssetLibrary
report=[]
plan=[("Kenney/barrel.glb","SM_SupplyBarrel"),("Kenney/box-large.glb","SM_SupplyCrate"),
      ("Kenney/workbench.glb","SM_MaintenanceBench"),
      ("Original/SM_CheckpointGenerator.glb","SM_CheckpointGenerator"),
      ("Original/SM_CheckpointBarrier.glb","SM_CheckpointBarrier")]
try:
 for src,name in plan:
    dest="/Game/Props/Checkpoint/"+name
    pipe=unreal.InterchangeGenericAssetsPipeline()
    mp=pipe.get_editor_property("mesh_pipeline")
    mp.set_editor_property("combine_static_meshes_behavior",unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    mp.set_editor_property("import_static_meshes",True)
    mp.set_editor_property("import_skeletal_meshes",False)
    pipe.get_editor_property("common_meshes_properties").set_editor_property("force_all_mesh_as_type",unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    pipe.get_editor_property("animation_pipeline").set_editor_property("import_animations",False)
    ov=unreal.InterchangePipelineStackOverride();ov.add_pipeline(pipe)
    task=unreal.AssetImportTask()
    for k,v in dict(filename=str(p/"RawAssets/Checkpoint"/src),destination_path=dest,automated=True,
                    replace_existing=True,save=True,options=ov).items():task.set_editor_property(k,v)
    at.import_asset_tasks([task])
    meshes=[eal.load_asset(a) for a in eal.list_assets(dest,recursive=True,include_folder=False)]
    meshes=[a for a in meshes if isinstance(a,unreal.StaticMesh)]
    if len(meshes)!=1:raise RuntimeError(f"{name}: expected one combined mesh, got {len(meshes)}")
    mesh=meshes[0]
    want=dest+"/"+name
    if mesh.get_path_name().split(".")[0]!=want:
        if not eal.rename_asset(mesh.get_path_name(),want):raise RuntimeError("rename failed")
    eal.save_loaded_asset(mesh,only_if_is_dirty=False)
    report.append({"name":name,"asset":want,"triangles":mesh.get_num_triangles(0)})
 (p/"Saved/checkpoint-import.json").write_text(json.dumps({"passed":True,"assets":report},indent=2))
except Exception as ex:
 (p/"Saved/checkpoint-import.json").write_text(json.dumps({"passed":False,"error":str(ex),"assets":report},indent=2))
 raise
