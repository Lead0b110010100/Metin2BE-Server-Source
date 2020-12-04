#ifndef __INC_METIN2_GAME_LOCALE_H__
#define __INC_METIN2_GAME_LOCALE_H__

extern "C"
{
#ifdef ENABLE_LANG_SYSTEM
	void locale_init(const char* filename, BYTE lang);

	const char* locale_find(const char* string);
	const char* locale_find_trans(const char* string, BYTE lang = 0);

#define LC_TEXT(str) locale_find(str)
#define LC_TEXT_TRANS(str, lang) locale_find_trans(str, lang)
#else
	void locale_init(const char* filename);

	const char* locale_find(const char* string);

#define LC_TEXT(str) locale_find(str)
#endif
};

#endif
