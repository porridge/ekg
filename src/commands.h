/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
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

#include "dynstuff.h"

#define printq(x...) do { if (!quiet) { print(x); } } while(0)

#define COMMAND(x) int x(const char *name, const char **params, const char *target, int quiet)
typedef COMMAND(command_func_t);

struct command {
	char *name;
	char *params;
	command_func_t *function;
	int alias;
	char *params_help;
	char *brief_help;
	char *long_help;
};

list_t commands;
int change_quiet;
int userlist_get_config, userlist_put_config;

int command_add(const char *name, const char *params, command_func_t function, int alias, const char *params_help, const char *brief_help, const char *long_help);
int command_remove(const char *name);
void command_init(void);
void command_free(void);
int command_exec(const char *target, const char *line, int quiet);
int command_exec_format(const char *target, int quiet, const char *format, ...);

COMMAND(cmd_alias_exec);
COMMAND(cmd_exec);

/*
 * jaka¶ malutka lista tych, do których by³y wysy³ane wiadomo¶ci.
 */
#define SEND_NICKS_MAX 100

char *send_nicks[SEND_NICKS_MAX];
int send_nicks_count, send_nicks_index;

void add_send_nick(const char *nick);
void remove_send_nick(const char *nick);

int binding_help(int a, int b);
int binding_quick_list(int a, int b);
int binding_toggle_contacts(int a, int b);

int match_arg(const char *arg, char shortopt, const char *longopt, int longoptlen);

#endif /* __COMMANDS_H */
