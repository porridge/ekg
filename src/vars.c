/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dynstuff.h"
#include "libgadu.h"
#include "mail.h"
#ifndef HAVE_STRLCPY
#  include "../compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "vars.h"
#include "xmalloc.h"

list_t variables = NULL;

/*
 * dd_*()
 *
 * funkcje informuj±ce, czy dana grupa zmiennych ma zostaæ wy¶wietlona.
 * równie dobrze mo¿na by³o przekazaæ wska¼nik do zmiennej, która musi
 * byæ ró¿na od zera, ale dziêki funkcjom nie trzeba bêdzie mieszaæ w 
 * przysz³o¶ci.
 */
static int dd_beep(const char *name)
{
	return (config_beep);
}

static int dd_check_mail(const char *name)
{
	return (config_check_mail);
}

static int dd_dcc(const char *name)
{
	return (config_dcc);
}

static int dd_sound(const char *name)
{
	return (config_sound_app != NULL);
}

static int dd_sms(const char *name)
{
	return (config_sms_app != NULL);
}

static int dd_log(const char *name)
{
	return (config_log);
}

static int dd_color(const char *name)
{
	return (config_display_color);
}

#ifdef WITH_UI_NCURSES
static int dd_contacts(const char *name)
{
	return (config_contacts);
}
#endif

/*
 * variable_init()
 *
 * inicjuje listê zmiennych.
 */
void variable_init()
{
	variable_add("uin", "ui", VAR_INT, 1, &config_uin, changed_uin, NULL, NULL);
	variable_add("password", "pa", VAR_STR, 0, &config_password, NULL, NULL, NULL);
	variable_add("email", "em", VAR_STR, 1, &config_email, NULL, NULL, NULL);
#ifdef HAVE_VOIP
	variable_add("audio_device", "ad", VAR_STR, 1, &config_audio_device, NULL, NULL, NULL);
#endif
	variable_add("auto_away", "aa", VAR_INT, 1, &config_auto_away, NULL, NULL, NULL);
	variable_add("auto_back", "ab", VAR_INT, 1, &config_auto_back, NULL, NULL, NULL);
	variable_add("auto_reconnect", "ac", VAR_INT, 1, &config_auto_reconnect, NULL, NULL, NULL);
	variable_add("auto_save", "as", VAR_INT, 1, &config_auto_save, NULL, NULL, NULL);
	variable_add("away_reason", "ar", VAR_STR, 1, &config_away_reason, changed_xxx_reason, NULL, NULL);
	variable_add("back_reason", "br", VAR_STR, 1, &config_back_reason, changed_xxx_reason, NULL, NULL);
#ifdef WITH_UI_NCURSES
	if (ui_init == ui_ncurses_init)
		variable_add("backlog_size", "bs", VAR_INT, 1, &config_backlog_size, changed_backlog_size, NULL, NULL);
#endif
	variable_add("beep", "be", VAR_BOOL, 1, &config_beep, NULL, NULL, NULL);
	variable_add("beep_msg", "bm", VAR_BOOL, 1, &config_beep_msg, NULL, NULL, dd_beep);
	variable_add("beep_chat", "bc", VAR_BOOL, 1, &config_beep_chat, NULL, NULL, dd_beep);
	variable_add("beep_notify", "bn", VAR_BOOL, 1, &config_beep_notify, NULL, NULL, dd_beep);
	variable_add("beep_mail", "bM", VAR_BOOL, 1, &config_beep_mail, NULL, NULL, dd_beep);
	variable_add("check_mail", "cm", VAR_MAP, 1, &config_check_mail, changed_check_mail, variable_map(4, 0, 0, "no", 1, 2, "mbox", 2, 1, "maildir", 4, 0, "notify"), NULL);
	variable_add("check_mail_frequency", "cf", VAR_INT, 1, &config_check_mail_frequency, changed_check_mail, NULL, dd_check_mail);
	variable_add("check_mail_folders", "cF", VAR_STR, 1, &config_check_mail_folders, changed_check_mail_folders, NULL, dd_check_mail);
	variable_add("completion_notify", "cn", VAR_MAP, 1, &config_completion_notify, NULL, variable_map(4, 0, 0, "none", 1, 2, "add", 2, 1, "addremove", 4, 0, "busy"), NULL);
#ifdef WITH_UI_READLINE
	if (ui_init == ui_readline_init)
		variable_add("ctrld_quits", "cq", VAR_BOOL, 1, &config_ctrld_quits, NULL, NULL, NULL);
#endif
#ifdef WITH_UI_NCURSES
	if (ui_init == ui_ncurses_init) {
		variable_add("contacts", "co", VAR_INT, 1, &config_contacts, NULL, NULL, NULL);
		variable_add("contacts_groups", "cg", VAR_STR, 1, &config_contacts_groups, NULL, NULL, dd_contacts);
		variable_add("contacts_options", "cO", VAR_STR, 1, &config_contacts_options, NULL, NULL, dd_contacts);
		variable_add("contacts_size", "cs", VAR_INT, 1, &config_contacts_size, NULL, NULL, dd_contacts);
	}
#endif
	variable_add("dcc", "dc", VAR_BOOL, 1, &config_dcc, changed_dcc, NULL, NULL);
	variable_add("dcc_ip", "di", VAR_STR, 1, &config_dcc_ip, changed_dcc, NULL, dd_dcc);
	variable_add("dcc_dir", "dd", VAR_STR, 1, &config_dcc_dir, NULL, NULL, dd_dcc);
	variable_add("display_ack", "da", VAR_INT, 1, &config_display_ack, NULL, variable_map(4, 0, 0, "none", 1, 0, "all", 2, 0, "delivered", 3, 0, "queued"), NULL);
	variable_add("display_color", "dC", VAR_INT, 1, &config_display_color, NULL, NULL, NULL);
	variable_add("display_color_map", "dm", VAR_STR, 1, &config_display_color_map, NULL, NULL, dd_color);
	variable_add("display_crap", "dr", VAR_BOOL, 1, &config_display_crap, NULL, NULL, NULL);
	variable_add("display_notify", "dn", VAR_INT, 1, &config_display_notify, NULL, variable_map(3, 0, 0, "none", 1, 2, "all", 2, 1, "significant"), NULL);
	variable_add("display_pl_chars", "dp", VAR_BOOL, 1, &config_display_pl_chars, NULL, NULL, NULL);
	variable_add("display_sent", "ds", VAR_BOOL, 1, &config_display_sent, NULL, NULL, NULL);
	variable_add("display_welcome", "dw", VAR_BOOL, 1, &config_display_welcome, NULL, NULL, NULL);
#ifdef WITH_UI_NCURSES
	if (ui_init == ui_ncurses_init)
		variable_add("display_transparent", "dt", VAR_BOOL, 1, &config_display_transparent, NULL, NULL, NULL);
#endif
	variable_add("emoticons", "eM", VAR_BOOL, 1, &config_emoticons, NULL, NULL, NULL);
#ifdef HAVE_OPENSSL
	variable_add("encryption", "en", VAR_INT, 1, &config_encryption, NULL, variable_map(3, 0, 0, "none", 1, 2, "sim", 2, 1, "simlite"), NULL);
#endif
	variable_add("enter_scrolls", "es", VAR_BOOL, 1, &config_enter_scrolls, NULL, NULL, NULL);
	variable_add("events_delay", "ev", VAR_INT, 1, &config_events_delay, NULL, NULL, NULL);
#ifdef WITH_UI_NCURSES
	if (ui_init == ui_ncurses_init)
		variable_add("header_size", "hs", VAR_INT, 1, &config_header_size, header_statusbar_resize, NULL, NULL);
#endif
	variable_add("keep_reason", "kr", VAR_BOOL, 1, &config_keep_reason, NULL, NULL, NULL);
	variable_add("last", "la", VAR_MAP, 1, &config_last, NULL, variable_map(4, 0, 0, "none", 1, 2, "all", 2, 1, "separate", 4, 0, "sent"), NULL);
	variable_add("last_size", "ls", VAR_INT, 1, &config_last_size, NULL, NULL, NULL);
	variable_add("log", "lo", VAR_MAP, 1, &config_log, NULL, variable_map(4, 0, 0, "none", 1, 2, "file", 2, 1, "dir", 4, 0, "gzip"), NULL);
	variable_add("log_ignored", "li", VAR_INT, 1, &config_log_ignored, NULL, NULL, dd_log);
	variable_add("log_status", "lS", VAR_BOOL, 1, &config_log_status, NULL, NULL, dd_log);
	variable_add("log_path", "lp", VAR_STR, 1, &config_log_path, NULL, NULL, dd_log);
	variable_add("log_timestamp", "lt", VAR_STR, 1, &config_log_timestamp, NULL, NULL, dd_log);
	variable_add("make_window", "mw", VAR_INT, 1, &config_make_window, NULL, variable_map(3, 0, 0, "none", 1, 2, "usefree", 2, 1, "always"), NULL);
	variable_add("mesg", "ma", VAR_INT, 1, &config_mesg, changed_mesg, variable_map(3, 0, 0, "no", 1, 2, "yes", 2, 1, "default"), NULL);
	variable_add("proxy", "pr", VAR_STR, 1, &config_proxy, changed_proxy, NULL, NULL);
	variable_add("proxy_forwarding", "pf", VAR_STR, 1, &config_proxy_forwarding, NULL, NULL, NULL);
	variable_add("random_reason", "rr", VAR_MAP, 1, &config_random_reason, NULL, variable_map(5, 0, 0, "none", 1, 0, "away", 2, 0, "notavail", 4, 0, "avail", 8, 0, "invisible"), NULL);
	variable_add("reason_limit", "rl", VAR_BOOL, 1, &config_reason_limit, NULL, NULL, NULL);
	variable_add("quit_reason", "qr", VAR_STR, 1, &config_quit_reason, changed_xxx_reason, NULL, NULL);
	variable_add("query_commands", "qc", VAR_BOOL, 1, &config_query_commands, NULL, NULL, NULL);
	variable_add("save_password", "sp", VAR_BOOL, 1, &config_save_password, NULL, NULL, NULL);
	variable_add("server", "se", VAR_STR, 1, &config_server, NULL, NULL, NULL);
	variable_add("server_save", "ss", VAR_BOOL, 1, &config_server_save, NULL, NULL, NULL);
	variable_add("sms_away", "sa", VAR_MAP, 1, &config_sms_away, NULL, variable_map(3, 0, 0, "none", 1, 2, "all", 2, 1, "separate"), dd_sms);
	variable_add("sms_away_limit", "sl", VAR_INT, 1, &config_sms_away_limit, NULL, NULL, dd_sms);
	variable_add("sms_max_length", "sm", VAR_INT, 1, &config_sms_max_length, NULL, NULL, dd_sms);
	variable_add("sms_number", "sn", VAR_STR, 1, &config_sms_number, NULL, NULL, dd_sms);
	variable_add("sms_send_app", "sA", VAR_STR, 1, &config_sms_app, NULL, NULL, NULL);
	variable_add("sort_windows", "sw", VAR_BOOL, 1, &config_sort_windows, NULL, NULL, NULL);
	variable_add("sound_msg_file", "Sm", VAR_STR, 1, &config_sound_msg_file, NULL, NULL, dd_sound);
	variable_add("sound_chat_file", "Sc", VAR_STR, 1, &config_sound_chat_file, NULL, NULL, dd_sound);
	variable_add("sound_sysmsg_file", "Ss", VAR_STR, 1, &config_sound_sysmsg_file, NULL, NULL, dd_sound);
	variable_add("sound_notify_file", "Sn", VAR_STR, 1, &config_sound_notify_file, NULL, NULL, dd_sound);
	variable_add("sound_mail_file", "SM", VAR_STR, 1, &config_sound_mail_file, NULL, NULL, dd_sound);
	variable_add("sound_app", "Sa", VAR_STR, 1, &config_sound_app, NULL, NULL, NULL);
	variable_add("speech_app", "SA", VAR_STR, 1, &config_speech_app, NULL, NULL, NULL);
#ifdef WITH_UI_NCURSES
	if (ui_init == ui_ncurses_init)
		variable_add("statusbar_size", "sS", VAR_INT, 1, &config_statusbar_size, header_statusbar_resize, NULL, NULL);
#endif
	variable_add("tab_command", "tc", VAR_STR, 1, &config_tab_command, NULL, NULL, NULL);
	variable_add("theme", "th", VAR_STR, 1, &config_theme, changed_theme, NULL, NULL);
	variable_add("time_deviation", "td", VAR_INT, 1, &config_time_deviation, NULL, NULL, NULL);
	variable_add("timestamp", "ts", VAR_STR, 1, &config_timestamp, NULL, NULL, NULL);
#ifdef WITH_UI_NCURSES
	if (ui_init == ui_ncurses_init) {
		variable_add("windows_save", "ws", VAR_BOOL, 1, &config_windows_save, NULL, NULL, NULL);
		variable_add("windows_layout", "wl", VAR_STR, 2, &config_windows_layout, NULL, NULL, NULL);
	}
#endif
	variable_add("status", "st", VAR_INT, 2, &config_status, NULL, NULL, NULL);
	variable_add("protocol", "pR", VAR_INT, 2, &config_protocol, NULL, NULL, NULL);
	variable_add("reason", "re", VAR_STR, 2, &config_reason, NULL, NULL, NULL);
	variable_add("interface", "in", VAR_STR, 2, &config_interface, NULL, NULL, NULL);
	variable_add("password_cp1250", "c1", VAR_BOOL, 2, &config_password_cp1250, NULL, NULL, NULL);
	variable_add("last_sysmsg", "LS", VAR_INT, 2, &config_last_sysmsg, NULL, NULL, NULL);
}

/*
 * variable_set_default()
 *
 * ustawia pewne standardowe warto¶ci zmiennych.
 */
void variable_set_default()
{
	xfree(config_timestamp);
	xfree(config_display_color_map);

	config_timestamp = xstrdup("%H:%M ");
	config_display_color_map = xstrdup("nTgGbBrR");
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

		if (v->name_hash == hash && !strcasecmp(v->name, name))
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
 *  - short_name - krótka nazwa,
 *  - type - typ zmiennej,
 *  - display - czy i jak ma wy¶wietlaæ,
 *  - ptr - wska¼nik do zmiennej,
 *  - notify - funkcja powiadomienia,
 *  - map - mapa warto¶ci,
 *  - dyndisplay - funkcja sprawdzaj±ca czy wy¶wietliæ zmienn±.
 *
 * zwraca 0 je¶li siê uda³o, je¶li nie to -1.
 */
int variable_add(const char *name, const char *short_name, int type, int display, void *ptr, void (*notify)(const char*), struct value_map *map, int (*dyndisplay)(const char *name))
{
	struct variable v;
	list_t l;

	if (!name || (type != VAR_FOREIGN && !short_name))
		return -1;

	if (type != VAR_FOREIGN) {
		for (l = variables; l; l = l->next) {
			struct variable *v = l->data;

			if (!strcmp(v->short_name, short_name)) {
				fprintf(stderr, "Error! Variable short name conflict:\n- short name: \"%s\"\n- existing variable: \"%s\"\n- conflicting variable: \"%s\"\n\nPress any key to continue...", short_name, v->name, name);
				getchar();
			}
		}
	}

	memset(&v, 0, sizeof(v));

	v.name = xstrdup(name);
	v.name_hash = ekg_hash(name);
	v.type = type;
	if (short_name)
		strlcpy(v.short_name, short_name, sizeof(v.short_name));
	v.display = display;
	v.ptr = ptr;
	v.notify = notify;
	v.map = map;
	v.dyndisplay = dyndisplay;

	return (list_add(&variables, &v, sizeof(v)) ? 0 : -1);
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
		variable_add(name, "##", VAR_FOREIGN, 2, xstrdup(value), NULL, NULL, NULL);
		return -1;
	}

	if (!v && !allow_foreign)
		return -1;

	switch (v->type) {
		case VAR_INT:
		case VAR_MAP:
		{
			const char *p = value;
			int hex, tmp;

			if (!value)
				return -2;

			if (v->map && v->type == VAR_INT && !xisdigit(*p)) {
				int i;

				for (i = 0; v->map[i].label; i++)
					if (!strcasecmp(v->map[i].label, value))
						value = itoa(v->map[i].value);
			}

			if (v->map && v->type == VAR_MAP && !xisdigit(*p)) {
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
				
			if ((hex = !strncasecmp(p, "0x", 2)))
				p += 2;

			while (*p && *p != ' ') {
				if (hex && !xisxdigit(*p))
					return -2;
				
				if (!hex && !xisdigit(*p))
					return -2;
				p++;
			}

			tmp = strtol(value, NULL, 0);

			if (v->map) {
				int i;

				for (i = 0; v->map[i].label; i++) {
					if ((tmp & v->map[i].value) && (tmp & v->map[i].conflicts))
						return -2;
				}
			}

			*(int*)(v->ptr) = tmp;

			if (v->notify)
				(v->notify)(v->name);

			if (ui_event)
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

			if (ui_event)
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
			} else
				*tmp = NULL;
	
			if (v->notify)
				(v->notify)(v->name);

			if (ui_event)
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

/*
 * variable_digest()
 *
 * tworzy skrócon± wersjê listy zmiennej do zachowania w li¶cie kontaktów
 * na serwerze.
 */
char *variable_digest()
{
	string_t s = string_init(NULL);
	list_t l;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		if ((v->type == VAR_INT || v->type == VAR_BOOL || v->type == VAR_MAP) && strcmp(v->name, "uin")) {
			string_append(s, v->short_name);
			string_append(s, itoa(*(int*)(v->ptr)));
		}
	}

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;
		char *val;

		if (!v->type == VAR_STR || !strcmp(v->name, "password"))
			continue;
	
		val = *((char**)v->ptr);

		string_append(s, v->short_name);

		if (!val) {
			string_append_c(s, '-');
			continue;
		}

		if (strchr(val, ':') || val[0] == '+') {
			char *tmp = base64_encode(val);
			string_append_c(s, '+');
			string_append(s, tmp);
			string_append_c(s, ':');
			xfree(tmp);
		} else {
			string_append(s, val);
			string_append_c(s, ':');
		}
	}
	
	return string_free(s, 0);
}

/*
 * variable_undigest()
 *
 * rozszyfrowuje skrót zmiennych z listy kontaktów i ustawia wszystko
 * co trzeba.
 *
 *  - str - ci±g znaków.
 *
 * 0/-1
 */
int variable_undigest(const char *digest)
{
	const char *p = digest;

	if (!digest)
		return -1;

	while (*p) {
		struct variable *v;
		list_t l;

		for (v = NULL, l = variables; l; l = l->next) {
			struct variable *w = l->data;

			if (!strncmp(p, w->short_name, 2)) {
				v = w;
				break;
			}
		}

		if (!v) {
			gg_debug(GG_DEBUG_MISC, "// unknown short \"%c%c\"\n", p[0], p[1]);
			return -1;
		}

		p += 2;

		if (v->type == VAR_INT || v->type == VAR_BOOL || v->type == VAR_MAP) {
			char *end;
			int val;
			
			val = strtol(p, &end, 10);

			variable_set(v->name, itoa(val), 0);

			p = end;
		}

		if (v->type == VAR_STR) {
			char *val = NULL;
			
			if (*p == '-') {
				val = NULL;
				p++;
			} else {
				const char *q;
				char *tmp;
				int len = 0, base64 = 0;

				if (*p == '+') {
					base64 = 1;
					p++;
				}
				
				for (q = p; *q && *q != ':'; q++, len++)
					;

				tmp = xstrmid(p, 0, len);

				gg_debug(GG_DEBUG_MISC, "// got string variable \"%s\"\n", tmp);

				if (base64) {
					val = base64_decode(tmp);
					xfree(tmp);
				} else
					val = tmp;

				p += len + 1;
			}

			gg_debug(GG_DEBUG_MISC, "// setting variable %s = \"%s\"\n", v->name, ((val) ? val : "(null)"));

			variable_set(v->name, val, 0);
			
			xfree(val);
		}
	}

	return 0;
}

/*
 * variable_help()
 *
 * wy¶wietla pomoc dotycz±c± danej zmiennej na podstawie pliku
 * ${datadir}/ekg/vars.txt.
 *
 *  - name - nazwa zmiennej.
 */
void variable_help(const char *name)
{
	FILE *f = fopen(DATADIR "/vars.txt", "r");
	char *line, *type = NULL, *def = NULL, *tmp;
	string_t s = string_init(NULL);
	int found = 0;

	if (!f) {
		print("help_set_file_not_found");
		return;
	}

	while ((line = read_file(f))) {
		if (!strcasecmp(line, name)) {
			found = 1;
			xfree(line);
			break;
		}

		xfree(line);
	}

	if (!found) {
		fclose(f);
		print("help_set_var_not_found", name);
		return;
	}

	line = read_file(f);
	
	if ((tmp = strstr(line, ": ")))
		type = xstrdup(tmp + 2);
	else
		type = xstrdup("?");
	
	xfree(line);

	tmp = NULL;
	
	line = read_file(f);
	if ((tmp = strstr(line, ": ")))
		def = xstrdup(tmp + 2);
	else
		def = xstrdup("?");
	xfree(line);

	print("help_set_header", name, type, def);

	xfree(type);
	xfree(def);

	if (tmp)			/* je¶li nie jest to ukryta zmienna... */
		xfree(read_file(f));	/* ... pomijamy liniê */

	while ((line = read_file(f))) {
		if (line[0] != '\t') {
			xfree(line);
			break;
		}

		if (!strncmp(line, "\t- ", 3) && strcmp(s->str, "")) {
			print("help_set_body", s->str);
			string_clear(s);
		}
		
		string_append(s, line + 1);

		if (line[strlen(line) - 1] != ' ')
			string_append_c(s, ' ');

		xfree(line);
	}

	if (strcmp(s->str, ""))
		print("help_set_body", s->str);

	string_free(s, 1);
	
	print("help_set_footer", name);

	fclose(f);
}
