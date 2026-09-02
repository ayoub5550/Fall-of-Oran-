class_name AnimLib
## Runtime retargeting of Mixamo-rig animation GLBs onto any Mixamo-rig character.
## All rigs must share bone names (mixamorig: prefix stripped offline).

static func add_clip(target_ap: AnimationPlayer, skel: Skeleton3D, glb_path: String, clip_name: String, loop: bool) -> bool:
	var sc: PackedScene = load(glb_path)
	if sc == null:
		push_warning("AnimLib: missing " + glb_path); return false
	var inst := sc.instantiate()
	var src_ap: AnimationPlayer = inst.find_child("AnimationPlayer", true, false)
	if src_ap == null or src_ap.get_animation_list().is_empty():
		inst.free(); return false
	var src: Animation = src_ap.get_animation(src_ap.get_animation_list()[0])
	var src_skel: Skeleton3D = inst.find_child("Skeleton3D", true, false)
	var hip_ratio := 1.0
	if src_skel and src_skel.find_bone("Hips") >= 0 and skel.find_bone("Hips") >= 0:
		var sh := src_skel.get_bone_rest(src_skel.find_bone("Hips")).origin.y
		var dh := skel.get_bone_rest(skel.find_bone("Hips")).origin.y
		if abs(sh) > 0.0001:
			hip_ratio = dh / sh
	var dst := Animation.new()
	dst.length = src.length
	dst.loop_mode = Animation.LOOP_LINEAR if loop else Animation.LOOP_NONE
	var root_node := target_ap.get_node(target_ap.root_node)
	var skel_path := str(root_node.get_path_to(skel))
	for t in src.get_track_count():
		var ttype := src.track_get_type(t)
		var bone := str(src.track_get_path(t).get_concatenated_subnames())
		if bone == "" or skel.find_bone(bone) < 0:
			continue
		if ttype == Animation.TYPE_SCALE_3D:
			continue
		if ttype == Animation.TYPE_POSITION_3D and bone != "Hips":
			continue
		var nt := dst.add_track(ttype)
		dst.track_set_path(nt, NodePath(skel_path + ":" + bone))
		dst.track_set_interpolation_type(nt, src.track_get_interpolation_type(t))
		for k in src.track_get_key_count(t):
			var tm := src.track_get_key_time(t, k)
			var v = src.track_get_key_value(t, k)
			if ttype == Animation.TYPE_POSITION_3D:
				# scale hips motion to target rig (handles cm vs m exports)
				dst.position_track_insert_key(nt, tm, v * hip_ratio)
			elif ttype == Animation.TYPE_ROTATION_3D:
				# re-express the animated delta in the target's rest frame
				var q: Quaternion = v
				if src_skel:
					var sb := src_skel.find_bone(bone)
					var db := skel.find_bone(bone)
					var src_rest := src_skel.get_bone_rest(sb).basis.get_rotation_quaternion()
					var dst_rest := skel.get_bone_rest(db).basis.get_rotation_quaternion()
					q = dst_rest * src_rest.inverse() * q
				dst.rotation_track_insert_key(nt, tm, q)
	inst.free()
	# IMPORTANT: instances of the same PackedScene share one AnimationLibrary resource.
	# Adding clips to the shared one crashes the other instances (their playing
	# animation gets freed) — so give this player its own copy first.
	var lib := target_ap.get_animation_library("")
	if lib == null:
		lib = AnimationLibrary.new(); target_ap.add_animation_library("", lib)
	elif not lib.has_meta("animlib_private"):
		lib = lib.duplicate()
		lib.set_meta("animlib_private", true)
		target_ap.remove_animation_library("")
		target_ap.add_animation_library("", lib)
	lib.add_animation(clip_name, dst)
	return true

static func prep_materials(model: Node, tint: Color = Color.WHITE) -> void:
	## Fix Mixamo exports: kill alpha (their TransparentColor makes bodies invisible), apply tint.
	for mi in model.find_children("*", "MeshInstance3D", true, false):
		var mesh: Mesh = mi.mesh
		if mesh == null: continue
		for i in mesh.get_surface_count():
			var m := mesh.surface_get_material(i)
			if m is BaseMaterial3D:
				var d: BaseMaterial3D = m.duplicate()
				d.transparency = BaseMaterial3D.TRANSPARENCY_DISABLED
				d.albedo_color = Color(tint.r, tint.g, tint.b, 1.0)
				d.roughness = 0.8
				mi.set_surface_override_material(i, d)
