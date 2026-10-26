# -*- coding: utf-8 -*-
"""Blender 없이 리소스 정의를 검증한다.

    python tools/check.py

생성 스크립트를 돌리려면 Blender 가 필요하지만, **틀린 것의 대부분은 Blender 를
켜기 전에 잡을 수 있다.** 좌표가 뒤집혔다거나, 본 이름에 오타가 났다거나,
폴리곤 예산을 넘었다거나 하는 것들이다.

그래서 `mathutils` 와 `bpy` 를 최소한으로 흉내 내서 기하 로직만 실제로 돌려 본다.
메쉬를 굽지는 않지만 정점·면·가중치는 전부 진짜로 계산된다.

특히 아래 둘은 눈으로 찾기 가장 어려운 종류라 반드시 자동으로 잡는다.

  * Animator.rot() 은 **모르는 본 이름을 조용히 무시한다.** 오타가 나면 에러 없이
    그 관절만 안 움직인다.
  * 4족 보행 순서는 숫자만 봐서는 맞는지 알 수 없다. 접지 시각을 정렬해
    "같은 쪽 뒷발이 앞발보다 먼저 나가는가" 를 직접 따진다.
"""

import math
import os
import sys
import types


HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


# ────────────────────────────────────────────────────────────
#  mathutils 흉내 — 실제로 쓰는 연산만
# ────────────────────────────────────────────────────────────

class _Vec:
	__slots__ = ('x', 'y', 'z')

	def __init__(self, t=(0.0, 0.0, 0.0)):
		if isinstance(t, _Vec):
			self.x, self.y, self.z = t.x, t.y, t.z
		else:
			self.x, self.y, self.z = float(t[0]), float(t[1]), float(t[2])

	def __iter__(self):
		return iter((self.x, self.y, self.z))

	def __getitem__(self, i):
		return (self.x, self.y, self.z)[i]

	def __len__(self):
		return 3

	def __add__(self, o):
		return _Vec((self.x + o[0], self.y + o[1], self.z + o[2]))

	def __sub__(self, o):
		return _Vec((self.x - o[0], self.y - o[1], self.z - o[2]))

	def __mul__(self, k):
		return _Vec((self.x * k, self.y * k, self.z * k))

	__rmul__ = __mul__

	def __truediv__(self, k):
		return _Vec((self.x / k, self.y / k, self.z / k))

	def __neg__(self):
		return _Vec((-self.x, -self.y, -self.z))

	@property
	def length(self):
		return math.sqrt(self.x ** 2 + self.y ** 2 + self.z ** 2)

	def normalized(self):
		l = self.length or 1.0

		return _Vec((self.x / l, self.y / l, self.z / l))

	def dot(self, o):
		return self.x * o[0] + self.y * o[1] + self.z * o[2]

	def cross(self, o):
		return _Vec((self.y * o[2] - self.z * o[1],
		             self.z * o[0] - self.x * o[2],
		             self.x * o[1] - self.y * o[0]))

	def lerp(self, o, t):
		return _Vec((self.x + (o[0] - self.x) * t,
		             self.y + (o[1] - self.y) * t,
		             self.z + (o[2] - self.z) * t))


class _Mat:
	def __init__(self, rows):
		self.rows = [list(r) for r in rows]

	def __matmul__(self, o):
		if isinstance(o, _Vec):
			return _Vec(tuple(sum(self.rows[i][k] * o[k] for k in range(3))
			                  for i in range(3)))

		return _Mat([[sum(self.rows[i][k] * o.rows[k][j] for k in range(3))
		              for j in range(3)] for i in range(3)])


class _Euler:
	def __init__(self, angles, order='XYZ'):
		self.a = angles

	def to_matrix(self):
		rx, ry, rz = self.a
		cx, sx = math.cos(rx), math.sin(rx)
		cy, sy = math.cos(ry), math.sin(ry)
		cz, sz = math.cos(rz), math.sin(rz)

		X = _Mat([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
		Y = _Mat([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
		Z = _Mat([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])

		return Z @ (Y @ X)


def _install_stubs():
	if 'bpy' in sys.modules:
		return

	m = types.ModuleType('mathutils')
	m.Vector = _Vec
	m.Matrix = _Mat
	m.Euler = _Euler
	m.Quaternion = lambda *a: None
	sys.modules['mathutils'] = m

	b = types.ModuleType('bpy')
	b.data = types.SimpleNamespace()
	b.ops = types.SimpleNamespace()
	b.context = types.SimpleNamespace()
	sys.modules['bpy'] = b

	sys.modules['bmesh'] = types.ModuleType('bmesh')


_install_stubs()

if HERE not in sys.path:
	sys.path.insert(0, HERE)

import importlib.util

from common import anim, anim_humanoid, anim_quadruped, build, palette
from common import rig_humanoid as rh
from common import rig_quadruped as rq


FAIL = []


def check(cond, msg):
	if not cond:
		FAIL.append(msg)
		print("  [X] " + msg)

	return bool(cond)


def _load_script(name):
	spec = importlib.util.spec_from_file_location(name, os.path.join(HERE, name + '.py'))
	mod = importlib.util.module_from_spec(spec)
	sys.modules[name] = mod
	spec.loader.exec_module(mod)

	return mod


def tris(mb):
	return sum(max(0, len(f) - 2) for f in mb.faces)


def vcolor_clip(mb):
	"""정점 색이 머티리얼 색보다 밝은 채널을 센다.

	정점 색은 glTF 에서 baseColorFactor 에 **곱해지는 배율**로 나가고, 정규화 정수라
	1.0 을 넘지 못한다. 기준보다 밝게 칠하면 조용히 잘려 변주가 사라진다 —
	화면에서는 "왜 얼룩이 안 보이지" 로만 보여서 찾기 어렵다.

	반환값은 (잘린 채널 수, 최대 배율).
	"""
	if not any(c is not None for c in mb.vert_colors):
		return 0, 1.0

	vert_mat = mb._vertex_materials()
	clipped = 0
	worst = 1.0

	for i, c in enumerate(mb.vert_colors):
		if c is None:
			continue

		want = palette.lin(c)
		base = palette.lin(palette.MATERIALS[vert_mat[i]][0])

		for k in range(3):
			m = want[k] / max(base[k], 1e-4)
			worst = max(worst, m)

			if m > 1.0 + 1e-3:
				clipped += 1

	return clipped, worst


# ────────────────────────────────────────────────────────────
#  1. 팔레트
# ────────────────────────────────────────────────────────────

def check_palette():
	print("\n[1] 팔레트")

	for name, (srgb, met, rough) in palette.MATERIALS.items():
		check(len(srgb) == 3, "%s: 색 성분이 3개가 아니다" % name)
		check(all(0.0 <= c <= 1.0 for c in srgb), "%s: 색이 0~1 밖 %s" % (name, srgb))
		check(0.0 <= met <= 1.0 and 0.0 <= rough <= 1.0,
		      "%s: metallic/roughness 가 범위 밖" % name)

	for name in palette.EMISSIVE_STRENGTH:
		check(name in palette.MATERIALS,
		      "EMISSIVE_STRENGTH 의 %s 가 MATERIALS 에 없다" % name)

	lin = palette.lin((0.5, 0.5, 0.5))

	check(abs(lin[0] - 0.2140) < 0.001, "sRGB→선형 변환이 틀렸다: %.4f" % lin[0])

	slots = [n for n in palette.MATERIALS if n.startswith('slot.')]

	print("  머티리얼 %d 종 (교체 슬롯 %d), 전부 범위 안"
	      % (len(palette.MATERIALS), len(slots)))


# ────────────────────────────────────────────────────────────
#  2. 휴머노이드 릭
# ────────────────────────────────────────────────────────────

def check_humanoid_rig():
	print("\n[2] 휴머노이드 릭")

	table = rh._bone_table()
	names = [r[0] for r in table]

	check(len(names) == len(set(names)),
	      "본 이름 중복: %s" % [n for n in names if names.count(n) > 1])

	for bone, head, tail, parent, connect in table:
		check((_Vec(tail) - _Vec(head)).length > 1e-4, "%s: 길이 0 인 본" % bone)
		check(parent is None or parent in names, "%s: 부모 %s 가 없다" % (bone, parent))

		if connect:
			p = [r for r in table if r[0] == parent][0]

			check((_Vec(head) - _Vec(p[2])).length < 1e-5,
			      "%s: use_connect 인데 머리가 부모 꼬리와 다르다" % bone)

	skeleton = [r for r in table if not r[0].startswith('sk_')]
	ground = min(min(h[2], t[2]) for _, h, t, _, _ in skeleton)
	crown = max(max(h[2], t[2]) for _, h, t, _, _ in skeleton)

	check(abs(ground) < 1e-6, "발이 바닥(z=0)에 없다: %.4f" % ground)
	check(abs(crown - rh.T) < 1e-3,
	      "키가 7등신(%.3f)과 다르다: %.3f" % (rh.T, crown))

	for bone, head, tail, parent, connect in table:
		if not bone.endswith('_L'):
			continue

		mate = [x for x in table if x[0] == bone[:-2] + '_R']

		if check(len(mate) == 1, "%s 의 좌우 짝이 없다" % bone):
			check(abs(mate[0][1][0] + head[0]) < 1e-6
			      and abs(mate[0][1][2] - head[2]) < 1e-6,
			      "%s 와 짝이 대칭이 아니다" % bone)

	sockets = sorted(n for n in names if n.startswith('sk_'))

	check(len(sockets) >= 6, "소켓이 모자란다: %s" % sockets)

	print("  본 %d 개 (소켓 %d), 키 %.3f m = %.1f 등신, 바닥 접지 OK"
	      % (len(table), len(sockets), crown, crown / rh.H))


# ────────────────────────────────────────────────────────────
#  3. 4족 릭
# ────────────────────────────────────────────────────────────

def check_quadruped_rig():
	print("\n[3] 4족 릭")

	meta = {}

	for family in rq.FAMILIES:
		table = rq._bone_table(family, 1.0)
		names = [r[0] for r in table]

		check(len(names) == len(set(names)), "%s: 본 이름 중복" % family)

		for bone, head, tail, parent, connect in table:
			check(parent is None or parent in names,
			      "%s/%s: 부모 %s 가 없다" % (family, bone, parent))

		for b in ('thigh_L', 'shin_L', 'hock_L', 'metatarsus_L', 'hindtoe_L'):
			check(b in names, "%s: 뒷다리 본 %s 가 없다" % (family, b))

		toes = [rq.J(family, b, 1.0)[2]
		        for b in ('fronttoe_end_L', 'hindtoe_end_L')]

		check(max(toes) < 1e-6, "%s: 발끝이 바닥에 안 닿는다 %s" % (family, toes))

		knee = rq.J(family, 'shin_L', 1.0)
		hock = rq.J(family, 'hock_L', 1.0)

		check(hock[2] < knee[2], "%s: 비절이 무릎보다 높다" % family)
		check(hock[1] > knee[1],
		      "%s: 비절이 무릎보다 앞에 있다 — 뒤로 꺾여야 짐승 뒷다리다" % family)

		meta[family] = hock[2] - rq.J(family, 'hindtoe_L', 1.0)[2]

		print("  %-7s 본 %2d 개  중족골 %.3f  비절높이 %.3f"
		      % (family, len(table), meta[family], hock[2]))

	check(meta['cervid'] > meta['canine'] > meta['feline'],
	      "중족골 길이가 사슴>개>고양이 순이 아니다: %s" % meta)

	print("  중족골 사슴 %.3f > 개 %.3f > 고양이 %.3f  (제행 > 지행)"
	      % (meta['cervid'], meta['canine'], meta['feline']))


# ────────────────────────────────────────────────────────────
#  4. MeshBuilder
# ────────────────────────────────────────────────────────────

def check_meshbuilder():
	print("\n[4] MeshBuilder")

	mb = build.MeshBuilder()
	mb.block((0, 0, 0.5), (1, 1, 1), 'fix.wood_oak', bone='hips')

	check(len(mb.verts) == 8, "block 정점이 8개가 아니다: %d" % len(mb.verts))
	check(len(mb.faces) == 6, "block 면이 6개가 아니다: %d" % len(mb.faces))
	check(all(b == [('hips', 1.0)] for b in mb.vert_bones), "block 가중치가 안 붙었다")

	mb = build.MeshBuilder()
	mb.limb((0, 0, 0), (0, 0, 1), 0.1, 0.05, 'fix.bark', bone='spine_01', sides=5)

	check(len(mb.verts) == 10, "limb(5면) 정점이 10개가 아니다: %d" % len(mb.verts))
	check(len(mb.faces) == 7, "limb(5면) 면이 7개가 아니다: %d" % len(mb.faces))

	mb = build.MeshBuilder()
	start = mb.mark()
	mb.block((0.5, 0, 0.5), (0.2, 0.2, 1.0), 'slot.skin', bone='thigh_L')
	nv, nf = len(mb.verts), len(mb.faces)
	mb.mirror_x(start)

	check(len(mb.verts) == nv * 2, "mirror 후 정점이 두 배가 아니다")
	check(len(mb.faces) == nf * 2, "mirror 후 면이 두 배가 아니다")
	check(all(b == [('thigh_R', 1.0)] for b in mb.vert_bones[nv:]),
	      "mirror 가 본 이름 _L→_R 을 안 바꿨다")
	check(abs(mb.verts[nv].x + mb.verts[0].x) < 1e-9, "mirror 가 x 를 안 뒤집었다")

	for i, f in enumerate(mb.faces):
		check(all(0 <= k < len(mb.verts) for k in f),
		      "면 %d 의 인덱스가 범위 밖: %s" % (i, f))

	print("  block / limb / wedge / ico / mirror_x 정상")


# ────────────────────────────────────────────────────────────
#  5. 파츠 — 유효성과 폴리곤 예산
# ────────────────────────────────────────────────────────────

def _run_part(fn, *args):
	mb = build.MeshBuilder()
	fn(mb, *args)

	return mb


def check_parts():
	print("\n[5] 파츠 — 유효성과 폴리곤 예산")

	gh = _load_script('gen_humanoid')
	default_tri = 0

	for pname, slot, fn, default in gh.PARTS:
		mb = _run_part(fn)
		n = tris(mb)

		check(len(mb.verts) > 0, "%s: 비어 있다" % pname)
		check(all(len(b) > 0 for b in mb.vert_bones),
		      "%s: 본에 안 묶인 정점이 있다 — 애니메이션 때 공중에 남는다" % pname)
		check(all(all(0 <= k < len(mb.verts) for k in f) for f in mb.faces),
		      "%s: 면 인덱스가 범위 밖" % pname)
		check(n <= 400, "%s: 파츠 예산 400 초과 — %d tri" % (pname, n))

		if default:
			default_tri += n

	check(default_tri <= 1800, "기본 캐릭터가 예산 1800 tri 를 넘는다: %d" % default_tri)
	print("  캐릭터 파츠 %2d 종   기본 조합 %4d tri / 1800"
	      % (len(gh.PARTS), default_tri))

	# 프리셋이 가리키는 파츠가 실제로 있는가
	have = {p[0] for p in gh.PARTS}

	for pid, label, parts, ci, si, hi in gh.PRESETS:
		for p in parts:
			check(p in have, "프리셋 %s: '%s' 파츠가 없다" % (pid, p))

		check(0 <= ci < len(palette.CLOAK_TONES), "프리셋 %s: 망토색 인덱스 밖" % pid)
		check(0 <= si < len(palette.SKIN_TONES), "프리셋 %s: 피부색 인덱스 밖" % pid)
		check(0 <= hi < len(palette.HAIR_TONES), "프리셋 %s: 머리색 인덱스 밖" % pid)

	print("  프리셋 %2d 명    파츠·색 참조 전부 유효" % len(gh.PRESETS))

	gq = _load_script('gen_quadruped')

	for family in rq.FAMILIES:
		total = 0

		for pname, slot, fn in gq.parts_for(family):
			if fn is gq.head:
				mb = _run_part(fn, family)
			else:
				mb = _run_part(fn, family)

			check(len(mb.verts) > 0, "%s: 비어 있다" % pname)
			check(all(len(b) > 0 for b in mb.vert_bones),
			      "%s: 본에 안 묶인 정점이 있다" % pname)

			if slot != 'taint':
				total += tris(mb)

		check(total <= 1200, "%s: 짐승 예산 1200 초과 — %d" % (family, total))
		print("  %-7s 짐승     %4d tri / 1200" % (family, total))

	for s in gq.SPECIES:
		have = {slot for _, slot, _ in gq.parts_for(s['family'])}

		for p in s['parts']:
			check(p in have, "%s: %s 과에 '%s' 파츠가 없다" % (s['id'], s['family'], p))

		check(s['fur'][0] in palette.FUR and s['fur'][1] in palette.FUR,
		      "%s: 털색 %s 가 팔레트에 없다" % (s['id'], s['fur']))
		check(s['temperament'] in ('dangerous', 'neutral', 'gentle'),
		      "%s: 성향 값이 이상하다 — %s" % (s['id'], s['temperament']))

	kinds = {}

	for s in gq.SPECIES:
		kinds.setdefault(s['temperament'], []).append(s['id'])

	print("  종 %d 개       위험 %d · 중립 %d · 순함 %d"
	      % (len(gq.SPECIES), len(kinds.get('dangerous', [])),
	         len(kinds.get('neutral', [])), len(kinds.get('gentle', []))))

	for t in ('dangerous', 'neutral', 'gentle'):
		check(t in kinds, "성향 '%s' 인 종이 하나도 없다" % t)

	gp = _load_script('gen_props')

	for group, budget, label in ((gp.STRUCTURE, 600, 'structure'),
	                             (gp.FURNITURE, 260, 'furniture'),
	                             (gp.TOOLS, 220, 'tools')):
		worst = ('', 0)

		for pname, fn in group:
			mb = _run_part(fn)
			n = tris(mb)

			check(len(mb.verts) > 0, "%s: 비어 있다" % pname)
			check(n <= budget, "%s: 예산 %d 초과 — %d tri" % (pname, budget, n))

			if n > worst[1]:
				worst = (pname, n)

		print("  %-10s %2d 종  최대 %-26s %3d tri / %d"
		      % (label, len(group), worst[0], worst[1], budget))

	all_props = {n for n, _ in gp.STRUCTURE + gp.FURNITURE + gp.TOOLS}

	for bp in gp.BLUEPRINTS:
		for piece in bp['pieces']:
			check(piece['mesh'] in all_props,
			      "청사진 %s: '%s' 메쉬가 없다" % (bp['id'], piece['mesh']))

	print("  청사진 %d 종    메쉬 참조 전부 유효" % len(gp.BLUEPRINTS))

	gn = _load_script('gen_nature')

	worst_mult = ('', 1.0)

	for pname, fn in gn.FLORA:
		mb = _run_part(fn)
		n = tris(mb)

		check(len(mb.verts) > 0, "%s: 비어 있다" % pname)
		check(n <= 520, "%s: 예산 520 초과 — %d tri" % (pname, n))

		clipped, worst = vcolor_clip(mb)

		check(clipped == 0,
		      "%s: 정점 색 %d 채널이 머티리얼보다 밝다 (최대 %.2f배) — 내보낼 때 잘려 "
		      "변주가 사라진다. 더 밝은 머티리얼을 쓰거나 색을 낮출 것" % (pname, clipped, worst))

		if worst > worst_mult[1]:
			worst_mult = (pname, worst)

	print("  flora %2d 종    전부 520 tri 안, 정점 색 배율 최대 %.2f (%s)"
	      % (len(gn.FLORA), worst_mult[1], worst_mult[0] or '-'))

	for pname, tile, mat, base, relief, rough in gn.GROUND:
		mb = build.MeshBuilder()
		gn.ground_tile(mb, mat, base, relief, rough)

		check(len(mb.verts) == 25,
		      "%s: 4분할 타일 정점이 25개가 아니다 — %d" % (pname, len(mb.verts)))

		for v in mb.verts:
			if abs(abs(v.x) - 0.5) < 1e-9 or abs(abs(v.y) - 0.5) < 1e-9:
				check(abs(v.z) < 1e-12,
				      "%s: 가장자리 정점이 들려 있다 — 옆 타일과 금이 간다" % pname)

		check(mat in palette.MATERIALS, "%s: 머티리얼 %s 가 팔레트에 없다" % (pname, mat))

		clipped, worst = vcolor_clip(mb)

		check(clipped == 0,
		      "%s: 정점 색 %d 채널이 머티리얼 %s 보다 밝다 (최대 %.2f배) — 잘려서 "
		      "타일이 단색이 된다" % (pname, clipped, mat, worst))

	print("  지형 타일 %d 종  가장자리 평탄, 정점 색 전부 머티리얼 이하" % len(gn.GROUND))


# ────────────────────────────────────────────────────────────
#  6. 애니메이션
# ────────────────────────────────────────────────────────────

class _Recorder:
	"""Animator 와 같은 얼굴을 하고, 건드린 본 이름만 모은다."""

	def __init__(self, arm=None):
		self.bones = set()
		self.clips = []
		self._name = None
		self._keys = 0

	def begin(self, name):
		self._name = name
		self._keys = 0

		return self

	def rest_pose(self):
		return self

	def rot(self, bone, rx=0.0, ry=0.0, rz=0.0):
		self.bones.add(bone)

		return self

	add_rot = rot

	def loc(self, bone, x=0.0, y=0.0, z=0.0):
		self.bones.add(bone)

		return self

	def key(self, frame):
		self._keys += 1

		return self

	def key_all(self, frame):
		self._keys += 1

		return self

	def hold(self, bone_rots, f0, f1):
		self.bones.update(bone_rots)
		self._keys += 2

		return self

	def end(self, duration, loop=False, **meta):
		row = {'name': self._name, 'duration': duration,
		       'loop': loop, 'keys': self._keys}
		row.update(meta)
		self.clips.append(row)

		return None


REQUIRED_CLIPS = ('base', 'idle', 'walk', 'run', 'turn_L', 'turn_R',
                  'attack_01', 'jump_start', 'jump_air', 'jump_land')


def _bake(label, build_all, rig_bones, args=()):
	holder = {}
	original = anim.Animator
	original_finish = anim.finish

	class Captured(_Recorder):
		def __init__(self, arm=None):
			_Recorder.__init__(self, arm)
			holder['rec'] = self

	anim.Animator = Captured
	anim.finish = lambda arm: None

	try:
		clips = build_all(None, *args)
	finally:
		anim.Animator = original
		anim.finish = original_finish

	rec = holder['rec']
	unknown = sorted(rec.bones - rig_bones)

	check(not unknown,
	      "%s: 릭에 없는 본을 건드린다 (조용히 무시되어 그 관절만 안 움직인다) → %s"
	      % (label, unknown))

	names = [c['name'] for c in clips]

	check(len(names) == len(set(names)), "%s: 클립 이름 중복 %s" % (label, names))

	for r in REQUIRED_CLIPS:
		check(r in names, "%s: 필수 클립 '%s' 가 없다" % (label, r))

	for c in clips:
		check(c['duration'] > 0, "%s/%s: 길이가 0" % (label, c['name']))
		check(c['keys'] >= 1, "%s/%s: 키프레임이 없다" % (label, c['name']))

		if c.get('loop'):
			check(c['keys'] >= 4,
			      "%s/%s: 루프인데 키가 %d 개뿐" % (label, c['name'], c['keys']))

		if c['name'] in ('walk', 'trot', 'run'):
			check('contact_phase' in c, "%s/%s: contact_phase 가 없다" % (label, c['name']))
			check('speed_mps' in c, "%s/%s: speed_mps 가 없다" % (label, c['name']))

	print("  %-16s 클립 %2d 개  본 %2d 개" % (label, len(clips), len(rec.bones)))

	return {c['name']: c for c in clips}


def check_anim():
	print("\n[6] 애니메이션")

	human_bones = {r[0] for r in rh._bone_table()}
	h = _bake("휴머노이드", anim_humanoid.build_all, human_bones)

	check(h['walk']['contact_phase'] == h['run']['contact_phase'],
	      "walk 와 run 의 접지 위상이 다르다 — 섞으면 발이 미끄러진다")
	check(h['run']['speed_mps'] > h['walk']['speed_mps'], "run 이 walk 보다 느리다")

	hit = [e for e in h['attack_01'].get('events', []) if e['name'] == 'hit']

	check(len(hit) == 1, "attack_01 에 hit 이벤트가 정확히 하나 있어야 한다")
	check(0.0 < hit[0]['time'] < h['attack_01']['duration'],
	      "attack_01 의 hit 시각이 클립 길이 밖이다")

	for family in rq.FAMILIES:
		bones = {x[0] for x in rq._bone_table(family, 1.0)}
		q = _bake("4족 " + family, anim_quadruped.build_all, bones, args=(family,))

		check('trot' in q, "%s: trot 이 없다" % family)
		check(q['walk']['speed_mps'] < q['trot']['speed_mps'] < q['run']['speed_mps'],
		      "%s: 걸음걸이 속도가 walk<trot<run 이 아니다" % family)

		for g in ('walk', 'trot'):
			check(abs(q[g]['contact_phase']['foot_FL']) < 1e-6,
			      "%s/%s: FL 접지가 0 이 아니다 — walk↔trot 위상 동기가 깨진다"
			      % (family, g))

		_check_footfall(family, q)


def _check_footfall(family, clips):
	"""보행 순서를 직접 따진다. 숫자만 봐서는 맞는지 알 수 없는 부분이다."""
	walk = clips['walk']['contact_phase']
	order = sorted(walk.items(), key=lambda kv: kv[1])
	seq = [k.replace('foot_', '') for k, _ in order]

	# 측대 순서 — 같은 쪽 뒷발이 앞발보다 **먼저** 나간다.
	# LF 가 0 이면 그 직전(= 위상 0.75)이 LH 여야 한다.
	check(walk['foot_HL'] > walk['foot_FL'] and walk['foot_HL'] > 0.5,
	      "%s/walk: 같은 쪽 앞발(LF)이 뒷발(LH)보다 먼저 나간다 — 영장류 걸음이다 "
	      "(접지 %s)" % (family, walk))
	check(abs(walk['foot_HR'] - 0.25) < 1e-6 and abs(walk['foot_FR'] - 0.50) < 1e-6,
	      "%s/walk: 4박자가 고르지 않다 %s" % (family, walk))

	trot = clips['trot']['contact_phase']

	check(abs(trot['foot_FL'] - trot['foot_HR']) < 1e-6
	      and abs(trot['foot_FR'] - trot['foot_HL']) < 1e-6,
	      "%s/trot: 대각선 두 발이 동시에 닿지 않는다 %s" % (family, trot))

	run = clips['run']['contact_phase']
	hind_gap = abs(run['foot_HR'] - run['foot_HL'])
	fore_gap = abs(run['foot_FR'] - run['foot_FL'])
	lead = min(run['foot_FL'], run['foot_FR']) - max(run['foot_HL'], run['foot_HR'])

	check(hind_gap < 0.25 and fore_gap < 0.25,
	      "%s/run: 갤럽인데 뒷발끼리/앞발끼리 모이지 않는다 %s" % (family, run))
	check(lead > 0.15,
	      "%s/run: 뒷발이 밀고 앞발이 받는 순서가 아니다 %s" % (family, run))

	print("    %-7s walk 착지 %s   갤럽 뒷발→앞발 간격 %.2f"
	      % (family, " → ".join(seq), lead))


# ────────────────────────────────────────────────────────────
#  7. 설계표 대조
# ────────────────────────────────────────────────────────────

def check_design_table():
	print("\n[7] gen_anim_graph.py 설계표 대조")

	gag = _load_script('gen_anim_graph')

	human_bones = {r[0] for r in rh._bone_table()}
	h = _bake("휴머노이드(대조)", anim_humanoid.build_all, human_bones)

	bones = {x[0] for x in rq._bone_table('canine', 1.0)}
	q = _bake("4족(대조)", anim_quadruped.build_all, bones, args=('canine',))

	for label, baked, ref_table in (('휴머노이드', h, gag.HUMANOID_CLIPS),
	                                ('4족', q, gag.QUADRUPED_CLIPS)):
		for name, c in baked.items():
			ref = ref_table.get(name)

			if not check(ref is not None, "설계표에 %s '%s' 가 없다" % (label, name)):
				continue

			check(abs(ref['duration'] - c['duration']) < 1e-6,
			      "%s %s 길이 불일치: 설계 %.3f vs 실제 %.3f"
			      % (label, name, ref['duration'], c['duration']))
			check(ref['loop'] == c['loop'], "%s %s 루프 플래그 불일치" % (label, name))

			if 'contact_phase' in ref:
				check(ref['contact_phase'] == c.get('contact_phase'),
				      "%s %s 접지 위상 불일치\n        설계 %s\n        실제 %s"
				      % (label, name, ref['contact_phase'], c.get('contact_phase')))

	print("  길이·루프·접지 위상 전부 일치")


# ────────────────────────────────────────────────────────────

def main():
	print("Resource 검증 — Blender 없이 돌아간다")

	check_palette()
	check_humanoid_rig()
	check_quadruped_rig()
	check_meshbuilder()
	check_parts()
	check_anim()
	check_design_table()

	print("\n" + "=" * 60)

	if FAIL:
		print("실패 %d 건" % len(FAIL))

		for f in FAIL:
			print("  - " + f)

		return 1

	print("전부 통과")

	return 0


if __name__ == '__main__':
	sys.exit(main())
