#include "stdafx.h"
#include "Actor.h"

Actor::Actor()
{
}

Actor::~Actor()
{
	// 부모의 자식 목록에서 자신을 지운다. 자식은 Scene 이 따로 소유하므로
	// 여기서 지우지 않는다 — 소유와 계층을 분리해 둔 이유다.
	Detach();

	for (size_t i = 0; i < m_Children.size(); ++i)
		m_Children[i]->m_Parent = 0;

	m_Children.clear();
}

void Actor::Attach(Actor* parent)
{
	if (parent == this || parent == m_Parent)
		return;

	Detach();

	m_Parent = parent;

	if (parent != 0)
		parent->m_Children.push_back(this);
}

void Actor::Detach()
{
	if (m_Parent == 0)
		return;

	std::vector<Actor*>& siblings = m_Parent->m_Children;

	for (size_t i = 0; i < siblings.size(); ++i)
	{
		if (siblings[i] != this)
			continue;

		siblings[i] = siblings.back();
		siblings.pop_back();

		break;
	}

	m_Parent = 0;
}

void Actor::PropagateTransform(float px, float py, float pz)
{
	m_WorldX = px + x;
	m_WorldY = py + y;
	m_WorldZ = pz + z;

	for (size_t i = 0; i < m_Children.size(); ++i)
		m_Children[i]->PropagateTransform(m_WorldX, m_WorldY, m_WorldZ);
}

void Actor::Update(float dt, const UpdateContext& ctx)
{
	// 기본 액터는 스스로 움직이지 않는다.
	(void)dt;
	(void)ctx;
}

void Actor::Render(const RenderContext& ctx) const
{
	// 기본 액터는 보이지 않는다. 그룹 노드로 쓰기 위한 것이다.
	(void)ctx;
}
