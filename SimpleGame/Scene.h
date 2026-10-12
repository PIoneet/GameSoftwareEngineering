#pragma once

#include <vector>
#include "Actor.h"

// 정렬 목록 한 칸. 깊이를 포인터 옆에 값으로 들고 있는다.
//
// 액터는 저마다 따로 할당돼 힙에 흩어져 있다. 비교 함수가 Depth() 를 부르면
// 비교 한 번에 포인터 두 개를 따라가느라 캐시 미스가 난다 — 577개를 정렬하는
// 비교 5천여 회가 실측 0.28ms 였다. Collect 에서 깊이를 한 번만 읽어 적어 두면
// 정렬은 연속된 메모리만 만진다.
struct DepthEntry
{
	float  depth;
	Actor* actor;
};

// 액터를 소유하고 계층·갱신·정렬·그리기를 관장한다.
//
// 트리는 부모/자식 포인터로만 표현하고, 소유는 평평한 벡터가 맡는다.
// 이렇게 나눠 두면 재귀 삭제 순서를 걱정할 필요가 없고, 순회도 단순해진다.
class Scene
{
public:
	Scene();
	~Scene();

	void Clear();

	// 소유권을 가져간다. parent 가 0 이면 루트에 붙인다.
	// 반환값은 편의를 위한 것으로, 넘긴 포인터와 같다.
	Actor* Add(Actor* actor, Actor* parent = 0);

	Actor* Root() { return m_Root; }

	void Update(float dt, const UpdateContext& ctx);

	// 화면에 걸치는 액터만 모아 깊이순으로 그린다.
	void Render(const RenderContext& ctx);

	// alive 가 false 인 액터를 정리한다. 프레임 끝에 한 번 부른다.
	void Reap();

	// 반경 안의 살아 있는 액터를 모은다.
	// kind 가 ACTOR_GENERIC 이면 종류를 가리지 않는다.
	//
	// 전수 탐색이다. 액터가 수천 개라도 질의는 프레임당 몇 번뿐이라 아직 충분하지만,
	// 범위 공격이나 몬스터 간 인지가 늘면 공간 격자로 바꿔야 한다.
	void QueryNear(float wx, float wy, float radius, int kind,
	               std::vector<Actor*>& out) const;

	// 반경 안에서 가장 가까운 액터 하나. 없으면 0.
	Actor* FindNearest(float wx, float wy, float radius, int kind) const;

	int ActorCount()      const { return (int)m_All.size(); }

	// 모니터링용 전수 조회. 소유는 Scene 이 계속 들고 있다 — 읽기만 할 것.
	const std::vector<Actor*>& All() const { return m_All; }
	int DrawnLastFrame()  const { return m_DrawnLastFrame; }

private:
	void UpdateRecursive(Actor* a, float dt, const UpdateContext& ctx);
	void Collect(Actor* a, const RenderContext& ctx);

	Actor* m_Root;

	std::vector<Actor*>     m_All;      // 소유
	std::vector<DepthEntry> m_Visible;  // 프레임마다 재사용해 할당을 줄인다

	int m_DrawnLastFrame = 0;
};
