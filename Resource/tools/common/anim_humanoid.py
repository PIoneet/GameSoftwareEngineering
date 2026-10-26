# -*- coding: utf-8 -*-
"""휴머노이드 애니메이션 클립.

회전 부호를 매번 헷갈리지 않도록 한 번만 정리해 둔다.
캐릭터는 **-Y 를 바라보고** Z 가 위다.

    위를 향한 본(척추)      rx > 0  →  앞으로 숙인다
    아래를 향한 본(팔·다리) rx > 0  →  끝이 뒤로 간다
    앞을 향한 본(발)        rx > 0  →  발끝이 내려간다
    모든 본                 rz > 0  →  왼쪽으로 돈다 (캐릭터 기준 왼쪽 = +X)

보행 클립은 전부 **왼발 접지를 위상 0.0 에** 맞춘다. walk 와 run 의 길이가 달라도
정규화 위상을 공유하면 발이 맞는다 (FORMAT.md 7-3).
"""

import math

from . import anim
from .anim import deg, ease, ease_out, frames


# ────────────────────────────────────────────────────────────
#  보행 설정
# ────────────────────────────────────────────────────────────

WALK = {
	'duration':    1.00,
	'speed':       1.35,
	'thigh_swing': 24.0,
	'knee_min':     6.0,
	'knee_bend':   42.0,
	'foot_amp':    16.0,
	'toe_push':    20.0,
	'arm_swing':   17.0,
	'elbow_base':  14.0,
	'elbow_swing': 10.0,
	'lean':         3.0,
	'bob':         0.022,
	'hip_yaw':      4.0,
	'hip_roll':     2.5,
	'chest_yaw':    5.0,
	'samples':     16,
}

RUN = {
	'duration':    0.60,
	'speed':       4.20,
	'thigh_swing': 38.0,
	'knee_min':    16.0,
	'knee_bend':   78.0,
	'foot_amp':    22.0,
	'toe_push':    30.0,
	'arm_swing':   38.0,
	'elbow_base':  78.0,
	'elbow_swing': 18.0,
	'lean':        13.0,
	'bob':         0.052,
	'hip_yaw':      7.0,
	'hip_roll':     4.0,
	'chest_yaw':    9.0,
	'samples':     12,
}


def _locomotion_pose(a, p, cfg):
	"""위상 p(0~1) 에서의 보행 포즈 한 장."""
	tau = math.tau

	# ── 몸통 ──
	bob = -cfg['bob'] * math.cos(tau * 2.0 * p) * 0.5
	a.loc('hips', 0.0, 0.0, bob)

	a.rot('hips', 0.0, cfg['hip_roll'] * math.sin(tau * p), -cfg['hip_yaw'] * math.cos(tau * p))
	a.rot('spine_01', cfg['lean'] * 0.4, 0.0, 0.0)
	a.rot('spine_02', cfg['lean'] * 0.3, 0.0, 0.0)
	a.rot('chest', cfg['lean'] * 0.3, 0.0, cfg['chest_yaw'] * math.cos(tau * p))

	# 시선은 수평으로 유지한다 — 몸이 숙은 만큼 목을 세운다
	a.rot('neck', -cfg['lean'] * 0.5, 0.0, 0.0)
	a.rot('head', -cfg['lean'] * 0.4, 0.0, -cfg['chest_yaw'] * 0.3 * math.cos(tau * p))

	for side, off in (('L', 0.0), ('R', 0.5)):
		ps = (p + off) % 1.0

		# ── 다리 ──
		thigh = -cfg['thigh_swing'] * math.cos(tau * ps)
		knee = cfg['knee_min'] + cfg['knee_bend'] * max(0.0, math.sin(tau * (ps - 0.40)))
		foot = cfg['foot_amp'] * math.sin(tau * (ps - 0.25))
		toe = cfg['toe_push'] * max(0.0, math.sin(tau * (ps - 0.375))) ** 2

		a.rot('thigh_' + side, thigh, 0.0, 0.0)
		a.rot('shin_' + side, knee, 0.0, 0.0)

		# 발목은 허벅지·무릎이 만든 기울기를 어느 정도 상쇄해 발바닥을 땅과 나란히 둔다
		a.rot('foot_' + side, foot - (thigh + knee) * 0.25, 0.0, 0.0)
		a.rot('toe_' + side, toe, 0.0, 0.0)

		# ── 팔 — 같은 쪽 다리와 반대로 ──
		arm = cfg['arm_swing'] * math.cos(tau * ps)
		elbow = cfg['elbow_base'] + cfg['elbow_swing'] * (0.5 - 0.5 * math.cos(tau * ps))

		a.rot('shoulder_' + side, 0.0, 0.0, 0.0)
		a.rot('upperarm_' + side, arm, 0.0, 0.0)
		a.rot('lowerarm_' + side, -elbow, 0.0, 0.0)
		a.rot('hand_' + side, 0.0, 0.0, 0.0)


def _bake_locomotion(a, name, cfg):
	n = cfg['samples']
	total = frames(cfg['duration'])

	a.begin(name)

	for i in range(n + 1):
		p = i / n

		_locomotion_pose(a, p % 1.0, cfg)
		a.key(total * p)

	return a.end(cfg['duration'], loop=True,
	             speed_mps=cfg['speed'],
	             stride_m=round(cfg['speed'] * cfg['duration'], 3),
	             contact_phase={'foot_L': 0.0, 'foot_R': 0.5})


# ────────────────────────────────────────────────────────────
#  클립들
# ────────────────────────────────────────────────────────────

def clip_base(a):
	"""바인드 포즈 한 장. 모든 보간의 기준이다 (FORMAT.md 7-2)."""
	a.begin('base')
	a.rest_pose()

	# 같은 포즈를 두 프레임에 박는다. 키가 하나뿐이면 내보내기가 보간할 것이 없다고
	# 판단해 STEP 으로 써 버리고, 그러면 "보간은 LINEAR 뿐" (FORMAT.md 1절) 이 깨진다.
	a.key_all(0)
	a.key_all(1)

	return a.end(1.0 / anim.FPS, loop=False, reference=True)


def clip_idle(a):
	"""숨만 쉰다. 2.4초 — 너무 짧으면 반복이 눈에 띈다."""
	dur = 2.40
	total = frames(dur)
	n = 12

	a.begin('idle')

	for i in range(n + 1):
		p = (i / n) % 1.0
		s = math.sin(math.tau * p)

		a.loc('hips', 0.0, 0.0, 0.006 * s)
		a.rot('spine_01', 1.2 * s, 0.0, 0.0)
		a.rot('spine_02', 1.0 * s, 0.0, 0.0)
		a.rot('chest', -1.6 * s, 0.0, 0.0)
		a.rot('neck', 0.8 * s, 0.0, 0.0)

		# 고개를 아주 천천히 좌우로. 주기를 몸통과 다르게 둬야 기계적으로 안 보인다
		a.rot('head', -0.6 * s, 0.0, 2.2 * math.sin(math.tau * p * 0.5))

		for side in ('L', 'R'):
			sgn = 1.0 if side == 'L' else -1.0

			a.rot('upperarm_' + side, 1.4 * s, 0.0, 1.0 * sgn * s)
			a.rot('lowerarm_' + side, -(10.0 + 2.0 * s), 0.0, 0.0)

		a.key(total * p if i < n else total)

	return a.end(dur, loop=True, speed_mps=0.0)


def clip_walk(a):
	return _bake_locomotion(a, 'walk', WALK)


def clip_run(a):
	return _bake_locomotion(a, 'run', RUN)


def clip_turn(a, side):
	"""제자리 회전 — 몸통 비틀기 + 발 바꿔 딛기.

	**실제 방향 전환은 게임이 한다.** 이 클립은 몸통을 25도까지만 비튼다.
	클립 자체가 90도를 돌아 버리면 게임의 yaw 보간과 겹쳐 두 배로 돌아간다.

	시작과 끝이 모두 base 포즈라 (FORMAT.md 7-2) 회전 도중 끊겨도 포즈가 튀지 않는다.
	"""
	dur = 0.50
	total = frames(dur)
	sgn = 1.0 if side == 'L' else -1.0
	name = 'turn_' + side

	# 도는 쪽 발이 축이 되고 반대쪽 발이 끌려온다
	pivot = side
	step = 'R' if side == 'L' else 'L'

	a.begin(name)
	a.rest_pose()
	a.key_all(0)

	keys = [
		# (정규화 시간, 몸통비틀기, 들린발 높이계수, 무게중심 이동)
		(0.00, 0.0,  0.0, 0.0),
		(0.30, 0.62, 1.0, 0.6),
		(0.62, 1.00, 0.7, 1.0),
		(1.00, 0.0,  0.0, 0.0),
	]

	for t, twist, lift, shift in keys:
		yaw = 25.0 * twist * sgn

		a.loc('hips', 0.020 * shift * sgn, 0.0, -0.014 * lift)
		a.rot('hips', 0.0, -2.5 * shift * sgn, yaw * 0.55)
		a.rot('spine_01', 1.5 * lift, 0.0, yaw * 0.18)
		a.rot('spine_02', 1.0 * lift, 0.0, yaw * 0.16)
		a.rot('chest', 0.0, 0.0, yaw * 0.26)
		a.rot('head', 0.0, 0.0, yaw * 0.40)

		# 축이 되는 발 — 거의 그대로, 무릎만 살짝
		a.rot('thigh_' + pivot, -3.0 * lift, 0.0, yaw * 0.25)
		a.rot('shin_' + pivot, 7.0 * lift, 0.0, 0.0)
		a.rot('foot_' + pivot, -2.0 * lift, 0.0, 0.0)

		# 끌려오는 발 — 들었다가 새 자리에 놓는다
		a.rot('thigh_' + step, -16.0 * lift, 6.0 * lift * sgn, yaw * 0.70)
		a.rot('shin_' + step, 34.0 * lift, 0.0, 0.0)
		a.rot('foot_' + step, -12.0 * lift, 0.0, 0.0)

		for s2 in ('L', 'R'):
			a.rot('upperarm_' + s2, 6.0 * lift, 0.0, 0.0)
			a.rot('lowerarm_' + s2, -(12.0 + 6.0 * lift), 0.0, 0.0)

		a.key(total * t)

	return a.end(dur, loop=False, yaw_deg=90.0 * sgn, note='게임이 실제 yaw 를 보간한다')


def clip_attack(a):
	"""오른손 내려베기.

	타격 순간을 events 로 남긴다 — 로더가 그 시각에 판정을 내면 된다.
	모션과 판정을 따로 재는 대신 **클립이 타이밍을 들고 있게** 하는 쪽이
	나중에 모션을 손봐도 어긋나지 않는다.
	"""
	dur = 0.70
	total = frames(dur)

	a.begin('attack_01')
	a.rest_pose()
	a.key_all(0)

	#      t     몸통yaw  오른팔rx  오른팔꿈치  왼팔rx  숙임   무게
	keys = [
		(0.00,    0.0,     0.0,      -14.0,     0.0,    0.0,  0.0),
		(0.14,  -14.0,    78.0,      -72.0,   -16.0,   -5.0, -0.3),   # 당기기
		(0.32,  -24.0,   142.0,      -96.0,   -26.0,   -9.0, -0.5),   # 최대 준비
		(0.44,   16.0,   -34.0,      -22.0,    22.0,   14.0,  0.7),   # 타격
		(0.56,   22.0,   -52.0,      -34.0,    30.0,   17.0,  0.9),   # 따라나감
		(0.76,    8.0,   -14.0,      -30.0,    12.0,    7.0,  0.4),
		(1.00,    0.0,     0.0,      -14.0,     0.0,    0.0,  0.0),
	]

	for t, yaw, arm, elbow, larm, lean, weight in keys:
		a.loc('hips', 0.0, -0.035 * weight, -0.012 * abs(weight))
		a.rot('hips', lean * 0.3, 0.0, yaw * 0.35)
		a.rot('spine_01', lean * 0.3, 0.0, yaw * 0.20)
		a.rot('spine_02', lean * 0.3, 0.0, yaw * 0.20)
		a.rot('chest', lean * 0.4, 0.0, yaw * 0.45)
		a.rot('neck', -lean * 0.3, 0.0, -yaw * 0.15)
		a.rot('head', -lean * 0.3, 0.0, -yaw * 0.25)

		a.rot('shoulder_R', 0.0, 0.0, -yaw * 0.25)
		a.rot('upperarm_R', arm, -12.0 if arm > 40 else 0.0, 0.0)
		a.rot('lowerarm_R', elbow, 0.0, 0.0)
		a.rot('hand_R', 0.0, 0.0, 0.0)

		a.rot('shoulder_L', 0.0, 0.0, -yaw * 0.15)
		a.rot('upperarm_L', larm, 0.0, 0.0)
		a.rot('lowerarm_L', -(26.0 + abs(larm) * 0.5), 0.0, 0.0)

		# 발은 앞뒤로 벌려 버틴다
		a.rot('thigh_R', -10.0 * weight - 6.0, 0.0, 0.0)
		a.rot('shin_R', 12.0 + 10.0 * abs(weight), 0.0, 0.0)
		a.rot('thigh_L', 8.0 * weight + 4.0, 0.0, 0.0)
		a.rot('shin_L', 8.0 + 6.0 * abs(weight), 0.0, 0.0)

		a.key(total * t)

	return a.end(dur, loop=False,
	             events=[
	                 {'name': 'hit',    'time': round(dur * 0.44, 3)},
	                 {'name': 'recover', 'time': round(dur * 0.62, 3)},
	             ],
	             cancel_after=round(dur * 0.62, 3))


def clip_jump_start(a):
	"""웅크렸다 뜬다. 짧게 — 길면 조작이 굼떠 보인다."""
	dur = 0.25
	total = frames(dur)

	a.begin('jump_start')
	a.rest_pose()
	a.key_all(0)

	#      t    웅크림  팔
	keys = [
		(0.00, 0.00,   0.0),
		(0.55, 1.00,  34.0),   # 최대 웅크림. 팔은 뒤로
		(1.00, 0.15, -48.0),   # 펴면서 팔을 앞·위로 던진다
	]

	for t, crouch, arm in keys:
		a.loc('hips', 0.0, 0.0, -0.165 * crouch)
		a.rot('hips', 6.0 * crouch, 0.0, 0.0)
		a.rot('spine_01', 7.0 * crouch, 0.0, 0.0)
		a.rot('spine_02', 6.0 * crouch, 0.0, 0.0)
		a.rot('chest', 5.0 * crouch, 0.0, 0.0)
		a.rot('neck', -8.0 * crouch, 0.0, 0.0)
		a.rot('head', -6.0 * crouch, 0.0, 0.0)

		for side in ('L', 'R'):
			a.rot('thigh_' + side, -46.0 * crouch, 0.0, 0.0)
			a.rot('shin_' + side, 76.0 * crouch, 0.0, 0.0)
			a.rot('foot_' + side, -34.0 * crouch, 0.0, 0.0)
			a.rot('toe_' + side, 10.0 * crouch, 0.0, 0.0)
			a.rot('upperarm_' + side, arm, 0.0, 0.0)
			a.rot('lowerarm_' + side, -(18.0 + 20.0 * crouch), 0.0, 0.0)

		a.key(total * t)

	return a.end(dur, loop=False, events=[{'name': 'liftoff', 'time': round(dur * 0.85, 3)}])


def clip_jump_air(a):
	"""체공 루프. 게임이 체공 시간을 정하므로 길이에 의존하지 않게 루프로 둔다."""
	dur = 0.60
	total = frames(dur)
	n = 6

	a.begin('jump_air')

	for i in range(n + 1):
		p = (i / n) % 1.0
		s = math.sin(math.tau * p)

		a.loc('hips', 0.0, 0.0, 0.0)
		a.rot('hips', -4.0 + 2.0 * s, 0.0, 0.0)
		a.rot('spine_01', 5.0 + 1.5 * s, 0.0, 0.0)
		a.rot('chest', 4.0, 0.0, 2.0 * s)
		a.rot('head', -6.0, 0.0, 0.0)

		# 앞다리는 접고 뒷다리는 뻗는다 — 가만히 떠 있는 것보다 훨씬 산다
		a.rot('thigh_L', -34.0 - 4.0 * s, 0.0, 0.0)
		a.rot('shin_L', 58.0 + 6.0 * s, 0.0, 0.0)
		a.rot('foot_L', -16.0, 0.0, 0.0)

		a.rot('thigh_R', 14.0 + 4.0 * s, 0.0, 0.0)
		a.rot('shin_R', 22.0 - 6.0 * s, 0.0, 0.0)
		a.rot('foot_R', 14.0, 0.0, 0.0)

		for side in ('L', 'R'):
			sgn = 1.0 if side == 'L' else -1.0

			a.rot('upperarm_' + side, -38.0 + 5.0 * s, 0.0, 14.0 * sgn)
			a.rot('lowerarm_' + side, -(44.0 + 8.0 * s), 0.0, 0.0)

		a.key(total * p if i < n else total)

	return a.end(dur, loop=True)


def clip_jump_land(a):
	"""착지 — 충격을 받고 일어선다. 끝은 base 포즈라 바로 다른 상태로 넘어갈 수 있다."""
	dur = 0.35
	total = frames(dur)

	a.begin('jump_land')
	a.rest_pose()

	keys = [
		(0.00, 0.20),
		(0.26, 1.00),   # 충격 흡수가 가장 깊은 지점
		(0.60, 0.42),
		(1.00, 0.00),
	]

	for t, crouch in keys:
		a.loc('hips', 0.0, 0.0, -0.185 * crouch)
		a.rot('hips', 8.0 * crouch, 0.0, 0.0)
		a.rot('spine_01', 9.0 * crouch, 0.0, 0.0)
		a.rot('spine_02', 7.0 * crouch, 0.0, 0.0)
		a.rot('chest', 6.0 * crouch, 0.0, 0.0)
		a.rot('neck', -10.0 * crouch, 0.0, 0.0)
		a.rot('head', -7.0 * crouch, 0.0, 0.0)

		for side in ('L', 'R'):
			sgn = 1.0 if side == 'L' else -1.0

			a.rot('thigh_' + side, -52.0 * crouch, 4.0 * crouch * sgn, 0.0)
			a.rot('shin_' + side, 84.0 * crouch, 0.0, 0.0)
			a.rot('foot_' + side, -38.0 * crouch, 0.0, 0.0)
			a.rot('upperarm_' + side, -26.0 * crouch, 0.0, 10.0 * crouch * sgn)
			a.rot('lowerarm_' + side, -(14.0 + 40.0 * crouch), 0.0, 0.0)

		a.key(total * t)

	return a.end(dur, loop=False, events=[{'name': 'footstep', 'time': 0.0}])


# ────────────────────────────────────────────────────────────

def build_all(arm_obj):
	"""휴머노이드 클립 전체. 메타데이터 목록을 돌려준다."""
	a = anim.Animator(arm_obj)

	clip_base(a)
	clip_idle(a)
	clip_walk(a)
	clip_run(a)
	clip_turn(a, 'L')
	clip_turn(a, 'R')
	clip_attack(a)
	clip_jump_start(a)
	clip_jump_air(a)
	clip_jump_land(a)

	anim.finish(arm_obj)

	return a.clips
