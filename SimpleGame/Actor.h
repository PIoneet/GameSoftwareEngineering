#pragma once

#include <vector>

class Renderer;
class ModelLibrary;
class World;
class Scene;
class PlayerActor;

struct PlayerStats;

// 액터를 그릴 때 필요한 주변 정보. 매 프레임 Scene 이 채워 넘긴다.
// 액터가 렌더러나 카메라를 직접 들고 있지 않게 하려는 장치다.
struct RenderContext
{
	Renderer*           r;
	const ModelLibrary* models;

	float originX;      // 월드 → 화면 변환의 기준점 (카메라)
	float originY;
	float time;

	float viewX;        // 조명 기준. 보통 플레이어 위치.
	float viewY;

	int winW;
	int winH;
};

// 액터를 갱신할 때 필요한 주변 정보.
//
// 액터가 게임 전역 상태를 직접 붙들지 않도록, 필요한 것만 여기에 실어 넘긴다.
struct UpdateContext
{
	const World* world;
	Scene*       scene;      // 범위 질의 — 공격 판정, 드롭 생성 등

	PlayerStats* stats;      // 소유는 Game. 액터는 읽고 쓰기만 한다.
	PlayerActor* player;

	float time;
	float playerX;           // 대부분의 AI 가 이것만 본다
	float playerY;

	bool  paused;            // 대화창이 떠 있으면 true
};

enum ActorKind
{
	ACTOR_GENERIC,
	ACTOR_PROP,
	ACTOR_PLAYER,
	ACTOR_NPC,
	ACTOR_BEAST,
	ACTOR_MONSTER,
	ACTOR_PICKUP
};

// 화면에 놓이는 모든 것의 공통 조상.
//
// 좌표는 부모 기준의 로컬 값이고, 월드 좌표는 Scene 이 부모에서 자식 방향으로
// 한 번에 누적해 캐시한다. 조회할 때마다 부모를 거슬러 올라가지 않기 위해서다.
class Actor
{
public:
	Actor();
	virtual ~Actor();

	virtual int Kind() const { return ACTOR_GENERIC; }

	// ── 계층 ──
	void Attach(Actor* parent);
	void Detach();

	Actor*                     Parent()   const { return m_Parent; }
	const std::vector<Actor*>& Children() const { return m_Children; }

	// ── 변환 ──
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;        // 높이. 화면 Y 를 위로 밀 뿐 깊이에는 관여하지 않는다.
	int   facing = 1;

	float WorldX() const { return m_WorldX; }
	float WorldY() const { return m_WorldY; }
	float WorldZ() const { return m_WorldZ; }

	void PropagateTransform(float px, float py, float pz);

	// 깊이는 x + y 만으로 결정한다 — 프로젝트 전체의 규약이다.
	float Depth() const { return m_WorldX + m_WorldY; }

	// ── 상태 ──
	bool visible = true;
	bool alive   = true;

	// 화면 밖 판정에 쓰는 여유 반경(픽셀). 큰 건물은 늘려 잡는다.
	float cullPad = 96.0f;

	// 정적 액터의 서브트리는 Update 순회에서 통째로 건너뛴다.
	bool IsStatic() const { return m_Static; }

	virtual void Update(float dt, const UpdateContext& ctx);

	// 이름이 Draw 가 아닌 이유: 그리기 네임스페이스도 Draw 라서,
	// 멤버 함수 안에서 Draw::Object 를 부를 수 없게 된다.
	virtual void Render(const RenderContext& ctx) const;

protected:
	bool m_Static = false;

private:
	Actor*              m_Parent = 0;
	std::vector<Actor*> m_Children;

	float m_WorldX = 0.0f;
	float m_WorldY = 0.0f;
	float m_WorldZ = 0.0f;
};
