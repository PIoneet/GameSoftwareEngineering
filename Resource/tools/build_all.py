# -*- coding: utf-8 -*-
"""전체 리소스를 한 번에 굽는다.

    blender --background --python tools/build_all.py

생성 스크립트를 차례로 돌리고 마지막에 조합 정보(anim_graph.json)를 갱신한다.
각 스크립트가 시작할 때 씬을 비우므로 (build.reset_scene) 한 프로세스에서
연달아 돌려도 서로 간섭하지 않는다.

Blender 가 없으면 `python tools/check.py` 로 정의만 검증할 수 있고,
`python tools/gen_anim_graph.py` 는 설계값으로 anim_graph.json 을 만들어 준다.
"""

import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

if HERE not in sys.path:
	sys.path.insert(0, HERE)


STEPS = [
	('gen_humanoid',  '휴머노이드 캐릭터'),
	('gen_quadruped', '4족 짐승'),
	('gen_props',     '주거 환경'),
	('gen_nature',    '자연 환경'),
	('gen_anim_graph', '애니메이션 조합 정보'),
]


def main():
	try:
		import bpy   # noqa: F401
	except ImportError:
		print("Blender 안에서 실행해야 한다:")
		print("    blender --background --python tools/build_all.py")

		return 1

	import importlib

	t0 = time.time()
	failed = []

	for name, label in STEPS:
		print("\n" + "=" * 60)
		print("  %s  (%s.py)" % (label, name))
		print("=" * 60)

		try:
			mod = importlib.import_module(name)
			mod.main()
		except Exception as exc:
			import traceback

			traceback.print_exc()
			failed.append((name, exc))

	print("\n" + "=" * 60)
	print("  %.1f 초" % (time.time() - t0))

	if failed:
		print("  실패 %d 건:" % len(failed))

		for name, exc in failed:
			print("    %s — %s" % (name, exc))

		return 1

	print("  전부 완료 → export/ · data/ · blend/")

	return 0


if __name__ == '__main__':
	sys.exit(main())
