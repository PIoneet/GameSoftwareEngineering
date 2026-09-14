#pragma once

#include <vector>
#include "World.h"

class Renderer;

// 튜토리얼 의뢰 "호수 보고서" 의 진행 단계
enum QuestStage
{
	QS_NOT_STARTED,
	QS_ASK_VILLAGERS,
	QS_GO_TO_SHORE,
	QS_RETURN,
	QS_DONE
};

struct Character
{
	float x, y;
	float vx, vy;         // 이번 프레임 이동량
	float walkPhase;      // 걷기 사이클 위상
	float walkAmount;     // 0..1 — 걷는 정도. 급격히 튀지 않게 보간한다
	float bobPhase;       // 개체별 대기 애니메이션 위상
	int   facing;         // -1 왼쪽, +1 오른쪽
};

struct Npc
{
	Character      c;
	const wchar_t* name;
	int            dialogueSet;
	int            witnessId;   // -1 이면 의뢰 증인이 아님
	bool           talked;
	int            look;        // 외형 팔레트 인덱스

	float homeX, homeY;
	float targetX, targetY;
	float waitTimer;
	float wander;
};

struct Beast
{
	Character c;
	int   kind;           // BeastKind
	float homeX, homeY;
	float targetX, targetY;
	float waitTimer;
	float alert;          // 0..1 — 플레이어를 의식하는 정도
	float speed;
};

struct Mote
{
	float x, y, z;
	float vx, vy, vz;
	float life, maxLife;
	int   kind;           // 0 = 반딧불, 1 = 호수 쪽 재, 2 = 모닥불 불티
};

struct LightSource
{
	float x, y;
	float r, g, b;
	float radius;
	float intensity;
	bool  flicker;
};

struct Drawable
{
	float depth;
	int   kind;           // 0 = 오브젝트, 1 = NPC, 2 = 플레이어, 3 = 짐승
	int   index;
};

class Game
{
public:
	void Init(Renderer* renderer, int windowW, int windowH);
	void Resize(int windowW, int windowH);
	void Update(float dt);
	void Render();

private:
	// ── 갱신 ──
	void UpdatePlayer(float dt);
	void UpdateNpcs(float dt);
	void UpdateBeasts(float dt);
	void UpdateInteraction();
	void UpdateMotes(float dt);
	void MoveWithCollision(Character& c, float dx, float dy, float radius);
	void StepAnim(Character& c, float dt, bool moving);

	// ── 대화 / 퀘스트 ──
	void StartDialogue(int setIndex, int npcIndex);
	void AdvanceDialogue();
	void OnDialogueFinished(int setIndex);
	int  DialogueSetForNpc(int npcIndex) const;
	int  DialogueSetForSigil() const;
	void Toast(const wchar_t* text);

	// ── 렌더 ──
	void RenderGroundGlow(float ox, float oy);
	void RenderSorted(float ox, float oy);
	void RenderMotes(float ox, float oy);
	void RenderUI();
	void RenderDialogueBox();
	void RenderJournal();
	void RenderTitleCard();
	void RenderEndCard();
	void Panel(float x, float y, float w, float h, float a);

	// 막히지 않은 가까운 자리를 찾는다 (NPC 초기 배치 안전장치)
	bool FindFreeSpot(float& x, float& y, float radius) const;

	Renderer* m_R = 0;
	World     m_World;

	int   m_WinW = 1280;
	int   m_WinH = 720;
	float m_Time = 0.0f;

	Character m_Player;
	std::vector<Npc>         m_Npcs;
	std::vector<Beast>       m_Beasts;
	std::vector<Mote>        m_Motes;
	std::vector<LightSource> m_Lights;
	std::vector<Drawable>    m_Draws;

	float m_CamX = 0.0f;
	float m_CamY = 0.0f;
	bool  m_CamReady = false;

	QuestStage m_Stage = QS_NOT_STARTED;
	int   m_WitnessCount = 0;
	bool  m_HasCoin = false;

	bool  m_DialogueOpen = false;
	int   m_DialogueSet = -1;
	int   m_DialogueLine = 0;
	int   m_TalkingNpc = -1;

	int   m_FocusNpc = -1;
	bool  m_FocusSigil = false;

	bool  m_JournalOpen = false;
	float m_TitleTimer = 5.5f;
	float m_EndTimer = -1.0f;
	float m_ToastTimer = 0.0f;
	const wchar_t* m_ToastText = 0;
	float m_Dread = 0.0f;
};
