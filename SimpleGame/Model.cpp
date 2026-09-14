#include "stdafx.h"
#include "Model.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <iostream>

// 캐시 파일 형식이 바뀌면 이 번호를 올린다. 옛 캐시는 무시되고 다시 구워진다.
static const int   CACHE_VERSION = 1;
static const char* CACHE_MAGIC   = "RVMD";

namespace
{
	// 파츠를 "모델 기준 픽셀"로 적어 넣고, 마지막에 비율로 정규화한다.
	// 이렇게 해야 기존 드로잉 코드를 그대로 옮겨 적을 수 있다.
	struct Builder
	{
		std::vector<ModelPart>* out;
		float nomW;
		float nomH;

		void Add(int shape, float cx, float cy, float w, float h,
		         float r, float g, float b, float a = 1.0f,
		         int slot = COLOR_OWN, int anim = ANIM_NONE, float amp = 0.0f)
		{
			ModelPart p;

			p.shape = shape;
			p.ox    = cx / nomW;
			p.oy    = cy / nomH;
			p.w     = w  / nomW;
			p.h     = h  / nomH;

			p.r = r;
			p.g = g;
			p.b = b;
			p.a = a;

			p.colorSlot = slot;
			p.anim      = anim;
			p.amp       = amp / nomW;

			out->push_back(p);
		}
	};

	// 벽에 세로 널판 결
	void Planks(Builder& b, float cx, float cy, float w, float h, int count,
	            float r, float g, float bl)
	{
		for (int i = 1; i < count; ++i)
		{
			float x = cx - w * 0.5f + w * ((float)i / (float)count);
			b.Add(SHAPE_QUAD, x, cy, 1.2f, h * 0.88f, r * 0.72f, g * 0.72f, bl * 0.72f, 0.55f);
		}
	}

	// 아이소메트릭 지붕을 기와 줄로 겹친다
	void RoofRows(Builder& b, float cx, float cy, float w, float h, int rows,
	              float r, float g, float bl)
	{
		for (int i = 0; i < rows; ++i)
		{
			float t     = (float)i / (float)rows;
			float scale = 1.0f - t * 0.78f;
			float shade = 0.80f + t * 0.42f;

			b.Add(SHAPE_DIAMOND, cx, cy - h * 0.34f * t, w * scale, h * scale,
			      r * shade, g * shade, bl * shade);
		}
	}

	void BuildHouse(Builder& b, float w, float h, int windowCount, bool steeple)
	{
		float wallH = h * 0.52f;
		float wallC = -wallH * 0.5f;

		float wr = 0.243f, wg = 0.203f, wb = 0.152f;
		float rr = 0.338f, rg = 0.163f, rb = 0.118f;

		// 기단
		b.Add(SHAPE_QUAD, 0.0f, -h * 0.025f, w * 1.04f, h * 0.07f, 0.135f, 0.130f, 0.125f);

		// 벽 — 정면
		b.Add(SHAPE_QUAD, 0.0f, wallC, w, wallH, wr, wg, wb);
		Planks(b, 0.0f, wallC, w, wallH, 7, wr, wg, wb);

		// 측면 — 모서리가 보이도록 한쪽을 어둡게
		b.Add(SHAPE_QUAD, w * 0.42f, -wallH * 0.47f, w * 0.16f, wallH * 0.94f,
		      wr * 0.62f, wg * 0.62f, wb * 0.62f);

		// 처마 그늘
		b.Add(SHAPE_QUAD, 0.0f, -wallH + h * 0.022f, w, h * 0.045f,
		      wr * 0.45f, wg * 0.45f, wb * 0.45f, 0.85f);

		// 지붕
		RoofRows(b, 0.0f, -wallH, w * 1.26f, w * 0.60f, 5, rr, rg, rb);

		// 용마루 하이라이트
		b.Add(SHAPE_DIAMOND, 0.0f, -wallH - w * 0.20f, w * 0.34f, w * 0.16f,
		      rr * 1.45f, rg * 1.45f, rb * 1.45f, 0.7f);

		// 문
		b.Add(SHAPE_QUAD, 0.0f, -h * 0.155f, w * 0.20f, h * 0.31f, 0.075f, 0.058f, 0.048f);
		b.Add(SHAPE_QUAD, 0.0f, -h * 0.31f,  w * 0.22f, h * 0.02f, 0.150f, 0.118f, 0.088f);

		// 창 — 황혼의 마을에서 유일하게 따뜻한 색. 밝기는 슬롯으로 흔든다.
		const float off3[3] = { -0.34f, 0.04f, 0.24f };
		const float off2[2] = { -0.32f, 0.19f };

		float ww = w * 0.13f;
		float wh = wallH * 0.30f;
		float wy = -wallH * 0.72f;

		for (int i = 0; i < windowCount; ++i)
		{
			float x = w * ((windowCount >= 3) ? off3[i] : off2[i]) + ww * 0.5f;

			b.Add(SHAPE_QUAD, x, wy, ww + 3.0f, wh + 3.0f, 0.13f, 0.10f, 0.08f, 0.9f);
			b.Add(SHAPE_QUAD, x, wy, ww, wh, 0.99f, 0.76f, 0.40f, 1.0f,
			      COLOR_OWN, ANIM_FLICKER);
			b.Add(SHAPE_QUAD, x + ww * 0.45f, wy, 1.2f, wh, 0.20f, 0.14f, 0.09f, 0.8f);
		}

		if (steeple)
		{
			b.Add(SHAPE_QUAD, 0.0f, -h * 0.85f, w * 0.18f, h * 0.42f,
			      wr * 0.92f, wg * 0.92f, wb * 0.92f);

			b.Add(SHAPE_DIAMOND, 0.0f, -h * 1.06f, w * 0.30f, w * 0.16f,
			      rr * 1.1f, rg * 1.1f, rb * 1.1f);

			b.Add(SHAPE_QUAD, 0.0f, -h * 1.17f, 3.0f, h * 0.10f, 0.42f, 0.38f, 0.30f);

			// 둥근 창
			b.Add(SHAPE_ELLIPSE, 0.0f, -wallH * 1.02f, w * 0.16f, w * 0.16f,
			      0.95f, 0.72f, 0.38f, 1.0f, COLOR_OWN, ANIM_FLICKER);
		}
		else
		{
			// 굴뚝
			b.Add(SHAPE_QUAD, w * 0.30f, -h * 0.90f, w * 0.12f, h * 0.24f,
			      0.190f, 0.165f, 0.145f);
		}
	}

	void BuildCanopy(Builder& b, float w, float h, bool pine)
	{
		// 줄기 — 아래가 굵다
		b.Add(SHAPE_QUAD, 0.0f, -h * 0.20f, w * 0.17f, h * 0.40f, 0.112f, 0.084f, 0.064f);
		b.Add(SHAPE_QUAD, 0.0f, -h * 0.44f, w * 0.11f, h * 0.16f, 0.112f, 0.084f, 0.064f);

		if (pine)
		{
			// 침엽수 — 좁고 층이 뚜렷하다
			float cr = 0.072f, cg = 0.152f, cb = 0.110f;

			for (int i = 0; i < 4; ++i)
			{
				float t     = (float)i / 3.0f;
				float yy    = -h * (0.36f + t * 0.50f);
				float ww    = w * (1.00f - t * 0.58f);
				float shade = 1.0f + t * 0.42f;

				b.Add(SHAPE_DIAMOND, 0.0f, yy, ww, ww * 0.62f,
				      cr * shade, cg * shade, cb * shade, 1.0f,
				      COLOR_OWN, ANIM_SWAY, 2.6f * (0.3f + t * 0.7f));
			}

			b.Add(SHAPE_DIAMOND, 0.0f, -h * 0.90f, w * 0.22f, w * 0.20f,
			      0.16f, 0.30f, 0.22f, 1.0f, COLOR_OWN, ANIM_SWAY, 2.6f);
		}
		else
		{
			// 활엽수 — 덩어리를 겹쳐 실루엣을 흐트러뜨린다
			float cr = 0.097f, cg = 0.197f, cb = 0.111f;

			b.Add(SHAPE_DIAMOND, 0.0f, -h * 0.42f, w * 1.06f, w * 0.62f,
			      cr, cg, cb, 1.0f, COLOR_OWN, ANIM_SWAY, 1.04f);

			b.Add(SHAPE_DIAMOND, -w * 0.22f, -h * 0.58f, w * 0.70f, w * 0.46f,
			      cr * 1.08f, cg * 1.08f, cb * 1.08f, 1.0f, COLOR_OWN, ANIM_SWAY, 1.56f);

			b.Add(SHAPE_DIAMOND, w * 0.20f, -h * 0.62f, w * 0.66f, w * 0.44f,
			      cr * 1.16f, cg * 1.16f, cb * 1.16f, 1.0f, COLOR_OWN, ANIM_SWAY, 2.08f);

			b.Add(SHAPE_DIAMOND, 0.0f, -h * 0.78f, w * 0.62f, w * 0.40f,
			      cr * 1.30f, cg * 1.30f, cb * 1.30f, 1.0f, COLOR_OWN, ANIM_SWAY, 2.6f);

			// 달빛 받는 면
			b.Add(SHAPE_DIAMOND, w * 0.14f, -h * 0.82f, w * 0.30f, w * 0.20f,
			      cr * 1.55f, cg * 1.55f, cb * 1.55f, 0.75f, COLOR_OWN, ANIM_SWAY, 2.6f);
		}
	}

	// 네 발 짐승의 공통 골격. 늑대와 사슴이 비율만 달리해 공유한다.
	void BuildQuadruped(Builder& b, float bodyW, float bodyH, float legH,
	                    bool antlers, bool glowEyes)
	{
		float backY = -legH;

		// 다리 넷 — 앞뒤가 엇갈려 움직인다
		const float legX[4] = { 0.30f, 0.30f, -0.30f, -0.30f };
		const int   legAnim[4] = { ANIM_LEG_A, ANIM_LEG_B, ANIM_LEG_B, ANIM_LEG_A };

		for (int i = 0; i < 4; ++i)
		{
			float x = bodyW * legX[i] + (float)(i % 2) * 2.2f;

			b.Add(SHAPE_QUAD, x, backY + bodyH * 0.5f + legH * 0.5f, 3.2f, legH,
			      0.72f, 0.72f, 0.72f, 1.0f, COLOR_BODY, legAnim[i], 2.0f);
		}

		// 몸통
		b.Add(SHAPE_DIAMOND, 0.0f, backY, bodyW, bodyH * 1.6f,
		      1.0f, 1.0f, 1.0f, 1.0f, COLOR_BODY);

		b.Add(SHAPE_QUAD, 0.0f, backY, bodyW * 0.72f, bodyH,
		      1.0f, 1.0f, 1.0f, 1.0f, COLOR_BODY);

		// 등에 닿는 빛
		b.Add(SHAPE_ELLIPSE, -2.0f, backY - bodyH * 0.45f, bodyW * 0.62f, bodyH * 0.5f,
		      1.35f, 1.35f, 1.35f, 0.7f, COLOR_BODY);

		// 목과 머리
		float neckUp = antlers ? 11.5f : 5.0f;
		float hx     = bodyW * 0.46f;
		float hy     = backY - neckUp;

		b.Add(SHAPE_QUAD, hx, hy + neckUp * 0.5f, 6.0f, neckUp + 3.0f,
		      0.92f, 0.92f, 0.92f, 1.0f, COLOR_BODY, ANIM_HEAD, 1.5f);

		b.Add(SHAPE_ELLIPSE, hx + 3.0f, hy - 3.0f, 15.0f, 9.0f,
		      1.0f, 1.0f, 1.0f, 1.0f, COLOR_BODY, ANIM_HEAD, 2.0f);

		if (antlers)
		{
			for (int i = 0; i < 2; ++i)
			{
				float ox = hx + 2.0f + (float)i * 4.0f - 2.0f;

				b.Add(SHAPE_QUAD, ox, hy - 7.0f,  1.8f, 10.0f, 0.30f, 0.24f, 0.16f,
				      1.0f, COLOR_OWN, ANIM_HEAD, 2.0f);
				b.Add(SHAPE_QUAD, ox - 1.0f, hy - 12.0f, 4.0f, 1.6f, 0.30f, 0.24f, 0.16f,
				      1.0f, COLOR_OWN, ANIM_HEAD, 2.0f);
				b.Add(SHAPE_QUAD, ox + 2.7f, hy - 16.0f, 3.4f, 1.6f, 0.30f, 0.24f, 0.16f,
				      1.0f, COLOR_OWN, ANIM_HEAD, 2.0f);
			}
		}
		else
		{
			b.Add(SHAPE_QUAD, hx + 2.3f, hy - 3.5f, 2.6f, 5.0f,
			      0.8f, 0.8f, 0.8f, 1.0f, COLOR_BODY, ANIM_HEAD, 2.0f);
			b.Add(SHAPE_QUAD, hx + 6.3f, hy - 3.5f, 2.6f, 5.0f,
			      0.8f, 0.8f, 0.8f, 1.0f, COLOR_BODY, ANIM_HEAD, 2.0f);
		}

		// 눈 — 늑대는 어둠 속에서 빛난다
		if (glowEyes)
			b.Add(SHAPE_QUAD, hx + 7.0f, hy, 2.2f, 2.0f, 0.95f, 0.80f, 0.35f,
			      1.0f, COLOR_OWN, ANIM_HEAD, 2.0f);
		else
			b.Add(SHAPE_QUAD, hx + 7.0f, hy, 2.0f, 2.0f, 0.06f, 0.05f, 0.05f,
			      1.0f, COLOR_OWN, ANIM_HEAD, 2.0f);

		// 꼬리
		b.Add(SHAPE_QUAD, -bodyW * 0.50f, backY - bodyH * 0.4f, 7.0f, 3.0f,
		      0.85f, 0.85f, 0.85f, 1.0f, COLOR_BODY, ANIM_LEG_A, 1.2f);
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  모델 정의
// ─────────────────────────────────────────────────────────────────────────

void ModelLibrary::Build()
{
	for (int i = 0; i < MODEL_COUNT; ++i)
		m_Models[i].clear();

	Builder b;

	// ── 나무 ──
	b.out = &m_Models[MODEL_TREE];  b.nomW = 50.0f; b.nomH = 104.0f;
	BuildCanopy(b, 50.0f, 104.0f, false);

	b.out = &m_Models[MODEL_PINE];  b.nomW = 50.0f; b.nomH = 104.0f;
	BuildCanopy(b, 50.0f, 104.0f, true);

	// ── 덤불 ──
	b.out = &m_Models[MODEL_BUSH];  b.nomW = 38.0f; b.nomH = 30.0f;
	b.Add(SHAPE_DIAMOND, -6.1f, -9.0f,  25.1f, 16.7f, 0.101f, 0.180f, 0.107f,
	      1.0f, COLOR_OWN, ANIM_SWAY, 0.64f);
	b.Add(SHAPE_DIAMOND,  6.8f, -10.2f, 22.8f, 15.2f, 0.113f, 0.202f, 0.120f,
	      1.0f, COLOR_OWN, ANIM_SWAY, 1.12f);
	b.Add(SHAPE_DIAMOND,  0.0f, -15.6f, 19.8f, 12.9f, 0.127f, 0.227f, 0.135f,
	      1.0f, COLOR_OWN, ANIM_SWAY, 1.6f);

	// ── 그루터기 ──
	b.out = &m_Models[MODEL_STUMP];  b.nomW = 34.0f; b.nomH = 22.0f;
	b.Add(SHAPE_QUAD,    0.0f, -7.7f,  19.0f, 15.4f, 0.128f, 0.096f, 0.072f);
	b.Add(SHAPE_DIAMOND, 0.0f, -15.4f, 20.4f, 10.2f, 0.176f, 0.138f, 0.100f);
	b.Add(SHAPE_DIAMOND, 0.0f, -15.4f, 11.6f,  5.8f, 0.146f, 0.112f, 0.082f);

	// ── 바위 ──
	b.out = &m_Models[MODEL_ROCK];  b.nomW = 34.0f; b.nomH = 22.0f;
	b.Add(SHAPE_DIAMOND,  0.0f, -5.3f,  34.0f, 25.3f, 0.186f, 0.192f, 0.204f);
	b.Add(SHAPE_ELLIPSE, -2.7f, -10.1f, 21.1f, 15.4f, 0.252f, 0.258f, 0.272f);
	b.Add(SHAPE_ELLIPSE, -4.8f, -12.3f,  9.5f,  7.0f, 0.320f, 0.326f, 0.342f, 0.85f);

	// ── 건물 ──
	b.out = &m_Models[MODEL_HOUSE];  b.nomW = 120.0f; b.nomH = 105.0f;
	BuildHouse(b, 120.0f, 105.0f, 2, false);

	b.out = &m_Models[MODEL_INN];  b.nomW = 168.0f; b.nomH = 136.0f;
	BuildHouse(b, 168.0f, 136.0f, 3, false);

	b.out = &m_Models[MODEL_CHAPEL];  b.nomW = 140.0f; b.nomH = 158.0f;
	BuildHouse(b, 140.0f, 158.0f, 2, true);

	// ── 울타리 ──
	b.out = &m_Models[MODEL_FENCE];  b.nomW = 30.0f; b.nomH = 26.0f;
	b.Add(SHAPE_QUAD, -10.1f, -11.4f, 3.9f, 22.9f, 0.180f, 0.146f, 0.102f);
	b.Add(SHAPE_QUAD,   9.9f, -11.4f, 3.9f, 22.9f, 0.180f, 0.146f, 0.102f);
	b.Add(SHAPE_QUAD,   0.0f, -16.5f, 24.0f,  3.4f, 0.204f, 0.166f, 0.116f);
	b.Add(SHAPE_QUAD,   0.0f, -10.1f, 24.0f,  2.9f, 0.192f, 0.156f, 0.110f);

	// ── 우물 ──
	b.out = &m_Models[MODEL_WELL];  b.nomW = 54.0f; b.nomH = 50.0f;
	b.Add(SHAPE_QUAD,    0.0f, -11.0f, 36.7f, 22.0f, 0.208f, 0.208f, 0.214f);
	b.Add(SHAPE_QUAD,    0.0f, -15.0f, 36.7f,  1.4f, 0.145f, 0.145f, 0.150f, 0.8f);
	b.Add(SHAPE_QUAD,    0.0f,  -8.0f, 36.7f,  1.4f, 0.145f, 0.145f, 0.150f, 0.8f);
	b.Add(SHAPE_DIAMOND, 0.0f, -22.0f, 37.8f, 18.4f, 0.038f, 0.078f, 0.094f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -22.0f, 23.8f, 10.8f, 0.070f, 0.135f, 0.150f, 0.7f);
	b.Add(SHAPE_QUAD,  -14.0f, -38.0f,  4.3f, 32.0f, 0.172f, 0.138f, 0.098f);
	b.Add(SHAPE_QUAD,   13.9f, -38.0f,  4.3f, 32.0f, 0.172f, 0.138f, 0.098f);
	RoofRows(b, 0.0f, -54.0f, 58.3f, 24.8f, 3, 0.248f, 0.128f, 0.100f);

	// ── 게시판 ──
	b.out = &m_Models[MODEL_BOARD];  b.nomW = 48.0f; b.nomH = 56.0f;
	b.Add(SHAPE_QUAD, -14.4f, -15.7f,  4.3f, 31.4f, 0.148f, 0.118f, 0.084f);
	b.Add(SHAPE_QUAD,  12.2f, -15.7f,  4.3f, 31.4f, 0.148f, 0.118f, 0.084f);
	b.Add(SHAPE_QUAD,   0.0f, -43.1f, 42.2f, 28.0f, 0.228f, 0.178f, 0.118f);
	Planks(b, 0.0f, -43.1f, 42.2f, 28.0f, 4, 0.228f, 0.178f, 0.118f);
	b.Add(SHAPE_QUAD,  -1.9f, -43.7f, 12.5f, 15.7f, 0.740f, 0.700f, 0.560f);
	b.Add(SHAPE_QUAD,  -2.4f, -47.4f,  9.6f,  1.2f, 0.350f, 0.310f, 0.240f, 0.8f);

	// ── 모닥불 ── (불꽃 자체는 셰이더가 그린다)
	b.out = &m_Models[MODEL_CAMPFIRE];  b.nomW = 46.0f; b.nomH = 40.0f;
	for (int i = 0; i < 6; ++i)
	{
		float a  = (float)i * 1.047f;
		float rx = cosf(a) * 46.0f * 0.36f;
		float ry = sinf(a) * 46.0f * 0.18f;

		b.Add(SHAPE_DIAMOND, rx, ry, 10.1f, 5.5f, 0.180f, 0.180f, 0.186f);
	}
	b.Add(SHAPE_QUAD, 0.0f, -3.6f, 23.9f, 4.0f, 0.130f, 0.098f, 0.070f);
	b.Add(SHAPE_QUAD, 0.0f, -6.8f,  9.2f, 6.4f, 0.115f, 0.086f, 0.062f);

	// ── 등불 ──
	b.out = &m_Models[MODEL_LANTERN];  b.nomW = 26.0f; b.nomH = 62.0f;
	b.Add(SHAPE_QUAD, 0.0f, -27.3f, 3.6f, 54.6f, 0.128f, 0.108f, 0.082f);
	b.Add(SHAPE_QUAD, 3.1f, -57.0f, 7.8f,  2.3f, 0.128f, 0.108f, 0.082f);
	b.Add(SHAPE_QUAD, 5.5f, -53.3f, 5.7f, 13.6f, 0.16f, 0.13f, 0.09f);
	b.Add(SHAPE_QUAD, 5.5f, -52.1f, 4.7f, 11.2f, 1.00f, 0.78f, 0.40f,
	      1.0f, COLOR_OWN, ANIM_FLICKER);

	// ── 비석 ──
	b.out = &m_Models[MODEL_SIGIL];  b.nomW = 48.0f; b.nomH = 58.0f;
	b.Add(SHAPE_DIAMOND, 0.0f,   0.0f, 39.4f, 18.2f, 0.150f, 0.156f, 0.166f);
	b.Add(SHAPE_QUAD,    0.0f, -26.7f, 25.9f, 53.4f, 0.170f, 0.180f, 0.196f);
	b.Add(SHAPE_QUAD,    9.1f, -25.5f,  5.3f, 51.0f, 0.118f, 0.126f, 0.140f);
	b.Add(SHAPE_QUAD,    0.0f,  -9.3f, 25.9f,  5.8f, 0.100f, 0.145f, 0.098f, 0.7f);

	// 소용돌이 — 동심원 세 겹
	b.Add(SHAPE_ELLIPSE, 0.0f, -31.3f, 19.2f, 19.2f, 0.18f, 0.80f, 0.74f, 0.30f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -31.3f, 13.0f, 13.0f, 0.24f, 0.88f, 0.80f, 0.48f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -31.3f,  6.2f,  6.2f, 0.62f, 1.00f, 0.94f, 0.74f);

	// ── 갈대 ──
	b.out = &m_Models[MODEL_REED];  b.nomW = 22.0f; b.nomH = 36.0f;
	for (int i = 0; i < 4; ++i)
	{
		float off = ((float)i - 1.5f) * 22.0f * 0.24f;
		float hh  = 36.0f * (0.62f + (float)i * 0.12f);

		b.Add(SHAPE_QUAD, off, -hh * 0.5f, 2.2f, hh, 0.098f, 0.168f, 0.120f,
		      1.0f, COLOR_OWN, ANIM_SWAY, 3.4f * (0.3f + (float)i * 0.18f));
	}

	// ── 상자 ──
	b.out = &m_Models[MODEL_CRATE];  b.nomW = 32.0f; b.nomH = 28.0f;
	b.Add(SHAPE_QUAD,    0.0f, -11.5f, 25.6f, 23.0f, 0.196f, 0.154f, 0.106f);
	Planks(b, 0.0f, -11.5f, 25.6f, 23.0f, 3, 0.196f, 0.154f, 0.106f);
	b.Add(SHAPE_QUAD,    0.0f, -14.6f, 25.6f,  1.6f, 0.130f, 0.102f, 0.070f, 0.9f);
	b.Add(SHAPE_DIAMOND, 0.0f, -23.0f, 27.5f, 12.8f, 0.240f, 0.192f, 0.130f);

	// ── 사람 ──
	{
		b.out = &m_Models[MODEL_PERSON];  b.nomW = 24.0f; b.nomH = 45.0f;

		// 다리와 신발
		b.Add(SHAPE_QUAD, -2.7f, -6.0f, 4.6f, 12.0f, 0.62f, 0.62f, 0.62f,
		      1.0f, COLOR_CLOAK, ANIM_LEG_A, 4.2f);
		b.Add(SHAPE_QUAD, -2.7f, -1.3f, 5.8f,  2.6f, 0.075f, 0.062f, 0.052f,
		      1.0f, COLOR_OWN, ANIM_LEG_A, 4.2f);

		b.Add(SHAPE_QUAD,  3.3f, -6.0f, 4.6f, 12.0f, 0.62f, 0.62f, 0.62f,
		      1.0f, COLOR_CLOAK, ANIM_LEG_B, 4.2f);
		b.Add(SHAPE_QUAD,  3.3f, -1.3f, 5.8f,  2.6f, 0.075f, 0.062f, 0.052f,
		      1.0f, COLOR_OWN, ANIM_LEG_B, 4.2f);

		// 망토
		b.Add(SHAPE_QUAD, 0.0f, -21.4f, 18.4f, 19.2f, 0.72f, 0.72f, 0.72f,
		      0.95f, COLOR_CLOAK, ANIM_ARM_A, 2.0f);

		// 뒤쪽 팔
		b.Add(SHAPE_QUAD, -8.7f, -22.8f, 4.0f, 14.4f, 0.70f, 0.70f, 0.70f,
		      1.0f, COLOR_CLOAK, ANIM_ARM_A, 3.4f);

		// 몸통
		b.Add(SHAPE_QUAD, 0.0f, -22.0f, 17.0f, 20.0f, 1.0f, 1.0f, 1.0f,
		      1.0f, COLOR_CLOAK, ANIM_BREATH, 0.6f);
		b.Add(SHAPE_QUAD, 0.0f, -29.6f, 17.0f,  4.8f, 0.76f, 0.76f, 0.76f,
		      1.0f, COLOR_CLOAK);
		b.Add(SHAPE_QUAD, 0.0f, -15.2f, 17.0f,  2.4f, 0.115f, 0.090f, 0.068f);

		// 앞쪽 팔
		b.Add(SHAPE_QUAD, 8.7f, -22.8f, 4.0f, 14.4f, 0.88f, 0.88f, 0.88f,
		      1.0f, COLOR_CLOAK, ANIM_ARM_B, 3.4f);

		// 머리
		b.Add(SHAPE_QUAD, 0.0f, -38.3f, 13.0f, 12.5f, 1.0f, 1.0f, 1.0f,
		      1.0f, COLOR_SKIN, ANIM_HEAD, 1.3f);
		b.Add(SHAPE_QUAD, 0.0f, -43.0f, 14.6f,  5.5f, 1.0f, 1.0f, 1.0f,
		      1.0f, COLOR_HAIR, ANIM_HEAD, 1.3f);
		b.Add(SHAPE_QUAD, 3.4f, -35.9f,  2.0f,  2.0f, 0.05f, 0.04f, 0.04f,
		      1.0f, COLOR_OWN, ANIM_HEAD, 1.3f);
	}

	// ── 짐승 ──
	b.out = &m_Models[MODEL_WOLF];  b.nomW = 46.0f; b.nomH = 30.0f;
	BuildQuadruped(b, 32.0f, 12.0f, 11.0f, false, true);

	b.out = &m_Models[MODEL_DEER];  b.nomW = 46.0f; b.nomH = 44.0f;
	BuildQuadruped(b, 34.5f, 15.0f, 18.4f, true, false);

	b.out = &m_Models[MODEL_CROW];  b.nomW = 22.0f; b.nomH = 18.0f;
	b.Add(SHAPE_QUAD,   -1.0f,  -2.5f,  1.4f,  5.0f, 0.28f, 0.24f, 0.20f);
	b.Add(SHAPE_QUAD,    1.4f,  -2.5f,  1.4f,  5.0f, 0.28f, 0.24f, 0.20f);
	b.Add(SHAPE_ELLIPSE, 0.0f,  -9.0f, 15.0f,  9.0f, 0.075f, 0.070f, 0.082f);
	b.Add(SHAPE_DIAMOND,-5.0f, -10.0f, 11.0f,  5.0f, 0.050f, 0.048f, 0.060f,
	      1.0f, COLOR_OWN, ANIM_ARM_A, 4.0f);
	b.Add(SHAPE_DIAMOND, 5.0f, -10.0f, 11.0f,  5.0f, 0.050f, 0.048f, 0.060f,
	      1.0f, COLOR_OWN, ANIM_ARM_B, 4.0f);
	b.Add(SHAPE_ELLIPSE, 6.0f, -13.0f,  7.0f,  6.0f, 0.085f, 0.080f, 0.092f);
	b.Add(SHAPE_QUAD,    9.0f, -14.0f,  3.5f,  1.6f, 0.42f, 0.34f, 0.12f);

	// ── 1레벨 몬스터 ──
	b.out = &m_Models[MODEL_SLIME];  b.nomW = 34.0f; b.nomH = 26.0f;
	b.Add(SHAPE_ELLIPSE, 0.0f, -10.0f, 34.0f, 20.0f, 0.180f, 0.420f, 0.280f, 0.85f,
	      COLOR_OWN, ANIM_BREATH, 1.4f);
	b.Add(SHAPE_ELLIPSE,-5.0f, -14.0f, 13.0f,  8.0f, 0.320f, 0.640f, 0.430f, 0.7f);
	b.Add(SHAPE_QUAD,   -4.5f, -11.0f,  2.6f,  3.0f, 0.04f, 0.06f, 0.05f);
	b.Add(SHAPE_QUAD,    4.5f, -11.0f,  2.6f,  3.0f, 0.04f, 0.06f, 0.05f);

	b.out = &m_Models[MODEL_CHOMPER];  b.nomW = 42.0f; b.nomH = 34.0f;
	b.Add(SHAPE_QUAD,   -8.0f,  -5.0f,  4.0f, 10.0f, 0.180f, 0.140f, 0.130f,
	      1.0f, COLOR_OWN, ANIM_LEG_A, 3.0f);
	b.Add(SHAPE_QUAD,    8.0f,  -5.0f,  4.0f, 10.0f, 0.180f, 0.140f, 0.130f,
	      1.0f, COLOR_OWN, ANIM_LEG_B, 3.0f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -16.0f, 40.0f, 22.0f, 0.260f, 0.180f, 0.160f);
	b.Add(SHAPE_ELLIPSE,-4.0f, -21.0f, 22.0f, 10.0f, 0.330f, 0.230f, 0.200f, 0.8f);
	b.Add(SHAPE_DIAMOND, 9.0f, -14.0f, 20.0f, 12.0f, 0.060f, 0.030f, 0.035f,
	      1.0f, COLOR_OWN, ANIM_HEAD, 2.5f);

	for (int i = 0; i < 4; ++i)
	{
		b.Add(SHAPE_QUAD, 4.0f + (float)i * 3.4f, -15.5f, 2.2f, 4.0f,
		      0.92f, 0.90f, 0.84f, 1.0f, COLOR_OWN, ANIM_HEAD, 2.5f);
	}

	b.out = &m_Models[MODEL_SPITTER];  b.nomW = 36.0f; b.nomH = 46.0f;
	b.Add(SHAPE_QUAD,   -6.0f,  -7.0f,  3.2f, 14.0f, 0.150f, 0.170f, 0.190f,
	      1.0f, COLOR_OWN, ANIM_LEG_A, 2.4f);
	b.Add(SHAPE_QUAD,    6.0f,  -7.0f,  3.2f, 14.0f, 0.150f, 0.170f, 0.190f,
	      1.0f, COLOR_OWN, ANIM_LEG_B, 2.4f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -22.0f, 28.0f, 20.0f, 0.190f, 0.300f, 0.280f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -34.0f, 20.0f, 16.0f, 0.230f, 0.360f, 0.330f,
	      1.0f, COLOR_OWN, ANIM_HEAD, 2.0f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -34.0f,  8.0f,  6.0f, 0.500f, 0.960f, 0.880f,
	      0.9f, COLOR_OWN, ANIM_HEAD, 2.0f);

	// ── 1레벨 지형지물 ──
	b.out = &m_Models[MODEL_PILLAR];  b.nomW = 40.0f; b.nomH = 92.0f;
	b.Add(SHAPE_DIAMOND, 0.0f,   0.0f, 40.0f, 18.0f, 0.165f, 0.170f, 0.180f);
	b.Add(SHAPE_QUAD,    0.0f, -34.0f, 24.0f, 68.0f, 0.205f, 0.210f, 0.222f);
	b.Add(SHAPE_QUAD,   -8.0f, -34.0f,  3.0f, 68.0f, 0.160f, 0.164f, 0.174f, 0.8f);
	b.Add(SHAPE_QUAD,    7.0f, -34.0f,  3.0f, 68.0f, 0.160f, 0.164f, 0.174f, 0.8f);
	b.Add(SHAPE_DIAMOND, 0.0f, -70.0f, 30.0f, 14.0f, 0.240f, 0.246f, 0.258f);

	b.out = &m_Models[MODEL_RUBBLE];  b.nomW = 36.0f; b.nomH = 18.0f;
	b.Add(SHAPE_DIAMOND, -7.0f, -3.0f, 18.0f, 10.0f, 0.175f, 0.180f, 0.190f);
	b.Add(SHAPE_DIAMOND,  6.0f, -5.0f, 15.0f,  9.0f, 0.200f, 0.205f, 0.216f);
	b.Add(SHAPE_DIAMOND,  0.0f, -9.0f, 11.0f,  7.0f, 0.230f, 0.236f, 0.248f);

	// ── 습득물 ──
	b.out = &m_Models[MODEL_POTION];  b.nomW = 16.0f; b.nomH = 22.0f;
	b.Add(SHAPE_QUAD,    0.0f, -14.0f,  4.0f,  5.0f, 0.240f, 0.200f, 0.150f);
	b.Add(SHAPE_ELLIPSE, 0.0f,  -7.0f, 14.0f, 14.0f, 0.760f, 0.180f, 0.220f);
	b.Add(SHAPE_ELLIPSE,-3.0f,  -9.0f,  5.0f,  4.0f, 0.980f, 0.640f, 0.640f, 0.75f);

	b.out = &m_Models[MODEL_COIN];  b.nomW = 14.0f; b.nomH = 14.0f;
	b.Add(SHAPE_ELLIPSE, 0.0f, -7.0f, 14.0f, 10.0f, 0.860f, 0.660f, 0.240f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -7.0f,  7.0f,  5.0f, 0.980f, 0.860f, 0.480f);

	b.out = &m_Models[MODEL_ORB];  b.nomW = 14.0f; b.nomH = 14.0f;
	b.Add(SHAPE_ELLIPSE, 0.0f, -7.0f, 16.0f, 16.0f, 0.220f, 0.780f, 0.760f, 0.35f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -7.0f, 10.0f, 10.0f, 0.380f, 0.920f, 0.880f, 0.65f);
	b.Add(SHAPE_ELLIPSE, 0.0f, -7.0f,  5.0f,  5.0f, 0.820f, 1.000f, 0.980f, 0.95f);
}


// ─────────────────────────────────────────────────────────────────────────
//  캐시 입출력
// ─────────────────────────────────────────────────────────────────────────

bool ModelLibrary::Save(const char* path) const
{
	FILE* f = 0;

	if (fopen_s(&f, path, "wb") != 0 || f == 0)
		return false;

	int version  = CACHE_VERSION;
	int partSize = (int)sizeof(ModelPart);
	int count    = MODEL_COUNT;

	fwrite(CACHE_MAGIC, 1, 4, f);
	fwrite(&version,  sizeof(int), 1, f);
	fwrite(&partSize, sizeof(int), 1, f);   // 구조체가 바뀌면 옛 캐시를 버리기 위한 값
	fwrite(&count,    sizeof(int), 1, f);

	for (int i = 0; i < MODEL_COUNT; ++i)
	{
		int n = (int)m_Models[i].size();
		fwrite(&n, sizeof(int), 1, f);

		if (n > 0)
			fwrite(&m_Models[i][0], sizeof(ModelPart), n, f);
	}

	fclose(f);

	return true;
}

bool ModelLibrary::Load(const char* path)
{
	FILE* f = 0;

	if (fopen_s(&f, path, "rb") != 0 || f == 0)
		return false;

	char magic[4] = { 0 };
	int  version  = 0;
	int  partSize = 0;
	int  count    = 0;

	bool ok = fread(magic, 1, 4, f) == 4
	       && fread(&version,  sizeof(int), 1, f) == 1
	       && fread(&partSize, sizeof(int), 1, f) == 1
	       && fread(&count,    sizeof(int), 1, f) == 1;

	if (!ok || memcmp(magic, CACHE_MAGIC, 4) != 0 ||
	    version != CACHE_VERSION || partSize != (int)sizeof(ModelPart) ||
	    count != MODEL_COUNT)
	{
		fclose(f);
		return false;
	}

	for (int i = 0; i < MODEL_COUNT; ++i)
	{
		int n = 0;

		if (fread(&n, sizeof(int), 1, f) != 1 || n < 0 || n > 4096)
		{
			fclose(f);
			return false;
		}

		m_Models[i].resize(n);

		if (n > 0 && fread(&m_Models[i][0], sizeof(ModelPart), n, f) != (size_t)n)
		{
			fclose(f);
			return false;
		}
	}

	fclose(f);

	return true;
}

bool ModelLibrary::LoadOrBuild(const char* path)
{
	if (Load(path))
	{
		std::cout << "Model cache loaded: " << path << "\n";
		return true;
	}

	Build();

	if (Save(path))
		std::cout << "Model cache built and saved: " << path << "\n";
	else
		std::cout << "Model cache could not be saved (will rebuild next run).\n";

	return false;
}

const std::vector<ModelPart>& ModelLibrary::Parts(int id) const
{
	static const std::vector<ModelPart> empty;

	if (id < 0 || id >= MODEL_COUNT)
		return empty;

	return m_Models[id];
}
