# -*- coding: utf-8 -*-
"""4족 애니메이션 클립.

## 걸음걸이(gait)를 발 위상으로 정의한다

4족은 발이 넷이라 "언제 어느 발이 땅에 닿느냐" 가 곧 걸음걸이다.
다리마다 위상 오프셋만 다르게 주면 같은 코드로 전혀 다른 걸음이 나온다.

아래 표는 **발이 땅에 닿는 시각**이다 (클립 길이로 정규화한 0~1).
클립을 만들 때 쓰는 위상 오프셋은 그 역수(`1 - 접지시각`)인데, 둘을 섞어 적다가
보행 순서가 뒤집히기 쉬워 **표에는 접지 시각만** 적고 오프셋은 코드가 계산한다.

    walk  (측대 4박자)  FL 0.00 · HR 0.25 · FR 0.50 · HL 0.75
                        순서는 LF → RH → RF → LH. **같은 쪽 뒷발이 앞발보다 먼저** 나간다
                        (LH→LF→RH→RF 를 LF 부터 끊어 적은 것). 개·고양이·사슴이 전부 이렇다.
                        앞발이 먼저 나가는 대각 순서는 영장류 걸음이라 짐승으로 안 보인다.
                        항상 세 발이 땅에 있어 안정적이다.

    trot  (대각 2박자)  FL 0.00 · HR 0.00 · FR 0.50 · HL 0.50
                        대각선 두 발이 동시에. walk 와 run 사이를 잇는다.

    run   (횡단 갤럽)   HL 0.00 · HR 0.12 · FL 0.42 · FR 0.54
                        뒷발 두 짝이 거의 같이 땅을 밀고, 한 박자 뒤에 앞발 두 짝이 받는다.
                        그 사이에 네 발이 다 뜨는 구간이 생긴다.

walk 와 trot 은 **FL 접지를 0.0 에** 맞춰 두어 위상을 공유한 채 섞을 수 있다.
갤럽은 구조 자체가 달라 위상 동기가 의미 없다 — trot → run 전이는 크로스페이드로 간다
(data/anim_graph.json 의 `sync` 필드 참고).

## 갤럽은 척추가 만든다

다리만 움직이면 빨리 걷는 것처럼 보일 뿐 달리는 것으로 안 보인다.
갤럽의 인상은 **등이 굽었다 펴지는 것**에서 온다 — 몸이 활처럼 접혔다가 튕긴다.
그래서 run 에서는 척추 rx 진폭을 크게 준다 (`spine_flex`).
"""

import math

from . import anim
from .anim import frames


HIND = ('thigh', 'shin', 'hock', 'metatarsus', 'hindtoe')
FRONT = ('scapula', 'upperarm', 'lowerarm', 'frontfoot', 'fronttoe')

# 발이 땅에 닿는 시각 (0~1). 위 설명의 표 그대로다.
GAIT_CONTACT = {
	'walk': {'FL': 0.00, 'HR': 0.25, 'FR': 0.50, 'HL': 0.75},
	'trot': {'FL': 0.00, 'HR': 0.00, 'FR': 0.50, 'HL': 0.50},
	'run':  {'HL': 0.00, 'HR': 0.12, 'FL': 0.42, 'FR': 0.54},
}


def gait_offsets(gait):
	"""접지 시각 → 위상 오프셋. 다리 내부 위상이 0 일 때 발이 닿도록 민다."""
	return {leg: (1.0 - t) % 1.0 for leg, t in GAIT_CONTACT[gait].items()}


# ────────────────────────────────────────────────────────────
#  과별 움직임 성향
# ────────────────────────────────────────────────────────────
#
# 뼈 길이는 rig_quadruped 가, 살은 gen_quadruped 가, **움직임의 성격은 여기가** 정한다.

STYLE = {
	'feline': {
		'spine_lateral': 4.5,    # 좌우로 흐느적거린다
		'spine_flex':   15.0,    # 갤럽 때 등이 크게 굽는다
		'tail_amp':     14.0,    # 꼬리가 길어 크게 흔들린다
		'head_bob':      2.5,
		'step_height':   1.00,
		'shoulder_roll': 6.0,    # 어깨뼈가 솟았다 내렸다 한다
	},
	'canine': {
		'spine_lateral': 2.5,
		'spine_flex':   11.0,
		'tail_amp':     10.0,
		'head_bob':      3.5,
		'step_height':   1.00,
		'shoulder_roll': 4.5,
	},
	'cervid': {
		'spine_lateral': 1.2,    # 등이 뻣뻣하다
		'spine_flex':    6.0,
		'tail_amp':       4.0,   # 꼬리가 짧다
		'head_bob':       5.0,   # 목이 길어 머리가 크게 끄덕인다
		'step_height':    1.25,  # 다리를 높이 든다
		'shoulder_roll':  2.5,
	},
}

GAITS = {
	# 이름    길이   속도(어깨높이 1.0 기준 m/s)  보폭진폭  무릎굽힘  듀티
	'walk': {'duration': 1.00, 'speed': 1.10, 'swing': 20.0, 'flex': 26.0, 'duty': 0.62},
	'trot': {'duration': 0.62, 'speed': 2.60, 'swing': 28.0, 'flex': 40.0, 'duty': 0.45},
	'run':  {'duration': 0.44, 'speed': 6.00, 'swing': 40.0, 'flex': 64.0, 'duty': 0.32},
}


# ────────────────────────────────────────────────────────────
#  보행 포즈
# ────────────────────────────────────────────────────────────

def _hind_leg(a, side, pl, cfg, style):
	"""뒷다리 5단. 위상 pl 에서 접지(0.0) → 디딤 → 떼기 → 흔들기."""
	tau = math.tau
	sw = cfg['swing']
	fx = cfg['flex']
	lift = style['step_height']

	swing = max(0.0, math.sin(tau * (pl - cfg['duty'] * 0.5 - 0.25)))

	a.rot('thigh_' + side, -sw * math.cos(tau * pl), 0.0, 0.0)

	# 무릎(stifle)은 흔들 때 접힌다
	a.rot('shin_' + side, fx * 0.55 * swing * lift + fx * 0.18, 0.0, 0.0)

	# 비절(hock) — 무릎과 **반대로** 움직인다. 이 반대 위상이 뒷다리의 스프링이다.
	a.rot('hock_' + side, -(fx * 0.70 * swing * lift + fx * 0.22), 0.0, 0.0)

	# 중족골은 발바닥(발굽)을 땅과 나란히 유지하는 보정 역할
	a.rot('metatarsus_' + side, fx * 0.45 * swing * lift + sw * 0.30 * math.cos(tau * pl), 0.0, 0.0)

	# 발끝 — 디딤 끝에서 차고 나간다
	push = max(0.0, math.sin(tau * (pl - cfg['duty'] * 0.5))) ** 3

	a.rot('hindtoe_' + side, 22.0 * push, 0.0, 0.0)


def _front_leg(a, side, pl, cfg, style):
	"""앞다리 4단 + 어깨뼈."""
	tau = math.tau
	sw = cfg['swing']
	fx = cfg['flex']
	lift = style['step_height']

	swing = max(0.0, math.sin(tau * (pl - cfg['duty'] * 0.5 - 0.25)))

	a.rot('scapula_' + side, style['shoulder_roll'] * math.cos(tau * pl), 0.0, 0.0)
	a.rot('upperarm_' + side, -sw * 0.95 * math.cos(tau * pl), 0.0, 0.0)
	a.rot('lowerarm_' + side, fx * 0.60 * swing * lift + fx * 0.14, 0.0, 0.0)
	a.rot('frontfoot_' + side, -(fx * 0.40 * swing * lift) + sw * 0.25 * math.cos(tau * pl), 0.0, 0.0)

	push = max(0.0, math.sin(tau * (pl - cfg['duty'] * 0.5))) ** 3

	a.rot('fronttoe_' + side, 16.0 * push, 0.0, 0.0)


def _spine(a, p, gait, cfg, style):
	"""척추·목·꼬리.

	walk/trot 은 좌우 흔들림(rz)이, 갤럽은 상하 굽힘(rx)이 지배한다.
	"""
	tau = math.tau

	lateral = style['spine_lateral'] * math.sin(tau * p)
	flex = style['spine_flex'] * math.sin(tau * p * (1.0 if gait == 'run' else 2.0))

	if gait != 'run':
		flex *= 0.22

	a.rot('hips', flex * 0.25, 0.0, -lateral * 0.5)
	a.rot('spine_03', flex * 0.40, 0.0, lateral * 0.30)
	a.rot('spine_02', flex * 0.40, 0.0, lateral * 0.45)
	a.rot('spine_01', flex * 0.30, 0.0, lateral * 0.45)
	a.rot('chest', -flex * 0.25, 0.0, lateral * 0.30)

	bob = style['head_bob']

	a.rot('neck_01', -flex * 0.30 + bob * math.sin(tau * p * 2.0), 0.0, -lateral * 0.5)
	a.rot('neck_02', -flex * 0.20 + bob * 0.6 * math.sin(tau * p * 2.0 - 0.6), 0.0, -lateral * 0.4)
	a.rot('head', bob * 0.5 * math.sin(tau * p * 2.0 - 1.2), 0.0, -lateral * 0.3)

	# 꼬리는 몸통보다 늦게 따라온다 — 위상을 밀어 두면 그 지연이 생긴다
	t_amp = style['tail_amp']

	a.rot('tail_01', -flex * 0.30, 0.0, t_amp * 0.6 * math.sin(tau * (p - 0.12)))
	a.rot('tail_02', -flex * 0.20, 0.0, t_amp * 0.9 * math.sin(tau * (p - 0.24)))
	a.rot('tail_03', -flex * 0.10, 0.0, t_amp * 1.1 * math.sin(tau * (p - 0.36)))

	# 갤럽은 몸 전체가 떴다 가라앉는다
	if gait == 'run':
		a.loc('hips', 0.0, 0.0, 0.045 * max(0.0, math.sin(tau * p + 0.9)))
	else:
		a.loc('hips', 0.0, 0.0, 0.010 * math.sin(tau * p * 2.0))


def _bake_gait(a, family, gait):
	cfg = GAITS[gait]
	style = STYLE[family]
	phase = gait_offsets(gait)
	total = frames(cfg['duration'])
	n = 16 if gait == 'walk' else 12

	a.begin(gait)

	for i in range(n + 1):
		p = (i / n) % 1.0

		_spine(a, p, gait, cfg, style)

		_front_leg(a, 'L', (p + phase['FL']) % 1.0, cfg, style)
		_front_leg(a, 'R', (p + phase['FR']) % 1.0, cfg, style)
		_hind_leg(a, 'L', (p + phase['HL']) % 1.0, cfg, style)
		_hind_leg(a, 'R', (p + phase['HR']) % 1.0, cfg, style)

		a.key(total * (i / n))

	contacts = {'foot_' + leg: round(t, 3)
	            for leg, t in GAIT_CONTACT[gait].items()}

	return a.end(cfg['duration'], loop=True,
	             speed_mps=cfg['speed'],
	             duty_factor=cfg['duty'],
	             contact_phase=contacts)


# ────────────────────────────────────────────────────────────
#  그 외 클립
# ────────────────────────────────────────────────────────────

def clip_base(a):
	a.begin('base')
	a.rest_pose()

	# 같은 포즈를 두 프레임에 박는다. 키가 하나뿐이면 내보내기가 보간할 것이 없다고
	# 판단해 STEP 으로 써 버리고, 그러면 "보간은 LINEAR 뿐" (FORMAT.md 1절) 이 깨진다.
	a.key_all(0)
	a.key_all(1)

	return a.end(1.0 / anim.FPS, loop=False, reference=True)


def clip_idle(a, family):
	"""숨 + 귀·꼬리 잔동작. 과마다 성격이 다르다."""
	style = STYLE[family]
	dur = 2.80
	total = frames(dur)
	n = 14

	a.begin('idle')

	for i in range(n + 1):
		p = (i / n) % 1.0
		s = math.sin(math.tau * p)
		s2 = math.sin(math.tau * p * 0.5)

		a.loc('hips', 0.0, 0.0, 0.004 * s)
		a.rot('spine_02', 1.0 * s, 0.0, 0.0)
		a.rot('chest', 1.4 * s, 0.0, 0.0)

		# 초식동물은 고개를 자주 든다 — 경계하는 버릇
		if family == 'cervid':
			a.rot('neck_01', -6.0 * max(0.0, s2), 0.0, 0.0)
			a.rot('neck_02', -4.0 * max(0.0, s2), 0.0, 0.0)
			a.rot('head', 3.0 * max(0.0, s2), 0.0, 4.0 * s)
		else:
			a.rot('neck_01', 1.2 * s, 0.0, 2.0 * s2)
			a.rot('neck_02', 0.8 * s, 0.0, 1.5 * s2)
			a.rot('head', -0.8 * s, 0.0, 3.0 * s2)

		a.rot('tail_01', 0.0, 0.0, style['tail_amp'] * 0.25 * s2)
		a.rot('tail_02', 0.0, 0.0, style['tail_amp'] * 0.35 * math.sin(math.tau * (p * 0.5 - 0.1)))
		a.rot('tail_03', 0.0, 0.0, style['tail_amp'] * 0.45 * math.sin(math.tau * (p * 0.5 - 0.2)))

		a.key(total * (i / n))

	return a.end(dur, loop=True, speed_mps=0.0)


def clip_walk(a, family):
	return _bake_gait(a, family, 'walk')


def clip_trot(a, family):
	return _bake_gait(a, family, 'trot')


def clip_run(a, family):
	return _bake_gait(a, family, 'run')


def clip_turn(a, family, side):
	"""제자리 선회. 휴머노이드와 같은 규약 — 몸통만 틀고 실제 yaw 는 게임이 돌린다.

	4족은 몸이 길어 돌 때 **몸통이 휜다.** 그 휘어짐이 곧 선회의 인상이다.
	"""
	style = STYLE[family]
	dur = 0.60
	total = frames(dur)
	sgn = 1.0 if side == 'L' else -1.0

	a.begin('turn_' + side)
	a.rest_pose()
	a.key_all(0)

	keys = [
		(0.00, 0.00, 0.0),
		(0.32, 0.70, 1.0),
		(0.64, 1.00, 0.6),
		(1.00, 0.00, 0.0),
	]

	for t, bend, lift in keys:
		# 몸통 전체가 바나나처럼 휜다
		arc = 20.0 * bend * sgn

		a.rot('hips', 0.0, -3.0 * bend * sgn, arc * 0.30)
		a.rot('spine_03', 0.0, -2.0 * bend * sgn, arc * 0.35)
		a.rot('spine_02', 0.0, 0.0, arc * 0.40)
		a.rot('spine_01', 0.0, 2.0 * bend * sgn, arc * 0.40)
		a.rot('chest', 0.0, 3.0 * bend * sgn, arc * 0.35)
		a.rot('neck_01', 0.0, 0.0, arc * 0.50)
		a.rot('neck_02', 0.0, 0.0, arc * 0.45)
		a.rot('head', 0.0, 0.0, arc * 0.40)

		a.rot('tail_01', 0.0, 0.0, -style['tail_amp'] * 0.5 * bend * sgn)
		a.rot('tail_02', 0.0, 0.0, -style['tail_amp'] * 0.7 * bend * sgn)

		# 안쪽 발은 제자리에서 짧게, 바깥쪽 발은 크게 돈다
		inner = side
		outer = 'R' if side == 'L' else 'L'

		a.rot('upperarm_' + inner, -8.0 * lift, 0.0, arc * 0.4)
		a.rot('lowerarm_' + inner, 16.0 * lift, 0.0, 0.0)
		a.rot('upperarm_' + outer, -20.0 * lift, 0.0, arc * 0.6)
		a.rot('lowerarm_' + outer, 30.0 * lift, 0.0, 0.0)

		a.rot('thigh_' + inner, -6.0 * lift, 0.0, arc * 0.3)
		a.rot('shin_' + inner, 12.0 * lift, 0.0, 0.0)
		a.rot('hock_' + inner, -14.0 * lift, 0.0, 0.0)
		a.rot('thigh_' + outer, -16.0 * lift, 0.0, arc * 0.5)
		a.rot('shin_' + outer, 24.0 * lift, 0.0, 0.0)
		a.rot('hock_' + outer, -28.0 * lift, 0.0, 0.0)

		a.key(total * t)

	return a.end(dur, loop=False, yaw_deg=90.0 * sgn, note='게임이 실제 yaw 를 보간한다')


def clip_attack(a, family):
	"""공격 — 과마다 방식이 다르다.

	던전밥식 "종별 공략법" 을 염두에 두면, 공격 모션이 곧 그 짐승을 읽는 단서다.
	모션이 다르면 피하는 법도 달라야 한다.

	  고양이과 — 앞발 후려치기. 상체를 세웠다가 내리친다. 좌우로 피한다
	  개  과 — 돌진해 물기. 몸을 낮추고 직선으로 들어온다. 뒤로 빠지면 안 된다
	  사슴과 — 머리로 받기. 고개를 들었다가 아래로 찍는다. 옆으로 돌면 따라오지 못한다
	"""
	dur = 0.80
	total = frames(dur)

	a.begin('attack_01')
	a.rest_pose()
	a.key_all(0)

	if family == 'feline':
		#       t    상체세움  앞발       목    입
		keys = [
			(0.00, 0.00,   0.0,    0.0,  0.0),
			(0.22, 0.85, -60.0,  -14.0, 12.0),
			(0.40, 1.00, -95.0,  -20.0, 26.0),   # 최대로 치켜든다
			(0.52, 0.30,  55.0,   16.0, 30.0),   # 내리친다
			(0.66, 0.10,  22.0,    8.0, 10.0),
			(1.00, 0.00,   0.0,    0.0,  0.0),
		]
		hit_t = 0.52

		for t, rear, paw, neck, jaw in keys:
			a.rot('hips', -18.0 * rear, 0.0, 0.0)
			a.rot('spine_03', -14.0 * rear, 0.0, 0.0)
			a.rot('spine_02', -12.0 * rear, 0.0, 0.0)
			a.rot('spine_01', -10.0 * rear, 0.0, 0.0)
			a.rot('chest', -8.0 * rear, 0.0, 0.0)
			a.rot('neck_01', neck, 0.0, 0.0)
			a.rot('neck_02', neck * 0.6, 0.0, 0.0)
			a.rot('head', neck * 0.4, 0.0, 0.0)
			a.rot('jaw', jaw, 0.0, 0.0)

			# 오른 앞발이 주공격, 왼발은 늦게 따라온다
			a.rot('scapula_R', -12.0 * rear, 0.0, 0.0)
			a.rot('upperarm_R', paw, -8.0 * rear, 0.0)
			a.rot('lowerarm_R', -paw * 0.35 + 20.0 * rear, 0.0, 0.0)
			a.rot('upperarm_L', paw * 0.55, 6.0 * rear, 0.0)
			a.rot('lowerarm_L', -paw * 0.20 + 16.0 * rear, 0.0, 0.0)

			# 뒷다리는 버틴다
			for s in ('L', 'R'):
				a.rot('thigh_' + s, 14.0 * rear, 0.0, 0.0)
				a.rot('shin_' + s, 26.0 * rear, 0.0, 0.0)
				a.rot('hock_' + s, -30.0 * rear, 0.0, 0.0)

			a.rot('tail_01', 10.0 * rear, 0.0, 0.0)
			a.rot('tail_02', 16.0 * rear, 0.0, 0.0)

			a.key(total * t)

	elif family == 'canine':
		#       t   움츠림  돌진   목    입
		keys = [
			(0.00, 0.00,  0.00,   0.0,  0.0),
			(0.24, 1.00,  0.00, -16.0, 10.0),   # 몸을 낮춘다
			(0.44, 0.20,  1.00,  14.0, 34.0),   # 뻗어 물기
			(0.58, 0.10,  0.80,  10.0, 10.0),
			(1.00, 0.00,  0.00,   0.0,  0.0),
		]
		hit_t = 0.44

		for t, crouch, lunge, neck, jaw in keys:
			a.loc('hips', 0.0, 0.0, -0.06 * crouch)
			a.rot('hips', 10.0 * crouch - 6.0 * lunge, 0.0, 0.0)
			a.rot('spine_02', 8.0 * crouch - 8.0 * lunge, 0.0, 0.0)
			a.rot('spine_01', 6.0 * crouch - 10.0 * lunge, 0.0, 0.0)
			a.rot('chest', -4.0 * crouch - 6.0 * lunge, 0.0, 0.0)
			a.rot('neck_01', neck, 0.0, 0.0)
			a.rot('neck_02', neck * 0.8, 0.0, 0.0)
			a.rot('head', neck * 0.5, 0.0, 0.0)
			a.rot('jaw', jaw, 0.0, 0.0)

			for s in ('L', 'R'):
				a.rot('upperarm_' + s, -14.0 * crouch - 26.0 * lunge, 0.0, 0.0)
				a.rot('lowerarm_' + s, 30.0 * crouch + 10.0 * lunge, 0.0, 0.0)
				a.rot('thigh_' + s, -10.0 * crouch + 24.0 * lunge, 0.0, 0.0)
				a.rot('shin_' + s, 34.0 * crouch - 12.0 * lunge, 0.0, 0.0)
				a.rot('hock_' + s, -38.0 * crouch + 14.0 * lunge, 0.0, 0.0)

			a.rot('tail_01', -14.0 * crouch + 10.0 * lunge, 0.0, 0.0)

			a.key(total * t)

	else:   # cervid — 머리받기
		#       t   들기   찍기
		keys = [
			(0.00, 0.00, 0.00),
			(0.26, 1.00, 0.00),   # 고개를 치켜든다 — 뿔이 보인다
			(0.46, 0.00, 1.00),   # 아래로 찍는다
			(0.60, 0.10, 0.70),
			(1.00, 0.00, 0.00),
		]
		hit_t = 0.46

		for t, rear, ram in keys:
			a.rot('hips', -6.0 * rear + 4.0 * ram, 0.0, 0.0)
			a.rot('spine_02', -6.0 * rear + 5.0 * ram, 0.0, 0.0)
			a.rot('spine_01', -8.0 * rear + 6.0 * ram, 0.0, 0.0)
			a.rot('chest', -10.0 * rear + 8.0 * ram, 0.0, 0.0)
			a.rot('neck_01', -34.0 * rear + 30.0 * ram, 0.0, 0.0)
			a.rot('neck_02', -26.0 * rear + 26.0 * ram, 0.0, 0.0)
			a.rot('head', -18.0 * rear + 34.0 * ram, 0.0, 0.0)

			for s in ('L', 'R'):
				a.rot('upperarm_' + s, -20.0 * rear - 10.0 * ram, 0.0, 0.0)
				a.rot('lowerarm_' + s, 16.0 * rear + 8.0 * ram, 0.0, 0.0)
				a.rot('thigh_' + s, 10.0 * rear + 12.0 * ram, 0.0, 0.0)
				a.rot('shin_' + s, 14.0 * rear + 10.0 * ram, 0.0, 0.0)
				a.rot('hock_' + s, -18.0 * rear - 12.0 * ram, 0.0, 0.0)

			a.key(total * t)

	return a.end(dur, loop=False,
	             events=[{'name': 'hit', 'time': round(dur * hit_t, 3)}],
	             cancel_after=round(dur * (hit_t + 0.18), 3))


def clip_jump_start(a, family):
	dur = 0.28
	total = frames(dur)

	a.begin('jump_start')
	a.rest_pose()
	a.key_all(0)

	for t, crouch in ((0.00, 0.0), (0.58, 1.0), (1.00, 0.15)):
		a.loc('hips', 0.0, 0.0, -0.10 * crouch)
		a.rot('hips', 12.0 * crouch, 0.0, 0.0)
		a.rot('spine_02', 10.0 * crouch, 0.0, 0.0)
		a.rot('spine_01', 8.0 * crouch, 0.0, 0.0)
		a.rot('chest', -6.0 * crouch, 0.0, 0.0)
		a.rot('neck_01', -14.0 * crouch, 0.0, 0.0)

		for s in ('L', 'R'):
			a.rot('thigh_' + s, -20.0 * crouch, 0.0, 0.0)
			a.rot('shin_' + s, 46.0 * crouch, 0.0, 0.0)
			a.rot('hock_' + s, -52.0 * crouch, 0.0, 0.0)
			a.rot('metatarsus_' + s, 30.0 * crouch, 0.0, 0.0)
			a.rot('upperarm_' + s, -18.0 * crouch, 0.0, 0.0)
			a.rot('lowerarm_' + s, 34.0 * crouch, 0.0, 0.0)

		a.key(total * t)

	return a.end(dur, loop=False, events=[{'name': 'liftoff', 'time': round(dur * 0.88, 3)}])


def clip_jump_air(a, family):
	"""체공 — 앞다리는 뻗고 뒷다리는 접는다. 갤럽의 공중 구간과 같은 실루엣."""
	dur = 0.55
	total = frames(dur)
	n = 6

	a.begin('jump_air')

	for i in range(n + 1):
		p = (i / n) % 1.0
		s = math.sin(math.tau * p)

		a.rot('hips', -8.0 + 2.0 * s, 0.0, 0.0)
		a.rot('spine_02', -10.0 + 2.0 * s, 0.0, 0.0)
		a.rot('spine_01', -8.0 + 2.0 * s, 0.0, 0.0)
		a.rot('chest', 4.0, 0.0, 0.0)
		a.rot('neck_01', 6.0 + 1.5 * s, 0.0, 0.0)
		a.rot('head', -4.0, 0.0, 0.0)

		for sd in ('L', 'R'):
			a.rot('upperarm_' + sd, -44.0 + 4.0 * s, 0.0, 0.0)
			a.rot('lowerarm_' + sd, 14.0 + 4.0 * s, 0.0, 0.0)
			a.rot('thigh_' + sd, 26.0 + 3.0 * s, 0.0, 0.0)
			a.rot('shin_' + sd, 40.0 - 4.0 * s, 0.0, 0.0)
			a.rot('hock_' + sd, -46.0 + 4.0 * s, 0.0, 0.0)

		a.rot('tail_01', -16.0, 0.0, 0.0)
		a.rot('tail_02', -20.0, 0.0, 0.0)

		a.key(total * (i / n))

	return a.end(dur, loop=True)


def clip_jump_land(a, family):
	dur = 0.38
	total = frames(dur)

	a.begin('jump_land')
	a.rest_pose()

	for t, hit in ((0.00, 0.25), (0.28, 1.00), (0.62, 0.40), (1.00, 0.00)):
		a.loc('hips', 0.0, 0.0, -0.085 * hit)
		a.rot('hips', 14.0 * hit, 0.0, 0.0)
		a.rot('spine_02', 10.0 * hit, 0.0, 0.0)
		a.rot('chest', 8.0 * hit, 0.0, 0.0)
		a.rot('neck_01', -12.0 * hit, 0.0, 0.0)

		for s in ('L', 'R'):
			a.rot('upperarm_' + s, 18.0 * hit, 0.0, 0.0)
			a.rot('lowerarm_' + s, 38.0 * hit, 0.0, 0.0)
			a.rot('thigh_' + s, -16.0 * hit, 0.0, 0.0)
			a.rot('shin_' + s, 40.0 * hit, 0.0, 0.0)
			a.rot('hock_' + s, -46.0 * hit, 0.0, 0.0)

		a.key(total * t)

	return a.end(dur, loop=False, events=[{'name': 'footstep', 'time': 0.0}])


# ────────────────────────────────────────────────────────────

def build_all(arm_obj, family):
	a = anim.Animator(arm_obj)

	clip_base(a)
	clip_idle(a, family)
	clip_walk(a, family)
	clip_trot(a, family)
	clip_run(a, family)
	clip_turn(a, family, 'L')
	clip_turn(a, family, 'R')
	clip_attack(a, family)
	clip_jump_start(a, family)
	clip_jump_air(a, family)
	clip_jump_land(a, family)

	anim.finish(arm_obj)

	return a.clips
