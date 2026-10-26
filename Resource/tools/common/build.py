# -*- coding: utf-8 -*-
"""로우폴리 메쉬 조립 유틸.

설계 의도 — **오브젝트를 여러 개 만들어 join 하지 않는다.**
파츠 하나는 블록 수십 개로 이루어지는데, 블록마다 bpy 오브젝트를 만들고
bpy.ops.object.join 으로 합치면 느린 데다 선택 상태·컨텍스트에 의존해서
백그라운드 실행 중에 조용히 깨지기 쉽다.

대신 MeshBuilder 가 정점·면·머티리얼 인덱스를 파이썬 리스트에 쌓아 두었다가
마지막에 from_pydata 로 **메쉬 하나를 한 번에** 만든다. bpy.ops 를 거의 안 쓰므로
--background 에서도 안정적이다.
"""

import math

import bpy
import bmesh
from mathutils import Vector, Matrix, Euler

from . import palette


# ────────────────────────────────────────────────────────────
#  씬
# ────────────────────────────────────────────────────────────

def reset_scene():
	"""빈 씬에서 시작한다. 여러 생성 스크립트를 한 프로세스에서 돌릴 때 필수."""
	bpy.ops.wm.read_factory_settings(use_empty=True)

	scene = bpy.context.scene
	scene.unit_settings.system = 'METRIC'
	scene.unit_settings.scale_length = 1.0

	# 애니메이션 기본 프레임레이트. anim.py 가 초 → 프레임 변환에 쓴다.
	scene.render.fps = 30

	return scene


def link(obj):
	bpy.context.collection.objects.link(obj)

	return obj


# ────────────────────────────────────────────────────────────
#  머티리얼
# ────────────────────────────────────────────────────────────

_MAT_CACHE = {}


def _principled_set(bsdf, key, value):
	"""Principled BSDF 입력 이름이 Blender 버전마다 달라서 후보를 순회한다.

	4.0 에서 'Specular' → 'Specular IOR Level', 'Emission' → 'Emission Color' 로
	바뀌었다. 없는 이름에 쓰면 KeyError 가 나므로 조용히 넘긴다.
	"""
	names = key if isinstance(key, (list, tuple)) else [key]

	for n in names:
		if n in bsdf.inputs:
			bsdf.inputs[n].default_value = value

			return True

	return False


def make_material(name):
	"""이름으로 머티리얼을 만든다. 이미 있으면 그대로 돌려준다.

	이름 규칙은 FORMAT.md 3절. 'emit.' 으로 시작하면 발광까지 켠다.
	"""
	if name in _MAT_CACHE:
		return _MAT_CACHE[name]

	existing = bpy.data.materials.get(name)
	if existing is not None:
		_MAT_CACHE[name] = existing

		return existing

	spec = palette.MATERIALS.get(name)
	if spec is None:
		raise KeyError("팔레트에 없는 머티리얼: %s — palette.MATERIALS 에 먼저 추가할 것" % name)

	srgb, metallic, roughness = spec
	rgba = palette.lin(srgb)

	mat = bpy.data.materials.new(name)
	mat.use_nodes = True

	bsdf = mat.node_tree.nodes.get("Principled BSDF")
	if bsdf is None:
		bsdf = mat.node_tree.nodes.new("ShaderNodeBsdfPrincipled")

	_principled_set(bsdf, "Base Color", rgba)
	_principled_set(bsdf, "Metallic", metallic)
	_principled_set(bsdf, "Roughness", roughness)

	# 스페큘러는 **기본값에서 건드리지 않는다.**
	# 낮추면 Blender 뷰포트에서는 보기 좋지만, 내보낼 때 KHR_materials_specular
	# 확장이 붙는다. 로더는 스페큘러를 쓰지도 않는데 확장만 늘어나는 셈이라
	# "확장은 쓰지 않는다"(FORMAT.md 1절)는 약속을 깨면서 얻는 것이 없다.

	if name.startswith("emit."):
		strength = palette.EMISSIVE_STRENGTH.get(name, 1.0)

		_principled_set(bsdf, ["Emission Color", "Emission"], rgba)
		_principled_set(bsdf, "Emission Strength", strength)

	# 뷰포트 색도 맞춰 둔다 — .blend 를 열었을 때 눈으로 확인하기 위함
	mat.diffuse_color = rgba

	_MAT_CACHE[name] = mat

	return mat


def clear_material_cache():
	"""reset_scene 뒤에는 캐시의 머티리얼이 전부 무효다."""
	_MAT_CACHE.clear()


# ────────────────────────────────────────────────────────────
#  MeshBuilder
# ────────────────────────────────────────────────────────────

class MeshBuilder:
	"""정점/면/머티리얼/본가중치를 쌓아 두었다가 메쉬 하나로 굽는다."""

	def __init__(self):
		self.verts = []           # [Vector, ...]
		self.faces = []           # [(i, j, k, ...), ...]
		self.face_mats = []       # 면별 머티리얼 이름
		self.vert_bones = []      # 정점별 [(bone, weight), ...]
		self.vert_colors = []     # 정점별 (r, g, b) sRGB 또는 None
		self.smooth = []          # 면별 스무스 여부

	# ── 내부 ──────────────────────────────────────────

	def _push_verts(self, points, bone, weights, color):
		base = len(self.verts)

		for p in points:
			self.verts.append(Vector(p))

			if weights is not None:
				self.vert_bones.append(list(weights))
			elif bone is not None:
				self.vert_bones.append([(bone, 1.0)])
			else:
				self.vert_bones.append([])

			self.vert_colors.append(color)

		return base

	def _push_faces(self, faces, mat, smooth):
		for f in faces:
			self.faces.append(tuple(f))
			self.face_mats.append(mat)
			self.smooth.append(smooth)

	# ── 블록 — 가장 기본이 되는 조각 ───────────────────
	#
	# 아래가 넓고 위가 좁은 사각기둥. 팔다리·몸통·기둥 전부 이걸로 만든다.
	# taper 로 위쪽 크기를 줄이면 사지 끝으로 갈수록 가늘어지는 모양이 나온다.

	def block(self, center, size, mat,
	          taper=1.0, taper_y=None, top_shift=(0.0, 0.0),
	          rot=None, bone=None, weights=None, color=None, smooth=False):
		"""축 정렬 육면체 (위쪽을 줄이거나 밀 수 있다).

		center  : 블록 중심 (x, y, z)
		size    : 아래 단면 기준 (sx, sy, sz)
		taper   : 위쪽 단면 배율. taper_y 를 따로 주면 x/y 를 다르게 줄인다
		top_shift: 위 단면을 (dx, dy) 만큼 민다 — 기울어진 사지에 쓴다
		rot     : (rx, ry, rz) 라디안. 중심 기준 회전
		"""
		cx, cy, cz = center
		sx, sy, sz = size
		tx = taper
		ty = taper if taper_y is None else taper_y
		dx, dy = top_shift

		hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5

		pts = [
			(-hx,      -hy,      -hz),   # 0 아래
			( hx,      -hy,      -hz),   # 1
			( hx,       hy,      -hz),   # 2
			(-hx,       hy,      -hz),   # 3
			(-hx * tx + dx, -hy * ty + dy,  hz),   # 4 위
			( hx * tx + dx, -hy * ty + dy,  hz),   # 5
			( hx * tx + dx,  hy * ty + dy,  hz),   # 6
			(-hx * tx + dx,  hy * ty + dy,  hz),   # 7
		]

		if rot is not None:
			m = Euler(rot, 'XYZ').to_matrix()
			pts = [m @ Vector(p) for p in pts]

		pts = [(p[0] + cx, p[1] + cy, p[2] + cz) for p in pts]

		b = self._push_verts(pts, bone, weights, color)

		self._push_faces([
			(b + 0, b + 1, b + 2, b + 3),   # 바닥
			(b + 7, b + 6, b + 5, b + 4),   # 천장
			(b + 0, b + 4, b + 5, b + 1),   # -y
			(b + 1, b + 5, b + 6, b + 2),   # +x
			(b + 2, b + 6, b + 7, b + 3),   # +y
			(b + 3, b + 7, b + 4, b + 0),   # -x
		], mat, smooth)

		return self

	def limb(self, a, b, r0, r1, mat, bone=None, weights=None, color=None, sides=4):
		"""두 점을 잇는 각기둥. 사지·나뭇가지처럼 방향이 있는 것에 쓴다.

		a, b : 시작·끝 점
		r0, r1: 시작·끝 반지름
		sides: 4 면 각진 막대, 6 이상이면 둥글어진다
		"""
		a = Vector(a)
		b = Vector(b)
		axis = b - a
		length = axis.length

		if length < 1e-6:
			return self

		# 축에 수직인 기저 두 개
		up = Vector((0.0, 0.0, 1.0))
		if abs(axis.normalized().dot(up)) > 0.99:
			up = Vector((0.0, 1.0, 0.0))

		u = axis.cross(up).normalized()
		v = axis.cross(u).normalized()

		ring_a = []
		ring_b = []

		for i in range(sides):
			t = (i + 0.5) / sides * math.tau
			d = u * math.cos(t) + v * math.sin(t)

			ring_a.append(a + d * r0)
			ring_b.append(b + d * r1)

		base = self._push_verts(list(ring_a) + list(ring_b), bone, weights, color)

		faces = []
		for i in range(sides):
			j = (i + 1) % sides

			faces.append((base + i, base + j, base + sides + j, base + sides + i))

		faces.append(tuple(base + i for i in range(sides - 1, -1, -1)))
		faces.append(tuple(base + sides + i for i in range(sides)))

		self._push_faces(faces, mat, sides > 5)

		return self

	def wedge(self, center, size, mat, axis='y', flip=False,
	          bone=None, weights=None, color=None):
		"""삼각 프리즘. 지붕·코·귀처럼 뾰족한 것."""
		cx, cy, cz = center
		sx, sy, sz = size
		hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5

		if axis == 'y':
			# y 방향으로 긴 삼각 프리즘 — 용마루가 y 축과 나란하다
			pts = [
				(-hx, -hy, -hz), (hx, -hy, -hz), (0.0, -hy, hz),
				(-hx,  hy, -hz), (hx,  hy, -hz), (0.0,  hy, hz),
			]
			faces = [
				(0, 1, 2), (5, 4, 3),
				(0, 2, 5, 3), (1, 4, 5, 2), (0, 3, 4, 1),
			]
		else:
			pts = [
				(-hx, -hy, -hz), (-hx, hy, -hz), (-hx, 0.0, hz),
				( hx, -hy, -hz), ( hx, hy, -hz), ( hx, 0.0, hz),
			]
			faces = [
				(2, 1, 0), (3, 4, 5),
				(0, 1, 4, 3), (1, 2, 5, 4), (2, 0, 3, 5),
			]

		if flip:
			pts = [(p[0], p[1], -p[2]) for p in pts]
			faces = [tuple(reversed(f)) for f in faces]

		pts = [(p[0] + cx, p[1] + cy, p[2] + cz) for p in pts]

		base = self._push_verts(pts, bone, weights, color)

		self._push_faces([tuple(base + i for i in f) for f in faces], mat, False)

		return self

	def ico(self, center, radius, mat, scale=(1.0, 1.0, 1.0),
	        bone=None, weights=None, color=None, smooth=True):
		"""저해상도 구. 머리·열매·돌멩이용. 12정점 20면 — 그 이상은 필요 없다."""
		t = (1.0 + math.sqrt(5.0)) / 2.0

		raw = [
			(-1,  t, 0), ( 1,  t, 0), (-1, -t, 0), ( 1, -t, 0),
			(0, -1,  t), (0,  1,  t), (0, -1, -t), (0,  1, -t),
			( t, 0, -1), ( t, 0,  1), (-t, 0, -1), (-t, 0,  1),
		]
		faces = [
			(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
			(1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
			(3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
			(4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1),
		]

		norm = 1.0 / math.sqrt(1.0 + t * t)
		pts = []

		for p in raw:
			pts.append((
				center[0] + p[0] * norm * radius * scale[0],
				center[1] + p[1] * norm * radius * scale[1],
				center[2] + p[2] * norm * radius * scale[2],
			))

		base = self._push_verts(pts, bone, weights, color)

		self._push_faces([tuple(base + i for i in f) for f in faces], mat, smooth)

		return self

	def plane_fan(self, center, radius, mat, sides=6, bone=None, color=None):
		"""납작한 다각형. 바닥 패치·이끼·물웅덩이."""
		pts = [center]

		for i in range(sides):
			t = i / sides * math.tau

			pts.append((center[0] + math.cos(t) * radius,
			            center[1] + math.sin(t) * radius,
			            center[2]))

		base = self._push_verts(pts, bone, None, color)
		faces = []

		for i in range(sides):
			j = (i + 1) % sides

			faces.append((base, base + 1 + i, base + 1 + j))

		self._push_faces(faces, mat, False)

		return self

	# ── 변환 ──────────────────────────────────────────

	def transform(self, matrix, start=0):
		"""start 번째 이후 정점을 한꺼번에 변환한다.

		같은 모양을 좌우 대칭으로 두 번 찍을 때 쓴다 — 먼저 한쪽을 쌓고,
		쌓기 전 인덱스를 기억해 두었다가 거울 행렬을 먹인다.
		"""
		for i in range(start, len(self.verts)):
			self.verts[i] = matrix @ self.verts[i]

		return self

	def mirror_x(self, start, swap_suffix=True):
		"""start 이후를 x 축으로 뒤집어 **복제**한다. _L / _R 본 이름도 바꾼다."""
		n_v = len(self.verts)
		n_f = len(self.faces)

		for i in range(start, n_v):
			v = self.verts[i]

			self.verts.append(Vector((-v.x, v.y, v.z)))
			self.vert_colors.append(self.vert_colors[i])

			bones = self.vert_bones[i]
			if swap_suffix:
				flipped = []

				for name, w in bones:
					if name.endswith("_L"):
						name = name[:-2] + "_R"
					elif name.endswith("_R"):
						name = name[:-2] + "_L"

					flipped.append((name, w))

				bones = flipped

			self.vert_bones.append(list(bones))

		offset = n_v - start

		# start 이후 정점만으로 이루어진 면이 복제 대상이다.
		# 면 인덱스가 아니라 **정점 인덱스**로 판정해야 한다 — 두 리스트의 길이는 다르다.
		for i in range(n_f):
			f = self.faces[i]

			if not all(idx >= start for idx in f):
				continue

			# 미러링하면 면의 감는 방향이 뒤집힌다 — 되돌려야 노멀이 바깥을 본다
			self.faces.append(tuple(reversed([idx + offset for idx in f])))
			self.face_mats.append(self.face_mats[i])
			self.smooth.append(self.smooth[i])

		return self

	def mark(self):
		"""지금까지 쌓인 정점 개수. mirror_x 에 넘길 기준점."""
		return len(self.verts)

	def _vertex_materials(self):
		"""정점마다 그 정점을 쓰는 면의 머티리얼. 정점 색을 배율로 바꿀 때 필요하다.

		블록 하나가 자기 정점을 따로 쌓으므로 한 정점이 두 머티리얼에 걸치는 일은 없다.
		혹시 걸치더라도 먼저 만난 쪽을 쓴다 — 어차피 변주용 색이라 치명적이지 않다.
		"""
		out = [None] * len(self.verts)

		for fi, f in enumerate(self.faces):
			mat = self.face_mats[fi]

			for vi in f:
				if out[vi] is None:
					out[vi] = mat

		# 어느 면에도 안 쓰인 정점 — 있어선 안 되지만 KeyError 로 죽지는 않게
		for i, m in enumerate(out):
			if m is None:
				out[i] = self.face_mats[0] if self.face_mats else 'fix.wood_oak'

		return out

	# ── 굽기 ──────────────────────────────────────────

	def bake(self, name, bevel=0.0):
		"""쌓아 둔 것을 bpy 오브젝트로 만든다."""
		mesh = bpy.data.meshes.new(name)
		mesh.from_pydata([tuple(v) for v in self.verts], [], self.faces)
		mesh.update()

		# 머티리얼 슬롯 — 쓰인 순서대로
		slot_index = {}

		for mat_name in self.face_mats:
			if mat_name not in slot_index:
				slot_index[mat_name] = len(slot_index)
				mesh.materials.append(make_material(mat_name))

		for i, poly in enumerate(mesh.polygons):
			poly.material_index = slot_index[self.face_mats[i]]
			poly.use_smooth = self.smooth[i]

		obj = bpy.data.objects.new(name, mesh)
		link(obj)

		# 버텍스 컬러 — 한 정점이라도 색이 지정됐으면 만든다.
		#
		# **절대색이 아니라 배율을 저장한다.** glTF 에서 COLOR_0 은 baseColorFactor 에
		# 곱해지므로 (최종색 = baseColorFactor × COLOR_0), 절대색을 넣으면 색이 두 번
		# 곱해져 거의 검게 죽는다. 그래서 굽는 시점에 머티리얼 색으로 나눠 배율로 바꾼다.
		#
		# 내보낼 때 정규화 정수가 되어 1.0 을 넘지 못하므로 1.0 에서 자른다 —
		# 기준색보다 밝게 칠하려 했다면 그건 머티리얼 쪽을 올려야 할 일이다.
		if any(c is not None for c in self.vert_colors):
			attr = mesh.color_attributes.new(name="Color", type='FLOAT_COLOR', domain='POINT')

			vert_mat = self._vertex_materials()
			clipped = 0

			for i, c in enumerate(self.vert_colors):
				if c is None:
					attr.data[i].color = (1.0, 1.0, 1.0, 1.0)

					continue

				want = palette.lin(c)
				base = palette.lin(palette.MATERIALS[vert_mat[i]][0])
				mult = []

				for k in range(3):
					m = want[k] / max(base[k], 1e-4)

					if m > 1.0 + 1e-3:
						clipped += 1
						m = 1.0

					mult.append(max(0.0, m))

				attr.data[i].color = (mult[0], mult[1], mult[2], 1.0)

			if clipped:
				print("    [주의] %s: 머티리얼보다 밝은 정점 색 %d 채널이 1.0 에서 잘렸다"
				      % (name, clipped))

		# 본 가중치 → 버텍스 그룹
		groups = {}

		for i, pairs in enumerate(self.vert_bones):
			for bone_name, w in pairs:
				if bone_name not in groups:
					groups[bone_name] = obj.vertex_groups.new(name=bone_name)

				groups[bone_name].add([i], w, 'REPLACE')

		if bevel > 0.0:
			_bevel(obj, bevel)

		mesh.validate(verbose=False)

		return obj


def _bevel(obj, width):
	"""아주 얕은 베벨. 로우폴리 모서리가 조명을 받아 선처럼 살아난다.

	bmesh.ops.bevel 의 인자 이름이 버전마다 달라서 실패해도 그냥 넘어간다 —
	베벨은 있으면 좋은 것이지 없으면 안 되는 것이 아니다.
	"""
	bm = bmesh.new()
	bm.from_mesh(obj.data)

	try:
		bmesh.ops.bevel(bm,
		                geom=list(bm.verts) + list(bm.edges),
		                offset=width,
		                offset_type='OFFSET',
		                segments=1,
		                profile=0.5,
		                affect='EDGES')
		bm.to_mesh(obj.data)
	except TypeError:
		pass
	finally:
		bm.free()

	obj.data.update()


# ────────────────────────────────────────────────────────────
#  스키닝
# ────────────────────────────────────────────────────────────

def bind(obj, armature):
	"""이미 만들어 둔 버텍스 그룹을 그대로 쓰는 아마튜어 바인딩.

	bpy.ops.object.parent_set(type='ARMATURE_AUTO') 는 가중치를 자동 계산하면서
	우리가 넣은 리지드 가중치를 **덮어쓴다.** 그래서 모디파이어를 직접 건다.
	"""
	obj.parent = armature
	obj.matrix_parent_inverse = armature.matrix_world.inverted()

	mod = obj.modifiers.new("Armature", 'ARMATURE')
	mod.object = armature
	mod.use_vertex_groups = True

	return obj


def tri_count(obj):
	"""삼각형 환산 개수. 폴리곤 예산 확인용."""
	n = 0

	for poly in obj.data.polygons:
		n += max(0, len(poly.vertices) - 2)

	return n
