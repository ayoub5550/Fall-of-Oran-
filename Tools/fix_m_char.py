"""Patch M_Char in place: Normal default -> DefaultNormal, enable SkeletalMesh usage, recompile, save.
Run: UnrealEditor-Cmd FallOfOran.uproject -run=pythonscript -script=Tools/fix_m_char.py -nullrhi -unattended"""
import unreal
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
mat = unreal.load_asset("/Game/Materials/M_Char")
mat.set_editor_property("used_with_skeletal_mesh", True)
dn = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
n = 0
for e in MEL.get_material_expressions(mat):
    if isinstance(e, unreal.MaterialExpressionTextureSampleParameter2D) and str(e.get_editor_property("parameter_name")) == "Normal":
        e.set_editor_property("texture", dn); n += 1
unreal.log(f"FIX_M_CHAR patched {n} normal params")
MEL.recompile_material(mat)
EAL.save_loaded_asset(mat)
for path in EAL.list_assets("/Game/Chars", recursive=True):
    a = unreal.load_asset(path.split(".")[0])
    if isinstance(a, unreal.MaterialInstanceConstant):
        MEL.update_material_instance(a); EAL.save_loaded_asset(a)
unreal.log("FIX_M_CHAR done")
