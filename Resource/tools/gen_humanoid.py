# -*- coding: utf-8 -*-
"""휴머노이드 캐릭터 파츠 + 애니메이션 생성.

    blender --background --python tools/gen_humanoid.py

결과물
    export/characters/humanoid.gltf (+ .bin)
    blend/humanoid.blend
    data/character_parts.json

## 왜 파일 하나에 다 넣는가

아마튜어 하나에 **모든 파츠가 함께** 들어 있고, 애니메이션도 그 아마튜어에 붙는다.
파츠마다 파일을 쪼개면 같은 스켈레톤과 같은 클립이 N 벌 복제된다 — 용량도 낭비고
무엇보다 **본 인덱스가 파일마다 달라져** 로더에서 맞춰 주어야 한다.
한 파일에 두면 스킨이 하나라 그럴 일이 없다. 게임은 필요한 메쉬만 그리면 된다.

좌표는 전부 tools/common/rig_humanoid.py 의 JOINTS 에서 가져온다. 여기서 숫자를
새로 적지 않는다 — 적는 순간 뼈와 살이 어긋나기 시작한다.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

if HERE not in sys.path:
	sys.path.insert(0, HERE)

import bpy
from mathutils import Vector

from common import anim_humanoid, build, export, palette
from common import rig_humanoid as rig


J = rig.J

# ── 몸통 단면 (절반 폭) ──
HIP_W, HIP_D = 0.118, 0.088
WAIST_W, WAIST_D = 0.108, 0.080
CHEST_W, CHEST_D = 0.140, 0.092
SHLD_W, SHLD_D = 0.168, 0.088

# ── 사지 반지름 ──
R_UPPERARM = (0.058, 0.047)
R_LOWERARM = (0.047, 0.038)
R_THIGH = (0.080, 0.060)
R_SHIN = (0.058, 0.042)


# ────────────────────────────────────────────────────────────
#  머리
# ────────────────────────────────────────────────────────────

def _skull_common(mb, mat='slot.skin'):
	"""턱·귀 — 세 머리 모양이 공유한다."""
	mb.block((0.000, -0.012, 1.533), (0.132, 0.150, 0.062), mat,
	         taper=1.12, bone='head')

	for x in (0.090, -0.090):
		mb.block((x, 0.004, 1.618), (0.022, 0.044, 0.056), mat,
		         taper=0.80, bone='head')


def head_round(mb):
	"""둥근 머리 — 기본형."""
	mb.ico((0.000, -0.004, 1.628), 0.100, 'slot.skin',
	       scale=(0.86, 0.96, 1.02), bone='head', smooth=True)

	_skull_common(mb)


def head_square(mb):
	"""각진 머리 — 완고한 인상. 대장장이·자경단."""
	mb.block((0.000, -0.006, 1.630), (0.168, 0.186, 0.190), 'slot.skin',
	         taper=0.90, taper_y=0.94, bone='head')
	mb.block((0.000, 0.020, 1.556), (0.150, 0.140, 0.052), 'slot.skin',
	         taper=1.04, bone='head')

	_skull_common(mb)


def head_long(mb):
	"""길쭉한 머리 — 학자·사제."""
	mb.block((0.000, -0.004, 1.636), (0.142, 0.176, 0.208), 'slot.skin',
	         taper=0.82, taper_y=0.88, bone='head')

	_skull_common(mb)


# ────────────────────────────────────────────────────────────
#  얼굴 — 머리와 분리해 둔다 (같은 두개골에 다른 표정)
# ────────────────────────────────────────────────────────────

FACE_Y = -0.094   # 얼굴 평면. 앞이 -Y


def _eyes(mb, z=1.646, w=0.030, deep=False):
	for x in (0.042, -0.042):
		mb.block((x, FACE_Y + 0.010, z), (w + 0.014, 0.014, 0.026),
		         'fix.eye_white', bone='head')
		mb.block((x, FACE_Y + 0.001, z), (w, 0.012, 0.020),
		         'slot.eye', bone='head')

		if deep:
			# 움푹한 눈두덩 — 늙거나 지친 인상
			mb.block((x, FACE_Y + 0.018, z + 0.026), (w + 0.024, 0.020, 0.012),
			         'slot.skin', bone='head')


def _nose(mb, z=1.600, size=(0.030, 0.040, 0.046)):
	mb.wedge((0.000, FACE_Y + 0.018, z), size, 'slot.skin', axis='x', bone='head')


def face_plain(mb):
	_eyes(mb)
	_nose(mb)

	# 입 — 선 하나로 충분하다. 로우폴리에서 입을 파면 지저분해진다
	mb.block((0.000, FACE_Y + 0.006, 1.556), (0.044, 0.010, 0.008),
	         'fix.leather_dark', bone='head')

	# 눈썹
	for x in (0.046, -0.046):
		mb.block((x, FACE_Y + 0.004, 1.672), (0.040, 0.012, 0.010),
		         'slot.hair', bone='head')


def face_bearded(mb):
	_eyes(mb)
	_nose(mb, size=(0.034, 0.044, 0.050))

	for x in (0.048, -0.048):
		mb.block((x, FACE_Y + 0.004, 1.674), (0.044, 0.012, 0.012),
		         'slot.hair', bone='head')

	# 수염 — 턱을 감싸고 아래로 흐른다
	mb.block((0.000, -0.020, 1.528), (0.140, 0.150, 0.070), 'slot.hair',
	         taper=0.92, bone='head')
	mb.block((0.000, -0.052, 1.474), (0.104, 0.080, 0.062), 'slot.hair',
	         taper=0.70, bone='head')

	# 콧수염
	mb.block((0.000, FACE_Y + 0.008, 1.566), (0.072, 0.016, 0.018),
	         'slot.hair', bone='head')


def face_elder(mb):
	_eyes(mb, deep=True)
	_nose(mb, size=(0.032, 0.046, 0.052))

	# 주름 — 이마에 가로 세 줄. 면을 더 쓰지 않고 색만 얹는다
	for i, z in enumerate((1.690, 1.706, 1.720)):
		mb.block((0.000, FACE_Y + 0.012, z), (0.104 - i * 0.012, 0.008, 0.006),
		         'fix.leather_dark', bone='head')

	mb.block((0.000, FACE_Y + 0.006, 1.552), (0.038, 0.010, 0.008),
	         'fix.leather_dark', bone='head')

	mb.block((0.000, -0.030, 1.520), (0.116, 0.130, 0.058), 'slot.hair',
	         taper=0.78, bone='head')


# ────────────────────────────────────────────────────────────
#  머리카락 · 머리장식
# ────────────────────────────────────────────────────────────

def hair_short(mb):
	mb.block((0.000, 0.000, 1.706), (0.180, 0.196, 0.064), 'slot.hair',
	         taper=0.78, bone='head')
	mb.block((0.000, 0.052, 1.646), (0.168, 0.096, 0.074), 'slot.hair',
	         taper=0.92, bone='head')


def hair_long(mb):
	mb.block((0.000, 0.000, 1.708), (0.184, 0.200, 0.062), 'slot.hair',
	         taper=0.80, bone='head')

	# 뒤로 흘러내리는 머리 — 목과 등 위쪽을 덮는다
	mb.block((0.000, 0.062, 1.628), (0.172, 0.082, 0.136), 'slot.hair',
	         taper=0.96, bone='head')
	mb.block((0.000, 0.070, 1.500), (0.158, 0.070, 0.130), 'slot.hair',
	         taper=0.90, bone='neck')
	mb.block((0.000, 0.074, 1.392), (0.140, 0.062, 0.110), 'slot.hair',
	         taper=0.82, bone='chest')

	# 옆머리
	for x in (0.086, -0.086):
		mb.block((x, 0.004, 1.624), (0.028, 0.170, 0.108), 'slot.hair',
		         taper=0.86, bone='head')


def hair_bun(mb):
	mb.block((0.000, 0.000, 1.704), (0.178, 0.194, 0.060), 'slot.hair',
	         taper=0.80, bone='head')
	mb.ico((0.000, 0.088, 1.700), 0.052, 'slot.hair',
	       scale=(0.92, 0.80, 0.86), bone='head')


def headgear_hood(mb):
	"""두건 — 이 세계의 기본 외출복. 얼굴에 그늘이 진다."""
	mb.block((0.000, 0.004, 1.672), (0.212, 0.228, 0.136), 'slot.cloth_main',
	         taper=0.74, bone='head')
	mb.block((0.000, 0.058, 1.556), (0.196, 0.136, 0.126), 'slot.cloth_main',
	         taper=0.96, bone='neck')

	# 어깨에 내려앉는 자락
	mb.block((0.000, 0.030, 1.432), (0.270, 0.210, 0.078), 'slot.cloth_main',
	         taper=1.10, bone='chest')

	# 챙 — 얼굴 위로 조금 나온다
	mb.wedge((0.000, -0.118, 1.690), (0.200, 0.064, 0.044), 'slot.cloth_trim',
	         axis='x', bone='head')


def headgear_cap(mb):
	mb.block((0.000, 0.000, 1.704), (0.190, 0.202, 0.072), 'slot.cloth_main',
	         taper=0.72, bone='head')
	mb.block((0.000, 0.000, 1.666), (0.206, 0.218, 0.020), 'slot.cloth_trim',
	         bone='head')


def headgear_helm(mb):
	mb.block((0.000, -0.002, 1.676), (0.186, 0.202, 0.124), 'slot.metal',
	         taper=0.80, bone='head')

	# 코가리개 — 투구를 투구로 보이게 하는 건 결국 이 한 조각이다
	mb.block((0.000, -0.098, 1.628), (0.026, 0.018, 0.096), 'slot.metal',
	         bone='head')

	for x in (0.092, -0.092):
		mb.block((x, 0.006, 1.600), (0.020, 0.150, 0.082), 'slot.metal',
		         taper=0.90, bone='head')


# ────────────────────────────────────────────────────────────
#  몸통
# ────────────────────────────────────────────────────────────

def _torso_stack(mb, mat, pad=0.0, hem_z=None, hem_mat=None):
	"""골반 → 어깨까지 블록 다섯 장. 각 장을 가장 가까운 척추 본에 묶는다.

	옷마다 두께(pad)만 달리하면 같은 실루엣의 변주가 쉽게 나온다.
	"""
	p = pad

	mb.block((0.000, 0.000, 0.930), (2 * (HIP_W + p), 2 * (HIP_D + p), 0.140),
	         mat, taper=0.94, bone='hips')
	mb.block((0.000, -0.002, 1.046), (2 * (WAIST_W + p), 2 * (WAIST_D + p), 0.096),
	         mat, taper=1.06, bone='spine_01')
	mb.block((0.000, -0.004, 1.140), (2 * (WAIST_W + p) * 1.06, 2 * (WAIST_D + p) * 1.04, 0.094),
	         mat, taper=1.12, bone='spine_02')
	mb.block((0.000, -0.004, 1.246), (2 * (CHEST_W + p), 2 * (CHEST_D + p), 0.120),
	         mat, taper=1.06, bone='chest')
	mb.block((0.000, 0.000, 1.356), (2 * (SHLD_W + p), 2 * (SHLD_D + p), 0.106),
	         mat, taper=0.88, taper_y=0.92, bone='chest')

	if hem_z is not None:
		mb.block((0.000, 0.000, hem_z), (2 * (HIP_W + p) * 1.04, 2 * (HIP_D + p) * 1.04, 0.022),
		         hem_mat or mat, bone='hips')


def _neck(mb, mat='slot.skin'):
	mb.limb((0.000, 0.004, 1.418), (0.000, 0.004, 1.534), 0.050, 0.044,
	        mat, bone='neck', sides=6)


def torso_tunic(mb):
	"""평민의 기본 — 무릎 위까지 오는 튜닉."""
	_neck(mb)
	_torso_stack(mb, 'slot.cloth_main', pad=0.012, hem_z=0.862, hem_mat='slot.cloth_trim')

	# 목깃
	mb.block((0.000, 0.000, 1.414), (0.136, 0.126, 0.026), 'slot.cloth_trim', bone='chest')


def torso_robe(mb):
	"""사제·학자의 긴 옷. 아래로 퍼진다."""
	_neck(mb)
	_torso_stack(mb, 'slot.cloth_main', pad=0.020)

	# 치맛자락 — 허벅지 본에 나눠 묶어야 걸을 때 다리를 따라간다
	mb.block((0.000, 0.000, 0.840), (0.286, 0.230, 0.150), 'slot.cloth_main',
	         taper=0.92, bone='hips')
	mb.block((0.062, 0.000, 0.700), (0.150, 0.216, 0.150), 'slot.cloth_main',
	         taper=1.04, bone='thigh_L')
	mb.block((-0.062, 0.000, 0.700), (0.150, 0.216, 0.150), 'slot.cloth_main',
	         taper=1.04, bone='thigh_R')

	mb.block((0.000, 0.000, 1.414), (0.148, 0.134, 0.030), 'slot.cloth_trim', bone='chest')


def torso_jerkin(mb):
	"""가죽 조끼 — 안에 린넨 속옷이 보인다."""
	_neck(mb)
	_torso_stack(mb, 'slot.cloth_trim', pad=0.008)

	# 조끼는 몸통 위에 한 겹 더
	mb.block((0.000, -0.004, 1.236), (2 * CHEST_W + 0.040, 2 * CHEST_D + 0.034, 0.168),
	         'slot.leather', taper=1.02, bone='chest')
	mb.block((0.000, -0.002, 1.088), (2 * WAIST_W + 0.034, 2 * WAIST_D + 0.030, 0.128),
	         'slot.leather', taper=1.06, bone='spine_01')

	# 어깨 보강
	for x in (0.148, -0.148):
		mb.block((x, 0.000, 1.370), (0.088, 0.164, 0.070), 'slot.leather',
		         taper=0.86, bone='chest')


def torso_apron(mb):
	"""앞치마 — 여관 주인, 대장장이, 농부."""
	_neck(mb)
	_torso_stack(mb, 'slot.cloth_main', pad=0.012, hem_z=0.862, hem_mat='slot.cloth_trim')

	mb.block((0.000, -0.104, 1.160), (0.210, 0.022, 0.300), 'slot.leather',
	         taper=1.08, bone='spine_02')
	mb.block((0.000, -0.098, 0.930), (0.226, 0.022, 0.170), 'slot.leather',
	         bone='hips')

	for x in (0.062, -0.062):
		mb.block((x, -0.046, 1.330), (0.030, 0.104, 0.024), 'slot.leather',
		         bone='chest')


# ────────────────────────────────────────────────────────────
#  팔 · 손
# ────────────────────────────────────────────────────────────

def _arm_side(mb, s, r_upper, r_lower, mat, cap=True):
	seg = rig.limb_segments(s)

	a0, a1, b_upper = seg['upperarm']
	b0, b1, b_lower = seg['lowerarm']

	if cap:
		# 어깨 삼각근 — 이게 없으면 팔이 몸통에 꽂힌 막대로 보인다
		mb.ico(tuple(a0), r_upper[0] * 1.24, mat,
		       scale=(1.0, 0.86, 0.92), bone=b_upper)

	mb.limb(a0, a1, r_upper[0], r_upper[1], mat, bone=b_upper, sides=5)
	mb.limb(b0, b1, r_lower[0], r_lower[1], mat, bone=b_lower, sides=5)


def arms_bare(mb):
	start = mb.mark()
	_arm_side(mb, 'L', R_UPPERARM, R_LOWERARM, 'slot.skin')
	mb.mirror_x(start)


def arms_sleeve(mb):
	start = mb.mark()

	seg = rig.limb_segments('L')
	a0, a1, b_upper = seg['upperarm']
	b0, b1, b_lower = seg['lowerarm']

	mb.ico(tuple(a0), 0.078, 'slot.cloth_main', scale=(1.0, 0.88, 0.94), bone=b_upper)
	mb.limb(a0, a1, 0.072, 0.058, 'slot.cloth_main', bone=b_upper, sides=5)
	mb.limb(b0, b0.lerp(b1, 0.72), 0.058, 0.050,
	        'slot.cloth_main', bone=b_lower, sides=5)

	# 소매 끝에서 맨살이 나온다
	mb.limb(b0.lerp(b1, 0.70), b1, 0.046, 0.038, 'slot.skin', bone=b_lower, sides=5)
	mb.limb(b0.lerp(b1, 0.66), b0.lerp(b1, 0.74), 0.062, 0.062,
	        'slot.cloth_trim', bone=b_lower, sides=6)

	mb.mirror_x(start)


def arms_bracer(mb):
	start = mb.mark()

	seg = rig.limb_segments('L')
	a0, a1, b_upper = seg['upperarm']
	b0, b1, b_lower = seg['lowerarm']

	mb.ico(tuple(a0), 0.072, 'slot.leather', scale=(1.0, 0.88, 0.94), bone=b_upper)
	mb.limb(a0, a1, 0.060, 0.048, 'slot.skin', bone=b_upper, sides=5)
	mb.limb(b0, b1, 0.048, 0.039, 'slot.skin', bone=b_lower, sides=5)

	# 팔보호대 — 아래팔 바깥에 한 겹
	mb.limb(b0.lerp(b1, 0.18), b0.lerp(b1, 0.92), 0.058, 0.048,
	        'slot.leather', bone=b_lower, sides=5)

	mb.mirror_x(start)


def _hand(mb, s, mat):
	seg = rig.limb_segments(s)
	h0, h1, bone = seg['hand']

	mb.limb(h0, h1, 0.040, 0.030, mat, bone=bone, sides=4)

	# 엄지 — 손을 손으로 보이게 하는 유일한 조각
	sign = 1.0 if s == 'L' else -1.0
	thumb = h0.lerp(h1, 0.30) + Vector((-0.024 * sign, -0.020, 0.0))

	mb.limb(h0.lerp(h1, 0.22), thumb, 0.017, 0.013, mat, bone=bone, sides=4)


def hands_bare(mb):
	start = mb.mark()
	_hand(mb, 'L', 'slot.skin')
	mb.mirror_x(start)


def hands_glove(mb):
	start = mb.mark()
	_hand(mb, 'L', 'slot.leather')

	seg = rig.limb_segments('L')
	h0, h1, bone = seg['hand']

	mb.limb(h0 + Vector((0.0, 0.0, 0.014)), h0 - Vector((0.0, 0.0, 0.008)),
	        0.048, 0.048, 'slot.cloth_trim', bone=bone, sides=5)

	mb.mirror_x(start)


# ────────────────────────────────────────────────────────────
#  다리 · 발
# ────────────────────────────────────────────────────────────

def _leg_side(mb, s, r_thigh, r_shin, mat):
	seg = rig.limb_segments(s)

	t0, t1, b_thigh = seg['thigh']
	s0, s1, b_shin = seg['shin']

	mb.limb(t0, t1, r_thigh[0], r_thigh[1], mat, bone=b_thigh, sides=5)
	mb.limb(s0, s1, r_shin[0], r_shin[1], mat, bone=b_shin, sides=5)

	# 무릎 — 관절이 접힐 때 빈틈이 생기지 않게 덩어리 하나를 끼운다
	mb.ico(tuple(t1), r_thigh[1] * 1.08, mat, scale=(1.0, 1.0, 0.86), bone=b_shin)


def legs_trouser(mb):
	start = mb.mark()
	_leg_side(mb, 'L', (0.086, 0.068), (0.066, 0.048), 'slot.cloth_main')
	mb.mirror_x(start)


def legs_bare(mb):
	start = mb.mark()
	_leg_side(mb, 'L', R_THIGH, R_SHIN, 'slot.skin')
	mb.mirror_x(start)


def legs_greaves(mb):
	start = mb.mark()
	_leg_side(mb, 'L', (0.082, 0.064), (0.060, 0.044), 'slot.cloth_main')

	seg = rig.limb_segments('L')
	s0, s1, b_shin = seg['shin']

	# 정강이받이는 앞쪽에만. 뒤까지 감싸면 무릎이 안 접힌다
	mb.limb(s0.lerp(s1, 0.12), s0.lerp(s1, 0.86), 0.070, 0.052,
	        'slot.metal', bone=b_shin, sides=5)

	mb.mirror_x(start)


def _foot(mb, s, mat, boot=False):
	seg = rig.limb_segments(s)

	f0, f1, b_foot = seg['foot']
	t0, t1, b_toe = seg['toe']
	x = f0.x

	mb.block((x, -0.046, 0.048), (0.092, 0.150, 0.080), mat,
	         taper=0.92, bone=b_foot)
	mb.block((x, -0.136, 0.030), (0.084, 0.072, 0.048), mat,
	         taper=0.86, bone=b_toe)

	# 밑창
	mb.block((x, -0.062, 0.012), (0.098, 0.206, 0.024), 'fix.leather_dark',
	         bone=b_foot)

	if boot:
		# 발목을 덮는 목 — 종아리 본에 묶는다
		mb.limb((x, 0.006, 0.062), (x, 0.014, 0.236), 0.072, 0.064,
		        mat, bone='shin_' + s, sides=5)
		mb.limb((x, 0.014, 0.228), (x, 0.016, 0.258), 0.078, 0.072,
		        'slot.cloth_trim', bone='shin_' + s, sides=5)


def feet_shoe(mb):
	start = mb.mark()
	_foot(mb, 'L', 'slot.leather')
	mb.mirror_x(start)


def feet_boot(mb):
	start = mb.mark()
	_foot(mb, 'L', 'slot.leather', boot=True)
	mb.mirror_x(start)


# ────────────────────────────────────────────────────────────
#  겉옷 · 소품
# ────────────────────────────────────────────────────────────

def cloak_short(mb):
	"""어깨에서 허리까지. 걸을 때 흔들리도록 척추 본에 나눠 묶는다."""
	mb.block((0.000, 0.064, 1.370), (0.372, 0.070, 0.120), 'slot.cloth_main',
	         taper=0.96, bone='chest')
	mb.block((0.000, 0.080, 1.240), (0.356, 0.058, 0.150), 'slot.cloth_main',
	         taper=1.02, bone='chest')
	mb.block((0.000, 0.092, 1.090), (0.340, 0.054, 0.160), 'slot.cloth_main',
	         taper=1.00, bone='spine_01')
	mb.block((0.000, 0.098, 0.950), (0.320, 0.050, 0.140), 'slot.cloth_main',
	         taper=0.96, bone='hips')

	# 어깨를 덮는 부분
	for x in (0.140, -0.140):
		mb.block((x, 0.010, 1.404), (0.120, 0.180, 0.034), 'slot.cloth_main',
		         bone='chest')

	# 여밈쇠
	mb.block((0.000, -0.092, 1.398), (0.040, 0.026, 0.030), 'slot.metal', bone='chest')


def cloak_hooded(mb):
	cloak_short(mb)

	# 뒤로 넘긴 후드 — 쓰지 않은 상태
	mb.block((0.000, 0.106, 1.470), (0.206, 0.104, 0.150), 'slot.cloth_main',
	         taper=0.86, bone='chest')


def belt_plain(mb):
	mb.block((0.000, 0.000, 1.000), (2 * WAIST_W + 0.034, 2 * WAIST_D + 0.030, 0.046),
	         'slot.leather', bone='spine_01')
	mb.block((0.000, -0.098, 1.000), (0.052, 0.024, 0.052), 'slot.metal', bone='spine_01')


def belt_pouch(mb):
	belt_plain(mb)

	mb.block((0.104, -0.020, 0.952), (0.072, 0.068, 0.086), 'slot.leather',
	         taper=0.88, bone='hips')
	mb.block((0.104, -0.020, 0.996), (0.080, 0.076, 0.018), 'slot.cloth_trim',
	         bone='hips')

	# 열쇠꾸러미 — 길드 서기·여관 주인용 디테일
	mb.block((-0.100, -0.040, 0.962), (0.018, 0.012, 0.052), 'slot.metal', bone='hips')


def backpack(mb):
	mb.block((0.000, 0.120, 1.250), (0.230, 0.130, 0.260), 'slot.leather',
	         taper=0.92, bone='chest')
	mb.block((0.000, 0.126, 1.372), (0.212, 0.120, 0.034), 'slot.cloth_trim',
	         bone='chest')

	for x in (0.088, -0.088):
		mb.block((x, 0.028, 1.330), (0.034, 0.126, 0.100), 'slot.leather',
		         bone='chest')

	# 묶어 둔 담요 — 여행자라는 걸 한눈에 보여 준다
	mb.limb((-0.110, 0.126, 1.126), (0.110, 0.126, 1.126), 0.038, 0.038,
	        'slot.cloth_main', bone='chest', sides=6)


# ────────────────────────────────────────────────────────────
#  파츠 등록
# ────────────────────────────────────────────────────────────

PARTS = [
	# (이름,                    슬롯,        생성함수,        기본 선택지)
	('chr_head_round',      'head',     head_round,      True),
	('chr_head_square',     'head',     head_square,     False),
	('chr_head_long',       'head',     head_long,       False),

	('chr_face_plain',      'face',     face_plain,      True),
	('chr_face_bearded',    'face',     face_bearded,    False),
	('chr_face_elder',      'face',     face_elder,      False),

	('chr_hair_short',      'hair',     hair_short,      True),
	('chr_hair_long',       'hair',     hair_long,       False),
	('chr_hair_bun',        'hair',     hair_bun,        False),

	('chr_torso_tunic',     'torso',    torso_tunic,     True),
	('chr_torso_robe',      'torso',    torso_robe,      False),
	('chr_torso_jerkin',    'torso',    torso_jerkin,    False),
	('chr_torso_apron',     'torso',    torso_apron,     False),

	('chr_arms_bare',       'arms',     arms_bare,       False),
	('chr_arms_sleeve',     'arms',     arms_sleeve,     True),
	('chr_arms_bracer',     'arms',     arms_bracer,     False),

	('chr_hands_bare',      'hands',    hands_bare,      True),
	('chr_hands_glove',     'hands',    hands_glove,     False),

	('chr_legs_trouser',    'legs',     legs_trouser,    True),
	('chr_legs_bare',       'legs',     legs_bare,       False),
	('chr_legs_greaves',    'legs',     legs_greaves,    False),

	('chr_feet_shoe',       'feet',     feet_shoe,       True),
	('chr_feet_boot',       'feet',     feet_boot,       False),

	('chr_cloak_short',     'cloak',    cloak_short,     False),
	('chr_cloak_hooded',    'cloak',    cloak_hooded,    False),

	('chr_belt_plain',      'belt',     belt_plain,      False),
	('chr_belt_pouch',      'belt',     belt_pouch,      False),

	('chr_headgear_hood',   'headgear', headgear_hood,   False),
	('chr_headgear_cap',    'headgear', headgear_cap,    False),
	('chr_headgear_helm',   'headgear', headgear_helm,   False),

	('chr_backpack',        'backpack', backpack,        False),
]


# 조립 예시 — 기존 Actors.cpp 의 LOOKS 와 짝이 맞게 15명분을 적어 둔다.
# 색 인덱스는 palette.CLOAK_TONES / SKIN_TONES / HAIR_TONES 를 가리킨다.
PRESETS = [
	('miren',   '미렌',   ['chr_head_round', 'chr_face_plain', 'chr_hair_long', 'chr_torso_robe',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe',
	                        'chr_cloak_short', 'chr_belt_plain'], 0, 0, 0),
	('hodd',    '호드',   ['chr_head_square', 'chr_face_bearded', 'chr_hair_short', 'chr_torso_apron',
	                        'chr_arms_bare', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_boot',
	                        'chr_belt_plain'], 1, 1, 1),
	('fel',     '펠',     ['chr_head_long', 'chr_face_plain', 'chr_hair_short', 'chr_torso_robe',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe',
	                        'chr_headgear_hood', 'chr_belt_pouch'], 2, 2, 2),
	('tama',    '타마',   ['chr_head_round', 'chr_face_plain', 'chr_hair_bun', 'chr_torso_tunic',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe',
	                        'chr_belt_plain'], 3, 2, 2),
	('goren',   '고렌',   ['chr_head_square', 'chr_face_bearded', 'chr_hair_short', 'chr_torso_jerkin',
	                        'chr_arms_bracer', 'chr_hands_glove', 'chr_legs_greaves', 'chr_feet_boot',
	                        'chr_belt_plain', 'chr_headgear_helm'], 4, 3, 0),
	('nib',     '닙',     ['chr_head_round', 'chr_face_plain', 'chr_hair_short', 'chr_torso_tunic',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe'], 5, 4, 4),
	('bosk',    '보스크', ['chr_head_square', 'chr_face_elder', 'chr_hair_short', 'chr_torso_robe',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe',
	                        'chr_cloak_hooded'], 6, 1, 1),
	('edra',    '에드라', ['chr_head_long', 'chr_face_elder', 'chr_hair_long', 'chr_torso_robe',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe',
	                        'chr_cloak_short'], 7, 0, 3),
	('marta',   '마르타', ['chr_head_round', 'chr_face_plain', 'chr_hair_bun', 'chr_torso_apron',
	                        'chr_arms_bare', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe',
	                        'chr_belt_pouch'], 8, 2, 2),
	('kolm',    '콜름',   ['chr_head_square', 'chr_face_bearded', 'chr_hair_short', 'chr_torso_jerkin',
	                        'chr_arms_bracer', 'chr_hands_glove', 'chr_legs_trouser', 'chr_feet_boot',
	                        'chr_belt_plain'], 9, 1, 0),
	('wina',    '위나',   ['chr_head_round', 'chr_face_plain', 'chr_hair_long', 'chr_torso_tunic',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe',
	                        'chr_belt_pouch'], 10, 4, 4),
	('dorn',    '도른',   ['chr_head_square', 'chr_face_bearded', 'chr_hair_short', 'chr_torso_tunic',
	                        'chr_arms_bare', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_boot',
	                        'chr_belt_plain', 'chr_backpack'], 11, 1, 0),
	('selbi',   '셀비',   ['chr_head_round', 'chr_face_plain', 'chr_hair_bun', 'chr_torso_tunic',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe'], 12, 2, 2),
	('harwin',  '하르윈', ['chr_head_long', 'chr_face_elder', 'chr_hair_long', 'chr_torso_robe',
	                        'chr_arms_sleeve', 'chr_hands_bare', 'chr_legs_trouser', 'chr_feet_shoe',
	                        'chr_cloak_hooded', 'chr_belt_pouch'], 13, 3, 3),
	('player',  '플레이어', ['chr_head_round', 'chr_face_plain', 'chr_hair_short', 'chr_torso_jerkin',
	                        'chr_arms_bracer', 'chr_hands_glove', 'chr_legs_trouser', 'chr_feet_boot',
	                        'chr_cloak_short', 'chr_belt_pouch'], 14, 0, 2),
]


# ────────────────────────────────────────────────────────────
#  실행
# ────────────────────────────────────────────────────────────

def main():
	build.reset_scene()
	build.clear_material_cache()

	arm = rig.build("rig_humanoid")

	print("\n[휴머노이드] 파츠 %d 개" % len(PARTS))

	objects = [arm]
	meta_parts = []

	for name, slot, fn, default in PARTS:
		mb = build.MeshBuilder()
		fn(mb)

		obj = mb.bake(name)
		build.bind(obj, arm)

		obj['slot'] = slot
		objects.append(obj)

		meta_parts.append({
			'name': name,
			'slot': slot,
			'default': default,
			'tris': build.tri_count(obj),
		})

	export.report(objects[1:], label="파츠 전체")

	print("\n[휴머노이드] 애니메이션")
	clips = anim_humanoid.build_all(arm)

	for c in clips:
		print("    %-12s %5.2fs  %s" % (c['name'], c['duration'],
		                                "loop" if c['loop'] else ""))

	# ── 내보내기 ──
	gltf = os.path.join(ROOT, 'export', 'characters', 'humanoid.gltf')
	export.write(gltf, objects)
	export.save_blend(os.path.join(ROOT, 'blend', 'humanoid.blend'))

	# ── 조립 메타데이터 ──
	slots = []

	for slot in ('head', 'face', 'hair', 'torso', 'arms', 'hands',
	             'legs', 'feet', 'cloak', 'belt', 'headgear', 'backpack'):
		slots.append({
			'name': slot,
			'required': slot in ('head', 'face', 'torso', 'arms', 'hands', 'legs', 'feet'),
			'parts': [p['name'] for p in meta_parts if p['slot'] == slot],
		})

	data = {
		'_comment': 'gen_humanoid.py 가 생성. 손으로 고치지 말 것 — 다시 돌리면 덮어쓴다.',
		'gltf': 'export/characters/humanoid.gltf',
		'armature': 'rig_humanoid',
		'height_m': rig.T,
		'heads': 7,
		'slots': slots,
		'parts': meta_parts,
		'sockets': ['sk_hand_L', 'sk_hand_R', 'sk_back', 'sk_hip_L', 'sk_hip_R', 'sk_head_top'],
		'color_slots': {
			'skin': [list(c) for c in palette.SKIN_TONES],
			'hair': [list(c) for c in palette.HAIR_TONES],
			'cloth_main': [list(c) for c in palette.CLOAK_TONES],
		},
		'presets': [
			{
				'id': pid,
				'label': label,
				'parts': parts,
				'color': {'cloth_main': ci, 'skin': si, 'hair': hi},
			}
			for pid, label, parts, ci, si, hi in PRESETS
		],
		'clips': clips,
	}

	out = os.path.join(ROOT, 'data', 'character_parts.json')
	export.ensure_dir(out)

	with open(out, 'w', encoding='utf-8') as f:
		json.dump(data, f, ensure_ascii=False, indent='\t')

	print("\n  → %s" % gltf)
	print("  → %s" % out)

	return clips


if __name__ == '__main__':
	main()
