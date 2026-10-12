#include "stdafx.h"
#include "Actors.h"
#include "Scene.h"
#include "Draw.h"
#include "Stats.h"
#include "Input.h"
#include "IsoMath.h"

#include <cmath>

namespace
{
	const float BODY_R        = 0.30f;
	const float PLAYER_SPEED  = 4.6f;
	const float NPC_SPEED     = 1.1f;

	const float ATTACK_COOLDOWN = 0.45f;
	const float ATTACK_REACH    = 2.0f;
	const float MAGNET_RANGE    = 3.2f;

	float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

	// 액터의 월드 좌표를 화면 좌표로 옮긴다. 구체 액터마다 반복되는 계산이라 묶어 둔다.
	void ToScreen(const Actor& a, const RenderContext& ctx, float& sx, float& sy)
	{
		sx = ctx.originX + IsoScreenX(a.WorldX(), a.WorldY());
		sy = ctx.originY + IsoScreenY(a.WorldX(), a.WorldY(), a.WorldZ() * TILE_H);
	}

	// 액터마다 다른 값을 주되 프레임 사이에는 튀지 않는 잡값.
	float Wobble(float seed, float time, int salt)
	{
		return (float)(((int)(seed * 131.0f) + salt * 37 + (int)(time * 10.0f)) % 628) * 0.01f;
	}

	// 0~1. 확률 판정용 — Wobble 은 0~6.28 이라 그대로 쓰면 항상 참이 된다.
	float Wobble01(float seed, float time, int salt)
	{
		return Wobble(seed, time, salt) * 0.159155f;
	}

	// 시드에서 -0.25 ~ +0.25 를 뽑는다. 원형의 성향 중앙값을 개체별로 흩을 때 쓴다.
	float Spread(unsigned int seed, unsigned int salt)
	{
		unsigned int h = seed * 1664525u + salt * 1013904223u;

		h ^= h >> 15;
		h *= 2246822519u;
		h ^= h >> 13;

		return (float)(h % 1000u) * 0.0005f - 0.25f;
	}

	// 외형 팔레트 — 망토 / 피부 / 머리
	const PersonLook LOOKS[] =
	{
		{ { 0.30f, 0.29f, 0.40f }, { 0.76f, 0.62f, 0.49f }, { 0.20f, 0.16f, 0.13f }, 1.00f }, //  0 미렌
		{ { 0.34f, 0.29f, 0.19f }, { 0.70f, 0.55f, 0.41f }, { 0.36f, 0.33f, 0.28f }, 1.00f }, //  1 호드
		{ { 0.20f, 0.31f, 0.34f }, { 0.72f, 0.57f, 0.44f }, { 0.24f, 0.21f, 0.17f }, 1.00f }, //  2 펠
		{ { 0.46f, 0.31f, 0.34f }, { 0.80f, 0.66f, 0.52f }, { 0.30f, 0.19f, 0.11f }, 0.94f }, //  3 타마
		{ { 0.26f, 0.23f, 0.23f }, { 0.66f, 0.50f, 0.37f }, { 0.15f, 0.13f, 0.11f }, 1.06f }, //  4 고렌
		{ { 0.36f, 0.37f, 0.26f }, { 0.79f, 0.66f, 0.53f }, { 0.27f, 0.20f, 0.13f }, 0.74f }, //  5 닙
		{ { 0.33f, 0.26f, 0.31f }, { 0.74f, 0.54f, 0.44f }, { 0.31f, 0.28f, 0.25f }, 0.98f }, //  6 보스크
		{ { 0.22f, 0.21f, 0.26f }, { 0.73f, 0.60f, 0.48f }, { 0.55f, 0.53f, 0.50f }, 1.00f }, //  7 에드라
		{ { 0.40f, 0.28f, 0.22f }, { 0.77f, 0.62f, 0.48f }, { 0.28f, 0.22f, 0.16f }, 0.97f }, //  8 마르타
		{ { 0.24f, 0.28f, 0.32f }, { 0.71f, 0.57f, 0.44f }, { 0.18f, 0.16f, 0.14f }, 1.05f }, //  9 콜름
		{ { 0.28f, 0.34f, 0.25f }, { 0.78f, 0.64f, 0.50f }, { 0.34f, 0.26f, 0.15f }, 0.95f }, // 10 위나
		{ { 0.32f, 0.27f, 0.20f }, { 0.69f, 0.54f, 0.40f }, { 0.22f, 0.18f, 0.14f }, 1.02f }, // 11 도른
		{ { 0.42f, 0.36f, 0.24f }, { 0.80f, 0.67f, 0.54f }, { 0.24f, 0.17f, 0.11f }, 0.72f }, // 12 셀비
		{ { 0.27f, 0.26f, 0.28f }, { 0.74f, 0.63f, 0.53f }, { 0.62f, 0.61f, 0.58f }, 0.90f }, // 13 하르윈
		{ { 0.50f, 0.25f, 0.18f }, { 0.82f, 0.68f, 0.52f }, { 0.26f, 0.18f, 0.12f }, 1.02f }, // 14 플레이어
	};

	struct MonsterDef
	{
		int   hp;
		int   attack;
		int   xp;
		float speed;
		float aggro;
	};

	// 슬라임 / 초퍼 / 스피터
	const MonsterDef MONSTER_DEFS[3] =
	{
		{ 22,  5,  8, 1.5f,  7.0f },
		{ 38,  9, 16, 2.4f, 10.0f },
		{ 30, 12, 22, 1.8f, 12.0f },
	};
}

const PersonLook& LookAt(int index)
{
	if (index < 0 || index >= 15)
		index = 14;

	return LOOKS[index];
}


// ─────────────────────────────────────────────────────────────────────────
//  정적 배치물
// ─────────────────────────────────────────────────────────────────────────

PropActor::PropActor(const WorldObject& obj)
	: m_Obj(obj)
{
	x = obj.x;
	y = obj.y;
	z = 0.0f;

	m_Static = true;

	// 집처럼 큰 것은 밑동이 화면 밖이어도 지붕이 보인다. 여유를 크게 잡는다.
	cullPad = obj.height + 96.0f;
}

void PropActor::Render(const RenderContext& ctx) const
{
	float sx, sy;
	ToScreen(*this, ctx, sx, sy);

	float lit = Draw::LightAt(WorldX(), WorldY(), ctx.viewX, ctx.viewY);

	Draw::Object(*ctx.r, *ctx.models, m_Obj, sx, sy, lit, ctx.time,
	             sigilTaken, sigilHighlight);
}


// ─────────────────────────────────────────────────────────────────────────
//  걷는 것들의 공통 뼈대
// ─────────────────────────────────────────────────────────────────────────

// 주의: 충돌은 월드 좌표로 판정한다. 이동하는 액터는 루트 바로 아래에 두어
// 로컬 좌표와 월드 좌표가 같게 유지한다. 부모에 매달면 이 판정이 어긋난다.
bool CharacterActor::MoveWithCollision(const World& world, float dx, float dy, float radius)
{
	float beforeX = x;
	float beforeY = y;

	// 축을 나눠 처리해야 벽을 따라 미끄러진다.
	if (dx != 0.0f && !world.Blocked(x + dx, y, radius))
		x += dx;

	if (dy != 0.0f && !world.Blocked(x, y + dy, radius))
		y += dy;

	return (fabsf(x - beforeX) + fabsf(y - beforeY)) > 0.0001f;
}

void CharacterActor::FaceByMovement(float dx, float dy)
{
	float screenDir = IsoScreenX(dx, dy);

	if (screenDir >  0.001f) facing =  1;
	if (screenDir < -0.001f) facing = -1;
}

void CharacterActor::StepAnim(float dt, bool moving, float cycleSpeed)
{
	float target = moving ? 1.0f : 0.0f;

	m_WalkAmount += (target - m_WalkAmount) * Clamp01(dt * 9.0f);

	if (moving)
		m_WalkPhase += dt * cycleSpeed;
}


// ─────────────────────────────────────────────────────────────────────────
//  플레이어
// ─────────────────────────────────────────────────────────────────────────

PlayerActor::PlayerActor()
{
	cullPad = 128.0f;
}

void PlayerActor::Update(float dt, const UpdateContext& ctx)
{
	if (m_AttackCd  > 0.0f) m_AttackCd  -= dt;
	if (m_HurtFlash > 0.0f) m_HurtFlash -= dt * 2.5f;

	if (m_AttackAnim >= 0.0f)
	{
		m_AttackAnim += dt * 5.0f;

		if (m_AttackAnim > 1.0f)
			m_AttackAnim = -1.0f;
	}

	if (frozen || ctx.paused)
	{
		StepAnim(dt, false);
		return;
	}

	// ── 이동 ──
	// 쿼터뷰에서는 화면 기준으로 걷는 느낌이 자연스럽다.
	// W = 화면 위쪽 = 월드 (-x, -y)
	float ix = 0.0f;
	float iy = 0.0f;

	if (Input::Held('w')) { ix -= 1.0f; iy -= 1.0f; }
	if (Input::Held('s')) { ix += 1.0f; iy += 1.0f; }
	if (Input::Held('a')) { ix -= 1.0f; iy += 1.0f; }
	if (Input::Held('d')) { ix += 1.0f; iy -= 1.0f; }

	float len    = sqrtf(ix * ix + iy * iy);
	bool  moving = false;

	if (len > 0.0001f && ctx.world != 0)
	{
		ix /= len;
		iy /= len;

		moving = MoveWithCollision(*ctx.world,
		                           ix * PLAYER_SPEED * dt,
		                           iy * PLAYER_SPEED * dt, BODY_R);

		FaceByMovement(ix, iy);
	}

	StepAnim(dt, moving);

	// ── 공격 ──
	if (combatEnabled && Input::Pressed(' ') && m_AttackCd <= 0.0f)
		DoAttack(ctx);
}

void PlayerActor::DoAttack(const UpdateContext& ctx)
{
	m_AttackCd   = ATTACK_COOLDOWN;
	m_AttackAnim = 0.0f;

	if (ctx.scene == 0 || ctx.stats == 0)
		return;

	std::vector<Actor*> hits;
	ctx.scene->QueryNear(WorldX(), WorldY(), ATTACK_REACH, ACTOR_MONSTER, hits);

	for (size_t i = 0; i < hits.size(); ++i)
	{
		MonsterActor* m = (MonsterActor*)hits[i];

		if (m->IsDying())
			continue;

		float dx = m->WorldX() - WorldX();
		float dy = m->WorldY() - WorldY();
		float d  = sqrtf(dx * dx + dy * dy);

		// 바라보는 쪽만 벤다. 아주 가까우면 방향을 따지지 않는다.
		float screenDir = IsoScreenX(dx, dy);
		bool  inFront   = (screenDir * (float)facing) > 0.0f;

		if (!inFront && d > ATTACK_REACH * 0.55f)
			continue;

		int damage = ctx.stats->attack + (int)(ctx.time * 13.0f) % 4;

		if (m->TakeDamage(damage, WorldX(), WorldY(), ctx))
			++kills;
	}

	// 산적처럼 파티와 적대하는 NPC 도 벤다. 주민은 여기서 걸러진다 —
	// 적대 판정은 세력이 하고, 칼은 그 결과만 따른다.
	ctx.scene->QueryNear(WorldX(), WorldY(), ATTACK_REACH, ACTOR_NPC, hits);

	for (size_t i = 0; i < hits.size(); ++i)
	{
		NpcActor* n = (NpcActor*)hits[i];

		if (n->IsDying())
			continue;

		int theirFaction = (n->covertCultist && !n->revealed) ? (int)FC_NONE : n->Faction();

		if (!FactionHostile(FC_PARTY, theirFaction))
			continue;

		float dx = n->WorldX() - WorldX();
		float dy = n->WorldY() - WorldY();
		float d  = sqrtf(dx * dx + dy * dy);

		float screenDir = IsoScreenX(dx, dy);
		bool  inFront   = (screenDir * (float)facing) > 0.0f;

		if (!inFront && d > ATTACK_REACH * 0.55f)
			continue;

		int damage = ctx.stats->attack + (int)(ctx.time * 13.0f) % 4;

		if (n->TakeHit(damage, WorldX(), WorldY(), ctx))
			++kills;
	}
}

void PlayerActor::Hurt(int amount, const UpdateContext& ctx)
{
	if (ctx.stats == 0)
		return;

	int taken = amount - ctx.stats->defense;

	if (taken < 1)
		taken = 1;

	ctx.stats->hp -= taken;
	m_HurtFlash = 1.0f;

	if (ctx.stats->hp > 0)
		return;

	// 쓰러지면 입구에서 절반만 회복한 채로 다시 선다.
	ctx.stats->hp = ctx.stats->maxHp / 2;

	if (ctx.world != 0)
	{
		x = ctx.world->SpawnX();
		y = ctx.world->SpawnY();
	}

	respawned = true;
}

void PlayerActor::Render(const RenderContext& ctx) const
{
	float sx, sy;
	ToScreen(*this, ctx, sx, sy);

	const PersonLook& look = LookAt(14);

	Draw::Person(*ctx.r, *ctx.models, sx, sy, 1.0f, ctx.time, look,
	             facing, WalkPhase(), WalkAmount(), Seed(), true);

	// 검은 폐허에서만 든다. 마을에서는 애초에 휘두를 수 없다.
	// m_AttackAnim 이 음수면 Sword 가 알아서 내린 자세로 그린다.
	if (combatEnabled)
		Draw::Sword(*ctx.r, sx, sy, look.scale, facing, m_AttackAnim, 1.0f);
}


// ─────────────────────────────────────────────────────────────────────────
//  마을 사람
// ─────────────────────────────────────────────────────────────────────────

NpcActor::NpcActor(const wchar_t* name, int dialogueSet, int witnessId, int look, float wander)
	: m_Name(name)
	, m_DialogueSet(dialogueSet)
	, m_WitnessId(witnessId)
	, m_Look(look)
	, m_Wander(wander)
{
	cullPad = 120.0f;
}

void NpcActor::SetHome(float hx, float hy)
{
	x = m_HomeX = m_TargetX = hx;
	y = m_HomeY = m_TargetY = hy;
}

// 원형이 중앙값을 주고 시드가 개체차를 만든다 (STORY.md §2-5-1).
// 같은 산적이라도 겁 많은 놈과 없는 놈이 섞여야 무리가 살아 보인다.
void NpcActor::SetupFrom(int archetype, unsigned int seed)
{
	const ArchetypeDef& def = ArchetypeInfo(archetype);

	m_Archetype = archetype;
	m_Faction   = def.faction;
	m_Motive    = def.motive;
	m_Sight     = def.sight;
	m_Armed     = def.armed;
	m_Wander    = def.wander;

	m_MaxHp = def.hp;
	m_Hp    = def.hp;

	m_Traits.faith      = Clamp01(def.faith      + Spread(seed, 1));
	m_Traits.fear       = Clamp01(def.fear       + Spread(seed, 2));
	m_Traits.aggression = Clamp01(def.aggression + Spread(seed, 3));
	m_Traits.loyalty    = Clamp01(def.loyalty    + Spread(seed, 4));

	// 첫 판단 시점을 개체마다 흩어 둔다. 안 그러면 서른 명이 같은 프레임에
	// 한꺼번에 주변을 훑는다.
	m_ThinkTimer = (float)(seed % 100) * 0.006f;
}

// R3 / R4 — 물든 자는 지워진 자를 보지 못한다 (§1-1, §14-4).
bool NpcActor::Perceives(const NpcActor* other) const
{
	if (other == 0)
		return false;

	return CanPerceive(isTuned, hasAnchor, other->isErased);
}

// §14-7 공격 발생 확률. 성인은 동기가 0 이라 어떤 상황에서도 0 이 나온다.
float NpcActor::AttackChance(const UpdateContext& ctx, bool targetAlone, int guards) const
{
	if (m_Motive <= 0.0f)
		return 0.0f;

	float opportunity = (ctx.night ? 1.5f : 1.0f) * (targetAlone ? 1.5f : 0.5f);
	float deterrence  = (float)guards * 0.2f;

	float p = m_Motive * (0.5f + m_Traits.aggression) * opportunity - deterrence;

	if (p < 0.0f) p = 0.0f;
	if (p > 0.9f) p = 0.9f;

	return p;
}

// 주변에서 적대 대상을 고른다.
//
// 매 프레임 돌리면 NPC 수 × 전체 액터 수만큼 훑게 된다. 개체마다 간격을 두고
// 드문드문 본다 — 사람도 사방을 매 순간 살피지는 않는다.
void NpcActor::Retarget(const UpdateContext& ctx)
{
	m_Threat = 0;

	if (ctx.scene == 0)
		return;

	std::vector<Actor*> around;
	ctx.scene->QueryNear(WorldX(), WorldY(), m_Sight, ACTOR_GENERIC, around);

	// 잠입 신도는 정체가 드러나기 전까지 주민으로 행세한다 (§2-6).
	int myFaction = (covertCultist && !revealed) ? (int)FC_NONE : m_Faction;

	Actor* best   = 0;
	float  bestD  = m_Sight * m_Sight;
	int    guards = 0;

	for (size_t i = 0; i < around.size(); ++i)
	{
		Actor* a = around[i];

		if (a == this || !a->alive)
			continue;

		int kind = a->Kind();
		int theirFaction;

		if (kind == ACTOR_PLAYER)
		{
			theirFaction = FC_PARTY;
		}
		else if (kind == ACTOR_MONSTER)
		{
			theirFaction = FC_MONSTER;
		}
		else if (kind == ACTOR_NPC)
		{
			NpcActor* n = (NpcActor*)a;

			if (n->IsDying() || !Perceives(n))
				continue;

			theirFaction = (n->covertCultist && !n->revealed) ? (int)FC_NONE : n->m_Faction;

			// 무장한 이웃은 억지력이다 (§14-7 deterrence).
			if (n->m_Armed && !FactionHostile(myFaction, theirFaction))
				++guards;
		}
		else
		{
			continue;
		}

		if (!FactionHostile(myFaction, theirFaction))
			continue;

		float dx = a->WorldX() - WorldX();
		float dy = a->WorldY() - WorldY();
		float d2 = dx * dx + dy * dy;

		if (d2 > bestD)
			continue;

		bestD = d2;
		best  = a;
	}

	if (best == 0)
		return;

	m_Threat = best;

	// 대상이 혼자인가 — 곁에 일행이 있으면 기회 점수가 절반이 된다.
	int companions = 0;

	for (size_t i = 0; i < around.size(); ++i)
	{
		Actor* a = around[i];

		if (a == best || a == this || !a->alive || a->Kind() != ACTOR_NPC)
			continue;

		float dx = a->WorldX() - best->WorldX();
		float dy = a->WorldY() - best->WorldY();

		if (dx * dx + dy * dy < 9.0f)
			++companions;
	}

	// 해칠 마음이 있으면 덤비고, 없으면 겁에 따라 달아나거나 지켜보기만 한다.
	float chance = AttackChance(ctx, companions < 2, guards);

	if (chance > 0.0f && Wobble01(m_Seed, ctx.time, 23) < chance)
		m_State = AG_HOSTILE;
	else if (m_Motive <= 0.0f && m_Traits.fear > 0.45f)
		m_State = AG_FLEE;
	else
		m_State = AG_ALERT;

	m_StateTimer = 0.0f;
}

bool NpcActor::TakeHit(int amount, float fromX, float fromY, const UpdateContext& ctx)
{
	if (m_DeathTimer > 0.0f || !alive)
		return false;

	// T0/T1 은 스토리의 기둥이라 쓰러지지 않는다 (§2-5).
	if (tier == TIER_T0 || tier == TIER_T1)
	{
		m_HurtFlash = 1.0f;
		return false;
	}

	m_Hp -= amount;
	m_HurtFlash = 1.0f;

	// 맞으면 때린 쪽을 본다.
	FaceByMovement(fromX - x, fromY - y);

	if (m_Hp > 0)
	{
		// 싸울 줄 모르고 겁까지 많으면 그대로 달아난다.
		m_State      = (m_Motive <= 0.0f && m_Traits.fear > 0.35f) ? AG_FLEE : AG_HOSTILE;
		m_StateTimer = 0.0f;

		return false;
	}

	m_DeathTimer = 0.8f;
	m_State      = AG_CALM;

	// ── 사망의 파급 (§14-5) ──
	// 지워진 자의 죽음은 아무 파문도 남기지 않는다 (R6). 기억되지 않으니까.
	if (ctx.hidden != 0 && !isErased)
	{
		ctx.hidden->fear += 8;

		if (ctx.hidden->fear > 100)
			ctx.hidden->fear = 100;

		// 신도가 줄면 교단 세력도 준다 (§13-2).
		if (m_Faction == FC_CHOIR)
		{
			ctx.hidden->choir -= 3;

			if (ctx.hidden->choir < 0)
				ctx.hidden->choir = 0;
		}
	}

	return true;
}

void NpcActor::Update(float dt, const UpdateContext& ctx)
{
	if (m_HurtFlash > 0.0f) m_HurtFlash -= dt * 3.0f;
	if (m_SwingCd   > 0.0f) m_SwingCd   -= dt;

	if (m_SwingAnim >= 0.0f)
	{
		m_SwingAnim += dt * 5.0f;

		if (m_SwingAnim > 1.0f)
			m_SwingAnim = -1.0f;
	}

	// 쓰러지는 중이면 가라앉다가 사라진다.
	if (m_DeathTimer > 0.0f)
	{
		m_DeathTimer -= dt;

		if (m_DeathTimer <= 0.0f)
			alive = false;

		return;
	}

	// 말을 거는 중이면 가만히 서서 플레이어 쪽을 본다.
	if (talking)
	{
		StepAnim(dt, false);
		FaceByMovement(ctx.playerX - x, ctx.playerY - y);

		return;
	}

	if (ctx.paused || ctx.world == 0)
	{
		StepAnim(dt, false);
		return;
	}

	m_StateTimer += dt;
	m_ThinkTimer -= dt;

	if (m_ThinkTimer <= 0.0f)
	{
		Retarget(ctx);

		// 겁이 많을수록 자주 두리번거린다.
		m_ThinkTimer = 0.45f + (1.0f - m_Traits.fear) * 0.5f;
	}

	// 표적이 사라졌으면 일상으로 돌아온다.
	if (m_Threat != 0 && !m_Threat->alive)
	{
		m_Threat = 0;
		m_State  = AG_CALM;
	}

	if (m_Threat == 0 && m_State != AG_CALM && m_StateTimer > 2.0f)
		m_State = AG_CALM;

	bool moving = false;

	switch (m_State)
	{

	// ── 일상 — 집 주변을 배회한다 ──
	case AG_CALM:
	{
		m_WaitTimer -= dt;

		if (m_WaitTimer <= 0.0f)
		{
			float a = Wobble(m_Seed, ctx.time, 3);
			float r = m_Wander * (0.35f + 0.65f * Clamp01(Wobble(m_Seed, ctx.time, 7) * 0.16f));

			m_TargetX = m_HomeX + cosf(a) * r;
			m_TargetY = m_HomeY + sinf(a) * r;

			m_WaitTimer = 2.4f + Wobble(m_Seed, ctx.time, 11);
		}

		float dx = m_TargetX - x;
		float dy = m_TargetY - y;
		float d  = sqrtf(dx * dx + dy * dy);

		if (d > 0.12f)
		{
			float step = NPC_SPEED * dt;

			moving = MoveWithCollision(*ctx.world, dx / d * step, dy / d * step, BODY_R * 0.8f);

			// 벽에 막혀 제자리면 다음 목적지를 새로 뽑는다.
			if (!moving)
				m_WaitTimer = 0.0f;
			else
				FaceByMovement(dx, dy);
		}

		break;
	}

	// ── 경계 — 멈춰 서서 지켜본다 ──
	case AG_ALERT:
	{
		if (m_Threat != 0)
			FaceByMovement(m_Threat->WorldX() - x, m_Threat->WorldY() - y);

		break;
	}

	// ── 적대 — 다가간다 ──
	case AG_HOSTILE:
	{
		if (m_Threat == 0)
		{
			m_State = AG_CALM;
			break;
		}

		float dx = m_Threat->WorldX() - x;
		float dy = m_Threat->WorldY() - y;
		float d  = sqrtf(dx * dx + dy * dy);

		if (d < 1.05f)
		{
			m_State      = AG_ATTACK;
			m_StateTimer = 0.0f;

			FaceByMovement(dx, dy);
		}
		else if (d > m_Sight * 1.6f)
		{
			// 놓쳤다.
			m_Threat = 0;
			m_State  = AG_CALM;
		}
		else
		{
			float step = NPC_SPEED * 1.7f * dt;

			moving = MoveWithCollision(*ctx.world, dx / d * step, dy / d * step, BODY_R * 0.8f);

			if (moving)
				FaceByMovement(dx, dy);
		}

		break;
	}

	// ── 공격 ──
	case AG_ATTACK:
	{
		if (m_Threat == 0)
		{
			m_State = AG_CALM;
			break;
		}

		float dx = m_Threat->WorldX() - x;
		float dy = m_Threat->WorldY() - y;
		float d  = sqrtf(dx * dx + dy * dy);

		if (d > 1.35f)
		{
			m_State = AG_HOSTILE;
			break;
		}

		FaceByMovement(dx, dy);

		if (m_SwingCd <= 0.0f)
		{
			m_SwingCd   = 1.1f;
			m_SwingAnim = 0.0f;

			int damage = 2 + (int)(m_Traits.aggression * 4.0f);

			if (m_Threat->Kind() == ACTOR_PLAYER && ctx.player != 0)
			{
				ctx.player->Hurt(damage, ctx);
			}
			else if (m_Threat->Kind() == ACTOR_NPC)
			{
				NpcActor* victim = (NpcActor*)m_Threat;

				if (victim->TakeHit(damage, x, y, ctx))
					m_Threat = 0;
			}
		}

		break;
	}

	// ── 도주 ──
	case AG_FLEE:
	{
		if (m_Threat == 0)
		{
			m_State = AG_CALM;
			break;
		}

		float dx = x - m_Threat->WorldX();
		float dy = y - m_Threat->WorldY();
		float d  = sqrtf(dx * dx + dy * dy);

		if (d > m_Sight * 1.4f)
		{
			m_Threat = 0;
			m_State  = AG_CALM;

			break;
		}

		if (d > 0.01f)
		{
			float step = NPC_SPEED * 1.5f * dt;

			moving = MoveWithCollision(*ctx.world, dx / d * step, dy / d * step, BODY_R * 0.8f);

			if (moving)
				FaceByMovement(dx, dy);
		}

		break;
	}

	}

	StepAnim(dt, moving);
}

void NpcActor::Render(const RenderContext& ctx) const
{
	float sx, sy;
	ToScreen(*this, ctx, sx, sy);

	// 쓰러지는 중이면 가라앉는다.
	float sink = (m_DeathTimer > 0.0f) ? (1.0f - m_DeathTimer / 0.8f) * 7.0f : 0.0f;

	float lit = Draw::LightAt(WorldX(), WorldY(), ctx.viewX, ctx.viewY);

	// 맞은 직후엔 잠깐 하얗게 뜬다.
	if (m_HurtFlash > 0.0f)
		lit += m_HurtFlash * 0.7f;

	const PersonLook& look = LookAt(m_Look);

	Draw::Person(*ctx.r, *ctx.models, sx, sy + sink, lit, ctx.time, look,
	             facing, WalkPhase(), WalkAmount(), Seed(), false);

	// 무장한 원형은 검을 들고 있다. 휘두르는 동작이 그대로 보인다.
	if (m_Armed && m_DeathTimer <= 0.0f)
		Draw::Sword(*ctx.r, sx, sy, look.scale, facing, m_SwingAnim, lit);
}


// ─────────────────────────────────────────────────────────────────────────

BeastActor::BeastActor(int beastKind)
	: m_BeastKind(beastKind)
{
	m_Speed = (beastKind == BEAST_WOLF) ? 2.2f
	        : (beastKind == BEAST_DEER) ? 2.6f
	                                    : 1.6f;
	cullPad = 110.0f;
}

void BeastActor::SetHome(float hx, float hy)
{
	x = m_HomeX = m_TargetX = hx;
	y = m_HomeY = m_TargetY = hy;
}

void BeastActor::Update(float dt, const UpdateContext& ctx)
{
	if (ctx.world == 0)
		return;

	float dxp = ctx.playerX - x;
	float dyp = ctx.playerY - y;
	float toPlayer = sqrtf(dxp * dxp + dyp * dyp);

	// 종마다 사람을 의식하는 거리가 다르다.
	float notice = (m_BeastKind == BEAST_DEER) ? 13.0f
	             : (m_BeastKind == BEAST_WOLF) ? 10.0f
	                                           : 7.0f;

	float wantAlert = Clamp01((notice - toPlayer) / notice);
	m_Alert += (wantAlert - m_Alert) * Clamp01(dt * 2.5f);

	bool fleeing = false;

	if (toPlayer < notice * 0.62f && m_BeastKind != BEAST_WOLF && toPlayer > 0.01f)
	{
		// 사슴과 까마귀는 달아난다.
		m_TargetX = x - dxp / toPlayer * 7.0f;
		m_TargetY = y - dyp / toPlayer * 7.0f;

		fleeing = true;
	}
	else if (toPlayer < notice * 0.45f && m_BeastKind == BEAST_WOLF && toPlayer > 0.01f)
	{
		// 늑대는 물러서되 등을 보이지 않는다 — 아주 가까울 때만 거리를 벌린다.
		m_TargetX = x - dxp / toPlayer * 3.5f;
		m_TargetY = y - dyp / toPlayer * 3.5f;
	}
	else
	{
		m_WaitTimer -= dt;

		if (m_WaitTimer <= 0.0f)
		{
			float a = Wobble(m_Seed, ctx.time, 5);
			float r = 2.0f + Clamp01(Wobble(m_Seed, ctx.time, 13) * 0.16f) * 5.0f;

			m_TargetX = m_HomeX + cosf(a) * r;
			m_TargetY = m_HomeY + sinf(a) * r;

			m_WaitTimer = 2.0f + Wobble(m_Seed, ctx.time, 17);
		}
	}

	float dx = m_TargetX - x;
	float dy = m_TargetY - y;
	float d  = sqrtf(dx * dx + dy * dy);

	bool moving = false;

	if (d > 0.15f)
	{
		float speed = m_Speed * (fleeing ? 2.0f : (0.55f + m_Alert * 0.5f));
		float step  = speed * dt;

		moving = MoveWithCollision(*ctx.world, dx / d * step, dy / d * step, BODY_R * 0.7f);

		if (!moving)
			m_WaitTimer = 0.0f;
		else
			FaceByMovement(dx, dy);
	}

	StepAnim(dt, moving, (m_BeastKind == BEAST_CROW) ? 13.0f : 11.0f);
}

void BeastActor::Render(const RenderContext& ctx) const
{
	float sx, sy;
	ToScreen(*this, ctx, sx, sy);

	float lit = Draw::LightAt(WorldX(), WorldY(), ctx.viewX, ctx.viewY);

	Draw::Beast(*ctx.r, *ctx.models, sx, sy, lit, ctx.time, m_BeastKind,
	            facing, WalkPhase(), WalkAmount(), m_Alert);
}


// ─────────────────────────────────────────────────────────────────────────
//  1레벨 몬스터
// ─────────────────────────────────────────────────────────────────────────

MonsterActor::MonsterActor(int monsterKind)
	: m_MonsterKind(monsterKind)
{
	if (m_MonsterKind < 0 || m_MonsterKind > 2)
		m_MonsterKind = 0;

	const MonsterDef& def = MONSTER_DEFS[m_MonsterKind];

	m_Hp         = def.hp;
	m_MaxHp      = def.hp;
	m_Attack     = def.attack;
	m_XpReward   = def.xp;
	m_Speed      = def.speed;
	m_AggroRange = def.aggro;

	cullPad = 110.0f;
}

bool MonsterActor::TakeDamage(int amount, float fromX, float fromY, const UpdateContext& ctx)
{
	if (m_DeathTimer > 0.0f)
		return false;

	m_Hp -= amount;
	m_HurtFlash = 1.0f;

	// 맞은 방향으로 밀려난다.
	float dx = WorldX() - fromX;
	float dy = WorldY() - fromY;
	float d  = sqrtf(dx * dx + dy * dy);

	if (d > 0.01f)
	{
		m_KnockX = dx / d * 5.5f;
		m_KnockY = dy / d * 5.5f;
	}

	if (m_Hp > 0)
		return false;

	m_DeathTimer = 0.5f;
	DropLoot(ctx);

	return true;
}

void MonsterActor::DropLoot(const UpdateContext& ctx)
{
	if (ctx.scene == 0)
		return;

	// 경험치 구슬은 반드시 떨어진다.
	PickupActor* orb = new PickupActor(0, m_XpReward, m_Seed);
	orb->x = x;
	orb->y = y;
	ctx.scene->Add(orb);

	unsigned int roll = (unsigned int)(ctx.time * 1000.0f) + (unsigned int)(m_Seed * 71.0f);

	if ((roll % 100u) < 45u)
	{
		PickupActor* coin = new PickupActor(1, 3 + (int)(roll % 7u), m_Seed + 1.0f);
		coin->x = x + 0.4f;
		coin->y = y - 0.3f;
		ctx.scene->Add(coin);
	}

	if ((roll % 100u) < 18u)
	{
		PickupActor* potion = new PickupActor(2, 1, m_Seed + 2.0f);
		potion->x = x - 0.4f;
		potion->y = y + 0.3f;
		ctx.scene->Add(potion);
	}
}

void MonsterActor::Update(float dt, const UpdateContext& ctx)
{
	if (m_HurtFlash > 0.0f)
		m_HurtFlash -= dt * 3.5f;

	// 죽는 중이면 가라앉다가 사라진다.
	if (m_DeathTimer > 0.0f)
	{
		m_DeathTimer -= dt;

		if (m_DeathTimer <= 0.0f)
			alive = false;

		return;
	}

	if (ctx.paused || ctx.world == 0)
		return;

	if (m_AttackCd > 0.0f)
		m_AttackCd -= dt;

	float dx = ctx.playerX - x;
	float dy = ctx.playerY - y;
	float d  = sqrtf(dx * dx + dy * dy);

	bool moving = false;

	// 밀려나는 중이면 그것부터 처리한다.
	if (fabsf(m_KnockX) + fabsf(m_KnockY) > 0.01f)
	{
		MoveWithCollision(*ctx.world, m_KnockX * dt, m_KnockY * dt, BODY_R * 0.8f);

		m_KnockX *= 0.82f;
		m_KnockY *= 0.82f;

		moving = true;
	}
	else if (d < m_AggroRange)
	{
		if (d > 0.95f)
		{
			float step = m_Speed * dt;

			moving = MoveWithCollision(*ctx.world, dx / d * step, dy / d * step, BODY_R * 0.8f);

			if (moving)
				FaceByMovement(dx, dy);
		}
		else if (m_AttackCd <= 0.0f && ctx.player != 0)
		{
			ctx.player->Hurt(m_Attack, ctx);
			m_AttackCd = 1.2f;
		}
	}

	StepAnim(dt, moving, 9.0f);
}

void MonsterActor::Render(const RenderContext& ctx) const
{
	float sx, sy;
	ToScreen(*this, ctx, sx, sy);

	float lit = Draw::LightAt(WorldX(), WorldY(), ctx.viewX, ctx.viewY);

	// 죽는 중이면 가라앉으며 어두워진다.
	if (m_DeathTimer > 0.0f)
	{
		float t = Clamp01(m_DeathTimer / 0.5f);

		sy  += (1.0f - t) * 8.0f;
		lit *= t;
	}

	Draw::Monster(*ctx.r, *ctx.models, sx, sy, lit, ctx.time, m_MonsterKind,
	              facing, WalkPhase(), WalkAmount(), m_HurtFlash);
}


// ─────────────────────────────────────────────────────────────────────────
//  습득물
// ─────────────────────────────────────────────────────────────────────────

PickupActor::PickupActor(int kind, int value, float bobSeed)
	: m_Kind(kind)
	, m_Value(value)
	, m_BobSeed(bobSeed)
	, m_Life(60.0f)
{
	cullPad = 64.0f;
}

void PickupActor::Update(float dt, const UpdateContext& ctx)
{
	m_Life -= dt;

	if (m_Life <= 0.0f)
	{
		alive = false;
		return;
	}

	// 가까워지면 빨려 온다 — 일일이 밟으러 다니지 않아도 되게.
	float dx = ctx.playerX - x;
	float dy = ctx.playerY - y;
	float d  = sqrtf(dx * dx + dy * dy);

	if (d >= MAGNET_RANGE || d <= 0.01f)
		return;

	float pull = (MAGNET_RANGE - d) / MAGNET_RANGE * 6.0f * dt;

	x += dx / d * pull;
	y += dy / d * pull;
}

void PickupActor::Render(const RenderContext& ctx) const
{
	float sx, sy;
	ToScreen(*this, ctx, sx, sy);

	float lit = Draw::LightAt(WorldX(), WorldY(), ctx.viewX, ctx.viewY);

	Draw::Pickup(*ctx.r, *ctx.models, sx, sy, lit, ctx.time, m_Kind, m_BobSeed);
}
