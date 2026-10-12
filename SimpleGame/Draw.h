#pragma once

#include "World.h"
#include "Model.h"

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

// 모델 하나를 화면에 올릴 때 필요한 인스턴스 정보.
// 모델 자체는 색도 움직임도 갖고 있지 않다 — 전부 여기서 채워 넣는다.
struct ModelInstance
{
	float sx, sy;        // 화면 좌표 — 밑동 중앙
	float w, h;          // 그릴 크기 (픽셀)
	float lit;           // 주변 밝기
	float tint;          // 개체별 색 변주
	float alpha;
	int   facing;        // -1 이면 좌우를 뒤집는다

	const float* cloak;  // 색 슬롯. 쓰지 않으면 0.
	const float* skin;
	const float* hair;
	const float* body;

	float anim[ANIM_SLOT_COUNT];
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

	// 인스턴스 기본값을 채운다. 호출한 쪽이 필요한 항목만 덮어쓰면 된다.
	void InitInstance(ModelInstance& inst, float sx, float sy, float w, float h, float lit);

	// 애니메이션 슬롯 값을 채운다.
	void SetPropAnim(ModelInstance& inst, float time, float seed);
	void SetCreatureAnim(ModelInstance& inst, float time, float seed,
	                     float walkPhase, float walkAmount);

	// 모델 하나를 그린다.
	void Model(Renderer& r, const ModelLibrary& lib, int id, const ModelInstance& inst);

	// 지형 + 잔디/자갈/물결 같은 자잘한 디테일
	void Tiles(Renderer& r, const World& world, float ox, float oy,
	           int winW, int winH, float time, float px, float py);

	// 지형 위에 서는 것들
	void Object(Renderer& r, const ModelLibrary& lib, const WorldObject& o,
	            float sx, float sy, float lit, float time,
	            bool sigilTaken, bool sigilHighlight);

	void Person(Renderer& r, const ModelLibrary& lib, float sx, float sy,
	            float lit, float time, const PersonLook& look, int facing,
	            float walkPhase, float walkAmount, float idleSeed, bool lantern);

	void Beast(Renderer& r, const ModelLibrary& lib, float sx, float sy,
	           float lit, float time, int kind, int facing,
	           float walkPhase, float walkAmount, float alert);

	// 1레벨 몬스터. hurt 는 피격 섬광(0~1).
	void Monster(Renderer& r, const ModelLibrary& lib, float sx, float sy,
	             float lit, float time, int kind, int facing,
	             float walkPhase, float walkAmount, float hurt);

	// 바닥에 떨어진 습득물
	void Pickup(Renderer& r, const ModelLibrary& lib, float sx, float sy,
	            float lit, float time, int kind, float bob);

	// 플레이어가 든 검. progress 가 0~1 이면 휘두르는 중, 음수면 내린 자세.
	// scale 은 PersonLook::scale — 몸과 같은 비율로 커지고 작아진다.
	void Sword(Renderer& r, float sx, float sy, float scale, int facing,
	           float progress, float lit);
}
