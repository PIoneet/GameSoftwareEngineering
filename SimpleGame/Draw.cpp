#include "stdafx.h"
#include "Draw.h"
#include "Renderer.h"
#include "IsoMath.h"

#include <cmath>

namespace
{
	float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

	// 오브젝트 variant 로부터 0..1 변주값을 뽑는다.
	float Var(int variant, int salt)
	{
		return (float)((variant * 37 + salt * 101) % 100) * 0.01f;
	}
}

float Draw::LightAt(float wx, float wy, float px, float py)
{
	float dx = wx - px;
	float dy = wy - py;
	float d  = sqrtf(dx * dx + dy * dy);
	float t  = Clamp01((d - 7.0f) / 24.0f);
	return 1.0f - t * 0.48f;
}

void Draw::SoftShadow(Renderer& r, float sx, float sy, float w, float h, float strength)
{
	// 세 겹으로 나눠 바깥쪽을 옅게 — 단일 마름모보다 훨씬 부드럽다.
	r.DrawDiamond(sx, sy, w * 1.35f, h * 1.35f, 0.0f, 0.0f, 0.0f, strength * 0.16f);
	r.DrawDiamond(sx, sy, w * 1.00f, h * 1.00f, 0.0f, 0.0f, 0.0f, strength * 0.26f);
	r.DrawDiamond(sx, sy, w * 0.66f, h * 0.66f, 0.0f, 0.0f, 0.0f, strength * 0.34f);
}

void Draw::LightPool(Renderer& r, float sx, float sy, float w,
                     float cr, float cg, float cb, float intensity)
{
	r.DrawDiamond(sx, sy, w * 2.10f, w * 1.05f, cr, cg, cb, intensity * 0.30f);
	r.DrawDiamond(sx, sy, w * 1.40f, w * 0.70f, cr, cg, cb, intensity * 0.45f);
	r.DrawDiamond(sx, sy, w * 0.78f, w * 0.39f, cr, cg, cb, intensity * 0.70f);
}

// ─────────────────────────────────────────────────────────────────────────
//  지형
// ─────────────────────────────────────────────────────────────────────────
void Draw::Tiles(Renderer& r, const World& world, float ox, float oy,
                 int winW, int winH, float time, float px, float py)
{
	const float margin = 110.0f;

	for (int ty = 0; ty < MAP_H; ++ty)
	for (int tx = 0; tx < MAP_W; ++tx)
	{
		float wx = (float)tx + 0.5f;
		float wy = (float)ty + 0.5f;

		float sx = ox + IsoScreenX(wx, wy);
		float sy = oy + IsoScreenY(wx, wy);

		if (sx < -margin || sx > (float)winW + margin ||
		    sy < -margin || sy > (float)winH + margin)
			continue;

		TileType t = world.Tile(tx, ty);
		float    v = world.Variation(tx, ty);

		float cr, cg, cb;
		switch (t)
		{
		case TILE_MEADOW:  cr = 0.176f; cg = 0.229f; cb = 0.133f; break;
		case TILE_PATH:    cr = 0.246f; cg = 0.209f; cb = 0.156f; break;
		case TILE_SAND:    cr = 0.274f; cg = 0.245f; cb = 0.183f; break;
		case TILE_SHALLOW: cr = 0.102f; cg = 0.216f; cb = 0.224f; break;
		case TILE_WATER:   cr = 0.050f; cg = 0.124f; cb = 0.150f; break;
		case TILE_GRASS:
		default:           cr = 0.141f; cg = 0.196f; cb = 0.126f; break;
		}

		float shade = 0.88f + v * 0.24f;

		// 수면 일렁임
		if (t == TILE_WATER || t == TILE_SHALLOW)
			shade += sinf(time * 1.3f + (float)tx * 0.62f + (float)ty * 0.48f) * 0.11f;

		float lit = Draw::LightAt(wx, wy, px, py) * shade;

		r.DrawDiamond(sx, sy, TILE_W, TILE_H, cr * lit, cg * lit, cb * lit, 1.0f);

		// ── 디테일 ──
		if (t == TILE_WATER)
		{
			// 흐르는 물결선
			float w = sinf(time * 0.9f + (float)tx * 0.5f + (float)ty * 0.9f);
			if (w > 0.72f)
			{
				float a = (w - 0.72f) * 1.6f;
				r.DrawDiamond(sx, sy - 2.0f, TILE_W * 0.52f, TILE_H * 0.22f,
				              0.42f * lit, 0.72f * lit, 0.76f * lit, a * 0.30f);
			}
		}
		else if (t == TILE_SHALLOW)
		{
			// 물가 거품
			float f = sinf(time * 1.9f + v * 12.0f);
			if (f > 0.55f)
			{
				r.DrawDiamond(sx, sy, TILE_W * 0.66f, TILE_H * 0.30f,
				              0.62f * lit, 0.80f * lit, 0.80f * lit, (f - 0.55f) * 0.55f);
			}
		}
		else if (t == TILE_PATH)
		{
			// 자갈
			if (v > 0.86f)
			{
				r.DrawQuad(sx - 6.0f, sy - 2.0f, 3.0f, 2.0f,
				           0.34f * lit, 0.30f * lit, 0.24f * lit, 0.85f);
				r.DrawQuad(sx + 4.0f, sy + 3.0f, 2.0f, 2.0f,
				           0.32f * lit, 0.28f * lit, 0.22f * lit, 0.85f);
			}
		}
		else if (t == TILE_SAND)
		{
			if (v > 0.88f)
				r.DrawQuad(sx - 3.0f, sy, 2.0f, 2.0f,
				           0.36f * lit, 0.33f * lit, 0.26f * lit, 0.8f);
		}
		else
		{
			// 풀포기 — 바람에 같이 눕는다
			if (v > 0.80f)
			{
				float sway = sinf(time * 1.1f + (float)tx * 0.7f + (float)ty * 0.4f) * 1.6f;
				float gr = 0.118f * lit, gg = 0.192f * lit, gb = 0.110f * lit;

				r.DrawQuad(sx - 7.0f + sway, sy - 7.0f, 1.8f, 7.0f, gr, gg, gb, 0.9f);
				r.DrawQuad(sx - 1.0f + sway * 1.2f, sy - 9.0f, 1.8f, 9.0f, gr, gg, gb, 0.9f);
				r.DrawQuad(sx + 5.0f + sway, sy - 6.0f, 1.8f, 6.0f, gr, gg, gb, 0.9f);
			}
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────
//  오브젝트
// ─────────────────────────────────────────────────────────────────────────
namespace
{
	// 벽에 세로 널판 결을 넣는다.
	void Planks(Renderer& r, float x, float y, float w, float h,
	            float br, float bg, float bb, int count)
	{
		for (int i = 1; i < count; ++i)
		{
			float px = x + w * ((float)i / (float)count);
			r.DrawQuad(px, y + h * 0.06f, 1.2f, h * 0.88f, br * 0.72f, bg * 0.72f, bb * 0.72f, 0.55f);
		}
	}

	// 아이소메트릭 지붕을 기와 줄로 겹쳐 그린다.
	void RoofTiles(Renderer& r, float cx, float cy, float w, float h,
	               float rr, float rg, float rb, int rows)
	{
		for (int i = 0; i < rows; ++i)
		{
			float t     = (float)i / (float)rows;
			float scale = 1.0f - t * 0.78f;
			float shade = 0.80f + t * 0.42f;

			r.DrawDiamond(cx, cy - h * 0.34f * t, w * scale, h * scale,
			              rr * shade, rg * shade, rb * shade, 1.0f);
		}
	}

	void Chimney(Renderer& r, float sx, float sy, float w, float h,
	             float lit, float time, int variant)
	{
		float cx = sx + w * 0.30f;
		float cy = sy - h * 1.02f;

		r.DrawQuad(cx - w * 0.06f, cy, w * 0.12f, h * 0.24f,
		           0.190f * lit, 0.165f * lit, 0.145f * lit);

		// 굴뚝 연기 — 세 덩이가 시차를 두고 올라간다
		for (int i = 0; i < 3; ++i)
		{
			float phase = fmodf(time * 0.26f + (float)i * 0.33f + Var(variant, i) , 1.0f);
			float rise  = phase * 40.0f;
			float a     = (1.0f - phase) * 0.16f;
			float size  = 6.0f + phase * 12.0f;

			r.DrawDiamond(cx + sinf(phase * 3.0f + (float)i) * 5.0f,
			              cy - rise, size, size * 0.6f,
			              0.55f, 0.55f, 0.58f, a);
		}
	}

	void Windows(Renderer& r, float sx, float sy, float w, float wallH,
	             float time, int variant, int count)
	{
		float flick = 0.86f + 0.14f * sinf(time * 3.1f + (float)variant * 0.7f);
		float ww = w * 0.13f;
		float wh = wallH * 0.30f;
		float wy = sy - wallH * 0.72f;

		const float offsets3[3] = { -0.34f, 0.04f, 0.24f };
		const float offsets2[2] = { -0.32f, 0.19f };

		for (int i = 0; i < count; ++i)
		{
			float ox = (count >= 3) ? offsets3[i] : offsets2[i];
			float x  = sx + w * ox;

			// 창틀
			r.DrawQuad(x - 1.5f, wy - 1.5f, ww + 3.0f, wh + 3.0f,
			           0.13f, 0.10f, 0.08f, 0.9f);
			// 불빛
			r.DrawQuad(x, wy, ww, wh, 0.99f * flick, 0.76f * flick, 0.40f * flick);
			// 창살
			r.DrawQuad(x + ww * 0.45f, wy, 1.2f, wh, 0.20f, 0.14f, 0.09f, 0.8f);
		}
	}

	void Building(Renderer& r, const WorldObject& o, float sx, float sy,
	              float lit, float time, int windowCount, bool steeple)
	{
		float w     = o.width;
		float h     = o.height;
		float v     = Var(o.variant, 1);
		float wallH = h * 0.52f;

		Draw::SoftShadow(r, sx, sy, w * 1.05f, w * 0.50f, 1.0f);

		// 기단
		r.DrawQuad(sx - w * 0.52f, sy - h * 0.06f, w * 1.04f, h * 0.07f,
		           0.135f * lit, 0.130f * lit, 0.125f * lit);

		// 벽 — 정면
		float wr = (0.228f + v * 0.030f) * lit;
		float wg = (0.192f + v * 0.022f) * lit;
		float wb = 0.152f * lit;
		r.DrawQuad(sx - w * 0.5f, sy - wallH, w, wallH, wr, wg, wb);
		Planks(r, sx - w * 0.5f, sy - wallH, w, wallH, wr, wg, wb, 7);

		// 측면 — 아이소메트릭에서 모서리가 보이도록 한쪽을 어둡게
		r.DrawQuad(sx + w * 0.34f, sy - wallH * 0.94f, w * 0.16f, wallH * 0.94f,
		           wr * 0.62f, wg * 0.62f, wb * 0.62f);

		// 처마 그늘
		r.DrawQuad(sx - w * 0.5f, sy - wallH, w, h * 0.045f,
		           wr * 0.45f, wg * 0.45f, wb * 0.45f, 0.85f);

		// 지붕
		float rr = (0.300f + v * 0.075f) * lit;
		float rg = (0.148f + v * 0.030f) * lit;
		float rb = 0.118f * lit;
		RoofTiles(r, sx, sy - wallH, w * 1.26f, w * 0.60f, rr, rg, rb, 5);

		// 용마루 하이라이트
		r.DrawDiamond(sx, sy - wallH - w * 0.20f, w * 0.34f, w * 0.16f,
		              rr * 1.45f, rg * 1.45f, rb * 1.45f, 0.7f);

		// 문
		r.DrawQuad(sx - w * 0.10f, sy - h * 0.31f, w * 0.20f, h * 0.31f,
		           0.075f * lit, 0.058f * lit, 0.048f * lit);
		r.DrawQuad(sx - w * 0.11f, sy - h * 0.32f, w * 0.22f, h * 0.02f,
		           0.150f * lit, 0.118f * lit, 0.088f * lit);

		Windows(r, sx, sy, w, wallH, time, o.variant, windowCount);

		if (steeple)
		{
			// 첨탑
			r.DrawQuad(sx - w * 0.09f, sy - h * 1.06f, w * 0.18f, h * 0.42f,
			           wr * 0.92f, wg * 0.92f, wb * 0.92f);
			r.DrawDiamond(sx, sy - h * 1.06f, w * 0.30f, w * 0.16f,
			              rr * 1.1f, rg * 1.1f, rb * 1.1f);
			r.DrawQuad(sx - 1.5f, sy - h * 1.22f, 3.0f, h * 0.10f,
			           0.42f * lit, 0.38f * lit, 0.30f * lit);
			// 둥근 창
			r.DrawDiamond(sx, sy - wallH * 1.02f, w * 0.16f, w * 0.16f,
			              0.95f, 0.72f, 0.38f, 0.9f);
		}
		else
		{
			Chimney(r, sx, sy, w, h, lit, time, o.variant);
		}
	}

	void Canopy(Renderer& r, float sx, float sy, float w, float h,
	            float lit, float time, int variant, bool pine)
	{
		float sway = sinf(time * 0.7f + (float)variant * 0.11f) * 2.6f;
		float v    = Var(variant, 3);

		// 줄기 — 아래가 굵다
		float tr = 0.112f * lit, tg = 0.084f * lit, tb = 0.064f * lit;
		r.DrawQuad(sx - w * 0.085f, sy - h * 0.40f, w * 0.17f, h * 0.40f, tr, tg, tb);
		r.DrawQuad(sx - w * 0.055f, sy - h * 0.52f, w * 0.11f, h * 0.16f, tr, tg, tb);

		if (pine)
		{
			// 침엽수 — 좁고 층이 뚜렷하다
			float cr = (0.062f + v * 0.020f) * lit;
			float cg = (0.132f + v * 0.040f) * lit;
			float cb = (0.098f + v * 0.024f) * lit;

			for (int i = 0; i < 4; ++i)
			{
				float t     = (float)i / 3.0f;
				float yy    = sy - h * (0.36f + t * 0.50f);
				float ww    = w * (1.00f - t * 0.58f);
				float shade = 1.0f + t * 0.42f;

				r.DrawDiamond(sx + sway * (0.3f + t * 0.7f), yy, ww, ww * 0.62f,
				              cr * shade, cg * shade, cb * shade);
			}
			// 꼭대기 하이라이트
			r.DrawDiamond(sx + sway, sy - h * 0.90f, w * 0.22f, w * 0.20f,
			              0.16f * lit, 0.30f * lit, 0.22f * lit);
		}
		else
		{
			// 활엽수 — 덩어리 몇 개를 겹쳐 실루엣을 흐트러뜨린다
			float cr = (0.082f + v * 0.030f) * lit;
			float cg = (0.168f + v * 0.058f) * lit;
			float cb = (0.096f + v * 0.030f) * lit;

			r.DrawDiamond(sx + sway * 0.4f, sy - h * 0.42f, w * 1.06f, w * 0.62f, cr, cg, cb);
			r.DrawDiamond(sx + sway * 0.6f - w * 0.22f, sy - h * 0.58f, w * 0.70f, w * 0.46f,
			              cr * 1.08f, cg * 1.08f, cb * 1.08f);
			r.DrawDiamond(sx + sway * 0.8f + w * 0.20f, sy - h * 0.62f, w * 0.66f, w * 0.44f,
			              cr * 1.16f, cg * 1.16f, cb * 1.16f);
			r.DrawDiamond(sx + sway, sy - h * 0.78f, w * 0.62f, w * 0.40f,
			              cr * 1.30f, cg * 1.30f, cb * 1.30f);
			// 달빛 받는 면
			r.DrawDiamond(sx + sway + w * 0.14f, sy - h * 0.82f, w * 0.30f, w * 0.20f,
			              cr * 1.55f, cg * 1.55f, cb * 1.55f, 0.75f);
		}
	}
}

void Draw::Object(Renderer& r, const WorldObject& o, float sx, float sy,
                  float lit, float time, bool sigilTaken, bool sigilHighlight)
{
	float w = o.width;
	float h = o.height;
	float v = Var(o.variant, 1);

	switch (o.type)
	{
	case OBJ_TREE:
	case OBJ_PINE:
		SoftShadow(r, sx, sy, w * 0.80f, w * 0.38f, 0.95f);
		Canopy(r, sx, sy, w, h, lit, time, o.variant, o.type == OBJ_PINE);
		break;

	case OBJ_BUSH:
	{
		float sway = sinf(time * 1.0f + (float)o.variant * 0.2f) * 1.6f;
		SoftShadow(r, sx, sy, w * 0.8f, w * 0.36f, 0.8f);

		float cr = (0.088f + v * 0.026f) * lit;
		float cg = (0.156f + v * 0.048f) * lit;
		float cb = (0.094f + v * 0.026f) * lit;

		r.DrawDiamond(sx - w * 0.16f + sway, sy - h * 0.30f, w * 0.66f, w * 0.44f, cr, cg, cb);
		r.DrawDiamond(sx + w * 0.18f + sway, sy - h * 0.34f, w * 0.60f, w * 0.40f,
		              cr * 1.12f, cg * 1.12f, cb * 1.12f);
		r.DrawDiamond(sx + sway, sy - h * 0.52f, w * 0.52f, w * 0.34f,
		              cr * 1.26f, cg * 1.26f, cb * 1.26f);
		break;
	}

	case OBJ_STUMP:
		SoftShadow(r, sx, sy, w * 0.8f, w * 0.38f, 0.85f);
		r.DrawQuad(sx - w * 0.28f, sy - h * 0.70f, w * 0.56f, h * 0.70f,
		           0.128f * lit, 0.096f * lit, 0.072f * lit);
		r.DrawDiamond(sx, sy - h * 0.70f, w * 0.60f, w * 0.30f,
		              0.176f * lit, 0.138f * lit, 0.100f * lit);
		r.DrawDiamond(sx, sy - h * 0.70f, w * 0.34f, w * 0.17f,
		              0.146f * lit, 0.112f * lit, 0.082f * lit);
		break;

	case OBJ_ROCK:
		SoftShadow(r, sx, sy, w * 0.9f, w * 0.42f, 0.9f);
		r.DrawDiamond(sx, sy - h * 0.24f, w, h * 1.15f,
		              0.186f * lit, 0.192f * lit, 0.204f * lit);
		r.DrawDiamond(sx - w * 0.08f, sy - h * 0.46f, w * 0.62f, h * 0.70f,
		              0.252f * lit, 0.258f * lit, 0.272f * lit);
		r.DrawDiamond(sx - w * 0.14f, sy - h * 0.56f, w * 0.28f, h * 0.32f,
		              0.320f * lit, 0.326f * lit, 0.342f * lit, 0.85f);
		break;

	case OBJ_HOUSE:  Building(r, o, sx, sy, lit, time, 2, false); break;
	case OBJ_INN:    Building(r, o, sx, sy, lit, time, 3, false); break;
	case OBJ_CHAPEL: Building(r, o, sx, sy, lit, time, 2, true);  break;

	case OBJ_FENCE:
		SoftShadow(r, sx, sy, w * 0.7f, w * 0.28f, 0.6f);
		r.DrawQuad(sx - w * 0.40f, sy - h * 0.88f, w * 0.13f, h * 0.88f,
		           0.180f * lit, 0.146f * lit, 0.102f * lit);
		r.DrawQuad(sx + w * 0.27f, sy - h * 0.88f, w * 0.13f, h * 0.88f,
		           0.180f * lit, 0.146f * lit, 0.102f * lit);
		r.DrawQuad(sx - w * 0.40f, sy - h * 0.70f, w * 0.80f, h * 0.13f,
		           0.204f * lit, 0.166f * lit, 0.116f * lit);
		r.DrawQuad(sx - w * 0.40f, sy - h * 0.42f, w * 0.80f, h * 0.11f,
		           0.192f * lit, 0.156f * lit, 0.110f * lit);
		break;

	case OBJ_WELL:
		SoftShadow(r, sx, sy, w * 1.0f, w * 0.46f, 1.0f);
		r.DrawQuad(sx - w * 0.34f, sy - h * 0.44f, w * 0.68f, h * 0.44f,
		           0.208f * lit, 0.208f * lit, 0.214f * lit);
		// 돌 줄눈
		r.DrawQuad(sx - w * 0.34f, sy - h * 0.30f, w * 0.68f, 1.4f,
		           0.145f * lit, 0.145f * lit, 0.150f * lit, 0.8f);
		r.DrawQuad(sx - w * 0.34f, sy - h * 0.16f, w * 0.68f, 1.4f,
		           0.145f * lit, 0.145f * lit, 0.150f * lit, 0.8f);
		r.DrawDiamond(sx, sy - h * 0.44f, w * 0.70f, w * 0.34f,
		              0.038f * lit, 0.078f * lit, 0.094f * lit);
		r.DrawDiamond(sx, sy - h * 0.44f, w * 0.44f, w * 0.20f,
		              0.070f * lit, 0.135f * lit, 0.150f * lit, 0.7f);
		r.DrawQuad(sx - w * 0.30f, sy - h * 1.08f, w * 0.08f, h * 0.64f,
		           0.172f * lit, 0.138f * lit, 0.098f * lit);
		r.DrawQuad(sx + w * 0.22f, sy - h * 1.08f, w * 0.08f, h * 0.64f,
		           0.172f * lit, 0.138f * lit, 0.098f * lit);
		RoofTiles(r, sx, sy - h * 1.08f, w * 1.08f, w * 0.46f,
		          0.248f * lit, 0.128f * lit, 0.100f * lit, 3);
		break;

	case OBJ_BOARD:
		SoftShadow(r, sx, sy, w * 0.8f, w * 0.34f, 0.8f);
		r.DrawQuad(sx - w * 0.30f, sy - h * 0.56f, w * 0.09f, h * 0.56f,
		           0.148f * lit, 0.118f * lit, 0.084f * lit);
		r.DrawQuad(sx + w * 0.21f, sy - h * 0.56f, w * 0.09f, h * 0.56f,
		           0.148f * lit, 0.118f * lit, 0.084f * lit);
		r.DrawQuad(sx - w * 0.44f, sy - h * 1.02f, w * 0.88f, h * 0.50f,
		           0.228f * lit, 0.178f * lit, 0.118f * lit);
		Planks(r, sx - w * 0.44f, sy - h * 1.02f, w * 0.88f, h * 0.50f,
		       0.228f * lit, 0.178f * lit, 0.118f * lit, 4);
		// 붙어 있는 의뢰서 — 한 장뿐이다
		r.DrawQuad(sx - w * 0.17f, sy - h * 0.92f, w * 0.26f, h * 0.28f,
		           0.74f * lit, 0.70f * lit, 0.56f * lit);
		r.DrawQuad(sx - w * 0.14f, sy - h * 0.86f, w * 0.20f, 1.2f,
		           0.35f * lit, 0.31f * lit, 0.24f * lit, 0.8f);
		break;

	case OBJ_CAMPFIRE:
	{
		SoftShadow(r, sx, sy, w * 0.9f, w * 0.42f, 0.7f);

		// 돌 테두리
		for (int i = 0; i < 6; ++i)
		{
			float a  = (float)i * 1.047f;
			float rx = cosf(a) * w * 0.36f;
			float ry = sinf(a) * w * 0.18f;
			r.DrawDiamond(sx + rx, sy + ry, w * 0.22f, w * 0.12f,
			              0.180f * lit, 0.180f * lit, 0.186f * lit);
		}

		// 장작
		r.DrawQuad(sx - w * 0.26f, sy - h * 0.14f, w * 0.52f, h * 0.10f,
		           0.130f * lit, 0.098f * lit, 0.070f * lit);
		r.DrawQuad(sx - w * 0.10f, sy - h * 0.20f, w * 0.20f, h * 0.16f,
		           0.115f * lit, 0.086f * lit, 0.062f * lit);

		// 불꽃 — 세 갈래가 서로 다른 주기로 흔들린다
		for (int i = 0; i < 3; ++i)
		{
			float ph  = time * (3.4f + (float)i * 0.8f) + (float)i * 2.1f;
			float wob = sinf(ph) * w * 0.10f;
			float hh  = h * (0.44f + 0.16f * sinf(ph * 1.7f)) * (1.0f - (float)i * 0.16f);
			float ww  = w * (0.30f - (float)i * 0.05f);

			r.DrawDiamond(sx + wob, sy - h * 0.16f - hh * 0.5f, ww, hh,
			              1.0f, 0.52f + (float)i * 0.10f, 0.16f, 0.72f);
		}
		// 심지
		r.DrawDiamond(sx, sy - h * 0.30f, w * 0.16f, h * 0.34f, 1.0f, 0.92f, 0.66f, 0.9f);
		break;
	}

	case OBJ_LANTERN:
	{
		float flick = 0.85f + 0.15f * sinf(time * 5.0f + (float)o.variant);
		SoftShadow(r, sx, sy, w * 0.6f, w * 0.26f, 0.6f);

		r.DrawQuad(sx - w * 0.07f, sy - h * 0.88f, w * 0.14f, h * 0.88f,
		           0.128f * lit, 0.108f * lit, 0.082f * lit);
		r.DrawQuad(sx - w * 0.06f, sy - h * 0.92f, w * 0.30f, w * 0.09f,
		           0.128f * lit, 0.108f * lit, 0.082f * lit);
		// 등롱
		r.DrawQuad(sx + w * 0.10f, sy - h * 0.86f, w * 0.22f, h * 0.22f,
		           0.16f, 0.13f, 0.09f);
		r.DrawQuad(sx + w * 0.12f, sy - h * 0.84f, w * 0.18f, h * 0.18f,
		           1.0f * flick, 0.78f * flick, 0.40f * flick);
		break;
	}

	case OBJ_SIGIL:
	{
		float pulse = 0.5f + 0.5f * sinf(time * 1.9f);
		float alive = sigilTaken ? 0.30f : 1.0f;
		float boost = sigilHighlight ? 1.6f : 1.0f;

		SoftShadow(r, sx, sy, w * 0.95f, w * 0.44f, 1.0f);
		r.DrawDiamond(sx, sy, w * 0.82f, w * 0.38f,
		              0.150f * lit, 0.156f * lit, 0.166f * lit);

		// 기울어 박힌 비석
		r.DrawQuad(sx - w * 0.27f, sy - h * 0.92f, w * 0.54f, h * 0.92f,
		           0.170f * lit, 0.180f * lit, 0.196f * lit);
		r.DrawQuad(sx + w * 0.16f, sy - h * 0.88f, w * 0.11f, h * 0.88f,
		           0.118f * lit, 0.126f * lit, 0.140f * lit);
		// 이끼
		r.DrawQuad(sx - w * 0.27f, sy - h * 0.22f, w * 0.54f, h * 0.10f,
		           0.100f * lit, 0.145f * lit, 0.098f * lit, 0.7f);

		// 소용돌이 — 동심원 세 겹, 젖어 있다
		float ax = sx, ay = sy - h * 0.54f;
		r.DrawDiamond(ax, ay, w * 0.40f, w * 0.40f, 0.18f, 0.80f, 0.74f,
		              (0.16f + pulse * 0.14f) * alive * boost);
		r.DrawDiamond(ax, ay, w * 0.27f, w * 0.27f, 0.24f, 0.88f, 0.80f,
		              (0.26f + pulse * 0.22f) * alive * boost);
		r.DrawDiamond(ax, ay, w * 0.13f, w * 0.13f, 0.62f, 1.00f, 0.94f,
		              (0.40f + pulse * 0.34f) * alive * boost);
		break;
	}

	case OBJ_REED:
	{
		float sway = sinf(time * 1.25f + (float)o.variant * 0.2f) * 3.4f;
		for (int i = 0; i < 4; ++i)
		{
			float off = ((float)i - 1.5f) * w * 0.24f;
			float hh  = h * (0.62f + Var(o.variant, i) * 0.42f);
			r.DrawQuad(sx + off + sway * (0.3f + (float)i * 0.18f), sy - hh,
			           w * 0.10f, hh,
			           0.098f * lit, 0.168f * lit, 0.120f * lit);
		}
		break;
	}

	case OBJ_CRATE:
		SoftShadow(r, sx, sy, w * 0.85f, w * 0.40f, 0.9f);
		r.DrawQuad(sx - w * 0.40f, sy - h * 0.82f, w * 0.80f, h * 0.82f,
		           0.196f * lit, 0.154f * lit, 0.106f * lit);
		Planks(r, sx - w * 0.40f, sy - h * 0.82f, w * 0.80f, h * 0.82f,
		       0.196f * lit, 0.154f * lit, 0.106f * lit, 3);
		r.DrawQuad(sx - w * 0.40f, sy - h * 0.52f, w * 0.80f, 1.6f,
		           0.130f * lit, 0.102f * lit, 0.070f * lit, 0.9f);
		r.DrawDiamond(sx, sy - h * 0.82f, w * 0.86f, w * 0.40f,
		              0.240f * lit, 0.192f * lit, 0.130f * lit);
		break;

	default:
		break;
	}
}

// ─────────────────────────────────────────────────────────────────────────
//  캐릭터 — 파츠를 나눠 절차적으로 애니메이션한다.
//  실제 스프라이트 시트가 없으므로, 걷기 사이클(다리 스윙 / 팔 반대 위상 /
//  상하 bob / 진행 방향 기울기)을 코드로 만든다.
// ─────────────────────────────────────────────────────────────────────────
void Draw::Person(Renderer& r, float sx, float sy, float lit, float time,
                  const PersonLook& look, int facing,
                  float walkPhase, float walkAmount, float idleSeed, bool lantern)
{
	float s = look.scale;

	float swing = sinf(walkPhase);
	float lift  = cosf(walkPhase);

	// 걷는 동안엔 상하로 튀고, 서 있을 땐 숨만 쉰다.
	float bob    = walkAmount * fabsf(lift) * 2.2f * s
	             - (1.0f - walkAmount) * sinf(time * 1.5f + idleSeed) * 0.8f * s;
	float lean   = (float)facing * walkAmount * 1.6f;
	float breath = (1.0f - walkAmount) * sinf(time * 1.5f + idleSeed) * 0.6f * s;

	float legH  = 12.0f * s;
	float bodyW = 17.0f * s;
	float bodyH = 20.0f * s;
	float headW = 13.0f * s;
	float headH = 12.5f * s;

	float hipY  = sy - legH + bob;
	float shldY = hipY - bodyH;

	const float* cl = look.cloak;
	const float* sk = look.skin;
	const float* hr = look.hair;

	// 그림자 — 걸을 때 살짝 줄어든다
	SoftShadow(r, sx, sy, 24.0f * s * (1.0f - walkAmount * 0.12f),
	           11.0f * s, 1.0f);

	// ── 다리 ──
	float legSwing = swing * 4.2f * walkAmount;
	for (int i = 0; i < 2; ++i)
	{
		float dir = (i == 0) ? 1.0f : -1.0f;
		float lx  = sx - 5.0f * s + (float)i * 6.0f * s + legSwing * dir;
		float lh  = legH - fabsf(legSwing) * 0.18f;

		r.DrawQuad(lx, hipY, 4.6f * s, lh,
		           cl[0] * lit * 0.62f, cl[1] * lit * 0.62f, cl[2] * lit * 0.62f);
		// 신발
		r.DrawQuad(lx - 0.6f * s, hipY + lh - 2.6f * s, 5.8f * s, 2.6f * s,
		           0.075f * lit, 0.062f * lit, 0.052f * lit);
	}

	// ── 망토 ──
	float capeSway = -swing * 2.0f * walkAmount - (float)facing * 1.2f;
	r.DrawQuad(sx - bodyW * 0.54f + capeSway, shldY + 1.0f * s,
	           bodyW * 1.08f, bodyH * 0.96f + breath,
	           cl[0] * lit * 0.72f, cl[1] * lit * 0.72f, cl[2] * lit * 0.72f, 0.95f);

	// ── 뒤쪽 팔 ──
	float armSwing = -swing * 3.4f * walkAmount;
	r.DrawQuad(sx - bodyW * 0.5f - 2.2f * s - armSwing, shldY + 2.0f * s,
	           4.0f * s, bodyH * 0.72f,
	           cl[0] * lit * 0.70f, cl[1] * lit * 0.70f, cl[2] * lit * 0.70f);

	// ── 몸통 ──
	r.DrawQuad(sx - bodyW * 0.5f + lean, shldY - breath, bodyW, bodyH + breath,
	           cl[0] * lit, cl[1] * lit, cl[2] * lit);
	// 어깨 그늘
	r.DrawQuad(sx - bodyW * 0.5f + lean, shldY - breath, bodyW, bodyH * 0.24f,
	           cl[0] * lit * 0.76f, cl[1] * lit * 0.76f, cl[2] * lit * 0.76f);
	// 허리띠
	r.DrawQuad(sx - bodyW * 0.5f + lean, hipY - bodyH * 0.22f, bodyW, 2.4f * s,
	           0.115f * lit, 0.090f * lit, 0.068f * lit);

	// ── 앞쪽 팔 ──
	r.DrawQuad(sx + bodyW * 0.5f - 1.8f * s + armSwing, shldY + 2.0f * s,
	           4.0f * s, bodyH * 0.72f,
	           cl[0] * lit * 0.88f, cl[1] * lit * 0.88f, cl[2] * lit * 0.88f);

	// ── 머리 ──
	float headX = sx - headW * 0.5f + lean * 1.3f;
	float headY = shldY - headH - breath;

	r.DrawQuad(headX, headY, headW, headH, sk[0] * lit, sk[1] * lit, sk[2] * lit);
	// 머리카락 / 후드
	r.DrawQuad(headX - 0.8f * s, headY - 0.8f * s, headW + 1.6f * s, headH * 0.44f,
	           hr[0] * lit, hr[1] * lit, hr[2] * lit);
	r.DrawQuad(headX + (facing > 0 ? 0.0f : headW * 0.72f), headY,
	           headW * 0.28f, headH * 0.78f,
	           hr[0] * lit * 0.85f, hr[1] * lit * 0.85f, hr[2] * lit * 0.85f);
	// 눈
	r.DrawQuad(headX + headW * (facing > 0 ? 0.60f : 0.22f), headY + headH * 0.50f,
	           2.0f * s, 2.0f * s, 0.05f, 0.04f, 0.04f);

	// ── 랜턴 ──
	if (lantern)
	{
		float lx = sx + (float)facing * (bodyW * 0.62f) + armSwing;
		float ly = shldY + bodyH * 0.58f;

		r.DrawQuad(lx - 1.0f * s, shldY + 2.0f * s, 2.0f * s, bodyH * 0.52f,
		           0.14f * lit, 0.12f * lit, 0.09f * lit);
		r.DrawQuad(lx - 3.2f * s, ly, 6.4f * s, 8.0f * s, 0.16f, 0.13f, 0.09f);
		r.DrawQuad(lx - 2.4f * s, ly + 0.8f * s, 4.8f * s, 6.4f * s, 1.0f, 0.82f, 0.46f);
		r.DrawQuad(lx - 5.0f * s, ly - 1.5f * s, 10.0f * s, 11.0f * s, 1.0f, 0.78f, 0.42f, 0.20f);
	}
}

// ─────────────────────────────────────────────────────────────────────────
//  야생 짐승
// ─────────────────────────────────────────────────────────────────────────
void Draw::Beast(Renderer& r, float sx, float sy, float lit, float time,
                 int kind, int facing, float walkPhase, float walkAmount, float alert)
{
	float f = (float)facing;

	if (kind == BEAST_CROW)
	{
		// 까마귀 — 종종거리다 날개를 턴다
		float hop  = fabsf(sinf(walkPhase)) * 3.0f * walkAmount;
		float flap = sinf(time * 9.0f) * 4.0f * (0.25f + alert * 0.75f);

		SoftShadow(r, sx, sy, 13.0f, 6.0f, 0.8f);

		r.DrawQuad(sx - 1.0f, sy - 5.0f - hop, 1.4f, 5.0f, 0.28f * lit, 0.24f * lit, 0.20f * lit);
		r.DrawQuad(sx + 1.4f, sy - 5.0f - hop, 1.4f, 5.0f, 0.28f * lit, 0.24f * lit, 0.20f * lit);

		r.DrawDiamond(sx, sy - 9.0f - hop, 15.0f, 9.0f, 0.075f * lit, 0.070f * lit, 0.082f * lit);
		// 날개
		r.DrawDiamond(sx - 5.0f, sy - 10.0f - hop - flap, 11.0f, 5.0f,
		              0.050f * lit, 0.048f * lit, 0.060f * lit);
		r.DrawDiamond(sx + 5.0f, sy - 10.0f - hop + flap, 11.0f, 5.0f,
		              0.050f * lit, 0.048f * lit, 0.060f * lit);
		// 머리 + 부리
		r.DrawDiamond(sx + f * 6.0f, sy - 13.0f - hop, 7.0f, 6.0f,
		              0.085f * lit, 0.080f * lit, 0.092f * lit);
		r.DrawQuad(sx + f * 9.0f, sy - 14.0f - hop, 3.5f * f, 1.6f,
		           0.42f * lit, 0.34f * lit, 0.12f * lit);
		return;
	}

	bool  deer   = (kind == BEAST_DEER);
	float s      = deer ? 1.15f : 1.0f;
	float legH   = (deer ? 16.0f : 11.0f) * s;
	float bodyW  = (deer ? 30.0f : 32.0f) * s;
	float bodyH  = (deer ? 13.0f : 12.0f) * s;

	float br, bg, bb;
	if (deer) { br = 0.230f; bg = 0.156f; bb = 0.102f; }
	else      { br = 0.148f; bg = 0.148f; bb = 0.160f; }

	br *= lit; bg *= lit; bb *= lit;

	float swing = sinf(walkPhase) * 4.0f * walkAmount;
	float bob   = fabsf(cosf(walkPhase)) * 1.6f * walkAmount;
	float backY = sy - legH - bob;

	SoftShadow(r, sx, sy, bodyW * 0.85f, bodyW * 0.34f, 1.0f);

	// 다리 넷 — 앞뒤가 엇갈려 움직인다
	for (int i = 0; i < 4; ++i)
	{
		float side  = (i < 2) ? 1.0f : -1.0f;             // 앞다리 / 뒷다리
		float phase = ((i % 2) == 0) ? 1.0f : -1.0f;
		float lx    = sx + f * (i < 2 ? bodyW * 0.30f : -bodyW * 0.30f)
		            + (float)(i % 2) * 2.2f * s + swing * phase * side * 0.5f;

		r.DrawQuad(lx, backY + bodyH * 0.5f, 3.2f * s, legH,
		           br * 0.72f, bg * 0.72f, bb * 0.72f);
	}

	// 몸통
	r.DrawDiamond(sx, backY, bodyW, bodyH * 1.6f, br, bg, bb);
	r.DrawQuad(sx - bodyW * 0.36f, backY - bodyH * 0.5f, bodyW * 0.72f, bodyH,
	           br, bg, bb);
	// 등에 닿는 빛
	r.DrawDiamond(sx - f * 2.0f, backY - bodyH * 0.45f, bodyW * 0.62f, bodyH * 0.5f,
	              br * 1.35f, bg * 1.35f, bb * 1.35f, 0.7f);

	// 목 + 머리 — 경계 상태면 고개를 든다
	float neckUp = (deer ? 10.0f : 5.0f) * s + alert * 7.0f;
	float hx = sx + f * bodyW * 0.46f;
	float hy = backY - neckUp;

	r.DrawQuad(hx - 3.0f * s, hy, 6.0f * s, neckUp + 3.0f * s,
	           br * 0.92f, bg * 0.92f, bb * 0.92f);
	r.DrawDiamond(hx + f * 3.0f, hy - 3.0f * s, 15.0f * s, 9.0f * s, br, bg, bb);

	// 귀 / 뿔
	if (deer)
	{
		for (int i = 0; i < 2; ++i)
		{
			float ox = hx + f * 2.0f + (float)i * 4.0f * s - 2.0f * s;
			r.DrawQuad(ox, hy - 12.0f * s, 1.8f * s, 10.0f * s,
			           0.30f * lit, 0.24f * lit, 0.16f * lit);
			r.DrawQuad(ox - 3.0f * s, hy - 12.0f * s, 4.0f * s, 1.6f * s,
			           0.30f * lit, 0.24f * lit, 0.16f * lit);
			r.DrawQuad(ox + 1.0f * s, hy - 16.0f * s, 3.4f * s, 1.6f * s,
			           0.30f * lit, 0.24f * lit, 0.16f * lit);
		}
	}
	else
	{
		r.DrawQuad(hx + f * 1.0f, hy - 6.0f * s, 2.6f * s, 5.0f * s,
		           br * 0.8f, bg * 0.8f, bb * 0.8f);
		r.DrawQuad(hx + f * 5.0f, hy - 6.0f * s, 2.6f * s, 5.0f * s,
		           br * 0.8f, bg * 0.8f, bb * 0.8f);
	}

	// 눈 — 늑대는 어둠 속에서 빛난다
	if (!deer)
	{
		float glow = 0.55f + alert * 0.45f;
		r.DrawQuad(hx + f * 6.0f, hy - 1.0f * s, 2.2f * s, 2.0f * s,
		           0.95f * glow, 0.80f * glow, 0.35f * glow);
	}
	else
	{
		r.DrawQuad(hx + f * 6.0f, hy - 1.0f * s, 2.0f * s, 2.0f * s, 0.06f, 0.05f, 0.05f);
	}

	// 꼬리
	float tailSway = sinf(time * 2.2f + walkPhase) * 2.5f;
	r.DrawQuad(sx - f * bodyW * 0.50f, backY - bodyH * 0.4f + tailSway,
	           7.0f * s, 3.0f * s, br * 0.85f, bg * 0.85f, bb * 0.85f);
}
