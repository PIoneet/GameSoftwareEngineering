#pragma once

#include <vector>

#include "World.h"
#include "Model.h"
#include "Stats.h"
#include "Scene.h"
#include "Actors.h"

class Renderer;

// 캐릭터 충돌 반경 (타일)
const float BODY_RADIUS = 0.30f;

// 튜토리얼 의뢰 "호수 보고서" 의 진행 단계
enum QuestStage
{
	QS_NOT_STARTED,
	QS_ASK_VILLAGERS,
	QS_GO_TO_SHORE,
	QS_RETURN,
	QS_DONE
};

// 분위기 입자. 액터로 만들 만큼 무겁지 않아 한 배열로 굴린다.
struct Mote
{
	float x, y, z;
	float vx, vy, vz;
	float life, maxLife;
	int   kind;           // 0 = 반딧불, 1 = 호수 쪽 재, 2 = 모닥불 불티
};

// 바닥을 밝히는 광원. 그리기 전용이라 액터가 아니다.
struct LightSource
{
	float x, y;
	float r, g, b;
	float radius;
	float intensity;
	bool  flicker;
};

// 게임 전체를 묶는다.
//
// 화면에 놓이는 것은 전부 Scene 안의 액터다. Game 에는 그 위에 얹히는 것들만
// 남는다 — 퀘스트 진행, 대화, 능력치, 카메라, UI.
class Game
{
public:
	void Init(Renderer* renderer, int windowW, int windowH);
	void Resize(int windowW, int windowH);
	void Update(float dt);
	void Render();

private:
	// ── 씬 구성 ──
	void BuildVillageScene();
	void SpawnPropsFromWorld();
	void SpawnVillagers();
	void SpawnVillageMonsters();
	void SpawnWildlife();

	// 거점 앞마당인지 — 몬스터 배치에서 걸러 낸다.
	bool NearOutpost(float x, float y, float radius) const;

	// 모니터로 내보낼 상태를 JSON 으로 말아 넘긴다.
	void PushMonitorSnapshot();
	void BuildLights();

	UpdateContext MakeUpdateContext();
	RenderContext MakeRenderContext(float ox, float oy);

	// ── 갱신 ──
	void UpdateInteraction();
	void UpdateMotes(float dt);
	void CollectPickups();

	// ── 대화 / 퀘스트 ──
	void StartDialogue(int setIndex, NpcActor* npc);
	void AdvanceDialogue();
	void OnDialogueFinished(int setIndex);
	int  DialogueSetForNpc(const NpcActor* npc) const;
	int  DialogueSetForSigil() const;
	void Toast(const wchar_t* text);

	// ── 렌더 ──
	void RenderGroundGlow(float ox, float oy);
	void RenderMotes(float ox, float oy);
	void RenderUI();
	void RenderDialogueBox();
	void RenderJournal();
	void RenderTitleCard();
	void RenderEndCard();
	void Panel(float x, float y, float w, float h, float a);

	// 막히지 않은 가까운 자리를 찾는다 (NPC 초기 배치 안전장치)
	bool FindFreeSpot(float& x, float& y, float radius) const;

	// ── 1레벨 (Level1.cpp) ──
	void EnterRuin(unsigned int seed);
	void ReturnToVillage();
	void SpawnRuinMonsters();
	void UpdateRuinInput();
	void RenderRuinUI();

	Renderer*    m_R = 0;
	World        m_World;

	// 숨은 월드 상태 (STORY.md §13). UI 에 수치로 내보내지 않는다 —
	// 플레이어는 몬스터 수와 사람들의 태도로만 눈치챈다.
	HiddenState  m_Hidden;

	// 스냅샷 주기. 매 프레임 1만 액터를 훑으면 재는 쪽이 더 비싸진다.
	float m_MonitorTimer = 0.0f;
	ModelLibrary m_Models;
	Scene        m_Scene;

	// 자주 들여다봐야 하는 액터는 따로 붙들어 둔다.
	// 소유는 어디까지나 Scene 이다.
	PlayerActor* m_Player = 0;
	PropActor*   m_Sigil  = 0;

	int   m_WinW = 1280;
	int   m_WinH = 720;
	float m_Time = 0.0f;

	std::vector<Mote>        m_Motes;
	std::vector<LightSource> m_Lights;
	std::vector<Actor*>      m_Query;   // 질의 결과 재사용 버퍼

	float m_CamX = 0.0f;
	float m_CamY = 0.0f;
	bool  m_CamReady = false;

	// ── 퀘스트 ──
	QuestStage m_Stage = QS_NOT_STARTED;
	int   m_WitnessCount = 0;
	bool  m_HasCoin = false;

	// ── 대화 ──
	bool      m_DialogueOpen = false;
	int       m_DialogueSet  = -1;
	int       m_DialogueLine = 0;
	NpcActor* m_TalkingNpc   = 0;

	NpcActor* m_FocusNpc   = 0;
	bool      m_FocusSigil = false;

	// ── 연출 ──
	bool  m_JournalOpen = false;
	float m_TitleTimer  = 5.5f;
	float m_EndTimer    = -1.0f;
	float m_ToastTimer  = 0.0f;
	const wchar_t* m_ToastText = 0;
	float m_Dread = 0.0f;

	// ── 1레벨 ──
	LevelKind   m_Level = LEVEL_VILLAGE;
	PlayerStats m_Stats;

	bool  m_StatsOpen    = false;
	float m_LevelUpTimer = 0.0f;
	int   m_Killed       = 0;
	float m_RuinIntro    = 0.0f;
	unsigned int m_RuinSeed = 0;
};
