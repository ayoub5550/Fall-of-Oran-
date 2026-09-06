"""Fall of Oran - headless asset import for UE5 (run inside UnrealEditor-Cmd).

Usage (from the engine root, no GPU needed):
  Engine/Binaries/Linux/UnrealEditor-Cmd <proj>/FallOfOran.uproject \
      -run=pythonscript -script=<proj>/Tools/import_assets.py -nullrhi -unattended -nopause

Reads RawAssets/ (textures, audio, char_materials.json), the Mixamo FBX folder
and the legacy Godot GLB props, and creates every asset the C++ code expects:
  /Game/Textures/*, /Game/Materials/M_*, /Game/Chars/**, /Game/Props/*,
  /Game/Audio/*, /Game/Maps/Oran  (font: raw TTF in Content/Fonts, importing UFont crashes in commandlets)
Progress is written to <proj>/Saved/import_assets.log (print() is invisible
in commandlets).
"""
import json
import os
import traceback

import unreal

PROJ = unreal.Paths.project_dir().rstrip("/")
RAW = os.path.join(PROJ, "RawAssets")
MIXAMO = os.environ.get("FO_MIXAMO", "/work/downloads/mixamo")
GODOT_ASSETS = os.environ.get("FO_GODOT_ASSETS", "/work/repos/fall-of-oran/assets")
FONT_TTF = os.environ.get("FO_FONT", "/usr/share/fonts/truetype/noto/NotoKufiArabic-Regular.ttf")
LOG_PATH = os.path.join(PROJ, "Saved", "import_assets.log")

os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
_log = open(LOG_PATH, "w", encoding="utf-8")


def log(*a):
    s = " ".join(str(x) for x in a)
    _log.write(s + "\n")
    _log.flush()
    unreal.log("[FO-IMPORT] " + s)


AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


# --------------------------------------------------------------------------- helpers
def import_file(src, dest_path, dest_name=None, options=None, replace=True):
    """Import one file with AssetImportTask. Returns list of imported object paths."""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", dest_path)
    if dest_name:
        task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", replace)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    AT.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    if not paths:
        log("  !! import produced nothing:", src)
    return paths


def save(asset):
    EAL.save_loaded_asset(asset, only_if_is_dirty=False)


def texture_import(src, dest_path, name, normal=False, linear=False, max_size=0, clamp=False):
    paths = import_file(src, dest_path, name)
    if not paths:
        return None
    tex = unreal.load_asset(paths[0])
    if normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif linear:
        tex.set_editor_property("srgb", False)
    if max_size:
        tex.set_editor_property("max_texture_size", max_size)
    if clamp:
        tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    save(tex)
    return tex


# --------------------------------------------------------------------------- textures
def import_textures():
    log("== textures")
    tdir = os.path.join(RAW, "Textures")
    for f in sorted(os.listdir(tdir)):
        p = os.path.join(tdir, f)
        if not os.path.isfile(p):
            continue
        name = os.path.splitext(f)[0]
        normal = name.endswith("_normal")
        linear = name.endswith("_rough")
        is_pbr = normal or linear or name.endswith("_color")
        texture_import(p, "/Game/Textures", name, normal=normal, linear=linear,
                       max_size=1024 if is_pbr else 0,
                       clamp=name in ("T_blood", "T_puddle", "T_palm_frond", "T_window_lit", "T_window_dark"))
        log("  tex", name)
    sdir = os.path.join(tdir, "Signs")
    for f in sorted(os.listdir(sdir)):
        name = os.path.splitext(f)[0]
        texture_import(os.path.join(sdir, f), "/Game/Textures/Signs", name, clamp=True)
        log("  sign", name)


# --------------------------------------------------------------------------- materials
def new_material(name, path="/Game/Materials"):
    full = f"{path}/{name}"
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    mat = AT.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def expr(mat, cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def scalar_param(mat, name, default, x, y):
    return expr(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)


def vector_param(mat, name, default, x, y):
    return expr(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=default)


def tex_param(mat, name, tex, x, y, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
    e = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, sampler_type=sampler)
    if tex:
        e.set_editor_property("texture", tex)
    return e


def connect(a, a_out, b, b_in):
    ok = MEL.connect_material_expressions(a, a_out, b, b_in)
    if not ok:
        log(f"  !! connect failed {a.get_name()}.{a_out} -> {b.get_name()}.{b_in}")
    return ok


def connect_prop(e, out, prop):
    ok = MEL.connect_material_property(e, out, prop)
    if not ok:
        log(f"  !! connect_property failed {e.get_name()}.{out} -> {prop}")
    return ok


def finish(mat):
    MEL.recompile_material(mat)
    save(mat)
    log("  material", mat.get_name())


def T(name):
    return unreal.load_asset(f"/Game/Textures/{name}")


def build_m_pbr():
    """World-aligned tiling PBR for the procedural boxes (axis aligned).
    UV = (floor: world XY | wall: world (X+Y), Z) / TileCm, chosen by |normal.z|."""
    mat = new_material("M_PBR")
    tile = scalar_param(mat, "TileCm", 300.0, -1500, -200)
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -1500, 0)
    m_xy = expr(mat, unreal.MaterialExpressionComponentMask, -1300, -60, r=True, g=True, b=False, a=False)
    m_x = expr(mat, unreal.MaterialExpressionComponentMask, -1300, 40, r=True, g=False, b=False, a=False)
    m_y = expr(mat, unreal.MaterialExpressionComponentMask, -1300, 120, r=False, g=True, b=False, a=False)
    m_z = expr(mat, unreal.MaterialExpressionComponentMask, -1300, 200, r=False, g=False, b=True, a=False)
    connect(wp, "", m_xy, "")
    connect(wp, "", m_x, "")
    connect(wp, "", m_y, "")
    connect(wp, "", m_z, "")
    add_xy = expr(mat, unreal.MaterialExpressionAdd, -1150, 80)
    connect(m_x, "", add_xy, "A")
    connect(m_y, "", add_xy, "B")
    app = expr(mat, unreal.MaterialExpressionAppendVector, -1000, 120)
    connect(add_xy, "", app, "A")
    connect(m_z, "", app, "B")
    # blend factor from vertex normal
    vn = expr(mat, unreal.MaterialExpressionVertexNormalWS, -1300, 320)
    vn_z = expr(mat, unreal.MaterialExpressionComponentMask, -1150, 320, r=False, g=False, b=True, a=False)
    connect(vn, "", vn_z, "")
    vabs = expr(mat, unreal.MaterialExpressionAbs, -1000, 320)
    connect(vn_z, "", vabs, "")
    vround = expr(mat, unreal.MaterialExpressionRound, -880, 320)
    connect(vabs, "", vround, "")
    lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, -800, 60)
    connect(app, "", lerp, "A")     # wall
    connect(m_xy, "", lerp, "B")    # floor
    connect(vround, "", lerp, "Alpha")
    div = expr(mat, unreal.MaterialExpressionDivide, -650, 60)
    connect(lerp, "", div, "A")
    connect(tile, "", div, "B")

    base = tex_param(mat, "BaseColor", T("T_plaster_color"), -450, -250)
    nrm = tex_param(mat, "Normal", T("T_plaster_normal"), -450, 50, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rgh = tex_param(mat, "Rough", T("T_plaster_rough"), -450, 350, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    for s in (base, nrm, rgh):
        connect(div, "", s, "UVs")

    tint = vector_param(mat, "Tint", unreal.LinearColor(1, 1, 1, 1), -450, -450)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -200, -300)
    connect(base, "RGB", mul, "A")
    connect(tint, "", mul, "B")
    connect_prop(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

    emis = vector_param(mat, "Emissive", unreal.LinearColor(0, 0, 0, 1), -450, -600)
    connect_prop(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    connect_prop(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)

    rmul = scalar_param(mat, "RoughMul", 1.0, -450, 550)
    rm = expr(mat, unreal.MaterialExpressionMultiply, -200, 400)
    connect(rgh, "R", rm, "A")
    connect(rmul, "", rm, "B")
    connect_prop(rm, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat)


def build_m_flat():
    mat = new_material("M_Flat")
    tint = vector_param(mat, "Tint", unreal.LinearColor(0.5, 0.5, 0.5, 1), -400, -200)
    emis = vector_param(mat, "Emissive", unreal.LinearColor(0, 0, 0, 1), -400, 0)
    rough = scalar_param(mat, "Roughness", 0.6, -400, 200)
    metal = scalar_param(mat, "Metallic", 0.0, -400, 300)
    connect_prop(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    connect_prop(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    connect_prop(metal, "", unreal.MaterialProperty.MP_METALLIC)
    finish(mat)


def build_m_uv(name, masked):
    mat = new_material(name)
    if masked:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("two_sided", True)
    tex = tex_param(mat, "Tex", T("T_window_lit"), -600, -100)
    tint = vector_param(mat, "Tint", unreal.LinearColor(1, 1, 1, 1), -600, -350)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -350, -200)
    connect(tex, "RGB", mul, "A")
    connect(tint, "", mul, "B")
    connect_prop(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    emul = scalar_param(mat, "EmisMul", 0.0, -600, 150)
    em = expr(mat, unreal.MaterialExpressionMultiply, -350, 100)
    connect(mul, "", em, "A")
    connect(emul, "", em, "B")
    connect_prop(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = scalar_param(mat, "Roughness", 0.7, -600, 300)
    connect_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if masked:
        connect_prop(tex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(mat)


def build_m_decal():
    mat = new_material("M_Decal")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    tex = tex_param(mat, "Tex", T("T_blood"), -600, -100)
    tint = vector_param(mat, "Tint", unreal.LinearColor(1, 1, 1, 1), -600, -350)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -350, -200)
    connect(tex, "RGB", mul, "A")
    connect(tint, "", mul, "B")
    connect_prop(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    connect_prop(tex, "A", unreal.MaterialProperty.MP_OPACITY)
    rough = scalar_param(mat, "Roughness", 0.3, -600, 200)
    metal = scalar_param(mat, "Metallic", 0.0, -600, 300)
    connect_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    connect_prop(metal, "", unreal.MaterialProperty.MP_METALLIC)
    finish(mat)


def build_m_sky():
    mat = new_material("M_Sky")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    tex = tex_param(mat, "Tex", T("T_sky_pano"), -600, -100)
    emul = scalar_param(mat, "EmisMul", 1.0, -600, 150)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -350, 0)
    connect(tex, "RGB", mul, "A")
    connect(emul, "", mul, "B")
    connect_prop(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


def build_m_char():
    mat = new_material("M_Char")
    mat.set_editor_property("two_sided", True)
    base = tex_param(mat, "BaseColor", None, -600, -200)
    nrm = tex_param(mat, "Normal", None, -600, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    tint = vector_param(mat, "Tint", unreal.LinearColor(1, 1, 1, 1), -600, -400)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -350, -250)
    connect(base, "RGB", mul, "A")
    connect(tint, "", mul, "B")
    connect_prop(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    connect_prop(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    rough = scalar_param(mat, "Roughness", 0.75, -600, 350)
    connect_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat)


def build_materials():
    log("== materials")
    build_m_pbr()
    build_m_flat()
    build_m_uv("M_UV", False)
    build_m_uv("M_UVMasked", True)
    build_m_decal()
    build_m_sky()
    build_m_char()


# --------------------------------------------------------------------------- characters
def fbx_options(skeletal, skeleton=None, anim_only=False):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("automated_import_should_detect_type", False)
    ui.set_editor_property("import_mesh", not anim_only)
    ui.set_editor_property("import_as_skeletal", skeletal)
    ui.set_editor_property("import_animations", anim_only)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", not anim_only)
    if anim_only:
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    else:
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    if skeleton:
        ui.set_editor_property("skeleton", skeleton)
    smd = ui.get_editor_property("skeletal_mesh_import_data")
    smd.set_editor_property("import_morph_targets", False)
    smd.set_editor_property("update_skeleton_reference_pose", False)
    smd.set_editor_property("use_t0_as_ref_pose", False)
    smd.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    smd.set_editor_property("convert_scene", True)
    ad = ui.get_editor_property("anim_sequence_import_data")
    ad.set_editor_property("import_bone_tracks", True)
    ad.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    ad.set_editor_property("remove_redundant_keys", True)
    ad.set_editor_property("convert_scene", True)
    return ui


def import_char_textures(char, dest):
    fbm = os.path.join(MIXAMO, char + ".fbm")
    out = {}
    if not os.path.isdir(fbm):
        log("  !! no fbm for", char)
        return out
    for f in sorted(os.listdir(fbm)):
        low = f.lower()
        if not (low.endswith(".png") or low.endswith(".jpg")):
            continue
        if not ("diffuse" in low or "normal" in low):
            continue
        name = "T_" + os.path.splitext(f)[0].replace(" ", "_")
        tex = texture_import(os.path.join(fbm, f), dest + "/Tex", name, normal="normal" in low, max_size=1024)
        if tex:
            out[f] = tex
    return out


def assign_char_materials(sk, char, slots, texmap, dest):
    m_char = unreal.load_asset("/Game/Materials/M_Char")
    mats = list(sk.get_editor_property("materials"))
    by_slot = {s["name"]: s for s in slots}
    new_mats = []
    for i, sm in enumerate(mats):
        slot = str(sm.get_editor_property("material_slot_name"))
        info = by_slot.get(slot) or by_slot.get(slot.replace(" ", "_"))
        if info is None and len(slots) == 1:
            info = slots[0]
        if info is None:
            # fuzzy: first slot whose name is contained
            for k, v in by_slot.items():
                if k.lower() in slot.lower() or slot.lower() in k.lower():
                    info = v
                    break
        if info is None:
            log(f"  !! no material info for slot '{slot}' on {char}; using first")
            info = slots[0]
        mic_name = f"MI_{char}_{i}"
        mic_path = f"{dest}/{mic_name}"
        if EAL.does_asset_exist(mic_path):
            EAL.delete_asset(mic_path)
        mic = AT.create_asset(mic_name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mic, m_char)
        if info.get("base") in texmap:
            MEL.set_material_instance_texture_parameter_value(mic, "BaseColor", texmap[info["base"]])
        if info.get("normal") in texmap:
            MEL.set_material_instance_texture_parameter_value(mic, "Normal", texmap[info["normal"]])
        MEL.update_material_instance(mic)
        save(mic)
        sm.set_editor_property("material_interface", mic)
        new_mats.append(sm)
        log(f"  slot {i} '{slot}' -> {mic_name} ({info.get('base')})")
    sk.set_editor_property("materials", new_mats)
    save(sk)


def import_characters():
    log("== characters")
    with open(os.path.join(RAW, "char_materials.json"), encoding="utf-8") as f:
        charmats = json.load(f)

    plan = [
        # (char, dest folder, asset name, skeleton group)
        ("hero_swat", "/Game/Chars/Hero", "SK_hero_swat", "hero"),
        ("z_war", "/Game/Chars/Zombies", "SK_z_war", "zombie"),
        ("z_girl", "/Game/Chars/Zombies", "SK_z_girl", "zombie"),
        ("z_cop", "/Game/Chars/Zombies", "SK_z_cop", "zombie"),
        ("z_parasite", "/Game/Chars/Zombies", "SK_z_parasite", "zombie"),
    ]
    skeletons = {}
    for char, dest, name, group in plan:
        src = os.path.join(MIXAMO, char + ".fbx")
        if not os.path.isfile(src):
            log("  !! missing", src)
            continue
        opts = fbx_options(True, skeletons.get(group))
        paths = import_file(src, dest, name, opts)
        sk = None
        for p in paths:
            o = unreal.load_asset(p)
            if isinstance(o, unreal.SkeletalMesh):
                sk = o
        if sk is None:
            log("  !! skeletal mesh import failed for", char, paths)
            continue
        skel = sk.get_editor_property("skeleton")
        if group not in skeletons and skel:
            # rename skeleton to a stable name
            want = f"{dest}/SKEL_{group}"
            if skel.get_path_name().split(".")[0] != want:
                if EAL.does_asset_exist(want):
                    EAL.delete_asset(want)
                EAL.rename_asset(skel.get_path_name(), want)
                skel = unreal.load_asset(want)
            skeletons[group] = skel
        log("  mesh", name, "bones:", skel.get_name() if skel else None)
        texmap = import_char_textures(char, dest)
        assign_char_materials(sk, char, charmats.get(char, []), texmap, dest)
        save(sk)

    # animations
    anim_plan = [("h_", "/Game/Chars/Hero/Anims", "hero"), ("z_", "/Game/Chars/Zombies/Anims", "zombie")]
    for prefix, dest, group in anim_plan:
        skel = skeletons.get(group)
        if not skel:
            log("  !! no skeleton for", group)
            continue
        for f in sorted(os.listdir(MIXAMO)):
            if f.startswith(prefix) and f.endswith(".fbx") and not f.startswith("hero_"):
                name = os.path.splitext(f)[0]
                paths = import_file(os.path.join(MIXAMO, f), dest, name, fbx_options(True, skel, anim_only=True))
                got = [p for p in paths if unreal.load_asset(p) and isinstance(unreal.load_asset(p), unreal.AnimSequence)]
                if got:
                    anim = unreal.load_asset(got[0])
                    # normalise asset name to the clip name the code expects
                    want = f"{dest}/{name}"
                    if anim.get_path_name().split(".")[0] != want:
                        if EAL.does_asset_exist(want):
                            EAL.delete_asset(want)
                        EAL.rename_asset(anim.get_path_name(), want)
                    log("  anim", name)
                else:
                    log("  !! anim import failed", f, paths)
    return skeletons


# --------------------------------------------------------------------------- props (GLB via Interchange)
def glb_options():
    """Interchange pipeline: merge all meshes of the GLB into one static mesh (cars ship wheels as
    separate nodes), keep the glTF materials, no skeletal/animation."""
    pipe = unreal.InterchangeGenericAssetsPipeline()
    mp = pipe.get_editor_property("mesh_pipeline")
    mp.set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    mp.set_editor_property("import_static_meshes", True)
    mp.set_editor_property("import_skeletal_meshes", False)
    cm = pipe.get_editor_property("common_meshes_properties")
    cm.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    ap = pipe.get_editor_property("animation_pipeline")
    ap.set_editor_property("import_animations", False)
    ov = unreal.InterchangePipelineStackOverride()
    ov.add_pipeline(pipe)
    return ov


def import_props():
    log("== props")
    wanted = ["car_Cop", "car_NormalCar1", "car_NormalCar2", "car_SUV", "car_Taxi",
              "TrafficLight", "TrafficLight_2", "Sign_Stop", "Sign_NoParking", "Sign_Triangle",
              "Streetlight_Single", "Streetlight_Double"]
    for w in wanted:
        src = os.path.join(GODOT_ASSETS, w + ".glb")
        if not os.path.isfile(src):
            log("  !! missing", src)
            continue
        sub = f"/Game/Props/src_{w}"
        if EAL.does_directory_exist(sub):
            EAL.delete_directory(sub)
        want = f"/Game/Props/{w}"
        if EAL.does_asset_exist(want):
            EAL.delete_asset(want)
        paths = import_file(src, sub, None, glb_options())
        meshes = []
        for p in EAL.list_assets(sub, recursive=True, include_folder=False):
            o = EAL.load_asset(p)
            if isinstance(o, unreal.StaticMesh):
                meshes.append(o)
        if not meshes:
            log("  !! no static mesh from", w, paths)
            continue
        mesh = meshes[0]
        if len(meshes) > 1:
            log(f"  !! {w} produced {len(meshes)} meshes (combine failed?), using {mesh.get_name()}")
        EAL.rename_asset(mesh.get_path_name(), want)
        mesh = unreal.load_asset(want)
        save(mesh)
        log("  prop", w, "materials:", len(mesh.get_editor_property("static_materials")),
            "tris:", mesh.get_num_triangles(0) if hasattr(mesh, "get_num_triangles") else "?")


# --------------------------------------------------------------------------- audio / font / map
def import_audio():
    log("== audio")
    adir = os.path.join(RAW, "Audio")
    for f in sorted(os.listdir(adir)):
        if not f.endswith(".wav"):
            continue
        name = os.path.splitext(f)[0]
        paths = import_file(os.path.join(adir, f), "/Game/Audio", name)
        if paths:
            sw = unreal.load_asset(paths[0])
            if name in ("ambience", "heartbeat"):
                sw.set_editor_property("looping", True)
            save(sw)
            log("  sound", name)


def import_font():
    log("== font")
    if not os.path.isfile(FONT_TTF):
        log("  !! font missing", FONT_TTF)
        return
    paths = import_file(FONT_TTF, "/Game/UI", "F_Kufi")
    log("  font ->", paths)
    for p in paths:
        o = unreal.load_asset(p)
        if isinstance(o, unreal.Font):
            want = "/Game/UI/F_Kufi"
            if o.get_path_name().split(".")[0] != want:
                if EAL.does_asset_exist(want):
                    EAL.delete_asset(want)
                EAL.rename_asset(o.get_path_name(), want)
            log("  font asset ok")


def create_map():
    log("== map")
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    path = "/Game/Maps/Oran"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    ok = les.new_level(path)
    log("  new_level", ok)
    world = unreal.EditorLevelLibrary.get_editor_world()
    ws = world.get_world_settings() if world else None
    if ws:
        gm = unreal.load_class(None, "/Script/FallOfOran.FOGameMode")
        if gm:
            ws.set_editor_property("default_game_mode", gm)
            log("  game mode override set")
        else:
            log("  !! FOGameMode class not found")
    saved = les.save_current_level()
    log("  saved", saved)


# --------------------------------------------------------------------------- main
STEPS = os.environ.get("FO_STEPS", "textures,materials,characters,props,audio,map").split(",")


def main():
    log("project:", PROJ, "steps:", STEPS)
    for step in STEPS:
        try:
            {"textures": import_textures, "materials": build_materials, "characters": import_characters,
             "props": import_props, "audio": import_audio, "font": import_font, "map": create_map}[step]()
        except Exception:
            log("!! step failed:", step)
            log(traceback.format_exc())
    EAL.save_directory("/Game", only_if_is_dirty=True, recursive=True)
    log("DONE")


main()
