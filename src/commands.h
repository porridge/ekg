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

#ifndef __COMMANDS_H
#define __COMMANDS_H

struct command {
	char *name;
	char *params;
	int (*function)(char *name, char **params);
	char *params_help;
	char *brief_help;
	char *long_help;
};

/*
 * jaka¶ malutka lista tych, do których by³y wysy³ane wiadomo¶ci.
 */
#define SEND_NICKS_MAX 100

char *send_nicks[SEND_NICKS_MAX];
int send_nicks_count, send_nicks_index;
extern struct command commands[];

#define COMMAND(x) int x(char *name, char **params)

int ekg_execute(char *target, char *line);

void add_send_nick(const char *nick);

int binding_quick_list(int a, int b);
int binding_help(int a, int b);
int binding_toggle_debug(int a, int b);

#endif
