/* $Id$ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>
 *			Robert J. Wo¼ny <speedy@ziew.org>
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

#include <stdlib.h>
#include <unistd.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdarg.h>
#include "stuff.h"
#include "vars.h"
#include "libgg.h"

struct variable variables[MAX_VARS] = {
	{ "uin", VAR_INT, 1, &config_uin },
	{ "password", VAR_STR, 0, &config_password },

	{ "auto_away", VAR_INT, 1, &auto_away },
	{ "auto_reconnect", VAR_INT, 1, &auto_reconnect },
	{ "beep", VAR_BOOL, 1, &enable_beep },
	{ "beep_msg", VAR_BOOL, 1, &enable_beep_msg },
	{ "beep_chat", VAR_BOOL, 1, &enable_beep_chat },
	{ "beep_notify", VAR_BOOL, 1, &enable_beep_notify },
	{ "completion_notify", VAR_BOOL, 1, &completion_notify },
	{ "display_ack", VAR_INT, 1, &display_ack },
	{ "display_color", VAR_BOOL, 1, &display_color },
	{ "display_notify", VAR_BOOL, 1, &display_notify },
	{ "log", VAR_INT, 1, &log },
	{ "log_path", VAR_STR, 1, &log_path },
	{ "proxy_port", VAR_INT, 1, &gg_http_proxy_port },
	{ "proxy_host", VAR_STR, 1, &gg_http_proxy_host },
	{ "sms_away", VAR_BOOL, 1, &sms_away },
	{ "sms_max_length", VAR_INT, 1, &sms_max_length },
	{ "sms_number", VAR_STR, 1, &sms_number },
	{ "sms_send_app", VAR_STR, 1, &sms_send_app },
	{ "sound_msg_file", VAR_STR, 1, &sound_msg_file },
	{ "sound_chat_file", VAR_STR, 1, &sound_chat_file },
	{ "sound_app", VAR_STR, 1, &sound_app },
	{ "theme", VAR_STR, 1, &default_theme },
	{ "use_proxy", VAR_INT, 1, &use_proxy },

	{ "default_status", VAR_INT, 2, &default_status },
	{ "bold_font", VAR_STR, 2, &bold_font },	/* GNU Gadu */
	{ "debug", VAR_BOOL, 2, &display_debug },

	{ NULL, 0, 0, NULL }
};

/*
 * find_variable()
 *
 * znajduje strukturê `variable' opisuj±c± zmienn± o podanej nazwie.
 *
 * - name.
 */
struct variable *find_variable(char *name)
{
	struct variable *v = variables;
	
	while (v->name) {
		if (!strcasecmp(v->name, name))
			return v;
		v++;
	}

	return NULL;
}

/*
 * set_variable()
 *
 * ustawia warto¶æ podanej zmiennej. je¶li to zmienna liczbowa lub binarna,
 * zmienia ci±g na liczbê. w przypadku binarnych, rozumie zwroty typu `on',
 * `off', `yes', `no' itp.
 *
 * - name,
 * - value.
 */
int set_variable(char *name, char *value)
{
	struct variable *v = find_variable(name);

	if (!v)	
		return -1;

	if (v->type == VAR_INT) {
		char *p = value;

		if (!p)
			return -2;

		while (*p) {
			if (*p < '0' || *p > '9')
				return -2;
			p++;
		}

		*(int*)(v->ptr) = atoi(value);

		return 0;
	}

	if (v->type == VAR_BOOL) {
		int tmp;
		
		if (!value)
			return -2;
		
		if ((tmp = on_off(value)) == -1)
			return -2;

		*(int*)(v->ptr) = tmp;
		return 0;
	}

	free(*(char**)(v->ptr));
	if (value) {
		if (!(*(char**)(v->ptr) = strdup(value)))
			return -3;
	} else
		*(char**)(v->ptr) = NULL;

	return 0;
}


