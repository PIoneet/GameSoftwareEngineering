#include "stdafx.h"
#include "Draw.h"
#include "Renderer.h"
#include "IsoMath.h"
#include "Profiler.h"

#include <cmath>

namespace
{
	float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

	// 오브젝트 variant 로부터 0..1 변주값을 뽑는다.
	float Var(int variant, int salt)
	{
		return (float)((variant * 37 + salt * 101) % 100) * 0.01f;
	}

	int ModelForObject(int type)
	{
		switch (type)
		{
		case OBJ_TREE:     return MODEL_TREE;
		case OBJ_PINE:     return MODEL_PINE;
		case OBJ_BUSH:     return MODEL_BUSH;
		case OBJ_STUMP:    return MODEL_STUMP;
		case OBJ_ROCK:     return MODEL_ROCK;
		case OBJ_HOUSE:    return MODEL_HOUSE;
		case OBJ_INN:      return MODEL_INN;
		case OBJ_CHAPEL:   return MODEL_CHAPEL;
		case OBJ_FENCE:    return MODEL_FENCE;
		case OBJ_WELL:     return MODEL_WELL;
		case OBJ_BOARD:    return MODEL_BOARD;
		case OBJ_CAMPFIRE: return MODEL_CAMPFIRE;
		case OBJ_LANTERN:  return MODEL_LANTERN;
		case OBJ_SIGIL:    return MODEL_SIGIL;
		case OBJ_REED:     return MODEL_REED;
		case OBJ_CRATE:    return MODEL_CRATE;
		case OBJ_PILLAR:   return MODEL_PILLAR;
		case OBJ_RUBBLE:   return MODEL_RUBBLE;
		default:           return -1;
		}
	}

	// 오브젝트마다 그림자의 크기와 진하기가 다르다.
	void ShadowFor(int type, float w, float& outW, float& outH, float& outStrength)
	{
		outW        = w * 0.85f;
		outH        = w * 0.40f;
		outStrength = 0.9f;

		switch (type)
		{
		case OBJ_HOUSE:
		case OBJ_INN:
		case OBJ_CHAPEL:
			outW = w * 1.05f; outH = w * 0.50f; outStrength = 1.0f;
			break;

		case OBJ_TREE:
		case OBJ_PINE:
			outW = w * 0.80f; outH = w * 0.38f; outStrength = 0.95f;
			break;

		case OBJ_FENCE:
		case OBJ_LANTERN:
			outW = w * 0.65f; outH = w * 0.27f; outStrength = 0.6f;
			break;

		case OBJ_REED:
		case OBJ_RUBBLE:
			outStrength = 0.0f;   // 그림자 없음
			break;

		default:
			break;
		}
	}
}

float Draw::LightAt(float wx, float wy, float px, float py)
{
	float dx = wx - px;
	float dy = wy - py;
	float d  = sqrtf(dx * dx + dy * dy);

	float t = Clamp01((d - 7.0f) / 24.0f);

	return 1.0f - t * 0.48f;
}

void Draw::SoftShadow(Renderer& r, float sx, float sy, float w, float h, float strength)
{
	if (strength <= 0.0f)
		return;

	// 세 겹으로 나눠 바깥쪽을 옅게 — 단일 마름모보다 훨씬 부드럽다.
	r.DrawEllipse(sx, sy, w * 1.35f, h * 1.35f, 0.0f, 0.0f, 0.0f, strength * 0.14f);
	r.DrawEllipse(sx, sy, w * 1.00f, h * 1.00f, 0.0f, 0.0f, 0.0f, strength * 0.24f);
	r.DrawEllipse(sx, sy, w * 0.66f, h * 0.66f, 0.0f, 0.0f, 0.0f, strength * 0.32f);
}

void Draw::LightPool(Renderer& r, float sx, float sy, float w,
                     float cr, float cg, float cb, float intensity)
{
	r.DrawEllipse(sx, sy, w * 2.10f, w * 1.05f, cr, cg, cb, intensity * 0.30f);
	r.DrawEllipse(sx, sy, w * 1.40f, w * 0.70f, cr, cg, cb, intensity * 0.45f);
	r.DrawEllipse(sx, sy, w * 0.78f, w * 0.39f, cr, cg, cb, intensity * 0.70f);
}


// ─────────────────────────────────────────────────────────────────────────
//  모델
// ─────────────────────────────────────────────────────────────────────────

void Draw::InitInstance(ModelInstance& inst, float sx, float sy, float w, float h, float lit)
{
	inst.sx     = sx;
	inst.sy     = sy;
	inst.w      = w;
	inst.h      = h;
	inst.lit    = lit;
	inst.tint   = 1.0f;
	inst.alpha  = 1.0f;
	inst.facing = 1;

	inst.cloak = 0;
	inst.skin  = 0;
	inst.hair  = 0;
	inst.body  = 0;

	for (int i = 0; i < ANIM_SLOT_COUNT; ++i)
		inst.anim[i] = 0.0f;
}

void Draw::SetPropAnim(ModelInstance& inst, float time, float seed)
{
	inst.anim[ANIM_SWAY]    = sinf(time * 0.70f + seed);
	inst.anim[ANIM_FLICKER] = sinf(time * 3.10f + seed);
}

void Draw::SetCreatureAnim(ModelInstance& inst, float time, float seed,
                           float walkPhase, float walkAmount)
{
	float swing = sinf(walkPhase) * walkAmount;

	// 걸을 땐 상하로 튀고, 서 있을 땐 숨만 쉰다.
	float breath = (1.0f - walkAmount) * sinf(time * 1.5f + seed) * 0.6f
	             - walkAmount * fabsf(cosf(walkPhase)) * 1.6f;

	inst.anim[ANIM_SWAY]    = sinf(time * 0.70f + seed);
	inst.anim[ANIM_BREATH]  = breath;
	inst.anim[ANIM_LEG_A]   =  swing;
	inst.anim[ANIM_LEG_B]   = -swing;
	inst.anim[ANIM_ARM_A]   = -swing;
	inst.anim[ANIM_ARM_B]   =  swing;
	inst.anim[ANIM_HEAD]    =  swing * 0.35f;
	inst.anim[ANIM_FLICKER] = sinf(time * 3.10f + seed);
}

void Draw::Model(Renderer& r, const ModelLibrary& lib, int id, const ModelInstance& in)
{
	if (id < 0)
		return;

	const std::vector<ModelPart>& parts = lib.Parts(id);

	for (size_t i = 0; i < parts.size(); ++i)
	{
		const ModelPart& p = parts[i];

		float px = in.sx + p.ox * in.w * (float)in.facing;
		float py = in.sy + p.oy * in.h;
		float pw = p.w * in.w;
		float ph = p.h * in.h;

		float bright = in.lit * in.tint;

		// 애니메이션 슬롯 — 축이 슬롯마다 다르다.
		if (p.anim != ANIM_NONE)
		{
			float v = in.anim[p.anim];

			if (p.anim == ANIM_FLICKER)
				bright *= 0.86f + 0.14f * v;
			else if (p.anim == ANIM_BREATH)
				py += v * p.amp * in.w;
			else
				px += v * p.amp * in.w * (float)in.facing;
		}

		// 색 슬롯 — 파츠가 든 색은 슬롯 색에 곱해지는 음영값으로 쓴다.
		float cr = p.r;
		float cg = p.g;
		float cb = p.b;

		const float* slot = 0;

		switch (p.colorSlot)
		{
		case COLOR_CLOAK: slot = in.cloak; break;
		case COLOR_SKIN:  slot = in.skin;  break;
		case COLOR_HAIR:  slot = in.hair;  break;
		case COLOR_BODY:  slot = in.body;  break;
		default:          break;
		}

		if (slot != 0)
		{
			cr = slot[0] * p.r;
			cg = slot[1] * p.g;
			cb = slot[2] * p.b;
		}

		cr *= bright;
		cg *= bright;
		cb *= bright;

		float alpha = p.a * in.alpha;

		if (p.shape == SHAPE_QUAD)
			r.DrawQuad(px - pw * 0.5f, py - ph * 0.5f, pw, ph, cr, cg, cb, alpha);
		else if (p.shape == SHAPE_DIAMOND)
			r.DrawDiamond(px, py, pw, ph, cr, cg, cb, alpha);
		else
			r.DrawEllipse(px, py, pw, ph, cr, cg, cb, alpha);
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  지형
// ─────────────────────────────────────────────────────────────────────────

void Draw::Tiles(Renderer& r, const World& world, float ox, float oy,
                 int winW, int winH, float time, float px, float py)
{
	const float margin = 110.0f;

	// 맵 전체를 도는 비용이 화면 크기와 무관하게 고정이라는 점을 드러낸다.
	Prof::Add(PROF_TILE_SCANNED, MAP_W * MAP_H);

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

		Prof::Add(PROF_TILE_DRAWN);

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
		case TILE_FLOOR:   cr = 0.152f; cg = 0.148f; cb = 0.156f; break;
		case TILE_WALL:    cr = 0.058f; cg = 0.056f; cb = 0.062f; break;
		case TILE_GRASS:
		default:           cr = 0.141f; cg = 0.196f; cb = 0.126f; break;
		}

		float lit = Draw::LightAt(wx, wy, px, py) * (0.88f + v * 0.24f);

		// 물결은 셰이더가 그린다 — CPU 에서 타일마다 sin 을 돌리지 않는다.
		if (t == TILE_WATER || t == TILE_SHALLOW)
		{
			float depth = (t == TILE_WATER) ? 1.0f : 0.0f;

			r.DrawWater(sx, sy, TILE_W, TILE_H, wx, wy, depth, lit, time,
			            cr * lit, cg * lit, cb * lit);
			continue;
		}

		r.DrawDiamond(sx, sy, TILE_W, TILE_H, cr * lit, cg * lit, cb * lit, 1.0f);

		// ── 지면 디테일 ──
		if (t == TILE_PATH)
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
			// 모래 알갱이
			if (v > 0.88f)
			{
				r.DrawQuad(sx - 3.0f, sy, 2.0f, 2.0f,
				           0.36f * lit, 0.33f * lit, 0.26f * lit, 0.8f);
			}
		}
		else if (t == TILE_FLOOR)
		{
			// 깨진 포석
			if (v > 0.78f)
			{
				r.DrawDiamond(sx, sy, TILE_W * 0.42f, TILE_H * 0.42f,
				              0.190f * lit, 0.186f * lit, 0.196f * lit, 0.8f);
			}
			else if (v < 0.16f)
			{
				r.DrawDiamond(sx, sy, TILE_W * 0.60f, TILE_H * 0.60f,
				              0.112f * lit, 0.110f * lit, 0.118f * lit, 0.7f);
			}
		}
		else if (t == TILE_GRASS || t == TILE_MEADOW)
		{
			// 풀포기 — 바람에 같이 눕는다
			if (v > 0.80f)
			{
				float sway = sinf(time * 1.1f + (float)tx * 0.7f + (float)ty * 0.4f) * 1.6f;

				float gr = 0.118f * lit;
				float gg = 0.192f * lit;
				float gb = 0.110f * lit;

				r.DrawQuad(sx - 7.0f + sway,        sy - 7.0f, 1.8f, 7.0f, gr, gg, gb, 0.9f);
				r.DrawQuad(sx - 1.0f + sway * 1.2f, sy - 9.0f, 1.8f, 9.0f, gr, gg, gb, 0.9f);
				r.DrawQuad(sx + 5.0f + sway,        sy - 6.0f, 1.8f, 6.0f, gr, gg, gb, 0.9f);
			}
		}
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  오브젝트
// ─────────────────────────────────────────────────────────────────────────

void Draw::Object(Renderer& r, const ModelLibrary& lib, const WorldObject& o,
                  float sx, float sy, float lit, float time,
                  bool sigilTaken, bool sigilHighlight)
{
	// 벽은 타일 격자에 붙은 지형이라 모델이 아니라 직접 그린다.
	if (o.type == OBJ_WALL)
	{
		float h   = o.height;
		float top = sy - h;

		r.DrawQuad(sx - TILE_W * 0.5f, top, TILE_W, h + TILE_H * 0.5f,
		           0.104f * lit, 0.100f * lit, 0.110f * lit);

		r.DrawQuad(sx, top, TILE_W * 0.5f, h + TILE_H * 0.5f,
		           0.078f * lit, 0.075f * lit, 0.084f * lit);

		r.DrawDiamond(sx, top, TILE_W, TILE_H,
		              0.156f * lit, 0.152f * lit, 0.164f * lit);

		return;
	}

	int id = ModelForObject(o.type);

	if (id < 0)
		return;

	float shW, shH, shS;
	ShadowFor(o.type, o.width, shW, shH, shS);
	SoftShadow(r, sx, sy, shW, shH, shS);

	ModelInstance inst;
	InitInstance(inst, sx, sy, o.width, o.height, lit);
	SetPropAnim(inst, time, (float)o.variant * 0.11f);

	// 개체마다 색을 조금씩 흔들어 복제 티를 없앤다.
	inst.tint = 0.92f + Var(o.variant, 1) * 0.16f;

	Model(r, lib, id, inst);

	// ── 모델로 표현하지 않는 동적 요소 ──
	if (o.type == OBJ_CAMPFIRE)
	{
		// 불꽃은 셰이더가 그린다.
		r.DrawFlame(sx, sy - o.height * 0.14f, o.width * 0.80f, o.height * 1.25f,
		            time, (float)o.variant * 0.37f, 1.0f);
	}
	else if (o.type == OBJ_SIGIL)
	{
		float pulse = 0.5f + 0.5f * sinf(time * 1.9f);
		float alive = sigilTaken ? 0.30f : 1.0f;
		float boost = sigilHighlight ? 1.6f : 1.0f;

		r.DrawEllipse(sx, sy - o.height * 0.54f, o.width * 0.46f, o.width * 0.46f,
		              0.30f, 0.92f, 0.86f, (0.10f + pulse * 0.18f) * alive * boost);
	}
}


// ─────────────────────────────────────────────────────────────────────────
//  생물
// ─────────────────────────────────────────────────────────────────────────

void Draw::Person(Renderer& r, const ModelLibrary& lib, float sx, float sy,
                  float lit, float time, const PersonLook& look, int facing,
                  float walkPhase, float walkAmount, float idleSeed, bool lantern)
{
	float s = look.scale;
	float w = 24.0f * s;
	float h = 45.0f * s;

	SoftShadow(r, sx, sy, 24.0f * s * (1.0f - walkAmount * 0.12f), 11.0f * s, 1.0f);

	ModelInstance inst;
	InitInstance(inst, sx, sy, w, h, lit);
	SetCreatureAnim(inst, time, idleSeed, walkPhase, walkAmount);

	inst.facing = facing;
	inst.cloak  = look.cloak;
	inst.skin   = look.skin;
	inst.hair   = look.hair;

	Model(r, lib, MODEL_PERSON, inst);

	if (!lantern)
		return;

	// 손에 든 랜턴 — 팔을 따라 흔들리므로 모델이 아니라 여기서 붙인다.
	float armSwing = -sinf(walkPhase) * walkAmount * 3.4f;
	float lx = sx + (float)facing * (17.0f * s * 0.62f) + armSwing;
	float ly = sy - 45.0f * s * 0.28f;

	r.DrawQuad(lx - 1.0f * s, sy - h * 0.68f, 2.0f * s, h * 0.24f,
	           0.14f * lit, 0.12f * lit, 0.09f * lit);

	r.DrawQuad(lx - 3.2f * s, ly, 6.4f * s, 8.0f * s, 0.16f, 0.13f, 0.09f);
	r.DrawQuad(lx - 2.4f * s, ly + 0.8f * s, 4.8f * s, 6.4f * s, 1.0f, 0.82f, 0.46f);

	r.DrawEllipse(lx, ly + 4.0f * s, 14.0f * s, 14.0f * s, 1.0f, 0.78f, 0.42f, 0.22f);
}

void Draw::Beast(Renderer& r, const ModelLibrary& lib, float sx, float sy,
                 float lit, float time, int kind, int facing,
                 float walkPhase, float walkAmount, float alert)
{
	static const float WOLF_BODY[3] = { 0.148f, 0.148f, 0.160f };
	static const float DEER_BODY[3] = { 0.230f, 0.156f, 0.102f };

	int   id = MODEL_CROW;
	float w  = 22.0f;
	float h  = 18.0f;

	const float* body = 0;

	if (kind == BEAST_WOLF)
	{
		id = MODEL_WOLF;  w = 46.0f; h = 30.0f; body = WOLF_BODY;
	}
	else if (kind == BEAST_DEER)
	{
		id = MODEL_DEER;  w = 46.0f; h = 44.0f; body = DEER_BODY;
	}

	SoftShadow(r, sx, sy, w * 0.70f, w * 0.28f, 0.95f);

	ModelInstance inst;
	InitInstance(inst, sx, sy, w, h, lit);
	SetCreatureAnim(inst, time, (float)kind * 1.7f, walkPhase, walkAmount);

	inst.facing = facing;
	inst.body   = body;

	// 경계하면 고개를 든다.
	inst.anim[ANIM_HEAD] -= alert * 1.2f;

	Model(r, lib, id, inst);
}

void Draw::Monster(Renderer& r, const ModelLibrary& lib, float sx, float sy,
                   float lit, float time, int kind, int facing,
                   float walkPhase, float walkAmount, float hurt)
{
	int   id = MODEL_SLIME;
	float w  = 34.0f;
	float h  = 26.0f;

	if (kind == 1)      { id = MODEL_CHOMPER; w = 42.0f; h = 34.0f; }
	else if (kind == 2) { id = MODEL_SPITTER; w = 36.0f; h = 46.0f; }

	SoftShadow(r, sx, sy, w * 0.72f, w * 0.30f, 0.95f);

	ModelInstance inst;
	InitInstance(inst, sx, sy, w, h, lit);
	SetCreatureAnim(inst, time, (float)kind * 2.3f, walkPhase, walkAmount);

	inst.facing = facing;

	// 맞으면 하얗게 튄다.
	inst.tint = 1.0f + hurt * 2.4f;

	Model(r, lib, id, inst);
}

void Draw::Pickup(Renderer& r, const ModelLibrary& lib, float sx, float sy,
                  float lit, float time, int kind, float bob)
{
	int   id = MODEL_ORB;
	float w  = 14.0f;
	float h  = 14.0f;

	if (kind == 1)      { id = MODEL_COIN;   w = 14.0f; h = 14.0f; }
	else if (kind == 2) { id = MODEL_POTION; w = 16.0f; h = 22.0f; }

	SoftShadow(r, sx, sy, 12.0f, 5.0f, 0.6f);

	// 경험치 구슬은 스스로 빛난다.
	if (kind == 0)
	{
		float pulse = 0.6f + 0.4f * sinf(time * 3.4f + bob);
		LightPool(r, sx, sy, 42.0f, 0.28f, 0.90f, 0.86f, 0.16f * pulse);
	}

	ModelInstance inst;
	InitInstance(inst, sx, sy - 6.0f - sinf(time * 2.6f + bob) * 3.0f, w, h, lit);

	Model(r, lib, id, inst);
}

void Draw::AttackArc(Renderer& r, float sx, float sy, int facing, float progress)
{
	// 앞쪽으로 훑고 지나가는 반달. progress 는 0 에서 1.
	float reach = 26.0f + progress * 28.0f;
	float fade  = (1.0f - progress);

	float cx = sx + (float)facing * reach * 0.55f;
	float cy = sy - 22.0f;

	r.DrawEllipse(cx, cy, reach * 1.5f, reach * 0.85f,
	              0.95f, 0.88f, 0.62f, 0.16f * fade);

	r.DrawEllipse(cx, cy, reach * 1.0f, reach * 0.55f,
	              1.00f, 0.94f, 0.74f, 0.22f * fade);
}
