#include "stdafx.h"
#include "utils.h"
#include "char.h"
#include "OXEvent.h"
#include "questmanager.h"
#include "questlua.h"
#include "config.h"
#include "locale_service.h"
#include "cmd.h"

ACMD(do_oxevent_show_quiz)
{
	ch->ChatPacket(CHAT_TYPE_INFO, "===== OX QUIZ LIST =====");
	COXEventManager::instance().ShowQuizList(ch);
	ch->ChatPacket(CHAT_TYPE_INFO, "===== OX QUIZ LIST END =====");
}

ACMD(do_oxevent_log)
{
	if ( COXEventManager::instance().LogWinner() == false )
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OX The other participants in the Event are listed."));
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("OX The other participants in the Event are not listed."));
	}
}

ACMD(do_oxevent_get_attender)
{
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Number of the other participants : %d "), COXEventManager::instance().GetAttenderCount());
}

