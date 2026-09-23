#pragma once

// 성능 프로파일러.
//
// 목적이 둘이다.
//   1) 지금 시간이 어디로 가는지
//   2) 앞으로 어디가 터질지 — 전수 순회처럼 규모에 비례해 커지는 비용을 미리 드러낸다
//
// 출력은 사람이 눈으로 훑기보다 "그대로 붙여넣어 분석시키기" 좋게 key=value 로 낸다.
// 단위는 이름에 박아 두고(_ms, _ratio), 원시 수치와 함께 파생 지표까지 계산해 찍는다.
// 원시 숫자만 있으면 결론까지 가는 데 한 단계가 더 필요하기 때문이다.

enum ProfZone
{
	PROF_FRAME,

	PROF_UPDATE,
	PROF_UPD_SCENE,
	PROF_UPD_MOTES,
	PROF_UPD_PICKUP,

	PROF_RENDER,
	PROF_RND_TILES,
	PROF_RND_GLOW,
	PROF_RND_COLLECT,
	PROF_RND_SORT,
	PROF_RND_ACTORS,
	PROF_RND_MOTES,
	PROF_RND_POST,
	PROF_RND_UI,

	PROF_ZONE_COUNT
};

enum ProfCounter
{
	PROF_DC_SHAPE,
	PROF_DC_TEXT,
	PROF_DC_POST,

	PROF_ACTOR_TOTAL,
	PROF_ACTOR_VISITED,     // 컬링 순회가 들른 노드 수
	PROF_ACTOR_VISIBLE,
	PROF_ACTOR_UPDATED,
	PROF_ACTOR_REAPED,

	PROF_TILE_SCANNED,      // 순회한 타일
	PROF_TILE_DRAWN,        // 실제로 그린 타일

	PROF_COLLIDE_CALLS,     // World::Blocked 호출 수
	PROF_COLLIDE_CANDIDATES,// 그 안에서 검사한 오브젝트 수

	PROF_QUERY_CALLS,       // Scene 범위 질의 수
	PROF_QUERY_SCANNED,     // 그 안에서 훑은 액터 수

	PROF_GLYPH_BAKED,       // 새로 구운 글리프 (0 이 정상, 튀면 새 문자열)

	PROF_COUNTER_COUNT
};

namespace Prof
{
	void Init();       // GL 컨텍스트가 만들어진 뒤에 부른다 (GPU 타이머 질의)
	void Shutdown();

	void SetEnabled(bool on);
	bool Enabled();

	// 어느 레벨을 재고 있는지. 로그만 보고도 맥락이 서게 한다.
	void SetLabel(const char* label);

	void BeginFrame();
	void EndFrame(float dt);

	void ZoneBegin(int zone);
	void ZoneEnd(int zone);

	void Add(int counter, int n = 1);
	void Set(int counter, int v);

	// OpenGL 은 비동기다. CPU 타이머는 "제출에 걸린 시간"만 재고
	// GPU 가 실제로 그린 시간은 못 잰다. 그건 타이머 질의로 따로 받는다.
	void GpuFrameBegin();
	void GpuFrameEnd();
}

// RAII 구역 타이머.
//
// 이름을 PROFILE_ 로 시작하면 안 된다 — WinBase.h / wingdi.h / Icm.h 가
// PROFILE_USER, PROFILE_LINKED 등으로 이미 선점했다.
struct ProfScope
{
	int zone;

	explicit ProfScope(int z) : zone(z) { Prof::ZoneBegin(z); }
	~ProfScope()                        { Prof::ZoneEnd(zone); }
};

#define PROF_CAT2(a, b) a##b
#define PROF_CAT(a, b)  PROF_CAT2(a, b)
#define PROF_SCOPE(z)   ProfScope PROF_CAT(s_profScope_, __LINE__)(z)
