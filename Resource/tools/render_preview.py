# -*- coding: utf-8 -*-
"""프리뷰 이미지를 굽는다 — 숫자가 아니라 눈으로 확인하기 위해.

    blender --background --factory-startup --python tools/render_preview.py

결과물: preview/*.png

폴리곤 수와 본 이름이 다 맞아도 **모양이 틀렸는지는 알 수 없다.** 다리가 몸을
뚫고 나왔는지, 사슴이 사슴처럼 보이는지는 봐야 안다. 그래서 따로 둔다.

Workbench 로 렌더한다. EEVEE 는 헤드리스에서 GPU 를 타느라 불안정하고,
어차피 여기서 보려는 건 **실루엣과 파츠 배치**지 조명이 아니다.
"""

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

if HERE not in sys.path:
	sys.path.insert(0, HERE)

import bpy
from mathutils import Vector


SIZE = 640
OUT_DIR = os.path.join(ROOT, 'preview')


def setup_render():
	scene = bpy.context.scene

	scene.render.engine = 'BLENDER_WORKBENCH'
	scene.render.resolution_x = SIZE
	scene.render.resolution_y = SIZE
	scene.render.resolution_percentage = 100
	scene.render.film_transparent = False
	scene.render.image_settings.file_format = 'PNG'

	sh = scene.display.shading
	sh.light = 'STUDIO'
	sh.color_type = 'MATERIAL'
	sh.show_cavity = True
	sh.cavity_type = 'BOTH'
	sh.show_object_outline = True

	scene.display.render_aa = '8'

	# 생성 스크립트가 빈 씬에서 시작하므로 월드가 없다. 배경색을 주려면 만들어야 한다.
	# (삼항으로 감싸도 소용없다 — 대입 대상 scene.world.color 가 먼저 평가된다)
	if scene.world is None:
		scene.world = bpy.data.worlds.new("preview")

	scene.world.color = (0.05, 0.055, 0.07)


def visible_meshes():
	return [o for o in bpy.context.scene.objects
	        if o.type == 'MESH' and not o.hide_render]


def frame(objects, azimuth=38.0, elevation=22.0, pad=1.22):
	"""보이는 것들을 전부 담도록 카메라를 놓는다."""
	pts = []

	for o in objects:
		for c in o.bound_box:
			pts.append(o.matrix_world @ Vector(c))

	if not pts:
		return None

	lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
	hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))

	center = (lo + hi) * 0.5
	radius = max((hi - lo).length * 0.5, 1e-3)

	cam_data = bpy.data.cameras.new("cam")
	cam_data.lens_unit = 'FOV'
	cam_data.angle = math.radians(38.0)

	cam = bpy.data.objects.new("cam", cam_data)
	bpy.context.collection.objects.link(cam)
	bpy.context.scene.camera = cam

	dist = radius / math.tan(cam_data.angle * 0.5) * pad

	a = math.radians(azimuth)
	e = math.radians(elevation)
	offset = Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))

	cam.location = center + offset * dist
	cam.rotation_euler = (center - cam.location).to_track_quat('-Z', 'Y').to_euler()

	return cam


def render(name):
	if not os.path.isdir(OUT_DIR):
		os.makedirs(OUT_DIR)

	path = os.path.join(OUT_DIR, name + '.png')
	bpy.context.scene.render.filepath = path

	bpy.ops.render.render(write_still=True)

	print("  → %s" % path)

	return path


def load(blend):
	bpy.ops.wm.open_mainfile(filepath=os.path.join(ROOT, 'blend', blend))
	setup_render()


def keep_only(names):
	"""이름이 목록에 없는 메쉬는 렌더에서 뺀다."""
	keep = set(names)

	for o in bpy.context.scene.objects:
		if o.type == 'MESH':
			o.hide_render = o.name not in keep

	return [o for o in bpy.context.scene.objects
	        if o.type == 'MESH' and not o.hide_render]


def grid(objects, cols=5, gap=None):
	"""겹쳐 있는 것들을 격자로 흩어 놓는다. 프리뷰에서만 쓴다."""
	if gap is None:
		span = 0.0

		for o in objects:
			bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
			span = max(span,
			           max(p.x for p in bb) - min(p.x for p in bb),
			           max(p.y for p in bb) - min(p.y for p in bb))

		gap = span * 1.25 + 0.3

	for i, o in enumerate(sorted(objects, key=lambda x: x.name)):
		o.location = ((i % cols) * gap, -(i // cols) * gap, 0.0)

	return objects


def read_json(name):
	p = os.path.join(ROOT, 'data', name)

	if not os.path.isfile(p):
		return None

	with open(p, encoding='utf-8') as f:
		return json.load(f)


# ────────────────────────────────────────────────────────────

def preview_character():
	data = read_json('character_parts.json')

	if data is None:
		return

	presets = {p['id']: p for p in data['presets']}

	for pid, azim in (('player', 28.0), ('hodd', 28.0), ('edra', 28.0)):
		if pid not in presets:
			continue

		load('humanoid.blend')

		objs = keep_only(presets[pid]['parts'])
		frame(objs, azimuth=azim, elevation=14.0, pad=1.12)

		render('char_' + pid)

	# 슬롯별 파츠 늘어놓기 — 교체 가능한 것들이 다 제대로 생겼는지
	load('humanoid.blend')

	heads = [p['name'] for p in data['parts']
	         if p['slot'] in ('head', 'face', 'hair', 'headgear')]
	base = [p['name'] for p in data['parts']
	        if p['slot'] in ('torso', 'arms', 'hands', 'legs', 'feet') and p['default']]

	objs = keep_only(heads + base)
	frame(objs, azimuth=20.0, elevation=10.0, pad=1.15)

	render('char_parts_head')


def preview_creatures():
	data = read_json('creature_catalog.json')

	if data is None:
		return

	for s in data['species']:
		if s['id'] not in ('wolf_winterfang', 'stag_great', 'panther_ruin', 'wolf_tainted'):
			continue

		load('creature_%s.blend' % s['family'])

		objs = keep_only(s['meshes'])

		for o in objs:
			o.scale = (s['scale'],) * 3

		bpy.context.view_layer.update()

		frame(objs, azimuth=52.0, elevation=16.0, pad=1.15)

		render('creature_' + s['id'])

	# 뒷다리 비교 — 과별 차이가 보여야 한다
	load('creature_cervid.blend')
	objs = keep_only(['cre_cervid_body', 'cre_cervid_head', 'cre_cervid_ears',
	                  'cre_cervid_legs', 'cre_cervid_tail', 'cre_cervid_antler'])
	frame(objs, azimuth=90.0, elevation=6.0, pad=1.1)
	render('creature_cervid_side')

	load('creature_canine.blend')
	objs = keep_only(['cre_canine_body', 'cre_canine_head', 'cre_canine_ears',
	                  'cre_canine_legs', 'cre_canine_tail', 'cre_canine_mane'])
	frame(objs, azimuth=90.0, elevation=6.0, pad=1.1)
	render('creature_canine_side')


def preview_props():
	for blend, tag, cols in (('props_structure.blend', 'props_structure', 5),
	                         ('props_furniture.blend', 'props_furniture', 5),
	                         ('props_tools.blend', 'props_tools', 6)):
		load(blend)

		objs = grid(visible_meshes(), cols=cols)
		bpy.context.view_layer.update()

		frame(objs, azimuth=30.0, elevation=30.0, pad=1.08)

		render(tag)


def preview_nature():
	load('nature_flora.blend')

	objs = grid(visible_meshes(), cols=6)
	bpy.context.view_layer.update()

	frame(objs, azimuth=28.0, elevation=20.0, pad=1.08)

	render('nature_flora')

	load('nature_terrain.blend')

	objs = grid(visible_meshes(), cols=5)
	bpy.context.view_layer.update()

	frame(objs, azimuth=25.0, elevation=42.0, pad=1.08)

	render('nature_terrain')


def preview_pose():
	"""걷기 중간 포즈 — 스키닝이 제대로 먹었는지 본다.

	T 포즈만 봐서는 가중치가 틀렸는지 알 수 없다. 관절을 꺾어 봐야
	소매가 어깨에서 떨어지거나 허벅지가 골반을 뚫는 것이 보인다.
	"""
	data = read_json('character_parts.json')

	if data is None:
		return

	presets = {p['id']: p for p in data['presets']}

	for clip, at in (('walk', 8), ('run', 5), ('attack_01', 13)):
		load('humanoid.blend')

		arm = bpy.data.objects.get('rig_humanoid')
		act = bpy.data.actions.get(clip)

		if arm is None or act is None:
			continue

		if arm.animation_data is None:
			arm.animation_data_create()

		arm.animation_data.action = act

		slots = getattr(act, 'slots', None)

		if slots:
			try:
				arm.animation_data.action_slot = slots[0]
			except (AttributeError, TypeError):
				pass

		bpy.context.scene.frame_set(at)

		objs = keep_only(presets['player']['parts'])
		bpy.context.view_layer.update()

		frame(objs, azimuth=34.0, elevation=12.0, pad=1.12)

		render('pose_' + clip)

	# 4족 걷기 포즈 — 뒷다리가 접히는 모양을 본다
	cat = read_json('creature_catalog.json')

	if cat is None:
		return

	wolf = [s for s in cat['species'] if s['id'] == 'wolf_winterfang'][0]

	for clip, at in (('walk', 9), ('run', 4)):
		load('creature_canine.blend')

		arm = bpy.data.objects.get('rig_canine')
		act = bpy.data.actions.get(clip)

		if arm is None or act is None:
			continue

		if arm.animation_data is None:
			arm.animation_data_create()

		arm.animation_data.action = act

		slots = getattr(act, 'slots', None)

		if slots:
			try:
				arm.animation_data.action_slot = slots[0]
			except (AttributeError, TypeError):
				pass

		bpy.context.scene.frame_set(at)

		objs = keep_only(wolf['meshes'])
		bpy.context.view_layer.update()

		frame(objs, azimuth=76.0, elevation=10.0, pad=1.12)

		render('pose_wolf_' + clip)


def main():
	print("프리뷰 렌더 → %s" % OUT_DIR)

	preview_character()
	preview_creatures()
	preview_props()
	preview_nature()
	preview_pose()

	print("\n완료")

	return 0


if __name__ == '__main__':
	sys.exit(main())
