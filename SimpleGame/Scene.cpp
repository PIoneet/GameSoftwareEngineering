#include "stdafx.h"
#include "Scene.h"
#include "IsoMath.h"

#include <algorithm>

namespace
{
	bool DepthLess(const Actor* a, const Actor* b)
	{
		return a->Depth() < b->Depth();
	}

	// 부모가 죽으면 서브트리도 함께 죽인다.
	// 그러지 않으면 자식이 트리에서 떨어진 채 목록에만 남아,
	// 갱신도 렌더도 되지 않는 유령이 된다.
	void CascadeDeath(Actor* a, bool parentDead)
	{
		bool dead = parentDead || !a->alive;

		if (dead)
			a->alive = false;

		const std::vector<Actor*>& kids = a->Children();

		for (size_t i = 0; i < kids.size(); ++i)
			CascadeDeath(kids[i], dead);
	}
}

Scene::Scene()
{
	m_Root = new Actor();
	m_All.push_back(m_Root);

	m_Visible.reserve(4096);
}

Scene::~Scene()
{
	Clear();

	delete m_Root;
	m_Root = 0;
}

void Scene::Clear()
{
	// 루트는 남기고 나머지를 지운다. 레벨을 갈아끼울 때 쓴다.
	for (size_t i = 0; i < m_All.size(); ++i)
	{
		if (m_All[i] != m_Root)
			delete m_All[i];
	}

	m_All.clear();
	m_Visible.clear();

	if (m_Root != 0)
		m_All.push_back(m_Root);

	m_DrawnLastFrame = 0;
}

Actor* Scene::Add(Actor* actor, Actor* parent)
{
	if (actor == 0)
		return 0;

	actor->Attach(parent != 0 ? parent : m_Root);
	m_All.push_back(actor);

	return actor;
}

void Scene::UpdateRecursive(Actor* a, float dt, const UpdateContext& ctx)
{
	// 정적 액터는 자신도 자식도 움직이지 않는다. 서브트리째 건너뛴다.
	// 마을 나무와 폐허 벽이 수천 개라 이 가지치기가 실제로 값을 한다.
	if (a->IsStatic())
		return;

	a->Update(dt, ctx);

	const std::vector<Actor*>& kids = a->Children();

	for (size_t i = 0; i < kids.size(); ++i)
		UpdateRecursive(kids[i], dt, ctx);
}

void Scene::Update(float dt, const UpdateContext& ctx)
{
	UpdateRecursive(m_Root, dt, ctx);

	// 갱신이 끝난 뒤 한 번에 월드 좌표를 내린다.
	m_Root->PropagateTransform(0.0f, 0.0f, 0.0f);
}

void Scene::Collect(Actor* a, const RenderContext& ctx)
{
	// 부모를 숨기면 서브트리가 통째로 숨는다.
	if (!a->visible)
		return;

	// 화면 밖이어도 자식은 보일 수 있으므로, 컬링으로는 가지치기하지 않는다.
	float sx = ctx.originX + IsoScreenX(a->WorldX(), a->WorldY());
	float sy = ctx.originY + IsoScreenY(a->WorldX(), a->WorldY(), a->WorldZ() * TILE_H);

	bool onScreen = sx > -a->cullPad
	             && sx < (float)ctx.winW + a->cullPad
	             && sy > -a->cullPad * 2.0f
	             && sy < (float)ctx.winH + a->cullPad;

	if (onScreen)
		m_Visible.push_back(a);

	const std::vector<Actor*>& kids = a->Children();

	for (size_t i = 0; i < kids.size(); ++i)
		Collect(kids[i], ctx);
}

void Scene::Render(const RenderContext& ctx)
{
	m_Visible.clear();

	Collect(m_Root, ctx);

	// painter's algorithm — 깊이가 작은 것(뒤)부터 그린다.
	std::sort(m_Visible.begin(), m_Visible.end(), DepthLess);

	for (size_t i = 0; i < m_Visible.size(); ++i)
		m_Visible[i]->Render(ctx);

	m_DrawnLastFrame = (int)m_Visible.size();
}

void Scene::QueryNear(float wx, float wy, float radius, int kind,
                      std::vector<Actor*>& out) const
{
	out.clear();

	float r2 = radius * radius;

	for (size_t i = 0; i < m_All.size(); ++i)
	{
		Actor* a = m_All[i];

		if (!a->alive)
			continue;

		if (kind != ACTOR_GENERIC && a->Kind() != kind)
			continue;

		float dx = a->WorldX() - wx;
		float dy = a->WorldY() - wy;

		if (dx * dx + dy * dy <= r2)
			out.push_back(a);
	}
}

Actor* Scene::FindNearest(float wx, float wy, float radius, int kind) const
{
	Actor* best  = 0;
	float  bestD = radius * radius;

	for (size_t i = 0; i < m_All.size(); ++i)
	{
		Actor* a = m_All[i];

		if (!a->alive)
			continue;

		if (kind != ACTOR_GENERIC && a->Kind() != kind)
			continue;

		float dx = a->WorldX() - wx;
		float dy = a->WorldY() - wy;
		float d2 = dx * dx + dy * dy;

		if (d2 > bestD)
			continue;

		bestD = d2;
		best  = a;
	}

	return best;
}

void Scene::Reap()
{
	bool any = false;

	for (size_t i = 0; i < m_All.size(); ++i)
	{
		if (!m_All[i]->alive)
		{
			any = true;
			break;
		}
	}

	if (!any)
		return;

	CascadeDeath(m_Root, false);

	std::vector<Actor*> kept;
	kept.reserve(m_All.size());

	for (size_t i = 0; i < m_All.size(); ++i)
	{
		Actor* a = m_All[i];

		// 루트는 죽지 않는다.
		if (a->alive || a == m_Root)
		{
			kept.push_back(a);
			continue;
		}

		// 소멸자가 부모와 자식 관계를 정리한다.
		delete a;
	}

	m_All.swap(kept);
}
