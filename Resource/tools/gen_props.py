# -*- coding: utf-8 -*-
"""주거 환경 — 건축 모듈 · 가구 · 생활 도구.

    blender --background --python tools/gen_props.py

결과물
    export/props/structure.gltf · furniture.gltf · tools.gltf (+ .bin)
    blend/props.blend
    data/prop_catalog.json

## 격자에 맞춰 조립한다

건축 모듈은 전부 **가로 2.0 m 격자**에 맞춰 만든다. 벽 한 장이 정확히 한 칸을
채우므로 나란히 놓기만 하면 틈이 없다. 집 한 채는 벽 몇 장 + 모서리 + 지붕이다.

    MODULE  2.00 m   한 칸 폭
    WALL_H  2.40 m   벽 높이 (7등신 1.75 m 캐릭터가 지나기에 넉넉하다)
    WALL_T  0.20 m   벽 두께
    PITCH   40도     지붕 경사

**모든 메쉬의 원점은 바닥 접지면 중앙**이다 (FORMAT.md 2절). 그래야 게임에서
바닥 높이만 맞추면 알아서 선다. 미리보기를 위해 흩어 놓지 않는다 — 흩어 놓으면
glTF 노드에 그 위치가 실려 나가 로더가 다시 빼야 한다.
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

from common import build, export, palette


MODULE = 2.00
WALL_H = 2.40
WALL_T = 0.20
PITCH = math.radians(40.0)


# ────────────────────────────────────────────────────────────
#  건축 모듈
# ────────────────────────────────────────────────────────────

def _timber_frame(mb, w, h, t, mat='fix.wood_dark', post=0.14, braces=True):
	"""목골조 — 회벽에 검은 기둥이 드러나는 중세 북유럽식.

	벽을 단색 상자로 두면 아무리 색을 맞춰도 판자때기로 보인다.
	기둥 몇 개만 얹어도 '지어진 것' 으로 읽힌다.
	"""
	for x in (-w * 0.5 + post * 0.5, w * 0.5 - post * 0.5):
		mb.block((x, 0.0, h * 0.5), (post, t * 1.08, h), mat)

	mb.block((0.0, 0.0, h - post * 0.5), (w, t * 1.08, post), mat)
	mb.block((0.0, 0.0, post * 0.4), (w, t * 1.08, post * 0.8), mat)

	if braces:
		# 빗댄 가새 둘 — 이 사선이 목골조의 인상 전부다
		for sx in (1.0, -1.0):
			mb.block((sx * w * 0.26, 0.0, h * 0.52),
			         (post * 0.8, t * 1.04, h * 0.78), mat,
			         rot=(0.0, sx * math.radians(26.0), 0.0))


def wall_straight(mb):
	mb.block((0.0, 0.0, WALL_H * 0.5), (MODULE, WALL_T, WALL_H), 'fix.stone_sandy')
	_timber_frame(mb, MODULE, WALL_H, WALL_T)

	# 기초 — 돌로 한 단
	mb.block((0.0, 0.0, 0.16), (MODULE + 0.06, WALL_T + 0.08, 0.32), 'fix.stone_granite')


def wall_window(mb):
	ow, oh, oz = 0.72, 0.78, 1.42

	# 창 주변만 남기고 네 조각으로 벽을 두른다
	side = (MODULE - ow) * 0.5

	for sx in (1.0, -1.0):
		mb.block((sx * (ow + side) * 0.5, 0.0, WALL_H * 0.5),
		         (side, WALL_T, WALL_H), 'fix.stone_sandy')

	mb.block((0.0, 0.0, (oz - oh * 0.5) * 0.5), (ow, WALL_T, oz - oh * 0.5), 'fix.stone_sandy')
	mb.block((0.0, 0.0, (oz + oh * 0.5 + WALL_H) * 0.5),
	         (ow, WALL_T, WALL_H - oz - oh * 0.5), 'fix.stone_sandy')

	# 창틀과 덧문
	mb.block((0.0, 0.0, oz), (ow + 0.10, WALL_T * 0.6, oh + 0.10), 'fix.wood_dark')
	mb.block((0.0, 0.0, oz), (0.06, WALL_T * 0.5, oh), 'fix.wood_dark')
	mb.block((0.0, 0.0, oz), (ow, WALL_T * 0.4, 0.06), 'fix.wood_dark')

	_timber_frame(mb, MODULE, WALL_H, WALL_T, braces=False)
	mb.block((0.0, 0.0, 0.16), (MODULE + 0.06, WALL_T + 0.08, 0.32), 'fix.stone_granite')


def wall_door(mb):
	dw, dh = 0.92, 1.96
	side = (MODULE - dw) * 0.5

	for sx in (1.0, -1.0):
		mb.block((sx * (dw + side) * 0.5, 0.0, WALL_H * 0.5),
		         (side, WALL_T, WALL_H), 'fix.stone_sandy')

	mb.block((0.0, 0.0, (dh + WALL_H) * 0.5), (dw, WALL_T, WALL_H - dh), 'fix.stone_sandy')

	# 상인방
	mb.block((0.0, 0.0, dh + 0.08), (dw + 0.24, WALL_T * 1.1, 0.16), 'fix.wood_oak')

	# 문짝 — 판자 다섯 장 + 쇠띠. 열리는 연출이 필요하면 이 블록만 따로 떼면 된다
	for i in range(5):
		x = (i - 2) * (dw / 5.0)

		mb.block((x, -WALL_T * 0.42, dh * 0.5), (dw / 5.0 - 0.012, 0.05, dh), 'fix.wood_weathered')

	for z in (dh * 0.24, dh * 0.76):
		mb.block((0.0, -WALL_T * 0.48, z), (dw, 0.03, 0.08), 'fix.metal_rust')

	mb.block((dw * 0.34, -WALL_T * 0.56, dh * 0.5), (0.07, 0.06, 0.07), 'fix.metal_iron')

	_timber_frame(mb, MODULE, WALL_H, WALL_T, braces=False)
	mb.block((0.0, 0.0, 0.16), (MODULE + 0.06, WALL_T + 0.08, 0.32), 'fix.stone_granite')


def wall_corner(mb):
	"""ㄱ 자 모서리. 두 벽이 만나는 자리를 덮어 틈을 없앤다."""
	h = WALL_H

	mb.block((0.0, 0.0, h * 0.5), (WALL_T + 0.04, WALL_T + 0.04, h), 'fix.stone_sandy')
	mb.block((0.0, 0.0, h * 0.5), (0.18, 0.18, h), 'fix.wood_dark')
	mb.block((0.0, 0.0, 0.16), (WALL_T + 0.14, WALL_T + 0.14, 0.32), 'fix.stone_granite')


def roof_slope(mb):
	"""한쪽 경사면 한 칸. 두 장을 마주 세우면 박공지붕이 된다."""
	run = MODULE * 0.5
	rise = run * math.tan(PITCH)
	length = math.hypot(run, rise)

	mb.block((0.0, run * 0.5, rise * 0.5),
	         (MODULE, length, 0.14), 'fix.thatch',
	         rot=(PITCH, 0.0, 0.0))

	# 서까래
	for x in (-MODULE * 0.38, 0.0, MODULE * 0.38):
		mb.block((x, run * 0.5, rise * 0.5 - 0.10),
		         (0.08, length, 0.08), 'fix.wood_dark',
		         rot=(PITCH, 0.0, 0.0))


def roof_ridge(mb):
	"""용마루 덮개. 두 경사면이 만나는 선을 가린다."""
	mb.wedge((0.0, 0.0, 0.06), (0.34, MODULE, 0.16), 'fix.thatch_old', axis='y')


def roof_gable(mb):
	"""박공 — 지붕 양 끝의 삼각 벽."""
	rise = MODULE * 0.5 * math.tan(PITCH)

	mb.wedge((0.0, 0.0, rise * 0.5), (MODULE, WALL_T, rise), 'fix.wood_weathered', axis='x')
	mb.block((0.0, 0.0, 0.04), (MODULE, WALL_T * 1.1, 0.10), 'fix.wood_dark')


def floor_tile(mb):
	mb.block((0.0, 0.0, -0.06), (MODULE, MODULE, 0.12), 'fix.wood_weathered')

	for i in range(5):
		y = (i - 2) * (MODULE / 5.0)

		mb.block((0.0, y, 0.005), (MODULE, MODULE / 5.0 - 0.016, 0.02), 'fix.wood_oak')


def pillar(mb):
	mb.block((0.0, 0.0, WALL_H * 0.5), (0.22, 0.22, WALL_H), 'fix.wood_oak', taper=0.92)
	mb.block((0.0, 0.0, 0.10), (0.34, 0.34, 0.20), 'fix.stone_granite')
	mb.block((0.0, 0.0, WALL_H - 0.08), (0.32, 0.32, 0.16), 'fix.wood_dark')


def beam(mb):
	mb.block((0.0, 0.0, 0.0), (0.18, MODULE, 0.20), 'fix.wood_oak')


def stair(mb):
	"""계단 한 칸(2.0 m 폭, 네 단). 층을 나눌 때 격자에 그대로 맞는다."""
	steps = 4
	rise = 0.60 / steps
	run = MODULE / steps

	for i in range(steps):
		mb.block((0.0, -MODULE * 0.5 + run * (i + 0.5), rise * (i + 0.5)),
		         (MODULE, run, rise * (i + 1)), 'fix.wood_weathered')
		mb.block((0.0, -MODULE * 0.5 + run * (i + 0.5), rise * (i + 1)),
		         (MODULE, run, 0.03), 'fix.wood_oak')


def fence_straight(mb):
	for x in (-MODULE * 0.5 + 0.08, MODULE * 0.5 - 0.08):
		mb.block((x, 0.0, 0.52), (0.12, 0.12, 1.04), 'fix.wood_weathered', taper=0.86)

	for z in (0.42, 0.84):
		mb.block((0.0, 0.0, z), (MODULE, 0.07, 0.10), 'fix.wood_weathered')


def fence_gate(mb):
	fence_straight(mb)

	for i in range(4):
		x = (i - 1.5) * 0.42

		mb.block((x, -0.06, 0.60), (0.10, 0.05, 1.00), 'fix.wood_pine')

	mb.block((0.0, -0.08, 0.62), (MODULE * 0.86, 0.04, 0.09), 'fix.wood_pine',
	         rot=(0.0, math.radians(22.0), 0.0))


def well(mb):
	"""우물 — 마을 광장의 중심. 지금 게임에도 MODEL_WELL 이 있다."""
	n = 10

	for i in range(n):
		t = i / n * math.tau
		x, y = math.cos(t) * 0.62, math.sin(t) * 0.62

		mb.block((x, y, 0.32), (0.26, 0.26, 0.64), 'fix.stone_granite',
		         rot=(0.0, 0.0, t), taper=0.90)

	for sx in (1.0, -1.0):
		mb.block((sx * 0.56, 0.0, 1.10), (0.12, 0.12, 1.50), 'fix.wood_dark')

	mb.block((0.0, 0.0, 1.86), (1.40, 0.16, 0.16), 'fix.wood_oak')
	mb.limb((-0.50, 0.0, 1.74), (0.50, 0.0, 1.74), 0.07, 0.07, 'fix.wood_oak', sides=6)
	mb.block((0.0, 0.0, 1.30), (0.30, 0.30, 0.34), 'fix.wood_weathered', taper=1.06)
	mb.block((0.0, 0.0, 1.49), (0.36, 0.36, 0.05), 'fix.metal_rust')


STRUCTURE = [
	('prop_house_wall_straight', wall_straight),
	('prop_house_wall_window',   wall_window),
	('prop_house_wall_door',     wall_door),
	('prop_house_corner',        wall_corner),
	('prop_house_roof_slope',    roof_slope),
	('prop_house_roof_ridge',    roof_ridge),
	('prop_house_roof_gable',    roof_gable),
	('prop_house_floor',         floor_tile),
	('prop_house_pillar',        pillar),
	('prop_house_beam',          beam),
	('prop_house_stair',         stair),
	('prop_fence_straight',      fence_straight),
	('prop_fence_gate',          fence_gate),
	('prop_house_well',          well),
]


# ────────────────────────────────────────────────────────────
#  가구
# ────────────────────────────────────────────────────────────

def _legs4(mb, w, d, h, t=0.08, mat='fix.wood_oak'):
	for sx in (1.0, -1.0):
		for sy in (1.0, -1.0):
			mb.block((sx * (w * 0.5 - t * 0.7), sy * (d * 0.5 - t * 0.7), h * 0.5),
			         (t, t, h), mat, taper=0.88)


def table(mb):
	w, d, h = 1.60, 0.80, 0.78

	_legs4(mb, w, d, h - 0.06)
	mb.block((0.0, 0.0, h - 0.03), (w, d, 0.06), 'fix.wood_pine')
	mb.block((0.0, 0.0, h - 0.14), (w - 0.18, d - 0.18, 0.05), 'fix.wood_oak')


def bench(mb):
	w, d, h = 1.40, 0.34, 0.46

	_legs4(mb, w, d, h - 0.05, t=0.07)
	mb.block((0.0, 0.0, h - 0.025), (w, d, 0.05), 'fix.wood_pine')


def stool(mb):
	h = 0.44

	for i in range(3):
		t = i / 3.0 * math.tau

		mb.block((math.cos(t) * 0.13, math.sin(t) * 0.13, h * 0.5 - 0.02),
		         (0.06, 0.06, h - 0.04), 'fix.wood_oak',
		         rot=(0.0, 0.0, t), taper=0.80)

	mb.block((0.0, 0.0, h - 0.02), (0.36, 0.36, 0.05), 'fix.wood_pine', taper=0.94)


def bed(mb):
	w, d, h = 0.96, 1.94, 0.42

	_legs4(mb, w, d, h, t=0.10, mat='fix.wood_dark')
	mb.block((0.0, 0.0, h + 0.04), (w, d, 0.08), 'fix.wood_oak')

	# 짚 매트리스와 모직 담요
	mb.block((0.0, 0.02, h + 0.14), (w - 0.08, d - 0.10, 0.14), 'fix.thatch')
	mb.block((0.0, 0.18, h + 0.23), (w - 0.06, d * 0.64, 0.07), 'fix.cloth_wool')
	mb.block((0.0, -d * 0.36, h + 0.25), (w * 0.58, 0.30, 0.12), 'fix.cloth_linen', taper=0.86)

	mb.block((0.0, -d * 0.5 + 0.05, h + 0.42), (w, 0.10, 0.66), 'fix.wood_dark')


def chest(mb):
	w, d, h = 0.84, 0.48, 0.52

	mb.block((0.0, 0.0, h * 0.42), (w, d, h * 0.84), 'fix.wood_oak')
	mb.wedge((0.0, 0.0, h * 0.92), (w, d, 0.18), 'fix.wood_oak', axis='y')

	for x in (-w * 0.3, w * 0.3):
		mb.block((x, 0.0, h * 0.46), (0.05, d + 0.02, h * 0.92), 'fix.metal_rust')

	mb.block((0.0, -d * 0.5, h * 0.52), (0.12, 0.04, 0.14), 'fix.metal_iron')


def shelf(mb):
	w, d, h = 1.10, 0.32, 1.70

	for sx in (1.0, -1.0):
		mb.block((sx * (w * 0.5 - 0.04), 0.0, h * 0.5), (0.08, d, h), 'fix.wood_weathered')

	for i in range(4):
		z = 0.30 + i * 0.44

		mb.block((0.0, 0.0, z), (w - 0.12, d, 0.05), 'fix.wood_pine')


def barrel(mb):
	n = 7
	h = 0.84

	for i in range(n):
		t = i / n * math.tau

		mb.block((math.cos(t) * 0.30, math.sin(t) * 0.30, h * 0.5),
		         (0.28, 0.10, h), 'fix.wood_weathered',
		         rot=(0.0, 0.0, t), taper=0.86)

	# 테는 고리 하나로 만든다. 블록을 둘러 세우면 같은 모양에 폴리곤만 네 배 든다
	for z in (h * 0.22, h * 0.78):
		mb.limb((0.0, 0.0, z - 0.03), (0.0, 0.0, z + 0.03),
		        0.325, 0.325, 'fix.metal_rust', sides=7)

	mb.block((0.0, 0.0, h - 0.02), (0.52, 0.52, 0.04), 'fix.wood_dark', taper=0.94)


def crate(mb):
	s = 0.62

	mb.block((0.0, 0.0, s * 0.5), (s, s, s), 'fix.wood_pine')

	for z in (s * 0.18, s * 0.82):
		mb.block((0.0, 0.0, z), (s + 0.02, s + 0.02, 0.05), 'fix.wood_dark')


def cupboard(mb):
	w, d, h = 0.92, 0.42, 1.58

	mb.block((0.0, 0.0, h * 0.5), (w, d, h), 'fix.wood_oak')

	for sx in (1.0, -1.0):
		mb.block((sx * w * 0.24, -d * 0.5 - 0.01, h * 0.62),
		         (w * 0.44, 0.03, h * 0.58), 'fix.wood_weathered')
		mb.block((sx * 0.06, -d * 0.54, h * 0.62), (0.04, 0.05, 0.10), 'fix.metal_iron')

	mb.block((0.0, 0.0, h + 0.03), (w + 0.10, d + 0.08, 0.06), 'fix.wood_dark')


FURNITURE = [
	('prop_furn_table',    table),
	('prop_furn_bench',    bench),
	('prop_furn_stool',    stool),
	('prop_furn_bed',      bed),
	('prop_furn_chest',    chest),
	('prop_furn_shelf',    shelf),
	('prop_furn_barrel',   barrel),
	('prop_furn_crate',    crate),
	('prop_furn_cupboard', cupboard),
]


# ────────────────────────────────────────────────────────────
#  생활 도구
# ────────────────────────────────────────────────────────────

def bucket(mb):
	n = 7
	h = 0.30

	for i in range(n):
		t = i / n * math.tau

		mb.block((math.cos(t) * 0.15, math.sin(t) * 0.15, h * 0.5),
		         (0.14, 0.05, h), 'fix.wood_weathered', rot=(0.0, 0.0, t), taper=1.12)

	mb.block((0.0, 0.0, 0.02), (0.28, 0.28, 0.04), 'fix.wood_dark')
	mb.limb((-0.17, 0.0, h), (0.17, 0.0, h), 0.012, 0.012, 'fix.metal_rust', sides=4)


def lantern(mb):
	"""등불 — 이 게임의 유일한 따뜻한 광원."""
	mb.block((0.0, 0.0, 0.03), (0.16, 0.16, 0.06), 'fix.metal_iron', taper=0.82)
	mb.block((0.0, 0.0, 0.17), (0.13, 0.13, 0.22), 'emit.lantern', taper=0.94)

	for sx in (1.0, -1.0):
		for sy in (1.0, -1.0):
			mb.block((sx * 0.065, sy * 0.065, 0.17), (0.018, 0.018, 0.24), 'fix.metal_iron')

	mb.block((0.0, 0.0, 0.31), (0.19, 0.19, 0.05), 'fix.metal_iron', taper=0.60)
	mb.limb((-0.05, 0.0, 0.345), (0.05, 0.0, 0.345), 0.010, 0.010, 'fix.metal_iron', sides=4)


def cauldron(mb):
	mb.ico((0.0, 0.0, 0.26), 0.26, 'fix.metal_iron', scale=(1.0, 1.0, 0.82), smooth=True)
	mb.block((0.0, 0.0, 0.44), (0.50, 0.50, 0.05), 'fix.metal_iron', taper=1.04)

	for i in range(3):
		t = i / 3.0 * math.tau

		mb.block((math.cos(t) * 0.16, math.sin(t) * 0.16, 0.05),
		         (0.05, 0.05, 0.10), 'fix.metal_iron', rot=(0.0, 0.0, t))


def anvil(mb):
	mb.block((0.0, 0.0, 0.22), (0.26, 0.40, 0.44), 'fix.wood_dark', taper=0.86)
	mb.block((0.0, 0.0, 0.50), (0.22, 0.62, 0.14), 'fix.metal_iron')
	mb.block((0.0, 0.0, 0.58), (0.26, 0.70, 0.06), 'fix.metal_pewter')
	mb.block((0.0, -0.44, 0.54), (0.14, 0.24, 0.10), 'fix.metal_iron', taper=0.30)


def basket(mb):
	n = 9
	h = 0.26

	for i in range(n):
		t = i / n * math.tau

		mb.block((math.cos(t) * 0.20, math.sin(t) * 0.20, h * 0.5),
		         (0.15, 0.04, h), 'fix.thatch', rot=(0.0, 0.0, t), taper=1.18)

	mb.block((0.0, 0.0, 0.02), (0.34, 0.34, 0.04), 'fix.thatch_old')


def sack(mb):
	mb.block((0.0, 0.0, 0.20), (0.38, 0.34, 0.40), 'fix.cloth_linen', taper=0.70)
	mb.block((0.0, 0.0, 0.43), (0.16, 0.14, 0.08), 'fix.cloth_linen', taper=1.30)
	mb.limb((-0.09, 0.0, 0.40), (0.09, 0.0, 0.40), 0.012, 0.012, 'fix.leather', sides=4)


def rope_coil(mb):
	for i in range(3):
		r = 0.20 - i * 0.025

		mb.limb((-r, 0.0, 0.03 + i * 0.045), (r, 0.0, 0.03 + i * 0.045),
		        0.022, 0.022, 'fix.thatch_old', sides=5)
		mb.limb((0.0, -r, 0.03 + i * 0.045), (0.0, r, 0.03 + i * 0.045),
		        0.022, 0.022, 'fix.thatch_old', sides=5)


def ladder(mb):
	h = 2.20

	for sx in (1.0, -1.0):
		mb.block((sx * 0.21, 0.0, h * 0.5), (0.07, 0.05, h), 'fix.wood_pine')

	for i in range(7):
		mb.limb((-0.21, 0.0, 0.18 + i * 0.30), (0.21, 0.0, 0.18 + i * 0.30),
		        0.022, 0.022, 'fix.wood_oak', sides=5)


def cart_wheel(mb):
	n = 8
	r = 0.44

	for i in range(n):
		t = i / n * math.tau

		mb.block((0.0, math.cos(t) * r, math.sin(t) * r + r),
		         (0.08, 0.36, 0.09), 'fix.wood_oak', rot=(-t, 0.0, 0.0))
		mb.limb((0.0, 0.0, r), (0.0, math.cos(t) * r * 0.9, math.sin(t) * r * 0.9 + r),
		        0.022, 0.018, 'fix.wood_pine', sides=4)

	mb.limb((-0.07, 0.0, r), (0.07, 0.0, r), 0.07, 0.07, 'fix.wood_dark', sides=6)


def sign_board(mb):
	mb.block((0.0, 0.0, 1.10), (0.12, 0.12, 2.20), 'fix.wood_dark', taper=0.90)
	mb.block((0.0, 0.0, 2.14), (0.70, 0.10, 0.10), 'fix.wood_dark')
	mb.block((0.22, 0.0, 1.78), (0.56, 0.05, 0.42), 'fix.wood_weathered')

	for sx in (-0.14, 0.14):
		mb.limb((0.22 + sx, 0.0, 2.10), (0.22 + sx, 0.0, 1.99),
		        0.008, 0.008, 'fix.metal_rust', sides=4)


def broom(mb):
	mb.limb((0.0, 0.0, 0.18), (0.0, 0.0, 1.42), 0.020, 0.017, 'fix.wood_pine', sides=5)
	mb.block((0.0, 0.0, 0.11), (0.22, 0.14, 0.22), 'fix.thatch', taper=1.35)
	mb.limb((-0.07, 0.0, 0.22), (0.07, 0.0, 0.22), 0.010, 0.010, 'fix.thatch_old', sides=4)


TOOLS = [
	('prop_tool_bucket',     bucket),
	('prop_tool_lantern',    lantern),
	('prop_tool_cauldron',   cauldron),
	('prop_tool_anvil',      anvil),
	('prop_tool_basket',     basket),
	('prop_tool_sack',       sack),
	('prop_tool_rope_coil',  rope_coil),
	('prop_tool_ladder',     ladder),
	('prop_tool_cart_wheel', cart_wheel),
	('prop_tool_sign_board', sign_board),
	('prop_tool_broom',      broom),
]


# 조립 예시 — "이 모듈들로 무엇을 만들 수 있는가" 를 보여 준다
BLUEPRINTS = [
	{
		'id': 'cottage_small', 'label': '작은 오두막 (2x2 칸)',
		'footprint': [2, 2],
		'pieces': [
			{'mesh': 'prop_house_floor', 'at': [0, 0, 0], 'yaw': 0},
			{'mesh': 'prop_house_floor', 'at': [2, 0, 0], 'yaw': 0},
			{'mesh': 'prop_house_wall_door', 'at': [0, -1, 0], 'yaw': 0},
			{'mesh': 'prop_house_wall_straight', 'at': [2, -1, 0], 'yaw': 0},
			{'mesh': 'prop_house_wall_window', 'at': [0, 1, 0], 'yaw': 180},
			{'mesh': 'prop_house_wall_straight', 'at': [2, 1, 0], 'yaw': 180},
			{'mesh': 'prop_house_corner', 'at': [-1, -1, 0], 'yaw': 0},
			{'mesh': 'prop_house_corner', 'at': [3, -1, 0], 'yaw': 0},
			{'mesh': 'prop_house_corner', 'at': [-1, 1, 0], 'yaw': 0},
			{'mesh': 'prop_house_corner', 'at': [3, 1, 0], 'yaw': 0},
			{'mesh': 'prop_house_roof_slope', 'at': [0, 0, 2.4], 'yaw': 0},
			{'mesh': 'prop_house_roof_slope', 'at': [2, 0, 2.4], 'yaw': 0},
			{'mesh': 'prop_house_roof_slope', 'at': [0, 0, 2.4], 'yaw': 180},
			{'mesh': 'prop_house_roof_slope', 'at': [2, 0, 2.4], 'yaw': 180},
			{'mesh': 'prop_house_roof_ridge', 'at': [0, 0, 3.24], 'yaw': 0},
			{'mesh': 'prop_house_roof_ridge', 'at': [2, 0, 3.24], 'yaw': 0},
		],
	},
	{
		'id': 'inn_interior', 'label': '여관 1층 배치',
		'footprint': [4, 4],
		'pieces': [
			{'mesh': 'prop_furn_table', 'at': [1.2, 0.8, 0], 'yaw': 0},
			{'mesh': 'prop_furn_bench', 'at': [1.2, 0.2, 0], 'yaw': 0},
			{'mesh': 'prop_furn_bench', 'at': [1.2, 1.4, 0], 'yaw': 0},
			{'mesh': 'prop_furn_table', 'at': [3.6, 0.8, 0], 'yaw': 0},
			{'mesh': 'prop_furn_stool', 'at': [3.0, 0.8, 0], 'yaw': 0},
			{'mesh': 'prop_furn_barrel', 'at': [0.4, 3.2, 0], 'yaw': 0},
			{'mesh': 'prop_furn_barrel', 'at': [1.0, 3.3, 0], 'yaw': 0},
			{'mesh': 'prop_furn_shelf', 'at': [2.4, 3.7, 0], 'yaw': 180},
			{'mesh': 'prop_tool_cauldron', 'at': [3.8, 3.4, 0], 'yaw': 0},
			{'mesh': 'prop_tool_lantern', 'at': [1.2, 0.8, 0.78], 'yaw': 0},
		],
	},
]


# ────────────────────────────────────────────────────────────
#  실행
# ────────────────────────────────────────────────────────────

def build_group(group, filename, budget):
	build.reset_scene()
	build.clear_material_cache()

	print("\n[%s]" % filename)

	objects = []
	meta = []

	for name, fn in group:
		mb = build.MeshBuilder()
		fn(mb)

		obj = mb.bake(name)
		objects.append(obj)

		meta.append({'name': name, 'tris': build.tri_count(obj)})

	export.report(objects, label=filename)

	over = [m for m in meta if m['tris'] > budget]
	if over:
		print("    ** 예산(%d) 초과: %s" % (budget, ", ".join(m['name'] for m in over)))

	path = os.path.join(ROOT, 'export', 'props', filename + '.gltf')
	export.write(path, objects)

	print("  → %s" % path)

	return meta, objects


def main():
	catalog = {}

	meta_s, _ = build_group(STRUCTURE, 'structure', 600)
	export.save_blend(os.path.join(ROOT, 'blend', 'props_structure.blend'))

	meta_f, _ = build_group(FURNITURE, 'furniture', 260)
	export.save_blend(os.path.join(ROOT, 'blend', 'props_furniture.blend'))

	meta_t, _ = build_group(TOOLS, 'tools', 220)
	export.save_blend(os.path.join(ROOT, 'blend', 'props_tools.blend'))

	catalog = {
		'_comment': 'gen_props.py 가 생성. 손으로 고치지 말 것 — 다시 돌리면 덮어쓴다.',
		'grid': {'module_m': MODULE, 'wall_height_m': WALL_H,
		         'wall_thickness_m': WALL_T, 'roof_pitch_deg': 40.0},
		'_note': '모든 메쉬의 원점은 바닥 접지면 중앙. blueprint 의 at 은 미터 단위.',
		'groups': {
			'structure': {'gltf': 'export/props/structure.gltf', 'items': meta_s},
			'furniture': {'gltf': 'export/props/furniture.gltf', 'items': meta_f},
			'tools':     {'gltf': 'export/props/tools.gltf', 'items': meta_t},
		},
		'blueprints': BLUEPRINTS,
	}

	out = os.path.join(ROOT, 'data', 'prop_catalog.json')
	export.ensure_dir(out)

	with open(out, 'w', encoding='utf-8') as f:
		json.dump(catalog, f, ensure_ascii=False, indent='\t')

	print("\n  → %s" % out)

	return catalog


if __name__ == '__main__':
	main()
