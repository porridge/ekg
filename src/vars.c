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
#include <ctype.h>
#include "stuff.h"
#include "vars.h"
#include "config.h"
#include "libgadu.h"
#include "dynstuff.h"
#include "xmalloc.h"
#include "ui.h"

list_t variables = NULL;

/*
 * variable_init()
 *
 * inicjuje listê zmiennych.
 */
void variable_init()
{
	variable_add("uin", VAR_INT, 1, &config_uin, changed_uin, NULL);
	variable_add("password", VAR_STR, 0, &config_password, NULL, NULL);

#ifdef HAVE_VOIP
	variable_add("audio_device", VAR_STR, 1, &config_audio_device, NULL, NULL);
#endif
	variable_add("auto_away", VAR_INT, 1, &config_auto_away, NULL, NULL);
	variable_add("auto_reconnect", VAR_INT, 1, &config_auto_reconnect, NULL, NULL);
	variable_add("auto_save", VAR_INT, 1, &config_auto_save, NULL, NULL);
	variable_add("away_reason", VAR_STR, 1, &config_away_reason, changed_xxx_reason, NULL);
	variable_add("back_reason", VAR_STR, 1, &config_back_reason, changed_xxx_reason, NULL);
	variable_add("beep", VAR_BOOL, 1, &config_beep, NULL, NULL);
	variable_add("beep_msg", VAR_BOOL, 1, &config_beep_msg, NULL, NULL);
	variable_add("beep_chat", VAR_BOOL, 1, &config_beep_chat, NULL, NULL);
	variable_add("beep_notify", VAR_BOOL, 1, &config_beep_notify, NULL, NULL);
	variable_add("completion_notify", VAR_MAP, 1, &config_completion_notify, NULL, variable_map(4, 0, 0, "none", 1, 2, "add", 2, 1, "addremove", 4, 0, "busy"));
	variable_add("ctrld_quits", VAR_BOOL, 1, &config_ctrld_quits, NULL, NULL);
	variable_add("dcc", VAR_BOOL, 1, &config_dcc, changed_dcc, NULL);
	variable_add("dcc_ip", VAR_STR, 1, &config_dcc_ip, changed_dcc, NULL);
	variable_add("dcc_dir", VAR_STR, 1, &config_dcc_dir, NULL, NULL);
	variable_add("display_ack", VAR_INT, 1, &config_display_ack, NULL, variable_map(4, 0, 0, "none", 1, 0, "all", 2, 0, "delivered", 3, 0, "queued"));
	variable_add("display_color", VAR_BOOL, 1, &config_display_color, NULL, NULL);
	variable_add("display_notify", VAR_INT, 1, &config_display_notify, NULL, variable_map(3, 0, 0, "none", 1, 0, "all", 2, 0, "significant"));
	variable_add("display_sent", VAR_BOOL, 1, &config_display_sent, NULL, NULL);
	variable_add("emoticons", VAR_BOOL, 1, &config_emoticons, NULL, NULL);
#ifdef HAVE_OPENSSL
	variable_add("encryption", VAR_INT, 1, &config_encryption, NULL, variable_map(2, 0, 0, "none", 1, 0, "sim"));
#endif
	variable_add("enter_scrolls", VAR_BOOL, 1, &config_enter_scrolls, NULL, NULL);
	variable_add("keep_reason", VAR_BOOL, 1, &config_keep_reason, NULL, NULL);
	variable_add("last", VAR_MAP, 1, &config_last, NULL, variable_map(4, 0, 0, "none", 1, 2, "all", 2, 1, "separate", 4, 0, "sent"));
	variable_add("last_size", VAR_INT, 1, &config_last_size, NULL, NULL);
	variable_add("log", VAR_MAP, 1, &config_log, NULL, variable_map(4, 0, 0, "none", 1, 2, "file", 2, 1, "dir", 4, 0, "gzip"));
	variable_add("log_ignored", VAR_INT, 1, &config_log_ignored, NULL, NULL);
	variable_add("log_status", VAR_BOOL, 1, &config_log_status, NULL, NULL);
	variable_add("log_path", VAR_STR, 1, &config_log_path, NULL, NULL);
	variable_add("log_timestamp", VAR_STR, 1, &config_log_timestamp, NULL, NULL);
	variable_add("make_window", VAR_INT, 1, &config_make_window, NULL, variable_map(3, 0, 0, "none", 1, 0, "usefree", 2, 0, "always"));
	variable_add("proxy", VAR_STR, 1, &config_proxy, changed_proxy, NULL);
	variable_add("random_reason", VAR_MAP, 1, &config_random_reason, NULL, variable_map(5, 0, 0, "none", 1, 0, "away", 2, 0, "notavail", 4, 0, "avail", 8, 0, "invisible"));
	variable_add("quit_reason", VAR_STR, 1, &config_quit_reason, changed_xxx_reason, NULL);
	variable_add("query_commands", VAR_BOOL, 1, &config_query_commands, NULL, NULL);
	variable_add("save_password", VAR_BOOL, 1, &config_save_password, NULL, NULL);
	variable_add("server", VAR_STR, 1, &config_server, NULL, NULL);
	variable_add("sms_away", VAR_BOOL, 1, &config_sms_away, NULL, NULL);
	variable_add("sms_max_length", VAR_INT, 1, &config_sms_max_length, NULL, NULL);
	variable_add("sms_number", VAR_STR, 1, &config_sms_number, NULL, NULL);
	variable_add("sms_send_app", VAR_STR, 1, &config_sms_app, NULL, NULL);
	variable_add("sort_windows", VAR_BOOL, 1, &config_sort_windows, NULL, NULL);
	variable_add("sound_msg_file", VAR_STR, 1, &config_sound_msg_file, NULL, NULL);
	variable_add("sound_chat_file", VAR_STR, 1, &config_sound_chat_file, NULL, NULL);
	variable_add("sound_sysmsg_file", VAR_STR, 1, &config_sound_sysmsg_file, NULL, NULL);
	variable_add("sound_app", VAR_STR, 1, &config_sound_app, NULL, NULL);
	variable_add("speech_app", VAR_STR, 1, &config_speech_app, NULL, NULL);
	variable_add("tab_command", VAR_STR, 1, &config_tab_command, NULL, NULL);
	variable_add("theme", VAR_STR, 1, &config_theme, changed_theme, NULL);
	variable_add("timestamp", VAR_STR, 1, &config_timestamp, NULL, NULL);

	variable_add("status", VAR_INT, 2, &config_status, NULL, NULL);
	variable_add("debug", VAR_BOOL, 2, &config_debug, changed_debug, NULL);
	variable_add("protocol", VAR_INT, 2, &config_protocol, NULL, NULL);
	variable_add("reason", VAR_STR, 2, &config_reason, NULL, NULL);
}

/*
 * variable_find()
 *
 * znajduje strukturê `variable' opisuj±c± zmienn± o podanej nazwie.
 *
 * - name.
 */
struct variable *variable_find(const char *name)
{
	list_t l;
	int hash;

	if (!name)
		return NULL;

	hash = ekg_hash(name);

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		if (v->name && v->name_hash == hash && !strcasecmp(v->name, name))
			return v;
	}

	return NULL;
}

/*
 * variable_map()
 *
 * tworzy now± mapê warto¶ci. je¶li która¶ z warto¶ci powinna wy³±czyæ inne
 * (na przyk³ad w ,,log'' 1 wy³±cza 2, 2 wy³±cza 1, ale nie maj± wp³ywu na 4)
 * nale¿y dodaæ do ,,konflikt''.
 *
 *  - count - ilo¶æ,
 *  - ... - warto¶æ, konflikt, opis.
 *
 * zaalokowana tablica.
 */
struct value_map *variable_map(int count, ...)
{
	struct value_map *res;
	va_list ap;
	int i;

	res = xcalloc(count + 1, sizeof(struct value_map));
	
	va_start(ap, count);

	for (i = 0; i < count; i++) {
		res[i].value = va_arg(ap, int);
		res[i].conflicts = va_arg(ap, int);
		res[i].label = xstrdup(va_arg(ap, char*));
	}
	
	va_end(ap);

	return res;
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
 *  - map - mapa warto¶ci,
 *
 * zwraca 0 je¶li siê uda³o, je¶li nie to -1.
 */
int variable_add(const char *name, int type, int display, void *ptr, void (*notify)(const char*), struct value_map *map)
{
	struct variable v;

	v.name = xstrdup(name);
	v.name_hash = ekg_hash(name);
	v.type = type;
	v.display = display;
	v.ptr = ptr;
	v.notify = notify;
	v.map = map;

	return (list_add(&variables, &v, sizeof(v)) != NULL);
}

/*
 * variable_set()
 *
 * ustawia warto¶æ podanej zmiennej. je¶li to zmienna liczbowa lub boolowska,
 * zmienia ci±g na liczbê. w przypadku boolowskich, rozumie zwroty typu `on',
 * `off', `yes', `no' itp. je¶li dana zmienna jest bitmap±, akceptuje warto¶æ
 * w postaci listy flag oraz konstrukcje `+flaga' i `-flaga'.
 *
 *  - name - nazwa zmiennej,
 *  - value - nowa warto¶æ,
 *  - allow_foreign - czy ma pozwalaæ dopisywaæ obce zmienne.
 */
int variable_set(const char *name, const char *value, int allow_foreign)
{
	struct variable *v = variable_find(name);

	if (!v && allow_foreign) {
		variable_add(name, VAR_FOREIGN, 2, xstrdup(value), NULL, NULL);
		return -1;
	}

	if (!v && !allow_foreign)
		return -1;

	switch (v->type) {
		case VAR_INT:
		case VAR_MAP:
		{
			const char *p = value;

			if (!value)
				return -2;

			if (v->map && v->type == VAR_INT && !isdigit(*p)) {
				int i;

				for (i = 0; v->map[i].label; i++)
					if (!strcasecmp(v->map[i].label, value))
						value = itoa(v->map[i].value);
			}

			if (v->map && v->type == VAR_MAP && !isdigit(*p)) {
				int i, k = *(int*)(v->ptr);
				int mode = 0; /* 0 set, 1 add, 2 remove */
				char **args;

				if (*p == '+') {
					mode = 1;
					p++;
				} else if (*p == '-') {
					mode = 2;
					p++;
				}

				if (!mode)
					k = 0;

				args = array_make(p, ",", 0, 1, 0);

				for (i = 0; args[i]; i++) {
					int j, found = 0;

					for (j = 0; v->map[j].label; j++) {
						if (!strcasecmp(args[i], v->map[j].label)) {
							found = 1;

							if (mode == 2)
								k &= ~(v->map[j].value);
							if (mode == 1)
								k &= ~(v->map[j].conflicts);
							if (mode == 1 || !mode)
								k |= v->map[j].value;
						}
					}

					if (!found) {
						array_free(args);
						return -2;
					}
				}

				array_free(args);

				value = itoa(k);
			}

			p = value;
				
			if (!strncmp(p, "0x", 2))
				p += 2;

			while (*p && *p != ' ') {
				if (!isdigit(*p))
					return -2;
				p++;
			}

			*(int*)(v->ptr) = strtol(value, NULL, 0);

			if (v->notify)
				(v->notify)(v->name);

			ui_event("variable_changed", v->name);
			
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

			ui_event("variable_changed", v->name);
		
			return 0;
		}

		case VAR_STR:
		{
			char **tmp = (char**)(v->ptr);
			
			xfree(*tmp);
			
			if (value) {
				if (*value == 1)
					*tmp = base64_decode(value + 1);
				else
					*tmp = xstrdup(value);

				if (!*tmp)
					return -3;
			} else
				*tmp = NULL;
	
			if (v->notify)
				(v->notify)(v->name);

			ui_event("variable_changed", v->name);

			return 0;
		}
	}

	return -1;
}

/*
 * variable_free()
 *
 * zwalnia pamiêæ u¿ywan± przez zmienne.
 *
 * nie zwraca niczego.
 */
void variable_free()
{
	list_t l;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		xfree(v->name);

		if (v->type == VAR_STR) {
			xfree(*((char**) v->ptr));
			*((char**) v->ptr) = NULL;
		}

		if (v->type == VAR_FOREIGN)
			xfree((char*) v->ptr);

		if (v->map) {
			int i;

			for (i = 0; v->map[i].label; i++)
				xfree(v->map[i].label);

			xfree(v->map);
		}
	}

	list_destroy(variables, 1);
	variables = NULL;
}
