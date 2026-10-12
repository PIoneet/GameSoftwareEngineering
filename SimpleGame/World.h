#pragma once

#include <vector>

enum TileType
{
	TILE_GRASS,
	TILE_MEADOW,
	TILE_PATH,
	TILE_SAND,
	TILE_SHALLOW,
	TILE_WATER,
	TILE_FLOOR,     // 폐허 바닥
	TILE_WALL       // 폐허 벽 — 통행 불가
};

enum LevelKind
{
	LEVEL_VILLAGE,   // 튜토리얼 마을
	LEVEL_RUIN       // 1레벨 — 랜덤 생성 폐허
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
	OBJ_CRATE,

	// 폐허
	OBJ_WALL,
	OBJ_PILLAR,
	OBJ_RUBBLE
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

// 맵을 다시 4배(면적)로 키웠다. 112x112 -> 224x224.
//
// 타일 순회는 화면에 보이는 범위만 돌기 때문에(Draw::Tiles) 맵이 커져도
// 프레임 비용은 늘지 않는다. 늘어난 공간에는 외곽 거점을 둔다.
const int MAP_W = 224;
const int MAP_H = 224;

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

// ── 외곽 거점 ──
// 마을 바깥의 세력별 근거지. Game 이 여기에 맞는 원형의 NPC 를 세운다.
const float CAMP_BANDIT_X  = 150.0f;   // 까마귀발 산적단 야영지
const float CAMP_BANDIT_Y  = 148.0f;
const float SHRINE_CHOIR_X =  58.0f;   // 성가대 은신처 — 숲 속 폐사당
const float SHRINE_CHOIR_Y = 150.0f;
const float CAMP_SILENCE_X = 152.0f;   // 침묵의 수도회 순찰 야영지
const float CAMP_SILENCE_Y =  54.0f;
const float FARM_OUT_X     =  34.0f;   // 외딴 농가
const float FARM_OUT_Y     = 128.0f;
const float MARSH_X        = 186.0f;   // 늪 초입 — 약초꾼의 오두막
const float MARSH_Y        = 184.0f;

// 한 방의 범위 (양 끝 포함)
struct Room
{
	int x0, y0, x1, y1;

	float CenterX() const { return (float)(x0 + x1) * 0.5f + 0.5f; }
	float CenterY() const { return (float)(y0 + y1) * 0.5f + 0.5f; }
};

class World
{
public:
	void Generate();                          // 마을 (튜토리얼)
	void GenerateRuin(unsigned int seed);     // 1레벨 — 랜덤 폐허

	LevelKind Kind()   const { return m_Kind; }
	float     SpawnX() const { return m_SpawnX; }
	float     SpawnY() const { return m_SpawnY; }

	const std::vector<Room>& Rooms() const { return m_Rooms; }

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
	void PlaceOutposts();

	// 거점 주변을 비운다 — 숲이 야영지 위에 자라지 않게.
	void ClearArea(float cx, float cy, float radius);
	void PlaceForest();
	void PlaceShore();
	void BuildGrid();

	// ── 폐허 생성 ──
	void CarveRect(int x0, int y0, int x1, int y1, TileType t);
	void CarveCorridor(int ax, int ay, int bx, int by, int half);
	void PlaceRuinObjects();
	void BuildWallShell();

	// 걸을 수 있는 칸이 모두 시작점에서 닿는지 확인하고, 아니면 고친다.
	// 막힌 오브젝트를 먼저 걷어내고, 그래도 닿지 않는 구역은 벽으로 메운다.
	void RepairConnectivity();

	void Add(ObjectType type, float x, float y,
	         float halfW, float halfH, float width, float height, bool blocking);

	float Rand01();

	TileType m_Tiles[MAP_H][MAP_W];
	float    m_Variation[MAP_H][MAP_W];
	bool     m_NearPath[MAP_H][MAP_W];

	std::vector<WorldObject> m_Objects;
	std::vector<int>         m_Grid[GRID_W * GRID_H];
	std::vector<Room>        m_Rooms;

	LevelKind m_Kind   = LEVEL_VILLAGE;
	float     m_SpawnX = SPAWN_X;
	float     m_SpawnY = SPAWN_Y;

	unsigned int m_Rng = 0x5EED1234u;
};
