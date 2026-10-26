# -*- coding: utf-8 -*-
"""4족 보행 아마튜어 — 고양이과 / 개과 / 사슴과.

**이 릭의 요점은 뒷다리다.**

사람 다리를 네 개 쓰면 짐승이 되지 않는다. 포유류 뒷다리는 관절이 다섯 단이고,
그중 `hock`(비절, 사람의 발뒤꿈치에 해당)이 **지면에서 떠서 뒤로 꺾여 있다.**
흔히 "역관절" 이라 부르는 그 모양이 사실은 뒤꿈치를 들고 선 자세다.

        hip ●
             \\            thigh — 짧고 굵다. 엉덩이 근육에 거의 묻혀 있다
              \\
      stifle   ● (무릎 — 몸 쪽으로 붙어 있다. 바깥에서 거의 안 보인다)
              /
             /             shin — 길다. 앞으로 기운다
            /
   hock    ●               ← 뒤로 튀어나온 이 점이 짐승 뒷다리의 인상을 결정한다
            \\
             \\            metatarsus — 사람의 발등. 짐승은 이걸 세워 딛는다
              \\
        toe    ●           ← 실제 접지면은 여기뿐이다
               bottom

세 과의 차이도 결국 이 비율 싸움이다.

  고양이과 — metatarsus 짧고 발바닥 넓다. 몸통이 길고 척추가 유연하다. 꼬리가 길다.
  개  과 — metatarsus 중간. 흉곽이 깊고 등이 곧다. 지구력 체형.
  사슴과 — **metatarsus 가 극단적으로 길다.** 다리 전체가 가늘고, 발굽 끝만 땅에 닿는다
           (제행, unguligrade). 목이 길고 꼬리가 짧다.

좌표는 전부 **어깨높이(shoulder height) 1.0 기준의 정규화 값**이다. 종마다
shoulder_height 만 곱하면 된다. 덕분에 같은 애니메이션을 크기만 달리해 공유할 수 있다.
"""

import bpy
from mathutils import Vector


FAMILIES = ('feline', 'canine', 'cervid')


# ────────────────────────────────────────────────────────────
#  과별 관절 좌표 (어깨높이 = 1.0 정규화)
# ────────────────────────────────────────────────────────────
#
# Blender 기준 — Z 위, 짐승은 -Y 를 바라본다. 꼬리 쪽이 +Y.
# 왼쪽(_L)이 +X.

_FELINE = {
	# 몸통 — 길고 낮다. 척추가 활처럼 약간 휜다
	'hips':       (0.000,  0.50, 0.80),
	'spine_03':   (0.000,  0.30, 0.83),
	'spine_02':   (0.000,  0.08, 0.84),
	'spine_01':   (0.000, -0.16, 0.84),
	'chest':      (0.000, -0.40, 0.81),
	'neck_01':    (0.000, -0.52, 0.84),
	'neck_02':    (0.000, -0.64, 0.88),
	'head':       (0.000, -0.74, 0.92),
	'head_end':   (0.000, -0.94, 0.90),
	'jaw':        (0.000, -0.80, 0.86),
	'jaw_end':    (0.000, -0.96, 0.83),

	# 앞다리 — 어깨뼈가 등 위로 솟는다 (걸을 때 오르내리는 그 봉우리)
	'scapula_L':   (0.105, -0.26, 0.94),
	'upperarm_L':  (0.115, -0.40, 0.68),
	'lowerarm_L':  (0.115, -0.32, 0.42),
	'frontfoot_L': (0.115, -0.37, 0.15),
	'fronttoe_L':  (0.115, -0.37, 0.045),
	'fronttoe_end_L': (0.115, -0.50, 0.000),

	# 뒷다리 — metatarsus 짧다. 웅크리기 좋은 체형
	'thigh_L':     (0.115,  0.46, 0.80),
	'shin_L':      (0.125,  0.31, 0.47),
	'hock_L':      (0.120,  0.46, 0.22),
	'metatarsus_L':(0.118,  0.38, 0.045),
	'hindtoe_L':   (0.118,  0.36, 0.000),
	'hindtoe_end_L': (0.118,  0.24, 0.000),

	# 꼬리 — 길다. 균형을 잡는 기관이라 마디가 많아야 설득력이 있다
	'tail_01':    (0.000,  0.54, 0.76),
	'tail_02':    (0.000,  0.76, 0.70),
	'tail_03':    (0.000,  0.98, 0.60),
	'tail_end':   (0.000,  1.18, 0.46),
}

_CANINE = {
	# 몸통 — 흉곽이 깊고 등이 곧다
	'hips':       (0.000,  0.44, 0.80),
	'spine_03':   (0.000,  0.27, 0.81),
	'spine_02':   (0.000,  0.07, 0.82),
	'spine_01':   (0.000, -0.14, 0.82),
	'chest':      (0.000, -0.36, 0.80),
	'neck_01':    (0.000, -0.46, 0.85),
	'neck_02':    (0.000, -0.58, 0.93),
	'head':       (0.000, -0.70, 0.99),
	'head_end':   (0.000, -0.92, 0.95),
	'jaw':        (0.000, -0.76, 0.93),
	'jaw_end':    (0.000, -0.96, 0.88),

	'scapula_L':   (0.110, -0.24, 0.93),
	'upperarm_L':  (0.120, -0.38, 0.69),
	'lowerarm_L':  (0.120, -0.30, 0.44),
	'frontfoot_L': (0.120, -0.36, 0.17),
	'fronttoe_L':  (0.120, -0.36, 0.045),
	'fronttoe_end_L': (0.120, -0.48, 0.000),

	# 뒷다리 — 비절이 뚜렷하게 뒤로 빠진다
	'thigh_L':     (0.120,  0.42, 0.80),
	'shin_L':      (0.130,  0.29, 0.50),
	'hock_L':      (0.124,  0.45, 0.26),
	'metatarsus_L':(0.122,  0.36, 0.050),
	'hindtoe_L':   (0.122,  0.34, 0.000),
	'hindtoe_end_L': (0.122,  0.22, 0.000),

	'tail_01':    (0.000,  0.48, 0.77),
	'tail_02':    (0.000,  0.66, 0.70),
	'tail_03':    (0.000,  0.82, 0.60),
	'tail_end':   (0.000,  0.96, 0.48),
}

_CERVID = {
	# 몸통 — 짧고 높다. 다리가 몸보다 길다
	'hips':       (0.000,  0.40, 0.84),
	'spine_03':   (0.000,  0.25, 0.86),
	'spine_02':   (0.000,  0.07, 0.86),
	'spine_01':   (0.000, -0.12, 0.85),
	'chest':      (0.000, -0.32, 0.83),
	'neck_01':    (0.000, -0.40, 0.90),
	'neck_02':    (0.000, -0.52, 1.08),   # 목이 길고 거의 수직으로 선다
	'head':       (0.000, -0.58, 1.24),
	'head_end':   (0.000, -0.76, 1.18),
	'jaw':        (0.000, -0.63, 1.18),
	'jaw_end':    (0.000, -0.79, 1.12),

	'scapula_L':   (0.090, -0.22, 0.95),
	'upperarm_L':  (0.098, -0.33, 0.72),
	'lowerarm_L':  (0.098, -0.27, 0.46),
	'frontfoot_L': (0.098, -0.31, 0.20),   # 발목(구절)이 높다
	'fronttoe_L':  (0.098, -0.31, 0.055),  # 발굽
	'fronttoe_end_L': (0.098, -0.39, 0.000),

	# 뒷다리 — metatarsus 가 아주 길다. 이게 사슴 실루엣의 전부다
	'thigh_L':     (0.098,  0.38, 0.84),
	'shin_L':      (0.108,  0.26, 0.56),
	'hock_L':      (0.102,  0.40, 0.34),   # 비절이 높이 붙는다
	'metatarsus_L':(0.100,  0.33, 0.055),  # 길이 0.29 — 개과의 1.4배
	'hindtoe_L':   (0.100,  0.32, 0.000),
	'hindtoe_end_L': (0.100,  0.23, 0.000),

	# 꼬리 — 짧다. 두 마디면 충분하다 (tail_03 은 길이를 거의 0 으로)
	'tail_01':    (0.000,  0.43, 0.80),
	'tail_02':    (0.000,  0.52, 0.72),
	'tail_03':    (0.000,  0.57, 0.65),
	'tail_end':   (0.000,  0.60, 0.58),
}

JOINTS = {
	'feline': _FELINE,
	'canine': _CANINE,
	'cervid': _CERVID,
}


# 과별 메쉬 두께 힌트 — 뼈가 아니라 살의 문제라 gen_quadruped.py 가 읽는다
BULK = {
	'feline': {'torso': 1.00, 'limb': 1.00, 'neck': 1.05, 'paw': 1.25},
	'canine': {'torso': 0.96, 'limb': 0.94, 'neck': 1.00, 'paw': 1.00},
	'cervid': {'torso': 0.90, 'limb': 0.66, 'neck': 0.74, 'paw': 0.62},
}

# 접지하는 발 끝. 애니메이션에서 발을 땅에 붙일 때 기준이 된다.
GROUND_BONES = ('fronttoe_L', 'fronttoe_R', 'hindtoe_L', 'hindtoe_R')


# ────────────────────────────────────────────────────────────
#  좌표 조회
# ────────────────────────────────────────────────────────────

def mirror(p):
	return (-p[0], p[1], p[2])


def J(family, name, scale=1.0):
	"""관절 좌표를 어깨높이 scale 로 늘려 돌려준다."""
	table = JOINTS[family]

	if name in table:
		p = table[name]
	elif name.endswith('_R') and (name[:-2] + '_L') in table:
		p = mirror(table[name[:-2] + '_L'])
	else:
		raise KeyError("%s 에 없는 관절: %s" % (family, name))

	return (p[0] * scale, p[1] * scale, p[2] * scale)


# ────────────────────────────────────────────────────────────
#  본 테이블
# ────────────────────────────────────────────────────────────

def _bone_table(family, scale):
	def j(n):
		return J(family, n, scale)

	rows = [
		('root',     j('hips')[0:2] + (0.0,), (0.0, -0.25 * scale, 0.0), None, False),

		# 척추는 엉덩이에서 앞으로 간다
		('hips',     j('hips'),     j('spine_03'), 'root',     False),
		('spine_03', j('spine_03'), j('spine_02'), 'hips',     True),
		('spine_02', j('spine_02'), j('spine_01'), 'spine_03', True),
		('spine_01', j('spine_01'), j('chest'),    'spine_02', True),
		('chest',    j('chest'),    j('neck_01'),  'spine_01', True),

		('neck_01',  j('neck_01'),  j('neck_02'),  'chest',    True),
		('neck_02',  j('neck_02'),  j('head'),     'neck_01',  True),
		('head',     j('head'),     j('head_end'), 'neck_02',  True),
		('jaw',      j('jaw'),      j('jaw_end'),  'head',     False),

		('tail_01',  j('tail_01'),  j('tail_02'),  'hips',     False),
		('tail_02',  j('tail_02'),  j('tail_03'),  'tail_01',  True),
		('tail_03',  j('tail_03'),  j('tail_end'), 'tail_02',  True),
	]

	for s in ('L', 'R'):
		def q(n):
			return J(family, n + '_' + s, scale)

		rows += [
			# 앞다리
			('scapula_' + s,   q('scapula'),   q('upperarm'),      'chest',           False),
			('upperarm_' + s,  q('upperarm'),  q('lowerarm'),      'scapula_' + s,    True),
			('lowerarm_' + s,  q('lowerarm'),  q('frontfoot'),     'upperarm_' + s,   True),
			('frontfoot_' + s, q('frontfoot'), q('fronttoe'),      'lowerarm_' + s,   True),
			('fronttoe_' + s,  q('fronttoe'),  q('fronttoe_end'),  'frontfoot_' + s,  True),

			# 뒷다리 — 5단
			('thigh_' + s,      q('thigh'),      q('shin'),          'hips',            False),
			('shin_' + s,       q('shin'),       q('hock'),          'thigh_' + s,      True),
			('hock_' + s,       q('hock'),       q('metatarsus'),    'shin_' + s,       True),
			('metatarsus_' + s, q('metatarsus'), q('hindtoe'),       'hock_' + s,       True),
			('hindtoe_' + s,    q('hindtoe'),    q('hindtoe_end'),   'metatarsus_' + s, True),
		]

	# 소켓 — 입(물기 판정·이펙트 부착점)
	head_end = j('head_end')
	rows.append(('sk_mouth', j('jaw_end'),
	             (head_end[0], head_end[1] - 0.06 * scale, head_end[2]), 'head', False))

	return rows


# ────────────────────────────────────────────────────────────
#  생성
# ────────────────────────────────────────────────────────────

def build(family, shoulder_height, name=None):
	"""4족 아마튜어를 만든다.

	family         : 'feline' | 'canine' | 'cervid'
	shoulder_height: 미터. 늑대 0.80, 사슴 1.20, 살쾡이 0.55 정도
	"""
	if family not in JOINTS:
		raise ValueError("모르는 과: %s" % family)

	name = name or ("rig_" + family)

	arm_data = bpy.data.armatures.new(name)
	arm_obj = bpy.data.objects.new(name, arm_data)

	bpy.context.collection.objects.link(arm_obj)
	bpy.context.view_layer.objects.active = arm_obj
	bpy.ops.object.mode_set(mode='EDIT')

	table = _bone_table(family, shoulder_height)
	created = {}

	for bone_name, head, tail, parent, connect in table:
		eb = arm_data.edit_bones.new(bone_name)

		eb.head = Vector(head)
		eb.tail = Vector(tail)
		eb.roll = 0.0

		# 길이 0 인 본은 Blender 가 조용히 지운다 — 사슴 꼬리처럼 짧은 마디가 위험하다
		if (eb.tail - eb.head).length < 1e-4:
			eb.tail = eb.head + Vector((0.0, 0.0, 1e-3))

		created[bone_name] = eb

	for bone_name, head, tail, parent, connect in table:
		if parent is None:
			continue

		eb = created[bone_name]
		eb.parent = created[parent]

		if connect and (eb.head - created[parent].tail).length < 1e-5:
			eb.use_connect = True

	bpy.ops.object.mode_set(mode='OBJECT')

	for pb in arm_obj.pose.bones:
		pb.rotation_mode = 'QUATERNION'

	arm_obj['family'] = family
	arm_obj['shoulder_height'] = shoulder_height

	return arm_obj
