#include "stdafx.h"
#include "World.h"
#include "Profiler.h"

#include <cmath>

// 호수 — 타원 하나로 잡고 가장자리를 노이즈로 흔들어 원형 티를 없앤다.
static const float LAKE_CX = 78.0f;
static const float LAKE_CY = 82.0f;
static const float LAKE_RX = 22.0f;
static const float LAKE_RY = 18.0f;

// 마을 + 밭이 차지하는 영역. 이 안에는 숲을 자동 생성하지 않는다.
static const float TOWN_X0 = 25.0f, TOWN_X1 = 58.0f;
static const float TOWN_Y0 = 25.0f, TOWN_Y1 = 64.0f;

static int ClampInt(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static float Hash01(int x, int y)
{
	unsigned int h = (unsigned int)(x * 374761393) + (unsigned int)(y * 668265263);
	h = (h ^ (h >> 13)) * 1274126177u;

	return (float)((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
}

static float SmoothNoise(float x, float y)
{
	int   xi = (int)floorf(x);
	int   yi = (int)floorf(y);
	float xf = x - (float)xi;
	float yf = y - (float)yi;

	float u = xf * xf * (3.0f - 2.0f * xf);
	float v = yf * yf * (3.0f - 2.0f * yf);

	float a = Hash01(xi,     yi);
	float b = Hash01(xi + 1, yi);
	float c = Hash01(xi,     yi + 1);
	float d = Hash01(xi + 1, yi + 1);

	return (a * (1.0f - u) + b * u) * (1.0f - v)
	     + (c * (1.0f - u) + d * u) * v;
}

float World::Rand01()
{
	m_Rng = m_Rng * 1664525u + 1013904223u;

	return (float)((m_Rng >> 8) & 0xFFFFFFu) / 16777216.0f;
}

TileType World::Tile(int x, int y) const
{
	// 맵 밖은 통행 불가로 취급한다.
	if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
		return TILE_WATER;

	return m_Tiles[y][x];
}

float World::Variation(int x, int y) const
{
	if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
		return 0.5f;

	return m_Variation[y][x];
}

void World::Add(ObjectType type, float x, float y,
                float halfW, float halfH, float width, float height, bool blocking)
{
	WorldObject o;

	o.type     = type;
	o.x        = x;
	o.y        = y;
	o.halfW    = halfW;
	o.halfH    = halfH;
	o.width    = width;
	o.height   = height;
	o.blocking = blocking;
	o.variant  = (int)(Rand01() * 1000.0f);

	m_Objects.push_back(o);
}


// ─────────────────────────────────────────────────────────────────────────
//  지형 생성
// ─────────────────────────────────────────────────────────────────────────

void World::CarvePath(float ax, float ay, float bx, float by, float radius)
{
	float dx  = bx - ax;
	float dy  = by - ay;
	float len = sqrtf(dx * dx + dy * dy);

	if (len < 0.001f)
		return;

	int steps = (int)(len * 4.0f);

	for (int s = 0; s <= steps; ++s)
	{
		float t  = (float)s / (float)steps;
		float px = ax + dx * t;
		float py = ay + dy * t;

		// 길 가장자리를 살짝 흔들어 자로 그은 티를 없앤다.
		float wob = (SmoothNoise(px * 0.3f, py * 0.3f) - 0.5f) * 0.6f;
		float r   = radius + wob;

		int x0 = (int)floorf(px - r), x1 = (int)floorf(px + r);
		int y0 = (int)floorf(py - r), y1 = (int)floorf(py + r);

		for (int ty = y0; ty <= y1; ++ty)
		for (int tx = x0; tx <= x1; ++tx)
		{
			if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H)
				continue;

			float ddx = ((float)tx + 0.5f) - px;
			float ddy = ((float)ty + 0.5f) - py;
			float d   = sqrtf(ddx * ddx + ddy * ddy);

			// 나무 배제용 여유 폭 — 길보다 넓게 잡는다.
			if (d < r + 1.5f)
				m_NearPath[ty][tx] = true;

			if (d < r && m_Tiles[ty][tx] != TILE_WATER && m_Tiles[ty][tx] != TILE_SHALLOW)
				m_Tiles[ty][tx] = TILE_PATH;
		}
	}
}

void World::ForceSand(float cx, float cy, float radius)
{
	int x0 = (int)floorf(cx - radius), x1 = (int)floorf(cx + radius);
	int y0 = (int)floorf(cy - radius), y1 = (int)floorf(cy + radius);

	for (int ty = y0; ty <= y1; ++ty)
	for (int tx = x0; tx <= x1; ++tx)
	{
		if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H)
			continue;

		float ddx = ((float)tx + 0.5f) - cx;
		float ddy = ((float)ty + 0.5f) - cy;

		if (ddx * ddx + ddy * ddy > radius * radius)
			continue;

		m_Tiles[ty][tx] = TILE_SAND;
	}
}

// 타일만 보고 걸을 수 있는지 — 오브젝트는 보지 않는다.
static bool TileWalkable(TileType t)
{
	return t != TILE_WALL && t != TILE_WATER;
}

void World::Generate()
{
	m_Kind   = LEVEL_VILLAGE;
	m_SpawnX = SPAWN_X;
	m_SpawnY = SPAWN_Y;

	m_Rng = 0x5EED1234u;
	m_Objects.clear();
	m_Rooms.clear();

	for (int i = 0; i < GRID_W * GRID_H; ++i)
		m_Grid[i].clear();

	// ── 1) 기본 지면 ────────────────────────────────────────────────
	for (int y = 0; y < MAP_H; ++y)
	for (int x = 0; x < MAP_W; ++x)
	{
		float n = SmoothNoise((float)x * 0.09f, (float)y * 0.09f);

		m_Tiles[y][x]     = (n > 0.57f) ? TILE_MEADOW : TILE_GRASS;
		m_Variation[y][x] = Hash01(x, y);
		m_NearPath[y][x]  = false;
	}

	// ── 2) 호수 ─────────────────────────────────────────────────────
	for (int y = 0; y < MAP_H; ++y)
	for (int x = 0; x < MAP_W; ++x)
	{
		float dx = ((float)x - LAKE_CX) / LAKE_RX;
		float dy = ((float)y - LAKE_CY) / LAKE_RY;
		float d  = dx * dx + dy * dy;

		d += (SmoothNoise((float)x * 0.15f + 7.0f, (float)y * 0.15f + 3.0f) - 0.5f) * 0.32f;

		if      (d < 0.74f) m_Tiles[y][x] = TILE_WATER;
		else if (d < 0.94f) m_Tiles[y][x] = TILE_SHALLOW;
		else if (d < 1.18f) m_Tiles[y][x] = TILE_SAND;
	}

	// ── 3) 길 ───────────────────────────────────────────────────────
	CarvePath(VILLAGE_CX, VILLAGE_CY, SPAWN_X, 16.0f, 1.7f);       // 북문 — 숲을 통과해 들어온다
	CarvePath(VILLAGE_CX, VILLAGE_CY, SHORE_X, SHORE_Y, 1.4f);     // 호숫가
	CarvePath(VILLAGE_CX, VILLAGE_CY, 30.0f, 56.0f, 1.2f);         // 밭
	CarvePath(VILLAGE_CX, VILLAGE_CY, 60.0f, 32.0f, 1.1f);         // 동쪽 들길

	// 외곽으로 뻗는 길. 중간에 갈라지는 지점을 두어 한 줄기로 보이지 않게 한다.
	CarvePath(60.0f, 32.0f, CAMP_SILENCE_X, CAMP_SILENCE_Y, 1.2f);            // 동 — 수도회
	CarvePath(SHORE_X, SHORE_Y, 110.0f, 120.0f, 1.2f);                        // 남동 — 호수 건너
	CarvePath(110.0f, 120.0f, CAMP_BANDIT_X, CAMP_BANDIT_Y, 1.0f);            //      산적
	CarvePath(110.0f, 120.0f, MARSH_X, MARSH_Y, 1.0f);                        //      늪
	CarvePath(30.0f, 56.0f, FARM_OUT_X, FARM_OUT_Y, 1.1f);                    // 남 — 외딴 농가
	CarvePath(FARM_OUT_X, FARM_OUT_Y, SHRINE_CHOIR_X, SHRINE_CHOIR_Y, 0.9f);  //      성가대

	// 비석이 설 자리는 마른 모래로 만들어 둔다.
	ForceSand(SIGIL_X, SIGIL_Y, 3.4f);

	// ── 4) 배치 ─────────────────────────────────────────────────────
	PlaceVillage();
	PlaceFarm();
	PlaceOutposts();
	PlaceShore();
	PlaceForest();

	BuildGrid();
}


// ─────────────────────────────────────────────────────────────────────────
//  배치
// ─────────────────────────────────────────────────────────────────────────

void World::PlaceVillage()
{
	// 여관과 예배당 — 마을에서 가장 큰 건물 둘
	Add(OBJ_INN,    34.0f, 34.0f, 2.1f, 2.1f, 168.0f, 136.0f, true);
	Add(OBJ_CHAPEL, 36.0f, 27.5f, 1.9f, 1.9f, 140.0f, 158.0f, true);

	// 일반 가옥
	Add(OBJ_HOUSE, 46.0f, 33.0f, 1.6f, 1.6f, 124.0f, 108.0f, true);   // 대장간
	Add(OBJ_HOUSE, 32.0f, 46.0f, 1.6f, 1.6f, 118.0f, 102.0f, true);
	Add(OBJ_HOUSE, 47.0f, 45.5f, 1.6f, 1.6f, 126.0f, 110.0f, true);
	Add(OBJ_HOUSE, 45.5f, 28.0f, 1.5f, 1.5f, 112.0f,  98.0f, true);
	Add(OBJ_HOUSE, 52.0f, 40.0f, 1.6f, 1.6f, 122.0f, 106.0f, true);
	Add(OBJ_HOUSE, 29.5f, 39.5f, 1.6f, 1.6f, 120.0f, 104.0f, true);
	Add(OBJ_HOUSE, 38.5f, 50.5f, 1.6f, 1.6f, 116.0f, 100.0f, true);
	Add(OBJ_HOUSE, 50.5f, 52.0f, 1.5f, 1.5f, 114.0f,  98.0f, true);

	// 광장 시설물
	Add(OBJ_WELL,     44.0f, 36.0f, 0.8f, 0.8f, 54.0f, 50.0f, true);
	Add(OBJ_BOARD,    36.2f, 43.2f, 0.5f, 0.4f, 48.0f, 56.0f, true);
	Add(OBJ_CAMPFIRE, 41.0f, 43.0f, 0.7f, 0.7f, 46.0f, 40.0f, true);

	// 호숫길 초입의 등불 — 마을 밖으로 나가는 동선을 밝혀 준다
	Add(OBJ_LANTERN, 46.5f, 48.0f, 0.35f, 0.35f, 26.0f, 62.0f, true);
	Add(OBJ_LANTERN, 52.0f, 54.5f, 0.35f, 0.35f, 26.0f, 62.0f, true);
	Add(OBJ_LANTERN, 38.0f, 34.5f, 0.35f, 0.35f, 26.0f, 62.0f, true);

	// 마을 안 나무와 덤불 — 완전히 휑하지 않게
	Add(OBJ_TREE, 30.0f, 30.0f, 0.4f, 0.4f, 54.0f, 108.0f, true);
	Add(OBJ_TREE, 50.5f, 30.5f, 0.4f, 0.4f, 50.0f, 100.0f, true);
	Add(OBJ_TREE, 35.0f, 54.0f, 0.4f, 0.4f, 56.0f, 112.0f, true);
	Add(OBJ_TREE, 54.5f, 47.0f, 0.4f, 0.4f, 48.0f,  96.0f, true);

	Add(OBJ_BUSH, 42.5f, 31.0f, 0.4f, 0.4f, 38.0f, 30.0f, true);
	Add(OBJ_BUSH, 31.5f, 43.0f, 0.4f, 0.4f, 36.0f, 28.0f, true);

	// 잡동사니
	Add(OBJ_CRATE, 45.0f, 34.5f, 0.4f, 0.4f, 32.0f, 28.0f, true);
	Add(OBJ_CRATE, 35.2f, 36.0f, 0.4f, 0.4f, 30.0f, 26.0f, true);
	Add(OBJ_CRATE, 48.5f, 44.0f, 0.4f, 0.4f, 30.0f, 26.0f, true);
}

void World::PlaceFarm()
{
	// 마을 남서쪽 밭. 울타리를 두르고 길이 들어오는 쪽에 출입구를 낸다.
	const float x0 = 26.0f, x1 = 36.0f;
	const float y0 = 52.0f, y1 = 62.0f;

	for (float x = x0; x <= x1; x += 1.0f)
	{
		bool gate = (x > 29.0f && x < 31.5f);

		if (!gate)
			Add(OBJ_FENCE, x, y0, 0.45f, 0.35f, 30.0f, 26.0f, true);

		Add(OBJ_FENCE, x, y1, 0.45f, 0.35f, 30.0f, 26.0f, true);
	}

	for (float y = y0 + 1.0f; y < y1; y += 1.0f)
	{
		Add(OBJ_FENCE, x0, y, 0.35f, 0.45f, 30.0f, 26.0f, true);
		Add(OBJ_FENCE, x1, y, 0.35f, 0.45f, 30.0f, 26.0f, true);
	}

	Add(OBJ_CRATE, 34.6f, 60.0f, 0.4f, 0.4f, 32.0f, 28.0f, true);
	Add(OBJ_STUMP, 27.5f, 58.0f, 0.4f, 0.4f, 34.0f, 22.0f, true);
}

// 거점이 설 자리를 비운다.
//
// PlaceForest 는 m_NearPath 가 선 칸을 건너뛴다. 야영지를 길과 같은 취급으로
// 표시해 두면 모닥불 위에 소나무가 자라는 일이 없다.
void World::ClearArea(float cx, float cy, float radius)
{
	int x0 = (int)(cx - radius), x1 = (int)(cx + radius);
	int y0 = (int)(cy - radius), y1 = (int)(cy + radius);

	for (int y = y0; y <= y1; ++y)
	for (int x = x0; x <= x1; ++x)
	{
		if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
			continue;

		float dx = (float)x + 0.5f - cx;
		float dy = (float)y + 0.5f - cy;

		if (dx * dx + dy * dy > radius * radius)
			continue;

		// 물가는 건드리지 않는다. 늪 오두막이 호수를 메우면 곤란하다.
		if (m_Tiles[y][x] == TILE_WATER || m_Tiles[y][x] == TILE_SHALLOW)
			continue;

		m_NearPath[y][x] = true;
	}
}

// 마을 바깥의 세력별 근거지 (STORY.md §2-3, §2-5-1).
//
// 각 거점은 "누가 사는가"가 배치로 읽혀야 한다 — 산적은 울타리 없이 불만 피우고,
// 수도회는 줄을 맞춰 세우고, 성가대는 무너진 사당에 숨는다.
void World::PlaceOutposts()
{
	// ── 까마귀발 산적단 야영지 ──
	// 훔쳐 온 것들이 아무렇게나 쌓여 있다. 울타리는 반쪽뿐이다.
	{
		const float cx = CAMP_BANDIT_X, cy = CAMP_BANDIT_Y;

		ClearArea(cx, cy, 9.0f);

		Add(OBJ_CAMPFIRE, cx, cy, 0.7f, 0.7f, 50.0f, 44.0f, true);

		Add(OBJ_CRATE, cx - 3.2f, cy - 2.0f, 0.4f, 0.4f, 34.0f, 30.0f, true);
		Add(OBJ_CRATE, cx - 2.4f, cy + 2.8f, 0.4f, 0.4f, 30.0f, 26.0f, true);
		Add(OBJ_CRATE, cx + 3.6f, cy + 1.2f, 0.4f, 0.4f, 32.0f, 28.0f, true);
		Add(OBJ_CRATE, cx + 1.0f, cy - 3.6f, 0.4f, 0.4f, 28.0f, 24.0f, true);

		Add(OBJ_STUMP, cx - 4.6f, cy + 0.4f, 0.4f, 0.4f, 36.0f, 24.0f, true);
		Add(OBJ_STUMP, cx + 4.8f, cy - 1.6f, 0.4f, 0.4f, 34.0f, 22.0f, true);

		// 반쪽짜리 방책 — 북쪽만 막았다
		for (float x = cx - 5.0f; x <= cx + 5.0f; x += 1.0f)
			Add(OBJ_FENCE, x, cy - 6.0f, 0.45f, 0.35f, 30.0f, 26.0f, true);

		Add(OBJ_ROCK, cx - 7.0f, cy + 5.0f, 0.4f, 0.35f, 40.0f, 26.0f, true);
		Add(OBJ_ROCK, cx + 6.4f, cy + 4.2f, 0.4f, 0.35f, 36.0f, 22.0f, true);
	}

	// ── 침묵의 수도회 순찰 야영지 ──
	// 천막 대신 상자를 줄 맞춰 세우고, 게시판을 걸었다. 질서가 보이게.
	{
		const float cx = CAMP_SILENCE_X, cy = CAMP_SILENCE_Y;

		ClearArea(cx, cy, 8.0f);

		Add(OBJ_CAMPFIRE, cx, cy, 0.7f, 0.7f, 46.0f, 40.0f, true);
		Add(OBJ_BOARD,    cx + 2.6f, cy - 3.0f, 0.5f, 0.4f, 48.0f, 56.0f, true);

		for (int i = 0; i < 4; ++i)
		{
			float ox = -3.0f + (float)i * 2.0f;

			Add(OBJ_CRATE, cx + ox, cy + 3.4f, 0.4f, 0.4f, 30.0f, 26.0f, true);
		}

		Add(OBJ_LANTERN, cx - 4.4f, cy - 1.0f, 0.35f, 0.35f, 26.0f, 62.0f, true);
		Add(OBJ_LANTERN, cx + 4.4f, cy - 1.0f, 0.35f, 0.35f, 26.0f, 62.0f, true);

		for (float y = cy - 5.0f; y <= cy + 5.0f; y += 1.0f)
		{
			Add(OBJ_FENCE, cx - 6.0f, y, 0.35f, 0.45f, 30.0f, 26.0f, true);
			Add(OBJ_FENCE, cx + 6.0f, y, 0.35f, 0.45f, 30.0f, 26.0f, true);
		}
	}

	// ── 성가대 은신처 ──
	// 숲에 파묻힌 폐사당. 비석이 하나 서 있고 등불은 없다.
	{
		const float cx = SHRINE_CHOIR_X, cy = SHRINE_CHOIR_Y;

		ClearArea(cx, cy, 8.0f);

		Add(OBJ_CHAPEL, cx, cy - 1.0f, 1.9f, 1.9f, 140.0f, 158.0f, true);
		Add(OBJ_SIGIL,  cx, cy + 3.4f, 0.5f, 0.5f,  48.0f,  58.0f, true);

		Add(OBJ_ROCK, cx - 4.0f, cy + 2.6f, 0.4f, 0.35f, 38.0f, 24.0f, true);
		Add(OBJ_ROCK, cx + 4.2f, cy + 3.0f, 0.4f, 0.35f, 34.0f, 22.0f, true);

		Add(OBJ_PINE, cx - 6.0f, cy - 4.0f, 0.4f, 0.4f, 52.0f, 116.0f, true);
		Add(OBJ_PINE, cx + 6.2f, cy - 3.4f, 0.4f, 0.4f, 50.0f, 110.0f, true);
		Add(OBJ_PINE, cx - 5.4f, cy + 5.6f, 0.4f, 0.4f, 48.0f, 104.0f, true);
	}

	// ── 외딴 농가 ──
	{
		const float cx = FARM_OUT_X, cy = FARM_OUT_Y;

		ClearArea(cx, cy, 9.0f);

		Add(OBJ_HOUSE, cx, cy, 1.6f, 1.6f, 120.0f, 104.0f, true);
		Add(OBJ_WELL,  cx + 3.6f, cy + 2.6f, 0.8f, 0.8f, 54.0f, 50.0f, true);

		Add(OBJ_CRATE, cx - 3.0f, cy + 2.4f, 0.4f, 0.4f, 32.0f, 28.0f, true);
		Add(OBJ_STUMP, cx + 4.6f, cy - 2.2f, 0.4f, 0.4f, 34.0f, 22.0f, true);

		for (float x = cx - 5.0f; x <= cx + 5.0f; x += 1.0f)
		{
			bool gate = (x > cx - 1.5f && x < cx + 1.5f);

			if (!gate)
				Add(OBJ_FENCE, x, cy + 6.0f, 0.45f, 0.35f, 30.0f, 26.0f, true);
		}
	}

	// ── 늪 초입의 오두막 ──
	// 약초꾼은 어느 편도 아니다. 그래서 아무도 지켜 주지 않는 자리에 산다.
	{
		const float cx = MARSH_X, cy = MARSH_Y;

		ClearArea(cx, cy, 7.0f);

		Add(OBJ_HOUSE, cx, cy, 1.5f, 1.5f, 108.0f, 94.0f, true);

		Add(OBJ_STUMP, cx - 3.4f, cy + 2.4f, 0.4f, 0.4f, 34.0f, 22.0f, true);
		Add(OBJ_ROCK,  cx + 3.6f, cy + 1.8f, 0.4f, 0.35f, 36.0f, 22.0f, true);
		Add(OBJ_BUSH,  cx - 2.6f, cy - 3.0f, 0.4f, 0.4f, 38.0f, 30.0f, true);
		Add(OBJ_BUSH,  cx + 2.8f, cy - 2.6f, 0.4f, 0.4f, 36.0f, 28.0f, true);
	}
}

void World::PlaceShore()
{
	// 비석 — 이 프로토타입의 목적지
	Add(OBJ_SIGIL, SIGIL_X, SIGIL_Y, 0.5f, 0.5f, 48.0f, 58.0f, true);

	// 어부가 두고 간 것들
	Add(OBJ_CRATE, 60.4f, 64.0f, 0.4f, 0.4f,  32.0f, 28.0f, true);
	Add(OBJ_CRATE, 65.6f, 69.0f, 0.4f, 0.4f,  30.0f, 26.0f, true);
	Add(OBJ_ROCK,  59.0f, 69.5f, 0.4f, 0.35f, 36.0f, 22.0f, true);
	Add(OBJ_ROCK,  66.5f, 64.5f, 0.4f, 0.35f, 32.0f, 20.0f, true);

	// 얕은 물 가장자리의 갈대 — 통행은 막지 않는다
	for (int y = 0; y < MAP_H; ++y)
	for (int x = 0; x < MAP_W; ++x)
	{
		if (m_Tiles[y][x] != TILE_SHALLOW)
			continue;

		if (Rand01() > 0.14f)
			continue;

		Add(OBJ_REED, (float)x + 0.5f, (float)y + 0.5f,
		    0.0f, 0.0f, 22.0f, 36.0f, false);
	}
}

void World::PlaceForest()
{
	for (int y = 0; y < MAP_H; ++y)
	for (int x = 0; x < MAP_W; ++x)
	{
		TileType t = m_Tiles[y][x];

		if (t == TILE_WATER || t == TILE_SHALLOW || t == TILE_SAND || t == TILE_PATH)
			continue;

		if (m_NearPath[y][x])
			continue;

		float fx = (float)x + 0.5f;
		float fy = (float)y + 0.5f;

		// 마을과 밭은 비워 둔다.
		if (fx > TOWN_X0 && fx < TOWN_X1 && fy > TOWN_Y0 && fy < TOWN_Y1)
			continue;

		// 가장자리로 갈수록 빽빽해지는 숲 띠
		int edge = x;
		if (y < edge)              edge = y;
		if (MAP_W - 1 - x < edge)  edge = MAP_W - 1 - x;
		if (MAP_H - 1 - y < edge)  edge = MAP_H - 1 - y;

		float density = 0.0f;
		if (edge < 14)
			density = 1.0f - (float)edge / 14.0f;

		// 안쪽에는 뭉친 군락
		float clump = SmoothNoise((float)x * 0.075f + 31.0f, (float)y * 0.075f + 17.0f);

		if (clump > 0.60f)
		{
			float c = (clump - 0.60f) * 2.4f;

			if (c > density)
				density = c;
		}

		float roll = Rand01();

		if (roll >= density * 0.80f)
		{
			// 나무가 안 서는 자리에는 가끔 덤불이나 그루터기
			if (density > 0.15f && roll > 0.94f)
			{
				if (Rand01() > 0.35f)
					Add(OBJ_BUSH, fx, fy, 0.4f, 0.4f, 34.0f + Rand01() * 12.0f, 26.0f, true);
				else
					Add(OBJ_STUMP, fx, fy, 0.38f, 0.38f, 32.0f, 20.0f, true);
			}

			continue;
		}

		float scale = 0.82f + Rand01() * 0.55f;

		// 북쪽 숲은 침엽수, 남쪽은 활엽수로 갈라 지역감을 준다.
		ObjectType kind = ((float)y < LAKE_CY - 10.0f && Rand01() > 0.35f) ? OBJ_PINE : OBJ_TREE;

		Add(kind,
		    fx + (Rand01() - 0.5f) * 0.55f,
		    fy + (Rand01() - 0.5f) * 0.55f,
		    0.38f, 0.38f,
		    50.0f * scale, 104.0f * scale, true);
	}

	// 들판에 굴러다니는 바위
	for (int y = 0; y < MAP_H; ++y)
	for (int x = 0; x < MAP_W; ++x)
	{
		TileType t = m_Tiles[y][x];

		if (t != TILE_GRASS && t != TILE_MEADOW && t != TILE_SAND)
			continue;

		if (m_NearPath[y][x] || Rand01() > 0.006f)
			continue;

		Add(OBJ_ROCK, (float)x + 0.5f, (float)y + 0.5f,
		    0.35f, 0.3f, 34.0f, 22.0f, true);
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  충돌
// ─────────────────────────────────────────────────────────────────────────

void World::BuildGrid()
{
	for (size_t i = 0; i < m_Objects.size(); ++i)
	{
		const WorldObject& o = m_Objects[i];

		if (!o.blocking)
			continue;

		// 조회 반경(최대 1타일)을 미리 얹어 셀 경계에서 놓치는 일이 없게 한다.
		const float pad = 1.0f;

		int cx0 = ClampInt((int)floorf((o.x - o.halfW - pad) / (float)GRID_CELL), 0, GRID_W - 1);
		int cx1 = ClampInt((int)floorf((o.x + o.halfW + pad) / (float)GRID_CELL), 0, GRID_W - 1);
		int cy0 = ClampInt((int)floorf((o.y - o.halfH - pad) / (float)GRID_CELL), 0, GRID_H - 1);
		int cy1 = ClampInt((int)floorf((o.y + o.halfH + pad) / (float)GRID_CELL), 0, GRID_H - 1);

		for (int cy = cy0; cy <= cy1; ++cy)
		for (int cx = cx0; cx <= cx1; ++cx)
			m_Grid[cy * GRID_W + cx].push_back((int)i);
	}
}

bool World::Blocked(float x, float y, float radius) const
{
	Prof::Add(PROF_COLLIDE_CALLS);

	if (x < radius || y < radius || x > MAP_W - radius || y > MAP_H - radius)
		return true;

	// 타일 — 깊은 물과 벽이 막는다. 얕은 물은 걸어 들어갈 수 있다.
	int x0 = (int)floorf(x - radius), x1 = (int)floorf(x + radius);
	int y0 = (int)floorf(y - radius), y1 = (int)floorf(y + radius);

	for (int ty = y0; ty <= y1; ++ty)
	for (int tx = x0; tx <= x1; ++tx)
	{
		if (!TileWalkable(Tile(tx, ty)))
			return true;
	}

	// 오브젝트 — 자기가 속한 격자 셀만 본다.
	int cx = ClampInt((int)floorf(x / (float)GRID_CELL), 0, GRID_W - 1);
	int cy = ClampInt((int)floorf(y / (float)GRID_CELL), 0, GRID_H - 1);

	const std::vector<int>& cell = m_Grid[cy * GRID_W + cx];

	Prof::Add(PROF_COLLIDE_CANDIDATES, (int)cell.size());

	for (size_t i = 0; i < cell.size(); ++i)
	{
		const WorldObject& o = m_Objects[cell[i]];

		if (fabsf(x - o.x) < o.halfW + radius &&
		    fabsf(y - o.y) < o.halfH + radius)
			return true;
	}

	return false;
}

bool World::IsOpenGround(float x, float y) const
{
	TileType t = Tile((int)floorf(x), (int)floorf(y));

	if (t != TILE_GRASS && t != TILE_MEADOW && t != TILE_FLOOR)
		return false;

	return !Blocked(x, y, 0.5f);
}


// ─────────────────────────────────────────────────────────────────────────
//  1레벨 — 랜덤 폐허
// ─────────────────────────────────────────────────────────────────────────

// 시작점에서 네 방향으로 퍼지며 닿는 칸을 표시한다.
static void FloodFill(const std::vector<unsigned char>& open, int sx, int sy,
                      std::vector<unsigned char>& out)
{
	out.assign(open.size(), 0);

	if (sx < 0 || sy < 0 || sx >= MAP_W || sy >= MAP_H)
		return;

	int start = sy * MAP_W + sx;

	if (!open[start])
		return;

	std::vector<int> stack;
	stack.reserve(2048);
	stack.push_back(start);
	out[start] = 1;

	const int dx[4] = { 1, -1, 0,  0 };
	const int dy[4] = { 0,  0, 1, -1 };

	while (!stack.empty())
	{
		int i = stack.back();
		stack.pop_back();

		int x = i % MAP_W;
		int y = i / MAP_W;

		for (int k = 0; k < 4; ++k)
		{
			int nx = x + dx[k];
			int ny = y + dy[k];

			if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H)
				continue;

			int ni = ny * MAP_W + nx;

			if (out[ni] || !open[ni])
				continue;

			out[ni] = 1;
			stack.push_back(ni);
		}
	}
}

void World::CarveRect(int x0, int y0, int x1, int y1, TileType t)
{
	if (x0 > x1) { int tmp = x0; x0 = x1; x1 = tmp; }
	if (y0 > y1) { int tmp = y0; y0 = y1; y1 = tmp; }

	for (int y = y0; y <= y1; ++y)
	for (int x = x0; x <= x1; ++x)
	{
		if (x < 1 || y < 1 || x >= MAP_W - 1 || y >= MAP_H - 1)
			continue;

		m_Tiles[y][x] = t;
	}
}

void World::CarveCorridor(int ax, int ay, int bx, int by, int half)
{
	// ㄱ자 — 가로로 먼저 가고 세로로 꺾는다.
	CarveRect(ax, ay - half, bx, ay + half, TILE_FLOOR);
	CarveRect(bx - half, ay, bx + half, by, TILE_FLOOR);

	// 복도에는 통행을 막는 물건을 놓지 않는다. m_NearPath 를 그 표식으로 쓴다.
	int lox = (ax < bx ? ax : bx) - 1;
	int hix = (ax > bx ? ax : bx) + 1;
	int loy = (ay < by ? ay : by) - 1;
	int hiy = (ay > by ? ay : by) + 1;

	for (int y = ay - half - 1; y <= ay + half + 1; ++y)
	for (int x = lox; x <= hix; ++x)
	{
		if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H)
			m_NearPath[y][x] = true;
	}

	for (int y = loy; y <= hiy; ++y)
	for (int x = bx - half - 1; x <= bx + half + 1; ++x)
	{
		if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H)
			m_NearPath[y][x] = true;
	}
}

void World::PlaceRuinObjects()
{
	for (size_t i = 0; i < m_Rooms.size(); ++i)
	{
		const Room& rm = m_Rooms[i];

		// 기둥 — 네 칸 간격의 열주. 복도로 쓰이는 줄은 비운다.
		for (int y = rm.y0 + 2; y <= rm.y1 - 2; y += 4)
		for (int x = rm.x0 + 2; x <= rm.x1 - 2; x += 4)
		{
			if (m_NearPath[y][x] || m_Tiles[y][x] != TILE_FLOOR)
				continue;

			if (Rand01() > 0.55f)
				continue;

			Add(OBJ_PILLAR, (float)x + 0.5f, (float)y + 0.5f,
			    0.45f, 0.45f, 40.0f, 92.0f, true);
		}

		// 부스러기 — 통행은 막지 않는 장식
		int rubble = 3 + (int)(Rand01() * 6.0f);

		for (int k = 0; k < rubble; ++k)
		{
			int x = rm.x0 + 1 + (int)(Rand01() * (float)(rm.x1 - rm.x0 - 1));
			int y = rm.y0 + 1 + (int)(Rand01() * (float)(rm.y1 - rm.y0 - 1));

			if (m_Tiles[y][x] != TILE_FLOOR)
				continue;

			Add(OBJ_RUBBLE, (float)x + 0.5f, (float)y + 0.5f,
			    0.0f, 0.0f, 36.0f, 18.0f, false);
		}

		// 상자 — 방마다 최대 하나
		if (Rand01() < 0.45f)
		{
			int x = rm.x0 + 1 + (int)(Rand01() * (float)(rm.x1 - rm.x0 - 1));
			int y = rm.y0 + 1 + (int)(Rand01() * (float)(rm.y1 - rm.y0 - 1));

			if (m_Tiles[y][x] == TILE_FLOOR && !m_NearPath[y][x])
			{
				Add(OBJ_CRATE, (float)x + 0.5f, (float)y + 0.5f,
				    0.4f, 0.4f, 32.0f, 28.0f, true);
			}
		}
	}
}

void World::BuildWallShell()
{
	// 바닥에 면한 벽만 실제 덩어리로 세운다. 안쪽 벽은 보이지 않으므로 만들지 않는다.
	// 벽을 오브젝트로 두면 캐릭터와 같은 목록에서 정렬되어 앞뒤가 맞는다.
	const int dx[4] = { 1, -1, 0,  0 };
	const int dy[4] = { 0,  0, 1, -1 };

	for (int y = 1; y < MAP_H - 1; ++y)
	for (int x = 1; x < MAP_W - 1; ++x)
	{
		if (m_Tiles[y][x] != TILE_WALL)
			continue;

		bool facesFloor = false;

		for (int k = 0; k < 4; ++k)
		{
			if (TileWalkable(m_Tiles[y + dy[k]][x + dx[k]]))
			{
				facesFloor = true;
				break;
			}
		}

		if (!facesFloor)
			continue;

		// 통행 판정은 타일이 이미 막고 있으므로 오브젝트는 그리기 전용이다.
		Add(OBJ_WALL, (float)x + 0.5f, (float)y + 0.5f,
		    0.0f, 0.0f, 64.0f, 54.0f, false);
	}
}

void World::RepairConnectivity()
{
	const size_t total = (size_t)MAP_W * MAP_H;

	std::vector<unsigned char> tileOpen(total, 0);

	for (int y = 0; y < MAP_H; ++y)
	for (int x = 0; x < MAP_W; ++x)
		tileOpen[(size_t)y * MAP_W + x] = TileWalkable(m_Tiles[y][x]) ? 1 : 0;

	int sx = (int)m_SpawnX;
	int sy = (int)m_SpawnY;

	// ── 1) 타일만 보고 닿는 범위 ──
	// 구조적으로 끊긴 칸은 벽으로 메운다. 갈 수 없는 방은 없느니만 못하다.
	std::vector<unsigned char> reach;
	FloodFill(tileOpen, sx, sy, reach);

	for (int y = 0; y < MAP_H; ++y)
	for (int x = 0; x < MAP_W; ++x)
	{
		size_t i = (size_t)y * MAP_W + x;

		if (tileOpen[i] && !reach[i])
		{
			m_Tiles[y][x] = TILE_WALL;
			tileOpen[i]   = 0;
		}
	}

	// ── 2) 오브젝트까지 포함해 다시 확인 ──
	for (int pass = 0; pass < 4; ++pass)
	{
		BuildGrid();

		std::vector<unsigned char> open(total, 0);

		for (int y = 0; y < MAP_H; ++y)
		for (int x = 0; x < MAP_W; ++x)
		{
			size_t i = (size_t)y * MAP_W + x;

			if (!tileOpen[i])
				continue;

			open[i] = Blocked((float)x + 0.5f, (float)y + 0.5f, 0.30f) ? 0 : 1;
		}

		std::vector<unsigned char> got;
		FloodFill(open, sx, sy, got);

		// 열려 있는데 닿지 못한 칸 = 오브젝트가 길을 막았다.
		std::vector<unsigned char> orphan(total, 0);
		bool anyOrphan = false;

		for (size_t i = 0; i < total; ++i)
		{
			if (open[i] && !got[i])
			{
				orphan[i] = 1;
				anyOrphan = true;
			}
		}

		if (!anyOrphan)
			break;

		// 고립된 칸과 그 주변을 막고 있는 오브젝트를 걷어낸다.
		std::vector<WorldObject> kept;
		kept.reserve(m_Objects.size());

		for (size_t k = 0; k < m_Objects.size(); ++k)
		{
			const WorldObject& o = m_Objects[k];
			bool drop = false;

			if (o.blocking)
			{
				int x0 = (int)floorf(o.x - o.halfW - 1.0f);
				int x1 = (int)floorf(o.x + o.halfW + 1.0f);
				int y0 = (int)floorf(o.y - o.halfH - 1.0f);
				int y1 = (int)floorf(o.y + o.halfH + 1.0f);

				for (int y = y0; y <= y1 && !drop; ++y)
				for (int x = x0; x <= x1 && !drop; ++x)
				{
					if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
						continue;

					if (orphan[(size_t)y * MAP_W + x])
						drop = true;
				}
			}

			if (!drop)
				kept.push_back(o);
		}

		if (kept.size() == m_Objects.size())
		{
			// 지울 오브젝트가 없는데도 닿지 않는다면 그 구역 자체를 없앤다.
			for (int y = 0; y < MAP_H; ++y)
			for (int x = 0; x < MAP_W; ++x)
			{
				size_t i = (size_t)y * MAP_W + x;

				if (orphan[i])
				{
					m_Tiles[y][x] = TILE_WALL;
					tileOpen[i]   = 0;
				}
			}
		}
		else
		{
			m_Objects.swap(kept);
		}
	}
}

void World::GenerateRuin(unsigned int seed)
{
	m_Kind = LEVEL_RUIN;
	m_Rng  = (seed != 0u) ? seed : 1u;

	m_Objects.clear();
	m_Rooms.clear();

	for (int i = 0; i < GRID_W * GRID_H; ++i)
		m_Grid[i].clear();

	// ── 1) 전부 벽으로 채운다 ──
	for (int y = 0; y < MAP_H; ++y)
	for (int x = 0; x < MAP_W; ++x)
	{
		m_Tiles[y][x]     = TILE_WALL;
		m_Variation[y][x] = Hash01(x, y);
		m_NearPath[y][x]  = false;
	}

	// ── 2) 겹치지 않게 방을 파낸다 ──
	const int ROOM_TARGET = 15;

	for (int attempt = 0; attempt < 900 && (int)m_Rooms.size() < ROOM_TARGET; ++attempt)
	{
		int w = 8 + (int)(Rand01() * 11.0f);
		int h = 8 + (int)(Rand01() * 11.0f);
		int x = 3 + (int)(Rand01() * (float)(MAP_W - w - 7));
		int y = 3 + (int)(Rand01() * (float)(MAP_H - h - 7));

		Room r;
		r.x0 = x;
		r.y0 = y;
		r.x1 = x + w;
		r.y1 = y + h;

		bool clash = false;

		for (size_t i = 0; i < m_Rooms.size(); ++i)
		{
			const Room& o = m_Rooms[i];

			if (r.x0 <= o.x1 + 3 && r.x1 + 3 >= o.x0 &&
			    r.y0 <= o.y1 + 3 && r.y1 + 3 >= o.y0)
			{
				clash = true;
				break;
			}
		}

		if (clash)
			continue;

		CarveRect(r.x0, r.y0, r.x1, r.y1, TILE_FLOOR);
		m_Rooms.push_back(r);
	}

	// ── 3) 방을 차례로 잇는다 ──
	// 연결성은 여기서 구조적으로 보장된다. 뒤의 검증은 오브젝트가
	// 길을 막는 경우를 잡기 위한 안전망이다.
	for (size_t i = 1; i < m_Rooms.size(); ++i)
	{
		CarveCorridor((int)m_Rooms[i - 1].CenterX(), (int)m_Rooms[i - 1].CenterY(),
		              (int)m_Rooms[i].CenterX(),     (int)m_Rooms[i].CenterY(), 1);
	}

	// ── 4) 여분 통로 — 막다른 길만 있으면 답답하다 ──
	if (m_Rooms.size() >= 3)
	{
		for (int k = 0; k < 4; ++k)
		{
			int a = (int)(Rand01() * (float)m_Rooms.size());
			int b = (int)(Rand01() * (float)m_Rooms.size());

			if (a >= (int)m_Rooms.size()) a = 0;
			if (b >= (int)m_Rooms.size()) b = 0;
			if (a == b)                   continue;

			CarveCorridor((int)m_Rooms[a].CenterX(), (int)m_Rooms[a].CenterY(),
			              (int)m_Rooms[b].CenterX(), (int)m_Rooms[b].CenterY(), 1);
		}
	}

	// ── 5) 시작점 — 첫 방 한가운데 ──
	if (!m_Rooms.empty())
	{
		m_SpawnX = m_Rooms[0].CenterX();
		m_SpawnY = m_Rooms[0].CenterY();
	}
	else
	{
		m_SpawnX = (float)(MAP_W / 2);
		m_SpawnY = (float)(MAP_H / 2);
	}

	// ── 6) 물웅덩이 — 방 구석에만 둔다. 가운데를 막으면 길이 끊긴다. ──
	for (size_t i = 2; i < m_Rooms.size(); ++i)
	{
		if (Rand01() > 0.40f)
			continue;

		const Room& rm = m_Rooms[i];

		int cx = rm.x0 + 3;
		int cy = rm.y0 + 3;
		int rr = 2;

		for (int y = cy - rr; y <= cy + rr; ++y)
		for (int x = cx - rr; x <= cx + rr; ++x)
		{
			if (x <= rm.x0 || y <= rm.y0 || x >= rm.x1 || y >= rm.y1)
				continue;

			if (m_NearPath[y][x])
				continue;

			int ddx = x - cx;
			int ddy = y - cy;
			int d2  = ddx * ddx + ddy * ddy;

			if (d2 <= rr * rr - 2)       m_Tiles[y][x] = TILE_WATER;
			else if (d2 <= rr * rr + 1)  m_Tiles[y][x] = TILE_SHALLOW;
		}
	}

	// ── 7) 배치와 검증 ──
	PlaceRuinObjects();
	RepairConnectivity();

	// 벽 껍데기는 지형이 확정된 뒤에 세운다. 통행은 타일이 막으므로
	// 이 오브젝트들은 그리기 전용이고 연결성에 영향을 주지 않는다.
	BuildWallShell();
	BuildGrid();
}
