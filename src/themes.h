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

void print(const char *theme, ...);
void print_window(const char *target, const char *theme, ...);
void print_status(const char *theme, ...);

const char *find_format(const char *name);
char *format_string(const char *format, ...);

int add_format(const char *name, const char *value, int replace);
int del_format(const char *name);

void init_theme();
void reset_theme_cache();

int read_theme(const char *filename, int replace);

#endif /* __THEMES_H */
