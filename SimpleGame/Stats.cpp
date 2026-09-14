#include "stdafx.h"
#include "Stats.h"

int XpNeededFor(int level)
{
	// 1→2 는 20, 이후 레벨마다 25씩 더 든다.
	// 짧은 레벨 하나 안에서 두세 번은 오르도록 잡은 값이다.
	return 20 + (level - 1) * 25;
}

void PlayerStats::Reset()
{
	level = 1;
	xp    = 0;

	statPoints = 0;

	for (int i = 0; i < STAT_KIND_COUNT; ++i)
		invested[i] = 0;

	coins   = 0;
	potions = 2;

	Recalc(true);
}

bool PlayerStats::GainXp(int amount)
{
	if (amount <= 0)
		return false;

	xp += amount;

	bool leveled = false;

	// 한 번에 여러 레벨이 오를 수 있다.
	while (xp >= XpNeededFor(level))
	{
		xp -= XpNeededFor(level);

		++level;
		statPoints += 2;

		leveled = true;
	}

	if (leveled)
		Recalc(true);

	return leveled;
}

bool PlayerStats::Invest(int kind)
{
	if (statPoints <= 0 || kind < 0 || kind >= STAT_KIND_COUNT)
		return false;

	--statPoints;
	++invested[kind];

	Recalc(false);

	return true;
}

void PlayerStats::Recalc(bool healToFull)
{
	int before = maxHp;

	// 레벨업으로 자동으로 오르는 몫 + 직접 투자한 몫
	maxHp   = 60 + (level - 1) * 8 + invested[STAT_VITALITY] * 12;
	attack  =  8 + (level - 1) * 1 + invested[STAT_POWER]    * 3;
	defense =  2 +                   invested[STAT_GUARD]    * 2;

	if (healToFull)
	{
		hp = maxHp;
	}
	else
	{
		// 체력에 투자했으면 늘어난 만큼은 채워 준다.
		hp += (maxHp - before);

		if (hp > maxHp) hp = maxHp;
		if (hp < 0)     hp = 0;
	}
}
