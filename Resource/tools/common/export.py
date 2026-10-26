# -*- coding: utf-8 -*-
"""glTF 내보내기.

내보내기 연산자의 인자 이름이 Blender 버전마다 조금씩 바뀐다 (4.0 에서 색 관련
인자가 `export_colors` → `export_vertex_color` 로, 애니메이션 쪽도 여러 번 손봤다).
없는 인자를 넘기면 TypeError 로 통째로 실패하므로, **연산자가 실제로 가진 인자만
골라서** 넘긴다. 그러면 구버전에서는 그 옵션이 기본값으로 동작할 뿐 죽지는 않는다.
"""

import os

import bpy


GLTF_FORMAT = 'GLTF_SEPARATE'   # .gltf + .bin. 단일 파일이 필요하면 'GLB'


def _supported(kwargs):
	"""export_scene.gltf 가 실제로 받는 인자만 남긴다."""
	try:
		props = bpy.ops.export_scene.gltf.get_rna_type().properties.keys()
	except Exception:
		return kwargs

	return {k: v for k, v in kwargs.items() if k in props}


def ensure_dir(path):
	d = os.path.dirname(os.path.abspath(path))

	if d and not os.path.isdir(d):
		os.makedirs(d)

	return path


def select_only(objects):
	bpy.ops.object.select_all(action='DESELECT')

	objects = [o for o in objects if o is not None]

	for o in objects:
		o.select_set(True)

	if objects:
		bpy.context.view_layer.objects.active = objects[0]

	return objects


def write(filepath, objects=None):
	"""objects 를 glTF 로 내보낸다. None 이면 씬 전체.

	반환값은 실제로 쓰인 경로.
	"""
	ensure_dir(filepath)

	use_selection = objects is not None

	if use_selection:
		select_only(objects)

	opts = {
		'filepath': filepath,
		'export_format': GLTF_FORMAT,
		'use_selection': use_selection,

		# 지오메트리 — UV 와 텍스처는 애초에 안 만든다 (FORMAT.md 3절)
		'export_apply': False,          # 아마튜어 말고는 모디파이어가 없다
		'export_normals': True,
		'export_texcoords': False,
		'export_tangents': False,
		'export_colors': True,          # ~3.x (4.0 에서 사라졌다. 없으면 걸러진다)

		# 정점 색은 **이름으로 콕 집어** 내보낸다.
		#
		# 기본값(export_all_vertex_colors=True)으로 두면 Blender 5.2 가 같은 속성을
		# COLOR_0(ubyte) 과 COLOR_1(ushort) 두 벌로 내보낸다 — 용량만 두 배다.
		# 반대로 'MATERIAL' + all=False 로 두면 머티리얼 노드가 정점 색을 참조하지 않는 한
		# **아예 빠진다** (우리는 노드를 안 쓰고 팩터만 쓰므로 늘 빠진다).
		# 'NAME' 모드만 정확히 COLOR_0 하나를 내보낸다.
		'export_vertex_color': 'NAME',
		'export_vertex_color_name': 'Color',
		'export_all_vertex_colors': False,
		'export_active_vertex_color_when_no_material': False,

		'export_attributes': False,
		'export_materials': 'EXPORT',
		'export_image_format': 'NONE',

		# 스키닝
		'export_skins': True,
		'export_def_bones': False,      # 모든 본을 내보낸다 — 소켓 본이 빠지면 안 된다
		'export_rest_position_armature': True,

		# 애니메이션
		'export_animations': True,
		'export_animation_mode': 'ACTIONS',
		'export_frame_range': False,    # 씬 범위로 자르지 않는다. 액션마다 길이가 다르다
		'export_force_sampling': True,  # 매 프레임 샘플 — 보간은 전부 LINEAR 가 된다
		'export_anim_single_armature': True,
		'export_optimize_animation_size': False,
		'export_optimize_animation_keep_anim_armature': True,
		'export_bake_animation': False,
		'export_nla_strips': False,
		'export_current_frame': False,
		'export_anim_slide_to_zero': False,

		# 기타
		'export_extras': True,          # 커스텀 프로퍼티(family 등)를 실어 보낸다
		'export_cameras': False,
		'export_lights': False,
		'export_yup': True,             # glTF 표준 Y-up
		'will_save_settings': False,
	}

	bpy.ops.export_scene.gltf(**_supported(opts))

	return filepath


def save_blend(filepath):
	"""소스 .blend 도 남긴다. 나중에 손으로 손볼 때 필요하다."""
	ensure_dir(filepath)
	bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(filepath))

	return filepath


# ────────────────────────────────────────────────────────────
#  진단
# ────────────────────────────────────────────────────────────

def report(objects, budget=None, label=""):
	"""삼각형 수를 찍는다. 폴리곤 예산(FORMAT.md 8절) 확인용."""
	total = 0

	for o in objects:
		if o is None or o.type != 'MESH':
			continue

		n = sum(max(0, len(p.vertices) - 2) for p in o.data.polygons)
		total += n

		print("    %-34s %5d tri" % (o.name, n))

	mark = ""
	if budget is not None:
		mark = "  <= %d" % budget if total <= budget else "  ** 예산 초과 %d **" % budget

	print("    %-34s %5d tri%s" % ("(" + (label or "합계") + ")", total, mark))

	return total
