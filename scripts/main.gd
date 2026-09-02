extends Node3D
# ============================================================
#  وهران: السقوط — Oran: The Fall (Vertical Slice)
#  Third-person survival horror — dark Oran street at night.
#  Reach the harbor gate at the end of the street. Survive.
# ============================================================

enum GameState { MENU, PLAYING, DEAD, WON }
var state: GameState = GameState.MENU
var kills := 0
var fuel := 0
const FUEL_NEEDED := 3
var ammo_label: Label
var kills_label: Label
var fuel_label: Label
var hint_timer := 0.0

var player: CharacterBody3D
var zombies: Array = []

# UI
var ui: CanvasLayer
var health_fill: ColorRect
var overlay: ColorRect
var title_label: Label
var subtitle_label: Label
var objective_label: Label
var damage_flash: ColorRect

const STREET_LENGTH := 90.0
const STREET_WIDTH := 12.0

var _rng := RandomNumberGenerator.new()

# Weather (v1.2 realism pass)
var moon: DirectionalLight3D
var _lightning_t := 7.0

func _ready() -> void:
	_rng.seed = 20260901
	_build_environment()
	_build_street()
	_spawn_player()
	_spawn_zombies()
	_spawn_pickups()
	_build_exit_gate()
	_build_ui()
	_start_ambience()
	_build_weather()
	_show_menu()

# ------------------------------------------------------------
# Environment: night, fog, moonlight
# ------------------------------------------------------------
func _build_environment() -> void:
	var env := Environment.new()
	# v1.3: real night sky — moon, clouds, Santa Cruz fort on the hill, port cranes
	env.background_mode = Environment.BG_SKY
	var sky := Sky.new()
	var sky_mat := PanoramaSkyMaterial.new()
	sky_mat.panorama = load("res://assets/tex/sky_pano.jpg")
	sky.sky_material = sky_mat
	env.sky = sky
	env.sky_rotation = Vector3(0, PI, 0) # moon + fort face down the street (-Z)
	env.background_energy_multiplier = 1.12
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.11, 0.13, 0.20)
	env.ambient_light_energy = 0.65
	env.fog_enabled = true
	env.fog_light_color = Color(0.055, 0.07, 0.11)
	env.fog_density = 0.026
	env.fog_sky_affect = 0.1
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.glow_enabled = true
	env.glow_intensity = 0.65
	env.glow_bloom = 0.2
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.12
	env.adjustment_saturation = 0.86
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)

	moon = DirectionalLight3D.new()
	moon.rotation_degrees = Vector3(-35, 140, 0)
	moon.light_color = Color(0.5, 0.6, 0.85)
	moon.light_energy = 0.34
	moon.shadow_enabled = true
	add_child(moon)

	# v1.3: baked reflections so wet asphalt/puddles mirror the actual street
	var probe := ReflectionProbe.new()
	probe.update_mode = ReflectionProbe.UPDATE_ONCE
	probe.size = Vector3(34, 24, STREET_LENGTH + 24)
	probe.position = Vector3(0, 10, -STREET_LENGTH / 2.0)
	probe.intensity = 0.8
	add_child(probe)

# ------------------------------------------------------------
# The street: ground, buildings, lamps, cars, debris
# ------------------------------------------------------------
func _mat(color: Color, rough := 0.9, emissive := Color(0, 0, 0)) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = rough
	if emissive.r + emissive.g + emissive.b > 0.0:
		m.emission_enabled = true
		m.emission = emissive
		m.emission_energy_multiplier = 1.6
	return m

func _merged_aabb(n: Node3D) -> AABB:
	var aabb := AABB()
	var first := true
	for mi in _all_meshes(n):
		var a: AABB = (mi.global_transform if mi.is_inside_tree() else mi.transform) * mi.get_aabb()
		aabb = a if first else aabb.merge(a)
		first = false
	return aabb

func _all_meshes(n: Node) -> Array:
	var out := []
	if n is MeshInstance3D:
		out.append(n)
	for c in n.get_children():
		out += _all_meshes(c)
	return out

func _pbr(tname: String, uv_scale: float, tint := Color(1, 1, 1)) -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = load("res://assets/tex/%s_color.jpg" % tname)
	mat.albedo_color = tint
	mat.roughness_texture = load("res://assets/tex/%s_rough.jpg" % tname)
	mat.normal_enabled = true
	mat.normal_texture = load("res://assets/tex/%s_normal.jpg" % tname)
	mat.uv1_triplanar = true
	mat.uv1_scale = Vector3(uv_scale, uv_scale, uv_scale)
	mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	return mat

func _box(size: Vector3, pos: Vector3, mat: StandardMaterial3D, collide := true) -> void:
	var body := StaticBody3D.new()
	body.position = pos
	var mesh := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mesh.mesh = bm
	mesh.material_override = mat
	body.add_child(mesh)
	if collide:
		var col := CollisionShape3D.new()
		var shape := BoxShape3D.new()
		shape.size = size
		col.shape = shape
		body.add_child(col)
	add_child(body)

func _build_street() -> void:
	var asphalt := _pbr("asphalt", 0.14, Color(0.42, 0.42, 0.48))
	asphalt.roughness = 0.55  # rain-slick
	_box(Vector3(STREET_WIDTH + 24, 0.4, STREET_LENGTH + 30), Vector3(0, -0.2, -STREET_LENGTH * 0.5), asphalt)

	# Marked road surface (real lane markings, wet sheen)
	var road_mesh := PlaneMesh.new()
	road_mesh.size = Vector2(STREET_WIDTH, STREET_LENGTH + 30)
	var road_mat := StandardMaterial3D.new()
	road_mat.albedo_texture = load("res://assets/tex/road_color.jpg")
	road_mat.albedo_color = Color(0.62, 0.62, 0.68)
	road_mat.roughness_texture = load("res://assets/tex/road_rough.jpg")
	road_mat.roughness = 0.5
	road_mat.normal_enabled = true
	road_mat.normal_texture = load("res://assets/tex/road_normal.jpg")
	road_mat.uv1_scale = Vector3(1.0, (STREET_LENGTH + 30.0) / 14.0, 1.0)
	var road := MeshInstance3D.new()
	road.mesh = road_mesh
	road.material_override = road_mat
	road.position = Vector3(0, 0.012, -STREET_LENGTH * 0.5)
	add_child(road)

	# Rain puddles (mirror-smooth, catch the lamp light)
	var puddle_mat := StandardMaterial3D.new()
	puddle_mat.albedo_texture = load("res://assets/puddle.png")
	puddle_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	puddle_mat.metallic = 1.0
	puddle_mat.roughness = 0.05
	puddle_mat.metallic_specular = 0.9
	for i in range(12):
		var pm := PlaneMesh.new()
		var ps := _rng.randf_range(1.6, 3.4)
		pm.size = Vector2(ps, ps * _rng.randf_range(0.6, 1.0))
		var pud := MeshInstance3D.new()
		pud.mesh = pm
		pud.material_override = puddle_mat
		pud.position = Vector3(_rng.randf_range(-STREET_WIDTH * 0.45, STREET_WIDTH * 0.45), 0.022 + 0.004 * (i % 3), _rng.randf_range(-STREET_LENGTH + 4, -6))
		pud.rotation.y = _rng.randf_range(0, TAU)
		add_child(pud)

	# Old blood stains on the asphalt
	var blood_mat := StandardMaterial3D.new()
	blood_mat.albedo_texture = load("res://assets/blood_decal.png")
	blood_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	blood_mat.roughness = 0.3
	for i in range(9):
		var bm2 := PlaneMesh.new()
		var bs := _rng.randf_range(0.9, 2.2)
		bm2.size = Vector2(bs, bs)
		var bl := MeshInstance3D.new()
		bl.mesh = bm2
		bl.material_override = blood_mat
		bl.position = Vector3(_rng.randf_range(-5.5, 5.5), 0.03 + 0.003 * (i % 3), _rng.randf_range(-STREET_LENGTH + 4, -5))
		bl.rotation.y = _rng.randf_range(0, TAU)
		add_child(bl)

	# Sidewalks
	var walk := _pbr("paving", 0.35, Color(0.55, 0.52, 0.5))
	_box(Vector3(3, 0.25, STREET_LENGTH + 20), Vector3(-(STREET_WIDTH * 0.5 + 1.5), 0.02, -STREET_LENGTH * 0.5), walk)
	_box(Vector3(3, 0.25, STREET_LENGTH + 20), Vector3(STREET_WIDTH * 0.5 + 1.5, 0.02, -STREET_LENGTH * 0.5), walk)

	# Colonial-style building rows (varied boxes with lit windows)
	var wall_colors := [Color(0.10, 0.09, 0.075), Color(0.12, 0.10, 0.08), Color(0.085, 0.08, 0.09)]
	var z := 4.0
	while z < STREET_LENGTH + 10.0:
		for side: float in [-1.0, 1.0]:
			var w := _rng.randf_range(6.0, 10.0)
			var h := _rng.randf_range(7.0, 14.0)
			var d := _rng.randf_range(5.0, 8.0)
			var bx := side * (STREET_WIDTH * 0.5 + 3.0 + d * 0.5)
			var c: Color = wall_colors[_rng.randi() % wall_colors.size()]
			var bmat: StandardMaterial3D
			var r := _rng.randf()
			if r < 0.7:
				# Colonial facade with shuttered windows; some windows lit from inside
				var fname := "facade_a" if _rng.randf() < 0.55 else "facade_b"
				bmat = StandardMaterial3D.new()
				bmat.albedo_texture = load("res://assets/tex/%s_color.jpg" % fname)
				bmat.albedo_color = Color(0.28, 0.26, 0.25)
				bmat.roughness = 0.85
				bmat.emission_enabled = true
				bmat.emission_texture = load("res://assets/tex/%s_emis.jpg" % fname)
				bmat.emission_energy_multiplier = 1.1
				bmat.uv1_triplanar = true
				bmat.uv1_scale = Vector3(1.0 / 6.0, 1.0 / 6.0, 1.0 / 6.0)
				bmat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
			elif r < 0.85:
				bmat = _pbr("plaster", 0.22, c * 1.6)
			else:
				bmat = _pbr("bricks", 0.28, Color(0.6, 0.45, 0.4))
			_box(Vector3(d, h, w), Vector3(bx, h * 0.5, -z), bmat)
		z += _rng.randf_range(7.0, 11.0)

	# End walls so player cannot leave
	var block := _pbr("concrete", 0.2, Color(0.35, 0.35, 0.38))
	_box(Vector3(40, 12, 1), Vector3(0, 6, 6), block)
	_box(Vector3(1, 12, STREET_LENGTH + 30), Vector3(-(STREET_WIDTH * 0.5 + 12), 6, -STREET_LENGTH * 0.5), block)
	_box(Vector3(1, 12, STREET_LENGTH + 30), Vector3(STREET_WIDTH * 0.5 + 12, 6, -STREET_LENGTH * 0.5), block)

	# Flickering street lamps (real Quaternius streetlight models)
	for i in range(4):
		var lz := -12.0 - i * 22.0
		var side_sign := (1.0 if i % 2 == 0 else -1.0)
		var lx := (STREET_WIDTH * 0.5 + 0.8) * side_sign
		_prop("Streetlight_Single", Vector3(lx, 0, lz), 90.0 if side_sign > 0 else -90.0, 5.4)
		var lamp := OmniLight3D.new()
		lamp.position = Vector3(lx, 5.2, lz)
		lamp.light_color = Color(1.0, 0.75, 0.4)
		lamp.light_energy = 1.4
		lamp.omni_range = 14.0
		lamp.shadow_enabled = true
		lamp.set_meta("flicker", _rng.randf() < 0.6)
		lamp.set_meta("base_energy", lamp.light_energy)
		lamp.add_to_group("lamps")
		add_child(lamp)

	# Street furniture: traffic lights and road signs (Quaternius, CC0)
	_prop("TrafficLight", Vector3(-(STREET_WIDTH * 0.5 + 1.2), 0, -14), 90.0, 4.6)
	_prop("TrafficLight_2", Vector3(STREET_WIDTH * 0.5 + 1.2, 0, -58), -90.0, 4.6)
	var sign_files := ["Sign_Stop", "Sign_NoParking", "Sign_Triangle"]
	for i in range(6):
		var sz := -10.0 - i * 13.0
		var s_side := -1.0 if i % 2 == 0 else 1.0
		_prop(sign_files[i % sign_files.size()], Vector3(s_side * (STREET_WIDTH * 0.5 + 1.6), 0, sz), _rng.randf_range(60, 120) * s_side, 2.6)

	# v1.3: palm trees along the sidewalks (Oran boulevard look)
	for i in range(6):
		var pz := -8.0 - i * 15.0
		var p_side := 1.0 if i % 2 == 0 else -1.0
		_palm(Vector3(p_side * (STREET_WIDTH * 0.5 + 2.3), 0, pz + _rng.randf_range(-2, 2)))

	# Abandoned real 3D cars (Quaternius, CC0)
	var car_files := ["NormalCar1", "NormalCar2", "SUV", "Taxi", "Cop"]
	for i in range(9):
		var cz := _rng.randf_range(-STREET_LENGTH + 8, -8)
		var cx := _rng.randf_range(-STREET_WIDTH * 0.35, STREET_WIDTH * 0.35)
		var car := StaticBody3D.new()
		car.position = Vector3(cx, 0.0, cz)
		car.rotation_degrees.y = _rng.randf_range(0, 360)
		var model: Node3D = load("res://assets/car_%s.glb" % car_files[i % car_files.size()]).instantiate()
		car.add_child(model)
		# Fit to realistic car length (~4.3m) whatever the source scale
		var aabb := _merged_aabb(model)
		var longest: float = maxf(aabb.size.x, maxf(aabb.size.y, aabb.size.z))
		if longest > 0.001:
			var cs := 4.3 / longest
			model.scale = Vector3(cs, cs, cs)
			aabb = _merged_aabb(model)
		model.position.y = -aabb.position.y
		var col := CollisionShape3D.new()
		var shape := BoxShape3D.new()
		shape.size = Vector3(2.0, 1.6, 4.3)
		col.shape = shape
		col.position.y = 0.8
		car.add_child(col)
		add_child(car)
	# Dumpsters (metal)
	var metal := _pbr("metal", 0.4, Color(0.35, 0.4, 0.35))
	for i in range(4):
		var dz := _rng.randf_range(-STREET_LENGTH + 10, -12)
		var side2 := -1.0 if _rng.randf() < 0.5 else 1.0
		_box(Vector3(1.9, 1.2, 1.0), Vector3(side2 * (STREET_WIDTH * 0.5 + 0.9), 0.6, dz), metal)
	var debris_mat := _pbr("concrete", 0.4, Color(0.45, 0.42, 0.4))
	for i in range(22):
		var s := _rng.randf_range(0.3, 0.9)
		_box(Vector3(s, s * 0.6, s), Vector3(_rng.randf_range(-6, 6), s * 0.3, _rng.randf_range(-STREET_LENGTH + 4, -4)), debris_mat)

func _palm(pos: Vector3) -> void:
	# Cheap procedural palm: bent trunk segments + textured frond quads
	var palm := Node3D.new()
	palm.position = pos
	palm.rotation_degrees.y = _rng.randf_range(0, 360)
	var h := _rng.randf_range(4.5, 6.0)
	var lean := _rng.randf_range(-7.0, 7.0)
	var trunk_mat := _mat(Color(0.16, 0.13, 0.10), 0.95)
	var segs := 5
	var top := Vector3.ZERO
	for i in range(segs):
		var seg := MeshInstance3D.new()
		var cm := CylinderMesh.new()
		var f0 := float(i) / segs
		var f1 := float(i + 1) / segs
		cm.bottom_radius = 0.22 * (1.0 - f0 * 0.45)
		cm.top_radius = 0.22 * (1.0 - f1 * 0.45)
		cm.height = h / segs
		cm.radial_segments = 6
		seg.mesh = cm
		seg.material_override = trunk_mat
		var off := sin(f0 * 1.4) * lean * 0.06
		seg.position = Vector3(off, h / segs * (i + 0.5), 0)
		seg.rotation_degrees.z = lean * f1
		palm.add_child(seg)
		if i == segs - 1:
			top = seg.position + Vector3(0, h / segs * 0.5, 0)
	var frond_mat := StandardMaterial3D.new()
	frond_mat.albedo_texture = load("res://assets/palm_frond.png")
	frond_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	frond_mat.alpha_scissor_threshold = 0.4
	frond_mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	frond_mat.roughness = 0.9
	for i in range(9):
		# Pivot at crown; frond quad extends outward along +X and droops via Z rot.
		var pivot := Node3D.new()
		pivot.position = top
		pivot.rotation_degrees = Vector3(0, i * 40.0 + _rng.randf_range(-14, 14), _rng.randf_range(-38, -14))
		palm.add_child(pivot)
		var fq := MeshInstance3D.new()
		var qm := QuadMesh.new()
		qm.size = Vector2(3.4, 1.5)
		qm.center_offset = Vector3(1.55, 0.0, 0.0)
		fq.mesh = qm
		fq.material_override = frond_mat
		fq.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		pivot.add_child(fq)
	# Trunk collision
	var body := StaticBody3D.new()
	var col := CollisionShape3D.new()
	var cs := CylinderShape3D.new()
	cs.radius = 0.25
	cs.height = h
	col.shape = cs
	col.position = Vector3(0, h * 0.5, 0)
	body.add_child(col)
	palm.add_child(body)
	add_child(palm)

func _prop(file: String, pos: Vector3, roty_deg: float, target_h: float) -> void:
	# Place a decorative GLB prop, scaled so its height matches target_h, feet on ground.
	var model: Node3D = load("res://assets/%s.glb" % file).instantiate()
	var holder := Node3D.new()
	holder.position = pos
	holder.rotation_degrees.y = roty_deg
	holder.add_child(model)
	var aabb := _merged_aabb(model)
	if aabb.size.y > 0.001:
		var ps := target_h / aabb.size.y
		model.scale = Vector3(ps, ps, ps)
		aabb = _merged_aabb(model)
	model.position.y = -aabb.position.y
	add_child(holder)

func _build_exit_gate() -> void:
	var gz := -STREET_LENGTH - 2.0
	_box(Vector3(4.5, 0.2, 0.4), Vector3(0, 5.0, gz), _mat(Color(0.05, 0.05, 0.05)))
	_box(Vector3(0.4, 5.0, 0.4), Vector3(-2.2, 2.5, gz), _mat(Color(0.05, 0.05, 0.05)))
	_box(Vector3(0.4, 5.0, 0.4), Vector3(2.2, 2.5, gz), _mat(Color(0.05, 0.05, 0.05)))
	_box(Vector3(3.8, 4.6, 0.15), Vector3(0, 2.3, gz), _mat(Color(0.03, 0.06, 0.04), 0.6, Color(0.05, 0.28, 0.10)), false)
	var glow := OmniLight3D.new()
	glow.position = Vector3(0, 2.5, gz + 1.5)
	glow.light_color = Color(0.2, 1.0, 0.4)
	glow.light_energy = 0.7
	glow.omni_range = 10.0
	add_child(glow)

	var area := Area3D.new()
	area.position = Vector3(0, 1.5, gz + 1.0)
	var col := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = Vector3(5, 3, 2)
	col.shape = shape
	area.add_child(col)
	area.body_entered.connect(_on_exit_reached)
	add_child(area)

func _on_exit_reached(body: Node3D) -> void:
	if body == player and state == GameState.PLAYING:
		if fuel >= FUEL_NEEDED:
			_win()
		else:
			objective_label.text = "البوابة مقفلة! تحتاج %d عبوات وقود أخرى ⛽" % (FUEL_NEEDED - fuel)
			hint_timer = 4.0

func _spawn_pickups() -> void:
	# Fuel cans (mission objective)
	var fuel_spots := [Vector3(-4.2, 0.5, -18), Vector3(4.0, 0.5, -48), Vector3(-3.0, 0.5, -78)]
	for p in fuel_spots:
		_make_pickup(p, "fuel", Color(0.9, 0.35, 0.05), Vector3(0.35, 0.45, 0.25))
	# Medkits
	for p in [Vector3(2.5, 0.4, -28), Vector3(-4.5, 0.4, -58), Vector3(3.5, 0.4, -72)]:
		_make_pickup(p, "health", Color(0.85, 0.1, 0.1), Vector3(0.4, 0.25, 0.3))
	# Ammo boxes
	for p in [Vector3(-2.0, 0.4, -14), Vector3(4.5, 0.4, -38), Vector3(-4.0, 0.4, -44), Vector3(1.5, 0.4, -66)]:
		_make_pickup(p, "ammo", Color(0.25, 0.45, 0.15), Vector3(0.35, 0.22, 0.25))

func _make_pickup(pos: Vector3, kind: String, color: Color, size: Vector3) -> void:
	var area := Area3D.new()
	area.position = pos
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.emission_enabled = true
	mat.emission = color * 0.8
	mat.emission_energy_multiplier = 0.9
	mi.material_override = mat
	area.add_child(mi)
	var glow := OmniLight3D.new()
	glow.light_color = color
	glow.light_energy = 0.6
	glow.omni_range = 2.0
	area.add_child(glow)
	var col := CollisionShape3D.new()
	var shape := SphereShape3D.new()
	shape.radius = 1.0
	col.shape = shape
	area.add_child(col)
	area.set_meta("kind", kind)
	area.add_to_group("pickups")
	area.body_entered.connect(func(body: Node3D):
		if body == player and state == GameState.PLAYING:
			_collect_pickup(area))
	add_child(area)

func _collect_pickup(area: Area3D) -> void:
	var kind: String = area.get_meta("kind")
	var snd := AudioStreamPlayer.new()
	snd.stream = load("res://audio/pickup.wav")
	add_child(snd)
	snd.play()
	snd.finished.connect(snd.queue_free)
	match kind:
		"fuel":
			fuel += 1
			fuel_label.text = "⛽ %d / %d" % [fuel, FUEL_NEEDED]
			if fuel >= FUEL_NEEDED:
				objective_label.text = "البوابة فُتحت! اهرب إلى الميناء 🟢"
			else:
				objective_label.text = "وقود %d/%d — ابحث عن البقية" % [fuel, FUEL_NEEDED]
				hint_timer = 0.0
			# The horde hears you...
			_spawn_extra_zombies(2)
		"health":
			player.add_health(40.0)
			_on_player_damaged()
			damage_flash.color = Color(0, 0.4, 0, 0.3)
		"ammo":
			player.add_ammo(12)
	area.queue_free()

func _spawn_extra_zombies(count: int) -> void:
	for i in range(count):
		var zb: CharacterBody3D = preload("res://scripts/zombie.gd").new()
		var behind := player.global_position + Vector3(_rng.randf_range(-4, 4), 0.4, _rng.randf_range(14, 22))
		behind.x = clampf(behind.x, -5.0, 5.0)
		behind.z = clampf(behind.z, -STREET_LENGTH + 2.0, -2.0)
		zb.position = behind
		zb.target_path = player.get_path()
		zb.chasing = true
		zb.killed.connect(_on_zombie_killed)
		add_child(zb)
		zombies.append(zb)

# ------------------------------------------------------------
# Player & zombies
# ------------------------------------------------------------
func _spawn_player() -> void:
	player = preload("res://scripts/player.gd").new()
	player.position = Vector3(0, 1.2, -2)
	player.died.connect(_on_player_died)
	player.damaged.connect(_on_player_damaged)
	add_child(player)

func _spawn_zombies() -> void:
	var spots := [
		Vector3(-3, 1.2, -22), Vector3(3.5, 1.2, -35), Vector3(-2, 1.2, -50),
		Vector3(2, 1.2, -62), Vector3(-3.5, 1.2, -75), Vector3(0, 1.2, -82),
		Vector3(4, 1.2, -45), Vector3(-4, 1.2, -68),
		Vector3(1.5, 1.2, -28), Vector3(-1.0, 1.2, -58), Vector3(3.0, 1.2, -88),
		# v1.3: distant horde silhouettes in the fog
		Vector3(-2.5, 1.2, -70), Vector3(1.0, 1.2, -78), Vector3(-4.0, 1.2, -85),
		Vector3(4.0, 1.2, -80), Vector3(0.5, 1.2, -86),
	]
	var idx := 0
	for s in spots:
		var zb: CharacterBody3D = preload("res://scripts/zombie.gd").new()
		zb.position = s
		zb.target_path = player.get_path()
		zb.crawler = (idx % 4 == 3)  # every 4th zombie crawls
		zb.killed.connect(_on_zombie_killed)
		add_child(zb)
		zombies.append(zb)
		idx += 1

# ------------------------------------------------------------
# UI
# ------------------------------------------------------------
func _build_ui() -> void:
	ui = CanvasLayer.new()
	add_child(ui)

	# Health bar
	var hb_bg := ColorRect.new()
	hb_bg.color = Color(0.1, 0.1, 0.1, 0.7)
	hb_bg.position = Vector2(20, 20)
	hb_bg.size = Vector2(260, 22)
	ui.add_child(hb_bg)
	health_fill = ColorRect.new()
	health_fill.color = Color(0.7, 0.12, 0.1)
	health_fill.position = Vector2(23, 23)
	health_fill.size = Vector2(254, 16)
	ui.add_child(health_fill)

	objective_label = Label.new()
	objective_label.text = "الهدف: اجمع عبوات الوقود ⛽ لفتح بوابة الميناء"
	objective_label.position = Vector2(20, 50)
	objective_label.add_theme_font_size_override("font_size", 16)
	objective_label.add_theme_color_override("font_color", Color(0.8, 0.8, 0.75))
	ui.add_child(objective_label)

	# Ammo counter (bottom-right)
	ammo_label = Label.new()
	ammo_label.text = "12 / 36"
	ammo_label.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	ammo_label.position = Vector2(-150, -180)
	ammo_label.add_theme_font_size_override("font_size", 26)
	ammo_label.add_theme_color_override("font_color", Color(0.9, 0.85, 0.6))
	ui.add_child(ammo_label)

	# Kills counter (top-right)
	kills_label = Label.new()
	kills_label.text = "☠ 0"
	kills_label.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	kills_label.position = Vector2(-110, 20)
	kills_label.add_theme_font_size_override("font_size", 24)
	kills_label.add_theme_color_override("font_color", Color(0.75, 0.7, 0.65))
	ui.add_child(kills_label)

	# Fuel counter
	fuel_label = Label.new()
	fuel_label.text = "⛽ 0 / %d" % FUEL_NEEDED
	fuel_label.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	fuel_label.position = Vector2(-110, 52)
	fuel_label.add_theme_font_size_override("font_size", 24)
	fuel_label.add_theme_color_override("font_color", Color(0.95, 0.6, 0.2))
	ui.add_child(fuel_label)

	# Fire button (bottom-right)
	var fire := Button.new()
	fire.text = "نار"
	fire.add_theme_font_size_override("font_size", 34)
	fire.add_theme_color_override("font_color", Color(1.0, 0.45, 0.35))
	fire.add_theme_color_override("font_pressed_color", Color(1.0, 0.2, 0.1))
	fire.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	fire.position = Vector2(-160, -150)
	fire.size = Vector2(120, 120)
	fire.focus_mode = Control.FOCUS_NONE
	fire.add_to_group("fire_btn")
	fire.button_down.connect(func(): if state == GameState.PLAYING: player.try_shoot())
	ui.add_child(fire)

	damage_flash = ColorRect.new()
	damage_flash.color = Color(0.6, 0, 0, 0)
	damage_flash.set_anchors_preset(Control.PRESET_FULL_RECT)
	damage_flash.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui.add_child(damage_flash)

	# Crosshair dot
	var ch := ColorRect.new()
	ch.color = Color(1, 1, 1, 0.5)
	ch.set_anchors_preset(Control.PRESET_CENTER)
	ch.size = Vector2(5, 5)
	ch.position = Vector2(-2.5, -2.5)
	ch.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui.add_child(ch)

	# v1.3: animated film grain (adds texture to flat dark areas, RE-movie feel)
	var grain := ColorRect.new()
	grain.set_anchors_preset(Control.PRESET_FULL_RECT)
	grain.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var gsh := Shader.new()
	gsh.code = """
shader_type canvas_item;
render_mode blend_add;
uniform float strength = 0.045;
float rnd(vec2 co) { return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453); }
void fragment() {
	float g = rnd(SCREEN_UV * (fract(TIME) * 37.0 + 1.7)) - 0.5;
	COLOR = vec4(vec3(g * strength), 1.0);
}
"""
	var gmat := ShaderMaterial.new()
	gmat.shader = gsh
	grain.material = gmat
	ui.add_child(grain)

	# Cinematic vignette
	var vg := TextureRect.new()
	var grad := Gradient.new()
	grad.set_color(0, Color(0, 0, 0, 0))
	grad.set_color(1, Color(0, 0, 0, 0.55))
	var gt := GradientTexture2D.new()
	gt.gradient = grad
	gt.fill = GradientTexture2D.FILL_RADIAL
	gt.fill_from = Vector2(0.5, 0.5)
	gt.fill_to = Vector2(0.5, 1.05)
	gt.width = 512
	gt.height = 288
	vg.texture = gt
	vg.set_anchors_preset(Control.PRESET_FULL_RECT)
	vg.stretch_mode = TextureRect.STRETCH_SCALE
	vg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui.add_child(vg)

	# Menu / end overlay
	overlay = ColorRect.new()
	overlay.color = Color(0, 0, 0, 0.85)
	overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	ui.add_child(overlay)

	title_label = Label.new()
	title_label.text = "وهران: السقوط\nORAN: THE FALL"
	title_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title_label.add_theme_font_size_override("font_size", 52)
	title_label.add_theme_color_override("font_color", Color(0.75, 0.08, 0.08))
	title_label.set_anchors_preset(Control.PRESET_CENTER)
	title_label.position += Vector2(0, -70)
	overlay.add_child(title_label)

	subtitle_label = Label.new()
	subtitle_label.text = "المهمة: اجمع الوقود ⛽ واهرب من بوابة الميناء\nيسار: حركة · يمين: كاميرا · زر «نار»: إطلاق النار\nالمس الشاشة للبدء — Tap to start"
	subtitle_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	subtitle_label.add_theme_font_size_override("font_size", 24)
	subtitle_label.add_theme_color_override("font_color", Color(0.8, 0.8, 0.75))
	subtitle_label.set_anchors_preset(Control.PRESET_CENTER)
	subtitle_label.position += Vector2(0, 60)
	overlay.add_child(subtitle_label)

func _show_menu() -> void:
	state = GameState.MENU
	overlay.visible = true

func _start_game() -> void:
	state = GameState.PLAYING
	overlay.visible = false

func _on_zombie_killed() -> void:
	kills += 1
	kills_label.text = "☠ %d" % kills

func _on_player_damaged() -> void:
	health_fill.size.x = 254.0 * clampf(player.health / 100.0, 0.0, 1.0)
	damage_flash.color.a = 0.45

func _on_player_died() -> void:
	state = GameState.DEAD
	if player.anim:
		player.anim.play("Death", 0.2)
		await get_tree().create_timer(1.6).timeout
	title_label.text = "لقد مت...\nYOU DIED"
	subtitle_label.text = "المس الشاشة لإعادة المحاولة — Tap to retry"
	overlay.visible = true

func _win() -> void:
	state = GameState.WON
	title_label.text = "نجوت!\nYOU SURVIVED"
	title_label.add_theme_color_override("font_color", Color(0.15, 0.8, 0.3))
	subtitle_label.text = "نهاية الفصل التجريبي — الفصل الأول قادم\nEnd of vertical slice"
	overlay.visible = true

func _input(event: InputEvent) -> void:
	var pressed: bool = (event is InputEventScreenTouch and event.pressed) \
		or (event is InputEventMouseButton and event.pressed) \
		or (event is InputEventKey and event.pressed and event.keycode == KEY_ENTER)
	if not pressed:
		return
	match state:
		GameState.MENU:
			_start_game()
		GameState.DEAD, GameState.WON:
			get_tree().reload_current_scene()

# ------------------------------------------------------------
# Weather: rain, wet air, lightning (v1.2 realism pass)
# ------------------------------------------------------------
func _build_weather() -> void:
	if player == null:
		return
	# Rain streaks following the player
	var rain := GPUParticles3D.new()
	rain.amount = 650
	rain.lifetime = 0.75
	rain.preprocess = 0.75
	rain.visibility_aabb = AABB(Vector3(-22, -12, -22), Vector3(44, 24, 44))
	var pm := ParticleProcessMaterial.new()
	pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	pm.emission_box_extents = Vector3(17, 0.5, 17)
	pm.gravity = Vector3(0, -46, 0)
	pm.initial_velocity_min = 6.0
	pm.initial_velocity_max = 9.0
	pm.direction = Vector3(0.12, -1, 0)
	rain.process_material = pm
	var quad := QuadMesh.new()
	quad.size = Vector2(0.018, 0.42)
	var rm := StandardMaterial3D.new()
	rm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	rm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	rm.albedo_color = Color(0.65, 0.75, 0.95, 0.28)
	rm.billboard_mode = BaseMaterial3D.BILLBOARD_FIXED_Y
	quad.material = rm
	rain.draw_pass_1 = quad
	rain.position = Vector3(0, 8.5, 0)
	player.add_child(rain)
	# Rain sound loop
	var rs := AudioStreamPlayer.new()
	rs.stream = load("res://audio/rain.wav")
	rs.volume_db = -14.0
	rs.autoplay = true
	add_child(rs)
	rs.finished.connect(rs.play)

func _lightning(delta: float) -> void:
	_lightning_t -= delta
	if _lightning_t > 0.0 or moon == null:
		return
	_lightning_t = _rng.randf_range(9.0, 22.0)
	var tw := create_tween()
	tw.tween_property(moon, "light_energy", 2.4, 0.05)
	tw.tween_property(moon, "light_energy", 0.4, 0.09)
	tw.tween_property(moon, "light_energy", 1.7, 0.06)
	tw.tween_property(moon, "light_energy", 0.34, 0.25)
	var t := get_tree().create_timer(_rng.randf_range(0.5, 1.4))
	t.timeout.connect(func() -> void:
		var th := AudioStreamPlayer.new()
		th.stream = load("res://audio/thunder.wav")
		th.volume_db = -7.0
		add_child(th)
		th.play()
		th.finished.connect(th.queue_free))

# ------------------------------------------------------------
# Ambience + lamp flicker
# ------------------------------------------------------------
func _start_ambience() -> void:
	var amb := AudioStreamPlayer.new()
	amb.stream = load("res://audio/ambience.wav")
	amb.volume_db = -6.0
	amb.autoplay = true
	add_child(amb)
	amb.finished.connect(amb.play)

func _process(_delta: float) -> void:
	_lightning(_delta)
	if damage_flash and damage_flash.color.a > 0.0:
		damage_flash.color.a = maxf(0.0, damage_flash.color.a - _delta * 1.2)
	if player and ammo_label:
		ammo_label.text = ("إعادة تعبئة..." if player.reloading else "%d / %d" % [player.ammo, player.reserve])
	if hint_timer > 0.0:
		hint_timer -= _delta
		if hint_timer <= 0.0:
			objective_label.text = "الهدف: اجمع عبوات الوقود ⛽ لفتح بوابة الميناء"
	var tp := Time.get_ticks_msec() / 1000.0
	for pk in get_tree().get_nodes_in_group("pickups"):
		pk.rotation.y = tp * 1.5
		pk.position.y = pk.position.y + sin(tp * 2.0) * 0.001
	var t := Time.get_ticks_msec() / 1000.0
	for lamp in get_tree().get_nodes_in_group("lamps"):
		if lamp.get_meta("flicker"):
			var base: float = lamp.get_meta("base_energy")
			var n := sin(t * 13.0 + lamp.position.z) * sin(t * 7.3) 
			lamp.light_energy = base * (0.55 + 0.45 * clampf(n * 2.0 + 0.6, 0.0, 1.0))
