#include "stdafx.h"
#include "Game.h"
#include "Renderer.h"
#include "Input.h"
#include "Profiler.h"

#include <cmath>
#include <cstdio>
#include <cwchar>

// 1레벨(폐허)의 진입·배치·UI.
//
// 전투와 AI 는 전부 액터가 들고 있다 (Actors.cpp). 여기 남은 것은
// "레벨을 세우는 일" 과 "화면에 상태를 보여주는 일" 뿐이다.

namespace
{
	float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
}


// ─────────────────────────────────────────────────────────────────────────
//  레벨 전환
// ─────────────────────────────────────────────────────────────────────────

void Game::EnterRuin(unsigned int seed)
{
	m_RuinSeed = seed;
	m_Level    = LEVEL_RUIN;
	Prof::SetLabel("ruin");

	m_World.GenerateRuin(seed);

	// Scene::Clear 가 액터를 전부 지우므로 붙들고 있던 포인터도 함께 버린다.
	m_Scene.Clear();

	m_Player     = 0;
	m_Sigil      = 0;
	m_FocusNpc   = 0;
	m_FocusSigil = false;
	m_TalkingNpc = 0;

	m_DialogueOpen = false;

	SpawnPropsFromWorld();
	SpawnRuinMonsters();

	// 폐허에는 마을의 창문 불빛도 모닥불도 없다. 플레이어의 랜턴뿐이다.
	m_Lights.clear();

	m_Player = new PlayerActor();
	m_Player->x = m_World.SpawnX();
	m_Player->y = m_World.SpawnY();
	m_Player->combatEnabled = true;
	m_Scene.Add(m_Player);

	m_Killed      = 0;
	m_RuinIntro   = 4.0f;
	m_CamReady    = false;
	m_JournalOpen = false;
	m_StatsOpen   = false;

	Toast(L"가라앉은 채석장 — 깊이 1층");
}

void Game::ReturnToVillage()
{
	BuildVillageScene();

	Toast(L"라벤홀로 돌아왔다");
}

void Game::SpawnRuinMonsters()
{
	const std::vector<Room>& rooms = m_World.Rooms();

	unsigned int rng = m_RuinSeed ^ 0xA5A5u;
	int          made = 0;

	// 시작 방은 비워 둔다 — 들어가자마자 맞고 시작하면 곤란하다.
	for (size_t i = 1; i < rooms.size(); ++i)
	{
		const Room& rm = rooms[i];

		rng = rng * 1664525u + 1013904223u;
		int count = 2 + (int)((rng >> 8) % 4);

		for (int k = 0; k < count; ++k)
		{
			rng = rng * 1664525u + 1013904223u;
			float fx = (float)(rm.x0 + 1)
			         + (float)((rng >> 8) % 1000) * 0.001f * (float)(rm.x1 - rm.x0 - 2);

			rng = rng * 1664525u + 1013904223u;
			float fy = (float)(rm.y0 + 1)
			         + (float)((rng >> 8) % 1000) * 0.001f * (float)(rm.y1 - rm.y0 - 2);

			if (m_World.Blocked(fx, fy, 0.35f))
				continue;

			rng = rng * 1664525u + 1013904223u;

			// 안쪽 방일수록 험한 놈이 나온다.
			int kind = 0;

			if (i >= rooms.size() * 2 / 3)    kind = (int)((rng >> 8) % 3);
			else if (i >= rooms.size() / 3)   kind = (int)((rng >> 8) % 2);

			MonsterActor* m = new MonsterActor(kind);

			m->x = fx;
			m->y = fy;
			m->SetSeed((float)made * 1.9f);

			m_Scene.Add(m);

			++made;
		}
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  입력 — 전투는 PlayerActor 가 처리하고, 여기는 소비·분배만 본다
// ─────────────────────────────────────────────────────────────────────────

void Game::UpdateRuinInput()
{
	if (m_StatsOpen)
	{
		// 능력치 분배 — 남은 포인트가 있을 때만 반응한다.
		if (Input::Pressed('1') && m_Stats.Invest(STAT_VITALITY)) Toast(L"체력을 올렸다");
		if (Input::Pressed('2') && m_Stats.Invest(STAT_POWER))    Toast(L"공격을 올렸다");
		if (Input::Pressed('3') && m_Stats.Invest(STAT_GUARD))    Toast(L"방어를 올렸다");

		return;
	}

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
	if (m_Player != 0 && m_Player->HurtFlash() > 0.0f)
	{
		m_R->DrawQuad(0.0f, 0.0f, (float)m_WinW, (float)m_WinH,
		              0.75f, 0.10f, 0.10f, m_Player->HurtFlash() * 0.22f);
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

		bool  can  = (m_Stats.statPoints > 0);
		float keyR = can ? 0.95f : 0.42f;
		float keyG = can ? 0.72f : 0.40f;
		float keyB = can ? 0.28f : 0.38f;

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
