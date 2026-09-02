extends CharacterBody3D
# Third-person player (RE4 Remake style over-the-shoulder camera)
# Desktop: WASD/arrows + mouse. Phone: left half = virtual joystick, right half = look.

signal died
signal damaged
signal shot_fired

const SPEED := 4.0
const GRAVITY := 18.0
const LOOK_SENS := 0.0045
const TOUCH_LOOK_SENS := 0.0038

var health := 100.0
var yaw := 0.0
var pitch := -0.12

var cam_pivot: Node3D
var camera: Camera3D
var flashlight: SpotLight3D
var body_mesh: Node3D
var anim: AnimationPlayer

# touch state
var move_touch_idx := -1
var move_touch_origin := Vector2.ZERO
var move_vec := Vector2.ZERO
var look_touch_idx := -1

var heartbeat: AudioStreamPlayer
var hurt_sound: AudioStreamPlayer
var gun_sound: AudioStreamPlayer
var click_sound: AudioStreamPlayer

# Weapon
var ammo := 12
var reserve := 36
const MAG_SIZE := 12
const GUN_DAMAGE := 40.0
const GUN_RANGE := 45.0
var reloading := false
var fire_cooldown := 0.0
var _bob_t := 0.0
var muzzle: OmniLight3D

func _ready() -> void:
	# Collision
	var col := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.35
	cap.height = 1.7
	col.shape = cap
	add_child(col)

	# Hero 3D model (Quaternius animated man, CC0)
	var hero_scene: PackedScene = load("res://assets/hero.glb")
	body_mesh = hero_scene.instantiate()
	var s := 0.75
	body_mesh.scale = Vector3(s, s, s)
	body_mesh.position.y = -0.85
	body_mesh.rotation.y = PI
	add_child(body_mesh)
	var hero_tex: Texture2D = load("res://assets/hero_tex.png")
	var hero_mat := StandardMaterial3D.new()
	hero_mat.albedo_texture = hero_tex
	hero_mat.albedo_color = Color(1.0, 1.0, 1.0)
	hero_mat.roughness = 0.85
	for mi in _find_meshes(body_mesh):
		mi.material_override = hero_mat
	anim = body_mesh.find_child("AnimationPlayer", true, false)
	if anim:
		for an in ["Human Armature|Idle", "Human Armature|Walk", "Human Armature|Run"]:
			var a: Animation = anim.get_animation(an)
			if a:
				a.loop_mode = Animation.LOOP_LINEAR
		anim.play("Human Armature|Idle")

	# Camera rig — over the right shoulder
	cam_pivot = Node3D.new()
	cam_pivot.position = Vector3(0, 1.0, 0)
	add_child(cam_pivot)
	camera = Camera3D.new()
	camera.position = Vector3(0.95, 0.55, 3.1)
	camera.fov = 70.0
	camera.current = true
	cam_pivot.add_child(camera)

	# Flashlight mounted at shoulder
	flashlight = SpotLight3D.new()
	flashlight.position = Vector3(0.4, 0.1, 0.3)
	flashlight.light_color = Color(1.0, 0.93, 0.8)
	flashlight.light_energy = 7.0
	flashlight.spot_range = 30.0
	flashlight.spot_angle = 30.0
	flashlight.spot_angle_attenuation = 1.6
	flashlight.shadow_enabled = true
	cam_pivot.add_child(flashlight)

	# v1.3: visible light beam (fake volumetric cone — mobile renderer has no volumetric fog)
	var beam := MeshInstance3D.new()
	var cone := CylinderMesh.new()
	cone.top_radius = 0.03
	cone.bottom_radius = 1.7
	cone.height = 12.0
	cone.radial_segments = 24
	cone.cap_top = false
	cone.cap_bottom = false
	beam.mesh = cone
	var bmat := StandardMaterial3D.new()
	bmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	bmat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	bmat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	bmat.albedo_color = Color(1.0, 0.92, 0.72, 0.028)
	bmat.cull_mode = BaseMaterial3D.CULL_DISABLED
	beam.material_override = bmat
	beam.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	beam.rotation_degrees.x = -90.0
	beam.position = Vector3(0, 0, -6.4)
	flashlight.add_child(beam)

	# Subtle character light so the hero reads against the dark
	var rim := OmniLight3D.new()
	rim.position = Vector3(0, 1.7, 1.0)
	rim.light_color = Color(0.5, 0.6, 0.8)
	rim.light_energy = 0.3
	rim.omni_range = 2.5
	add_child(rim)

	heartbeat = AudioStreamPlayer.new()
	heartbeat.stream = load("res://audio/heartbeat.wav")
	heartbeat.volume_db = -80.0
	heartbeat.autoplay = true
	add_child(heartbeat)
	heartbeat.finished.connect(heartbeat.play)

	hurt_sound = AudioStreamPlayer.new()
	hurt_sound.stream = load("res://audio/hit.wav")
	hurt_sound.volume_db = -4.0
	add_child(hurt_sound)

	gun_sound = AudioStreamPlayer.new()
	gun_sound.stream = load("res://audio/gunshot.wav")
	gun_sound.volume_db = -6.0
	add_child(gun_sound)

	click_sound = AudioStreamPlayer.new()
	click_sound.stream = load("res://audio/click.wav")
	add_child(click_sound)

	# Simple pistol held at the right side
	var gun := Node3D.new()
	gun.position = Vector3(0.32, 0.45, -0.25)
	add_child(gun)
	var gm := StandardMaterial3D.new()
	gm.albedo_color = Color(0.08, 0.08, 0.09)
	gm.roughness = 0.35
	gm.metallic = 0.7
	var slide := MeshInstance3D.new()
	var sm := BoxMesh.new()
	sm.size = Vector3(0.05, 0.07, 0.22)
	slide.mesh = sm
	slide.material_override = gm
	gun.add_child(slide)
	var grip := MeshInstance3D.new()
	var grm := BoxMesh.new()
	grm.size = Vector3(0.045, 0.12, 0.06)
	grip.mesh = grm
	grip.position = Vector3(0, -0.08, 0.06)
	grip.material_override = gm
	gun.add_child(grip)
	# Muzzle flash light
	muzzle = OmniLight3D.new()
	muzzle.position = Vector3(0, 0, -0.25)
	muzzle.light_color = Color(1.0, 0.8, 0.4)
	muzzle.light_energy = 0.0
	muzzle.omni_range = 6.0
	gun.add_child(muzzle)

func _main() -> Node:
	return get_parent()

func _playing() -> bool:
	var m := _main()
	return m != null and m.get("state") == 1  # GameState.PLAYING

func _input(event: InputEvent) -> void:
	if not _playing():
		return
	var vp_width := get_viewport().get_visible_rect().size.x
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		yaw -= event.relative.x * LOOK_SENS
		pitch = clampf(pitch - event.relative.y * LOOK_SENS, -0.9, 0.5)
	elif event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		if OS.get_name() == "Windows" or OS.get_name() == "macOS" or OS.get_name() == "Linux":
			if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
				try_shoot()
			else:
				Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	elif event is InputEventScreenTouch:
		if event.pressed:
			var fb := get_tree().get_first_node_in_group("fire_btn")
			if fb != null and (fb as Control).get_global_rect().grow(20).has_point(event.position):
				return
			if event.position.x < vp_width * 0.45 and move_touch_idx == -1:
				move_touch_idx = event.index
				move_touch_origin = event.position
				move_vec = Vector2.ZERO
			elif event.position.x >= vp_width * 0.45 and look_touch_idx == -1:
				look_touch_idx = event.index
		else:
			if event.index == move_touch_idx:
				move_touch_idx = -1
				move_vec = Vector2.ZERO
			elif event.index == look_touch_idx:
				look_touch_idx = -1
	elif event is InputEventScreenDrag:
		if event.index == move_touch_idx:
			var d: Vector2 = event.position - move_touch_origin
			move_vec = (d / 90.0).limit_length(1.0)
		elif event.index == look_touch_idx:
			yaw -= event.relative.x * TOUCH_LOOK_SENS
			pitch = clampf(pitch - event.relative.y * TOUCH_LOOK_SENS, -0.9, 0.5)

func _physics_process(delta: float) -> void:
	if not _playing():
		velocity = Vector3.ZERO
		return

	cam_pivot.rotation = Vector3(pitch, 0, 0)
	rotation.y = yaw

	var input_dir := Vector2.ZERO
	if Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP):
		input_dir.y -= 1
	if Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN):
		input_dir.y += 1
	if Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT):
		input_dir.x -= 1
	if Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT):
		input_dir.x += 1
	if input_dir == Vector2.ZERO:
		input_dir = move_vec
	input_dir = input_dir.limit_length(1.0)

	var dir := (transform.basis * Vector3(input_dir.x, 0, input_dir.y))
	# Smooth acceleration/deceleration for fluid movement
	var accel := 1.0 - exp(-9.0 * delta)
	velocity.x = lerpf(velocity.x, dir.x * SPEED, accel)
	velocity.z = lerpf(velocity.z, dir.z * SPEED, accel)
	if not is_on_floor():
		velocity.y -= GRAVITY * delta
	else:
		velocity.y = 0.0
	move_and_slide()

	fire_cooldown = maxf(0.0, fire_cooldown - delta)
	# Subtle camera bob while moving
	var sp := input_dir.length()
	_bob_t += delta * (7.0 if sp > 0.05 else 0.0)
	camera.position.y = 0.55 + sin(_bob_t) * 0.035 * sp
	camera.position.x = 0.95 + cos(_bob_t * 0.5) * 0.02 * sp
	_update_anim(sp)
	_update_heartbeat()

func try_shoot() -> void:
	if not _playing() or reloading or fire_cooldown > 0.0:
		return
	if ammo <= 0:
		click_sound.play()
		_reload()
		return
	ammo -= 1
	fire_cooldown = 0.35
	gun_sound.pitch_scale = randf_range(0.95, 1.05)
	gun_sound.play()
	shot_fired.emit()
	# Muzzle flash
	muzzle.light_energy = 3.0
	var tw := create_tween()
	tw.tween_property(muzzle, "light_energy", 0.0, 0.09)
	# Raycast from camera center
	var from := camera.global_position
	var dir := -camera.global_transform.basis.z
	var space := get_world_3d().direct_space_state
	var q := PhysicsRayQueryParameters3D.create(from, from + dir * GUN_RANGE)
	q.exclude = [get_rid()]
	var hit := space.intersect_ray(q)
	if hit and hit.collider != null and hit.collider.has_method("take_hit"):
		hit.collider.take_hit(GUN_DAMAGE)
		_blood_puff(hit.position)
	else:
		# Aim assist: generous cone check for touch screens
		var best: Node3D = null
		var best_score := 99.0
		for zb in get_tree().get_nodes_in_group("zombies"):
			if zb.get("dying"):
				continue
			var to_z: Vector3 = (zb.global_position + Vector3(0, 0.3, 0)) - from
			var d := to_z.length()
			if d > GUN_RANGE:
				continue
			var ang := dir.angle_to(to_z.normalized())
			# Distance-scaled cone: wider up close, tighter far away
			var limit := clampf(1.9 / maxf(d, 1.0), 0.12, 0.35)
			if ang < limit and d < best_score:
				best_score = d
				best = zb
		if best != null:
			best.take_hit(GUN_DAMAGE)
			_blood_puff(best.global_position + Vector3(0, 0.5, 0))
	if ammo <= 0:
		_reload()

func _blood_puff(at: Vector3) -> void:
	var p := MeshInstance3D.new()
	var pm := SphereMesh.new()
	pm.radius = 0.12
	pm.height = 0.24
	p.mesh = pm
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.5, 0.02, 0.02)
	mat.emission_enabled = true
	mat.emission = Color(0.4, 0.02, 0.02)
	p.material_override = mat
	get_parent().add_child(p)
	p.global_position = at
	var tw := p.create_tween()
	tw.set_parallel(true)
	tw.tween_property(p, "scale", Vector3(2.2, 2.2, 2.2), 0.25)
	tw.tween_property(p, "transparency", 1.0, 0.25)
	tw.chain().tween_callback(p.queue_free)

func _reload() -> void:
	if reloading or reserve <= 0 or ammo >= MAG_SIZE:
		return
	reloading = true
	await get_tree().create_timer(1.3).timeout
	var need := MAG_SIZE - ammo
	var take: int = mini(need, reserve)
	ammo += take
	reserve -= take
	reloading = false

func add_ammo(n: int) -> void:
	reserve += n
	pickup_flash()

func add_health(n: float) -> void:
	health = minf(100.0, health + n)
	pickup_flash()

func pickup_flash() -> void:
	pass

func _find_meshes(n: Node) -> Array:
	var out := []
	if n is MeshInstance3D:
		out.append(n)
	for c in n.get_children():
		out += _find_meshes(c)
	return out

func _update_anim(speed_frac: float) -> void:
	if anim == null:
		return
	var want := "Human Armature|Idle"
	if speed_frac > 0.65:
		want = "Human Armature|Run"
	elif speed_frac > 0.05:
		want = "Human Armature|Walk"
	if anim.current_animation != want:
		anim.play(want, 0.25)

func _update_heartbeat() -> void:
	# Heartbeat volume rises when zombies are near or health is low
	var nearest := 999.0
	for zb in get_tree().get_nodes_in_group("zombies"):
		nearest = minf(nearest, global_position.distance_to(zb.global_position))
	var fear := clampf(1.0 - nearest / 15.0, 0.0, 1.0)
	fear = maxf(fear, 1.0 - health / 60.0)
	heartbeat.volume_db = lerpf(-80.0, -6.0, clampf(fear, 0.0, 1.0))

func take_damage(amount: float) -> void:
	if not _playing():
		return
	health -= amount
	hurt_sound.play()
	emit_signal("damaged")
	if health <= 0.0:
		health = 0.0
		emit_signal("died")
