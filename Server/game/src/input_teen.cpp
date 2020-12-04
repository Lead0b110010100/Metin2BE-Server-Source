/*********************************************************************
 * date        : 2007.06.07
 * file        : input_teen.cpp
 * author      : mhh
 * description :
 */

#define _input_teen_cpp_

#include "stdafx.h"
#include "constants.h"
#include "../../common/teen_packet.h"
#include "input.h"
#include "desc.h"
#include "desc_manager.h"
#include "db.h"
#include "protocol.h"
#include "char.h"
#include "dev_log.h"

#define HANDSHAKE_XOR                  0x6AB3D224

void CInputTeen::SetStep(int step)
{
	m_step = step;
}

bool CInputTeen::Process(LPDESC lpDesc, const void * c_pvOrig, int iBytes, int & r_iBytesProceed)
{
	switch (m_step)
	{
		case 0:
			return this->ProcessHandshake(lpDesc, c_pvOrig, iBytes, r_iBytesProceed);
			break;
		case 1:
			return this->ProcessMain(lpDesc, c_pvOrig, iBytes, r_iBytesProceed);
			break;
	}
	return false;
} /* end of CInputTeen::Process() */

bool CInputTeen::ProcessHandshake(LPDESC lpDesc, const void * c_pvOrig, size_t uiBytes, int & r_iBytesProceed)
{
	const char	*c_pData = (const char*) c_pvOrig;
	size_t packet_len = sizeof(DWORD);

	if (uiBytes < packet_len)
		return false;

	DWORD	handshake	= decode_4bytes(c_pData);

	c_pData			+= packet_len;
	m_iBufferLeft	-= packet_len;
	r_iBytesProceed	+= packet_len;

	this->SetStep(1);

	char buf[256];
	*((DWORD *) buf) = handshake ^ HANDSHAKE_XOR;
	lpDesc->Packet(buf, sizeof(DWORD));

	return true;
}


static int __packet_len(BYTE header)
{
	const int	header_size = sizeof(BYTE) + sizeof(WORD);

	switch (header)
	{
		case HEADER_TG_TEEN_NOTICE:	return (header_size + LOGIN_MAX_LEN + 4);
		case HEADER_TG_FORCE_LOGOUT:	return (header_size + LOGIN_MAX_LEN);
		case HEADER_TG_LOGIN_NOTICE:	return (header_size + LOGIN_MAX_LEN + 4 + 4);
	}
	return 0;
}


static void __proc_teen_notice(char *login, int hour)
{
	LPDESC desc = DESC_MANAGER::instance().FindByLoginName(login);
	if (NULL==desc) return;

	LPCHARACTER		ch = desc->GetCharacter();
	if (NULL==ch)	return;

	switch (hour)
	{
		case 0:
			{
				SET_OVER_TIME(ch, OT_NONE);
				return;
			}
			break;
		case 1:
		case 2:
			{
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Your total online hour has passed %d hours already."), hour);
				SET_OVER_TIME(ch, OT_NONE);
			}
			break;
		case 3:
			{
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Your total online hour is already %d hours."), hour);
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Please disconnect inmediately for your health."));
				SET_OVER_TIME(ch, OT_3HOUR);
			}
			break;
		case 4:
			{
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("You have already played too long to have severe fatigue,"));
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("In-game profit is set down to 50% of normal."));
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Please disconnect immediately for your health"));
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Please concentrate on your studies."));
				SET_OVER_TIME(ch, OT_3HOUR);
			}
			break;
		default:
			{
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Your game play hour is too long."));
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("For your health, Disconnect immediately and take a rest, please."));
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("If not, all the stat will become 0 and may harm your health."));
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("After 5 hours of log off, it will be back to normal."));
				SET_OVER_TIME(ch, OT_5HOUR);
			}
			break;
	}
}

static inline void __sec_to_timestring(int sec, char *buf, size_t buflen)
{
	int	hour	= (sec/60)/60;
	int min		= (sec/60);

	if (hour>0)
		snprintf(buf, buflen, LC_TEXT("%d hour"), hour);
	else
		snprintf(buf, buflen, LC_TEXT("%d min"), min);
}

static void __proc_login_notice(char *login, int on_time, int off_time)
{
	//{ check player's name
	LPDESC desc = DESC_MANAGER::instance().FindByLoginName(login);
	if (NULL==desc) return;

	LPCHARACTER		ch = desc->GetCharacter();
	if (NULL==ch)	return;
	//} check player's name

	char	on_time_string[64];
	char	off_time_string[64];

	__sec_to_timestring(on_time, on_time_string, sizeof(on_time_string));
	__sec_to_timestring(off_time, off_time_string, sizeof(off_time_string));

	if (0==on_time)
	{
		ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Your total off line hour is %s."), off_time_string);
		ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Your total online hour is 0. You can have 100% effect."));
		return;
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("Your total off line hour is %s, Your total online hour is %s."),
				off_time_string, on_time_string);
	}
}


static void __input_teen(BYTE header, WORD desc_num, char *body)
{
	switch (header)
	{
		case HEADER_TG_FORCE_LOGOUT:
			{
				char	*data = body;
				char	login[LOGIN_MAX_LEN+1] = {0};
				memcpy(login, data, LOGIN_MAX_LEN);
				data += LOGIN_MAX_LEN;
				LPDESC d = DESC_MANAGER::instance().FindByLoginName(login);

				if (NULL==d)
					return;

				d->SetPhase(PHASE_CLOSE);
			}
			break;
		case HEADER_TG_TEEN_NOTICE:
			{
				char	*data = body;
				char	login[LOGIN_MAX_LEN+1] = {0};
				memcpy(login, data, LOGIN_MAX_LEN);
				data += LOGIN_MAX_LEN;
				int	hour = decode_4bytes(data);
				data += 4;

				__proc_teen_notice(login, hour);
			}
			break;
		case HEADER_TG_LOGIN_NOTICE:
			{
				char	*data = body;
				char	login[LOGIN_MAX_LEN+1] = {0};
				memcpy(login, data, LOGIN_MAX_LEN);
				data += LOGIN_MAX_LEN;
				int		on_time = decode_4bytes(data);
				data += 4;
				int		off_time = decode_4bytes(data);
				data += 4;

				__proc_login_notice(login, on_time, off_time);
			}
			break;
	}
}


bool CInputTeen::ProcessMain(LPDESC lpDesc, const void * c_pvOrig, size_t uiBytes, int & r_iBytesProceed)
{
	const char	*c_pData = (const char*) c_pvOrig;
	const size_t header_size = sizeof(BYTE) + sizeof(WORD);

	if (uiBytes < header_size)
		return false;

	for (m_iBufferLeft = uiBytes; m_iBufferLeft > 0;)
	{
		BYTE header		= decode_byte(c_pData);
		WORD desc_num	= decode_2bytes(c_pData+sizeof(BYTE));
		char *body		= (char*) c_pData + header_size;

		int packet_len	= __packet_len(header);

		if (m_iBufferLeft < packet_len)
			return true;

		c_pData			+= packet_len;
		m_iBufferLeft	-= packet_len;
		r_iBytesProceed	+= packet_len;

		__input_teen(header, desc_num, body);
	}

	return true;
}

