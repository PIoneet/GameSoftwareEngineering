#include "stdafx.h"
#include "Story.h"

// 원형 표 (STORY.md §2-5-1).
//
// 성향은 중앙값이다. 개체는 여기서 흩어진 값을 시드로 받는다 —
// 같은 산적이라도 겁이 많은 놈과 없는 놈이 섞여 있어야 무리가 살아 보인다.
//
//                    label          faction      faith fear  aggr  loyal motive wander sight armed hp
static const ArchetypeDef ARCHETYPES[AT_COUNT] =
{
	{ L"주민",       FC_NONE,     0.45f, 0.50f, 0.15f, 0.50f, 0.00f, 1.6f, 5.0f, false, 10 },
	{ L"농부",       FC_NONE,     0.40f, 0.65f, 0.10f, 0.55f, 0.00f, 1.2f, 4.5f, false, 12 },
	{ L"상인",       FC_NONE,     0.30f, 0.55f, 0.15f, 0.25f, 0.00f, 1.0f, 5.0f, false, 10 },
	{ L"대장장이",   FC_NONE,     0.35f, 0.30f, 0.55f, 0.60f, 0.10f, 0.9f, 6.0f, true,  18 },
	{ L"성직자",     FC_NONE,     0.85f, 0.35f, 0.10f, 0.70f, 0.00f, 1.0f, 6.0f, false, 12 },
	{ L"성인",       FC_NONE,     1.00f, 0.10f, 0.00f, 0.90f, 0.00f, 0.8f, 8.0f, false, 14 },
	{ L"자경단",     FC_NONE,     0.40f, 0.60f, 0.70f, 0.45f, 0.35f, 2.2f, 7.0f, true,  16 },
	{ L"산적",       FC_BANDIT,   0.10f, 0.25f, 0.90f, 0.35f, 0.75f, 2.8f, 8.0f, true,  20 },
	{ L"신도",       FC_CHOIR,    0.95f, 0.30f, 0.60f, 0.85f, 0.45f, 1.8f, 7.0f, true,  16 },
	{ L"수도회 병사", FC_SILENCE, 0.50f, 0.20f, 0.55f, 0.90f, 0.30f, 3.2f, 8.0f, true,  22 },
	{ L"약초꾼",     FC_NONE,     0.30f, 0.35f, 0.05f, 0.40f, 0.00f, 0.9f, 5.0f, false, 10 },
	{ L"외지 학자",  FC_NONE,     0.15f, 0.70f, 0.05f, 0.30f, 0.00f, 1.1f, 5.0f, false,  8 },
};

const ArchetypeDef& ArchetypeInfo(int at)
{
	if (at < 0 || at >= AT_COUNT)
		at = AT_VILLAGER;

	return ARCHETYPES[at];
}

bool FactionHostile(int a, int b)
{
	// 같은 편끼리는 싸우지 않는다. 주민끼리(둘 다 FC_NONE)도 여기서 걸린다.
	if (a == b)
		return false;

	// 꿈의 부산물은 전원과 적대한다.
	if (a == FC_MONSTER || b == FC_MONSTER)
		return true;

	// 산적은 교리가 없다. 뺏을 것이 있으면 누구든 적이다.
	if (a == FC_BANDIT || b == FC_BANDIT)
		return true;

	// 성가대와 수도회 — 3자 교전의 나머지 한 축 (§14-3).
	if (a == FC_CHOIR   && b == FC_SILENCE) return true;
	if (a == FC_SILENCE && b == FC_CHOIR)   return true;

	return false;
}
