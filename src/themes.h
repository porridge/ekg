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

struct format {
	char *name;
	char *value;
};

struct list *formats;

char *readline_prompt, *readline_prompt_away, *readline_prompt_invisible;

void my_printf(char *theme, ...);
char *find_format(char *name);
char *format_string(char *format, ...);

int add_format(char *name, char *value, int replace);
int del_format(char *name);

void init_theme();
void reset_theme_cache();

int read_theme(char *filename, int replace);

#endif
