#include "stdafx.h"
#include "Game.h"
#include "Renderer.h"
#include "Draw.h"
#include "Input.h"
#include "IsoMath.h"
#include "Profiler.h"

#include <cmath>
#include <cstdio>
#include <cwchar>

// ─────────────────────────────────────────────────────────────────────────
//  상수
// ─────────────────────────────────────────────────────────────────────────

static const float TALK_RANGE   = 2.0f;
static const float PICKUP_RANGE = 0.9f;

static const int MOTE_COUNT = 96;
static const int NPC_COUNT  = 14;
static const int BEAST_COUNT = 16;


// ─────────────────────────────────────────────────────────────────────────
//  대사
// ─────────────────────────────────────────────────────────────────────────

struct DialogueSet
{
	const wchar_t* speaker;
	const wchar_t* lines[6];
	int            count;
};

static const DialogueSet DIALOGUE[] =
{
	// 0 — 미렌 : 의뢰 제시
	{ L"미렌 · 길드 서기", {
		L"라벤홀 모험가 길드입니다. 게시판 비었으니까 기대는 접으세요.",
		L"…아, 하나 있긴 하네요. 이번 주에 세 사람이 호수 얘기를 하러 왔거든요.",
		L"규정대로면 '취중 진술'로 분류하고 끝냅니다. 그런데 그 중 둘은 안 취했어요.",
		L"호드, 펠, 타마. 셋한테 뭘 봤는지 듣고, 직접 호숫가까지 내려가 보세요.",
		L"보수 40쿠퍼. 서류는 세 장이고요. 죽지 마세요, 죽으면 서류가 여섯 장 됩니다.",
	}, 5 },

	// 1 — 미렌 : 진행 중
	{ L"미렌 · 길드 서기", {
		L"호드, 펠, 타마. 이름 세 개예요. 제가 적어드리기까지 했잖아요.",
		L"호드는 밭에, 펠은 자기 집 앞에, 타마는 여관 쪽에 있을 겁니다.",
	}, 2 },

	// 2 — 미렌 : 증언 수집 완료
	{ L"미렌 · 길드 서기", {
		L"세 사람, 한 호수. 둘까지는 우연이라고 우겨볼 수 있는데 셋은 아니죠.",
		L"남동쪽 길 따라 호숫가로 내려가세요. 거기 있으면 안 될 것 같은 걸 찾으시면 됩니다.",
		L"길드를 대표해 미리 감사드립니다. 이 인사는 예산이 안 드니까요.",
	}, 3 },

	// 3 — 미렌 : 의뢰 완료
	{ L"미렌 · 길드 서기", {
		L"돌아오셨네요. 손에 뭘 쥐고 계시고. 이리 줘보세요.",
		L"…축제날 그 이방인이 흘리고 간 동전이랑 같은 문양인데요. 분실물로 처리해뒀던 거.",
		L"동전 두 개, 같은 소용돌이. 그리고 점점 조용해지는 호수 하나.",
		L"약속대로 40쿠퍼요. 길드의 감사도 같이 드리는데, 그건 한 4쿠퍼쯤 합니다.",
		L"부탁 하나만. 거기 혼자서는 다시 내려가지 마세요.",
	}, 5 },

	// 4 — 호드 (증인 0)
	{ L"호드 · 농부", {
		L"호수? 그래, 물이 이상해.",
		L"축제 지나고 손바닥 하나쯤 줄었어. 볕도 시원찮았고, 빠져나가는 개울도 없는데.",
		L"물이 그냥 없어지진 않아, 젊은이. 뭔가가 마시고 있는 거지.",
	}, 3 },

	// 5 — 펠 (증인 1)
	{ L"펠 · 어부", {
		L"난 다시 안 내려가. 두 번 묻지 마.",
		L"나흘 내리 그물이 비어서 올라왔어. 닷새째엔 깨끗하게 올라오더군.",
		L"깨끗하게. 새것처럼. 밑에 있는 뭔가가 대신 빨아준 것처럼 말이야.",
		L"호수는 자네가 가져. 난 땅이 말이 되는 데서 지낼란다.",
	}, 4 },

	// 6 — 타마 (증인 2)
	{ L"타마 · 여관집 딸", {
		L"미렌이 보낸 사람이구나. 노랫소리 때문에 온 거지.",
		L"정확히는 노래가 아니야. 다들 모르는 채로 같은 음을 흥얼거리는 쪽에 가까워.",
		L"호숫가에서 들려. 귀로 듣는 게 아니고.",
		L"아빠는 바람 소리래. 아빠는 지붕도 안 샌다고 하거든.",
	}, 4 },

	// 7 — 고렌
	{ L"고렌 · 대장장이", {
		L"검 사려고? 다들 검을 원하지. 근데 검값을 내려는 사람은 없어.",
		L"돈 들고 오면 그때 친한 척 해줄게.",
	}, 2 },

	// 8 — 닙
	{ L"닙", {
		L"나 호수 그림 그렸다.",
		L"큰 소용돌이가 있어. 그거 그리려던 거 아닌데. 그냥 거기 있었어.",
		L"엄마가 불에 넣었어. 그래서 또 그렸어.",
	}, 3 },

	// 9 — 보스크
	{ L"보스크", {
		L"미렌이 돈은 줬냐? 아직? 그럼 넌 아직 일하는 멍청이일 뿐이야.",
		L"충고 하나 하지. 호숫가에서 돌 세지 마라.",
		L"왜냐고? 셀 때마다 개수가 다르거든. 내가 어떻게 아는지는 묻지 말고.",
	}, 3 },

	// 10 — 비석 : 아직 이르다
	{ L"호숫가", {
		L"잿빛 돌이 진흙에 반쯤 박혀 있다. 침전물 아래에 뭔가 새겨져 있다.",
		L"뭘 찾는 건지부터 알아보는 게 낫겠다.",
	}, 2 },

	// 11 — 비석 : 발견
	{ L"호숫가", {
		L"침전물이 이상할 만큼 쉽게 닦여 나간다.",
		L"소용돌이. 깊게 파여 있고, 홈이 젖어 있다 — 수면 위쪽 돌은 바싹 말라 있는데도.",
		L"한가운데에 구리 동전이 박혀 있다. 봉헌물처럼. 혹은 열쇠처럼.",
		L"(동전을 집어 든다. 호수가 아주 조용하다.)",
	}, 4 },

	// 12 — 미렌 : 완료 후
	{ L"미렌 · 길드 서기", {
		L"이제 가서 쉬어요. 난 세 장을 써야 하는데 제목부터 막막하네요.",
	}, 1 },

	// 13 — 비석 : 이미 가져감
	{ L"호숫가", {
		L"돌은 이제 비어 있다.",
		L"소용돌이는 여전히 젖어 있다.",
	}, 2 },

	// 14 — 에드라
	{ L"에드라 · 사제", {
		L"등불은 켜 두고 있습니다만, 요즘은 누굴 위한 건지 모르겠군요.",
		L"기도문 중에 호수를 언급하는 구절이 하나 있습니다. 아주 오래된 판본에만요.",
		L"'그것이 잠든 동안은 이름을 부르지 말라.' 무슨 뜻인지는 아무도 설명 못 합니다.",
	}, 3 },

	// 15 — 마르타
	{ L"마르타 · 여관 주인", {
		L"방은 있어요. 손님이 없어서.",
		L"요 며칠 짐승들이 자꾸 마을 쪽으로 붙어 와요. 사슴이 울타리까지 온다니까요.",
		L"호수 쪽으로는 아예 안 가더라고요. 짐승이 안 가는 데는 사람도 안 가는 게 맞는데.",
	}, 3 },

	// 16 — 콜름
	{ L"콜름 · 경비병", {
		L"북문 지키는 중입니다. 지킬 게 있어서라기보단, 서 있으라니까 서 있는 거죠.",
		L"밤에 늑대 우는 소리가 들리는데, 요즘 방향이 이상해요.",
		L"숲에서 우는 게 아니라, 물 쪽에서 멀어지면서 우는 것 같달까.",
	}, 3 },

	// 17 — 위나
	{ L"위나 · 약초상", {
		L"호숫가 갈대는 이제 안 뜯어요. 말려도 냄새가 안 빠져서.",
		L"비린내는 아니에요. 뭐랄까… 오래된 동전 냄새.",
	}, 2 },

	// 18 — 도른
	{ L"도른 · 목수", {
		L"펠네 배를 고쳐달라기에 갔더니, 고칠 데가 없더군요.",
		L"물에 삼 년 담근 배 밑바닥이 새것 같았어요. 그건 좋은 일이 아닙니다.",
	}, 2 },

	// 19 — 셀비
	{ L"셀비", {
		L"닙이랑 호수까지 누가 먼저 가나 내기했는데.",
		L"둘 다 중간에 돌아왔어. 왜 돌아왔는지는 둘 다 기억이 안 나.",
	}, 2 },

	// 20 — 하르윈
	{ L"하르윈 · 노파", {
		L"예순 해를 여기서 살았는데, 호수가 이렇게 조용해진 건 두 번째야.",
		L"첫 번째? 그땐 다들 잊기로 했지. 그게 제일 편했거든.",
	}, 2 },
};


// ─────────────────────────────────────────────────────────────────────────
//  유틸
// ─────────────────────────────────────────────────────────────────────────

static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

static float Dist(float ax, float ay, float bx, float by)
{
	float dx = bx - ax;
	float dy = by - ay;

	return sqrtf(dx * dx + dy * dy);
}

static const int WRAP_LINES = 6;
static const int WRAP_CHARS = 256;

static wchar_t s_Wrap[WRAP_LINES][WRAP_CHARS];

// 폭 기준 줄바꿈. 한글은 글자 단위로 끊어도 되지만, 공백이 있으면 거기서 끊는다.
static int WrapByWidth(Renderer& r, const wchar_t* text, float maxW, TextFont f)
{
	int line = 0;

	const wchar_t* p = text;

	while (*p != L'\0' && line < WRAP_LINES)
	{
		const wchar_t* start     = p;
		const wchar_t* lastSpace = 0;
		const wchar_t* q         = p;

		float w = 0.0f;

		while (*q != L'\0')
		{
			wchar_t tmp[2] = { *q, L'\0' };
			float   cw     = r.TextWidth(tmp, f);

			if (w + cw > maxW && q > start)
				break;

			w += cw;

			if (*q == L' ')
				lastSpace = q;

			++q;
		}

		const wchar_t* end = q;

		if (*q != L'\0' && lastSpace != 0 && lastSpace > start)
			end = lastSpace;

		int n = 0;

		for (const wchar_t* c = start; c < end && n < WRAP_CHARS - 1; ++c)
			s_Wrap[line][n++] = *c;

		s_Wrap[line][n] = L'\0';

		++line;
		p = end;

		while (*p == L' ')
			++p;
	}

	return line > 0 ? line : 1;
}


// ─────────────────────────────────────────────────────────────────────────
//  초기화 / 씬 구성
// ─────────────────────────────────────────────────────────────────────────

bool Game::FindFreeSpot(float& x, float& y, float radius) const
{
	if (!m_World.Blocked(x, y, radius))
		return true;

	// 나선형으로 조금씩 넓혀가며 빈자리를 찾는다.
	for (float ring = 0.5f; ring <= 4.0f; ring += 0.5f)
	{
		for (int i = 0; i < 12; ++i)
		{
			float a  = (float)i * 0.5236f;
			float nx = x + cosf(a) * ring;
			float ny = y + sinf(a) * ring;

			if (!m_World.Blocked(nx, ny, radius))
			{
				x = nx;
				y = ny;

				return true;
			}
		}
	}

	return false;
}

void Game::SpawnPropsFromWorld()
{
	m_Sigil = 0;

	const std::vector<WorldObject>& objs = m_World.Objects();

	for (size_t i = 0; i < objs.size(); ++i)
	{
		PropActor* p = new PropActor(objs[i]);
		m_Scene.Add(p);

		// 비석은 진행 상태에 따라 연출이 바뀌므로 따로 붙들어 둔다.
		if (objs[i].type == OBJ_SIGIL)
			m_Sigil = p;
	}
}

void Game::SpawnVillagers()
{
	struct NpcDef
	{
		const wchar_t* name;
		float x, y;
		int   set;
		int   witness;
		int   look;
		float wander;
	};

	static const NpcDef defs[NPC_COUNT] =
	{
		{ L"미렌",   36.8f, 44.0f,  0, -1,  0, 0.7f },   // 게시판 — 의뢰인
		{ L"호드",   30.0f, 57.0f,  4,  0,  1, 1.8f },   // 밭
		{ L"펠",     49.2f, 48.0f,  5,  1,  2, 1.0f },   // 자기 집 앞
		{ L"타마",   35.5f, 37.4f,  6,  2,  3, 1.5f },   // 여관 앞
		{ L"고렌",   46.0f, 35.6f,  7, -1,  4, 0.9f },   // 대장간
		{ L"닙",     41.5f, 45.6f,  8, -1,  5, 2.6f },   // 광장
		{ L"보스크", 42.6f, 41.0f,  9, -1,  6, 1.6f },   // 모닥불 옆
		{ L"에드라", 36.5f, 30.2f, 14, -1,  7, 1.0f },   // 예배당
		{ L"마르타", 31.0f, 36.8f, 15, -1,  8, 1.1f },   // 여관
		{ L"콜름",   40.0f, 28.5f, 16, -1,  9, 1.3f },   // 북문
		{ L"위나",   30.2f, 42.2f, 17, -1, 10, 1.4f },   // 서쪽 골목
		{ L"도른",   50.0f, 42.2f, 18, -1, 11, 1.4f },   // 동쪽 골목
		{ L"셀비",   43.2f, 46.6f, 19, -1, 12, 2.6f },   // 광장
		{ L"하르윈", 41.6f, 52.6f, 20, -1, 13, 0.8f },   // 남쪽
	};

	for (int i = 0; i < NPC_COUNT; ++i)
	{
		// 배치 좌표가 건물에 물리면 주변 빈자리로 밀어낸다.
		float px = defs[i].x;
		float py = defs[i].y;
		FindFreeSpot(px, py, BODY_RADIUS);

		NpcActor* n = new NpcActor(defs[i].name, defs[i].set,
		                           defs[i].witness, defs[i].look, defs[i].wander);

		n->SetHome(px, py);
		n->SetSeed((float)i * 1.7f);

		m_Scene.Add(n);
	}
}

void Game::SpawnWildlife()
{
	// 마을에서 떨어진 빈 땅에 흩어 놓는다. 호수 근처에는 두지 않는다 —
	// 짐승이 물가를 피한다는 것 자체가 마을 사람들의 증언과 맞물린다.
	unsigned int seed  = 0xC0FFEEu;
	int          made  = 0;
	int          guard = 0;

	while (made < BEAST_COUNT && guard < 4000)
	{
		++guard;

		seed = seed * 1664525u + 1013904223u;
		float a = (float)((seed >> 8) % 6283) * 0.001f;

		seed = seed * 1664525u + 1013904223u;
		float d = 20.0f + (float)((seed >> 8) % 2600) * 0.01f;

		float bx = VILLAGE_CX + cosf(a) * d;
		float by = VILLAGE_CY + sinf(a) * d;

		if (bx < 4.0f || by < 4.0f || bx > MAP_W - 4.0f || by > MAP_H - 4.0f)
			continue;

		if (Dist(bx, by, SIGIL_X, SIGIL_Y) < 22.0f)
			continue;

		if (!m_World.IsOpenGround(bx, by))
			continue;

		int kind = (made % 4 == 0) ? BEAST_CROW
		         : (made % 4 == 2) ? BEAST_WOLF
		                           : BEAST_DEER;

		BeastActor* b = new BeastActor(kind);

		b->SetHome(bx, by);
		b->SetSeed((float)made * 2.3f);

		m_Scene.Add(b);

		++made;
	}
}

void Game::BuildLights()
{
	// 매 프레임 오브젝트를 훑지 않도록 미리 뽑아 둔다.
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
}

void Game::BuildVillageScene()
{
	m_Level = LEVEL_VILLAGE;
	Prof::SetLabel("village");

	m_World.Generate();

	// Scene::Clear 가 기존 액터를 전부 지우므로, 붙들고 있던 포인터도 함께 버린다.
	m_Scene.Clear();
	m_Player = 0;
	m_Sigil  = 0;

	m_FocusNpc   = 0;
	m_FocusSigil = false;
	m_TalkingNpc = 0;

	SpawnPropsFromWorld();
	SpawnVillagers();
	SpawnWildlife();
	BuildLights();

	m_Player = new PlayerActor();
	m_Player->x = m_World.SpawnX();
	m_Player->y = m_World.SpawnY();
	m_Player->combatEnabled = false;
	m_Scene.Add(m_Player);

	m_CamReady = false;
}

void Game::Init(Renderer* renderer, int windowW, int windowH)
{
	m_R    = renderer;
	m_WinW = windowW;
	m_WinH = windowH;

	// 모델은 첫 실행에서만 절차적으로 굽고, 이후에는 캐시 파일에서 읽는다.
	m_Models.LoadOrBuild("models.cache");

	m_Stats.Reset();

	BuildVillageScene();

	// ── 분위기 입자 ──
	m_Motes.resize(MOTE_COUNT);

	for (int i = 0; i < MOTE_COUNT; ++i)
	{
		Mote& m = m_Motes[i];

		m.life    = 0.0f;      // 첫 갱신에서 플레이어 주변에 재배치된다
		m.maxLife = 1.0f;

		m.x = m.y = m.z = 0.0f;
		m.vx = m.vy = m.vz = 0.0f;

		m.kind = 0;
	}

	m_Query.reserve(64);
}

void Game::Resize(int windowW, int windowH)
{
	m_WinW = windowW;
	m_WinH = windowH;
}


// ─────────────────────────────────────────────────────────────────────────
//  컨텍스트
// ─────────────────────────────────────────────────────────────────────────

UpdateContext Game::MakeUpdateContext()
{
	UpdateContext ctx;

	ctx.world  = &m_World;
	ctx.scene  = &m_Scene;
	ctx.stats  = &m_Stats;
	ctx.player = m_Player;

	ctx.time    = m_Time;
	ctx.playerX = (m_Player != 0) ? m_Player->WorldX() : 0.0f;
	ctx.playerY = (m_Player != 0) ? m_Player->WorldY() : 0.0f;

	ctx.paused = m_DialogueOpen || m_StatsOpen;

	return ctx;
}

RenderContext Game::MakeRenderContext(float ox, float oy)
{
	RenderContext ctx;

	ctx.r      = m_R;
	ctx.models = &m_Models;

	ctx.originX = ox;
	ctx.originY = oy;
	ctx.time    = m_Time;

	ctx.viewX = (m_Player != 0) ? m_Player->WorldX() : 0.0f;
	ctx.viewY = (m_Player != 0) ? m_Player->WorldY() : 0.0f;

	ctx.winW = m_WinW;
	ctx.winH = m_WinH;

	return ctx;
}


// ─────────────────────────────────────────────────────────────────────────
//  갱신
// ─────────────────────────────────────────────────────────────────────────

void Game::Update(float dt)
{
	PROF_SCOPE(PROF_UPDATE);

	m_Time += dt;

	if (m_TitleTimer   > 0.0f) m_TitleTimer   -= dt;
	if (m_ToastTimer   > 0.0f) m_ToastTimer   -= dt;
	if (m_LevelUpTimer > 0.0f) m_LevelUpTimer -= dt;
	if (m_RuinIntro    > 0.0f) m_RuinIntro    -= dt;
	if (m_EndTimer    >= 0.0f) m_EndTimer     += dt;

	if (Input::Pressed('j'))
		m_JournalOpen = !m_JournalOpen;

	if (Input::Pressed('c'))
		m_StatsOpen = !m_StatsOpen;

	// 개발용 — 1레벨을 바로 오간다. 튜토리얼을 매번 지나지 않아도 되게.
	if (Input::Pressed('0'))
	{
		if (m_Level == LEVEL_VILLAGE)
			EnterRuin((unsigned int)(m_Time * 997.0f) + 12345u);
		else
			ReturnToVillage();
	}

	// ── 대화 / 상호작용 ──
	if (m_DialogueOpen)
	{
		if (Input::Pressed('e') || Input::Pressed(' '))
			AdvanceDialogue();
	}
	else
	{
		UpdateInteraction();

		if (Input::Pressed('e'))
		{
			if (m_FocusNpc != 0)
				StartDialogue(DialogueSetForNpc(m_FocusNpc), m_FocusNpc);
			else if (m_FocusSigil)
				StartDialogue(DialogueSetForSigil(), 0);
		}
	}

	// 마을에서는 공격이 잠겨 있다. 그런데 무반응은 "잠김" 과 "고장" 이 구분되지
	// 않는다 — 실제로 키가 안 먹는 줄 알았다는 보고가 있었다. 왜 안 되는지 말해 준다.
	if (m_Level == LEVEL_VILLAGE && !m_DialogueOpen && Input::Pressed(' '))
		Toast(L"여기서는 휘두를 일이 없다");

	if (m_Level == LEVEL_RUIN)
		UpdateRuinInput();

	// ── 씬 ──
	UpdateContext ctx = MakeUpdateContext();

	m_Scene.Update(dt, ctx);

	if (m_Level == LEVEL_RUIN)
	{
		PROF_SCOPE(PROF_UPD_PICKUP);
		CollectPickups();
	}

	m_Scene.Reap();

	{
		PROF_SCOPE(PROF_UPD_MOTES);
		UpdateMotes(dt);
	}

	if (m_Player != 0)
	{
		m_Killed = m_Player->kills;

		// 신호는 읽은 쪽이 내린다. 액터가 스스로 지우면 몬스터가 먼저 갱신된
		// 프레임에서 신호가 한 번도 읽히지 않고 사라진다.
		if (m_Player->respawned)
		{
			m_Player->respawned = false;

			m_CamReady = false;
			Toast(L"정신을 잃었다 — 입구에서 깨어난다");
		}
	}

	// ── 카메라 — 플레이어를 부드럽게 따라간다 ──
	float px = (m_Player != 0) ? m_Player->WorldX() : 0.0f;
	float py = (m_Player != 0) ? m_Player->WorldY() : 0.0f;

	float targetCamX = IsoScreenX(px, py);
	float targetCamY = IsoScreenY(px, py);

	if (!m_CamReady)
	{
		m_CamX = targetCamX;
		m_CamY = targetCamY;

		m_CamReady = true;
	}
	else
	{
		float k = Clamp01(dt * 7.0f);

		m_CamX += (targetCamX - m_CamX) * k;
		m_CamY += (targetCamY - m_CamY) * k;
	}

	// ── 화면의 무게 ──
	float target = 0.55f;   // 폐허는 항상 무겁다

	if (m_Level == LEVEL_VILLAGE)
	{
		// 마을에서는 비석에 가까울수록 무거워진다.
		float d = Dist(px, py, SIGIL_X, SIGIL_Y);
		target  = 1.0f - Clamp01((d - 5.0f) / 22.0f);

		// 동전을 쥔 뒤로는 완전히 걷히지 않는다.
		if (m_HasCoin && target < 0.22f)
			target = 0.22f;
	}

	m_Dread += (target - m_Dread) * Clamp01(dt * 1.8f);
}

void Game::UpdateInteraction()
{
	m_FocusNpc   = 0;
	m_FocusSigil = false;

	// 폐허에는 말 걸 상대도 비석도 없다.
	if (m_Level == LEVEL_RUIN || m_Player == 0)
		return;

	float px = m_Player->WorldX();
	float py = m_Player->WorldY();

	// 변수 이름을 near 로 두면 안 된다 — <windows.h> 가 near / far 를
	// 빈 매크로로 정의해 두어서 선언문이 통째로 깨진다. DrawText 와 같은 함정이다.
	Actor* nearest = m_Scene.FindNearest(px, py, TALK_RANGE, ACTOR_NPC);

	float bestD = TALK_RANGE;

	if (nearest != 0)
	{
		m_FocusNpc = (NpcActor*)nearest;
		bestD = Dist(px, py, nearest->WorldX(), nearest->WorldY());
	}

	// 비석이 더 가까우면 그쪽이 이긴다.
	if (Dist(px, py, SIGIL_X, SIGIL_Y) < bestD)
	{
		m_FocusNpc   = 0;
		m_FocusSigil = true;
	}
}

void Game::CollectPickups()
{
	if (m_Player == 0)
		return;

	m_Scene.QueryNear(m_Player->WorldX(), m_Player->WorldY(),
	                  PICKUP_RANGE, ACTOR_PICKUP, m_Query);

	for (size_t i = 0; i < m_Query.size(); ++i)
	{
		PickupActor* p = (PickupActor*)m_Query[i];

		p->alive = false;

		if (p->PickupKind() == 0)
		{
			if (m_Stats.GainXp(p->Value()))
			{
				m_LevelUpTimer = 2.6f;
				Toast(L"레벨이 올랐다 — C 키로 능력치 분배");
			}
		}
		else if (p->PickupKind() == 1)
		{
			m_Stats.coins += p->Value();
		}
		else
		{
			++m_Stats.potions;
			Toast(L"회복약을 주웠다");
		}
	}
}

void Game::UpdateMotes(float dt)
{
	float px = (m_Player != 0) ? m_Player->WorldX() : 0.0f;
	float py = (m_Player != 0) ? m_Player->WorldY() : 0.0f;

	for (int i = 0; i < (int)m_Motes.size(); ++i)
	{
		Mote& m = m_Motes[i];

		m.life -= dt;

		if (m.life <= 0.0f)
		{
			// 수명이 다하면 플레이어 주변에 다시 뿌린다.
			float a = (float)((i * 71 + (int)(m_Time * 97.0f)) % 628) * 0.01f;
			float r = 3.0f + (float)((i * 13 + (int)(m_Time * 31.0f)) % 150) * 0.1f;

			m.x = px + cosf(a) * r;
			m.y = py + sinf(a) * r;
			m.z = 0.2f + (float)((i * 29) % 100) * 0.022f;

			m.vx = (((float)((i * 7  + (int)m_Time) % 100) / 100.0f) - 0.5f) * 0.25f;
			m.vy = (((float)((i * 11 + (int)m_Time) % 100) / 100.0f) - 0.5f) * 0.25f;
			m.vz = 0.05f + (float)((i * 3) % 50) * 0.004f;

			m.maxLife = 3.0f + (float)((i * 19) % 40) * 0.1f;
			m.life    = m.maxLife;

			// 위치에 따라 성격이 달라진다.
			if (m_Level == LEVEL_RUIN)
				m.kind = 1;
			else if (Dist(m.x, m.y, SIGIL_X, SIGIL_Y) < 16.0f)
				m.kind = 1;                        // 호수 쪽 — 차가운 재
			else if (Dist(m.x, m.y, 41.0f, 43.0f) < 5.0f)
				m.kind = 2;                        // 모닥불 불티
			else
				m.kind = 0;                        // 반딧불

			// 불티는 빠르게 솟았다 금방 꺼진다.
			if (m.kind == 2)
			{
				m.vz      = 0.9f + (float)((i * 3) % 40) * 0.02f;
				m.maxLife = 1.4f;
				m.life    = 1.4f;
			}
		}

		m.x += m.vx * dt;
		m.y += m.vy * dt;
		m.z += m.vz * dt;
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  대화 / 퀘스트
// ─────────────────────────────────────────────────────────────────────────

int Game::DialogueSetForNpc(const NpcActor* npc) const
{
	if (npc == 0)
		return 12;

	// 미렌만 진행 단계에 따라 대사가 갈린다.
	if (npc->DialogueSet() == 0)
	{
		switch (m_Stage)
		{
		case QS_NOT_STARTED:   return 0;
		case QS_ASK_VILLAGERS: return 1;
		case QS_GO_TO_SHORE:   return 2;
		case QS_RETURN:        return 3;
		default:               return 12;
		}
	}

	return npc->DialogueSet();
}

int Game::DialogueSetForSigil() const
{
	if (m_Stage == QS_GO_TO_SHORE) return 11;
	if (m_HasCoin)                 return 13;

	return 10;
}

void Game::StartDialogue(int setIndex, NpcActor* npc)
{
	m_DialogueOpen = true;
	m_DialogueSet  = setIndex;
	m_DialogueLine = 0;
	m_TalkingNpc   = npc;

	if (npc != 0)
		npc->talking = true;

	m_JournalOpen = false;
	m_StatsOpen   = false;
}

void Game::AdvanceDialogue()
{
	const DialogueSet& set = DIALOGUE[m_DialogueSet];

	++m_DialogueLine;

	if (m_DialogueLine < set.count)
		return;

	int       finished = m_DialogueSet;
	NpcActor* speaker  = m_TalkingNpc;

	if (speaker != 0)
		speaker->talking = false;

	m_DialogueOpen = false;
	m_DialogueSet  = -1;
	m_DialogueLine = 0;
	m_TalkingNpc   = 0;

	// 증인 표시는 방금 대화한 상대에게 직접 남긴다.
	if (speaker != 0 && speaker->WitnessId() >= 0 && !speaker->talked)
	{
		speaker->talked = true;
		++m_WitnessCount;
	}

	OnDialogueFinished(finished);
}

void Game::OnDialogueFinished(int setIndex)
{
	switch (setIndex)
	{
	// 미렌에게 의뢰를 받았다
	case 0:
		m_Stage = QS_ASK_VILLAGERS;
		Toast(L"새 의뢰 수락 — 호수 보고서");

		break;

	// 증인 셋 — 호드 / 펠 / 타마
	case 4:
	case 5:
	case 6:
		if (m_Stage != QS_ASK_VILLAGERS)
			break;

		if (m_WitnessCount >= 3)
		{
			m_Stage = QS_GO_TO_SHORE;
			Toast(L"세 사람의 증언을 모두 들었다 — 호숫가로");
		}
		else
		{
			Toast(L"증언 기록됨");
		}

		break;

	// 비석에서 동전을 얻었다
	case 11:
		m_HasCoin = true;
		m_Stage   = QS_RETURN;
		Toast(L"획득 — 소용돌이 문양 구리 동전");

		break;

	// 미렌에게 복귀했다
	case 3:
		m_Stage    = QS_DONE;
		m_EndTimer = 0.0f;
		Toast(L"의뢰 완료");

		break;

	default:
		break;
	}
}

void Game::Toast(const wchar_t* text)
{
	m_ToastText  = text;
	m_ToastTimer = 4.0f;
}


// ─────────────────────────────────────────────────────────────────────────
//  렌더
// ─────────────────────────────────────────────────────────────────────────

void Game::Render()
{
	PROF_SCOPE(PROF_RENDER);

	float px = (m_Player != 0) ? m_Player->WorldX() : 0.0f;
	float py = (m_Player != 0) ? m_Player->WorldY() : 0.0f;

	// ── 씬 : 오프스크린 버퍼에 그린다 ──
	m_R->BeginScene(0.026f, 0.032f, 0.044f);

	float ox = (float)m_WinW * 0.5f  - m_CamX;
	float oy = (float)m_WinH * 0.52f - m_CamY;

	{
		PROF_SCOPE(PROF_RND_TILES);
		Draw::Tiles(*m_R, m_World, ox, oy, m_WinW, m_WinH, m_Time, px, py);
	}

	{
		PROF_SCOPE(PROF_RND_GLOW);
		RenderGroundGlow(ox, oy);
	}

	// 비석 연출은 진행 상태를 따라간다.
	if (m_Sigil != 0)
	{
		m_Sigil->sigilTaken     = m_HasCoin;
		m_Sigil->sigilHighlight = (m_Stage == QS_GO_TO_SHORE);
	}

	RenderContext rctx = MakeRenderContext(ox, oy);
	m_Scene.Render(rctx);

	{
		PROF_SCOPE(PROF_RND_MOTES);
		RenderMotes(ox, oy);
	}

	m_R->EndScene();

	// ── 사후처리 : 블룸 + 색보정 + 비네트 + 그레인 ──
	m_R->PostProcess(m_Dread, m_Time);

	// ── UI 는 보정 위에 얹는다 ──
	{
		PROF_SCOPE(PROF_RND_UI);
		RenderUI();
	}
}

void Game::RenderGroundGlow(float ox, float oy)
{
	float px = (m_Player != 0) ? m_Player->WorldX() : 0.0f;
	float py = (m_Player != 0) ? m_Player->WorldY() : 0.0f;

	// 모닥불 · 등불 · 창문에서 새어 나오는 빛
	for (size_t i = 0; i < m_Lights.size(); ++i)
	{
		const LightSource& l = m_Lights[i];

		if (Dist(l.x, l.y, px, py) > 34.0f)
			continue;

		float sx = ox + IsoScreenX(l.x, l.y);
		float sy = oy + IsoScreenY(l.x, l.y);

		float k = l.intensity;

		if (l.flicker)
			k *= 0.86f + 0.14f * sinf(m_Time * 4.3f + l.x * 0.7f + l.y * 1.1f);

		Draw::LightPool(*m_R, sx, sy, l.radius, l.r, l.g, l.b, k);
	}

	// 플레이어의 랜턴
	{
		float sx = ox + IsoScreenX(px, py);
		float sy = oy + IsoScreenY(px, py);

		Draw::LightPool(*m_R, sx, sy, 150.0f, 0.98f, 0.72f, 0.36f, 0.30f);
	}

	// 비석에서 새어 나오는 빛
	if (m_Level == LEVEL_VILLAGE)
	{
		float sx = ox + IsoScreenX(SIGIL_X, SIGIL_Y);
		float sy = oy + IsoScreenY(SIGIL_X, SIGIL_Y);

		float pulse = 0.5f + 0.5f * sinf(m_Time * 1.5f);
		float boost = (m_Stage == QS_GO_TO_SHORE) ? 1.8f : 1.0f;

		Draw::LightPool(*m_R, sx, sy, 175.0f, 0.24f, 0.92f, 0.84f,
		                (0.16f + pulse * 0.10f) * boost);
	}

	// 경험치 구슬은 스스로 바닥을 밝힌다.
	m_Scene.QueryNear(px, py, 26.0f, ACTOR_PICKUP, m_Query);

	for (size_t i = 0; i < m_Query.size(); ++i)
	{
		PickupActor* p = (PickupActor*)m_Query[i];

		if (p->PickupKind() != 0)
			continue;

		float sx = ox + IsoScreenX(p->WorldX(), p->WorldY());
		float sy = oy + IsoScreenY(p->WorldX(), p->WorldY());

		Draw::LightPool(*m_R, sx, sy, 60.0f, 0.26f, 0.90f, 0.86f, 0.18f);
	}
}

void Game::RenderMotes(float ox, float oy)
{
	for (size_t i = 0; i < m_Motes.size(); ++i)
	{
		const Mote& m = m_Motes[i];

		if (m.life <= 0.0f)
			continue;

		float sx = ox + IsoScreenX(m.x, m.y);
		float sy = oy + IsoScreenY(m.x, m.y, m.z * TILE_H);

		if (sx < 0.0f || sx > (float)m_WinW || sy < 0.0f || sy > (float)m_WinH)
			continue;

		// 생성/소멸 구간에서 부드럽게 뜨고 진다.
		float t = m.life / m.maxLife;
		float a = Clamp01(t * 3.0f) * Clamp01((1.0f - t) * 3.0f) * 0.8f;

		float blink = 0.6f + 0.4f * sinf(m_Time * 4.0f + (float)i);

		if (m.kind == 1)
			m_R->DrawQuad(sx, sy, 3.0f, 3.0f, 0.42f, 0.92f, 0.88f, a * blink * 0.85f);
		else if (m.kind == 2)
			m_R->DrawQuad(sx, sy, 2.6f, 2.6f, 1.00f, 0.62f, 0.24f, a);
		else
			m_R->DrawQuad(sx, sy, 3.0f, 3.0f, 0.98f, 0.84f, 0.44f, a * blink);
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  UI
// ─────────────────────────────────────────────────────────────────────────

void Game::Panel(float x, float y, float w, float h, float a)
{
	m_R->DrawQuad(x, y, w, h, 0.040f, 0.044f, 0.058f, a);

	// 위쪽 테두리만 길드색으로 강조한다.
	m_R->DrawQuad(x, y, w, 2.0f, 0.66f, 0.33f, 0.12f, a);

	m_R->DrawQuad(x, y + h - 1.0f, w, 1.0f, 0.30f, 0.26f, 0.22f, a * 0.8f);
	m_R->DrawQuad(x, y, 1.0f, h, 0.30f, 0.26f, 0.22f, a * 0.8f);
	m_R->DrawQuad(x + w - 1.0f, y, 1.0f, h, 0.30f, 0.26f, 0.22f, a * 0.8f);
}

void Game::RenderUI()
{
	wchar_t buf[192];

	// 폐허에서는 의뢰 대신 전투 정보를 보여준다.
	if (m_Level == LEVEL_RUIN)
	{
		RenderRuinUI();
		return;
	}

	// ── 목표 ──
	{
		const float px = 20.0f, py = 20.0f, pw = 420.0f, ph = 64.0f;

		Panel(px, py, pw, ph, 0.82f);

		m_R->DrawString(px + 14.0f, py + 24.0f, L"의 뢰",
		                0.76f, 0.46f, 0.20f, 1.0f, FONT_SMALL);

		const wchar_t* obj = L"";

		switch (m_Stage)
		{
		case QS_NOT_STARTED:
			obj = L"광장 게시판 옆의 길드 서기를 찾아간다.";
			break;

		case QS_ASK_VILLAGERS:
			swprintf(buf, 192, L"호드 · 펠 · 타마에게 목격담을 듣는다  (%d/3)", m_WitnessCount);
			obj = buf;
			break;

		case QS_GO_TO_SHORE:
			obj = L"남동쪽 길을 따라 호숫가로 내려간다.";
			break;

		case QS_RETURN:
			obj = L"동전을 들고 미렌에게 돌아간다.";
			break;

		default:
			obj = L"의뢰 완료.";
			break;
		}

		m_R->DrawString(px + 14.0f, py + 50.0f, obj,
		                0.88f, 0.86f, 0.80f, 1.0f, FONT_BODY);
	}

	// ── 알림 ──
	if (m_ToastTimer > 0.0f && m_ToastText != 0)
	{
		float a = Clamp01(m_ToastTimer / 0.9f);
		float w = m_R->TextWidth(m_ToastText, FONT_BODY) + 32.0f;

		m_R->DrawQuad(20.0f, 94.0f, w, 34.0f, 0.040f, 0.044f, 0.058f, 0.84f * a);
		m_R->DrawQuad(20.0f, 94.0f, 3.0f, 34.0f, 0.24f, 0.82f, 0.76f, 0.92f * a);

		m_R->DrawString(36.0f, 117.0f, m_ToastText, 0.62f, 0.92f, 0.88f, a, FONT_BODY);
	}

	// ── 조작 안내 ──
	m_R->DrawString(20.0f, (float)m_WinH - 16.0f,
	                L"WASD 이동     E 상호작용     J 일지     0 폐허로     Esc 종료",
	                0.46f, 0.45f, 0.42f, 1.0f, FONT_SMALL);

	// ── 상호작용 프롬프트 ──
	if (!m_DialogueOpen && (m_FocusNpc != 0 || m_FocusSigil))
	{
		if (m_FocusNpc != 0)
			swprintf(buf, 192, L"[E]  %s 에게 말 걸기", m_FocusNpc->Name());
		else
			swprintf(buf, 192, L"[E]  비석 살펴보기");

		float tw = m_R->TextWidth(buf, FONT_BODY);
		float bx = (float)m_WinW * 0.5f - tw * 0.5f;
		float by = (float)m_WinH - 126.0f;

		m_R->DrawQuad(bx - 18.0f, by - 24.0f, tw + 36.0f, 34.0f, 0.040f, 0.044f, 0.058f, 0.88f);
		m_R->DrawQuad(bx - 18.0f, by - 24.0f, tw + 36.0f, 2.0f, 0.66f, 0.33f, 0.12f, 0.92f);

		m_R->DrawString(bx, by, buf, 0.95f, 0.88f, 0.72f, 1.0f, FONT_BODY);
	}

	if (m_DialogueOpen)
		RenderDialogueBox();

	if (m_JournalOpen)
		RenderJournal();

	if (m_TitleTimer > 0.0f)
		RenderTitleCard();

	if (m_EndTimer >= 1.2f)
		RenderEndCard();
}

void Game::RenderDialogueBox()
{
	const DialogueSet& set = DIALOGUE[m_DialogueSet];

	const float pw = (float)m_WinW - 160.0f;
	const float ph = 146.0f;
	const float px = 80.0f;
	const float py = (float)m_WinH - ph - 48.0f;

	Panel(px, py, pw, ph, 0.93f);

	m_R->DrawString(px + 22.0f, py + 34.0f, set.speaker,
	                0.88f, 0.57f, 0.25f, 1.0f, FONT_TITLE);

	int lines = WrapByWidth(*m_R, set.lines[m_DialogueLine], pw - 46.0f, FONT_BODY);

	for (int i = 0; i < lines; ++i)
	{
		m_R->DrawString(px + 22.0f, py + 68.0f + (float)i * 24.0f, s_Wrap[i],
		                0.91f, 0.89f, 0.84f, 1.0f, FONT_BODY);
	}

	wchar_t hint[64];
	swprintf(hint, 64, L"[E]  %s   (%d/%d)",
	         (m_DialogueLine + 1 < set.count) ? L"계속" : L"닫기",
	         m_DialogueLine + 1, set.count);

	float hw = m_R->TextWidth(hint, FONT_SMALL);

	m_R->DrawString(px + pw - hw - 20.0f, py + ph - 14.0f, hint,
	                0.54f, 0.52f, 0.48f, 1.0f, FONT_SMALL);
}

void Game::RenderJournal()
{
	const float pw = 470.0f, ph = 268.0f;
	const float px = (float)m_WinW * 0.5f - pw * 0.5f;
	const float py = (float)m_WinH * 0.5f - ph * 0.5f;

	Panel(px, py, pw, ph, 0.95f);

	m_R->DrawString(px + 22.0f, py + 38.0f, L"길드 일지",
	                0.88f, 0.57f, 0.25f, 1.0f, FONT_TITLE);

	m_R->DrawString(px + 22.0f, py + 64.0f, L"호 수 보 고 서",
	                0.74f, 0.72f, 0.66f, 1.0f, FONT_SMALL);

	wchar_t askLine[96];
	swprintf(askLine, 96, L"세 사람의 증언을 듣는다  (%d/3)", m_WitnessCount);

	struct Row
	{
		const wchar_t* text;
		bool           done;
	};

	Row rows[4] =
	{
		{ L"미렌에게 의뢰를 받는다", m_Stage != QS_NOT_STARTED },
		{ askLine,                   m_WitnessCount >= 3 },
		{ L"호숫가를 조사한다",      m_HasCoin },
		{ L"미렌에게 보고한다",      m_Stage == QS_DONE },
	};

	for (int i = 0; i < 4; ++i)
	{
		float y = py + 102.0f + (float)i * 30.0f;

		if (rows[i].done)
		{
			m_R->DrawString(px + 24.0f, y, L"[v]", 0.32f, 0.78f, 0.72f, 1.0f, FONT_BODY);
			m_R->DrawString(px + 60.0f, y, rows[i].text, 0.54f, 0.62f, 0.58f, 1.0f, FONT_BODY);
		}
		else
		{
			m_R->DrawString(px + 24.0f, y, L"[ ]", 0.74f, 0.48f, 0.22f, 1.0f, FONT_BODY);
			m_R->DrawString(px + 60.0f, y, rows[i].text, 0.91f, 0.89f, 0.84f, 1.0f, FONT_BODY);
		}
	}

	if (m_HasCoin)
	{
		m_R->DrawString(px + 24.0f, py + ph - 36.0f, L"소지품 — 소용돌이 문양 구리 동전",
		                0.44f, 0.84f, 0.80f, 1.0f, FONT_SMALL);
	}

	m_R->DrawString(px + pw - 82.0f, py + ph - 14.0f, L"[J] 닫기",
	                0.54f, 0.52f, 0.48f, 1.0f, FONT_SMALL);
}

void Game::RenderTitleCard()
{
	// 마지막 1.6초 동안 서서히 걷힌다.
	float a = Clamp01(m_TitleTimer / 1.6f);

	float cx = (float)m_WinW * 0.5f;
	float cy = (float)m_WinH * 0.34f;

	m_R->DrawQuad(0.0f, cy - 58.0f, (float)m_WinW, 110.0f, 0.018f, 0.022f, 0.030f, 0.74f * a);
	m_R->DrawQuad(0.0f, cy - 58.0f, (float)m_WinW, 2.0f, 0.66f, 0.33f, 0.12f, 0.78f * a);
	m_R->DrawQuad(0.0f, cy + 50.0f, (float)m_WinW, 2.0f, 0.66f, 0.33f, 0.12f, 0.78f * a);

	const wchar_t* t1 = L"라 벤 홀";
	const wchar_t* t2 = L"조용한 마을, 대체로는";

	float w1 = m_R->TextWidth(t1, FONT_TITLE);
	float w2 = m_R->TextWidth(t2, FONT_BODY);

	m_R->DrawString(cx - w1 * 0.5f, cy -  6.0f, t1, 0.92f, 0.84f, 0.70f, a, FONT_TITLE);
	m_R->DrawString(cx - w2 * 0.5f, cy + 28.0f, t2, 0.60f, 0.56f, 0.50f, a, FONT_BODY);
}

void Game::RenderEndCard()
{
	float a = Clamp01((m_EndTimer - 1.2f) / 1.4f);

	m_R->DrawQuad(0.0f, 0.0f, (float)m_WinW, (float)m_WinH, 0.010f, 0.014f, 0.020f, 0.90f * a);

	float cx = (float)m_WinW * 0.5f;
	float cy = (float)m_WinH * 0.42f;

	const wchar_t* l1 = L"서기는 보고서를 정리한다.";
	const wchar_t* l2 = L"호수는 나머지를 가진다.";
	const wchar_t* l3 = L"— 프로토타입 종료 —";
	const wchar_t* l4 = L"Esc 로 종료";

	float w1 = m_R->TextWidth(l1, FONT_TITLE);
	float w2 = m_R->TextWidth(l2, FONT_TITLE);
	float w3 = m_R->TextWidth(l3, FONT_BODY);
	float w4 = m_R->TextWidth(l4, FONT_SMALL);

	m_R->DrawString(cx - w1 * 0.5f, cy,          l1, 0.90f, 0.86f, 0.78f, a, FONT_TITLE);
	m_R->DrawString(cx - w2 * 0.5f, cy +  38.0f, l2, 0.90f, 0.86f, 0.78f, a, FONT_TITLE);
	m_R->DrawString(cx - w3 * 0.5f, cy +  86.0f, l3, 0.46f, 0.74f, 0.70f, a, FONT_BODY);
	m_R->DrawString(cx - w4 * 0.5f, cy + 122.0f, l4, 0.42f, 0.41f, 0.38f, a, FONT_SMALL);
}
