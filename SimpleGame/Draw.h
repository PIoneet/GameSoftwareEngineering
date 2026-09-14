#pragma once

#include "World.h"

class Renderer;

// 캐릭터 한 명의 외형. 색은 파츠별로 나눠 둔다.
struct PersonLook
{
	float cloak[3];
	float skin[3];
	float hair[3];
	float scale;
};

enum BeastKind
{
	BEAST_WOLF,
	BEAST_DEER,
	BEAST_CROW
};

// 그리기 전담 계층. 게임 로직(Game)과 분리해 두면 연출을 손볼 때
// 상태 관리 코드를 건드리지 않아도 된다.
namespace Draw
{
	// 플레이어 주변만 밝은 "랜턴 하나 든 밤길" 감광
	float LightAt(float wx, float wy, float px, float py);

	// 가장자리가 부드러운 그림자. 겹을 나눠 그려 단단한 타원 티를 없앤다.
	void SoftShadow(Renderer& r, float sx, float sy, float w, float h, float strength);

	// 바닥에 깔리는 빛 웅덩이
	void LightPool(Renderer& r, float sx, float sy, float w,
	               float cr, float cg, float cb, float intensity);

	// 지형 + 잔디/자갈/물결 같은 자잘한 디테일
	void Tiles(Renderer& r, const World& world, float ox, float oy,
	           int winW, int winH, float time, float px, float py);

	// 지형 위에 서는 것들
	void Object(Renderer& r, const WorldObject& o, float sx, float sy,
	            float lit, float time, bool sigilTaken, bool sigilHighlight);

	void Person(Renderer& r, float sx, float sy, float lit, float time,
	            const PersonLook& look, int facing,
	            float walkPhase, float walkAmount, float idleSeed, bool lantern);

	void Beast(Renderer& r, float sx, float sy, float lit, float time,
	           int kind, int facing, float walkPhase, float walkAmount, float alert);
}
