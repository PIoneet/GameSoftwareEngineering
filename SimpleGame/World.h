#pragma once

#include <vector>

enum TileType
{
	TILE_GRASS,
	TILE_MEADOW,
	TILE_PATH,
	TILE_SAND,
	TILE_SHALLOW,
	TILE_WATER
};

enum ObjectType
{
	OBJ_TREE,
	OBJ_PINE,
	OBJ_BUSH,
	OBJ_STUMP,
	OBJ_ROCK,
	OBJ_HOUSE,
	OBJ_INN,
	OBJ_CHAPEL,
	OBJ_FENCE,
	OBJ_WELL,
	OBJ_BOARD,
	OBJ_CAMPFIRE,
	OBJ_LANTERN,
	OBJ_SIGIL,
	OBJ_REED,
	OBJ_CRATE
};

struct WorldObject
{
	ObjectType type;
	float x, y;            // 월드(타일) 좌표 — 밑동 중심
	float halfW, halfH;    // 충돌 반경 (타일 단위)
	float width, height;   // 화면 크기 (픽셀)
	bool  blocking;
	int   variant;         // 색·형태 변주 시드
};

// 맵을 4배(면적)로 키웠다. 56x56 -> 112x112.
const int MAP_W = 112;
const int MAP_H = 112;

// 충돌 조회용 공간 격자. 오브젝트가 수천 개가 되면 선형 탐색으로는 못 버틴다.
const int GRID_CELL = 4;
const int GRID_W    = MAP_W / GRID_CELL;
const int GRID_H    = MAP_H / GRID_CELL;

// 주요 지점 — 레벨 디자인 상수. Game 에서도 참조한다.
const float VILLAGE_CX = 40.0f;   // 마을 광장
const float VILLAGE_CY = 40.0f;
const float SPAWN_X    = 40.0f;   // 북문 — 플레이어는 숲에서 마을로 걸어 들어온다
const float SPAWN_Y    = 24.0f;
const float SHORE_X    = 62.0f;   // 호숫가 — 길이 끝나는 곳
const float SHORE_Y    = 66.0f;
const float SIGIL_X    = 62.8f;   // 비석
const float SIGIL_Y    = 66.8f;

class World
{
public:
	void Generate();

	TileType Tile(int x, int y) const;
	float    Variation(int x, int y) const;   // 0..1, 타일 밝기·디테일 변주

	// 반지름 radius 인 원이 (x, y) 에 놓일 수 있는지.
	bool Blocked(float x, float y, float radius) const;

	// 걸어다닐 수 있는 빈 땅인지 (짐승 스폰용)
	bool IsOpenGround(float x, float y) const;

	const std::vector<WorldObject>& Objects() const { return m_Objects; }

private:
	void CarvePath(float ax, float ay, float bx, float by, float radius);
	void ForceSand(float cx, float cy, float radius);
	void PlaceVillage();
	void PlaceFarm();
	void PlaceForest();
	void PlaceShore();
	void BuildGrid();

	void Add(ObjectType type, float x, float y,
	         float halfW, float halfH, float width, float height, bool blocking);

	float Rand01();

	TileType m_Tiles[MAP_H][MAP_W];
	float    m_Variation[MAP_H][MAP_W];
	bool     m_NearPath[MAP_H][MAP_W];

	std::vector<WorldObject> m_Objects;
	std::vector<int>         m_Grid[GRID_W * GRID_H];

	unsigned int m_Rng = 0x5EED1234u;
};
