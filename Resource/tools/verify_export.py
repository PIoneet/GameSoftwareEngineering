# -*- coding: utf-8 -*-
"""구워진 glTF 가 FORMAT.md 의 약속대로 나왔는지 검사한다.

    python tools/verify_export.py        (Blender 없이 돈다)

`check.py` 가 **굽기 전** 정의를 보는 검사라면, 이쪽은 **구워진 결과**를 본다.
둘은 보는 것이 다르다 — 정의가 맞아도 내보내기 설정 하나로 결과가 틀어질 수 있고,
실제로 그런 일이 있었다 (정점 색이 COLOR_0/COLOR_1 두 벌로 나가거나, 아예 빠지거나).

검사 항목은 FORMAT.md 1절 "로더가 지원해야 하는 glTF 기능 범위" 그대로다.
여기서 통과하는 것만 로더가 구현하면 된다.
"""

import base64
import json
import os
import struct
import sys


HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

if HERE not in sys.path:
	sys.path.insert(0, HERE)

from common import palette


# FORMAT.md 1절에서 쓰기로 한 것만
ALLOWED_ATTRS = {'POSITION', 'NORMAL', 'COLOR_0', 'JOINTS_0', 'WEIGHTS_0'}
ALLOWED_MODES = {4, None}          # TRIANGLES
ALLOWED_INTERP = {'LINEAR', None}

# 값이 전 구간 같은 곡선은 내보내기가 STEP 으로 쓴다 (보간할 것이 없으므로).
# LINEAR 로 읽어도 결과가 같으니 허용한다 — 다만 **정말 상수인지 확인한다.**
CONSTANT_OK_INTERP = {'STEP'}

# 쓰기로 한 유일한 확장. emissiveFactor 에 곱할 배율 하나만 들어 있고,
# extensionsRequired 가 아니므로 무시해도 조금 덜 밝게 렌더될 뿐이다.
ALLOWED_EXTENSIONS = {'KHR_materials_emissive_strength'}
ALLOWED_PATHS = {'translation', 'rotation', 'scale'}

COMPONENT = {
	5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2),
	5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4),
}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}

FAIL = []
WARN = []


def fail(msg):
	FAIL.append(msg)
	print("  [X] " + msg)


def warn(msg):
	WARN.append(msg)
	print("  [!] " + msg)


# ────────────────────────────────────────────────────────────
#  버퍼 읽기
# ────────────────────────────────────────────────────────────

def load_buffers(doc, base_dir):
	out = []

	for b in doc.get('buffers', []):
		uri = b.get('uri')

		if uri is None:
			out.append(b'')
		elif uri.startswith('data:'):
			out.append(base64.b64decode(uri.split(',', 1)[1]))
		else:
			with open(os.path.join(base_dir, uri), 'rb') as f:
				out.append(f.read())

	return out


def read_accessor(doc, buffers, index):
	acc = doc['accessors'][index]
	n = NCOMP[acc['type']]
	fmt, size = COMPONENT[acc['componentType']]
	count = acc['count']

	if 'bufferView' not in acc:
		return [tuple([0] * n) for _ in range(count)]

	bv = doc['bufferViews'][acc['bufferView']]
	buf = buffers[bv.get('buffer', 0)]
	start = bv.get('byteOffset', 0) + acc.get('byteOffset', 0)
	stride = bv.get('byteStride') or (n * size)

	rows = []

	for i in range(count):
		off = start + i * stride
		vals = struct.unpack_from('<' + fmt * n, buf, off)

		if acc.get('normalized'):
			if fmt == 'B':
				vals = tuple(v / 255.0 for v in vals)
			elif fmt == 'H':
				vals = tuple(v / 65535.0 for v in vals)

		rows.append(vals)

	return rows


# ────────────────────────────────────────────────────────────
#  파일 하나 검사
# ────────────────────────────────────────────────────────────

def verify(path):
	rel = os.path.relpath(path, ROOT).replace(os.sep, '/')

	with open(path, encoding='utf-8') as f:
		doc = json.load(f)

	base_dir = os.path.dirname(path)
	buffers = load_buffers(doc, base_dir)

	if doc.get('asset', {}).get('version') != '2.0':
		fail("%s: glTF 버전이 2.0 이 아니다" % rel)

	# ── 확장 ──
	used = set(doc.get('extensionsUsed', []))
	required = set(doc.get('extensionsRequired', []))

	unexpected = used - ALLOWED_EXTENSIONS

	if unexpected:
		fail("%s: 약속 밖 확장 — %s (FORMAT.md 1절)" % (rel, ", ".join(sorted(unexpected))))

	if required:
		fail("%s: 필수 확장이 있다 — 무시할 수 없으므로 로더가 구현해야 한다: %s"
		     % (rel, ", ".join(sorted(required))))

	# ── 메쉬 / 프리미티브 ──
	attrs_seen = set()
	tri_total = 0

	for m in doc.get('meshes', []):
		for pi, pr in enumerate(m['primitives']):
			a = set(pr.get('attributes', {}))
			attrs_seen |= a

			extra = a - ALLOWED_ATTRS

			if extra:
				fail("%s: %s 프리미티브 %d 에 약속 밖 속성 — %s"
				     % (rel, m['name'], pi, ", ".join(sorted(extra))))

			if pr.get('mode', 4) not in ALLOWED_MODES:
				fail("%s: %s 가 TRIANGLES 가 아니다 (mode=%s)"
				     % (rel, m['name'], pr.get('mode')))

			if 'POSITION' not in a:
				fail("%s: %s 에 POSITION 이 없다" % (rel, m['name']))

			if 'indices' in pr:
				acc = doc['accessors'][pr['indices']]

				if acc['componentType'] not in (5123, 5125):
					fail("%s: %s 인덱스 타입이 ushort/uint 가 아니다" % (rel, m['name']))

				tri_total += acc['count'] // 3

			# 스키닝은 쌍으로 있어야 한다
			if ('JOINTS_0' in a) != ('WEIGHTS_0' in a):
				fail("%s: %s 의 JOINTS_0/WEIGHTS_0 가 짝이 안 맞는다" % (rel, m['name']))

	# ── 이름 규칙 ──
	for m in doc.get('meshes', []):
		nm = m.get('name', '')

		if nm != nm.lower() or ' ' in nm:
			fail("%s: 메쉬 이름이 규칙에 안 맞는다 — '%s'" % (rel, nm))

	# ── 스킨 ──
	for sk in doc.get('skins', []):
		if 'inverseBindMatrices' not in sk:
			fail("%s: 스킨에 inverseBindMatrices 가 없다" % rel)
		else:
			ibm = doc['accessors'][sk['inverseBindMatrices']]

			if ibm['type'] != 'MAT4':
				fail("%s: inverseBindMatrices 가 MAT4 가 아니다" % rel)

			if ibm['count'] != len(sk['joints']):
				fail("%s: IBM 개수(%d)와 조인트 수(%d)가 다르다"
				     % (rel, ibm['count'], len(sk['joints'])))

	# ── 애니메이션 ──
	node_names = [n.get('name', '') for n in doc.get('nodes', [])]
	anim_report = []

	for an in doc.get('animations', []):
		name = an.get('name', '?')
		longest = 0.0

		bad_interp = set()
		step_varying = 0

		for s in an['samplers']:
			interp = s.get('interpolation', 'LINEAR')

			if interp not in ALLOWED_INTERP:
				if interp in CONSTANT_OK_INTERP:
					# 상수 곡선일 때만 봐준다. 값이 변하는데 STEP 이면 동작이 끊긴다
					rows = read_accessor(doc, buffers, s['output'])

					if any(r != rows[0] for r in rows[1:]):
						step_varying += 1
				else:
					bad_interp.add(interp)

			t_in = doc['accessors'][s['input']]
			t_out = doc['accessors'][s['output']]

			if t_in['count'] != t_out['count']:
				fail("%s/%s: 샘플러 입력(%d)과 출력(%d) 개수가 다르다"
				     % (rel, name, t_in['count'], t_out['count']))

			longest = max(longest, t_in.get('max', [0.0])[0])

		if bad_interp:
			fail("%s/%s: 보간이 LINEAR 가 아니다 — %s (샘플러 %d 개)"
			     % (rel, name, ", ".join(sorted(bad_interp)), len(an['samplers'])))

		if step_varying:
			fail("%s/%s: 값이 변하는데 STEP 보간인 샘플러가 %d 개 — 동작이 끊긴다"
			     % (rel, name, step_varying))

		for ch in an['channels']:
			p = ch['target'].get('path')

			if p not in ALLOWED_PATHS:
				fail("%s/%s: 약속 밖 채널 경로 — %s" % (rel, name, p))

			ni = ch['target'].get('node')

			if ni is None or ni >= len(node_names):
				fail("%s/%s: 채널이 가리키는 노드가 없다" % (rel, name))

		anim_report.append((name, longest, len(an['channels'])))

	# ── 정점 색 ──
	color_note = "없음"

	flat_meshes = []
	g_lo, g_hi = 1.0, 1.0

	for m in doc.get('meshes', []):
		lo, hi = None, None

		for pr in m['primitives']:
			if 'COLOR_0' not in pr['attributes']:
				continue

			cols = read_accessor(doc, buffers, pr['attributes']['COLOR_0'])
			mx = max(max(c[:3]) for c in cols)
			mn = min(min(c[:3]) for c in cols)

			if mx > 1.0001:
				fail("%s: %s 의 COLOR_0 이 1.0 을 넘는다 (%.3f)" % (rel, m['name'], mx))

			lo = mn if lo is None else min(lo, mn)
			hi = mx if hi is None else max(hi, mx)

		if lo is None:
			continue

		g_lo, g_hi = min(g_lo, lo), max(g_hi, hi)

		# 메쉬 전체가 1.0 이면 변주가 통째로 사라진 것이다.
		# (한 메쉬 안에서 잎만 흔들고 줄기는 안 흔드는 건 정상이라 프리미티브 단위로는 안 본다)
		if hi - lo < 1e-4 and abs(hi - 1.0) < 1e-4:
			flat_meshes.append(m['name'])

		color_note = "%.2f~%.2f" % (g_lo, g_hi)

	if flat_meshes:
		warn("%s: 정점 색 변주가 전혀 없는 메쉬 %d 개 — %s"
		     % (rel, len(flat_meshes), ", ".join(flat_meshes[:4])))

	print("  %-36s 메쉬 %2d  삼각형 %5d  스킨 %d  애니 %2d  색배율 %s"
	      % (rel, len(doc.get('meshes', [])), tri_total,
	         len(doc.get('skins', [])), len(doc.get('animations', [])), color_note))

	if attrs_seen:
		print("  %-36s 속성 %s" % ("", ", ".join(sorted(attrs_seen))))

	return doc, buffers, anim_report


# ────────────────────────────────────────────────────────────
#  색 복원 — baseColorFactor × COLOR_0 이 의도한 색인가
# ────────────────────────────────────────────────────────────

def lin_to_srgb(c):
	if c <= 0.0031308:
		return c * 12.92

	return 1.055 * (c ** (1.0 / 2.4)) - 0.055


def verify_colors(path):
	with open(path, encoding='utf-8') as f:
		doc = json.load(f)

	buffers = load_buffers(doc, os.path.dirname(path))

	print("\n[정점 색 복원] 최종색 = baseColorFactor × COLOR_0 이 팔레트와 맞는가")

	for m in doc['meshes']:
		pr = m['primitives'][0]

		if 'COLOR_0' not in pr['attributes']:
			continue

		mat = doc['materials'][pr['material']]
		bc = mat['pbrMetallicRoughness']['baseColorFactor']
		cols = read_accessor(doc, buffers, pr['attributes']['COLOR_0'])

		lo = [min(lin_to_srgb(bc[k] * c[k]) for c in cols) for k in range(3)]
		hi = [max(lin_to_srgb(bc[k] * c[k]) for c in cols) for k in range(3)]

		base_srgb = [lin_to_srgb(bc[k]) for k in range(3)]

		# 기준색을 넘어서면 안 되고, 변주가 0 이어도 안 된다
		over = any(hi[k] > base_srgb[k] + 0.002 for k in range(3))
		spread = max(hi[k] - lo[k] for k in range(3))

		if over:
			fail("%s: 복원한 색이 머티리얼 기준(%s)을 넘는다 — %s"
			     % (m['name'], [round(c, 3) for c in base_srgb], [round(c, 3) for c in hi]))

		if spread < 0.004:
			warn("%s: 색 변주 폭이 %.4f 로 거의 없다" % (m['name'], spread))

		print("    %-24s 기준 %-22s 복원 %s ~ %s"
		      % (m['name'],
		         str([round(c, 3) for c in base_srgb]),
		         str([round(c, 3) for c in lo]),
		         str([round(c, 3) for c in hi])))


# ────────────────────────────────────────────────────────────

def main():
	print("구워진 glTF 검증 — FORMAT.md 1절 기준\n")

	paths = []

	for sub in ('characters', 'creatures', 'props', 'nature'):
		d = os.path.join(ROOT, 'export', sub)

		if not os.path.isdir(d):
			continue

		for f in sorted(os.listdir(d)):
			if f.endswith('.gltf'):
				paths.append(os.path.join(d, f))

	if not paths:
		print("export/ 에 .gltf 가 없다. 먼저 Blender 로 구워야 한다:")
		print("    blender --background --python tools/build_all.py")

		return 1

	anims = {}

	for p in paths:
		doc, buffers, report = verify(p)

		if report:
			anims[os.path.basename(p)] = report

	print("\n[애니메이션] 클립 길이와 채널 수")

	for fname, report in anims.items():
		print("  %s" % fname)

		for name, dur, nch in sorted(report):
			print("    %-12s %5.2fs  채널 %3d" % (name, dur, nch))

	# 설계값과 대조
	graph_path = os.path.join(ROOT, 'data', 'anim_graph.json')

	if os.path.isfile(graph_path):
		with open(graph_path, encoding='utf-8') as f:
			graph = json.load(f)

		print("\n[대조] anim_graph.json 의 길이와 실제 glTF 샘플러 길이")

		pairs = [('humanoid.gltf', graph['humanoid']['clips'])]

		for fam in ('feline', 'canine', 'cervid'):
			pairs.append(('%s.gltf' % fam,
			              graph['quadruped']['families'][fam]['clips']))

		for fname, clips in pairs:
			report = anims.get(fname)

			if not report:
				continue

			for name, dur, nch in report:
				ref = clips.get(name)

				if ref is None:
					fail("%s: anim_graph 에 '%s' 가 없다" % (fname, name))

					continue

				# 마지막 키프레임 시각이므로 (frames/fps) 와 같아야 한다
				if abs(ref['duration'] - dur) > 1.0 / 30.0 + 1e-6:
					fail("%s/%s: 길이 불일치 — 문서 %.3f vs glTF %.3f"
					     % (fname, name, ref['duration'], dur))

		print("  전부 일치")

	verify_colors(os.path.join(ROOT, 'export', 'nature', 'terrain.gltf'))

	print("\n" + "=" * 62)

	if WARN:
		print("경고 %d 건" % len(WARN))

	if FAIL:
		print("실패 %d 건" % len(FAIL))

		for f in FAIL:
			print("  - " + f)

		return 1

	print("전부 통과 — 로더는 POSITION/NORMAL/COLOR_0/JOINTS_0/WEIGHTS_0 와")
	print("            LINEAR 보간 TRS 애니메이션만 구현하면 된다")

	return 0


if __name__ == '__main__':
	sys.exit(main())
