#include "stdafx.h"
#include "World.h"

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

void World::Generate()
{
	m_Rng = 0x5EED1234u;
	m_Objects.clear();

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

	// 비석이 설 자리는 마른 모래로 만들어 둔다.
	ForceSand(SIGIL_X, SIGIL_Y, 3.4f);

	// ── 4) 배치 ─────────────────────────────────────────────────────
	PlaceVillage();
	PlaceFarm();
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
	if (x < radius || y < radius || x > MAP_W - radius || y > MAP_H - radius)
		return true;

	// 타일 — 깊은 물만 막는다. 얕은 물은 걸어 들어갈 수 있다.
	int x0 = (int)floorf(x - radius), x1 = (int)floorf(x + radius);
	int y0 = (int)floorf(y - radius), y1 = (int)floorf(y + radius);

	for (int ty = y0; ty <= y1; ++ty)
	for (int tx = x0; tx <= x1; ++tx)
	{
		if (Tile(tx, ty) == TILE_WATER)
			return true;
	}

	// 오브젝트 — 자기가 속한 격자 셀만 본다.
	int cx = ClampInt((int)floorf(x / (float)GRID_CELL), 0, GRID_W - 1);
	int cy = ClampInt((int)floorf(y / (float)GRID_CELL), 0, GRID_H - 1);

	const std::vector<int>& cell = m_Grid[cy * GRID_W + cx];

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

	if (t != TILE_GRASS && t != TILE_MEADOW)
		return false;

	return !Blocked(x, y, 0.5f);
}
