# -*- coding: utf-8 -*-
"""자연환경 — 나무 · 풀 · 바위 · 땅 재질.

    blender --background --python tools/gen_nature.py

결과물
    export/nature/flora.gltf · terrain.gltf (+ .bin)
    blend/nature_flora.blend · nature_terrain.blend
    data/nature_catalog.json

## 땅은 "재질" 이지 "모델" 이 아니다

나무 한 그루는 메쉬지만, 땅은 끝없이 반복되는 면이다. 그래서 1 m 타일 하나를
만들어 두고 **정점 색으로 얼룩을 주어** 반복이 눈에 덜 띄게 한다.
텍스처를 쓰지 않기로 했으므로 (FORMAT.md 3절) 변주는 정점 색이 전담한다.

타일 종류는 지금 코드의 `World.h` TILE_* 와 **일대일로 맞춰 두었다.**
이름이 같으면 나중에 로더에서 enum 으로 바로 색인할 수 있다.

## 풀은 교차 평면이 아니라 쐐기다

흔한 방법은 알파 텍스처를 입힌 십자 평면이지만, 텍스처가 없고 알파 정렬도
피하기로 했으므로 (CLAUDE.md 4-1) **얇은 쐐기 몇 장**으로 만든다.
폴리곤은 조금 더 들지만 알파 정렬 문제가 아예 사라진다.
"""

import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

if HERE not in sys.path:
	sys.path.insert(0, HERE)

import bpy

from common import build, export, palette


TILE = 1.00   # 땅 타일 한 변 (m)

RNG = random.Random(20261021)   # 고정 시드 — 다시 돌려도 같은 결과가 나와야 한다


def jitter(rgb, amount):
	"""기준색에서 **어두운 쪽으로만** 흔든다. amount 는 비율 (0.15 = 최대 15% 어둡게).

	밝은 쪽으로 흔들면 안 되는 이유는 palette.jitter 주석 참고 —
	정점 색은 머티리얼 색에 곱해지는 배율이라 1.0 을 넘지 못한다.
	"""
	return palette.jitter(rgb, amount, RNG)


# ────────────────────────────────────────────────────────────
#  나무
# ────────────────────────────────────────────────────────────

def _trunk(mb, h, r0, r1, mat='fix.bark', lean=0.0, segments=3):
	"""밑동에서 위로 가며 가늘어지는 줄기. 조금 기울여야 자연물로 보인다."""
	pts = []

	for i in range(segments + 1):
		t = i / segments
		x = math.sin(t * 1.6) * lean

		pts.append((x, 0.0, h * t))

	for i in range(segments):
		a = pts[i]
		b = pts[i + 1]
		ra = r0 + (r1 - r0) * (i / segments)
		rb = r0 + (r1 - r0) * ((i + 1) / segments)

		mb.limb(a, b, ra, rb, mat, sides=5)

	return pts[-1]


def tree_oak(mb):
	"""활엽수 — 마을과 숲의 기본. 덩어리진 잎을 셋 얹는다."""
	top = _trunk(mb, 3.20, 0.26, 0.15, lean=0.14)

	# 굵은 가지 셋
	tips = []

	for i in range(3):
		t = i / 3.0 * math.tau + 0.4
		tip = (top[0] + math.cos(t) * 0.86, math.sin(t) * 0.86, top[2] + 0.72)

		mb.limb(top, tip, 0.11, 0.06, 'fix.bark', sides=4)
		tips.append(tip)

	# 잎 덩어리 — 위쪽을 밝게, 아래를 어둡게. 정점 색으로만 처리한다
	for tip in tips:
		mb.ico(tip, 0.78, 'fix.leaf_summer', scale=(1.0, 1.0, 0.78),
		       color=jitter(palette.LEAF_SUMMER, 0.14), smooth=False)

	mb.ico((top[0], 0.0, top[2] + 1.10), 0.92, 'fix.leaf_summer',
	       scale=(1.0, 1.0, 0.80), color=palette.shade(palette.LEAF_SUMMER, 0.88))

	# 드러난 뿌리
	for i in range(4):
		t = i / 4.0 * math.tau + 0.7

		mb.limb((0.0, 0.0, 0.10),
		        (math.cos(t) * 0.42, math.sin(t) * 0.42, 0.0),
		        0.09, 0.04, 'fix.bark', sides=4)


def tree_pine(mb):
	"""침엽수 — 어둡고 푸르다. 원뿔 세 겹."""
	_trunk(mb, 4.20, 0.20, 0.09, segments=2)

	for i, (z, r, h) in enumerate(((1.30, 1.10, 1.40),
	                                (2.30, 0.86, 1.25),
	                                (3.20, 0.58, 1.10))):
		shade = 0.78 + i * 0.11

		mb.ico((0.0, 0.0, z + h * 0.34), r, 'fix.leaf_pine',
		       scale=(1.0, 1.0, h / r * 0.62),
		       color=palette.shade(palette.LEAF_PINE, shade), smooth=False)

	mb.wedge((0.0, 0.0, 4.26), (0.30, 0.30, 0.52), 'fix.leaf_pine', axis='y')


def tree_birch(mb):
	"""자작나무류 — 흰 수피. 어두운 숲에서 유일하게 밝은 수직선."""
	top = _trunk(mb, 3.80, 0.15, 0.09, mat='fix.bark_pale', lean=0.08, segments=4)

	# 수피의 검은 가로 무늬
	for i in range(5):
		z = 0.45 + i * 0.60

		mb.block((0.0, 0.0, z), (0.34, 0.34, 0.035), 'fix.bark',
		         color=palette.shade(palette.BARK, 0.8))

	for i in range(4):
		t = i / 4.0 * math.tau
		tip = (top[0] + math.cos(t) * 0.60, math.sin(t) * 0.60, top[2] + 0.46)

		mb.limb(top, tip, 0.05, 0.03, 'fix.bark_pale', sides=4)
		mb.ico(tip, 0.54, 'fix.leaf_summer', scale=(1.0, 1.0, 0.70),
		       color=jitter(palette.LEAF_SUMMER, 0.16))


def tree_dead(mb):
	"""고사목 — 폐허와 늪에. 잎이 없으니 실루엣이 전부다."""
	top = _trunk(mb, 3.40, 0.24, 0.10, mat='fix.wood_charred', lean=0.32, segments=4)

	for i in range(5):
		t = i / 5.0 * math.tau + 1.1
		z = 1.40 + (i % 3) * 0.62
		base = (math.sin(z * 0.5) * 0.18, 0.0, z)
		tip = (base[0] + math.cos(t) * (0.70 + (i % 2) * 0.34),
		       math.sin(t) * (0.70 + (i % 2) * 0.34),
		       z + 0.48)

		mb.limb(base, tip, 0.07, 0.02, 'fix.wood_charred', sides=4)

	mb.limb(top, (top[0] + 0.30, 0.22, top[2] + 0.54), 0.06, 0.015,
	        'fix.wood_charred', sides=4)


def tree_stump(mb):
	mb.limb((0.0, 0.0, 0.0), (0.0, 0.0, 0.46), 0.30, 0.27, 'fix.bark', sides=6)
	mb.plane_fan((0.0, 0.0, 0.465), 0.27, 'fix.wood_pine', sides=6)

	for i in range(3):
		t = i / 3.0 * math.tau + 0.5

		mb.limb((0.0, 0.0, 0.08),
		        (math.cos(t) * 0.40, math.sin(t) * 0.40, 0.0),
		        0.10, 0.04, 'fix.bark', sides=4)

	mb.plane_fan((0.10, 0.08, 0.47), 0.11, 'fix.moss', sides=5)


def tree_sapling(mb):
	_trunk(mb, 1.20, 0.055, 0.03, lean=0.06, segments=2)

	for i in range(3):
		t = i / 3.0 * math.tau

		mb.ico((math.cos(t) * 0.18, math.sin(t) * 0.18, 1.08 + (i % 2) * 0.14),
		       0.26, 'fix.leaf_summer', scale=(1.0, 1.0, 0.72),
		       color=jitter(palette.LEAF_SUMMER, 0.16))


def log_fallen(mb):
	mb.limb((-1.10, 0.0, 0.26), (1.10, 0.06, 0.24), 0.25, 0.21, 'fix.bark', sides=6)
	mb.plane_fan((1.12, 0.06, 0.24), 0.20, 'fix.wood_weathered', sides=6)

	for x, y in ((-0.40, 0.18), (0.30, -0.16), (0.72, 0.12)):
		mb.plane_fan((x, y, 0.47), 0.16, 'fix.moss', sides=5,
		             color=jitter(palette.MOSS, 0.14))


# ────────────────────────────────────────────────────────────
#  바위
# ────────────────────────────────────────────────────────────

def rock_boulder(mb):
	mb.ico((0.0, 0.0, 0.46), 0.62, 'fix.stone_granite',
	       scale=(1.18, 0.94, 0.78), color=jitter(palette.STONE_GRANITE, 0.16),
	       smooth=False)
	mb.ico((0.34, 0.20, 0.26), 0.30, 'fix.stone_granite',
	       scale=(1.0, 1.1, 0.8), color=jitter(palette.STONE_GRANITE, 0.18),
	       smooth=False)
	mb.plane_fan((-0.10, 0.06, 0.86), 0.26, 'fix.moss', sides=6,
	             color=jitter(palette.MOSS, 0.14))


def rock_cluster(mb):
	for x, y, r, h in ((0.0, 0.0, 0.30, 0.9), (0.46, 0.18, 0.21, 0.8),
	                   (-0.34, 0.26, 0.17, 0.7), (0.14, -0.40, 0.24, 0.85)):
		mb.ico((x, y, r * h * 0.9), r, 'fix.stone_granite',
		       scale=(1.1, 0.95, h), color=jitter(palette.STONE_GRANITE, 0.18),
		       smooth=False)


def rock_ruin_block(mb):
	"""깎인 돌 — 폐허용. 자연석과 달리 각이 서 있어야 '사람이 만든 것' 으로 읽힌다."""
	mb.block((0.0, 0.0, 0.30), (0.86, 0.62, 0.60), 'fix.stone_ruin',
	         taper=0.94, color=jitter(palette.STONE_RUIN, 0.12))
	mb.block((0.12, -0.06, 0.66), (0.54, 0.44, 0.16), 'fix.stone_ruin',
	         taper=0.88, rot=(0.0, 0.0, 0.3))
	mb.plane_fan((-0.18, 0.14, 0.605), 0.20, 'fix.moss', sides=5)


# ────────────────────────────────────────────────────────────
#  풀 · 덤불
# ────────────────────────────────────────────────────────────

def _blade(mb, x, y, h, lean, yaw, mat, color):
	"""풀잎 한 장 — 아래가 넓고 위가 뾰족한 쐐기. 알파가 없어 정렬 걱정이 없다."""
	mb.block((x, y, h * 0.5), (0.055, 0.016, h), mat,
	         taper=0.12, top_shift=(math.cos(yaw) * lean, math.sin(yaw) * lean),
	         color=color)


def grass_tuft(mb):
	for i in range(9):
		t = RNG.uniform(0.0, math.tau)
		r = RNG.uniform(0.0, 0.14)
		h = RNG.uniform(0.16, 0.30)

		tall = h > 0.24

		_blade(mb, math.cos(t) * r, math.sin(t) * r, h,
		       RNG.uniform(0.03, 0.10), t,
		       'fix.grass_light' if tall else 'fix.grass',
		       jitter(palette.GRASS_LIGHT if tall else palette.GRASS_DARK, 0.18))


def grass_reed(mb):
	"""갈대 — 물가. 키가 크고 곧다."""
	for i in range(7):
		t = RNG.uniform(0.0, math.tau)
		r = RNG.uniform(0.0, 0.12)
		h = RNG.uniform(0.70, 1.15)

		_blade(mb, math.cos(t) * r, math.sin(t) * r, h,
		       RNG.uniform(0.10, 0.22), t, 'fix.grass_light',
		       jitter(palette.mix(palette.GRASS_DARK, palette.SAND, 0.25), 0.18))

		if i % 3 == 0:
			mb.block((math.cos(t) * r, math.sin(t) * r, h + 0.07),
			         (0.035, 0.035, 0.16), 'fix.leaf_dead', taper=0.4)


def plant_fern(mb):
	for i in range(6):
		t = i / 6.0 * math.tau + RNG.uniform(-0.2, 0.2)
		tip = (math.cos(t) * 0.36, math.sin(t) * 0.36, 0.30)

		mb.limb((0.0, 0.0, 0.04), tip, 0.022, 0.008, 'fix.leaf_summer', sides=4)

		for k in range(3):
			f = (k + 1) / 4.0
			p = (tip[0] * f, tip[1] * f, 0.04 + (tip[2] - 0.04) * f)

			mb.block(p, (0.16, 0.05, 0.02), 'fix.leaf_summer',
			         rot=(0.0, 0.0, t), color=jitter(palette.LEAF_SUMMER, 0.16))


def bush_round(mb):
	for x, y, z, r in ((0.0, 0.0, 0.34, 0.42), (0.28, 0.14, 0.26, 0.26),
	                   (-0.22, 0.20, 0.24, 0.22), (0.06, -0.26, 0.28, 0.24)):
		mb.ico((x, y, z), r, 'fix.leaf_summer', scale=(1.0, 1.0, 0.82),
		       color=jitter(palette.LEAF_SUMMER, 0.18), smooth=False)

	mb.limb((0.0, 0.0, 0.0), (0.0, 0.0, 0.16), 0.07, 0.05, 'fix.bark', sides=4)


def bush_thorn(mb):
	"""가시덤불 — 길을 막는 용도. 잎이 적고 가지가 많다."""
	for i in range(9):
		t = RNG.uniform(0.0, math.tau)
		h = RNG.uniform(0.30, 0.62)

		mb.limb((0.0, 0.0, 0.02),
		        (math.cos(t) * RNG.uniform(0.18, 0.42), math.sin(t) * RNG.uniform(0.18, 0.42), h),
		        0.025, 0.008, 'fix.wood_dark', sides=4)

	for i in range(5):
		t = RNG.uniform(0.0, math.tau)

		mb.ico((math.cos(t) * 0.22, math.sin(t) * 0.22, RNG.uniform(0.24, 0.48)),
		       0.14, 'fix.leaf_dead', scale=(1.0, 1.0, 0.7),
		       color=jitter(palette.LEAF_DEAD, 0.16), smooth=False)


def mushroom_cluster(mb):
	"""버섯 — 던전밥 정서. 먹을 수 있는 것과 아닌 것이 있다는 암시."""
	for x, y, h, r in ((0.0, 0.0, 0.16, 0.11), (0.14, 0.09, 0.11, 0.08),
	                   (-0.11, 0.07, 0.08, 0.06), (0.05, -0.13, 0.13, 0.09)):
		mb.limb((x, y, 0.0), (x, y, h), r * 0.34, r * 0.30, 'fix.mushroom_pale', sides=5)
		mb.ico((x, y, h + r * 0.22), r, 'fix.mushroom_cap',
		       scale=(1.0, 1.0, 0.52), color=jitter(palette.MUSHROOM_CAP, 0.20))


def mushroom_glow(mb):
	"""발광 버섯 — 이계의 기운이 닿은 자리. 폐허 깊은 곳의 유일한 광원."""
	for x, y, h, r in ((0.0, 0.0, 0.20, 0.09), (0.13, 0.08, 0.14, 0.07),
	                   (-0.10, 0.10, 0.11, 0.055)):
		mb.limb((x, y, 0.0), (x, y, h), r * 0.30, r * 0.26, 'fix.mushroom_pale', sides=5)
		mb.ico((x, y, h + r * 0.20), r, 'emit.eldritch', scale=(1.0, 1.0, 0.56))


def flower_patch(mb):
	for i in range(7):
		t = RNG.uniform(0.0, math.tau)
		r = RNG.uniform(0.04, 0.20)
		h = RNG.uniform(0.14, 0.26)
		x, y = math.cos(t) * r, math.sin(t) * r

		mb.limb((x, y, 0.0), (x, y, h), 0.010, 0.008, 'fix.grass', sides=4)
		mb.plane_fan((x, y, h + 0.012), 0.045, 'fix.cloth_linen', sides=5,
		             color=jitter(palette.CLOTH_LINEN, 0.22))


FLORA = [
	('nat_tree_oak',          tree_oak),
	('nat_tree_pine',         tree_pine),
	('nat_tree_birch',        tree_birch),
	('nat_tree_dead',         tree_dead),
	('nat_tree_stump',        tree_stump),
	('nat_tree_sapling',      tree_sapling),
	('nat_tree_log_fallen',   log_fallen),
	('nat_rock_boulder',      rock_boulder),
	('nat_rock_cluster',      rock_cluster),
	('nat_rock_ruin_block',   rock_ruin_block),
	('nat_grass_tuft',        grass_tuft),
	('nat_grass_reed',        grass_reed),
	('nat_plant_fern',        plant_fern),
	('nat_bush_round',        bush_round),
	('nat_bush_thorn',        bush_thorn),
	('nat_mushroom_cluster',  mushroom_cluster),
	('nat_mushroom_glow',     mushroom_glow),
	('nat_flower_patch',      flower_patch),
]


# ────────────────────────────────────────────────────────────
#  땅 — World.h 의 TILE_* 와 일대일
# ────────────────────────────────────────────────────────────

GROUND = [
	# (메쉬 이름,            대응 TILE,       머티리얼,           기준색,                 기복,  색흔들림(비율))
	('nat_ground_grass',    'TILE_GRASS',    'fix.grass',        palette.GRASS_DARK,    0.028, 0.20),
	('nat_ground_meadow',   'TILE_MEADOW',   'fix.grass_light',  palette.MEADOW,        0.022, 0.18),
	('nat_ground_path',     'TILE_PATH',     'fix.earth',        palette.EARTH,         0.012, 0.15),
	('nat_ground_sand',     'TILE_SAND',     'fix.sand',         palette.SAND,          0.016, 0.13),
	('nat_ground_shallow',  'TILE_SHALLOW',  'fix.water_shallow', palette.WATER_SHALLOW, 0.008, 0.12),
	('nat_ground_water',    'TILE_WATER',    'fix.water_deep',   palette.WATER_DEEP,    0.004, 0.10),
	('nat_ground_floor',    'TILE_FLOOR',    'fix.stone_ruin',   palette.STONE_RUIN,    0.006, 0.12),
	('nat_ground_wall',     'TILE_WALL',     'fix.stone_dark',   palette.STONE_DARK,    0.010, 0.14),
]


def ground_tile(mb, mat, base, relief, rough, div=4):
	"""1 m 타일. 정점마다 높이와 색을 흔들어 반복감을 죽인다.

	가장자리 정점은 **흔들지 않는다** — 흔들면 옆 타일과 높이가 어긋나 금이 간다.
	"""
	step = TILE / div
	half = TILE * 0.5
	idx = {}

	for j in range(div + 1):
		for i in range(div + 1):
			x = -half + i * step
			y = -half + j * step
			edge = (i == 0 or j == 0 or i == div or j == div)
			z = 0.0 if edge else RNG.uniform(-relief, relief)

			idx[(i, j)] = len(mb.verts)
			mb.verts.append(build.Vector((x, y, z)))
			mb.vert_bones.append([])
			mb.vert_colors.append(jitter(base, rough))

	for j in range(div):
		for i in range(div):
			mb.faces.append((idx[(i, j)], idx[(i + 1, j)],
			                 idx[(i + 1, j + 1)], idx[(i, j + 1)]))
			mb.face_mats.append(mat)
			mb.smooth.append(False)


def ground_stairs(mb):
	"""흙·돌 계단 — 지형 높이차를 잇는다. 격자 1 m 에 맞춘 세 단."""
	for i in range(3):
		h = 0.18 * (i + 1)

		mb.block((0.0, -TILE * 0.5 + TILE / 3.0 * (i + 0.5), h * 0.5),
		         (TILE, TILE / 3.0, h), 'fix.stone_ruin',
		         color=jitter(palette.STONE_RUIN, 0.12))
		mb.block((0.0, -TILE * 0.5 + TILE / 3.0 * (i + 0.5), h),
		         (TILE, TILE / 3.0, 0.02), 'fix.stone_granite')


def ground_slope(mb):
	"""경사 타일 — 한 칸에 0.5 m 오른다."""
	half = TILE * 0.5

	base = len(mb.verts)
	pts = [(-half, -half, 0.0), (half, -half, 0.0),
	       (half, half, 0.50), (-half, half, 0.50)]

	for p in pts:
		mb.verts.append(build.Vector(p))
		mb.vert_bones.append([])
		mb.vert_colors.append(jitter(palette.EARTH, 0.16))

	mb.faces.append((base, base + 1, base + 2, base + 3))
	mb.face_mats.append('fix.earth')
	mb.smooth.append(False)


# ────────────────────────────────────────────────────────────
#  실행
# ────────────────────────────────────────────────────────────

def main():
	# ── 식생 ──
	build.reset_scene()
	build.clear_material_cache()

	print("\n[flora]")

	objects = []
	flora_meta = []

	for name, fn in FLORA:
		mb = build.MeshBuilder()
		fn(mb)

		obj = mb.bake(name)
		objects.append(obj)
		flora_meta.append({'name': name, 'tris': build.tri_count(obj)})

	export.report(objects, label="flora")

	flora_path = os.path.join(ROOT, 'export', 'nature', 'flora.gltf')
	export.write(flora_path, objects)
	export.save_blend(os.path.join(ROOT, 'blend', 'nature_flora.blend'))

	print("  → %s" % flora_path)

	# ── 지형 ──
	build.reset_scene()
	build.clear_material_cache()

	print("\n[terrain]")

	objects = []
	ground_meta = []

	for name, tile, mat, base, relief, rough in GROUND:
		mb = build.MeshBuilder()
		ground_tile(mb, mat, base, relief, rough)

		obj = mb.bake(name)
		objects.append(obj)

		ground_meta.append({
			'name': name,
			'tile_enum': tile,
			'material': mat,
			'base_color': list(base),
			'tris': build.tri_count(obj),
		})

	for name, fn in (('nat_ground_stairs', ground_stairs),
	                 ('nat_ground_slope', ground_slope)):
		mb = build.MeshBuilder()
		fn(mb)

		obj = mb.bake(name)
		objects.append(obj)
		ground_meta.append({'name': name, 'tile_enum': None,
		                    'material': 'fix.stone_ruin', 'tris': build.tri_count(obj)})

	export.report(objects, label="terrain")

	terrain_path = os.path.join(ROOT, 'export', 'nature', 'terrain.gltf')
	export.write(terrain_path, objects)
	export.save_blend(os.path.join(ROOT, 'blend', 'nature_terrain.blend'))

	print("  → %s" % terrain_path)

	# ── 카탈로그 ──
	materials = {}

	for name, (srgb, metallic, roughness) in sorted(palette.MATERIALS.items()):
		materials[name] = {
			'srgb': [round(c, 4) for c in srgb],
			'metallic': metallic,
			'roughness': roughness,
			'emissive': palette.EMISSIVE_STRENGTH.get(name, 0.0) if name.startswith('emit.') else 0.0,
			'swappable': name.startswith('slot.'),
		}

	data = {
		'_comment': 'gen_nature.py 가 생성. 손으로 고치지 말 것 — 다시 돌리면 덮어쓴다.',
		'tile_size_m': TILE,
		'flora': {'gltf': 'export/nature/flora.gltf', 'items': flora_meta},
		'ground': {'gltf': 'export/nature/terrain.gltf', 'items': ground_meta},
		'_ground_note': 'tile_enum 은 SimpleGame/World.h 의 TILE_* 와 일대일로 맞춰 두었다.',
		'materials': materials,
		'_materials_note': 'srgb 는 눈에 보이는 값. glTF 의 baseColorFactor 는 선형이라 '
		                   'palette.lin() 으로 변환된 뒤 들어간다.',
	}

	out = os.path.join(ROOT, 'data', 'nature_catalog.json')
	export.ensure_dir(out)

	with open(out, 'w', encoding='utf-8') as f:
		json.dump(data, f, ensure_ascii=False, indent='\t')

	print("\n  → %s" % out)

	return data


if __name__ == '__main__':
	main()
