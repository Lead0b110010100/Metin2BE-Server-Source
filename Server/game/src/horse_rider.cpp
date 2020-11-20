#include "stdafx.h"
#include "constants.h"
#include "utils.h"
#include "horse_rider.h"
#include "config.h"
#include "char_manager.h"

const int HORSE_HEALTH_DROP_INTERVAL = 3 * 24 * 60 * 60;
const int HORSE_STAMINA_CONSUME_INTERVAL = 6 * 60;
const int HORSE_STAMINA_REGEN_INTERVAL = 12 * 60;
//const int HORSE_HP_DROP_INTERVAL = 60;
//const int HORSE_STAMINA_CONSUME_INTERVAL = 3;
//const int HORSE_STAMINA_REGEN_INTERVAL = 6;

THorseStat c_aHorseStat[HORSE_MAX_LEVEL+1] =
/*
   int iMinLevel;	// 탑승할 수 있는 최소 레벨
   int iNPCRace;
   int iMaxHealth;	// 말의 최대 체력
   int iMaxStamina;	// 말의 최대 스테미너
   int iST;
   int iDX;
   int iHT;
   int iIQ;
   int iDamMean;
   int iDamMin;
   int iDamMax;
   int iDef;
 */
{
	{ 1,	20101,	3,	4,		10,	10,	10,	10,	54,	43,	64,	32 },
	{ 1,	20101,	3,	4,		13,	13,	13,	13,	54,	43,	64,	32 },
	{ 1,	20101,	4,	4,		16,	16,	16,	16,	55,	44,	66,	33 },
	{ 1,	20101,	5,	5,		19,	19,	19,	19,	56,	44,	67,	33 },
	{ 1,	20101,	7,	5,		22,	22,	22,	22,	57,	45,	68,	34 },
	{ 1,	20101,	8,	6,		25,	25,	25,	25,	58,	46,	69,	34 },
	{ 1,	20101,	9,	6,		28,	28,	28,	28,	59,	47,	70,	35 },
	{ 1,	20101,	11,	7,		31,	31,	31,	31,	60,	48,	72,	36 },
	{ 1,	20101,	12,	7,		34,	34,	34,	34,	61,	48,	73,	36 },
	{ 1,	20101,	13,	8,		37,	37,	37,	37,	62,	49,	74,	37 },
	{ 1,	20101,	15,	10,		40,	40,	40,	40,	63,	50,	75,	37 },
	{ 25,	20104,	18,	30,		43,	43,	43,	43,	69,	55,	82,	41 },
	{ 25,	20104,	19,	35,		46,	46,	46,	46,	70,	56,	84,	42 },
	{ 25,	20104,	21,	40,		49,	49,	49,	49,	71,	56,	85,	42 },
	{ 25,	20104,	22,	50,		52,	52,	52,	52,	72,	57,	86,	43 },
	{ 25,	20104,	24,	55,		55,	55,	55,	55,	73,	58,	87,	43 },
	{ 25,	20104,	25,	60,		58,	58,	58,	58,	74,	59,	88,	44 },
	{ 25,	20104,	27,	65,		61,	61,	61,	61,	75,	60,	90,	45 },
	{ 25,	20104,	28,	70,		64,	64,	64,	64,	76,	60,	91,	45 },
	{ 25,	20104,	30,	80,		67,	67,	67,	67,	77,	61,	92,	46 },
	{ 25,	20104,	32,	100,	70,	70,	70,	70,	78,	62,	93,	46 },
	{ 50,	20107,	35,	120,	73,	73,	73,	73,	84,	67,	100,	50 },
	{ 50,	20107,	36,	125,	76,	76,	76,	76,	86,	68,	103,	51 },
	{ 50,	20107,	37,	130,	79,	79,	79,	79,	88,	70,	105,	52 },
	{ 50,	20107,	38,	135,	82,	82,	82,	82,	90,	72,	108,	54 },
	{ 50,	20107,	40,	140,	85,	85,	85,	85,	91,	72,	109,	54 },
	{ 50,	20107,	42,	145,	88,	88,	88,	88,	92,	73,	110,	55 },
	{ 50,	20107,	44,	150,	91,	91,	91,	91,	94,	75,	112,	56 },
	{ 50,	20107,	46,	160,	94,	94,	94,	94,	95,	76,	114,	57 },
	{ 50,	20107,	48,	170,	97,	97,	97,	97,	97,	77,	116,	58 },
	{ 75,	20110,	50,	200,	100,	100,	100,	100,	99,	79,	118,	59 }
};

CHorseRider::CHorseRider()
{
	Initialize();
}

CHorseRider::~CHorseRider()
{
	Destroy();
}

void CHorseRider::Initialize()
{
	m_eventStaminaRegen = NULL;
	m_eventStaminaConsume = NULL;
	memset(&m_Horse, 0, sizeof(m_Horse));
}

void CHorseRider::Destroy()
{
	event_cancel(&m_eventStaminaRegen);
	event_cancel(&m_eventStaminaConsume);
}

void CHorseRider::EnterHorse()
{
	if (GetHorseHealth() <= 0)
		return;

	if (IsHorseRiding())
	{
		m_Horse.bRiding = !m_Horse.bRiding;
		StartRiding();
	}
	else
	{
		StartStaminaRegenEvent();
	}
	CheckHorseHealthDropTime(false);
}

bool CHorseRider::ReviveHorse()
{
	if (GetHorseHealth()>0)
		return false;

	int level = GetHorseLevel();

	m_Horse.sHealth = c_aHorseStat[level].iMaxHealth;
	m_Horse.sStamina = c_aHorseStat[level].iMaxStamina;

	// 2005.03.24.ipkn.말 살린후 다시 죽는 현상 수정
	ResetHorseHealthDropTime();

	StartStaminaRegenEvent();
	return true;
}

short CHorseRider::GetHorseMaxHealth()
{
	int level = GetHorseLevel();
	return c_aHorseStat[level].iMaxHealth;
}

short CHorseRider::GetHorseMaxStamina()
{
	int level = GetHorseLevel();
	return c_aHorseStat[level].iMaxStamina;
}

void CHorseRider::FeedHorse()
{
	if (GetHorseHealth() > 0)
	{
		UpdateHorseHealth(+1);
		ResetHorseHealthDropTime();
	}
}

void CHorseRider::SetHorseData(const THorseInfo& crInfo)
{
	m_Horse = crInfo;
}

// Stamina
void CHorseRider::UpdateHorseDataByLogoff(DWORD dwLogoffTime)
{
	if (dwLogoffTime >= 12 * 60)
		UpdateHorseStamina(dwLogoffTime / 12 / 60, false); // 로그오프 12분당 1씩 회복
}

void CHorseRider::UpdateHorseStamina(int iStamina, bool bSend)
{
	int level = GetHorseLevel();

	m_Horse.sStamina = MINMAX(0, m_Horse.sStamina + iStamina, c_aHorseStat[level].iMaxStamina);

	if (GetHorseStamina() == 0 && IsHorseRiding())
	{
		StopRiding();
	}

	if (bSend)
		SendHorseInfo();
}

bool CHorseRider::StartRiding()
{
	if (m_Horse.bRiding)
		return false;

	if (GetHorseHealth() <= 0)
		return false;

	if (GetHorseStamina() <= 0)
		return false;

	m_Horse.bRiding = true;
	StartStaminaConsumeEvent();
	SendHorseInfo();
	return true;
}

bool CHorseRider::StopRiding()
{
	if (!m_Horse.bRiding)
		return false;

	m_Horse.bRiding = false;
	StartStaminaRegenEvent();
	return true;
}

EVENTINFO(horserider_info)
{
	CHorseRider* hr;

	horserider_info()
	: hr( 0 )
	{
	}
};

EVENTFUNC(horse_stamina_consume_event)
{
	horserider_info* info = dynamic_cast<horserider_info*>( event->info );

	if ( info == NULL )
	{
		sys_err( "horse_stamina_consume_event> <Factor> Null pointer" );
		return 0;
	}

	CHorseRider* hr = info->hr;

	if (hr->GetHorseHealth() <= 0)
	{
		hr->m_eventStaminaConsume = NULL;
		return 0;
	}

	hr->UpdateHorseStamina(-1);
	hr->UpdateRideTime(HORSE_STAMINA_CONSUME_INTERVAL);

	int delta = PASSES_PER_SEC(HORSE_STAMINA_CONSUME_INTERVAL);

	if (hr->GetHorseStamina() == 0)
	{
		hr->m_eventStaminaConsume = NULL;
		delta = 0;
	}

	hr->CheckHorseHealthDropTime();
	sys_log(0, "HORSE STAMINA - %p", get_pointer(event));
	return delta;
}

EVENTFUNC(horse_stamina_regen_event)
{
	horserider_info* info = dynamic_cast<horserider_info*>( event->info );

	if ( info == NULL )
	{
		sys_err( "horse_stamina_regen_event> <Factor> Null pointer" );
		return 0;
	}

	CHorseRider* hr = info->hr;

	if (hr->GetHorseHealth()<=0)
	{
		hr->m_eventStaminaRegen = NULL;
		return 0;
	}

	hr->UpdateHorseStamina(+1);
	int delta = PASSES_PER_SEC(HORSE_STAMINA_REGEN_INTERVAL);
	if (hr->GetHorseStamina() == hr->GetHorseMaxStamina())
	{
		delta = 0;
		hr->m_eventStaminaRegen = NULL;
	}

	hr->CheckHorseHealthDropTime();
	sys_log(0, "HORSE STAMINA + %p", get_pointer(event));


	return delta;
}

void CHorseRider::StartStaminaConsumeEvent()
{
	if (GetHorseHealth() <= 0)
		return;

	sys_log(0,"HORSE STAMINA REGEN EVENT CANCEL %p", get_pointer(m_eventStaminaRegen));
	event_cancel(&m_eventStaminaRegen);

	if (m_eventStaminaConsume)
		return;

	horserider_info* info = AllocEventInfo<horserider_info>();

	info->hr = this;
	m_eventStaminaConsume = event_create(horse_stamina_consume_event, info, PASSES_PER_SEC(HORSE_STAMINA_CONSUME_INTERVAL));
	sys_log(0,"HORSE STAMINA CONSUME EVENT CREATE %p", get_pointer(m_eventStaminaConsume));
}

void CHorseRider::StartStaminaRegenEvent()
{
	if (GetHorseHealth() <= 0)
		return;

	sys_log(0,"HORSE STAMINA CONSUME EVENT CANCEL %p", get_pointer(m_eventStaminaConsume));
	event_cancel(&m_eventStaminaConsume);

	if (m_eventStaminaRegen)
		return;

	horserider_info* info = AllocEventInfo<horserider_info>();

	info->hr = this;
	m_eventStaminaRegen = event_create(horse_stamina_regen_event, info, PASSES_PER_SEC(HORSE_STAMINA_REGEN_INTERVAL));
	sys_log(0,"HORSE STAMINA REGEN EVENT CREATE %p", get_pointer(m_eventStaminaRegen));
}

// Health
void CHorseRider::ResetHorseHealthDropTime()
{
	m_Horse.dwHorseHealthDropTime = get_global_time() + HORSE_HEALTH_DROP_INTERVAL;
}

void CHorseRider::CheckHorseHealthDropTime(bool bSend)
{
	DWORD now = get_global_time();

	while (m_Horse.dwHorseHealthDropTime < now)
	{
		m_Horse.dwHorseHealthDropTime += HORSE_HEALTH_DROP_INTERVAL;
		UpdateHorseHealth(-1, bSend);
	}
}

void CHorseRider::UpdateHorseHealth(int iHealth, bool bSend)
{
	int level = GetHorseLevel();

	if (setHorsesThatCantDie.count(GetMyHorseVnum()))
		return;

	m_Horse.sHealth = MINMAX(0, m_Horse.sHealth + iHealth, c_aHorseStat[level].iMaxHealth);

	if (level && m_Horse.sHealth == 0)
		HorseDie();

	if (bSend)
		SendHorseInfo();
}

void CHorseRider::HorseDie()
{
	sys_log(0, "HORSE DIE %p %p", get_pointer(m_eventStaminaRegen), get_pointer(m_eventStaminaConsume));
	UpdateHorseStamina(-m_Horse.sStamina);
	event_cancel(&m_eventStaminaRegen);
	event_cancel(&m_eventStaminaConsume);
}

void CHorseRider::SetHorseLevel(int iLevel)
{
	m_Horse.bLevel = iLevel = MINMAX(0, iLevel, HORSE_MAX_LEVEL);

	m_Horse.sStamina = c_aHorseStat[iLevel].iMaxStamina;
	m_Horse.sHealth = c_aHorseStat[iLevel].iMaxHealth;
	m_Horse.dwHorseHealthDropTime = 0;

	ResetHorseHealthDropTime();

	SendHorseInfo();
}

BYTE CHorseRider::GetHorseGrade()
{
	BYTE grade = 0;

	if (GetHorseLevel())
		grade = (GetHorseLevel() - 1) / 10 + 1;

	return grade;
}

