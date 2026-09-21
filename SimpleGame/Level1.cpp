#include "stdafx.h"
#include "Game.h"
#include "Renderer.h"
#include "Draw.h"
#include "Input.h"
#include "IsoMath.h"

#include <cmath>
#include <cstdio>
#include <cwchar>

// 1레벨(폐허)의 전투·성장 규칙.
// 마을 쪽 로직과 섞이지 않도록 번역 단위를 나눠 두었다.

namespace
{
	float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

	float Dist(float ax, float ay, float bx, float by)
	{
		float dx = bx - ax;
		float dy = by - ay;

		return sqrtf(dx * dx + dy * dy);
	}

	const float ATTACK_COOLDOWN = 0.45f;
	const float ATTACK_REACH    = 2.0f;
	const float PICKUP_RANGE    = 0.9f;
	const float MAGNET_RANGE    = 3.2f;

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

	const wchar_t* MONSTER_NAMES[3] =
	{
		L"슬라임", L"초퍼", L"스피터"
	};
}


// ─────────────────────────────────────────────────────────────────────────
//  레벨 전환
// ─────────────────────────────────────────────────────────────────────────

void Game::EnterRuin(unsigned int seed)
{
	m_RuinSeed = seed;
	m_Level    = LEVEL_RUIN;

	m_World.GenerateRuin(seed);

	m_Player.x = m_World.SpawnX();
	m_Player.y = m_World.SpawnY();
	m_Player.vx = 0.0f;
	m_Player.vy = 0.0f;
	m_Player.walkAmount = 0.0f;

	m_Monsters.clear();
	m_Pickups.clear();
	m_Lights.clear();

	SpawnRuinMonsters();

	m_Killed      = 0;
	m_AttackCd    = 0.0f;
	m_AttackAnim  = -1.0f;
	m_HurtFlash   = 0.0f;
	m_RuinIntro   = 4.0f;
	m_CamReady    = false;
	m_JournalOpen = false;
	m_StatsOpen   = false;

	Toast(L"가라앉은 채석장 — 깊이 1층");
}

void Game::ReturnToVillage()
{
	m_Level = LEVEL_VILLAGE;

	m_World.Generate();

	m_Monsters.clear();
	m_Pickups.clear();

	m_Player.x = m_World.SpawnX();
	m_Player.y = m_World.SpawnY();
	m_Player.vx = 0.0f;
	m_Player.vy = 0.0f;

	m_CamReady  = false;
	m_StatsOpen = false;

	// 마을 광원 목록을 다시 만든다.
	m_Lights.clear();

	const std::vector<WorldObject>& objs = m_World.Objects();

	for (size_t i = 0; i < objs.size(); ++i)
	{
		const WorldObject& o = objs[i];

		if (o.type == OBJ_CAMPFIRE)
		{
			LightSource l = { o.x, o.y, 1.00f, 0.58f, 0.24f, 210.0f, 0.55f, true };
			m_Lights.push_back(l);
		}
		else if (o.type == OBJ_LANTERN)
		{
			LightSource l = { o.x, o.y, 1.00f, 0.74f, 0.36f, 120.0f, 0.34f, true };
			m_Lights.push_back(l);
		}
		else if (o.type == OBJ_INN || o.type == OBJ_HOUSE || o.type == OBJ_CHAPEL)
		{
			LightSource l = { o.x, o.y + 0.6f, 0.98f, 0.72f, 0.36f, 92.0f, 0.16f, false };
			m_Lights.push_back(l);
		}
	}

	Toast(L"라벤홀로 돌아왔다");
}

void Game::SpawnRuinMonsters()
{
	const std::vector<Room>& rooms = m_World.Rooms();

	unsigned int rng = m_RuinSeed ^ 0xA5A5u;

	// 시작 방은 비워 둔다 — 들어가자마자 맞고 시작하면 곤란하다.
	for (size_t i = 1; i < rooms.size(); ++i)
	{
		const Room& rm = rooms[i];

		rng = rng * 1664525u + 1013904223u;
		int count = 2 + (int)((rng >> 8) % 4);

		for (int k = 0; k < count; ++k)
		{
			rng = rng * 1664525u + 1013904223u;
			float fx = (float)(rm.x0 + 1) + (float)((rng >> 8) % 1000) * 0.001f
			         * (float)(rm.x1 - rm.x0 - 2);

			rng = rng * 1664525u + 1013904223u;
			float fy = (float)(rm.y0 + 1) + (float)((rng >> 8) % 1000) * 0.001f
			         * (float)(rm.y1 - rm.y0 - 2);

			if (m_World.Blocked(fx, fy, 0.35f))
				continue;

			rng = rng * 1664525u + 1013904223u;

			// 안쪽 방일수록 험한 놈이 나온다.
			int kind = 0;

			if (i >= rooms.size() * 2 / 3)      kind = (int)((rng >> 8) % 3);
			else if (i >= rooms.size() / 3)     kind = (int)((rng >> 8) % 2);

			const MonsterDef& def = MONSTER_DEFS[kind];

			Monster m;

			m.kind       = kind;
			m.maxHp      = def.hp;
			m.hp         = def.hp;
			m.attack     = def.attack;
			m.xpReward   = def.xp;
			m.speed      = def.speed;
			m.aggroRange = def.aggro;
			m.attackCd   = 0.0f;
			m.hurtFlash  = 0.0f;
			m.knockX     = 0.0f;
			m.knockY     = 0.0f;
			m.alive      = true;
			m.deathTimer = 0.0f;

			m.c.x  = fx;
			m.c.y  = fy;
			m.c.vx = 0.0f;
			m.c.vy = 0.0f;

			m.c.walkPhase  = (float)k;
			m.c.walkAmount = 0.0f;
			m.c.bobPhase   = (float)(i + k);
			m.c.facing     = 1;

			m_Monsters.push_back(m);
		}
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  전투
// ─────────────────────────────────────────────────────────────────────────

void Game::DoPlayerAttack()
{
	m_AttackCd   = ATTACK_COOLDOWN;
	m_AttackAnim = 0.0f;

	for (size_t i = 0; i < m_Monsters.size(); ++i)
	{
		Monster& m = m_Monsters[i];

		if (!m.alive)
			continue;

		float dx = m.c.x - m_Player.x;
		float dy = m.c.y - m_Player.y;
		float d  = sqrtf(dx * dx + dy * dy);

		if (d > ATTACK_REACH)
			continue;

		// 바라보는 쪽만 벤다. 아주 가까우면 방향을 따지지 않는다.
		float screenDir = IsoScreenX(dx, dy);
		bool  inFront   = (screenDir * (float)m_Player.facing) > 0.0f;

		if (!inFront && d > ATTACK_REACH * 0.55f)
			continue;

		int damage = m_Stats.attack + (int)(m_Time * 13.0f) % 4;

		m.hp -= damage;
		m.hurtFlash = 1.0f;

		// 뒤로 밀어낸다
		if (d > 0.01f)
		{
			m.knockX = dx / d * 5.5f;
			m.knockY = dy / d * 5.5f;
		}

		if (m.hp <= 0)
		{
			m.alive      = false;
			m.deathTimer = 0.5f;

			++m_Killed;

			DropLoot(m.c.x, m.c.y, m.kind);
		}
	}
}

void Game::HurtPlayer(int amount)
{
	int taken = amount - m_Stats.defense;

	if (taken < 1)
		taken = 1;

	m_Stats.hp -= taken;
	m_HurtFlash = 1.0f;

	if (m_Stats.hp > 0)
		return;

	// 쓰러지면 시작 지점에서 절반만 회복한 채로 다시 선다.
	m_Stats.hp = m_Stats.maxHp / 2;

	m_Player.x = m_World.SpawnX();
	m_Player.y = m_World.SpawnY();
	m_CamReady = false;

	Toast(L"정신을 잃었다 — 입구에서 깨어난다");
}

void Game::DropLoot(float x, float y, int monsterKind)
{
	// 경험치 구슬은 반드시 떨어진다.
	{
		Pickup p;

		p.x       = x;
		p.y       = y;
		p.kind    = 0;
		p.value   = MONSTER_DEFS[monsterKind].xp;
		p.bobSeed = (float)m_Pickups.size() * 0.7f;
		p.life    = 60.0f;
		p.taken   = false;

		m_Pickups.push_back(p);
	}

	unsigned int roll = (unsigned int)(m_Time * 1000.0f) + (unsigned int)m_Pickups.size() * 71u;

	// 동전과 회복약은 확률로 떨어진다.
	if ((roll % 100u) < 45u)
	{
		Pickup p;

		p.x       = x + 0.4f;
		p.y       = y - 0.3f;
		p.kind    = 1;
		p.value   = 3 + (int)(roll % 7u);
		p.bobSeed = (float)m_Pickups.size() * 0.7f;
		p.life    = 60.0f;
		p.taken   = false;

		m_Pickups.push_back(p);
	}

	if ((roll % 100u) < 18u)
	{
		Pickup p;

		p.x       = x - 0.4f;
		p.y       = y + 0.3f;
		p.kind    = 2;
		p.value   = 1;
		p.bobSeed = (float)m_Pickups.size() * 0.7f;
		p.life    = 60.0f;
		p.taken   = false;

		m_Pickups.push_back(p);
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  갱신
// ─────────────────────────────────────────────────────────────────────────

void Game::UpdateRuin(float dt)
{
	if (m_RuinIntro    > 0.0f) m_RuinIntro    -= dt;
	if (m_LevelUpTimer > 0.0f) m_LevelUpTimer -= dt;
	if (m_HurtFlash    > 0.0f) m_HurtFlash    -= dt * 2.5f;
	if (m_AttackCd     > 0.0f) m_AttackCd     -= dt;

	if (m_AttackAnim >= 0.0f)
	{
		m_AttackAnim += dt * 5.0f;

		if (m_AttackAnim > 1.0f)
			m_AttackAnim = -1.0f;
	}

	// ── 입력 ──
	if (!m_StatsOpen)
	{
		if (Input::Pressed(' ') && m_AttackCd <= 0.0f)
			DoPlayerAttack();

		// 회복약
		if (Input::Pressed('q') && m_Stats.potions > 0 && m_Stats.hp < m_Stats.maxHp)
		{
			--m_Stats.potions;

			m_Stats.hp += 30;

			if (m_Stats.hp > m_Stats.maxHp)
				m_Stats.hp = m_Stats.maxHp;

			Toast(L"회복약을 마셨다");
		}
	}
	else
	{
		// 스탯 분배 — 남은 포인트가 있을 때만 반응한다.
		if (Input::Pressed('1') && m_Stats.Invest(STAT_VITALITY)) Toast(L"체력을 올렸다");
		if (Input::Pressed('2') && m_Stats.Invest(STAT_POWER))    Toast(L"공격을 올렸다");
		if (Input::Pressed('3') && m_Stats.Invest(STAT_GUARD))    Toast(L"방어를 올렸다");
	}

	// ── 몬스터 ──
	for (size_t i = 0; i < m_Monsters.size(); ++i)
	{
		Monster& m = m_Monsters[i];

		if (m.hurtFlash > 0.0f)
			m.hurtFlash -= dt * 3.5f;

		if (!m.alive)
		{
			if (m.deathTimer > 0.0f)
				m.deathTimer -= dt;

			continue;
		}

		if (m.attackCd > 0.0f)
			m.attackCd -= dt;

		float dx = m_Player.x - m.c.x;
		float dy = m_Player.y - m.c.y;
		float d  = sqrtf(dx * dx + dy * dy);

		bool moving = false;

		// 밀려나는 중이면 그것부터 처리한다.
		if (fabsf(m.knockX) + fabsf(m.knockY) > 0.01f)
		{
			MoveWithCollision(m.c, m.knockX * dt, m.knockY * dt, BODY_RADIUS * 0.8f);

			m.knockX *= 0.82f;
			m.knockY *= 0.82f;

			moving = true;
		}
		else if (d < m.aggroRange)
		{
			// 쫓아온다
			if (d > 0.95f)
			{
				float step = m.speed * dt;

				float beforeX = m.c.x;
				float beforeY = m.c.y;

				MoveWithCollision(m.c, dx / d * step, dy / d * step, BODY_RADIUS * 0.8f);

				m.c.vx = m.c.x - beforeX;
				m.c.vy = m.c.y - beforeY;

				moving = (fabsf(m.c.vx) + fabsf(m.c.vy)) > 0.0001f;

				float screenDir = IsoScreenX(dx, dy);

				if (screenDir >  0.001f) m.c.facing =  1;
				if (screenDir < -0.001f) m.c.facing = -1;
			}
			else if (m.attackCd <= 0.0f)
			{
				HurtPlayer(m.attack);
				m.attackCd = 1.2f;
			}
		}

		float target = moving ? 1.0f : 0.0f;
		m.c.walkAmount += (target - m.c.walkAmount) * Clamp01(dt * 9.0f);

		if (moving)
			m.c.walkPhase += dt * 9.0f;
	}

	// ── 습득물 ──
	for (size_t i = 0; i < m_Pickups.size(); ++i)
	{
		Pickup& p = m_Pickups[i];

		if (p.taken)
			continue;

		p.life -= dt;

		float d = Dist(p.x, p.y, m_Player.x, m_Player.y);

		// 가까워지면 빨려 온다 — 일일이 밟으러 다니지 않아도 되게.
		if (d < MAGNET_RANGE && d > 0.01f)
		{
			float pull = (MAGNET_RANGE - d) / MAGNET_RANGE * 6.0f * dt;

			p.x += (m_Player.x - p.x) / d * pull;
			p.y += (m_Player.y - p.y) / d * pull;
		}

		if (d > PICKUP_RANGE)
			continue;

		p.taken = true;

		if (p.kind == 0)
		{
			if (m_Stats.GainXp(p.value))
			{
				m_LevelUpTimer = 2.6f;
				Toast(L"레벨이 올랐다 — C 키로 능력치 분배");
			}
		}
		else if (p.kind == 1)
		{
			m_Stats.coins += p.value;
		}
		else
		{
			++m_Stats.potions;
			Toast(L"회복약을 주웠다");
		}
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  UI
// ─────────────────────────────────────────────────────────────────────────

void Game::RenderRuinUI()
{
	wchar_t buf[192];

	const float px = 20.0f;
	const float py = 20.0f;
	const float pw = 300.0f;

	Panel(px, py, pw, 92.0f, 0.82f);

	// ── 레벨 ──
	swprintf(buf, 192, L"레벨 %d", m_Stats.level);
	m_R->DrawString(px + 14.0f, py + 28.0f, buf, 0.92f, 0.78f, 0.44f, 1.0f, FONT_TITLE);

	// ── 체력 막대 ──
	{
		float barX = px + 14.0f;
		float barY = py + 40.0f;
		float barW = pw - 28.0f;

		float t = (m_Stats.maxHp > 0)
		        ? Clamp01((float)m_Stats.hp / (float)m_Stats.maxHp) : 0.0f;

		m_R->DrawQuad(barX, barY, barW, 12.0f, 0.10f, 0.06f, 0.07f, 0.9f);
		m_R->DrawQuad(barX, barY, barW * t, 12.0f, 0.72f, 0.20f, 0.18f, 1.0f);
		m_R->DrawQuad(barX, barY, barW * t, 3.0f,  0.92f, 0.40f, 0.34f, 0.8f);

		swprintf(buf, 192, L"%d / %d", m_Stats.hp, m_Stats.maxHp);
		m_R->DrawString(barX + 6.0f, barY + 11.0f, buf, 0.95f, 0.88f, 0.84f, 1.0f, FONT_SMALL);
	}

	// ── 경험치 막대 ──
	{
		float barX = px + 14.0f;
		float barY = py + 62.0f;
		float barW = pw - 28.0f;

		int   need = m_Stats.XpNeeded();
		float t    = (need > 0) ? Clamp01((float)m_Stats.xp / (float)need) : 0.0f;

		m_R->DrawQuad(barX, barY, barW, 8.0f, 0.06f, 0.09f, 0.10f, 0.9f);
		m_R->DrawQuad(barX, barY, barW * t, 8.0f, 0.24f, 0.78f, 0.72f, 1.0f);

		swprintf(buf, 192, L"경험치  %d / %d", m_Stats.xp, need);
		m_R->DrawString(barX + 6.0f, barY + 20.0f, buf, 0.60f, 0.84f, 0.80f, 1.0f, FONT_SMALL);
	}

	// ── 소지품 ──
	swprintf(buf, 192, L"처치 %d      동전 %d      회복약 %d",
	         m_Killed, m_Stats.coins, m_Stats.potions);
	m_R->DrawString(px + 14.0f, py + 110.0f, buf, 0.78f, 0.74f, 0.66f, 1.0f, FONT_SMALL);

	// ── 분배 가능 알림 ──
	if (m_Stats.statPoints > 0 && !m_StatsOpen)
	{
		float pulse = 0.7f + 0.3f * sinf(m_Time * 4.0f);

		swprintf(buf, 192, L"[C]  분배할 능력치 %d 점", m_Stats.statPoints);
		m_R->DrawString(px + 14.0f, py + 132.0f, buf,
		                0.95f * pulse, 0.72f * pulse, 0.28f * pulse, 1.0f, FONT_BODY);
	}

	// ── 조작 안내 ──
	m_R->DrawString(20.0f, (float)m_WinH - 16.0f,
	                L"WASD 이동     Space 공격     Q 회복약     C 능력치     0 마을로     Esc 종료",
	                0.46f, 0.45f, 0.42f, 1.0f, FONT_SMALL);

	// ── 피격 섬광 ──
	if (m_HurtFlash > 0.0f)
	{
		m_R->DrawQuad(0.0f, 0.0f, (float)m_WinW, (float)m_WinH,
		              0.75f, 0.10f, 0.10f, m_HurtFlash * 0.22f);
	}

	// ── 알림 ──
	if (m_ToastTimer > 0.0f && m_ToastText != 0)
	{
		float a = Clamp01(m_ToastTimer / 0.9f);
		float w = m_R->TextWidth(m_ToastText, FONT_BODY) + 32.0f;

		m_R->DrawQuad(20.0f, 156.0f, w, 34.0f, 0.040f, 0.044f, 0.058f, 0.84f * a);
		m_R->DrawQuad(20.0f, 156.0f, 3.0f, 34.0f, 0.24f, 0.82f, 0.76f, 0.92f * a);

		m_R->DrawString(36.0f, 179.0f, m_ToastText, 0.62f, 0.92f, 0.88f, a, FONT_BODY);
	}

	// ── 레벨업 배너 ──
	if (m_LevelUpTimer > 0.0f)
	{
		float a  = Clamp01(m_LevelUpTimer / 0.8f);
		float cx = (float)m_WinW * 0.5f;
		float cy = (float)m_WinH * 0.30f;

		const wchar_t* t1 = L"레 벨 업";

		float w1 = m_R->TextWidth(t1, FONT_TITLE);

		m_R->DrawQuad(0.0f, cy - 34.0f, (float)m_WinW, 62.0f, 0.02f, 0.05f, 0.05f, 0.62f * a);
		m_R->DrawQuad(0.0f, cy - 34.0f, (float)m_WinW, 2.0f, 0.28f, 0.88f, 0.82f, 0.85f * a);
		m_R->DrawQuad(0.0f, cy + 26.0f, (float)m_WinW, 2.0f, 0.28f, 0.88f, 0.82f, 0.85f * a);

		m_R->DrawString(cx - w1 * 0.5f, cy + 6.0f, t1, 0.62f, 0.96f, 0.90f, a, FONT_TITLE);
	}

	// ── 진입 연출 ──
	if (m_RuinIntro > 0.0f)
	{
		float a  = Clamp01(m_RuinIntro / 1.5f);
		float cx = (float)m_WinW * 0.5f;
		float cy = (float)m_WinH * 0.40f;

		const wchar_t* t1 = L"가라앉은 채석장";
		const wchar_t* t2 = L"물이 빠진 자리에 길이 생겼다";

		float w1 = m_R->TextWidth(t1, FONT_TITLE);
		float w2 = m_R->TextWidth(t2, FONT_BODY);

		m_R->DrawQuad(0.0f, cy - 48.0f, (float)m_WinW, 96.0f, 0.012f, 0.020f, 0.024f, 0.72f * a);

		m_R->DrawString(cx - w1 * 0.5f, cy - 4.0f,  t1, 0.86f, 0.90f, 0.86f, a, FONT_TITLE);
		m_R->DrawString(cx - w2 * 0.5f, cy + 28.0f, t2, 0.52f, 0.62f, 0.60f, a, FONT_BODY);
	}

	// ── 능력치 분배 창 ──
	if (!m_StatsOpen)
		return;

	const float sw = 440.0f;
	const float sh = 268.0f;
	const float sx = (float)m_WinW * 0.5f - sw * 0.5f;
	const float sy = (float)m_WinH * 0.5f - sh * 0.5f;

	Panel(sx, sy, sw, sh, 0.95f);

	m_R->DrawString(sx + 22.0f, sy + 38.0f, L"능력치",
	                0.88f, 0.57f, 0.25f, 1.0f, FONT_TITLE);

	swprintf(buf, 192, L"남은 포인트  %d", m_Stats.statPoints);
	m_R->DrawString(sx + sw - 160.0f, sy + 38.0f, buf,
	                0.62f, 0.92f, 0.88f, 1.0f, FONT_BODY);

	struct Row
	{
		const wchar_t* key;
		const wchar_t* name;
		int            value;
		int            invested;
	};

	Row rows[3] =
	{
		{ L"[1]", L"체력", m_Stats.maxHp,   m_Stats.invested[STAT_VITALITY] },
		{ L"[2]", L"공격", m_Stats.attack,  m_Stats.invested[STAT_POWER]    },
		{ L"[3]", L"방어", m_Stats.defense, m_Stats.invested[STAT_GUARD]    },
	};

	for (int i = 0; i < 3; ++i)
	{
		float y = sy + 92.0f + (float)i * 40.0f;

		bool  can   = (m_Stats.statPoints > 0);
		float keyR  = can ? 0.95f : 0.42f;
		float keyG  = can ? 0.72f : 0.40f;
		float keyB  = can ? 0.28f : 0.38f;

		m_R->DrawString(sx + 24.0f, y, rows[i].key, keyR, keyG, keyB, 1.0f, FONT_BODY);
		m_R->DrawString(sx + 66.0f, y, rows[i].name, 0.91f, 0.89f, 0.84f, 1.0f, FONT_BODY);

		swprintf(buf, 192, L"%d", rows[i].value);
		m_R->DrawString(sx + 150.0f, y, buf, 0.95f, 0.90f, 0.80f, 1.0f, FONT_BODY);

		swprintf(buf, 192, L"( 투자 %d )", rows[i].invested);
		m_R->DrawString(sx + 220.0f, y, buf, 0.54f, 0.60f, 0.58f, 1.0f, FONT_SMALL);
	}

	swprintf(buf, 192, L"경험치는 처치로 쌓인다. 지금까지 %d 마리.", m_Killed);
	m_R->DrawString(sx + 24.0f, sy + sh - 40.0f, buf, 0.54f, 0.62f, 0.58f, 1.0f, FONT_SMALL);

	m_R->DrawString(sx + sw - 90.0f, sy + sh - 14.0f, L"[C] 닫기",
	                0.54f, 0.52f, 0.48f, 1.0f, FONT_SMALL);
}

void Game::RenderRuinEntities(float ox, float oy)
{
	// 정렬 목록에 이미 포함되어 RenderSorted 가 그린다.
	// 이 함수는 앞으로 몬스터 체력 막대 같은 머리 위 표시를 붙일 자리다.
	(void)ox;
	(void)oy;
}
