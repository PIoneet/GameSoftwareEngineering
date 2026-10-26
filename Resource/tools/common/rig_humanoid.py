# -*- coding: utf-8 -*-
"""7등신 휴머노이드 아마튜어.

**이 파일이 관절 좌표의 유일한 출처다.** 메쉬를 만드는 gen_humanoid.py 도 여기 JOINTS 를
읽어 쓴다. 뼈와 살을 따로 적으면 반드시 어긋나고, 어긋나면 스키닝이 미세하게 틀어져
팔을 들 때 소매가 어깨에서 떨어진다.

비율은 FORMAT.md 6-1 표를 그대로 따른다. 머리 하나 H = 0.25 m, 키 7H = 1.75 m.

바인드 포즈는 **완전한 T 포즈가 아니라 느슨한 A 포즈**다. 이유가 둘 있다.

  1. 팔다리를 정확히 수직으로 두면 본의 roll 이 수학적으로 모호해진다(축이 전역 Z 와
     평행해져 Blender 가 기준 축을 임의로 고른다). 조금 기울여 두면 로컬 축이 확정되고,
     애니메이션에서 오일러 회전을 줄 때 예측 가능해진다.
  2. 무릎·팔꿈치가 미리 살짝 굽어 있어야 굽히는 방향이 정해진다. 완전히 편 관절은
     어느 쪽으로 접힐지 알 수 없어 보간이 엉뚱하게 돈다.
"""

import math

import bpy
from mathutils import Vector


H = 0.25          # 머리 한 개 높이
T = H * 7.0       # 전체 키 = 1.75 m


# ────────────────────────────────────────────────────────────
#  관절 좌표
# ────────────────────────────────────────────────────────────
#
# 좌표계는 Blender 기준 — Z 위, 캐릭터는 -Y 를 바라본다 (앞 = -Y).
# 왼쪽(_L)이 +X 다.

JOINTS = {
	# 몸통
	'root':        (0.000,  0.000, 0.000),
	'hips':        (0.000,  0.000, 0.950),
	'spine_01':    (0.000,  0.000, 1.020),
	'spine_02':    (0.000, -0.005, 1.130),
	'chest':       (0.000, -0.005, 1.260),
	'chest_top':   (0.000,  0.000, 1.420),   # chest 의 꼬리
	'neck':        (0.000,  0.005, 1.440),
	'head':        (0.000,  0.000, 1.540),
	'head_top':    (0.000, -0.010, 1.750),

	# 왼팔 — A 포즈로 바깥·약간 뒤로
	'shoulder_L':  (0.040,  0.000, 1.400),
	'upperarm_L':  (0.165,  0.000, 1.420),
	'lowerarm_L':  (0.255,  0.010, 1.100),   # 팔꿈치
	'hand_L':      (0.315, -0.010, 0.810),   # 손목
	'hand_end_L':  (0.335, -0.020, 0.700),   # 손끝

	# 왼다리 — 무릎이 살짝 앞(-Y)으로 나가 있다
	'thigh_L':     (0.090,  0.000, 0.900),
	'shin_L':      (0.095, -0.025, 0.499),   # 무릎
	'foot_L':      (0.090,  0.000, 0.068),   # 발목
	'toe_L':       (0.090, -0.105, 0.025),
	'toe_end_L':   (0.090, -0.175, 0.020),
}

# 소켓 — 메쉬가 붙지 않는, 좌표만 제공하는 본
SOCKETS = {
	'sk_hand_L':   ((0.322, -0.014, 0.770), (0.322, -0.014, 0.710)),
	'sk_back':     ((0.000,  0.070, 1.300), (0.000,  0.130, 1.300)),
	'sk_hip_L':    ((0.115,  0.020, 0.950), (0.115,  0.020, 0.890)),
	'sk_head_top': ((0.000,  0.000, 1.700), (0.000,  0.000, 1.780)),
}


def mirror(p):
	"""왼쪽 좌표 → 오른쪽."""
	return (-p[0], p[1], p[2])


def J(name):
	"""관절 좌표. '..._R' 은 왼쪽을 거울질해 돌려준다."""
	if name in JOINTS:
		return JOINTS[name]

	if name.endswith('_R'):
		left = name[:-2] + '_L'

		if left in JOINTS:
			return mirror(JOINTS[left])

	raise KeyError("모르는 관절: %s" % name)


# ────────────────────────────────────────────────────────────
#  본 정의 — (이름, 머리, 꼬리, 부모, 이어붙임)
# ────────────────────────────────────────────────────────────

def _bone_table():
	rows = [
		('root',     J('root'),     (0.0, -0.12, 0.0), None,       False),
		('hips',     J('hips'),     J('spine_01'),     'root',     False),
		('spine_01', J('spine_01'), J('spine_02'),     'hips',     True),
		('spine_02', J('spine_02'), J('chest'),        'spine_01', True),
		('chest',    J('chest'),    J('chest_top'),    'spine_02', True),
		('neck',     J('neck'),     J('head'),         'chest',    False),
		('head',     J('head'),     J('head_top'),     'neck',     True),
	]

	for s in ('L', 'R'):
		def j(n):
			return J(n + '_' + s)

		shoulder_root = (0.040 if s == 'L' else -0.040, 0.000, 1.400)

		rows += [
			('shoulder_' + s, shoulder_root, j('upperarm'), 'chest',         False),
			('upperarm_' + s, j('upperarm'), j('lowerarm'), 'shoulder_' + s, True),
			('lowerarm_' + s, j('lowerarm'), j('hand'),     'upperarm_' + s, True),
			('hand_' + s,     j('hand'),     j('hand_end'), 'lowerarm_' + s, True),

			('thigh_' + s,    j('thigh'),    j('shin'),     'hips',          False),
			('shin_' + s,     j('shin'),     j('foot'),     'thigh_' + s,    True),
			('foot_' + s,     j('foot'),     j('toe'),      'shin_' + s,     True),
			('toe_' + s,      j('toe'),      j('toe_end'),  'foot_' + s,     True),
		]

	# 소켓
	for name, (head, tail) in SOCKETS.items():
		parent = {
			'sk_hand_L':   'hand_L',
			'sk_back':     'chest',
			'sk_hip_L':    'hips',
			'sk_head_top': 'head',
		}[name]

		rows.append((name, head, tail, parent, False))

		if name.endswith('_L'):
			rows.append((name[:-2] + '_R', mirror(head), mirror(tail),
			             parent[:-2] + '_R' if parent.endswith('_L') else parent, False))

	return rows


# ────────────────────────────────────────────────────────────
#  생성
# ────────────────────────────────────────────────────────────

def build(name="rig_humanoid", scale=1.0):
	"""아마튜어 오브젝트를 만들어 돌려준다.

	scale 로 키를 바꿀 수 있다 (아이·거한). 비율은 그대로 유지된다.
	"""
	arm_data = bpy.data.armatures.new(name)
	arm_obj = bpy.data.objects.new(name, arm_data)

	bpy.context.collection.objects.link(arm_obj)

	# edit_bones 는 EDIT 모드에서만 열린다
	bpy.context.view_layer.objects.active = arm_obj
	bpy.ops.object.mode_set(mode='EDIT')

	created = {}

	for bone_name, head, tail, parent, connect in _bone_table():
		eb = arm_data.edit_bones.new(bone_name)

		eb.head = Vector(head) * scale
		eb.tail = Vector(tail) * scale
		eb.roll = 0.0

		created[bone_name] = eb

	# 부모는 전부 만든 뒤에 건다 — 선언 순서에 의존하지 않기 위함
	for bone_name, head, tail, parent, connect in _bone_table():
		if parent is None:
			continue

		eb = created[bone_name]
		eb.parent = created[parent]

		# 머리가 부모 꼬리와 같을 때만 이어붙인다. 아니면 떨어진 채로 둔다 —
		# 억지로 이으면 Blender 가 머리를 부모 꼬리로 끌어당겨 좌표가 바뀐다.
		if connect and (eb.head - created[parent].tail).length < 1e-5:
			eb.use_connect = True

	bpy.ops.object.mode_set(mode='OBJECT')

	# 모든 포즈본을 쿼터니언으로. 오일러는 짐벌락이 있고, glTF 가 어차피
	# 쿼터니언으로 받으므로 변환 단계를 하나 줄이는 셈이다.
	for pb in arm_obj.pose.bones:
		pb.rotation_mode = 'QUATERNION'

	return arm_obj


# ────────────────────────────────────────────────────────────
#  메쉬 쪽에서 쓰는 보조
# ────────────────────────────────────────────────────────────

def limb_segments(side):
	"""(시작, 끝, 본이름) 목록. 팔다리 메쉬를 뼈에 정확히 맞춰 얹을 때 쓴다."""
	def j(n):
		return Vector(J(n + '_' + side))

	return {
		'upperarm': (j('upperarm'), j('lowerarm'), 'upperarm_' + side),
		'lowerarm': (j('lowerarm'), j('hand'),     'lowerarm_' + side),
		'hand':     (j('hand'),     j('hand_end'), 'hand_' + side),
		'thigh':    (j('thigh'),    j('shin'),     'thigh_' + side),
		'shin':     (j('shin'),     j('foot'),     'shin_' + side),
		'foot':     (j('foot'),     j('toe'),      'foot_' + side),
		'toe':      (j('toe'),      j('toe_end'),  'toe_' + side),
	}


# 체형 변주 — 같은 릭을 쓰되 메쉬 두께만 바꾼다.
# 본 길이를 건드리면 애니메이션을 공유할 수 없으므로 **길이는 절대 바꾸지 않는다.**
BUILDS = {
	'slim':    {'limb': 0.86, 'torso': 0.88, 'head': 0.96},
	'normal':  {'limb': 1.00, 'torso': 1.00, 'head': 1.00},
	'stout':   {'limb': 1.18, 'torso': 1.22, 'head': 1.04},
	'child':   {'limb': 0.80, 'torso': 0.90, 'head': 1.18},   # scale 0.72 와 함께 쓴다
}
