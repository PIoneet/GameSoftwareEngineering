# -*- coding: utf-8 -*-
"""4족 짐승 파츠 + 애니메이션 생성.

    blender --background --python tools/gen_quadruped.py

결과물
    export/creatures/feline.gltf · canine.gltf · cervid.gltf (+ .bin)
    blend/creature_<family>.blend
    data/creature_catalog.json

## 과(科)마다 파일 하나, 종(種)은 그 안의 조합

늑대와 마을개는 뼈대가 같다. 크기와 털색, 귀 모양이 다를 뿐이다.
종마다 파일을 만들면 **같은 스켈레톤과 같은 애니메이션을 7벌 복제**하게 된다.

그래서 파일은 과 단위로 셋만 만들고, 종은 `data/creature_catalog.json` 에
"어느 파츠를 켜고, 몇 배로 키우고, 무슨 색을 칠할지" 로만 적는다.
휴머노이드를 파츠로 조립한 것과 같은 생각이다.

**릭은 어깨높이 1.0 m 로 굽는다.** 종별 크기는 게임이 균일 스케일로 맞춘다
(`scale` 필드). 그래야 애니메이션 하나로 살쾡이부터 큰뿔사슴까지 덮는다.
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

from common import anim_quadruped, build, export, palette
from common import rig_quadruped as rig


SH = 1.0   # 릭 기준 어깨높이. 종별 크기는 catalog 의 scale 로 준다


def P(family, name):
	return Vector(rig.J(family, name, SH))


# ────────────────────────────────────────────────────────────
#  몸통
# ────────────────────────────────────────────────────────────
#
# 척추 역(station)마다 (반폭, 반높이). 과의 실루엣이 여기서 갈린다.
#   고양이과 — 단면이 둥글고 고르게 길다
#   개  과 — 흉곽이 깊다(세로로 길다). 허리가 잘록하다
#   사슴과 — 전체가 가늘고, 배가 통처럼 둥글다

TORSO = {
	'feline': {
		'hips':     (0.118, 0.112),
		'spine_03': (0.126, 0.118),
		'spine_02': (0.124, 0.116),
		'spine_01': (0.128, 0.124),
		'chest':    (0.130, 0.136),
	},
	'canine': {
		'hips':     (0.110, 0.108),
		'spine_03': (0.118, 0.114),
		'spine_02': (0.108, 0.112),   # 허리가 들어간다
		'spine_01': (0.126, 0.136),
		'chest':    (0.128, 0.152),   # 깊은 흉곽
	},
	'cervid': {
		'hips':     (0.106, 0.112),
		'spine_03': (0.114, 0.122),
		'spine_02': (0.112, 0.124),
		'spine_01': (0.112, 0.130),
		'chest':    (0.104, 0.134),
	},
}

SPINE_CHAIN = (
	('hips',     'spine_03', 'hips'),
	('spine_03', 'spine_02', 'spine_03'),
	('spine_02', 'spine_01', 'spine_02'),
	('spine_01', 'chest',    'spine_01'),
)

# 사지 반지름 (어깨높이 1.0 기준). 과별 BULK['limb'] 가 곱해진다
LIMB_R = {
	'scapula':    (0.062, 0.052),
	'upperarm':   (0.054, 0.044),
	'lowerarm':   (0.044, 0.032),
	'frontfoot':  (0.032, 0.030),
	'thigh':      (0.072, 0.056),
	'shin':       (0.054, 0.038),
	'hock':       (0.038, 0.030),
	'metatarsus': (0.030, 0.027),
}


def body(mb, family):
	"""몸통 + 목. 다리·머리·꼬리는 따로 만든다 (파츠 교체를 위해)."""
	t = TORSO[family]
	bulk = rig.BULK[family]['torso']

	for a_name, b_name, bone in SPINE_CHAIN:
		a = P(family, a_name)
		b = P(family, b_name)

		wa, ha = t[a_name]
		wb, hb = t[b_name]

		mid = (a + b) * 0.5
		length = abs(b.y - a.y)

		mb.block((mid.x, mid.y, mid.z),
		         (2 * wa * bulk, 2 * ha * bulk, length),
		         'slot.fur_main',
		         taper=(wb / wa), taper_y=(hb / ha),
		         rot=(math.pi * 0.5, 0.0, 0.0),
		         bone=bone)

	# 배 — 아래쪽만 밝은 털. 거의 모든 포유류가 이렇다
	hips = P(family, 'hips')
	chest = P(family, 'chest')
	belly_z = min(hips.z, chest.z) - t['spine_02'][1] * bulk * 0.78

	mb.block((0.0, (hips.y + chest.y) * 0.5, belly_z),
	         (2 * t['spine_02'][0] * bulk * 0.82, abs(chest.y - hips.y) * 0.86, 0.040),
	         'slot.fur_belly', bone='spine_02')

	# 목
	neck_bulk = rig.BULK[family]['neck']

	for a_name, b_name, bone, r0, r1 in (
		('chest',   'neck_01', 'chest',   0.098, 0.088),
		('neck_01', 'neck_02', 'neck_01', 0.088, 0.078),
		('neck_02', 'head',    'neck_02', 0.078, 0.070),
	):
		mb.limb(P(family, a_name), P(family, b_name),
		        r0 * neck_bulk, r1 * neck_bulk,
		        'slot.fur_main', bone=bone, sides=6)

	# 어깨뼈 — 걸을 때 등 위로 오르내리는 그 봉우리. 생략하면 몸통이 통나무가 된다
	for s in ('L', 'R'):
		sc = P(family, 'scapula_' + s)

		mb.block((sc.x * 0.72, sc.y, sc.z - 0.02),
		         (0.070 * bulk, 0.150, 0.130),
		         'slot.fur_main', taper=0.78, bone='scapula_' + s)


# ────────────────────────────────────────────────────────────
#  머리
# ────────────────────────────────────────────────────────────

HEAD = {
	# (두개골 폭, 두개골 높이, 두개골 길이, 주둥이 폭, 주둥이 높이, 주둥이 길이)
	'feline': (0.112, 0.104, 0.130, 0.072, 0.064, 0.070),
	'canine': (0.098, 0.096, 0.126, 0.062, 0.058, 0.130),   # 주둥이가 길다
	'cervid': (0.082, 0.090, 0.124, 0.052, 0.058, 0.136),   # 가늘고 길다
}

EARS = {
	'feline': {'w': 0.052, 'h': 0.072, 'x': 0.062, 'back': 0.030, 'up': 0.070},
	'canine': {'w': 0.046, 'h': 0.090, 'x': 0.054, 'back': 0.018, 'up': 0.078},
	'cervid': {'w': 0.070, 'h': 0.100, 'x': 0.062, 'back': 0.010, 'up': 0.054},
}


def head(mb, family, predator=True):
	w, h, l, mw, mh, ml = HEAD[family]
	hd = P(family, 'head')
	end = P(family, 'head_end')

	fwd = (end - hd).normalized()
	skull_c = hd + fwd * (l * 0.42)

	mb.block((skull_c.x, skull_c.y, skull_c.z), (2 * w, 2 * h, l),
	         'slot.fur_main', taper=0.88, rot=(math.pi * 0.5, 0.0, 0.0), bone='head')

	# 주둥이
	muzzle_c = hd + fwd * (l * 0.60 + ml * 0.45) + Vector((0.0, 0.0, -h * 0.34))

	mb.block((muzzle_c.x, muzzle_c.y, muzzle_c.z), (2 * mw, 2 * mh, ml),
	         'slot.fur_main', taper=0.76, rot=(math.pi * 0.5, 0.0, 0.0), bone='head')

	# 코
	nose = hd + fwd * (l * 0.60 + ml * 0.94) + Vector((0.0, 0.0, -h * 0.28))

	mb.block((nose.x, nose.y, nose.z), (mw * 0.90, 0.024, mh * 0.80),
	         'fix.nose', taper=0.82, bone='head')

	# 눈 — 포식자는 앞을 보고(양안시), 초식동물은 옆을 본다(시야각)
	eye_mat = 'emit.eye_glow' if predator else 'slot.eye'
	ex = w * (0.66 if predator else 0.96)
	ey = l * (0.30 if predator else 0.10)

	for sx in (1.0, -1.0):
		p = skull_c + fwd * ey + Vector((sx * ex, 0.0, h * 0.26))

		mb.block((p.x, p.y, p.z), (0.030, 0.026, 0.024), eye_mat, bone='head')

	# 턱
	jaw = P(family, 'jaw')
	jaw_end = P(family, 'jaw_end')
	jc = (jaw + jaw_end) * 0.5

	mb.block((jc.x, jc.y, jc.z), (2 * mw * 0.86, 0.034, (jaw_end - jaw).length),
	         'slot.fur_belly', taper=0.80, rot=(math.pi * 0.5, 0.0, 0.0), bone='jaw')

	if predator:
		# 송곳니 — 위험한 짐승이라는 신호. 작아도 실루엣에서 읽힌다
		for sx in (1.0, -1.0):
			p = hd + fwd * (l * 0.60 + ml * 0.80) + Vector((sx * mw * 0.56, 0.0, -h * 0.60))

			mb.block((p.x, p.y, p.z), (0.014, 0.014, 0.030), 'fix.tooth',
			         taper=0.30, bone='head')


def ears(mb, family):
	e = EARS[family]
	hd = P(family, 'head')
	end = P(family, 'head_end')
	fwd = (end - hd).normalized()

	base = hd + fwd * (HEAD[family][2] * 0.18)

	for sx in (1.0, -1.0):
		p = base + Vector((sx * e['x'], e['back'], e['up']))

		mb.wedge((p.x, p.y, p.z), (e['w'], 0.022, e['h']), 'slot.fur_main',
		         axis='y', bone='head')
		mb.wedge((p.x, p.y - 0.012, p.z), (e['w'] * 0.66, 0.012, e['h'] * 0.76),
		         'slot.fur_belly', axis='y', bone='head')


# ────────────────────────────────────────────────────────────
#  다리
# ────────────────────────────────────────────────────────────

def legs(mb, family):
	bulk = rig.BULK[family]['limb']
	paw = rig.BULK[family]['paw']
	hoofed = (family == 'cervid')

	start = mb.mark()

	# ── 앞다리 ──
	for a, b, bone, key in (
		('scapula_L',  'upperarm_L',  'scapula_L',  'scapula'),
		('upperarm_L', 'lowerarm_L',  'upperarm_L', 'upperarm'),
		('lowerarm_L', 'frontfoot_L', 'lowerarm_L', 'lowerarm'),
		('frontfoot_L', 'fronttoe_L', 'frontfoot_L', 'frontfoot'),
	):
		r0, r1 = LIMB_R[key]

		mb.limb(P(family, a), P(family, b), r0 * bulk, r1 * bulk,
		        'slot.fur_main', bone=bone, sides=5)

	# ── 뒷다리 — 5단. 비절에서 꺾이는 게 보여야 한다 ──
	for a, b, bone, key in (
		('thigh_L',      'shin_L',       'thigh_L',      'thigh'),
		('shin_L',       'hock_L',       'shin_L',       'shin'),
		('hock_L',       'metatarsus_L', 'hock_L',       'hock'),
		('metatarsus_L', 'hindtoe_L',    'metatarsus_L', 'metatarsus'),
	):
		r0, r1 = LIMB_R[key]

		mb.limb(P(family, a), P(family, b), r0 * bulk, r1 * bulk,
		        'slot.fur_main', bone=bone, sides=5)

	# 허벅지 근육 덩어리 — 뒷다리가 앞다리보다 굵어야 "뒤로 차고 나가는" 인상이 산다
	th = P(family, 'thigh_L')
	sh = P(family, 'shin_L')
	thc = th.lerp(sh, 0.34)

	mb.ico((thc.x, thc.y, thc.z), 0.092 * bulk, 'slot.fur_main',
	       scale=(0.78, 1.25, 1.05), bone='thigh_L')

	# ── 발 ──
	for toe, toe_end, bone in (
		('fronttoe_L', 'fronttoe_end_L', 'fronttoe_L'),
		('hindtoe_L',  'hindtoe_end_L',  'hindtoe_L'),
	):
		a = P(family, toe)
		b = P(family, toe_end)
		c = (a + b) * 0.5

		if hoofed:
			# 발굽 — 끝이 뾰족하고 단단하다. 둘로 갈라진 우제류 발굽
			for sx in (0.4, -0.4):
				mb.block((c.x + sx * 0.016, c.y, c.z + 0.018),
				         (0.026 * paw, 0.046, (b - a).length * 0.9),
				         'fix.hoof', taper=0.70, rot=(math.pi * 0.5, 0.0, 0.0),
				         bone=bone)
		else:
			mb.block((c.x, c.y, c.z + 0.020),
			         (0.072 * paw, 0.040, (b - a).length * 1.05),
			         'slot.fur_main', taper=0.86, rot=(math.pi * 0.5, 0.0, 0.0),
			         bone=bone)

			# 발톱
			for sx in (0.9, 0.0, -0.9):
				mb.block((c.x + sx * 0.020 * paw, b.y - 0.006, c.z + 0.008),
				         (0.012, 0.018, 0.010), 'fix.claw', taper=0.35, bone=bone)

	mb.mirror_x(start)


# ────────────────────────────────────────────────────────────
#  꼬리 · 장식
# ────────────────────────────────────────────────────────────

TAIL_R = {
	'feline': (0.030, 0.026, 0.020),
	'canine': (0.042, 0.034, 0.022),   # 복슬복슬
	'cervid': (0.026, 0.018, 0.010),
}


def tail(mb, family):
	r = TAIL_R[family]

	for i, (a, b, bone) in enumerate((
		('tail_01', 'tail_02', 'tail_01'),
		('tail_02', 'tail_03', 'tail_02'),
		('tail_03', 'tail_end', 'tail_03'),
	)):
		pa = P(family, a)
		pb = P(family, b)

		if (pb - pa).length < 1e-4:
			continue

		mb.limb(pa, pb, r[i], r[i] * 0.80, 'slot.fur_main', bone=bone, sides=5)

	if family == 'cervid':
		# 흰 꼬리 안쪽 — 사슴이 도망칠 때 번쩍이는 그 신호
		pe = P(family, 'tail_end')

		mb.block((pe.x, pe.y + 0.012, pe.z), (0.044, 0.016, 0.052),
		         'slot.fur_belly', taper=0.70, bone='tail_03')


def mane(mb, family):
	"""갈기 — 목덜미를 따라 선 털. 늑대·표범의 위협 표현."""
	for a_name, b_name, bone in (('chest', 'neck_01', 'chest'),
	                              ('neck_01', 'neck_02', 'neck_01'),
	                              ('neck_02', 'head', 'neck_02')):
		a = P(family, a_name)
		b = P(family, b_name)
		c = (a + b) * 0.5
		d = (b - a).length

		mb.wedge((c.x, c.y, c.z + 0.062), (0.030, d * 0.96, 0.072),
		         'slot.fur_main', axis='y', bone=bone)


def antler(mb, family):
	"""뿔 — 사슴과 수컷. 가지가 갈라져야 뿔로 보인다."""
	hd = P(family, 'head')
	end = P(family, 'head_end')
	fwd = (end - hd).normalized()
	base = hd + fwd * (HEAD[family][2] * 0.16) + Vector((0.0, 0.0, 0.058))

	start = mb.mark()

	root = base + Vector((0.052, 0.0, 0.0))
	beam1 = root + Vector((0.040, 0.030, 0.110))
	beam2 = beam1 + Vector((0.030, -0.020, 0.108))
	beam3 = beam2 + Vector((0.014, -0.060, 0.082))

	mb.limb(root, beam1, 0.022, 0.018, 'fix.horn', bone='head', sides=4)
	mb.limb(beam1, beam2, 0.018, 0.014, 'fix.horn', bone='head', sides=4)
	mb.limb(beam2, beam3, 0.014, 0.009, 'fix.horn', bone='head', sides=4)

	# 가지 — 앞으로 세 개
	for src, tip in (
		(beam1, beam1 + Vector((0.018, -0.080, 0.036))),
		(beam2, beam2 + Vector((0.016, -0.086, 0.044))),
		(beam2, beam2 + Vector((0.050, -0.018, 0.052))),
	):
		mb.limb(src, tip, 0.013, 0.006, 'fix.horn', bone='head', sides=4)

	mb.mirror_x(start)


def taint(mb, family):
	"""오염 — '잠든 것' 에 닿은 개체에 돋는 돌기.

	별도 파츠로 두면 어느 짐승에게나 얹을 수 있다. 같은 늑대인데 이것만 켜면
	플레이어는 "뭔가 잘못됐다" 를 즉시 읽는다 — 새 모델을 만들 필요가 없다.
	"""
	spots = [
		('spine_02', 0.0, 0.0, 0.105, 0.044),
		('spine_01', 0.052, -0.02, 0.098, 0.034),
		('spine_03', -0.048, 0.03, 0.095, 0.030),
		('neck_01', 0.0, 0.0, 0.082, 0.026),
		('hips', 0.044, 0.02, 0.090, 0.028),
	]

	for bone, dx, dy, dz, r in spots:
		p = P(family, bone if bone != 'neck_01' else 'neck_01')

		mb.limb((p.x + dx, p.y + dy, p.z + dz * 0.5),
		        (p.x + dx * 1.3, p.y + dy, p.z + dz + r * 1.6),
		        r, r * 0.25, 'emit.eldritch', bone=bone, sides=4)

	# 눈이 멀고 그 자리에 빛이 돈다
	hd = P(family, 'head')

	mb.ico((hd.x, hd.y - 0.06, hd.z + 0.03), 0.030, 'emit.eldritch',
	       scale=(1.3, 0.8, 0.9), bone='head')


# ────────────────────────────────────────────────────────────
#  파츠 등록 (과별)
# ────────────────────────────────────────────────────────────

def parts_for(family):
	rows = [
		('cre_%s_body' % family,  'body',  body),
		('cre_%s_head' % family,  'head',  head),
		('cre_%s_ears' % family,  'ears',  ears),
		('cre_%s_legs' % family,  'legs',  legs),
		('cre_%s_tail' % family,  'tail',  tail),
		('cre_%s_taint' % family, 'taint', taint),
	]

	if family in ('feline', 'canine'):
		rows.append(('cre_%s_mane' % family, 'mane', mane))

	if family == 'cervid':
		rows.append(('cre_cervid_antler', 'antler', antler))

	return rows


# ────────────────────────────────────────────────────────────
#  종 목록
# ────────────────────────────────────────────────────────────
#
# temperament — 던전밥식 "어떻게 상대할 것인가" 의 첫 단계.
#   dangerous : 보면 덤빈다
#   neutral   : 건드리면 싸우고, 아니면 제 갈 길을 간다
#   gentle    : 도망친다. 공격하려면 쫓아가야 한다

SPECIES = [
	{
		'id': 'wolf_winterfang', 'label': '겨울이빨늑대',
		'family': 'canine', 'diet': 'carnivore', 'temperament': 'dangerous',
		'shoulder_height': 0.85,
		'parts': ['body', 'head', 'ears', 'legs', 'tail', 'mane'],
		'fur': ('wolf_main', 'wolf_belly'),
		'counter': '무리로 온다. 좁은 길목에서 하나씩 상대하면 수가 줄어든다.',
	},
	{
		'id': 'hound_village', 'label': '마을개',
		'family': 'canine', 'diet': 'carnivore', 'temperament': 'gentle',
		'shoulder_height': 0.55,
		'parts': ['body', 'head', 'ears', 'legs', 'tail'],
		'fur': ('hound_main', 'hound_belly'),
		'counter': '싸울 상대가 아니다. 먹이를 주면 따라온다.',
	},
	{
		'id': 'panther_ruin', 'label': '폐허표범',
		'family': 'feline', 'diet': 'carnivore', 'temperament': 'dangerous',
		'shoulder_height': 0.78,
		'parts': ['body', 'head', 'ears', 'legs', 'tail'],
		'fur': ('panther_main', 'panther_belly'),
		'counter': '높은 곳에서 덮친다. 천장이 트인 방에서는 위를 봐야 한다.',
	},
	{
		'id': 'lynx_forest', 'label': '숲살쾡이',
		'family': 'feline', 'diet': 'carnivore', 'temperament': 'neutral',
		'shoulder_height': 0.50,
		'parts': ['body', 'head', 'ears', 'legs', 'tail'],
		'fur': ('lynx_main', 'lynx_belly'),
		'counter': '먼저 건드리지 않으면 지나간다. 새끼가 근처면 돌변한다.',
	},
	{
		'id': 'stag_great', 'label': '큰뿔사슴',
		'family': 'cervid', 'diet': 'herbivore', 'temperament': 'neutral',
		'shoulder_height': 1.30,
		'parts': ['body', 'head', 'ears', 'legs', 'tail', 'antler'],
		'fur': ('stag_main', 'stag_belly'),
		'counter': '정면으로 받는다. 옆으로 돌면 뿔이 닿지 않는다.',
	},
	{
		'id': 'doe_red', 'label': '붉은사슴',
		'family': 'cervid', 'diet': 'herbivore', 'temperament': 'gentle',
		'shoulder_height': 1.05,
		'parts': ['body', 'head', 'ears', 'legs', 'tail'],
		'fur': ('doe_main', 'doe_belly'),
		'counter': '달아난다. 몰이를 하거나 활이 필요하다.',
	},
	{
		'id': 'wolf_tainted', 'label': '오염된 늑대',
		'family': 'canine', 'diet': 'carnivore', 'temperament': 'dangerous',
		'shoulder_height': 0.95,
		'parts': ['body', 'head', 'ears', 'legs', 'tail', 'mane', 'taint'],
		'fur': ('panther_main', 'wolf_belly'),
		'counter': '아프지 않은 듯 계속 달려든다. 빛을 싫어한다.',
	},
]


# ────────────────────────────────────────────────────────────
#  실행
# ────────────────────────────────────────────────────────────

def build_family(family):
	build.reset_scene()
	build.clear_material_cache()

	arm = rig.build(family, SH, name="rig_" + family)

	print("\n[%s] 파츠" % family)

	objects = [arm]
	meta = []

	for name, slot, fn in parts_for(family):
		mb = build.MeshBuilder()

		if fn is head:
			fn(mb, family, predator=(family != 'cervid'))
		else:
			fn(mb, family)

		obj = mb.bake(name)
		build.bind(obj, arm)

		obj['slot'] = slot
		objects.append(obj)

		meta.append({'name': name, 'slot': slot, 'tris': build.tri_count(obj)})

	export.report(objects[1:], budget=1200, label=family)

	print("[%s] 애니메이션" % family)
	clips = anim_quadruped.build_all(arm, family)

	for c in clips:
		print("    %-12s %5.2fs  %s" % (c['name'], c['duration'],
		                                "loop" if c['loop'] else ""))

	gltf = os.path.join(ROOT, 'export', 'creatures', '%s.gltf' % family)
	export.write(gltf, objects)
	export.save_blend(os.path.join(ROOT, 'blend', 'creature_%s.blend' % family))

	print("  → %s" % gltf)

	return meta, clips


def main():
	families = {}

	for family in rig.FAMILIES:
		meta, clips = build_family(family)

		families[family] = {
			'gltf': 'export/creatures/%s.gltf' % family,
			'armature': 'rig_' + family,
			'rig_shoulder_height': SH,
			'gait_contact': {k: v for k, v in anim_quadruped.GAIT_CONTACT.items()},
			'parts': meta,
			'clips': clips,
		}

	species = []

	for s in SPECIES:
		main_fur, belly_fur = s['fur']
		prefix = 'cre_%s_' % s['family']

		species.append({
			'id': s['id'],
			'label': s['label'],
			'family': s['family'],
			'diet': s['diet'],
			'temperament': s['temperament'],
			'shoulder_height_m': s['shoulder_height'],
			'scale': round(s['shoulder_height'] / SH, 4),
			'meshes': [(prefix + p if p != 'antler' else 'cre_cervid_antler')
			           for p in s['parts']],
			'color': {
				'fur_main': list(palette.FUR[main_fur]),
				'fur_belly': list(palette.FUR[belly_fur]),
				'eye': list(palette.EYE_PREDATOR if s['diet'] == 'carnivore'
				            else palette.EYE_PREY),
			},
			'counter': s['counter'],
		})

	data = {
		'_comment': 'gen_quadruped.py 가 생성. 손으로 고치지 말 것 — 다시 돌리면 덮어쓴다.',
		'_note': '릭은 어깨높이 1.0 m 로 구워져 있다. 종별 크기는 scale 로 균일 적용한다.',
		'families': families,
		'species': species,
		'temperaments': {
			'dangerous': '보면 덤빈다',
			'neutral':   '건드리면 싸운다',
			'gentle':    '도망친다',
		},
	}

	out = os.path.join(ROOT, 'data', 'creature_catalog.json')
	export.ensure_dir(out)

	with open(out, 'w', encoding='utf-8') as f:
		json.dump(data, f, ensure_ascii=False, indent='\t')

	print("\n  → %s" % out)

	return data


if __name__ == '__main__':
	main()
