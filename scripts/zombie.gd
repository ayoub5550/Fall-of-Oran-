extends CharacterBody3D
# Zombie AI: shambles randomly until the player is close, then chases and attacks.
# Has health; dies from gunshots with a fall + fade.

const WALK_SPEED := 1.0
const CHASE_SPEED := 2.6
const DETECT_RANGE := 14.0
const ATTACK_RANGE := 1.7
const ATTACK_DAMAGE := 20.0
const ATTACK_COOLDOWN := 1.3
const GRAVITY := 18.0

signal killed

var target_path: NodePath
var target: CharacterBody3D
var chasing := false
var attack_timer := 0.0
var wander_dir := Vector3.ZERO
var wander_timer := 0.0
var groan: AudioStreamPlayer3D
var die_sound: AudioStreamPlayer3D
var groan_timer := 3.0
var anim: AnimationPlayer
var crawler := false
var hp := 100.0
var dying := false
var model: Node3D
var _rng := RandomNumberGenerator.new()

func _ready() -> void:
	add_to_group("zombies")
	_rng.randomize()
	var col := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.35
	cap.height = 1.7
	col.shape = cap
	add_child(col)
	_build_body()

	groan = AudioStreamPlayer3D.new()
	groan.stream = load("res://audio/groan.wav")
	groan.unit_size = 6.0
	groan.max_db = 0.0
	add_child(groan)

	die_sound = AudioStreamPlayer3D.new()
	die_sound.stream = load("res://audio/zombie_die.wav")
	die_sound.unit_size = 8.0
	add_child(die_sound)

func _build_body() -> void:
	# Realistic Mixamo zombie characters (see AGENT.md §7d). Two bodies, shared Mixamo rig.
	var girl := _rng.randf() < 0.55
	model = load("res://assets/chars/zombie_girl.glb" if girl else "res://assets/chars/zombie_jill.glb").instantiate()
	var s := (0.85 if girl else 0.98) * _rng.randf_range(0.94, 1.06)
	model.scale = Vector3(s, s, s)
	model.position.y = -0.85
	model.rotation.y = PI
	add_child(model)
	# Per-zombie decay tint (materials are duplicated per instance by AnimLib)
	var tints := [Color(0.95, 1.0, 0.92), Color(0.82, 0.86, 0.84), Color(1.0, 0.93, 0.88), Color(0.78, 0.84, 0.88), Color(0.9, 0.9, 0.9)]
	AnimLib.prep_materials(model, tints[_rng.randi() % tints.size()])
	anim = model.find_child("AnimationPlayer", true, false)
	var skel: Skeleton3D = model.find_child("Skeleton3D", true, false)
	if anim and skel:
		AnimLib.add_clip(anim, skel, "res://assets/chars/anim_z_idle.glb", "idle", true)
		AnimLib.add_clip(anim, skel, "res://assets/chars/anim_za_walk.glb" if girl else "res://assets/chars/anim_z_walk.glb", "walk", true)
		AnimLib.add_clip(anim, skel, "res://assets/chars/anim_z_run.glb", "run", true)
		AnimLib.add_clip(anim, skel, "res://assets/chars/anim_za_attack.glb", "bite", true)
		AnimLib.add_clip(anim, skel, "res://assets/chars/anim_za_death.glb", "death", false)
		anim.play("walk")
		anim.seek(_rng.randf_range(0.0, 1.0), true)
		anim.speed_scale = _rng.randf_range(0.85, 1.15)
	# faint red eye glow
	var eye := OmniLight3D.new()
	eye.position = Vector3(0, 0.75, -0.2)
	eye.light_color = Color(0.9, 0.1, 0.08)
	eye.light_energy = 0.1
	eye.omni_range = 0.9
	add_child(eye)

func _find_meshes(n: Node) -> Array:
	var out := []
	if n is MeshInstance3D:
		out.append(n)
	for c in n.get_children():
		out += _find_meshes(c)
	return out

func _playing() -> bool:
	var m := get_parent()
	return m != null and m.get("state") == 1

func take_hit(damage: float) -> void:
	if dying:
		return
	hp -= damage
	chasing = true
	if hp <= 0.0:
		_die()

func _die() -> void:
	dying = true
	killed.emit()
	die_sound.play()
	_spawn_blood_pool()
	set_collision_layer_value(1, false)
	set_collision_mask_value(1, false)
	if anim and anim.has_animation("death"):
		anim.speed_scale = 1.0
		anim.play("death", 0.15)
	# After the death animation, sink into the ground and free
	var tw := create_tween()
	tw.tween_interval(3.2)
	tw.tween_property(model, "position:y", -2.6, 2.0)
	tw.tween_callback(queue_free)

func _spawn_blood_pool() -> void:
	# Fresh blood pool that spreads under the corpse (v1.2 realism pass)
	var pm := PlaneMesh.new()
	pm.size = Vector2(1.7, 1.7)
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = load("res://assets/blood_decal.png")
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.roughness = 0.15
	var pool := MeshInstance3D.new()
	pool.mesh = pm
	pool.material_override = mat
	var parent := get_parent()
	if parent == null:
		return
	parent.add_child(pool)
	pool.global_position = Vector3(global_position.x, 0.045, global_position.z)
	pool.rotation.y = randf() * TAU
	pool.scale = Vector3(0.15, 1.0, 0.15)
	var tw := pool.create_tween()
	tw.tween_property(pool, "scale", Vector3(1.0, 1.0, 1.0), 2.2).set_ease(Tween.EASE_OUT)

func _physics_process(delta: float) -> void:
	if dying:
		velocity = Vector3.ZERO
		return
	if target == null and target_path != NodePath():
		target = get_node_or_null(target_path)
	if not _playing() or target == null:
		velocity = Vector3.ZERO
		return

	var to_player := target.global_position - global_position
	to_player.y = 0.0
	var dist := to_player.length()

	# Perf LOD: far zombies sleep (no AI, no anim) for smooth mobile FPS
	if dist > 45.0:
		if anim and anim.is_playing():
			anim.pause()
		velocity = Vector3.ZERO
		return
	elif anim and not anim.is_playing() and not dying:
		anim.play()

	if dist < DETECT_RANGE:
		chasing = true
	elif dist > DETECT_RANGE * 2.0:
		chasing = false

	var speed := CHASE_SPEED * (0.8 if crawler else 1.0)  # "crawler" flag now = slow shambler
	var move := Vector3.ZERO
	if chasing:
		move = to_player.normalized() * speed
		# Face the player
		if dist > 0.1:
			rotation.y = atan2(-to_player.x, -to_player.z) + PI
		attack_timer -= delta
		if dist <= ATTACK_RANGE and attack_timer <= 0.0:
			attack_timer = ATTACK_COOLDOWN
			target.take_damage(ATTACK_DAMAGE)
	else:
		wander_timer -= delta
		if wander_timer <= 0.0:
			wander_timer = _rng.randf_range(2.0, 5.0)
			var a := _rng.randf_range(0.0, TAU)
			wander_dir = Vector3(sin(a), 0, cos(a))
		move = wander_dir * WALK_SPEED
		rotation.y = atan2(-wander_dir.x, -wander_dir.z) + PI

	velocity.x = move.x
	velocity.z = move.z
	if not is_on_floor():
		velocity.y -= GRAVITY * delta
	else:
		velocity.y = 0.0
	move_and_slide()

	# Animation state
	if anim:
		var want := "run" if chasing else "walk"
		if dist <= ATTACK_RANGE + 0.3 and chasing:
			want = "bite"
		if anim.current_animation != want:
			anim.play(want, 0.3)

	# Random groans, louder while chasing
	groan_timer -= delta
	if groan_timer <= 0.0:
		groan_timer = _rng.randf_range(2.5, 7.0) if chasing else _rng.randf_range(6.0, 14.0)
		groan.pitch_scale = _rng.randf_range(0.75, 1.1)
		groan.play()
