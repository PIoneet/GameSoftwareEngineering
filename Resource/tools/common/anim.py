# -*- coding: utf-8 -*-
"""애니메이션 클립 작성 유틸.

## 왜 "아마튜어 공간 회전" 인가

포즈본의 `rotation_quaternion` 은 **그 본의 rest 로컬 좌표계** 기준이다. 본마다 방향이
제각각이라 "허벅지를 30도 앞으로" 를 쓰려면 매번 그 본의 로컬 축이 어디를 보는지
따져야 한다. 다리처럼 아래를 향한 본은 roll 계산이 특이점에 가까워 직관이 더 안 통한다.

그래서 이 모듈은 회전을 **아마튜어 공간**(X 오른쪽, Y 뒤, Z 위)으로 받아서 넣기 직전에
로컬로 변환한다.

    R_local = M⁻¹ · R_world · M        (M = bone.matrix_local 의 회전부)

덕분에 전부 해부학적으로 읽힌다.

    X 축 회전 — 앞뒤로 흔들기 (다리를 내딛고, 팔을 젓고, 고개를 끄덕인다)
    Y 축 회전 — 옆으로 벌리기 / 몸을 기울이기
    Z 축 회전 — 비틀기 (고개를 돌리고, 골반을 비튼다)

부모가 돌아가 있어도 자식은 **부모를 따라 도는 자기 프레임** 안에서 회전하므로
FK 로서 기대한 대로 동작한다.

## 루프 규약

루프 클립은 프레임 `0` 부터 `N = duration × fps` 까지 만들고, **프레임 N 의 포즈는
프레임 0 과 같게** 둔다. 게임은 `[0, N)` 구간을 반복 재생한다 (N 은 0 과 겹치므로 제외).
"""

import math

import bpy
from mathutils import Euler, Matrix, Quaternion, Vector


FPS = 30


def frames(seconds):
	return int(round(seconds * FPS))


def deg(d):
	return math.radians(d)


# ────────────────────────────────────────────────────────────
#  보간 곡선
# ────────────────────────────────────────────────────────────

def ease(t):
	"""부드러운 가감속 (smoothstep). 공격 준비동작처럼 '쌓는' 구간에 쓴다."""
	t = min(1.0, max(0.0, t))

	return t * t * (3.0 - 2.0 * t)


def ease_out(t):
	"""빠르게 시작해 천천히 멈춘다. 타격 직후·착지 충격 흡수."""
	t = min(1.0, max(0.0, t))

	return 1.0 - (1.0 - t) ** 3


def ease_in(t):
	"""천천히 시작해 가속. 도약 준비."""
	t = min(1.0, max(0.0, t))

	return t ** 3


def wave(phase, amp=1.0, offset=0.0):
	"""위상 0~1 을 사인파로. 보행 주기의 기본."""
	return math.sin((phase + offset) * math.tau) * amp


def wave_cos(phase, amp=1.0, offset=0.0):
	return math.cos((phase + offset) * math.tau) * amp


# ────────────────────────────────────────────────────────────
#  Animator
# ────────────────────────────────────────────────────────────

class Animator:
	"""아마튜어 하나에 클립을 차례로 구워 넣는다."""

	def __init__(self, arm_obj):
		self.arm = arm_obj
		self.clips = []            # 메타데이터 — anim_graph.json 으로 나간다
		self._touched = set()      # 이 클립에서 한 번이라도 회전시킨 본
		self._touched_loc = set()  # 이동까지 쓴 본

		# rest 회전 행렬을 미리 뽑아 둔다. 프레임마다 다시 구하면 느리다.
		self._rest = {}

		for pb in arm_obj.pose.bones:
			self._rest[pb.name] = pb.bone.matrix_local.to_3x3()

	# ── 클립 ──────────────────────────────────────────

	def begin(self, name):
		self._name = name
		self._touched = set()
		self._touched_loc = set()

		if self.arm.animation_data is not None:
			self.arm.animation_data.action = None

		self.rest_pose()

		return self

	def end(self, duration, loop=False, **meta):
		"""액션 이름을 붙이고 메타데이터를 기록한다."""
		ad = self.arm.animation_data

		if ad is None or ad.action is None:
			raise RuntimeError("%s: 키프레임이 하나도 없다" % self._name)

		action = ad.action
		action.name = self._name

		# 가짜 사용자를 걸지 않으면 다음 클립으로 넘어갈 때 참조가 끊겨
		# 저장·내보내기 전에 사라진다
		action.use_fake_user = True

		# 내보내기가 참조하는 재생 구간
		action.frame_start = 0
		action.frame_end = frames(duration)

		record = {
			'name': self._name,
			'duration': round(duration, 4),
			'loop': loop,
			'frames': frames(duration),
		}
		record.update(meta)

		self.clips.append(record)

		ad.action = None

		return action

	# ── 포즈 ──────────────────────────────────────────

	def rest_pose(self):
		"""모든 본을 바인드 포즈로. 클립 시작마다 호출해 이전 포즈가 새지 않게 한다."""
		for pb in self.arm.pose.bones:
			pb.rotation_quaternion = Quaternion((1.0, 0.0, 0.0, 0.0))
			pb.location = Vector((0.0, 0.0, 0.0))
			pb.scale = Vector((1.0, 1.0, 1.0))

	def rot(self, bone, rx=0.0, ry=0.0, rz=0.0):
		"""아마튜어 공간 오일러(도) 회전을 본에 건다. 같은 프레임에 여러 번 부르면 덮어쓴다."""
		pb = self.arm.pose.bones.get(bone)

		if pb is None:
			return self

		world = Euler((deg(rx), deg(ry), deg(rz)), 'XYZ').to_matrix()
		m = self._rest[bone]

		pb.rotation_quaternion = (m.inverted() @ world @ m).to_quaternion()

		self._touched.add(bone)

		return self

	def add_rot(self, bone, rx=0.0, ry=0.0, rz=0.0):
		"""이미 걸린 회전 위에 **덧붙인다.** 보행 위에 상체 반동을 얹을 때."""
		pb = self.arm.pose.bones.get(bone)

		if pb is None:
			return self

		world = Euler((deg(rx), deg(ry), deg(rz)), 'XYZ').to_matrix()
		m = self._rest[bone]
		extra = (m.inverted() @ world @ m).to_quaternion()

		pb.rotation_quaternion = pb.rotation_quaternion @ extra

		self._touched.add(bone)

		return self

	def loc(self, bone, x=0.0, y=0.0, z=0.0):
		"""본 로컬 이동. 주로 hips 의 상하 반동에 쓴다.

		hips 의 로컬 Y 가 위를 향하므로 **z 로 준 값이 로컬 Y 로 들어간다** —
		호출하는 쪽이 아마튜어 공간으로 생각할 수 있게 여기서 변환한다.
		"""
		pb = self.arm.pose.bones.get(bone)

		if pb is None:
			return self

		m = self._rest[bone]
		pb.location = m.inverted() @ Vector((x, y, z))

		self._touched.add(bone)
		self._touched_loc.add(bone)

		return self

	# ── 키 ────────────────────────────────────────────

	def key(self, frame):
		"""지금 포즈를 그 프레임에 박는다.

		한 번이라도 건드린 본은 **전부** 매 키프레임에 기록한다. 일부만 키를 넣으면
		다른 본의 값이 앞뒤 키 사이에서 제멋대로 보간되어 포즈가 흘러내린다.
		"""
		f = int(round(frame))

		for name in self._touched:
			pb = self.arm.pose.bones.get(name)

			if pb is None:
				continue

			pb.keyframe_insert('rotation_quaternion', frame=f, group=name)

			# 이동을 쓰는 본은 값이 0 인 프레임에도 반드시 키를 넣는다.
			# 빼먹으면 앞뒤 키 사이로 보간되면서 발이 땅에서 뜬다.
			if name in self._touched_loc:
				pb.keyframe_insert('location', frame=f, group=name)

		return self

	def key_all(self, frame):
		"""모든 본을 기록한다. base 포즈처럼 전체를 고정해야 할 때."""
		for pb in self.arm.pose.bones:
			self._touched.add(pb.name)

		return self.key(frame)

	# ── 흔한 모양 ─────────────────────────────────────

	def hold(self, bone_rots, f0, f1):
		"""두 프레임에 같은 포즈를 박아 그 구간을 정지시킨다."""
		for f in (f0, f1):
			for bone, r in bone_rots.items():
				self.rot(bone, *r)

			self.key(f)

		return self


# ────────────────────────────────────────────────────────────
#  마무리
# ────────────────────────────────────────────────────────────

def finish(arm_obj):
	"""내보내기 직전 정리.

	액션을 하나도 안 걸어 두면 glTF 내보내기가 '현재 액션' 을 기준으로 잡고
	나머지를 빼먹는 경우가 있다. base 를 걸어 둔다.
	"""
	base = bpy.data.actions.get('base')

	if base is not None:
		if arm_obj.animation_data is None:
			arm_obj.animation_data_create()

		arm_obj.animation_data.action = base

		# 4.4+ 슬롯 액션 — 할당된 슬롯이 없으면 포즈가 안 먹는다
		slots = getattr(base, 'slots', None)

		if slots:
			try:
				arm_obj.animation_data.action_slot = slots[0]
			except (AttributeError, TypeError):
				pass
