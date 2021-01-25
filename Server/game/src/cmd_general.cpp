#include "stdafx.h"
#ifdef __FreeBSD__
#include <md5.h>
#else
#include "../../libthecore/include/xmd5.h"
#endif

#include "utils.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char.h"
#include "char_manager.h"
#include "motion.h"
#include "packet.h"
#include "affect.h"
#include "pvp.h"
#include "start_position.h"
#include "party.h"
#include "guild_manager.h"
#include "p2p.h"
#include "dungeon.h"
#include "messenger_manager.h"
#include "war_map.h"
#include "questmanager.h"
#include "item_manager.h"
#include "monarch.h"
#include "mob_manager.h"
#include "dev_log.h"
#include "item.h"
#include "arena.h"
#include "buffer_manager.h"
#include "unique_item.h"
#include "threeway_war.h"
#include "log.h"
#include "../../common/VnumHelper.h"
#ifdef __AUCTION__
#include "auction_manager.h"
#endif

ACMD(do_user_horse_ride)
{
	if (ch->IsObserverMode())
		return;

	if (ch->IsDead() || ch->IsStun())
		return;

	if (ch->IsHorseRiding() == false)
	{
		// ¸»ÀÌ ¾Æ´Ñ ´Ù¸¥Å»°ÍÀ» Å¸°íÀÖ´Ù.
		if (ch->GetMountVnum())
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "You are already riding a mount.");
			return;
		}

		if (ch->GetHorse() == NULL)
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "Please call your Horse first.");
			return;
		}

		ch->StartRiding();
	}
	else
	{
		ch->StopRiding();
	}
}

ACMD(do_user_horse_back)
{
	if (ch->GetHorse() != NULL)
	{
		ch->HorseSummon(false);
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You sent back your Horse.");
	}
	else if (ch->IsHorseRiding() == true)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You have to get off your Horse.");
	}
	else
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Please call your Horse first.");
	}
}

ACMD(do_user_horse_feed)
{
	// °³ÀÎ»óÁ¡À» ¿¬ »óÅÂ¿¡¼­´Â ¸» ¸ÔÀÌ¸¦ ÁÙ ¼ö ¾ø´Ù.
	if (ch->GetMyShop())
		return;

	if (ch->GetHorse() == NULL)
	{
		if (ch->IsHorseRiding() == false)
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "Please call your Horse first.");
		else
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot feed your Horse while sitting on it.");
		return;
	}

	DWORD dwFood = ch->GetHorseGrade() + 50054 - 1;

	if (ch->CountSpecifyItem(dwFood) > 0)
	{
		ch->RemoveSpecifyItem(dwFood, 1);
		ch->FeedHorse();
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You gave %s%s to a Horse.",
				ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName,
				"");
	}
	else
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You need %s.", ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName);
	}
}

#define MAX_REASON_LEN		128

EVENTINFO(TimedEventInfo)
{
	DynamicCharacterPtr ch;
	int		subcmd;
	int         	left_second;
	char		szReason[MAX_REASON_LEN];

	TimedEventInfo()
	: ch()
	, subcmd( 0 )
	, left_second( 0 )
	{
		::memset( szReason, 0, MAX_REASON_LEN );
	}
};

#ifdef ENABLE_CHANGE_CHANNEL
EVENTINFO(ChangeChannelEventInfo)
{
	DynamicCharacterPtr ch;
	int				channel_number;
	int         	left_second;

	ChangeChannelEventInfo()
	: ch()
	, channel_number( 0 )
	, left_second( 0 )
	{
	}
};
#endif

struct SendDisconnectFunc
{
	void operator () (LPDESC d)
	{
		if (d->GetCharacter())
		{
			if (d->GetCharacter()->GetGMLevel() == GM_PLAYER)
				d->GetCharacter()->ChatPacket(CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
		}
	}
};

struct DisconnectFunc
{
	void operator () (LPDESC d)
	{
		if (d->GetType() == DESC_TYPE_CONNECTOR)
			return;

		if (d->IsPhase(PHASE_P2P))
			return;

		if (d->GetCharacter())
			d->GetCharacter()->Disconnect("Shutdown(DisconnectFunc)");

		d->SetPhase(PHASE_CLOSE);
	}
};

EVENTINFO(shutdown_event_data)
{
	int seconds;

	shutdown_event_data()
	: seconds( 0 )
	{
	}
};

EVENTFUNC(shutdown_event)
{
	shutdown_event_data* info = dynamic_cast<shutdown_event_data*>( event->info );

	if ( info == NULL )
	{
		sys_err( "shutdown_event> <Factor> Null pointer" );
		return 0;
	}

	int * pSec = & (info->seconds);

	if (*pSec < 0)
	{
		sys_log(0, "shutdown_event sec %d", *pSec);

		if (--*pSec == -10)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), DisconnectFunc());
			return passes_per_sec;
		}
		else if (*pSec < -10)
			return 0;

		return passes_per_sec;
	}
	else if (*pSec == 0)
	{
		const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
		std::for_each(c_set_desc.begin(), c_set_desc.end(), SendDisconnectFunc());
		g_bNoMoreClient = true;
		--*pSec;
		return passes_per_sec;
	}
	else
	{
		char buf[64];
		snprintf(buf, sizeof(buf), LC_TEXT("%d seconds until shutting off."), *pSec);
		SendNotice(buf);

		--*pSec;
		return passes_per_sec;
	}
}

void Shutdown(int iSec)
{
	if (g_bNoMoreClient)
	{
		thecore_shutdown();
		return;
	}

	CWarMapManager::instance().OnShutdown();

	char buf[64];
	snprintf(buf, sizeof(buf), LC_TEXT("The Game will be closed after %d Seconds."), iSec);

	SendNotice(buf);

	shutdown_event_data* info = AllocEventInfo<shutdown_event_data>();
	info->seconds = iSec;

	event_create(shutdown_event, info, 1);
}

ACMD(do_shutdown)
{
	if (NULL == ch)
	{
		sys_err("Accept shutdown command from %s.", ch->GetName());
	}
	TPacketGGShutdown p;
	p.bHeader = HEADER_GG_SHUTDOWN;
	P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGShutdown));

	Shutdown(10);
}

EVENTFUNC(timed_event)
{
	TimedEventInfo * info = dynamic_cast<TimedEventInfo *>( event->info );

	if ( info == NULL )
	{
		sys_err( "timed_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (ch == NULL) { // <Factor>
		return 0;
	}
	LPDESC d = ch->GetDesc();

	if (info->left_second <= 0)
	{
		ch->m_pkTimedEvent = NULL;

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
			case SCMD_QUIT:
			case SCMD_PHASE_SELECT:
				{
					TPacketNeedLoginLogInfo acc_info;
					acc_info.dwPlayerID = ch->GetDesc()->GetAccountTable().id;

					db_clientdesc->DBPacket( HEADER_GD_VALID_LOGOUT, 0, &acc_info, sizeof(acc_info) );

					LogManager::instance().DetailLoginLog( false, ch );
				}
				break;
		}

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
				if (d)
					d->SetPhase(PHASE_CLOSE);
				break;

			case SCMD_QUIT:
				ch->ChatPacket(CHAT_TYPE_COMMAND, "quit");
				break;

			case SCMD_PHASE_SELECT:
				{
					ch->Disconnect("timed_event - SCMD_PHASE_SELECT");

					if (d)
					{
						d->SetPhase(PHASE_SELECT);
					}
				}
				break;
		}

		return 0;
	}
	else
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "%d seconds until shutting off.", info->left_second);
		--info->left_second;
	}

	return PASSES_PER_SEC(1);
}

#ifdef ENABLE_CHANGE_CHANNEL
EVENTFUNC(change_channel_event)
{
	ChangeChannelEventInfo * info = dynamic_cast<ChangeChannelEventInfo *>( event->info );

	if ( info == NULL )
	{
		sys_err( "change_channel_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (ch == NULL) { // <Factor>
		return 0;
	}

	if (info->left_second <= 0)
	{
		ch->m_pkChangeChannelEvent = NULL;

		ch->ChangeChannel(info->channel_number);
	
		return 0;
	}
	else
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Change channel in %d seconds.", info->left_second);
		--info->left_second;
	}

	return PASSES_PER_SEC(1);
}

ACMD(do_change_channel)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	DWORD channel_number = 0;
	str_to_number(channel_number, arg1);
	
	//if (ch->m_pkChangeChannelEvent)
	//{
	//	ch->ChatPacketTrans(CHAT_TYPE_INFO, "Change channel canceled.");
	//	event_cancel(&ch->m_pkChangeChannelEvent);
	//	return;
	//}
	
	if(!ch)
	{
		return;	
	}
	
	if(channel_number == 99 || g_bChannel == 99){
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You can't change channel in this map.");
		return;		
	}
	
	if(channel_number == g_bChannel)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You are already in this channel.");
		return;		
	}
	
	if (ch->IsDead() || !ch->CanWarp())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You can't do that now. Wait 10 seconds and try again.");
		return;
	}
	
	if(channel_number <= 0 || channel_number > 6)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "This channel is not valid.");
		return;
	}
	
	if (channel_number != 0)
	{
		if (ch->m_pkChangeChannelEvent)
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "Change channel canceled.");
			event_cancel(&ch->m_pkChangeChannelEvent);
			return;
		}
	
		ChangeChannelEventInfo* info = AllocEventInfo<ChangeChannelEventInfo>();
	
		{
			if (ch->IsPosition(POS_FIGHTING))
				info->left_second = 10;
			else
				info->left_second = 3;
		}
	
		info->ch					= ch;
		info->channel_number		= channel_number;
	
		ch->m_pkChangeChannelEvent	= event_create(change_channel_event, info, 1);
	}
}
#endif

ACMD(do_cmd)
{
	/* RECALL_DELAY
	   if (ch->m_pkRecallEvent != NULL)
	   {
	   ch->ChatPacketTrans(CHAT_TYPE_INFO, "Your Logout was cancelled.");
	   event_cancel(&ch->m_pkRecallEvent);
	   return;
	   }
	// END_OF_RECALL_DELAY */

	if (ch->m_pkTimedEvent)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Your Logout was cancelled.");
		event_cancel(&ch->m_pkTimedEvent);
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "Back to Login Window. Please wait.");
			break;

		case SCMD_QUIT:
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "You will be disconnected from the server. Please wait.");
			break;

		case SCMD_PHASE_SELECT:
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "Changing character. Please wait.");
			break;
	}

	int nExitLimitTime = 10;

	if (ch->IsHack(false, true, nExitLimitTime) &&
		false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()) &&
	   	(!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG))
	{
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
		case SCMD_QUIT:
		case SCMD_PHASE_SELECT:
			{
				TimedEventInfo* info = AllocEventInfo<TimedEventInfo>();

				{
					if (ch->IsPosition(POS_FIGHTING))
						info->left_second = 10;
					else
						info->left_second = 3;
				}

				info->ch		= ch;
				info->subcmd		= subcmd;
				strlcpy(info->szReason, argument, sizeof(info->szReason));

				ch->m_pkTimedEvent	= event_create(timed_event, info, 1);
			}
			break;
	}
}

ACMD(do_mount)
{
	/*
	   char			arg1[256];
	   struct action_mount_param	param;

	// ÀÌ¹Ì Å¸°í ÀÖÀ¸¸é
	if (ch->GetMountingChr())
	{
	char arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
	return;

	param.x		= atoi(arg1);
	param.y		= atoi(arg2);
	param.vid	= ch->GetMountingChr()->GetVID();
	param.is_unmount = true;

	float distance = DISTANCE_SQRT(param.x - (DWORD) ch->GetX(), param.y - (DWORD) ch->GetY());

	if (distance > 600.0f)
	{
	ch->ChatPacketTrans(CHAT_TYPE_INFO, "Come closer to get off.");
	return;
	}

	action_enqueue(ch, ACTION_TYPE_MOUNT, &param, 0.0f, true);
	return;
	}

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	return;

	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(atoi(arg1));

	if (!tch->IsNPC() || !tch->IsMountable())
	{
	ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot ride that.");
	return;
	}

	float distance = DISTANCE_SQRT(tch->GetX() - ch->GetX(), tch->GetY() - ch->GetY());

	if (distance > 600.0f)
	{
	ch->ChatPacketTrans(CHAT_TYPE_INFO, "Come closer to ride.");
	return;
	}

	param.vid		= tch->GetVID();
	param.is_unmount	= false;

	action_enqueue(ch, ACTION_TYPE_MOUNT, &param, 0.0f, true);
	 */
}

ACMD(do_fishing)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	ch->SetRotation(atof(arg1));
	ch->fishing();
}

ACMD(do_console)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ConsoleEnable");
}

ACMD(do_restart)
{
	if (false == ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");
		ch->StartRecoveryEvent();
		return;
	}

	if (NULL == ch->m_pkDeadEvent)
		return;

	int iTimeToDead = (event_time(ch->m_pkDeadEvent) / passes_per_sec);

	if (subcmd != SCMD_RESTART_TOWN && (!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG))
	{
		if (!test_server)
		{
			if (ch->IsHack())
			{
				//¼ºÁö ¸ÊÀÏ°æ¿ì¿¡´Â Ã¼Å© ÇÏÁö ¾Ê´Â´Ù.
				if (false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
				{
					ch->ChatPacketTrans(CHAT_TYPE_INFO, "No new start possible.(%d seconds left) ", iTimeToDead - (180 - g_nPortalLimitTime));
					return;
				}
			}
#define eFRS_HERESEC	170
			if (iTimeToDead > eFRS_HERESEC)
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "No new start possible.(%d seconds left) ", iTimeToDead - eFRS_HERESEC);
				return;
			}
		}
	}

	//PREVENT_HACK
	//DESC : Ã¢°í, ±³È¯ Ã¢ ÈÄ Æ÷Å»À» »ç¿ëÇÏ´Â ¹ö±×¿¡ ÀÌ¿ëµÉ¼ö ÀÖ¾î¼­
	//		ÄðÅ¸ÀÓÀ» Ãß°¡
	if (subcmd == SCMD_RESTART_TOWN)
	{
		if (ch->IsHack())
		{
			//±æµå¸Ê, ¼ºÁö¸Ê¿¡¼­´Â Ã¼Å© ÇÏÁö ¾Ê´Â´Ù.
			if ((!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG) ||
			   	false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "No new start possible.(%d seconds left) ", iTimeToDead - (180 - g_nPortalLimitTime));
				return;
			}
		}

#define eFRS_TOWNSEC	173
		if (iTimeToDead > eFRS_TOWNSEC)
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "Still no new start possible in the city. (%d seconds left)", iTimeToDead - eFRS_TOWNSEC);
			return;
		}
	}
	//END_PREVENT_HACK

	ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");

	ch->GetDesc()->SetPhase(PHASE_GAME);
	ch->SetPosition(POS_STANDING);
	ch->StartRecoveryEvent();

	//FORKED_LOAD
	//DESC: »ï°Å¸® ÀüÅõ½Ã ºÎÈ°À» ÇÒ°æ¿ì ¸ÊÀÇ ÀÔ±¸°¡ ¾Æ´Ñ »ï°Å¸® ÀüÅõÀÇ ½ÃÀÛÁöÁ¡À¸·Î ÀÌµ¿ÇÏ°Ô µÈ´Ù.
	if (1 == quest::CQuestManager::instance().GetEventFlag("threeway_war"))
	{
		if (subcmd == SCMD_RESTART_TOWN || subcmd == SCMD_RESTART_HERE)
		{
			if (true == CThreeWayWar::instance().IsThreeWayWarMapIndex(ch->GetMapIndex()) &&
					false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));

				ch->ReviveInvisible(5);
				ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());

				return;
			}

			//¼ºÁö
			if (true == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				if (CThreeWayWar::instance().GetReviveTokenForPlayer(ch->GetPlayerID()) <= 0)
				{
					ch->ChatPacketTrans(CHAT_TYPE_INFO, "You lost the chance for resurrection. Go back to the City.");
					ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
				}
				else
				{
					ch->Show(ch->GetMapIndex(), GetSungziStartX(ch->GetEmpire()), GetSungziStartY(ch->GetEmpire()));
				}

				ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
				ch->ReviveInvisible(5);

				return;
			}
		}
	}
	//END_FORKED_LOAD

	if (ch->GetDungeon())
		ch->GetDungeon()->UseRevive(ch);

	if (ch->GetWarMap() && !ch->IsObserverMode())
	{
		CWarMap * pMap = ch->GetWarMap();
		DWORD dwGuildOpponent = pMap ? pMap->GetGuildOpponent(ch) : 0;

		if (dwGuildOpponent)
		{
			switch (subcmd)
			{
				case SCMD_RESTART_TOWN:
					sys_log(0, "do_restart: restart town");
					PIXEL_POSITION pos;

					if (CWarMapManager::instance().GetStartPosition(ch->GetMapIndex(), ch->GetGuild()->GetID() < dwGuildOpponent ? 0 : 1, pos))
						ch->Show(ch->GetMapIndex(), pos.x, pos.y);
					else
						ch->ExitToSavedLocation();

					ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
					ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
					ch->ReviveInvisible(5);
					break;

				case SCMD_RESTART_HERE:
					sys_log(0, "do_restart: restart here");
					ch->RestartAtSamePos();
					//ch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY());
					ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
					ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
					ch->ReviveInvisible(5);
					break;
			}

			return;
		}
	}
	switch (subcmd)
	{
		case SCMD_RESTART_TOWN:
			sys_log(0, "do_restart: restart town");
			PIXEL_POSITION pos;

			if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(ch->GetMapIndex(), ch->GetEmpire(), pos))
				ch->WarpSet(pos.x, pos.y);
			else
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
			ch->PointChange(POINT_HP, 50 - ch->GetHP());
			ch->DeathPenalty(1);
			break;

		case SCMD_RESTART_HERE:
			sys_log(0, "do_restart: restart here");
			ch->RestartAtSamePos();
			//ch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY());
			ch->PointChange(POINT_HP, 50 - ch->GetHP());
			ch->DeathPenalty(0);
			ch->ReviveInvisible(5);
			break;
	}
}

#define MAX_STAT g_iStatusPointSetMaxValue

ACMD(do_stat_reset)
{
	ch->PointChange(POINT_STAT_RESET_COUNT, 12 - ch->GetPoint(POINT_STAT_RESET_COUNT));
}

ACMD(do_stat_minus)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->IsPolymorphed())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot change your state as long as you are transformed.");
		return;
	}

	if (ch->GetPoint(POINT_STAT_RESET_COUNT) <= 0)
		return;

	if (!strcmp(arg1, "st"))
	{
		if (ch->GetRealPoint(POINT_ST) <= JobInitialPoints[ch->GetJob()].st)
			return;

		ch->SetRealPoint(POINT_ST, ch->GetRealPoint(POINT_ST) - 1);
		ch->SetPoint(POINT_ST, ch->GetPoint(POINT_ST) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_ST, 0);
	}
	else if (!strcmp(arg1, "dx"))
	{
		if (ch->GetRealPoint(POINT_DX) <= JobInitialPoints[ch->GetJob()].dx)
			return;

		ch->SetRealPoint(POINT_DX, ch->GetRealPoint(POINT_DX) - 1);
		ch->SetPoint(POINT_DX, ch->GetPoint(POINT_DX) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_DX, 0);
	}
	else if (!strcmp(arg1, "ht"))
	{
		if (ch->GetRealPoint(POINT_HT) <= JobInitialPoints[ch->GetJob()].ht)
			return;

		ch->SetRealPoint(POINT_HT, ch->GetRealPoint(POINT_HT) - 1);
		ch->SetPoint(POINT_HT, ch->GetPoint(POINT_HT) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_HT, 0);
		ch->PointChange(POINT_MAX_HP, 0);
	}
	else if (!strcmp(arg1, "iq"))
	{
		if (ch->GetRealPoint(POINT_IQ) <= JobInitialPoints[ch->GetJob()].iq)
			return;

		ch->SetRealPoint(POINT_IQ, ch->GetRealPoint(POINT_IQ) - 1);
		ch->SetPoint(POINT_IQ, ch->GetPoint(POINT_IQ) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_IQ, 0);
		ch->PointChange(POINT_MAX_SP, 0);
	}
	else
		return;

	ch->PointChange(POINT_STAT, +1);
	ch->PointChange(POINT_STAT_RESET_COUNT, -1);
	ch->ComputePoints();
}

ACMD(do_stat)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->IsPolymorphed())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot change your state as long as you are transformed.");
		return;
	}

	if (ch->GetPoint(POINT_STAT) <= 0)
		return;

	BYTE idx = 0;

	if (!strcmp(arg1, "st"))
		idx = POINT_ST;
	else if (!strcmp(arg1, "dx"))
		idx = POINT_DX;
	else if (!strcmp(arg1, "ht"))
		idx = POINT_HT;
	else if (!strcmp(arg1, "iq"))
		idx = POINT_IQ;
	else
		return;

	// ch->ChatPacket(CHAT_TYPE_INFO, "%s GRP(%d) idx(%u), MAX_STAT(%d), expr(%d)", __FUNCTION__, ch->GetRealPoint(idx), idx, MAX_STAT, ch->GetRealPoint(idx) >= MAX_STAT);
	if (ch->GetRealPoint(idx) >= MAX_STAT)
		return;

	ch->SetRealPoint(idx, ch->GetRealPoint(idx) + 1);
	ch->SetPoint(idx, ch->GetPoint(idx) + 1);
	ch->ComputePoints();
	ch->PointChange(idx, 0);

	if (idx == POINT_IQ)
	{
		ch->PointChange(POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT)
	{
		ch->PointChange(POINT_MAX_SP, 0);
	}

	ch->PointChange(POINT_STAT, -1);
	ch->ComputePoints();
}

ACMD(do_pvp)
{
	if (ch->GetArena() != NULL || CArenaManager::instance().IsArenaMap(ch->GetMapIndex()) == true)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You can't use this in the arena.");
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(vid);

	if (!pkVictim)
		return;

	if (pkVictim->IsNPC())
		return;

	if (pkVictim->GetArena() != NULL)
	{
		pkVictim->ChatPacketTrans(CHAT_TYPE_INFO, "The player is currently fighting.");
		return;
	}

	CPVPManager::instance().Insert(ch, pkVictim);
}

ACMD(do_guildskillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (!ch->GetGuild())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> It does not belong to the Guild.");
		return;
	}

	CGuild* g = ch->GetGuild();
	TGuildMember* gm = g->GetMember(ch->GetPlayerID());
	if (gm->grade == GUILD_LEADER_GRADE)
	{
		DWORD vnum = 0;
		str_to_number(vnum, arg1);
		g->SkillLevelUp(vnum);
	}
	else
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> You cannot change the Level of the Guild Skills.");
	}
}

ACMD(do_skillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vnum = 0;
	str_to_number(vnum, arg1);

	if (true == ch->CanUseSkill(vnum))
	{
		ch->SkillLevelUp(vnum);
	}
	else
	{
		switch(vnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:

			case SKILL_7_A_ANTI_TANHWAN:
			case SKILL_7_B_ANTI_AMSEOP:
			case SKILL_7_C_ANTI_SWAERYUNG:
			case SKILL_7_D_ANTI_YONGBI:

			case SKILL_8_A_ANTI_GIGONGCHAM:
			case SKILL_8_B_ANTI_YEONSA:
			case SKILL_8_C_ANTI_MAHWAN:
			case SKILL_8_D_ANTI_BYEURAK:

			case SKILL_ADD_HP:
			case SKILL_RESIST_PENETRATE:
				ch->SkillLevelUp(vnum);
				break;
		}
	}
}

//
// @version	05/06/20 Bang2ni - Ä¿¸Çµå Ã³¸® Delegate to CHARACTER class
//
ACMD(do_safebox_close)
{
	ch->CloseSafebox();
}

//
// @version	05/06/20 Bang2ni - Ä¿¸Çµå Ã³¸® Delegate to CHARACTER class
//
ACMD(do_safebox_password)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	ch->ReqSafeboxLoad(arg1);
}

ACMD(do_safebox_change_password)
{
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || strlen(arg1)>6)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Storages> You entered a wrong password.");
		return;
	}

	if (!*arg2 || strlen(arg2)>6)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Storages> You entered a wrong password.");
		return;
	}

	TSafeboxChangePasswordPacket p;

	p.dwID = ch->GetDesc()->GetAccountTable().id;
	strlcpy(p.szOldPassword, arg1, sizeof(p.szOldPassword));
	strlcpy(p.szNewPassword, arg2, sizeof(p.szNewPassword));

	db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_CHANGE_PASSWORD, ch->GetDesc()->GetHandle(), &p, sizeof(p));
}

ACMD(do_mall_password)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1 || strlen(arg1) > 6)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Storages> You entered a wrong password.");
		return;
	}

	int iPulse = thecore_pulse();

	if (ch->GetMall())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Storages> The storage is already open.");
		return;
	}

	if (iPulse - ch->GetMallLoadTime() < passes_per_sec * 10) // 10ÃÊ¿¡ ÇÑ¹ø¸¸ ¿äÃ» °¡´É
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Storages> You have to wait 10 seconds to open the storage again.");
		return;
	}

	ch->SetMallLoadTime(iPulse);

	TSafeboxLoadPacket p;
	p.dwID = ch->GetDesc()->GetAccountTable().id;
	strlcpy(p.szLogin, ch->GetDesc()->GetAccountTable().login, sizeof(p.szLogin));
	strlcpy(p.szPassword, arg1, sizeof(p.szPassword));

	db_clientdesc->DBPacket(HEADER_GD_MALL_LOAD, ch->GetDesc()->GetHandle(), &p, sizeof(p));
}

ACMD(do_mall_close)
{
	if (ch->GetMall())
	{
		ch->SetMallLoadTime(thecore_pulse());
		ch->CloseMall();
		ch->Save();
	}
}

ACMD(do_ungroup)
{
	if (!ch->GetParty())
		return;

	if (!CPartyManager::instance().IsEnablePCParty())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Group> The server cannot execute the Group request.");
		return;
	}

	if (ch->GetDungeon())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Group> You cannot leave a Group while you are in a dungeon.");
		return;
	}

	LPPARTY pParty = ch->GetParty();

	if (pParty->GetMemberCount() == 2)
	{
		// party disband
		CPartyManager::instance().DeleteParty(pParty);
	}
	else
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Group> You left the Group.");
		//pParty->SendPartyRemoveOneToAll(ch);
		pParty->Quit(ch->GetPlayerID());
		//pParty->SendPartyRemoveAllToOne(ch);
	}
}

#ifdef GIFT_SYSTEM
#include "db.h"
#include <string>
#include <boost/algorithm/string.hpp>

ACMD(do_gift_show)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "gift_show");
}
bool GetGift(LPCHARACTER ch, DWORD id,bool all=false)
{
	char szSockets[1024] = { '\0' };
	char *tempSockets = szSockets;
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
	{
		tempSockets += sprintf(tempSockets, "socket%d", i);

		if (i<ITEM_SOCKET_MAX_NUM - 1)
			tempSockets += sprintf(tempSockets, ",");
	}
	char szAttrs[1024] = { '\0' };
	char *tempAttrs = szAttrs;
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
	{
		if (i < 7)
			tempAttrs += sprintf(tempAttrs, "attrtype%d,attrvalue%d", i, i);
		else
			tempAttrs += sprintf(tempAttrs, "applytype%d,applyvalue%d", i - 7, i - 7);
		if (i<ITEM_ATTRIBUTE_MAX_NUM - 1)
			tempAttrs += sprintf(tempAttrs, ",");
	}
	char query[8192];
	if (!all)
		snprintf(query, sizeof(query), "SELECT id,vnum,count,%s,%s from player_gift where id='%d' and owner_id=%d and status='WAIT'", szSockets, szAttrs, id, ch->GetPlayerID());
	else
		snprintf(query, sizeof(query), "SELECT id,vnum,count,%s,%s from player_gift where owner_id=%d and status='WAIT'", szSockets, szAttrs, ch->GetPlayerID());
	SQLMsg * pkMsg(DBManager::instance().DirectQuery(query));
	SQLResult * pRes = pkMsg->Get();
	if (pRes->uiNumRows > 0)
	{
		ch->SetQuestFlag("gift.time", get_global_time()+(1*pRes->uiNumRows)+2);
		MYSQL_ROW row;
		bool force = false;
		while ((row = mysql_fetch_row(pRes->pSQLResult)) != NULL)
		{
			DWORD vnum, socket[ITEM_SOCKET_MAX_NUM], attr[ITEM_ATTRIBUTE_MAX_NUM][2];
			int col = 0;
			long long count = 0;
			str_to_number(id, row[col++]);
			str_to_number(vnum, row[col++]);
			str_to_number(count, row[col++]);
			if (vnum == 1)
			{
#ifndef FULL_YANG
				GoldType nTotalMoney = ch->GetGold()+count;

				if (GOLD_MAX <= nTotalMoney)
				{
					//sys_err("[OVERFLOW_GOLD] Overflow (GOLD_MAX) id %u name %s", ch->GetPlayerID(), ch->GetName());
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("20ld lÉR» AE°úÇDz© »óÁ^R» z­Lö°^ lr¨R´D´U"));
					return true;
				}
#endif
#ifdef FULL_YANG_OWN
				ch->ChangeGold(count);
#else
				ch->PointChange(POINT_GOLD, count, false);
#endif
			}
			else {
				if (force)
					continue;
				for (int s = 0; s < ITEM_SOCKET_MAX_NUM; s++)
					str_to_number(socket[s], row[col++]);

				for (int a = 0; a < ITEM_ATTRIBUTE_MAX_NUM; a++)
				{
					str_to_number(attr[a][0], row[col++]);
					str_to_number(attr[a][1], row[col++]);
				}
				LPITEM item = ITEM_MANAGER::instance().CreateItem(vnum, count, 0, true);
				if (item)
				{
					for (int s = 0; s < ITEM_SOCKET_MAX_NUM; s++)
						item->SetSocket(s, socket[s], false);
					item->ClearAttribute();
					for (int a = 0; a < ITEM_ATTRIBUTE_MAX_NUM; a++)
						item->SetForceAttribute(a, attr[a][0], attr[a][1]);

#ifdef ENABLE_SPECIAL_INVENTORY
					int iEmptyPos = ch->GetEmptyInventory(item);
#else
					int iEmptyPos = ch->GetEmptyInventory(item->GetSize());
#endif
					if (iEmptyPos != -1)
					{
						item->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
					}
					else
					{
						M2_DESTROY_ITEM(item);
						ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT( "INVENTORY_FULL_ERROR"));
						force = true;
						continue;
					}
				}
				else
				{
					ch->ChatPacket(CHAT_TYPE_INFO, "<Gift> %s #4", LC_TEXT( "UNKNOW_ERROR"));
					force = true;
					continue;
				}

			}
			DBManager::instance().DirectQuery("UPDATE player_gift SET status='OK',date_get=NOW() where id=%d;", id);
		}
		if (force)
			return true;
		if (all)
			ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT("GIFT_ADD_ALL_SUCCESS"));
		else
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT( "GIFT_ADD_SUCCESS"));
		ch->SetQuestFlag("gift.time", get_global_time()+2);
		return true;
	}
	return false;
}
ACMD(do_gift_get)
{
	if(ch->GetQuestFlag("gift.time") > get_global_time())
		return;
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	bool full = !isdigit(*arg1);
	DWORD id;
	str_to_number(id, arg1);
	if (GetGift(ch, id,full))
	{
		ch->RefreshGift();
		ch->LoadGiftPage(ch->GetLastGiftPage());
	}

}
ACMD(do_gift_refresh)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
	{
		if (ch->GetGiftPages() > 0)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "gift_info %d", ch->GetGiftPages());
	}
	else{
		int page;
		str_to_number(page, arg1);
		ch->LoadGiftPage(page);
	}
}
#endif
#ifdef OFFLINE_SHOP
void DeleteShop(DWORD id)
{
	CharacterVectorInteractor i;
	if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(30000, i))
	{
		CharacterVectorInteractor::iterator it = i.begin();

		while (it != i.end()) {
			LPCHARACTER pc = *it++;
			if (pc)
				if (pc->GetRaceNum() == 30000 && pc->GetPrivShop() == id) {
					pc->DeleteMyShop();
					return;
				}


		}
	}
	TPacketShopClose packet;
	packet.shop_id = id;
	packet.pid = 0;
	packet.error = false;
	db_clientdesc->DBPacket(HEADER_GD_SHOP_CLOSE, 0, &packet, sizeof(packet));
}

ACMD(do_close_shop)
{
#ifdef ACCOUNT_SHIELD
	if (ch->IsBlockAccount())
		return;
#endif
	if (ch->IsObserverMode() || ch->GetExchange())
		return;
	DWORD id;
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
	{
		ch->CloseMyShop();
	}else{
		str_to_number(id, arg1);
		char pid[4096];
		sprintf(pid, "and player_id=%d", ch->GetPlayerID());
		if (id > 0)
		{
			SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT player_id,channel from player_shop WHERE id = %d %s", id, (ch->GetGMLevel() >= SHOP_GM_PRIVILEGES ? "" : pid)));
			SQLResult * pRes = pkMsg->Get();
			if (pRes->uiNumRows > 0)
				DeleteShop(id);
		}



	}
}
#include "banword.h"
ACMD(do_set_name_shop)
{
#ifdef ACCOUNT_SHIELD
	if (ch->IsBlockAccount())
		return;
#endif
	if (ch->IsObserverMode() || ch->GetExchange())
		return;
	DWORD id;
	char arg1[256];
	char arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	if (!*arg1 || !*arg2)
		return;
	str_to_number(id, arg1);
	char pid[4096];
	sprintf(pid, "and player_id=%d", ch->GetPlayerID());
	std::string m_stShopSign(arg2);
	boost::algorithm::replace_all(m_stShopSign, "\\", " ");
	boost::algorithm::replace_all(m_stShopSign, "%", "%%");
	if (m_stShopSign.length()>SHOP_SIGN_MAX_LEN)
		m_stShopSign.resize(SHOP_SIGN_MAX_LEN);
	if (m_stShopSign.length()<1) {
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("þnLÓlîlÞ Rþlî°^ C÷ÇÔµC »óÁ^ RE¸§R¸·Î »óÁ^R» z­ Lö lr¨R´D´U."));
		return;
	}
#ifdef STRING_PROTECTION
	if (CBanwordManager::instance().CheckString(m_stShopSign.c_str(), m_stShopSign.length()) != "")
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("þnLÓlîlÞ Rþlî°^ C÷ÇÔµC »óÁ^ RE¸§R¸·Î »óÁ^R» z­ Lö lr¨R´D´U."));
		return;
	}
#else

	if (CBanwordManager::instance().CheckString(m_stShopSign.c_str(), m_stShopSign.length()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("þnLÓlîlÞ Rþlî°^ C÷ÇÔµC »óÁ^ RE¸§R¸·Î »óÁ^R» z­ Lö lr¨R´D´U."));
		return;
	}
#endif
	if (id>0)
	{
		SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT id,player_id,channel from player_shop WHERE id = %d %s", id, (ch->GetGMLevel() >= SHOP_GM_PRIVILEGES ? "" : pid)));
		SQLResult * pRes = pkMsg->Get();
		if (pRes->uiNumRows > 0)
		{
			char szName[256];
			DBManager::instance().EscapeString(szName, 256, m_stShopSign.c_str(), m_stShopSign.length());
			DBManager::Instance().DirectQuery("UPDATE player_shop SET name='%s' WHERE id=%d", szName, id);
			ch->LoadPrivShops();
			CharacterVectorInteractor i;
			if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(30000, i))
			{
				CharacterVectorInteractor::iterator it = i.begin();

				while (it != i.end()) {
					LPCHARACTER pc = *it++;
					if (pc)
						if (pc->GetMyShop() && pc->GetPrivShop() == id) {
							pc->SetShopSign(m_stShopSign.c_str());
							return;
						}


				}
			}
			TPacketShopName packet;
			packet.shop_id = id;
			strlcpy(packet.szSign, m_stShopSign.c_str(), sizeof(packet.szSign) - 1);
			db_clientdesc->DBPacket(HEADER_GD_SHOP_NAME, 0, &packet, sizeof(packet));


		}
	}
}

ACMD(do_shop_refresh)
{
	ch->SendShops();
}
ACMD(do_shop_yang)
{

#ifdef FULL_YANG
	if (ch->IsObserverMode() || ch->GetExchange())
		return;
	DWORD id;
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;
	str_to_number(id, arg1);
	if (*arg1)
	{
		SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT gold from player_shop WHERE id = %d and player_id=%d", id,ch->GetPlayerID()));
		SQLResult * pRes = pkMsg->Get();
		if (pRes->uiNumRows>0)
		{
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(pRes->pSQLResult)) != NULL)
			{
				GoldType gold;
				str_to_number(gold, row[0]);
				if (gold >0)
				{
#ifndef FULL_YANG
					long long nTotalMoney = ch->GetGold() + gold;

					if (GOLD_MAX <= nTotalMoney)
					{
						//sys_err("[OVERFLOW_GOLD] Overflow (GOLD_MAX) id %u name %s", ch->GetPlayerID(), ch->GetName());
						ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("20ld lÉR» AE°úÇDz© »óÁ^R» z­Lö°^ lr¨R´D´U"));
						return;
					}
#endif
#ifdef FULL_YANG_OWN
					ch->ChangeGold(gold);
#else
					ch->PointChange(POINT_GOLD,gold, false);
#endif

					TPrivShop s = ch->GetPrivShopTable(id);
					s.gold = s.gold - gold;
					s.rest_count = s.item_count - 1;
					ch->UpdatePrivShopTable(s.shop_id, s);
					ch->SendShops();
					DBManager::instance().DirectQuery("UPDATE player_shop SET gold=gold - %lld WHERE id = %d", gold, id);
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("SHOP_YANG_ADD"));
				}
			}
		}
	}
#else
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("SHOP_YANG_IN_GIFT"));
#endif
}
#include <boost/algorithm/string.hpp>

LPCHARACTER _GetOfflineShop(DWORD shop_id)
{
	CharacterVectorInteractor i;
	if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(30000, i))
	{
		CharacterVectorInteractor::iterator it = i.begin();
		while (it != i.end()) {
			LPCHARACTER pc = *it++;
			if (pc)
				if (pc->GetRaceNum() == 30000 && pc->GetPrivShop() == shop_id)
					return pc;
		}
	}
	return NULL;
}
void _UpdateOfflineShop(DWORD shop_id, bool tick, bool shop_locked, bool refresh = false)
{
	LPCHARACTER pc = _GetOfflineShop(shop_id);
	if (pc && pc->GetPrivShop() == shop_id && pc->GetMyShop()) {
		pc->SetShopEditModeTick();
		if(refresh)
			pc->UpdateShopItems();
		else if(shop_locked || !tick && !shop_locked)
			pc->SetShopEditMode(shop_locked);
		return;
	}
	TPacketShopUpdateItem packet;
	packet.shop_id = shop_id;
	packet.tick = tick;
	packet.refresh = refresh;
	packet.shop_locked = shop_locked;
	db_clientdesc->DBPacket(HEADER_GD_SHOP_UPDATE_ITEM, 0, &packet, sizeof(packet));

}
ACMD(do_shop_update_item)
{

	if (ch->IsObserverMode() || ch->GetExchange())
		return;
	extern ACMD(do_shop_refresh_items);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;
	std::vector<std::string> args;
	boost::split(args, arg1, boost::is_any_of("|"));
	if (args.size()<2) {
		return;
	}
	DWORD shop_id;
	str_to_number(shop_id, args[1].c_str());
	LPCHARACTER pc_shop = _GetOfflineShop(shop_id);
	bool edit_mode=false;
	if(pc_shop)
	{
		if(pc_shop->GetPrivShopOwner() != ch->GetPlayerID())
			return;
		edit_mode = pc_shop->GetShopEditMode();
	}else{
		SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT id,edit_mode from player_shop WHERE id = %d and player_id=%d", shop_id, ch->GetPlayerID()));
		SQLResult * pRes = pkMsg->Get();
		if (!pRes->uiNumRows)
			return;
		MYSQL_ROW row;
		if((row = mysql_fetch_row(pRes->pSQLResult)) != NULL)
			str_to_number(edit_mode,row[1]);
	}

	/*
	/update_shop_item edit|%d
	/update_shop_item price|%d|%d|%d)
	/update_shop_item remove|%d|%d
	/update_shop_item add|%d|%d|%d|%d|%s
	*/

	if (args.size() == 2)
	{
		_UpdateOfflineShop(shop_id, args[0] == "edit_tick", args[0] == "edit_start");
		return;
	}

	if(!edit_mode)
	{
		do_shop_refresh_items(ch,args[1].c_str(),0,0);
		ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT("SHOP_NO_EDIT_MODE"));
		return;
	}

	if (args.size() == 4)
	{
		DWORD item_id;
		str_to_number(item_id, args[2].c_str());
		long long price;
		str_to_number(price, args[3].c_str());
		if (price <= 0 || item_id <= 0)
			return;
		{
			SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT id from player_shop_items WHERE id = %d and shop_id=%d", item_id, shop_id));
			SQLResult * pRes = pkMsg->Get();
			if (pRes->uiNumRows > 0)
				DBManager::instance().DirectQuery("UPDATE player_shop_items SET price=%lld WHERE id = %d and shop_id=%d", price, item_id, shop_id);

		}
	}

	else if (args.size() == 3)
	{
		DWORD item_id;
		str_to_number(item_id, args[2].c_str());
		if (item_id <= 0)
			return;
		{

			std::string shop_name(LC_TEXT( "SHOP_NAME"));
			boost::replace_all(shop_name, "#PLAYER_NAME#", ch->GetName());
			boost::replace_all(shop_name, "#ID#", "");


			char szSockets[1024] = { '\0' };
			char *tempSockets = szSockets;
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
			{
				tempSockets += sprintf(tempSockets, "socket%d", i);

				if (i<ITEM_SOCKET_MAX_NUM - 1)
					tempSockets += sprintf(tempSockets, ",");
			}
			char szAttrs[1024] = { '\0' };
			char *tempAttrs = szAttrs;
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
			{
				if (i < 7)
					tempAttrs += sprintf(tempAttrs, "attrtype%d,attrvalue%d", i, i);
				else
					tempAttrs += sprintf(tempAttrs, "applytype%d,applyvalue%d", i - 7, i - 7);
				if (i<ITEM_ATTRIBUTE_MAX_NUM - 1)
					tempAttrs += sprintf(tempAttrs, ",");
			}

			SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT vnum,count,%s,%s from player_shop_items where shop_id='%d' and id=%d", szSockets,szAttrs,shop_id, item_id));
			SQLResult * pRes = pkMsg->Get();
			if (pRes->uiNumRows>0)
			{
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(pRes->pSQLResult)) != NULL)
				{

					int col = 0;
					char query[8192];
					sprintf(query, "INSERT INTO player_gift SET owner_id=%d,reason='%s',`from`=replace(\"%s\",' ','_'),status='WAIT',date_add=NOW()", ch->GetPlayerID(), LC_TEXT( "SHOP_ITEM_REASON"), shop_name.c_str());
					sprintf(query, "%s, vnum='%s'", query, row[col++]);
					sprintf(query, "%s, count='%s'", query, row[col++]);
					for (int s = 0; s < ITEM_SOCKET_MAX_NUM; s++)
						sprintf(query, "%s, socket%d='%s'", query, s, row[col++]);

					for (int ia = 0; ia < ITEM_ATTRIBUTE_MAX_NUM; ia++)
					{
						if (ia < 7)
						{
							sprintf(query, "%s, attrtype%d='%s'", query, ia, row[col++]);
							sprintf(query, "%s, attrvalue%d='%s'", query, ia, row[col++]);
						}
						else
						{
							sprintf(query, "%s, applytype%d='%s'", query, ia-7,row[col++]);
							sprintf(query, "%s, applyvalue%d='%s'", query, ia-7,row[col++]);
						}
					}
					SQLMsg * pkMsg(DBManager::instance().DirectQuery(query));
					SQLResult * pRes = pkMsg->Get();
					DWORD gift_id = pRes->uiInsertID;
					if (gift_id > 0)
					{
						GetGift(ch, gift_id);
						DBManager::instance().DirectQuery("delete from player_shop_items where id='%d'", item_id);
						DBManager::instance().DirectQuery("UPDATE player_shop SET item_count=item_count-1 WHERE id = %d", shop_id);
					}
				}
			}
		}
	}

	else if (args.size() == 6)
	{
		BYTE display_pos;
		str_to_number(display_pos, args[2].c_str());

		if (display_pos < 0)
			return;
		WORD pos;
		str_to_number(pos, args[3].c_str());
		if (pos < 0)
			return;

		BYTE window_type;
		str_to_number(window_type, args[4].c_str());
		if (window_type < 0)
			return;

	#ifdef ACCE_LENNT_FIX
		if(ch->IsAcceOpen())
			Acce_close(ch);
	#endif
		long long price;

		str_to_number(price, args[5].c_str());
		if (price <= 0)
			return;
	#ifndef FULL_YANG
		{
			GoldType nTotalMoney=price;
			SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT price from player_shop_items where shop_id='%d'",shop_id));
			SQLResult * pRes = pkMsg->Get();
			if (pRes->uiNumRows>0)
			{
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(pRes->pSQLResult)) != NULL)
				{
					long long temp;
					str_to_number(temp,row[0]);
					nTotalMoney+=temp;
				}
			}
			if (GOLD_MAX <= nTotalMoney)
			{
				//sys_err("[OVERFLOW_GOLD] Overflow (GOLD_MAX) id %u name %s", ch->GetPlayerID(), ch->GetName());
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("20ld lÉR» AE°úÇDz© »óÁ^R» z­Lö°^ lr¨R´D´U"));
				do_shop_refresh_items(ch,args[1].c_str(),0,0);
				return;
			}
		}
	#endif
		LPITEM item = ch->GetItem(TItemPos(window_type, pos));
		if (item)
		{
			const TItemTable * item_table = item->GetProto();

			if (item_table && (IS_SET(item_table->dwAntiFlags, ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP)))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT( "RZ·áC­ lCRELURþ °lRÎ»óÁ^z^L­ CÇ¸LÇN Lö lr¨R´D´U."));
				do_shop_refresh_items(ch,args[1].c_str(),0,0);
				return;
			}

			if (item->IsEquipped())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT( "RlþnÁßRÎ lCRELURþ °lRÎ»óÁ^z^L­ CÇ¸LÇN Lö lr¨R´D´U."));
				do_shop_refresh_items(ch,args[1].c_str(),0,0);
				return;
			}

			if (item->isLocked())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT( "»çzëÁßRÎ lCRELURþ °lRÎ»óÁ^z^L­ CÇ¸LÇN Lö lr¨R´D´U."));
				do_shop_refresh_items(ch,args[1].c_str(),0,0);
				return;
			}
			if (item->GetOwner() != ch)
			{
				do_shop_refresh_items(ch,args[1].c_str(),0,0);
				return;
			}
#ifdef SOULBIND_SYSTEM
			if (item->IsSoulBind())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT( "You can't sell in private shop item with soul bind."));
				do_shop_refresh_items(ch,args[1].c_str(),0,0);
				return;
			}
#endif
			char query[1024];
			sprintf(query, "INSERT INTO player_shop_items SET");
			sprintf(query, "%s player_id='%d'", query, ch->GetPlayerID());
			sprintf(query, "%s, shop_id='%d'", query, shop_id);
			sprintf(query, "%s, vnum='%d'", query, item->GetVnum());
			sprintf(query, "%s, count='%d'", query, item->GetCount());
			sprintf(query, "%s, price='%lld'", query, price);

			sprintf(query, "%s, display_pos='%u'", query, display_pos);
			for (int s = 0; s < ITEM_SOCKET_MAX_NUM; s++)
			{
				sprintf(query, "%s, socket%d='%ld'", query, s, item->GetSocket(s));

			}

			for (int ia = 0; ia < ITEM_ATTRIBUTE_MAX_NUM; ia++)
			{
				const TPlayerItemAttribute& attr = item->GetAttribute(ia);
				if (ia < 7)
				{
					sprintf(query, "%s, attrtype%d='%u'", query, ia, attr.bType);
					sprintf(query, "%s, attrvalue%d='%d'", query, ia, attr.sValue);
				}
				else
				{
					sprintf(query, "%s, applytype%d='%u'", query, ia-7, attr.bType);
					sprintf(query, "%s, applyvalue%d='%d'", query, ia-7, attr.sValue);
				}
			}

			DBManager::instance().DirectQuery(query);
			ITEM_MANAGER::Instance().RemoveItem(item, "Priv shop");
			DBManager::instance().DirectQuery("UPDATE player_shop SET item_count=item_count +1 WHERE id = %d", shop_id);
		}
		else
			return;
		_UpdateOfflineShop(shop_id, false, false, true);
		do_shop_refresh_items(ch,args[1].c_str(),0,0);
	}

}

ACMD(do_shop_refresh_items)
{
	if (ch->IsObserverMode() || ch->GetExchange() || ch->IsOpenSafebox() || ch->GetShopOwner() || ch->IsCubeOpen() || ch->GetMyShop())
	{

		ch->ChatPacket(CHAT_TYPE_COMMAND, "shop_item_clear");
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't edit shop while you have opened normal shop."));
		return;
	}

	DWORD id;
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;
	str_to_number(id, arg1);
	char szSockets[1024] = { '\0' };
	char *tempSockets = szSockets;
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
	{
		tempSockets += sprintf(tempSockets, "socket%d", i);

		if (i<ITEM_SOCKET_MAX_NUM - 1)
			tempSockets += sprintf(tempSockets, ",");
	}
	char szAttrs[1024] = { '\0' };
	char *tempAttrs = szAttrs;
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; i++)
	{
		if (i < 7)
			tempAttrs += sprintf(tempAttrs, "attrtype%d,attrvalue%d", i, i);
		else
			tempAttrs += sprintf(tempAttrs, "applytype%d,applyvalue%d", i - 7, i - 7);
		if (i<ITEM_ATTRIBUTE_MAX_NUM - 1)
			tempAttrs += sprintf(tempAttrs, ",");
	}
	SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT id,vnum,count,display_pos,price,%s,%s from player_shop_items where shop_id='%d'", szSockets,szAttrs,id));

	SQLResult * pRes = pkMsg->Get();
	BYTE bItemCount = pRes->uiNumRows;
	std::vector<TShopItemTable *> map_shop;
	ch->ChatPacket(CHAT_TYPE_COMMAND, "shop_item_clear");
	if (bItemCount>0)
	{
		bItemCount = 0;
		MYSQL_ROW row;
		int c = 0;
		while ((row = mysql_fetch_row(pRes->pSQLResult)) != NULL)
		{
			int col = 5;
			char attrs[1024] = { '\0' };
			char sockets[1024] = { '\0' };
			for (int i = 0; i<ITEM_SOCKET_MAX_NUM; i++)
				sprintf(sockets, "%s%s%s", sockets, row[col++], (i<ITEM_SOCKET_MAX_NUM-1 ? "|" : ""));
			//col--;

			for (int i = 0; i<ITEM_ATTRIBUTE_MAX_NUM; i++)
				sprintf(attrs, "%s%s,%s%s", attrs, row[col++], row[col++], (i<ITEM_ATTRIBUTE_MAX_NUM-1 ? "|" : ""));
			ch->ChatPacket(CHAT_TYPE_COMMAND, "shop_item %s#%s#%s#%s#%s#%s#%s", row[0], row[1], row[2], row[3], row[4], sockets, attrs);
		}
	}

}
ACMD(do_shop_update)
{
	if (ch->IsObserverMode() || ch->GetExchange())
		return;

	DWORD id;
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;
	str_to_number(id, arg1);
	if (*arg1)
	{
		SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT player_id from player_shop WHERE id = %d", id));
		SQLResult * pRes = pkMsg->Get();
		if (pRes->uiNumRows>0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Shop %s has been updated", arg1);
			CharacterVectorInteractor i;
			if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(30000, i))
			{
				CharacterVectorInteractor::iterator it = i.begin();

				while (it != i.end()) {
					LPCHARACTER pc = *it++;
					if (pc)
						if (pc->GetRaceNum() == 30000 && pc->GetPrivShop() == id) {
							pc->UpdateShopItems();
							return;
						}


				}
			}
			TPacketShopUpdateItem packet;
			packet.shop_id = id;
			db_clientdesc->DBPacket(HEADER_GD_SHOP_UPDATE_ITEM, 0, &packet, sizeof(packet));
		}
		else
			ch->ChatPacket(CHAT_TYPE_INFO, "Shop %s does exists", arg1);
	}
}
ACMD(do_shop_delete)
{
	if (ch->IsObserverMode() || ch->GetExchange())
		return;
	char arg1[256];
	char arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	if (!*arg1 || !*arg2)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage:");
		ch->ChatPacket(CHAT_TYPE_INFO, "/delete_shop <type> <arg> ");
		ch->ChatPacket(CHAT_TYPE_INFO, "Types:");
		ch->ChatPacket(CHAT_TYPE_INFO, "		shopid - Delete shop using ID");
		ch->ChatPacket(CHAT_TYPE_INFO, "		player - Delete all player shops by player name");
		ch->ChatPacket(CHAT_TYPE_INFO, "Example:");
		ch->ChatPacket(CHAT_TYPE_INFO, "		/delete_shop player Best4ever");
		ch->ChatPacket(CHAT_TYPE_INFO, "		/delete_shop shopid 1");
		return;
	}
	if (!strcmp(arg1, "player"))
	{
		SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT id from player_shop WHERE player_id=(select id from player where name='%s')", arg2));
		SQLResult * pRes = pkMsg->Get();
		if (pRes->uiNumRows>0)
		{
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(pRes->pSQLResult)) != NULL)
			{
				DWORD id;
				str_to_number(id, row[0]);
				DeleteShop(id);
			}
		}
		else
			ch->ChatPacket(CHAT_TYPE_INFO, "Player %s does have any shop", arg2);
	}
	if (!strcmp(arg1, "shopid"))
	{
		SQLMsg * pkMsg(DBManager::instance().DirectQuery("SELECT id from player_shop WHERE id='%s'", arg2));
		SQLResult * pRes = pkMsg->Get();
		if (pRes->uiNumRows>0)
		{
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(pRes->pSQLResult)) != NULL)
			{
				DWORD id;
				str_to_number(id, arg2);
				DeleteShop(id);
			}
		}
		else
			ch->ChatPacket(CHAT_TYPE_INFO, "Shop %s does exists", arg2);
	}
}
#else

ACMD(do_close_shop)
{
	if (ch->IsObserverMode())
		return;
	if (ch->GetMyShop())
	{
		ch->CloseMyShop();
		return;
	}
}
#endif

ACMD(do_set_walk_mode)
{
	ch->SetNowWalking(true);
	ch->SetWalking(true);
}

ACMD(do_set_run_mode)
{
	ch->SetNowWalking(false);
	ch->SetWalking(false);
}

#ifdef ENABLE_AFFECT_POLYMORPH_REMOVE
ACMD(do_remove_polymorph)
{
	if (!ch)
		return;
	
	if (!ch->IsPolymorphed())
		return;
	
	ch->SetPolymorph(0);
	ch->RemoveAffect(AFFECT_POLYMORPH);
}
#endif

ACMD(do_war)
{
	//³» ±æµå Á¤º¸¸¦ ¾ò¾î¿À°í
	CGuild * g = ch->GetGuild();

	if (!g)
		return;

	//ÀüÀïÁßÀÎÁö Ã¼Å©ÇÑ¹ø!
	if (g->UnderAnyWar())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> Your Guild is already participating in another war.");
		return;
	}

	//ÆÄ¶ó¸ÞÅÍ¸¦ µÎ¹è·Î ³ª´©°í
	char arg1[256], arg2[256];
	DWORD type = GUILD_WAR_TYPE_FIELD; //fixme102 base int modded uint
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
		return;

	if (*arg2)
	{
		str_to_number(type, arg2);

		if (type >= GUILD_WAR_TYPE_MAX_NUM)
			type = GUILD_WAR_TYPE_FIELD;
	}

	//±æµåÀÇ ¸¶½ºÅÍ ¾ÆÀÌµð¸¦ ¾ò¾î¿ÂµÚ
	DWORD gm_pid = g->GetMasterPID();

	//¸¶½ºÅÍÀÎÁö Ã¼Å©(±æÀüÀº ±æµåÀå¸¸ÀÌ °¡´É)
	if (gm_pid != ch->GetPlayerID())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> You are not entitled to declare a Guild war.");
		return;
	}

	//»ó´ë ±æµå¸¦ ¾ò¾î¿À°í
	CGuild * opp_g = CGuildManager::instance().FindGuildByName(arg1);

	if (!opp_g)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> This Guild does not exist.");
		return;
	}

	//»ó´ë±æµå¿ÍÀÇ »óÅÂ Ã¼Å©
	switch (g->GetGuildWarState(opp_g->GetID()))
	{
		case GUILD_WAR_NONE:
			{
				if (opp_g->UnderAnyWar())
				{
					ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> This Guild is already participating in a war.");
					return;
				}

				int iWarPrice = KOR_aGuildWarInfo[type].iWarPrice;

				if (g->GetGuildMoney() < iWarPrice)
				{
					ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> There is not enough Yang to participate in a Guild war.");
					return;
				}

				if (opp_g->GetGuildMoney() < iWarPrice)
				{
					ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> The Guild does not have enough Yang to participate in a Guild war.");
					return;
				}
			}
			break;

		case GUILD_WAR_SEND_DECLARE:
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "You already declared war on this Guild.");
				return;
			}
			break;

		case GUILD_WAR_RECV_DECLARE:
			{
				if (opp_g->UnderAnyWar())
				{
					ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> This Guild is already participating in a war.");
					g->RequestRefuseWar(opp_g->GetID());
					return;
				}
			}
			break;

		case GUILD_WAR_RESERVE:
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> This Guild is already noted for another Guild war.");
				return;
			}
			break;

		case GUILD_WAR_END:
			return;

		default:
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> This Guild is already participating in another war.");
			g->RequestRefuseWar(opp_g->GetID());
			return;
	}

	if (!g->CanStartWar(type))
	{
		// ±æµåÀüÀ» ÇÒ ¼ö ÀÖ´Â Á¶°ÇÀ» ¸¸Á·ÇÏÁö¾Ê´Â´Ù.
		if (g->GetLadderPoint() == 0)
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> Rank is too low to participate in a Guild war.");
			sys_log(0, "GuildWar.StartError.NEED_LADDER_POINT");
		}
		else if (g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> At least %d players have to participate in a Guild war.", GUILD_WAR_MIN_MEMBER_COUNT);
			sys_log(0, "GuildWar.StartError.NEED_MINIMUM_MEMBER[%d]", GUILD_WAR_MIN_MEMBER_COUNT);
		}
		else
		{
			sys_log(0, "GuildWar.StartError.UNKNOWN_ERROR");
		}
		return;
	}

	// ÇÊµåÀü Ã¼Å©¸¸ ÇÏ°í ¼¼¼¼ÇÑ Ã¼Å©´Â »ó´ë¹æÀÌ ½Â³«ÇÒ¶§ ÇÑ´Ù.
	if (!opp_g->CanStartWar(GUILD_WAR_TYPE_FIELD))
	{
		if (opp_g->GetLadderPoint() == 0)
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> The Guild does not have enough Rank Points to participate in a Guild war.");
		else if (opp_g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> The Guild does not have enough members to participate in a Guild war.");
		return;
	}

	do
	{
		if (g->GetMasterCharacter() != NULL)
			break;

		CCI *pCCI = P2P_MANAGER::instance().FindByPID(g->GetMasterPID());

		if (pCCI != NULL)
			break;

		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> The Guild master is not online.");
		g->RequestRefuseWar(opp_g->GetID());
		return;

	} while (false);

	do
	{
		if (opp_g->GetMasterCharacter() != NULL)
			break;

		CCI *pCCI = P2P_MANAGER::instance().FindByPID(opp_g->GetMasterPID());

		if (pCCI != NULL)
			break;

		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> The Guild master is not online.");
		g->RequestRefuseWar(opp_g->GetID());
		return;

	} while (false);

	g->RequestDeclareWar(opp_g->GetID(), type);
}

ACMD(do_nowar)
{
	CGuild* g = ch->GetGuild();
	if (!g)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD gm_pid = g->GetMasterPID();

	if (gm_pid != ch->GetPlayerID())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> You are not entitled to declare a Guild war.");
		return;
	}

	CGuild* opp_g = CGuildManager::instance().FindGuildByName(arg1);

	if (!opp_g)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "<Guild> This Guild does not exist.");
		return;
	}

	g->RequestRefuseWar(opp_g->GetID());
}

ACMD(do_detaillog)
{
	ch->DetailLog();
}

ACMD(do_monsterlog)
{
	ch->ToggleMonsterLog();
}

ACMD(do_pkmode)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	BYTE mode = 0;
	str_to_number(mode, arg1);

	if (mode == PK_MODE_PROTECT)
		return;

	if (ch->GetLevel() < PK_PROTECT_LEVEL && mode != 0)
		return;

	ch->SetPKMode(mode);
}

ACMD(do_messenger_auth)
{
	if (ch->GetArena())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You can't use this in the arena.");
		return;
	}

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	char answer = LOWER(*arg1);
	// @fixme130 AuthToAdd void -> bool
	bool bIsDenied = answer != 'y';
	bool bIsAdded = MessengerManager::instance().AuthToAdd(ch->GetName(), arg2, bIsDenied); // DENY
	if (bIsAdded && bIsDenied)
	{
		LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg2);

		if (tch)
			tch->ChatPacketTrans(CHAT_TYPE_INFO, "%s declined the invitation.", ch->GetName());
	}

}

ACMD(do_setblockmode)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		BYTE flag = 0;
		str_to_number(flag, arg1);
		ch->SetBlockMode(flag);
	}
}

ACMD(do_unmount)
{
	if (true == ch->UnEquipSpecialRideUniqueItem())
	{
		ch->RemoveAffect(AFFECT_MOUNT);
		ch->RemoveAffect(AFFECT_MOUNT_BONUS);

		if (ch->IsHorseRiding())
		{
			ch->StopRiding();
		}
	}
	else
	{
		ch->ChatPacket( CHAT_TYPE_INFO, LC_TEXT("Your inventory is full."));
	}

}

ACMD(do_observer_exit)
{
	if (ch->IsObserverMode())
	{
		if (ch->GetWarMap())
			ch->SetWarMap(NULL);

		if (ch->GetArena() != NULL || ch->GetArenaObserverMode() == true)
		{
			ch->SetArenaObserverMode(false);

			if (ch->GetArena() != NULL)
				ch->GetArena()->RemoveObserver(ch->GetPlayerID());

			ch->SetArena(NULL);
			ch->WarpSet(ARENA_RETURN_POINT_X(ch->GetEmpire()), ARENA_RETURN_POINT_Y(ch->GetEmpire()));
		}
		else
		{
			ch->ExitToSavedLocation();
		}
		ch->SetObserverMode(false);
	}
}

ACMD(do_view_equip)
{
	if (ch->GetGMLevel() <= GM_PLAYER)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		DWORD vid = 0;
		str_to_number(vid, arg1);
		LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

		if (!tch)
			return;

		if (!tch->IsPC())
			return;
		/*
		   int iSPCost = ch->GetMaxSP() / 3;

		   if (ch->GetSP() < iSPCost)
		   {
		   ch->ChatPacketTrans(CHAT_TYPE_INFO, "Not enough mana to see other's equipments.");
		   return;
		   }
		   ch->PointChange(POINT_SP, -iSPCost);
		 */
		tch->SendEquipment(ch);
	}
}

ACMD(do_party_request)
{
	if (ch->GetArena())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You can't use this in the arena.");
		return;
	}

	if (ch->GetParty())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot accept the invitation because you are already in the Group.");
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		if (!ch->RequestToParty(tch))
			ch->ChatPacket(CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

ACMD(do_party_request_accept)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		ch->AcceptToParty(tch);
}

ACMD(do_party_request_deny)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		ch->DenyToParty(tch);
}

ACMD(do_monarch_warpto)
{
	if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Only the Emperor can use this.");
		return;
	}

	//±ºÁÖ ÄðÅ¸ÀÓ °Ë»ç
	if (!ch->IsMCOK(CHARACTER::MI_WARP))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Cool Down Time roughly %d seconds.", ch->GetMCLTime(CHARACTER::MI_WARP));
		return;
	}

	//±ºÁÖ ¸÷ ¼ÒÈ¯ ºñ¿ë
	const int WarpPrice = 10000;

	//±ºÁÖ ±¹°í °Ë»ç
	if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Lack of Taxes. Current Capital : %u Needed Capital : %u", NationMoney, WarpPrice);
		return;
	}

	int x = 0, y = 0;
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Order: warp to <character name>");
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(arg1);

		if (pkCCI)
		{
			if (pkCCI->bEmpire != ch->GetEmpire())
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot teleport to an enemy player.");
				return;
			}

			if (pkCCI->bChannel != g_bChannel)
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "Adding of player %d into the Channel. (Present Channel %d)", pkCCI->bChannel, g_bChannel);
				return;
			}
			if (!IsMonarchWarpZone(pkCCI->lMapIndex))
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "ÇØ´ç Áö¿ªÀ¸·Î ÀÌµ¿ÇÒ ¼ö ¾ø½À´Ï´Ù.");
				return;
			}

			PIXEL_POSITION pos;

			if (!SECTREE_MANAGER::instance().GetCenterPositionOfMap(pkCCI->lMapIndex, pos))
				ch->ChatPacket(CHAT_TYPE_INFO, "Cannot find map (index %d)", pkCCI->lMapIndex);
			else
			{
				//ch->ChatPacket(CHAT_TYPE_INFO, "You warp to (%d, %d)", pos.x, pos.y);
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "Transport to %s.", arg1);
				ch->WarpSet(pos.x, pos.y);

				//±ºÁÖ µ· »è°¨
				CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

				//ÄðÅ¸ÀÓ ÃÊ±âÈ­
				ch->SetMC(CHARACTER::MI_WARP);
			}
		}
		else if (NULL == CHARACTER_MANAGER::instance().FindPC(arg1))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "There is no one by that name");
		}

		return;
	}
	else
	{
		if (tch->GetEmpire() != ch->GetEmpire())
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot teleport to an enemy player.");
			return;
		}
		if (!IsMonarchWarpZone(tch->GetMapIndex()))
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "ÇØ´ç Áö¿ªÀ¸·Î ÀÌµ¿ÇÒ ¼ö ¾ø½À´Ï´Ù.");
			return;
		}
		x = tch->GetX();
		y = tch->GetY();
	}

	ch->ChatPacketTrans(CHAT_TYPE_INFO, "Transport to %s.", arg1);
	ch->WarpSet(x, y);
	ch->Stop();

	//±ºÁÖ µ· »è°¨
	CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

	//ÄðÅ¸ÀÓ ÃÊ±âÈ­
	ch->SetMC(CHARACTER::MI_WARP);
}

ACMD(do_monarch_transfer)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "USE: transfer <name>");
		return;
	}

	if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Only the Emperor can use this.");
		return;
	}

	//±ºÁÖ ÄðÅ¸ÀÓ °Ë»ç
	if (!ch->IsMCOK(CHARACTER::MI_TRANSFER))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Cool Down Time roughly %d seconds.", ch->GetMCLTime(CHARACTER::MI_TRANSFER));
		return;
	}

	//±ºÁÖ ¿öÇÁ ºñ¿ë
	const int WarpPrice = 10000;

	//±ºÁÖ ±¹°í °Ë»ç
	if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Lack of Taxes. Current Capital : %u Needed Capital : %u", NationMoney, WarpPrice);
		return;
	}


	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(arg1);

		if (pkCCI)
		{
			if (pkCCI->bEmpire != ch->GetEmpire())
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot call players from other kingdoms.");
				return;
			}
			if (pkCCI->bChannel != g_bChannel)
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "%s is connecting %d channel. (current channel:%d)", arg1, pkCCI->bChannel, g_bChannel);
				return;
			}
			if (!IsMonarchWarpZone(pkCCI->lMapIndex))
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "ÇØ´ç Áö¿ªÀ¸·Î ÀÌµ¿ÇÒ ¼ö ¾ø½À´Ï´Ù.");
				return;
			}
			if (!IsMonarchWarpZone(ch->GetMapIndex()))
			{
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "ÇØ´ç Áö¿ªÀ¸·Î ¼ÒÈ¯ÇÒ ¼ö ¾ø½À´Ï´Ù.");
				return;
			}

			TPacketGGTransfer pgg;

			pgg.bHeader = HEADER_GG_TRANSFER;
			strlcpy(pgg.szName, arg1, sizeof(pgg.szName));
			pgg.lX = ch->GetX();
			pgg.lY = ch->GetY();

			P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGTransfer));
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "%s is called.", arg1);

			//±ºÁÖ µ· »è°¨
			CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);
			//ÄðÅ¸ÀÓ ÃÊ±âÈ­
			ch->SetMC(CHARACTER::MI_TRANSFER);
		}
		else
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "Cannot find the player.");
		}

		return;
	}


	if (ch == tch)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot call yourself.");
		return;
	}

	if (tch->GetEmpire() != ch->GetEmpire())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot call players from other kingdoms.");
		return;
	}
	if (!IsMonarchWarpZone(tch->GetMapIndex()))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "ÇØ´ç Áö¿ªÀ¸·Î ÀÌµ¿ÇÒ ¼ö ¾ø½À´Ï´Ù.");
		return;
	}
	if (!IsMonarchWarpZone(ch->GetMapIndex()))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "ÇØ´ç Áö¿ªÀ¸·Î ¼ÒÈ¯ÇÒ ¼ö ¾ø½À´Ï´Ù.");
		return;
	}

	//tch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY(), ch->GetZ());
	tch->WarpSet(ch->GetX(), ch->GetY(), ch->GetMapIndex());

	//±ºÁÖ µ· »è°¨
	CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);
	//ÄðÅ¸ÀÓ ÃÊ±âÈ­
	ch->SetMC(CHARACTER::MI_TRANSFER);
}

ACMD(do_monarch_info)
{
	if (CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "My Emperor Information");
		TMonarchInfo * p = CMonarch::instance().GetMonarch();
		for (int n = 1; n < 4; ++n)
		{
			if (n == ch->GetEmpire())
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "[%s] Monarch : %s Vault : %lld Yang", EMPIRE_NAME(n), p->name[n], p->money[n]);
			else
				ch->ChatPacketTrans(CHAT_TYPE_INFO, "[%sLord] : %s ", EMPIRE_NAME(n), p->name[n]);

		}
	}
	else
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Emperor Information");
		TMonarchInfo * p = CMonarch::instance().GetMonarch();
		for (int n = 1; n < 4; ++n)
		{
			ch->ChatPacketTrans(CHAT_TYPE_INFO, "[%sLord] : %s ", EMPIRE_NAME(n), p->name[n]);

		}
	}

}

ACMD(do_elect)
{
	db_clientdesc->DBPacketHeader(HEADER_GD_COME_TO_VOTE, ch->GetDesc()->GetHandle(), 0);
}

// LUA_ADD_GOTO_INFO
struct GotoInfo
{
	std::string 	st_name;

	BYTE 	empire;
	int 	mapIndex;
	DWORD 	x, y;

	GotoInfo()
	{
		st_name 	= "";
		empire 		= 0;
		mapIndex 	= 0;

		x = 0;
		y = 0;
	}

	GotoInfo(const GotoInfo& c_src)
	{
		__copy__(c_src);
	}

	void operator = (const GotoInfo& c_src)
	{
		__copy__(c_src);
	}

	void __copy__(const GotoInfo& c_src)
	{
		st_name 	= c_src.st_name;
		empire 		= c_src.empire;
		mapIndex 	= c_src.mapIndex;

		x = c_src.x;
		y = c_src.y;
	}
};

ACMD(do_monarch_tax)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: monarch_tax <1-50>");
		return;
	}

	// ±ºÁÖ °Ë»ç
	if (!ch->IsMonarch())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Only the Emperor can use this.");
		return;
	}

	// ¼¼±Ý¼³Á¤
	int tax = 0;
	str_to_number(tax,  arg1);

	if (tax < 1 || tax > 50)
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Choose a number between 1 - 50.");

	quest::CQuestManager::instance().SetEventFlag("trade_tax", tax);

	// ±ºÁÖ¿¡°Ô ¸Þ¼¼Áö ÇÏ³ª
	ch->ChatPacketTrans(CHAT_TYPE_INFO, "Taxes are set to %d%%.");

	// °øÁö
	char szMsg[1024];

	snprintf(szMsg, sizeof(szMsg), "±ºÁÖÀÇ ¸íÀ¸·Î ¼¼±ÝÀÌ %d %% ·Î º¯°æµÇ¾ú½À´Ï´Ù", tax);
	BroadcastNotice(szMsg);

	snprintf(szMsg, sizeof(szMsg), "¾ÕÀ¸·Î´Â °Å·¡ ±Ý¾×ÀÇ %d %% °¡ ±¹°í·Î µé¾î°¡°ÔµË´Ï´Ù.", tax);
	BroadcastNotice(szMsg);

	// ÄðÅ¸ÀÓ ÃÊ±âÈ­
	ch->SetMC(CHARACTER::MI_TAX);
}

static const DWORD cs_dwMonarchMobVnums[] =
{
	191, //	»ê°ß½Å
	192, //	Àú½Å
	193, //	¿õ½Å
	194, //	È£½Å
	391, //	¹ÌÁ¤
	392, //	ÀºÁ¤
	393, //	¼¼¶û
	394, //	ÁøÈñ
	491, //	¸ÍÈ¯
	492, //	º¸¿ì
	493, //	±¸ÆÐ
	494, //	ÃßÈç
	591, //	ºñ·ù´Ü´ëÀå
	691, //	¿õ±Í Á·Àå
	791, //	¹Ð±³±³ÁÖ
	1304, // ´©··¹ü±Í
	1901, // ±¸¹ÌÈ£
	2091, // ¿©¿Õ°Å¹Ì
	2191, // °Å´ë»ç¸·°ÅºÏ
	2206, // È­¿°¿Õi
	0,
};

ACMD(do_monarch_mob)
{
	char arg1[256];
	LPCHARACTER	tch;

	one_argument(argument, arg1, sizeof(arg1));

	if (!ch->IsMonarch())
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Only the Emperor can use this.");
		return;
	}

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: mmob <mob name>");
		return;
	}

#ifdef ENABLE_MONARCH_MOB_CMD_MAP_CHECK // @warme006
	BYTE pcEmpire = ch->GetEmpire();
	BYTE mapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(ch->GetMapIndex());
	if (mapEmpire != pcEmpire && mapEmpire != 0)
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You can use this skill only in your land.");
		return;
	}
#endif

	// ±ºÁÖ ¸÷ ¼ÒÈ¯ ºñ¿ë
	const int SummonPrice = 5000000;

	// ±ºÁÖ ÄðÅ¸ÀÓ °Ë»ç
	if (!ch->IsMCOK(CHARACTER::MI_SUMMON))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Cool Down Time roughly %d seconds.", ch->GetMCLTime(CHARACTER::MI_SUMMON));
		return;
	}

	// ±ºÁÖ ±¹°í °Ë»ç
	if (!CMonarch::instance().IsMoneyOk(SummonPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "Lack of Taxes. Current Capital : %u Needed Capital : %u", NationMoney, SummonPrice);
		return;
	}

	const CMob * pkMob;
	DWORD vnum = 0;

	if (isdigit(*arg1))
	{
		str_to_number(vnum, arg1);

		if ((pkMob = CMobManager::instance().Get(vnum)) == NULL)
			vnum = 0;
	}
	else
	{
		pkMob = CMobManager::Instance().Get(arg1, true);

		if (pkMob)
			vnum = pkMob->m_table.dwVnum;
	}

	DWORD count;

	// ¼ÒÈ¯ °¡´É ¸÷ °Ë»ç
	for (count = 0; cs_dwMonarchMobVnums[count] != 0; ++count)
		if (cs_dwMonarchMobVnums[count] == vnum)
			break;

	if (0 == cs_dwMonarchMobVnums[count])
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "The Monster cannot be called. Check the Mob Number.");
		return;
	}

	tch = CHARACTER_MANAGER::instance().SpawnMobRange(vnum,
			ch->GetMapIndex(),
			ch->GetX() - number(200, 750),
			ch->GetY() - number(200, 750),
			ch->GetX() + number(200, 750),
			ch->GetY() + number(200, 750),
			true,
			pkMob->m_table.bType == CHAR_TYPE_STONE,
			true);

	if (tch)
	{
		// ±ºÁÖ µ· »è°¨
		CMonarch::instance().SendtoDBDecMoney(SummonPrice, ch->GetEmpire(), ch);

		// ÄðÅ¸ÀÓ ÃÊ±âÈ­
		ch->SetMC(CHARACTER::MI_SUMMON);
	}
}

static const char* FN_point_string(int apply_number)
{
	switch (apply_number)
	{
		case POINT_MAX_HP:	return LC_TEXT("Energy Points +%d");
		case POINT_MAX_SP:	return LC_TEXT("Mana Points +%d");
		case POINT_HT:		return LC_TEXT("Endurance +%d");
		case POINT_IQ:		return LC_TEXT("Intelligence +%d");
		case POINT_ST:		return LC_TEXT("Strength +%d");
		case POINT_DX:		return LC_TEXT("Agility +%d");
		case POINT_ATT_SPEED:	return LC_TEXT("Attack Speed +%d");
		case POINT_MOV_SPEED:	return LC_TEXT("Moving Speed %d");
		case POINT_CASTING_SPEED:	return LC_TEXT("Cooldown Time -%d");
		case POINT_HP_REGEN:	return LC_TEXT("Energy Recovery +%d");
		case POINT_SP_REGEN:	return LC_TEXT("Spell Point Recovery +%d");
		case POINT_POISON_PCT:	return LC_TEXT("Poison Attack %d");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_BLEEDING_PCT:	return LC_TEXT("Poison Attack %d");
#endif
		case POINT_STUN_PCT:	return LC_TEXT("Stun +%d");
		case POINT_SLOW_PCT:	return LC_TEXT("Speed Reducing +%d");
		case POINT_CRITICAL_PCT:	return LC_TEXT("Critical Attack with a chance of %d%%");
		case POINT_RESIST_CRITICAL:	return LC_TEXT("»ó´ëÀÇ Ä¡¸íÅ¸ È®·ü %d%% °¨¼Ò");
		case POINT_PENETRATE_PCT:	return LC_TEXT("%d%% chance to thrusting attack");
		case POINT_RESIST_PENETRATE: return LC_TEXT("»ó´ëÀÇ °üÅë °ø°Ý È®·ü %d%% °¨¼Ò");
		case POINT_ATTBONUS_HUMAN:	return LC_TEXT("Player Attack Power against Monster +%d%%");
		case POINT_ATTBONUS_ANIMAL:	return LC_TEXT("Horse Attack Power against Monster +%d%%");
		case POINT_ATTBONUS_ORC:	return LC_TEXT("Attack Boost against Orc + %d%%");
		case POINT_ATTBONUS_MILGYO:	return LC_TEXT("Attack Boost against Esoteric + %d%%");
		case POINT_ATTBONUS_UNDEAD:	return LC_TEXT("Attack Boost against Undead + %d%%");
		case POINT_ATTBONUS_DEVIL:	return LC_TEXT("Attack Boost against Devil + %d%%");
		case POINT_STEAL_HP:		return LC_TEXT("Absorbing of HP %d%% while attacking.");
		case POINT_STEAL_SP:		return LC_TEXT("Absorbing of Mana %d%% while attacking.");
		case POINT_MANA_BURN_PCT:	return LC_TEXT("With a chance of %d%% Mana will be taken from the enemy.");
		case POINT_DAMAGE_SP_RECOVER:	return LC_TEXT("Absorbing of Spell Points with a chance of %d%% .");
		case POINT_BLOCK:			return LC_TEXT("%d%% Chance to block a Close Combat Attack.");
		case POINT_DODGE:			return LC_TEXT("To block a Distance Attack there is a chance of %d%%");
		case POINT_RESIST_SWORD:	return LC_TEXT("Sword Defence %d%%");
		case POINT_RESIST_TWOHAND:	return LC_TEXT("Two-Handed Sword Defence %d%%");
		case POINT_RESIST_DAGGER:	return LC_TEXT("Two-Handed Sword Defence %d%%");
		case POINT_RESIST_BELL:		return LC_TEXT("Bell Defence %d%%");
		case POINT_RESIST_FAN:		return LC_TEXT("Fan Defence %d%%");
		case POINT_RESIST_BOW:		return LC_TEXT("Distant Attack Resistance %d%%");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_RESIST_CLAW:		return LC_TEXT("Two-Handed Sword Defence %d%%");
#endif
		case POINT_RESIST_FIRE:		return LC_TEXT("Fire Resistance %d%%");
		case POINT_RESIST_ELEC:		return LC_TEXT("Lightning Resistance %d%%");
		case POINT_RESIST_MAGIC:	return LC_TEXT("Magic Resistance %d%%");
#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
		case POINT_RESIST_MAGIC_REDUCTION:	return LC_TEXT("Magic Resistance %d%%");
#endif
		case POINT_RESIST_WIND:		return LC_TEXT("Wind Resistance %d%%");
		case POINT_RESIST_ICE:		return LC_TEXT("³Ã±â ÀúÇ× %d%%");
		case POINT_RESIST_EARTH:	return LC_TEXT("´ëÁö ÀúÇ× %d%%");
		case POINT_RESIST_DARK:		return LC_TEXT("¾îµÒ ÀúÇ× %d%%");
		case POINT_REFLECT_MELEE:	return LC_TEXT("Reflect Direct Hit: %d%%");
		case POINT_REFLECT_CURSE:	return LC_TEXT("Reflect Curse: %d%%");
		case POINT_POISON_REDUCE:	return LC_TEXT("Poison Resistance %d%%");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_BLEEDING_REDUCE:	return LC_TEXT("Poison Resistance %d%%");
#endif
		case POINT_KILL_SP_RECOVER:	return LC_TEXT("Spell Points will be increased up to %d%% if you win.");
		case POINT_EXP_DOUBLE_BONUS:	return LC_TEXT("Experience increases up to %d%% if you win.");
		case POINT_GOLD_DOUBLE_BONUS:	return LC_TEXT("Increase of Yang up to %d%% if you win");
		case POINT_ITEM_DROP_BONUS:	return LC_TEXT("Increase of captured Items up to %d%% if you win.");
		case POINT_POTION_BONUS:	return LC_TEXT("Increase of Power up to %d%% when taking the Potion.");
		case POINT_KILL_HP_RECOVERY:	return LC_TEXT("%d%% Chance to fill up Energy Points after you won.");
//		case POINT_IMMUNE_STUN:	return LC_TEXT("No Dizzyness %d%%");
//		case POINT_IMMUNE_SLOW:	return LC_TEXT("No Slowing Down %d%%");
//		case POINT_IMMUNE_FALL:	return LC_TEXT("No Falling Down %d%%");
//		case POINT_SKILL:	return LC_TEXT("");
//		case POINT_BOW_DISTANCE:	return LC_TEXT("");
		case POINT_ATT_GRADE_BONUS:	return LC_TEXT("Attack Power + %d");
		case POINT_DEF_GRADE_BONUS:	return LC_TEXT("Armour + %d");
		case POINT_MAGIC_ATT_GRADE:	return LC_TEXT("Magical Attack + %d");
		case POINT_MAGIC_DEF_GRADE:	return LC_TEXT("Magical Defence + %d");
//		case POINT_CURSE_PCT:	return LC_TEXT("");
		case POINT_MAX_STAMINA:	return LC_TEXT("Maximum Endurance + %d");
		case POINT_ATTBONUS_WARRIOR:	return LC_TEXT("Strong against Warriors + %d%%");
		case POINT_ATTBONUS_ASSASSIN:	return LC_TEXT("Strong against Ninja + %d%%");
		case POINT_ATTBONUS_SURA:		return LC_TEXT("Strong against Sura + %d%%");
		case POINT_ATTBONUS_SHAMAN:		return LC_TEXT("Strong against Mages + %d%%");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_ATTBONUS_WOLFMAN:	return LC_TEXT("Strong against Mages + %d%%");
#endif
		case POINT_ATTBONUS_MONSTER:	return LC_TEXT("Strong against Monster + %d%%");
		case POINT_MALL_ATTBONUS:		return LC_TEXT("Attack + %d%%");
		case POINT_MALL_DEFBONUS:		return LC_TEXT("Defence + %d%%");
		case POINT_MALL_EXPBONUS:		return LC_TEXT("Experience %d%%");
		case POINT_MALL_ITEMBONUS:		return LC_TEXT("Chance to find an Item %. 1f");
		case POINT_MALL_GOLDBONUS:		return LC_TEXT("Chance to find Yang %. 1f");
		case POINT_MAX_HP_PCT:			return LC_TEXT("Energy Upper Limit +%d%%");
		case POINT_MAX_SP_PCT:			return LC_TEXT("Energy Upper Limit +%d%%");
		case POINT_SKILL_DAMAGE_BONUS:	return LC_TEXT("Skill Damage %d%%");
		case POINT_NORMAL_HIT_DAMAGE_BONUS:	return LC_TEXT("Hit Damage %d%%");
		case POINT_SKILL_DEFEND_BONUS:		return LC_TEXT("Resistance against Skill Damage %d%%");
		case POINT_NORMAL_HIT_DEFEND_BONUS:	return LC_TEXT("Resistance against Hits %d%%");
//		case POINT_PC_BANG_EXP_BONUS:	return LC_TEXT("");
//		case POINT_PC_BANG_DROP_BONUS:	return LC_TEXT("");
//		case POINT_EXTRACT_HP_PCT:	return LC_TEXT("");
		case POINT_RESIST_WARRIOR:	return LC_TEXT("%d%% Resistance against Warrior Attacks");
		case POINT_RESIST_ASSASSIN:	return LC_TEXT("%d%% Resistance against Ninja Attacks");
		case POINT_RESIST_SURA:		return LC_TEXT("%d%% Resistance against Sura Attacks");
		case POINT_RESIST_SHAMAN:	return LC_TEXT("%d%% Resistance against Mage Attacks");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_RESIST_WOLFMAN:	return LC_TEXT("%d%% Resistance against Mage Attacks");
#endif
		default:					return NULL;
	}
}

static bool FN_hair_affect_string(LPCHARACTER ch, char *buf, size_t bufsiz)
{
	if (NULL == ch || NULL == buf)
		return false;

	CAffect* aff = NULL;
	time_t expire = 0;
	struct tm ltm;
	int	year, mon, day;
	int	offset = 0;

	aff = ch->FindAffect(AFFECT_HAIR);

	if (NULL == aff)
		return false;

	expire = ch->GetQuestFlag("hair.limit_time");

	if (expire < get_global_time())
		return false;

	// set apply string
	offset = snprintf(buf, bufsiz, FN_point_string(aff->bApplyOn), aff->lApplyValue);

	if (offset < 0 || offset >= (int) bufsiz)
		offset = bufsiz - 1;

	localtime_r(&expire, &ltm);

	year	= ltm.tm_year + 1900;
	mon		= ltm.tm_mon + 1;
	day		= ltm.tm_mday;

	snprintf(buf + offset, bufsiz - offset, LC_TEXT("(Procedure: %d y- %d m - %d d)"), year, mon, day);

	return true;
}

ACMD(do_costume)
{
	char buf[512];
	const size_t bufferSize = sizeof(buf);

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	CItem* pBody = ch->GetWear(WEAR_COSTUME_BODY);
	CItem* pHair = ch->GetWear(WEAR_COSTUME_HAIR);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	CItem* pMount = ch->GetWear(WEAR_COSTUME_MOUNT);
#endif
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	CItem* pAcce = ch->GetWear(WEAR_COSTUME_ACCE);
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	CItem* pWeapon = ch->GetWear(WEAR_COSTUME_WEAPON);
#endif

	ch->ChatPacket(CHAT_TYPE_INFO, "COSTUME status:");

	if (pHair)
	{
		const char* itemName = pHair->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  HAIR : %s", itemName);

		for (int i = 0; i < pHair->GetAttributeCount(); ++i)
		{
			const TPlayerItemAttribute& attr = pHair->GetAttribute(i);
			if (0 < attr.bType)
			{
				snprintf(buf, bufferSize, FN_point_string(attr.bType), attr.sValue);
				ch->ChatPacket(CHAT_TYPE_INFO, "     %s", buf);
			}
		}

		if (pHair->IsEquipped() && arg1[0] == 'h')
			ch->UnequipItem(pHair);
	}

	if (pBody)
	{
		const char* itemName = pBody->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  BODY : %s", itemName);

		if (pBody->IsEquipped() && arg1[0] == 'b')
			ch->UnequipItem(pBody);
	}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (pMount)
	{
		const char* itemName = pMount->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  MOUNT : %s", itemName);

		if (pMount->IsEquipped() && arg1[0] == 'm')
			ch->UnequipItem(pMount);
	}
#endif

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	if (pAcce)
	{
		const char* itemName = pAcce->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  ACCE : %s", itemName);

		if (pAcce->IsEquipped() && arg1[0] == 'a')
			ch->UnequipItem(pMount);
	}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	if (pWeapon)
	{
		const char* itemName = pWeapon->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  WEAPON : %s", itemName);

		if (pWeapon->IsEquipped() && arg1[0] == 'w')
			ch->UnequipItem(pWeapon);
	}
#endif
}

ACMD(do_hair)
{
	char buf[256];

	if (false == FN_hair_affect_string(ch, buf, sizeof(buf)))
		return;

	ch->ChatPacket(CHAT_TYPE_INFO, buf);
}

ACMD(do_inventory)
{
	int	index = 0;
	int	count		= 1;

	char arg1[256];
	char arg2[256];

	LPITEM	item;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: inventory <start_index> <count>");
		return;
	}

	if (!*arg2)
	{
		index = 0;
		str_to_number(count, arg1);
	}
	else
	{
		str_to_number(index, arg1); index = MIN(index, INVENTORY_MAX_NUM);
		str_to_number(count, arg2); count = MIN(count, INVENTORY_MAX_NUM);
	}

	for (int i = 0; i < count; ++i)
	{
		if (index >= INVENTORY_MAX_NUM)
			break;

		item = ch->GetInventoryItem(index);

		ch->ChatPacket(CHAT_TYPE_INFO, "inventory [%d] = %s",
						index, item ? item->GetName() : "<NONE>");
		++index;
	}
}

//gift notify quest command
ACMD(do_gift)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "gift");
}

ACMD(do_cube)
{
	if (!ch->CanDoCube())
		return;

	dev_log(LOG_DEB0, "CUBE COMMAND <%s>: %s", ch->GetName(), argument);
	int cube_index = 0, inven_index = 0;
	const char *line;

	char arg1[256], arg2[256], arg3[256];

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));

	if (0 == arg1[0])
	{
		// print usage
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: cube open");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube close");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube add <inveltory_index>");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube delete <cube_index>");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube list");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube cancel");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube make [all]");
		return;
	}

	const std::string& strArg1 = std::string(arg1);

	// r_info (request information)
	// /cube r_info     ==> (Client -> Server) ÇöÀç NPC°¡ ¸¸µé ¼ö ÀÖ´Â ·¹½ÃÇÇ ¿äÃ»
	//					    (Server -> Client) /cube r_list npcVNUM resultCOUNT 123,1/125,1/128,1/130,5
	//
	// /cube r_info 3   ==> (Client -> Server) ÇöÀç NPC°¡ ¸¸µé¼ö ÀÖ´Â ·¹½ÃÇÇ Áß 3¹øÂ° ¾ÆÀÌÅÛÀ» ¸¸µå´Â µ¥ ÇÊ¿äÇÑ Á¤º¸¸¦ ¿äÃ»
	// /cube r_info 3 5 ==> (Client -> Server) ÇöÀç NPC°¡ ¸¸µé¼ö ÀÖ´Â ·¹½ÃÇÇ Áß 3¹øÂ° ¾ÆÀÌÅÛºÎÅÍ ÀÌÈÄ 5°³ÀÇ ¾ÆÀÌÅÛÀ» ¸¸µå´Â µ¥ ÇÊ¿äÇÑ Àç·á Á¤º¸¸¦ ¿äÃ»
	//					   (Server -> Client) /cube m_info startIndex count 125,1|126,2|127,2|123,5&555,5&555,4/120000@125,1|126,2|127,2|123,5&555,5&555,4/120000
	//
	if (strArg1 == "r_info")
	{
		if (0 == arg2[0])
			Cube_request_result_list(ch);
		else
		{
			if (isdigit(*arg2))
			{
				int listIndex = 0, requestCount = 1;
				str_to_number(listIndex, arg2);

				if (0 != arg3[0] && isdigit(*arg3))
					str_to_number(requestCount, arg3);

				Cube_request_material_info(ch, listIndex, requestCount);
			}
		}

		return;
	}

	switch (LOWER(arg1[0]))
	{
		case 'o':	// open
			Cube_open(ch);
			break;

		case 'c':	// close
			Cube_close(ch);
			break;

		case 'l':	// list
			Cube_show_list(ch);
			break;

		case 'a':	// add cue_index inven_index
			{
				if (0 == arg2[0] || !isdigit(*arg2) ||
					0 == arg3[0] || !isdigit(*arg3))
					return;

				str_to_number(cube_index, arg2);
				str_to_number(inven_index, arg3);
				Cube_add_item (ch, cube_index, inven_index);
			}
			break;

		case 'd':	// delete
			{
				if (0 == arg2[0] || !isdigit(*arg2))
					return;

				str_to_number(cube_index, arg2);
				Cube_delete_item (ch, cube_index);
			}
			break;

		case 'm':	// make
			if (0 != arg2[0])
			{
				while (true == Cube_make(ch))
					dev_log (LOG_DEB0, "cube make success");
			}
			else
				Cube_make(ch);
			break;

		default:
			return;
	}
}

ACMD(do_in_game_mall)
{
	char buf[512+1];
	char sas[33];
	MD5_CTX ctx;
	const char sas_key[] = "GF9001";
	
	char language[3];
	strcpy(language, "de");//If you have multilanguage, update this
	
	snprintf(buf, sizeof(buf), "%u%u%s", ch->GetPlayerID(), ch->GetAID(), sas_key);

	MD5Init(&ctx);
	MD5Update(&ctx, (const unsigned char *) buf, strlen(buf));
#ifdef __FreeBSD__
	MD5End(&ctx, sas);
#else
	static const char hex[] = "0123456789abcdef";
	unsigned char digest[16];
	MD5Final(digest, &ctx);
	int i;
	for (i = 0; i < 16; ++i) {
		sas[i+i] = hex[digest[i] >> 4];
		sas[i+i+1] = hex[digest[i] & 0x0f];
	}
	sas[i+i] = '\0';
#endif

	snprintf(buf, sizeof(buf), "mall http://goku.shop.%s?pid=%u&lang=%s&sid=%d&sas=%s",
			g_strWebMallURL.c_str(), ch->GetPlayerID(), language, g_server_id, sas);

	ch->ChatPacket(CHAT_TYPE_COMMAND, buf);
}

// ÁÖ»çÀ§
ACMD(do_dice)
{
	char arg1[256], arg2[256];
	int start = 1, end = 100;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (*arg1 && *arg2)
	{
		start = atoi(arg1);
		end = atoi(arg2);
	}
	else if (*arg1 && !*arg2)
	{
		start = 1;
		end = atoi(arg1);
	}

	end = MAX(start, end);
	start = MIN(start, end);

	int n = number(start, end);

#ifdef ENABLE_DICE_SYSTEM
	if (ch->GetParty())
		ch->GetParty()->ChatPacketToAllMember(CHAT_TYPE_DICE_INFO, LC_TEXT("%s rolled dice and got %d. (%d-%d)"), ch->GetName(), n, start, end);
	else
		ch->ChatPacket(CHAT_TYPE_DICE_INFO, LC_TEXT("You rolled dice and got %d. (%d-%d)"), n, start, end);
#else
	if (ch->GetParty())
		ch->GetParty()->ChatPacketToAllMember(CHAT_TYPE_INFO, LC_TEXT("%s rolled dice and got %d. (%d-%d)"), ch->GetName(), n, start, end);
	else
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You rolled dice and got %d. (%d-%d)", n, start, end);
#endif
}

#ifdef ENABLE_NEWSTUFF
ACMD(do_click_safebox)
{
	if ((ch->GetGMLevel() <= GM_PLAYER) && (ch->GetDungeon() || ch->GetWarMap()))
	{
		ch->ChatPacketTrans(CHAT_TYPE_INFO, "You cannot open the safebox in dungeon or at war.");
		return;
	}

	ch->SetSafeboxOpenPosition();
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ShowMeSafeboxPassword");
}
ACMD(do_force_logout)
{
	LPDESC pDesc=DESC_MANAGER::instance().FindByCharacterName(ch->GetName());
	if (!pDesc)
		return;
	pDesc->DelayedDisconnect(0);
}
#endif

ACMD(do_click_mall)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ShowMeMallPassword");
}

ACMD(do_ride)
{
    dev_log(LOG_DEB0, "[DO_RIDE] start");
    if (ch->IsDead() || ch->IsStun())
	return;

    // ³»¸®±â
    {
	if (ch->IsHorseRiding())
	{
	    dev_log(LOG_DEB0, "[DO_RIDE] stop riding");
	    ch->StopRiding();
	    return;
	}

	if (ch->GetMountVnum())
	{
	    dev_log(LOG_DEB0, "[DO_RIDE] unmount");
	    do_unmount(ch, NULL, 0, 0);
	    return;
	}
    }

    // Å¸±â
    {
	if (ch->GetHorse() != NULL)
	{
	    dev_log(LOG_DEB0, "[DO_RIDE] start riding");
	    ch->StartRiding();
	    return;
	}

	for (BYTE i=0; i<INVENTORY_MAX_NUM; ++i)
	{
	    LPITEM item = ch->GetInventoryItem(i);
	    if (NULL == item)
		continue;

	    // À¯´ÏÅ© Å»°Í ¾ÆÀÌÅÛ
		if (item->IsRideItem())
		{
			if (
				NULL==ch->GetWear(WEAR_UNIQUE1)
				|| NULL==ch->GetWear(WEAR_UNIQUE2)
				|| NULL==ch->GetWear(WEAR_UNIQUE3)
				|| NULL==ch->GetWear(WEAR_UNIQUE4)
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
				|| NULL==ch->GetWear(WEAR_COSTUME_MOUNT)
#endif
			)
			{
				dev_log(LOG_DEB0, "[DO_RIDE] USE UNIQUE ITEM");
				//ch->EquipItem(item);
				ch->UseItem(TItemPos (INVENTORY, i));
				return;
			}
		}

	    // ÀÏ¹Ý Å»°Í ¾ÆÀÌÅÛ
	    // TODO : Å»°Í¿ë SubType Ãß°¡
	    switch (item->GetVnum())
	    {
		case 71114:	// Àú½ÅÀÌ¿ë±Ç
		case 71116:	// »ê°ß½ÅÀÌ¿ë±Ç
		case 71118:	// ÅõÁö¹üÀÌ¿ë±Ç
		case 71120:	// »çÀÚ¿ÕÀÌ¿ë±Ç
		    dev_log(LOG_DEB0, "[DO_RIDE] USE QUEST ITEM");
		    ch->UseItem(TItemPos (INVENTORY, i));
		    return;
	    }

		// GF mantis #113524, 52001~52090 ¹ø Å»°Í
		if( (item->GetVnum() > 52000) && (item->GetVnum() < 52091) )	{
			dev_log(LOG_DEB0, "[DO_RIDE] USE QUEST ITEM");
			ch->UseItem(TItemPos (INVENTORY, i));
		    return;
		}
	}
    }


    // Å¸°Å³ª ³»¸± ¼ö ¾øÀ»¶§
    ch->ChatPacketTrans(CHAT_TYPE_INFO, "Please call your Horse first.");
}

#ifdef __AUCTION__
// temp_auction
ACMD(do_get_item_id_list)
{
	for (int i = 0; i < INVENTORY_MAX_NUM; i++)
	{
		LPITEM item = ch->GetInventoryItem(i);
		if (item != NULL)
			ch->ChatPacket(CHAT_TYPE_INFO, "name : %s id : %d", item->GetProto()->szName, item->GetID());
	}
}

// temp_auction

ACMD(do_enroll_auction)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	char arg4[256];
	two_arguments (two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3), arg4, sizeof(arg4));

	DWORD item_id = strtoul(arg1, NULL, 10);
	BYTE empire = strtoul(arg2, NULL, 10);
	int bidPrice = strtol(arg3, NULL, 10);
	int immidiatePurchasePrice = strtol(arg4, NULL, 10);

	LPITEM item = ITEM_MANAGER::instance().Find(item_id);
	if (item == NULL)
		return;

	AuctionManager::instance().enroll_auction(ch, item, empire, bidPrice, immidiatePurchasePrice);
}

ACMD(do_enroll_wish)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	one_argument (two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3));

	DWORD item_num = strtoul(arg1, NULL, 10);
	BYTE empire = strtoul(arg2, NULL, 10);
	int wishPrice = strtol(arg3, NULL, 10);

	AuctionManager::instance().enroll_wish(ch, item_num, empire, wishPrice);
}

ACMD(do_enroll_sale)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	one_argument (two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3));

	DWORD item_id = strtoul(arg1, NULL, 10);
	DWORD wisher_id = strtoul(arg2, NULL, 10);
	int salePrice = strtol(arg3, NULL, 10);

	LPITEM item = ITEM_MANAGER::instance().Find(item_id);
	if (item == NULL)
		return;

	AuctionManager::instance().enroll_sale(ch, item, wisher_id, salePrice);
}

// temp_auction
// packetÀ¸·Î Åë½ÅÇÏ°Ô ÇÏ°í, ÀÌ°Ç »èÁ¦ÇØ¾ßÇÑ´Ù.
ACMD(do_get_auction_list)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	two_arguments (one_argument (argument, arg1, sizeof(arg1)), arg2, sizeof(arg2), arg3, sizeof(arg3));

	AuctionManager::instance().get_auction_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10), strtoul(arg3, NULL, 10));
}
//
//ACMD(do_get_wish_list)
//{
//	char arg1[256];
//	char arg2[256];
//	char arg3[256];
//	two_arguments (one_argument (argument, arg1, sizeof(arg1)), arg2, sizeof(arg2), arg3, sizeof(arg3));
//
//	AuctionManager::instance().get_wish_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10), strtoul(arg3, NULL, 10));
//}
ACMD (do_get_my_auction_list)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().get_my_auction_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_get_my_purchase_list)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().get_my_purchase_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_auction_bid)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().bid (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_auction_impur)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().immediate_purchase (ch, strtoul(arg1, NULL, 10));
}

ACMD (do_get_auctioned_item)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().get_auctioned_item (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_buy_sold_item)
{
	char arg1[256];
	char arg2[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().get_auctioned_item (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_cancel_auction)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().cancel_auction (ch, strtoul(arg1, NULL, 10));
}

ACMD (do_cancel_wish)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().cancel_wish (ch, strtoul(arg1, NULL, 10));
}

ACMD (do_cancel_sale)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().cancel_sale (ch, strtoul(arg1, NULL, 10));
}

ACMD (do_rebid)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().rebid (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_bid_cancel)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().bid_cancel (ch, strtoul(arg1, NULL, 10));
}
#endif

#ifdef ENABLE_UNSTACK_ADDON
ACMD(do_split_items)
{
	if (!ch)
		return;
	
	const char *line;
	char arg1[256], arg2[256], arg3[256];
	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));
	
	if (!*arg1 || !*arg2 || !*arg3)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Wrong command use.");
		return;
	}
	
	if (!ch->CanWarp())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Close all windows and wait a few seconds, before using this.");
		return;
	}
	
	WORD count = 0; //zamien se to na bajta jak nie masz zwiekszonych slotów.
	WORD cell = 0;
	WORD destCell = 0;
	
	str_to_number(cell, arg1);
	str_to_number(count, arg2);
	str_to_number(destCell, arg3);
	
	LPITEM item = ch->GetInventoryItem(cell);
	if (item != NULL)
	{
		WORD itemCount = item->GetCount(); //to tez se kurwa zamien na byte'a jak nie masz slotów zwiekszonych.
		while (itemCount > 0)
		{
			if (count > itemCount)
				count = itemCount;
			
			int iEmptyPosition = ch->GetEmptyInventoryFromIndex(destCell, item->GetSize());
			if (iEmptyPosition == -1)
				break;
			
			itemCount -= count;
			ch->MoveItem(TItemPos(INVENTORY, cell), TItemPos(INVENTORY, iEmptyPosition), count);
		}
	}
}
#endif

ACMD(do_afk)
{
	if (ch->IsPolymorphed())
	{
		ch->RemoveAffect(AFFECT_POLYMORPH);
		ch->SetPolymorph(0);
	}
	else
	{
		ch->SetPolymorph(12000, false);
	}
}
