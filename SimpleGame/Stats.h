#pragma once

// 투자 가능한 능력치
enum StatKind
{
	STAT_VITALITY,   // 체력
	STAT_POWER,      // 공격
	STAT_GUARD,      // 방어
	STAT_KIND_COUNT
};

// 레벨업에 필요한 경험치. 완만하게 올라간다.
int XpNeededFor(int level);

struct PlayerStats
{
	int level;
	int xp;              // 현재 레벨 안에서 쌓은 양

	int maxHp;
	int hp;
	int attack;
	int defense;

	int statPoints;                  // 레벨업마다 받는 분배 포인트
	int invested[STAT_KIND_COUNT];   // 능력치별 투자량

	int coins;
	int potions;

	void Reset();

	// 경험치를 더한다. 레벨이 올랐으면 true.
	bool GainXp(int amount);

	// 포인트를 한 점 투자한다. 남은 포인트가 없으면 무시된다.
	bool Invest(int kind);

	// 레벨과 투자 결과를 실제 수치에 반영한다.
	void Recalc(bool healToFull);

	int  XpNeeded() const { return XpNeededFor(level); }
};
