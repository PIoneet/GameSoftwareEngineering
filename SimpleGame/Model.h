#pragma once

#include <vector>

// 파츠 하나가 그려지는 도형
enum ShapeKind
{
	SHAPE_QUAD,
	SHAPE_DIAMOND,
	SHAPE_ELLIPSE
};

// 색을 인스턴스에서 받아오는 슬롯.
// 같은 모델을 색만 바꿔 재사용하기 위한 장치다 — NPC 14명이 모델 하나를 공유한다.
enum ColorSlot
{
	COLOR_OWN,      // 파츠가 들고 있는 색 그대로
	COLOR_CLOAK,
	COLOR_SKIN,
	COLOR_HAIR,
	COLOR_BODY      // 짐승 몸통색
};

// 렌더 시점에 오프셋을 받아오는 애니메이션 슬롯.
// 모델은 "어느 파츠가 어떤 슬롯을 얼마나 따라가는지"만 들고 있고,
// 슬롯의 실제 값은 인스턴스 상태(걷기 위상 등)에서 계산한다.
enum AnimSlot
{
	ANIM_NONE,
	ANIM_SWAY,      // 바람 — 나무, 갈대
	ANIM_BREATH,    // 호흡 — 대기 중 상하
	ANIM_LEG_A,     // 다리 앞/뒤
	ANIM_LEG_B,
	ANIM_ARM_A,     // 팔 — 다리와 반대 위상
	ANIM_ARM_B,
	ANIM_HEAD,      // 머리 기울기
	ANIM_FLICKER,   // 밝기 깜빡임 — 창문, 등불
	ANIM_SLOT_COUNT
};

// 모델 파츠 하나.
// 좌표와 크기는 전부 "모델 기준 크기에 대한 비율"이다. 밑동 중앙이 원점이고
// oy 는 위로 갈수록 음수다. 덕분에 같은 모델을 어떤 크기로도 그릴 수 있다.
struct ModelPart
{
	int   shape;
	float ox, oy;       // 파츠 중심
	float w,  h;
	float r, g, b, a;
	int   colorSlot;
	int   anim;
	float amp;          // 애니메이션 진폭 (폭에 대한 비율)
};

enum ModelId
{
	// 마을 지형지물
	MODEL_TREE,
	MODEL_PINE,
	MODEL_BUSH,
	MODEL_STUMP,
	MODEL_ROCK,
	MODEL_HOUSE,
	MODEL_INN,
	MODEL_CHAPEL,
	MODEL_FENCE,
	MODEL_WELL,
	MODEL_BOARD,
	MODEL_CAMPFIRE,
	MODEL_LANTERN,
	MODEL_SIGIL,
	MODEL_REED,
	MODEL_CRATE,

	// 생물
	MODEL_PERSON,
	MODEL_WOLF,
	MODEL_DEER,
	MODEL_CROW,

	// 1레벨 몬스터
	MODEL_SLIME,
	MODEL_CHOMPER,
	MODEL_SPITTER,

	// 1레벨 지형지물과 습득물
	MODEL_PILLAR,
	MODEL_RUBBLE,
	MODEL_POTION,
	MODEL_COIN,
	MODEL_ORB,

	MODEL_COUNT
};

// 모델을 절차적으로 만들어 두고 바이너리로 캐싱한다.
// 다음 실행부터는 빌드를 건너뛰고 파일만 읽는다.
class ModelLibrary
{
public:
	// 캐시가 있고 형식이 맞으면 읽는다. 아니면 새로 만들어 저장한다.
	// 반환값은 "파일에서 읽었는지" — 진단용이다.
	bool LoadOrBuild(const char* path);

	const std::vector<ModelPart>& Parts(int id) const;

private:
	void Build();
	bool Load(const char* path);
	bool Save(const char* path) const;

	std::vector<ModelPart> m_Models[MODEL_COUNT];
};
