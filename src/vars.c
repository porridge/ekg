/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
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
#include "config.h"
#include "libgadu.h"
#include "dynstuff.h"

struct list *variables = NULL;

/*
 * variable_init()
 *
 * inicjuje listê zmiennych.
 */
void variable_init()
{
	variable_add("uin", VAR_INT, 1, &config_uin, NULL);
	variable_add("password", VAR_STR, 0, &config_password, NULL);

	variable_add("auto_away", VAR_INT, 1, &config_auto_away, NULL);
	variable_add("auto_reconnect", VAR_INT, 1, &config_auto_reconnect, NULL);
	variable_add("auto_save", VAR_INT, 1, &config_auto_save, NULL);
	variable_add("away_reason", VAR_STR, 1, &config_away_reason, NULL);
	variable_add("back_reason", VAR_STR, 1, &config_back_reason, NULL);
	variable_add("beep", VAR_BOOL, 1, &config_beep, NULL);
	variable_add("beep_msg", VAR_BOOL, 1, &config_beep_msg, NULL);
	variable_add("beep_chat", VAR_BOOL, 1, &config_beep_chat, NULL);
	variable_add("beep_notify", VAR_BOOL, 1, &config_beep_notify, NULL);
	variable_add("completion_notify", VAR_BOOL, 1, &config_completion_notify, NULL);
	variable_add("dcc", VAR_BOOL, 1, &config_dcc, changed_dcc);
	variable_add("dcc_ip", VAR_STR, 1, &config_dcc_ip, changed_dcc);
	variable_add("dcc_dir", VAR_STR, 1, &config_dcc_dir, NULL);
	variable_add("display_ack", VAR_INT, 1, &config_display_ack, NULL);
	variable_add("display_color", VAR_BOOL, 1, &config_display_color, NULL);
	variable_add("display_notify", VAR_BOOL, 1, &config_display_notify, NULL);
	variable_add("emoticons", VAR_BOOL, 1, &config_emoticons, NULL);
	variable_add("log", VAR_INT, 1, &config_log, NULL);
	variable_add("log_ignored", VAR_INT, 1, &config_log_ignored, NULL);
	variable_add("log_status", VAR_BOOL, 1, &config_log_status, NULL);
	variable_add("log_path", VAR_STR, 1, &config_log_path, NULL);
	variable_add("proxy", VAR_STR, 1, &config_proxy, changed_proxy);
	variable_add("random_reason", VAR_INT, 1, &config_random_reason, NULL);
	variable_add("quit_reason", VAR_STR, 1, &config_quit_reason, NULL);
	variable_add("query_commands", VAR_BOOL, 1, &config_query_commands, NULL);
	variable_add("server", VAR_STR, 1, &config_server, NULL);
	variable_add("sms_away", VAR_BOOL, 1, &config_sms_away, NULL);
	variable_add("sms_max_length", VAR_INT, 1, &config_sms_max_length, NULL);
	variable_add("sms_number", VAR_STR, 1, &config_sms_number, NULL);
	variable_add("sms_send_app", VAR_STR, 1, &config_sms_app, NULL);
	variable_add("sound_msg_file", VAR_STR, 1, &config_sound_msg_file, NULL);
	variable_add("sound_chat_file", VAR_STR, 1, &config_sound_chat_file, NULL);
	variable_add("sound_sysmsg_file", VAR_STR, 1, &config_sound_sysmsg_file, NULL);
	variable_add("sound_app", VAR_STR, 1, &config_sound_app, NULL);
	variable_add("theme", VAR_STR, 1, &config_theme, changed_theme);

	variable_add("status", VAR_INT, 2, &config_status, NULL);
	variable_add("debug", VAR_BOOL, 2, &config_debug, changed_debug);
	variable_add("protocol", VAR_INT, 2, &config_protocol, NULL);
}

/*
 * variable_find()
 *
 * znajduje strukturê `variable' opisuj±c± zmienn± o podanej nazwie.
 *
 * - name.
 */
struct variable *variable_find(char *name)
{
	struct list *l;

	if (!name)
		return NULL;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		if (v->name && !strcasecmp(v->name, name))
			return v;
	}

	return NULL;
}

/*
 * variable_add()
 *
 * dodaje zmienn± do listy zmiennych.
 *
 *  - name - nazwa,
 *  - type - typ zmiennej,
 *  - display - czy i jak ma wy¶wietlaæ,
 *  - ptr - wska¼nik do zmiennej,
 *  - notify - funkcja powiadomienia,
 *
 * zwraca 0 je¶li siê uda³o, je¶li nie to -1.
 */
int variable_add(char *name, int type, int display, void *ptr, void (*notify)(char*))
{
	struct variable v;

	v.name = strdup(name);
	v.type = type;
	v.display = display;
	v.ptr = ptr;
	v.notify = notify;

	return (list_add(&variables, &v, sizeof(v)) != NULL);
}

/*
 * variable_set()
 *
 * ustawia warto¶æ podanej zmiennej. je¶li to zmienna liczbowa lub binarna,
 * zmienia ci±g na liczbê. w przypadku binarnych, rozumie zwroty typu `on',
 * `off', `yes', `no' itp.
 *
 *  - name,
 *  - value,
 *  - allow_foreign - czy ma pozwalaæ dopisywaæ obce zmienne.
 */
int variable_set(char *name, char *value, int allow_foreign)
{
	struct variable *v = variable_find(name);

	if (!v && allow_foreign) {
		variable_add(name, VAR_FOREIGN, 2, strdup(value), NULL);
		return -1;
	}

	if (!v && !allow_foreign)
		return -1;

	switch (v->type) {
		case VAR_INT:
		{
			char *p = value;

			if (!value)
				return -2;

			if (!strncmp(p, "0x", 2))
				p += 2;

			while (*p) {
				if (*p < '0' || *p > '9')
					return -2;
				p++;
			}

			*(int*)(v->ptr) = strtol(value, NULL, 0);

			if (v->notify)
				(v->notify)(v->name);

			return 0;
		}

		case VAR_BOOL:
		{
			int tmp;
		
			if (!value)
				return -2;
		
			if ((tmp = on_off(value)) == -1)
				return -2;

			*(int*)(v->ptr) = tmp;

			if (v->notify)
				(v->notify)(v->name);
		
			return 0;
		}

		case VAR_STR:
		{
			char **tmp = (char**)(v->ptr);
			
			free(*tmp);
			
			if (value) {
				if (*value == 1)
					*tmp = base64_decode(value + 1);
				else
					*tmp = strdup(value);

				if (!*tmp)
					return -3;
			} else
				*tmp = NULL;
	
			if (v->notify)
				(v->notify)(v->name);

			return 0;
		}
	}

	return -1;
}

