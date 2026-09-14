#include "stdafx.h"
#include "Game.h"
#include "Renderer.h"
#include "Draw.h"
#include "Input.h"
#include "IsoMath.h"

#include <cmath>
#include <cstdio>
#include <cwchar>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────
//  상수
// ─────────────────────────────────────────────────────────────────────────

static const float PLAYER_SPEED = 4.6f;    // 타일/초
static const float NPC_SPEED    = 1.1f;
static const float BODY_RADIUS  = 0.30f;
static const float TALK_RANGE   = 2.0f;

static const int MOTE_COUNT  = 96;
static const int NPC_COUNT   = 14;
static const int BEAST_COUNT = 16;

// 외형 팔레트 — 망토 / 피부 / 머리
static const PersonLook LOOKS[] =
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

static bool DrawableLess(const Drawable& a, const Drawable& b)
{
	return a.depth < b.depth;
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
//  초기화
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

void Game::Init(Renderer* renderer, int windowW, int windowH)
{
	m_R    = renderer;
	m_WinW = windowW;
	m_WinH = windowH;

	m_World.Generate();

	// ── 플레이어 ──
	m_Player.x  = SPAWN_X;
	m_Player.y  = SPAWN_Y;
	m_Player.vx = 0.0f;
	m_Player.vy = 0.0f;

	m_Player.walkPhase  = 0.0f;
	m_Player.walkAmount = 0.0f;
	m_Player.bobPhase   = 0.0f;
	m_Player.facing     = 1;

	// ── NPC ──
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

	m_Npcs.clear();

	for (int i = 0; i < NPC_COUNT; ++i)
	{
		Npc n;

		n.name        = defs[i].name;
		n.dialogueSet = defs[i].set;
		n.witnessId   = defs[i].witness;
		n.look        = defs[i].look;
		n.talked      = false;
		n.wander      = defs[i].wander;

		// 배치 좌표가 건물에 물리면 주변 빈자리로 밀어낸다.
		float px = defs[i].x;
		float py = defs[i].y;
		FindFreeSpot(px, py, BODY_RADIUS);

		n.homeX = n.targetX = px;
		n.homeY = n.targetY = py;
		n.waitTimer = 1.0f + (float)i * 0.4f;

		n.c.x  = px;
		n.c.y  = py;
		n.c.vx = 0.0f;
		n.c.vy = 0.0f;

		n.c.walkPhase  = (float)i * 0.9f;
		n.c.walkAmount = 0.0f;
		n.c.bobPhase   = (float)i * 1.3f;
		n.c.facing     = 1;

		m_Npcs.push_back(n);
	}

	// ── 야생 짐승 ──
	// 마을에서 떨어진 빈 땅에 흩어 놓는다. 호수 근처에는 두지 않는다 —
	// 짐승이 물가를 피한다는 것 자체가 마을 사람들의 증언과 맞물린다.
	m_Beasts.clear();

	{
		unsigned int seed  = 0xC0FFEEu;
		int          guard = 0;

		while ((int)m_Beasts.size() < BEAST_COUNT && guard < 4000)
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

			Beast b;
			int   i = (int)m_Beasts.size();

			b.kind = (i % 4 == 0) ? BEAST_CROW
			       : (i % 4 == 1) ? BEAST_DEER
			       : (i % 4 == 2) ? BEAST_WOLF
			                      : BEAST_DEER;

			b.homeX = b.targetX = bx;
			b.homeY = b.targetY = by;

			b.waitTimer = (float)(i % 5) * 0.8f;
			b.alert     = 0.0f;
			b.speed     = (b.kind == BEAST_WOLF) ? 2.2f : (b.kind == BEAST_DEER ? 2.6f : 1.6f);

			b.c.x  = bx;
			b.c.y  = by;
			b.c.vx = 0.0f;
			b.c.vy = 0.0f;

			b.c.walkPhase  = (float)i;
			b.c.walkAmount = 0.0f;
			b.c.bobPhase   = (float)i * 0.7f;
			b.c.facing     = 1;

			m_Beasts.push_back(b);
		}
	}

	// ── 광원 목록 ──
	// 매 프레임 오브젝트를 훑지 않도록 미리 뽑아 둔다.
	m_Lights.clear();

	{
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

	m_Draws.reserve(4096);
}

void Game::Resize(int windowW, int windowH)
{
	m_WinW = windowW;
	m_WinH = windowH;
}


// ─────────────────────────────────────────────────────────────────────────
//  갱신
// ─────────────────────────────────────────────────────────────────────────

void Game::StepAnim(Character& c, float dt, bool moving)
{
	float target = moving ? 1.0f : 0.0f;

	c.walkAmount += (target - c.walkAmount) * Clamp01(dt * 9.0f);

	if (moving)
		c.walkPhase += dt * 9.0f;
}

void Game::Update(float dt)
{
	m_Time += dt;

	if (m_TitleTimer > 0.0f) m_TitleTimer -= dt;
	if (m_ToastTimer > 0.0f) m_ToastTimer -= dt;
	if (m_EndTimer  >= 0.0f) m_EndTimer  += dt;

	if (Input::Pressed('j'))
		m_JournalOpen = !m_JournalOpen;

	if (m_DialogueOpen)
	{
		// 대화 중에는 이동을 멈춘다.
		m_Player.vx = 0.0f;
		m_Player.vy = 0.0f;
		StepAnim(m_Player, dt, false);

		if (Input::Pressed('e') || Input::Pressed(' '))
			AdvanceDialogue();
	}
	else
	{
		UpdatePlayer(dt);
		UpdateInteraction();

		if (Input::Pressed('e'))
		{
			if (m_FocusNpc >= 0)
				StartDialogue(DialogueSetForNpc(m_FocusNpc), m_FocusNpc);
			else if (m_FocusSigil)
				StartDialogue(DialogueSetForSigil(), -1);
		}
	}

	UpdateNpcs(dt);
	UpdateBeasts(dt);
	UpdateMotes(dt);

	// ── 카메라 — 플레이어를 부드럽게 따라간다 ──
	float targetCamX = IsoScreenX(m_Player.x, m_Player.y);
	float targetCamY = IsoScreenY(m_Player.x, m_Player.y);

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

	// ── 비석에 가까울수록 화면이 무거워진다 ──
	float d      = Dist(m_Player.x, m_Player.y, SIGIL_X, SIGIL_Y);
	float target = 1.0f - Clamp01((d - 5.0f) / 22.0f);

	// 동전을 쥔 뒤로는 완전히 걷히지 않는다.
	if (m_HasCoin && target < 0.22f)
		target = 0.22f;

	m_Dread += (target - m_Dread) * Clamp01(dt * 1.8f);
}

void Game::MoveWithCollision(Character& c, float dx, float dy, float radius)
{
	// 축을 나눠 처리해야 벽을 따라 미끄러진다.
	if (dx != 0.0f && !m_World.Blocked(c.x + dx, c.y, radius))
		c.x += dx;

	if (dy != 0.0f && !m_World.Blocked(c.x, c.y + dy, radius))
		c.y += dy;
}

void Game::UpdatePlayer(float dt)
{
	float ix = 0.0f;
	float iy = 0.0f;

	// 쿼터뷰에서는 화면 기준으로 걷는 느낌이 자연스럽다.
	// W = 화면 위쪽 = 월드 (-x, -y)
	if (Input::Held('w')) { ix -= 1.0f; iy -= 1.0f; }
	if (Input::Held('s')) { ix += 1.0f; iy += 1.0f; }
	if (Input::Held('a')) { ix -= 1.0f; iy += 1.0f; }
	if (Input::Held('d')) { ix += 1.0f; iy -= 1.0f; }

	float len    = sqrtf(ix * ix + iy * iy);
	bool  moving = false;

	if (len > 0.0001f)
	{
		ix /= len;
		iy /= len;

		float beforeX = m_Player.x;
		float beforeY = m_Player.y;

		MoveWithCollision(m_Player, ix * PLAYER_SPEED * dt, iy * PLAYER_SPEED * dt, BODY_RADIUS);

		m_Player.vx = m_Player.x - beforeX;
		m_Player.vy = m_Player.y - beforeY;

		moving = (fabsf(m_Player.vx) + fabsf(m_Player.vy)) > 0.0002f;

		float screenDir = IsoScreenX(ix, iy);

		if (screenDir >  0.01f) m_Player.facing =  1;
		if (screenDir < -0.01f) m_Player.facing = -1;
	}
	else
	{
		m_Player.vx = 0.0f;
		m_Player.vy = 0.0f;
	}

	StepAnim(m_Player, dt, moving);
}

void Game::UpdateNpcs(float dt)
{
	for (size_t i = 0; i < m_Npcs.size(); ++i)
	{
		Npc& n = m_Npcs[i];

		// 말을 거는 중이면 가만히 서서 플레이어 쪽을 본다.
		if (m_DialogueOpen && m_TalkingNpc == (int)i)
		{
			n.c.vx = 0.0f;
			n.c.vy = 0.0f;
			StepAnim(n.c, dt, false);

			float sd = IsoScreenX(m_Player.x - n.c.x, m_Player.y - n.c.y);

			if (sd >  0.01f) n.c.facing =  1;
			if (sd < -0.01f) n.c.facing = -1;

			continue;
		}

		// 집 주변을 배회한다.
		n.waitTimer -= dt;

		if (n.waitTimer <= 0.0f)
		{
			float a = (float)((i * 37 + (int)(m_Time * 10.0f)) % 628) * 0.01f;
			float r = n.wander * (0.35f + 0.65f * ((float)((i * 53 + (int)m_Time) % 100) / 100.0f));

			n.targetX = n.homeX + cosf(a) * r;
			n.targetY = n.homeY + sinf(a) * r;

			n.waitTimer = 2.4f + (float)((i * 17) % 6);
		}

		float dx = n.targetX - n.c.x;
		float dy = n.targetY - n.c.y;
		float d  = sqrtf(dx * dx + dy * dy);

		bool moving = false;

		if (d > 0.12f)
		{
			float step = NPC_SPEED * dt;
			float mx   = dx / d * step;
			float my   = dy / d * step;

			float beforeX = n.c.x;
			float beforeY = n.c.y;

			MoveWithCollision(n.c, mx, my, BODY_RADIUS * 0.8f);

			n.c.vx = n.c.x - beforeX;
			n.c.vy = n.c.y - beforeY;

			moving = (fabsf(n.c.vx) + fabsf(n.c.vy)) > 0.0001f;

			// 벽에 막혀 제자리면 다음 목적지를 새로 뽑는다.
			if (!moving)
				n.waitTimer = 0.0f;

			float screenDir = IsoScreenX(mx, my);

			if (screenDir >  0.001f) n.c.facing =  1;
			if (screenDir < -0.001f) n.c.facing = -1;
		}
		else
		{
			n.c.vx = 0.0f;
			n.c.vy = 0.0f;
		}

		StepAnim(n.c, dt, moving);
	}
}

void Game::UpdateBeasts(float dt)
{
	for (size_t i = 0; i < m_Beasts.size(); ++i)
	{
		Beast& b = m_Beasts[i];

		float toPlayer = Dist(b.c.x, b.c.y, m_Player.x, m_Player.y);

		// 종마다 사람을 의식하는 거리가 다르다.
		float notice = (b.kind == BEAST_DEER) ? 13.0f
		             : (b.kind == BEAST_WOLF) ? 10.0f
		                                      : 7.0f;

		float wantAlert = Clamp01((notice - toPlayer) / notice);
		b.alert += (wantAlert - b.alert) * Clamp01(dt * 2.5f);

		bool fleeing = false;

		if (toPlayer < notice * 0.62f && b.kind != BEAST_WOLF)
		{
			// 사슴과 까마귀는 달아난다.
			float dx = b.c.x - m_Player.x;
			float dy = b.c.y - m_Player.y;
			float d  = sqrtf(dx * dx + dy * dy);

			if (d > 0.01f)
			{
				b.targetX = b.c.x + dx / d * 7.0f;
				b.targetY = b.c.y + dy / d * 7.0f;

				fleeing = true;
			}
		}
		else if (toPlayer < notice * 0.45f && b.kind == BEAST_WOLF)
		{
			// 늑대는 물러서되 등을 보이지 않는다 — 아주 가까울 때만 거리를 벌린다.
			float dx = b.c.x - m_Player.x;
			float dy = b.c.y - m_Player.y;
			float d  = sqrtf(dx * dx + dy * dy);

			if (d > 0.01f)
			{
				b.targetX = b.c.x + dx / d * 3.5f;
				b.targetY = b.c.y + dy / d * 3.5f;
			}
		}
		else
		{
			b.waitTimer -= dt;

			if (b.waitTimer <= 0.0f)
			{
				float a = (float)((i * 53 + (int)(m_Time * 13.0f)) % 628) * 0.01f;
				float r = 2.0f + (float)((i * 31 + (int)m_Time) % 60) * 0.09f;

				b.targetX = b.homeX + cosf(a) * r;
				b.targetY = b.homeY + sinf(a) * r;

				b.waitTimer = 2.0f + (float)((i * 11) % 7);
			}
		}

		float dx = b.targetX - b.c.x;
		float dy = b.targetY - b.c.y;
		float d  = sqrtf(dx * dx + dy * dy);

		bool moving = false;

		if (d > 0.15f)
		{
			float speed = b.speed * (fleeing ? 2.0f : (0.55f + b.alert * 0.5f));
			float step  = speed * dt;
			float mx    = dx / d * step;
			float my    = dy / d * step;

			float beforeX = b.c.x;
			float beforeY = b.c.y;

			MoveWithCollision(b.c, mx, my, BODY_RADIUS * 0.7f);

			b.c.vx = b.c.x - beforeX;
			b.c.vy = b.c.y - beforeY;

			moving = (fabsf(b.c.vx) + fabsf(b.c.vy)) > 0.0001f;

			if (!moving)
				b.waitTimer = 0.0f;

			float screenDir = IsoScreenX(mx, my);

			if (screenDir >  0.001f) b.c.facing =  1;
			if (screenDir < -0.001f) b.c.facing = -1;
		}
		else
		{
			b.c.vx = 0.0f;
			b.c.vy = 0.0f;
		}

		// 걷기 위상은 종마다 속도가 다르다.
		float target = moving ? 1.0f : 0.0f;
		b.c.walkAmount += (target - b.c.walkAmount) * Clamp01(dt * 9.0f);

		if (moving)
			b.c.walkPhase += dt * (b.kind == BEAST_CROW ? 13.0f : 11.0f);
	}
}

void Game::UpdateInteraction()
{
	m_FocusNpc   = -1;
	m_FocusSigil = false;

	float best = TALK_RANGE;

	for (size_t i = 0; i < m_Npcs.size(); ++i)
	{
		float d = Dist(m_Player.x, m_Player.y, m_Npcs[i].c.x, m_Npcs[i].c.y);

		if (d < best)
		{
			best = d;
			m_FocusNpc = (int)i;
		}
	}

	float ds = Dist(m_Player.x, m_Player.y, SIGIL_X, SIGIL_Y);

	if (ds < best)
	{
		m_FocusNpc   = -1;
		m_FocusSigil = true;
	}
}

void Game::UpdateMotes(float dt)
{
	for (int i = 0; i < (int)m_Motes.size(); ++i)
	{
		Mote& m = m_Motes[i];

		m.life -= dt;

		if (m.life <= 0.0f)
		{
			// 수명이 다하면 플레이어 주변에 다시 뿌린다.
			float a = (float)((i * 71 + (int)(m_Time * 97.0f)) % 628) * 0.01f;
			float r = 3.0f + (float)((i * 13 + (int)(m_Time * 31.0f)) % 150) * 0.1f;

			m.x = m_Player.x + cosf(a) * r;
			m.y = m_Player.y + sinf(a) * r;
			m.z = 0.2f + (float)((i * 29) % 100) * 0.022f;

			m.vx = (((float)((i * 7  + (int)m_Time) % 100) / 100.0f) - 0.5f) * 0.25f;
			m.vy = (((float)((i * 11 + (int)m_Time) % 100) / 100.0f) - 0.5f) * 0.25f;
			m.vz = 0.05f + (float)((i * 3) % 50) * 0.004f;

			m.maxLife = 3.0f + (float)((i * 19) % 40) * 0.1f;
			m.life    = m.maxLife;

			// 위치에 따라 성격이 달라진다.
			if (Dist(m.x, m.y, SIGIL_X, SIGIL_Y) < 16.0f)
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

int Game::DialogueSetForNpc(int npcIndex) const
{
	// 미렌만 진행 단계에 따라 대사가 갈린다.
	if (npcIndex == 0)
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

	return m_Npcs[npcIndex].dialogueSet;
}

int Game::DialogueSetForSigil() const
{
	if (m_Stage == QS_GO_TO_SHORE) return 11;
	if (m_HasCoin)                 return 13;

	return 10;
}

void Game::StartDialogue(int setIndex, int npcIndex)
{
	m_DialogueOpen = true;
	m_DialogueSet  = setIndex;
	m_DialogueLine = 0;
	m_TalkingNpc   = npcIndex;

	m_JournalOpen = false;
}

void Game::AdvanceDialogue()
{
	const DialogueSet& set = DIALOGUE[m_DialogueSet];

	++m_DialogueLine;

	if (m_DialogueLine < set.count)
		return;

	int finished = m_DialogueSet;

	m_DialogueOpen = false;
	m_DialogueSet  = -1;
	m_DialogueLine = 0;
	m_TalkingNpc   = -1;

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
	{
		int witness = (setIndex == 4) ? 0 : (setIndex == 5 ? 1 : 2);

		for (size_t i = 0; i < m_Npcs.size(); ++i)
		{
			if (m_Npcs[i].witnessId != witness || m_Npcs[i].talked)
				continue;

			m_Npcs[i].talked = true;
			++m_WitnessCount;

			if (m_WitnessCount >= 3)
			{
				if (m_Stage == QS_ASK_VILLAGERS)
				{
					m_Stage = QS_GO_TO_SHORE;
					Toast(L"세 사람의 증언을 모두 들었다 — 호숫가로");
				}
			}
			else if (m_Stage == QS_ASK_VILLAGERS)
			{
				Toast(L"증언 기록됨");
			}

			break;
		}

		break;
	}

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
	// ── 씬 : 오프스크린 버퍼에 그린다 ──
	m_R->BeginScene(0.026f, 0.032f, 0.044f);

	float ox = (float)m_WinW * 0.5f  - m_CamX;
	float oy = (float)m_WinH * 0.52f - m_CamY;

	Draw::Tiles(*m_R, m_World, ox, oy, m_WinW, m_WinH, m_Time, m_Player.x, m_Player.y);

	RenderGroundGlow(ox, oy);
	RenderSorted(ox, oy);
	RenderMotes(ox, oy);

	m_R->EndScene();

	// ── 사후처리 : 블룸 + 색보정 + 비네트 + 그레인 ──
	m_R->PostProcess(m_Dread, m_Time);

	// ── UI 는 보정 위에 얹는다 ──
	RenderUI();
}

void Game::RenderGroundGlow(float ox, float oy)
{
	// 모닥불 · 등불 · 창문에서 새어 나오는 빛
	for (size_t i = 0; i < m_Lights.size(); ++i)
	{
		const LightSource& l = m_Lights[i];

		if (Dist(l.x, l.y, m_Player.x, m_Player.y) > 34.0f)
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
		float sx = ox + IsoScreenX(m_Player.x, m_Player.y);
		float sy = oy + IsoScreenY(m_Player.x, m_Player.y);

		Draw::LightPool(*m_R, sx, sy, 150.0f, 0.98f, 0.72f, 0.36f, 0.30f);
	}

	// 비석에서 새어 나오는 빛
	{
		float sx = ox + IsoScreenX(SIGIL_X, SIGIL_Y);
		float sy = oy + IsoScreenY(SIGIL_X, SIGIL_Y);

		float pulse = 0.5f + 0.5f * sinf(m_Time * 1.5f);
		float boost = (m_Stage == QS_GO_TO_SHORE) ? 1.8f : 1.0f;

		Draw::LightPool(*m_R, sx, sy, 175.0f, 0.24f, 0.92f, 0.84f,
		                (0.16f + pulse * 0.10f) * boost);
	}
}

void Game::RenderSorted(float ox, float oy)
{
	m_Draws.clear();

	const std::vector<WorldObject>& objs = m_World.Objects();
	const float margin = 220.0f;

	// ── 화면에 걸치는 오브젝트만 목록에 올린다 ──
	for (size_t i = 0; i < objs.size(); ++i)
	{
		const WorldObject& o = objs[i];

		float sx = ox + IsoScreenX(o.x, o.y);
		float sy = oy + IsoScreenY(o.x, o.y);

		if (sx < -margin || sx > (float)m_WinW + margin ||
		    sy < -margin || sy > (float)m_WinH + margin)
			continue;

		Drawable d;
		d.depth = IsoDepth(o.x, o.y);
		d.kind  = 0;
		d.index = (int)i;

		m_Draws.push_back(d);
	}

	for (size_t i = 0; i < m_Npcs.size(); ++i)
	{
		Drawable d;
		d.depth = IsoDepth(m_Npcs[i].c.x, m_Npcs[i].c.y);
		d.kind  = 1;
		d.index = (int)i;

		m_Draws.push_back(d);
	}

	for (size_t i = 0; i < m_Beasts.size(); ++i)
	{
		Drawable d;
		d.depth = IsoDepth(m_Beasts[i].c.x, m_Beasts[i].c.y);
		d.kind  = 3;
		d.index = (int)i;

		m_Draws.push_back(d);
	}

	{
		Drawable d;
		d.depth = IsoDepth(m_Player.x, m_Player.y);
		d.kind  = 2;
		d.index = 0;

		m_Draws.push_back(d);
	}

	// painter's algorithm — 깊이가 작은 것(뒤)부터 그린다.
	std::sort(m_Draws.begin(), m_Draws.end(), DrawableLess);

	bool highlight = (m_Stage == QS_GO_TO_SHORE);

	for (size_t i = 0; i < m_Draws.size(); ++i)
	{
		const Drawable& d = m_Draws[i];

		if (d.kind == 0)
		{
			const WorldObject& o = objs[d.index];

			float sx  = ox + IsoScreenX(o.x, o.y);
			float sy  = oy + IsoScreenY(o.x, o.y);
			float lit = Draw::LightAt(o.x, o.y, m_Player.x, m_Player.y);

			Draw::Object(*m_R, o, sx, sy, lit, m_Time, m_HasCoin, highlight);
		}
		else if (d.kind == 1)
		{
			const Npc& n = m_Npcs[d.index];

			float sx  = ox + IsoScreenX(n.c.x, n.c.y);
			float sy  = oy + IsoScreenY(n.c.x, n.c.y);
			float lit = Draw::LightAt(n.c.x, n.c.y, m_Player.x, m_Player.y);

			Draw::Person(*m_R, sx, sy, lit, m_Time, LOOKS[n.look],
			             n.c.facing, n.c.walkPhase, n.c.walkAmount, n.c.bobPhase, false);
		}
		else if (d.kind == 3)
		{
			const Beast& b = m_Beasts[d.index];

			float sx  = ox + IsoScreenX(b.c.x, b.c.y);
			float sy  = oy + IsoScreenY(b.c.x, b.c.y);
			float lit = Draw::LightAt(b.c.x, b.c.y, m_Player.x, m_Player.y);

			Draw::Beast(*m_R, sx, sy, lit, m_Time, b.kind,
			            b.c.facing, b.c.walkPhase, b.c.walkAmount, b.alert);
		}
		else
		{
			float sx = ox + IsoScreenX(m_Player.x, m_Player.y);
			float sy = oy + IsoScreenY(m_Player.x, m_Player.y);

			Draw::Person(*m_R, sx, sy, 1.0f, m_Time, LOOKS[14],
			             m_Player.facing, m_Player.walkPhase, m_Player.walkAmount,
			             m_Player.bobPhase, true);
		}
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
	                L"WASD 이동     E 상호작용     J 일지     Esc 종료",
	                0.46f, 0.45f, 0.42f, 1.0f, FONT_SMALL);

	// ── 상호작용 프롬프트 ──
	if (!m_DialogueOpen && (m_FocusNpc >= 0 || m_FocusSigil))
	{
		if (m_FocusNpc >= 0)
			swprintf(buf, 192, L"[E]  %s 에게 말 걸기", m_Npcs[m_FocusNpc].name);
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
