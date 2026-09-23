#pragma once

#include "Actor.h"
#include "World.h"

// 구체 액터들.
//
// 마을과 폐허의 모든 배치물이 여기서 갈라진다. 공통 뼈대(위치·계층·정렬·컬링)는
// Actor 가 들고, 각자는 "무엇을 그리고 어떻게 움직이는가"만 채운다.

struct PersonLook;

// 외형 팔레트를 꺼낸다. 인덱스 14 가 플레이어다.
const PersonLook& LookAt(int index);


// ─────────────────────────────────────────────────────────────────────────
//  정적 배치물 — 나무, 집, 벽, 기둥, 상자 …
// ─────────────────────────────────────────────────────────────────────────

class PropActor : public Actor
{
public:
	explicit PropActor(const WorldObject& obj);

	int  Kind() const { return ACTOR_PROP; }
	void Render(const RenderContext& ctx) const;

	const WorldObject& Object() const { return m_Obj; }

	// 비석처럼 진행 상태에 따라 연출이 달라지는 프롭을 위한 창구
	bool sigilTaken     = false;
	bool sigilHighlight = false;

private:
	WorldObject m_Obj;
};


// ─────────────────────────────────────────────────────────────────────────
//  걷는 것들의 공통 뼈대
// ─────────────────────────────────────────────────────────────────────────

class CharacterActor : public Actor
{
public:
	float WalkPhase()  const { return m_WalkPhase; }
	float WalkAmount() const { return m_WalkAmount; }
	float Seed()       const { return m_Seed; }

	void SetSeed(float s) { m_Seed = s; }

protected:
	// 축을 나눠 밀어야 벽을 따라 미끄러진다. 실제로 움직였으면 true.
	bool MoveWithCollision(const World& world, float dx, float dy, float radius);

	// 화면 기준 진행 방향으로 좌우를 정한다.
	void FaceByMovement(float dx, float dy);

	void StepAnim(float dt, bool moving, float cycleSpeed = 9.0f);

	float m_WalkPhase  = 0.0f;
	float m_WalkAmount = 0.0f;
	float m_Seed       = 0.0f;
};


// ─────────────────────────────────────────────────────────────────────────
//  플레이어
// ─────────────────────────────────────────────────────────────────────────

class PlayerActor : public CharacterActor
{
public:
	PlayerActor();

	int  Kind() const { return ACTOR_PLAYER; }
	void Update(float dt, const UpdateContext& ctx);
	void Render(const RenderContext& ctx) const;

	// 몬스터가 부른다. 체력이 0 이 되면 입구에서 다시 선다.
	void Hurt(int amount, const UpdateContext& ctx);

	float HurtFlash() const { return m_HurtFlash; }

	bool combatEnabled = false;   // 폐허에서만 공격이 열린다
	bool frozen        = false;   // 대화창이 떠 있으면 멈춘다

	// 죽어서 입구로 돌아갔음을 알리는 일회성 신호.
	// 세우는 쪽과 지우는 쪽이 다르다 — Game 이 읽고 직접 내린다.
	// 액터 자신이 매 프레임 초기화하면, 몬스터가 먼저 갱신된 프레임에서
	// 신호가 읽히기 전에 지워진다.
	bool respawned = false;

	// 이 레벨에서 쓰러뜨린 수. 액터가 새로 만들어질 때마다 0 에서 시작한다.
	int kills = 0;

private:
	void DoAttack(const UpdateContext& ctx);

	float m_AttackCd   = 0.0f;
	float m_AttackAnim = -1.0f;   // 0~1 진행. 음수면 공격 중이 아님.
	float m_HurtFlash  = 0.0f;
};


// ─────────────────────────────────────────────────────────────────────────
//  마을 사람
// ─────────────────────────────────────────────────────────────────────────

class NpcActor : public CharacterActor
{
public:
	NpcActor(const wchar_t* name, int dialogueSet, int witnessId, int look, float wander);

	int  Kind() const { return ACTOR_NPC; }
	void Update(float dt, const UpdateContext& ctx);
	void Render(const RenderContext& ctx) const;

	const wchar_t* Name()        const { return m_Name; }
	int            DialogueSet() const { return m_DialogueSet; }
	int            WitnessId()   const { return m_WitnessId; }

	void SetHome(float hx, float hy);

	bool talked  = false;
	bool talking = false;   // 대화 중이면 멈춰 서서 플레이어를 본다

private:
	const wchar_t* m_Name;

	int   m_DialogueSet;
	int   m_WitnessId;
	int   m_Look;
	float m_Wander;

	float m_HomeX   = 0.0f, m_HomeY   = 0.0f;
	float m_TargetX = 0.0f, m_TargetY = 0.0f;
	float m_WaitTimer = 0.0f;
};


// ─────────────────────────────────────────────────────────────────────────
//  야생 짐승
// ─────────────────────────────────────────────────────────────────────────

class BeastActor : public CharacterActor
{
public:
	explicit BeastActor(int beastKind);

	int  Kind() const { return ACTOR_BEAST; }
	void Update(float dt, const UpdateContext& ctx);
	void Render(const RenderContext& ctx) const;

	void SetHome(float hx, float hy);

private:
	int   m_BeastKind;
	float m_Speed;
	float m_Alert = 0.0f;

	float m_HomeX   = 0.0f, m_HomeY   = 0.0f;
	float m_TargetX = 0.0f, m_TargetY = 0.0f;
	float m_WaitTimer = 0.0f;
};


// ─────────────────────────────────────────────────────────────────────────
//  1레벨 몬스터
// ─────────────────────────────────────────────────────────────────────────

class MonsterActor : public CharacterActor
{
public:
	explicit MonsterActor(int monsterKind);

	int  Kind() const { return ACTOR_MONSTER; }
	void Update(float dt, const UpdateContext& ctx);
	void Render(const RenderContext& ctx) const;

	// 피해를 입힌다. 이 일격으로 죽었으면 true.
	bool TakeDamage(int amount, float fromX, float fromY, const UpdateContext& ctx);

	bool IsDying() const { return m_DeathTimer > 0.0f; }

private:
	void DropLoot(const UpdateContext& ctx);

	int m_MonsterKind;
	int m_Hp;
	int m_MaxHp;
	int m_Attack;
	int m_XpReward;

	float m_Speed;
	float m_AggroRange;
	float m_AttackCd   = 0.0f;
	float m_HurtFlash  = 0.0f;
	float m_KnockX     = 0.0f;
	float m_KnockY     = 0.0f;
	float m_DeathTimer = 0.0f;
};


// ─────────────────────────────────────────────────────────────────────────
//  바닥에 떨어진 습득물
// ─────────────────────────────────────────────────────────────────────────

class PickupActor : public Actor
{
public:
	PickupActor(int kind, int value, float bobSeed);

	int  Kind() const { return ACTOR_PICKUP; }
	void Update(float dt, const UpdateContext& ctx);
	void Render(const RenderContext& ctx) const;

	int PickupKind() const { return m_Kind; }
	int Value()      const { return m_Value; }

private:
	int   m_Kind;       // 0 경험치 구슬, 1 동전, 2 회복약
	int   m_Value;
	float m_BobSeed;
	float m_Life;
};
