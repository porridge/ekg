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

list_t formats;

void print(const char *theme, ...);
void print_window(const char *target, int separate, const char *theme, ...);
void print_status(const char *theme, ...);

int format_add(const char *name, const char *value, int replace);
int format_remove(const char *name);
const char *format_find(const char *name);
char *format_string(const char *format, ...);
const char *format_ansi(char ch);

void theme_init();
int theme_read(const char *filename, int replace);
void theme_cache_reset();
void theme_free();

extern int automaton_color_escapes;

#endif /* __THEMES_H */
