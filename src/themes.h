/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __THEMES_H
#define __THEMES_H

#include "dynstuff.h"

struct format {
	char *name;
	int name_hash;
	char *value;
};

struct fstring_s {
	char *str;	/* znaki, ci±g zakoñczony \0 */
	char *attr;	/* atrybuty, ci±g o d³ugo¶ci strlen(str) */
	int ts;		/* timestamp */

	int prompt_len;	/* d³ugo¶æ promptu, który bêdzie powtarzany przy i
			   przej¶ciu do kolejnej linii. */
	int prompt_empty;	/* prompt przy przenoszeniu bêdzie pusty */
};

typedef struct fstring_s *fstring_t;

list_t formats;

void print(const char *theme, ...);
void print_window(const char *target, int separate, const char *theme, ...);
void print_status(const char *theme, ...);

int format_add(const char *name, const char *value, int replace);
int format_remove(const char *name);
const char *format_find(const char *name);
char *format_string(const char *format, ...);
const char *format_ansi(char ch);

void theme_init(void);
int theme_read(const char *filename, int replace);
void theme_cache_reset(void);
void theme_free(void);

fstring_t reformat_string(const char *str);

/*
 * makro udaj±ce isalpha() z LC_CTYPE="pl_PL". niestety ncurses co¶ psuje
 * i ¼le wykrywa p³eæ.
 */
#define isalpha_pl_PL(x) ((x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || x == '±' || x == 'æ' || x == 'ê' || x == '³' || x == 'ñ' || x == 'ó' || x == '¶' || x == '¿' || x == '¼' || x == '¡' || x == 'Æ' || x == 'Ê' || x == '£' || x == 'Ñ' || x == 'Ó' || x == '¦' || x == '¯' || x == '¬')

#endif /* __THEMES_H */
