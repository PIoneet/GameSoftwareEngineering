#pragma once

// docs/STORY.md 의 데이터 정의를 코드로 옮긴 것.
//
// 규칙(STORY.md §0-2): 여기 있는 ID 는 문서와 이름이 같아야 한다.
// 새 ID 가 필요하면 문서를 먼저 고치고 여기로 옮긴다.
//
// 이 파일은 "무엇이 존재하는가" 만 담는다. 판단은 Actors.cpp 가 하고,
// 월드 상태는 HiddenState 가 들고 있다.

// ── 세력 (§2-2) ──
enum FactionId
{
	FC_NONE,
	FC_PARTY,
	FC_CHOIR,      // 심연의 성가대 — 깨워서 기억하라
	FC_SILENCE,    // 침묵의 수도회 — 기억하는 자를 없애라
	FC_BANDIT,     // 까마귀발 산적단 — 교리 없음
	FC_MONSTER
};

// ── 보호 등급 (§2-5) ──
enum ProtectionTier
{
	TIER_T0,   // 불사
	TIER_T1,   // 동료 — 기절만
	TIER_T2,   // 핵심 NPC — 죽으면 역할 대체
	TIER_T3    // 일반 — 자유롭게 죽는다
};

// ── NPC 원형 (§2-5-1) ──
enum NpcArchetype
{
	AT_VILLAGER,
	AT_FARMER,
	AT_MERCHANT,
	AT_SMITH,
	AT_CLERIC,
	AT_SAINT,
	AT_VIGILANTE,
	AT_BANDIT,
	AT_CULTIST,
	AT_SOLDIER,
	AT_HERBWIFE,
	AT_SCHOLAR,

	AT_COUNT
};

// ── 행동 상태 (§14-2) ──
// 일상 → 경계 → 적대 → 공격 → (도주 | 사망)
enum AgentState
{
	AG_CALM,
	AG_ALERT,
	AG_HOSTILE,
	AG_ATTACK,
	AG_FLEE
};

// ── 개인 성향 (§13-2 `HV_TRAIT`) ──
// 각 0~1. 새 게임마다 시드로 뽑는다. 어떤 UI 에도 수치로 보이지 않는다.
struct Traits
{
	float faith;        // 신앙심
	float fear;         // 겁
	float aggression;   // 공격성
	float loyalty;      // 충성
};

// 원형이 정하는 기본값. 개체차는 시드가 만든다.
struct ArchetypeDef
{
	const wchar_t* label;
	int   faction;

	// 성향 중앙값. 실제 값은 여기서 ±0.25 만큼 흩어진다.
	float faith, fear, aggression, loyalty;

	float motive;    // 남을 해칠 동기 기본값 (§14-7). 0 이면 결코 해치지 않는다.
	float wander;    // 배회 반경 (타일)
	float sight;     // 위협을 알아채는 거리 (타일)
	bool  armed;     // 무기를 들고 있는가
	int   hp;
};

// 이름이 NpcActor::Archetype() 멤버와 겹치면 멤버 함수 안에서 전역이 가려진다.
// (클래스 범위에서 조회가 멈춘다.) 그래서 이름을 달리 둔다.
const ArchetypeDef& ArchetypeInfo(int at);

// ── 인식 판정 (§1-1) ──
//
// 지워진 자를 볼 수 있는가. 렌더링은 항상 주인공 시점이므로 그리기에는 쓰지 않는다.
// 대화 대상 / AI 타깃 / 이름 표시 / 사망 파급 네 곳에서만 쓴다.
inline bool CanPerceive(bool observerTuned, bool observerAnchor, bool targetErased)
{
	if (!targetErased)   return true;
	if (!observerTuned)  return true;   // 외지인은 물들지 않았다
	if (observerAnchor)  return true;   // 기억의 끈

	return false;
}

// ── 숨은 변수 (§13) ──
//
// 플레이어는 이 수치의 존재도 모른다. UI 에 절대 내보내지 않는다.
// 신호(§13-3)로만 드러난다 — 문이 닫히고, 순찰이 늘고, 몬스터가 많아진다.
struct HiddenState
{
	int fear  = 10;   // HV_FEAR  마을 공포도   0~100
	int choir = 20;   // HV_CHOIR 교단 세력     0~100
	int stir  =  5;   // HV_STIR  뒤척임        0~100

	// 임계 효과 (§13-2)
	bool VigilanteActive() const { return fear  >= 60; }   // 자경단 결성
	bool ChoirKilling()    const { return choir >= 70; }   // 마을 내 암살
	bool MonstersRising()  const { return stir  >= 30; }   // 몬스터 증가
};

// 세력 적대 (§14-3). 같은 편끼리는 싸우지 않는다.
bool FactionHostile(int a, int b);
