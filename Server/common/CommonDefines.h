#ifndef __INC_METIN2_COMMON_DEFINES_H__
#define __INC_METIN2_COMMON_DEFINES_H__

//////////////////////////////////////////////////////////////////////////

// --- TP Anzeige
//#define __VIEW_TARGET_PLAYER_HP__
#define __VIEW_TARGET_DECIMAL_HP__

// --- Channel Switcher
#define ENABLE_CHANGE_CHANNEL

// --- Verbesserungsfenster geöffnet lassen
#define ENABLE_REFINE_RENEWAL

// --- Restart Tod NEU
#define RENEWAL_DEAD_PACKET

// --- Aktive Affects bei Mobs und Spieler anzeigen
#define ENABLE_TARGET_AFFECT

// --- Schreibt info in PN
#define ENABLE_WHISPER_TIPPING

// --- VWK per Button rückverwandeln
#define ENABLE_AFFECT_POLYMORPH_REMOVE

// --- Stack enpacken Add-on
#define ENABLE_UNSTACK_ADDON

// --- Item Namen auf Boden Erweiterung
#define ENABLE_EXTENDED_ITEMNAME_ON_GROUND

// --- Monster Target Info
#define __SEND_TARGET_INFO__

// --- Ladescreen Infos
#define __LOADING_TIP__

#define ENABLE_GM_FUNCTIONS
#ifdef ENABLE_GM_FUNCTIONS
#define GMS_CAN_WALK_REALLY_FAST
#endif
#define ENABLE_LANG_SYSTEM

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// ### General Features ###
//#define ENABLE_QUEST_CATEGORY
#define ENABLE_D_NJGUILD
#define ENABLE_FULL_NOTICE
#define ENABLE_NEWSTUFF
#define ENABLE_PORT_SECURITY
enum eCommonDefines {
	MAP_ALLOW_LIMIT = 32, // 32 default
	ESTIMATED_SERVER_LIFETIME = (60 * 60 * 24 * 365 * 7),
};
// ### General Features ###
//////////////////////////////////////////////////////////////////////////

#define WJ_ENABLE_TRADABLE_ICON

typedef long long GoldType;

//////////////////////////////////////////////////////////////////////////
// ### CommonDefines Systems ###
// #define ENABLE_WOLFMAN_CHARACTER
#ifdef ENABLE_WOLFMAN_CHARACTER
#define USE_MOB_BLEEDING_AS_POISON
#define USE_MOB_CLAW_AS_DAGGER
// #define USE_ITEM_BLEEDING_AS_POISON
// #define USE_ITEM_CLAW_AS_DAGGER
#define USE_WOLFMAN_STONES
#define USE_WOLFMAN_BOOKS
#endif

#define ENABLE_EXTEND_ITEM_AWARD
#ifdef ENABLE_EXTEND_ITEM_AWARD
	#define ENABLE_ITEMAWARD_REFRESH
	#define USE_ITEM_AWARD_CHECK_ATTRIBUTES // c++11 or higher
#endif

// #define ENABLE_PLAYER_PER_ACCOUNT5
// #define ENABLE_DICE_SYSTEM
#define ENABLE_EXTEND_INVEN_SYSTEM

// #define ENABLE_MOUNT_COSTUME_SYSTEM
#define ENABLE_WEAPON_COSTUME_SYSTEM

// #define ENABLE_MAGIC_REDUCTION_SYSTEM
#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
// #define USE_MAGIC_REDUCTION_STONES
#endif
// ### CommonDefines Systems ###
//////////////////////////////////////////////////////////////////////////

#endif

