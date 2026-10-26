# -*- coding: utf-8 -*-
"""애니메이션 조합 정보 — 블렌드 트리 · 접지 위상 · 전이 표.

    python tools/gen_anim_graph.py        (Blender 없이 그냥 돈다)

**이 스크립트만 bpy 를 쓰지 않는다.** 메쉬를 만들지 않고 메타데이터만 쓰기 때문이다.
덕분에 Blender 를 깔기 전에도 로더 쪽 작업을 먼저 시작할 수 있다.

data/character_parts.json 과 data/creature_catalog.json 이 이미 있으면 거기서
실제 구워진 클립 길이를 읽어 덮어쓴다. 없으면 아래 표의 설계값을 그대로 쓴다 —
두 값은 같아야 하고, 다르면 생성 스크립트와 이 표 중 하나가 낡은 것이다.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(ROOT, 'data')


# ────────────────────────────────────────────────────────────
#  설계값 — anim_humanoid.py / anim_quadruped.py 와 같아야 한다
# ────────────────────────────────────────────────────────────

HUMANOID_CLIPS = {
	'base':       {'duration': 1 / 30.0, 'loop': False, 'reference': True},
	'idle':       {'duration': 2.40, 'loop': True,  'speed_mps': 0.0},
	'walk':       {'duration': 1.00, 'loop': True,  'speed_mps': 1.35,
	               'contact_phase': {'foot_L': 0.0, 'foot_R': 0.5}},
	'run':        {'duration': 0.60, 'loop': True,  'speed_mps': 4.20,
	               'contact_phase': {'foot_L': 0.0, 'foot_R': 0.5}},
	'turn_L':     {'duration': 0.50, 'loop': False, 'yaw_deg': 90.0},
	'turn_R':     {'duration': 0.50, 'loop': False, 'yaw_deg': -90.0},
	'attack_01':  {'duration': 0.70, 'loop': False,
	               'events': [{'name': 'hit', 'time': 0.308},
	                          {'name': 'recover', 'time': 0.434}]},
	'jump_start': {'duration': 0.25, 'loop': False,
	               'events': [{'name': 'liftoff', 'time': 0.213}]},
	'jump_air':   {'duration': 0.60, 'loop': True},
	'jump_land':  {'duration': 0.35, 'loop': False,
	               'events': [{'name': 'footstep', 'time': 0.0}]},
}

QUADRUPED_CLIPS = {
	'base':       {'duration': 1 / 30.0, 'loop': False, 'reference': True},
	'idle':       {'duration': 2.80, 'loop': True,  'speed_mps': 0.0},
	'walk':       {'duration': 1.00, 'loop': True,  'speed_mps': 1.10,
	               'contact_phase': {'foot_FL': 0.00, 'foot_HR': 0.25,
	                                 'foot_FR': 0.50, 'foot_HL': 0.75}},
	'trot':       {'duration': 0.62, 'loop': True,  'speed_mps': 2.60,
	               'contact_phase': {'foot_FL': 0.00, 'foot_HR': 0.00,
	                                 'foot_FR': 0.50, 'foot_HL': 0.50}},
	'run':        {'duration': 0.44, 'loop': True,  'speed_mps': 6.00,
	               'contact_phase': {'foot_HL': 0.00, 'foot_HR': 0.12,
	                                 'foot_FL': 0.42, 'foot_FR': 0.54}},
	'turn_L':     {'duration': 0.60, 'loop': False, 'yaw_deg': 90.0},
	'turn_R':     {'duration': 0.60, 'loop': False, 'yaw_deg': -90.0},
	'attack_01':  {'duration': 0.80, 'loop': False},
	'jump_start': {'duration': 0.28, 'loop': False,
	               'events': [{'name': 'liftoff', 'time': 0.246}]},
	'jump_air':   {'duration': 0.55, 'loop': True},
	'jump_land':  {'duration': 0.38, 'loop': False,
	               'events': [{'name': 'footstep', 'time': 0.0}]},
}


# ── 전이 ────────────────────────────────────────────────────
#
# sync 'phase' — 정규화 위상을 그대로 넘겨받는다. 두 클립의 접지 위상이 맞을 때만 쓴다.
#                발이 땅에 붙은 채로 섞이므로 미끄러지지 않는다.
# sync 'none'  — 그냥 크로스페이드. 걸음걸이 구조가 다르거나 보행이 아닐 때.
#
# duration 은 **끼어드는 동작일수록 짧다.** 공격·점프가 0.2초씩 늘어지면
# 입력에 반응하지 않는 것처럼 느껴진다.

COMMON_TRANSITIONS = [
	('idle',       'walk',       0.20, 'none'),
	('walk',       'idle',       0.22, 'none'),
	('walk',       'run',        0.18, 'phase'),
	('run',        'walk',       0.18, 'phase'),
	('run',        'idle',       0.26, 'none'),

	('*',          'turn_L',     0.12, 'none'),
	('*',          'turn_R',     0.12, 'none'),
	('turn_L',     'idle',       0.12, 'none'),
	('turn_R',     'idle',       0.12, 'none'),

	('*',          'attack_01',  0.08, 'none'),
	('attack_01',  'idle',       0.15, 'none'),

	('*',          'jump_start', 0.06, 'none'),
	('jump_start', 'jump_air',   0.05, 'none'),
	('jump_air',   'jump_land',  0.04, 'none'),
	('jump_land',  'idle',       0.12, 'none'),
	('jump_land',  'walk',       0.14, 'none'),
]

QUADRUPED_EXTRA = [
	('walk',  'trot', 0.18, 'phase'),   # 둘 다 FL 접지가 위상 0 이라 위상을 잇는다
	('trot',  'walk', 0.18, 'phase'),
	('trot',  'run',  0.14, 'none'),    # 갤럽은 발 순서 자체가 달라 위상을 못 잇는다
	('run',   'trot', 0.16, 'none'),
]

# 보행 블렌드 트리 — 속도 하나로 섞는다
HUMANOID_TREE = ['idle', 'walk', 'run']
QUADRUPED_TREE = ['idle', 'walk', 'trot', 'run']


# ────────────────────────────────────────────────────────────

def _load(name):
	path = os.path.join(DATA, name)

	if not os.path.isfile(path):
		return None

	with open(path, encoding='utf-8') as f:
		return json.load(f)


def _merge(defaults, baked):
	"""구워진 클립 메타데이터로 설계값을 덮어쓴다."""
	out = {k: dict(v) for k, v in defaults.items()}

	for c in baked or []:
		name = c.get('name')

		if name is None:
			continue

		row = out.setdefault(name, {})

		for k, v in c.items():
			if k != 'name':
				row[k] = v

	return out


def _transitions(rows):
	return [
		{'from': a, 'to': b, 'duration': d, 'sync': s}
		for a, b, d, s in rows
	]


def _tree(clips, order):
	stops = []

	for name in order:
		c = clips.get(name)

		if c is None:
			continue

		stops.append({'clip': name, 'value': round(c.get('speed_mps', 0.0), 3)})

	return {
		'parameter': 'speed_mps',
		'stops': stops,
		'_note': '파라미터는 수평 이동 속도(m/s). 두 구간 사이면 선형 보간한다. '
		         '섞는 동안 두 클립의 정규화 위상을 공유해야 발이 미끄러지지 않는다.',
	}


def main():
	chars = _load('character_parts.json')
	creatures = _load('creature_catalog.json')

	human_clips = _merge(HUMANOID_CLIPS, (chars or {}).get('clips'))

	quad = {}

	for family in ('feline', 'canine', 'cervid'):
		baked = ((creatures or {}).get('families', {}).get(family) or {}).get('clips')

		quad[family] = _merge(QUADRUPED_CLIPS, baked)

	graph = {
		'_comment': 'gen_anim_graph.py 가 생성. 손으로 고치지 말 것 — 다시 돌리면 덮어쓴다.',
		'_source_present': {
			'character_parts.json': chars is not None,
			'creature_catalog.json': creatures is not None,
		},

		'conventions': {
			'fps': 30,
			'loop_range': '[0, duration) — 마지막 프레임은 0 프레임과 같으므로 재생에서 제외한다',
			'root_motion': False,
			'root_motion_note': '모든 이동 클립은 제자리다. 실제 이동은 게임이 한다. '
			                    'speed_mps 는 "이 클립이 원래 어느 속도를 가정하고 만들어졌는가" 다.',
			'base_pose': 'base',
			'base_pose_note': 'turn/attack/jump 는 모두 base 포즈에서 시작해 base 로 끝난다. '
			                  '도중에 끊겨도 포즈가 튀지 않는 이유다.',
			'turn_note': 'turn_L/R 은 몸통을 25도만 비튼다. 실제 방향 전환은 게임이 '
			             'yaw 를 slerp 해서 한다 — 클립이 90도를 돌면 두 배로 돌아간다.',
			'interpolation': 'LINEAR',
		},

		'humanoid': {
			'gltf': 'export/characters/humanoid.gltf',
			'armature': 'rig_humanoid',
			'clips': human_clips,
			'locomotion': _tree(human_clips, HUMANOID_TREE),
			'transitions': _transitions(COMMON_TRANSITIONS),
			'foot_bones': ['foot_L', 'foot_R'],
		},

		'quadruped': {
			'_note': '릭은 어깨높이 1.0 m 로 구워져 있다. 종 크기를 scale 로 키우면 '
			         'speed_mps 에도 같은 scale 을 곱해야 보폭이 맞는다.',
			'families': {
				family: {
					'gltf': 'export/creatures/%s.gltf' % family,
					'armature': 'rig_' + family,
					'clips': clips,
					'locomotion': _tree(clips, QUADRUPED_TREE),
					'transitions': _transitions(COMMON_TRANSITIONS + QUADRUPED_EXTRA),
					'foot_bones': ['fronttoe_L', 'fronttoe_R', 'hindtoe_L', 'hindtoe_R'],
				}
				for family, clips in quad.items()
			},
			'gait_order': {
				'walk': '측대 4박자 — LF, RH, RF, LH 순. 같은 쪽 뒷발이 앞발보다 먼저 나간다',
				'trot': '대각 2박자 — (FL+HR), (FR+HL) 동시에',
				'run':  '횡단 갤럽 — LH, RH 가 거의 같이 밀고 한 박자 뒤 LF, RF 가 받는다. 사이에 네 발이 다 뜬다',
			},
		},
	}

	if not os.path.isdir(DATA):
		os.makedirs(DATA)

	out = os.path.join(DATA, 'anim_graph.json')

	with open(out, 'w', encoding='utf-8') as f:
		json.dump(graph, f, ensure_ascii=False, indent='\t')

	n_h = len(human_clips)
	n_q = sum(len(c) for c in quad.values())

	print("anim_graph.json — 휴머노이드 %d 클립 / 4족 %d 클립 (3개 과)" % (n_h, n_q))

	if chars is None or creatures is None:
		print("  (설계값으로 생성. Blender 생성 스크립트를 돌린 뒤 다시 실행하면"
		      " 실제 구워진 값으로 갱신된다)")

	print("  → %s" % out)

	return graph


if __name__ == '__main__':
	main()
