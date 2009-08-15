/* $Id$ */

/*
 *  (C) Copyright 2001-2005 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
 *                          Piotr Kupisiewicz <deli@rzepaknet.us>
 *                          Adam Wysocki <gophi@ekg.chmurka.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef WITH_IOCTLD
#  include <sys/un.h>
#  include "ioctld.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#else
#  include "../compat/dirname.h"
#endif
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "compat.h"
#include "dynstuff.h"
#include "libgadu.h"
#ifdef HAVE_OPENSSL
#  include "simlite.h"
#endif
#ifndef HAVE_STRLCAT
#  include "../compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "../compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "userlist.h"
#include "vars.h"
#include "xmalloc.h"
#ifdef WITH_PYTHON
#  include "python.h"
#endif

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

struct gg_session *sess = NULL;
list_t autofinds = NULL;
list_t children = NULL;
list_t aliases = NULL;
list_t watches = NULL;
list_t transfers = NULL;
list_t events = NULL;
list_t bindings = NULL;
list_t timers = NULL;
list_t conferences = NULL;
list_t sms_away = NULL;
list_t buffers = NULL;
list_t searches = NULL;
list_t spiedlist = NULL;

int command_processing = 0;
int in_autoexec = 0;
int in_auto_away = 0;
int config_auto_reconnect = 10;
int reconnect_timer = 0;
int config_auto_away = 600;
int config_auto_away_keep_descr = 1;
int config_auto_save = 0;
int config_auto_find = 0;
int config_auto_conference = 1;
time_t last_save = 0;
int config_display_color = 1;
int config_beep = 1;
int config_beep_msg = 1;
int config_beep_chat = 1;
int config_beep_notify = 1;
int config_beep_mail = 1;
int config_display_pl_chars = 1;
int config_events_delay = 3;
int config_files_mode_config_int = 600;	/* nie 0600, bo jest zamieniane z dec na oct */
int config_files_mode_config = 0600;		/* wartosc oktalna files_mode_config */
int config_files_mode_received_int = 600;
int config_files_mode_received = 0600;
char *config_sound_msg_file = NULL;
char *config_sound_chat_file = NULL;
char *config_sound_sysmsg_file = NULL;
char *config_sound_notify_file = NULL;
char *config_sound_mail_file = NULL;
char *config_sound_app = NULL;
int config_uin = 0;
int config_userlist_backup = 0;
int config_last_sysmsg = 0;
int config_last_sysmsg_changed = 0;
char *config_local_ip = NULL;
char *config_password = NULL;
int config_slash_messages = 1;
int config_sms_away = 0;
int config_sms_away_limit = 0;
char *config_sms_number = NULL;
char *config_sms_app = NULL;
int config_sms_max_length = 100;
int config_changed = 0;
int config_display_ack = 3;
int config_completion_notify = 1;
int connecting = 0;
time_t last_conn_event = 0;
time_t ekg_started = 0;
int config_display_daychanges = 1;
int config_display_notify = 1;
char *config_theme = NULL;
int config_status = GG_STATUS_AVAIL;
char *reg_password = NULL;
char *reg_email = NULL;
int config_dcc = 0;
int config_dcc_backups = 0;
char *config_dcc_ip = NULL;
char *config_dcc_dir = NULL;
int config_dcc_port = GG_DEFAULT_DCC_PORT;
char *config_reason = NULL;
char *home_dir = NULL;
char *config_quit_reason = NULL;
char *config_away_reason = NULL;
char *config_back_reason = NULL;
char *config_ffc_reason = NULL;
char *config_dnd_reason = NULL;
int config_random_reason = 0;
#ifdef HAVE_REGEX_H
int config_regex_flags = 0;
#endif
int config_query_commands = 0;
char *config_proxy = NULL;
char *config_server = NULL;
int quit_message_send = 0;
int registered_today = 0;
int config_protocol = 0;
int pipe_fd = -1;
int batch_mode = 0;
char *batch_line = NULL;
int config_make_window = 2;
char *config_tab_command = NULL;
int ioctld_sock = -1;
int config_ctrld_quits = 1;
int config_save_password = 1;
int config_receive_images = 0;
int config_image_size = 255;
int config_save_question = 1;
char *config_datestamp = NULL;
char *config_timestamp = NULL;
int config_display_sent = 1;
int config_sort_windows = 0;
int config_keep_reason = 0;
int config_enter_scrolls = 0;
int server_index = 0;
char *config_audio_device = NULL;
char *config_speech_app = NULL;
int config_encryption = 0;
int config_server_save = 0;
char *config_email = NULL;
int config_time_deviation = 300;
int config_msg_as_chat = 0;
int config_mesg = MESG_DEFAULT;
#ifdef WITH_UI_NCURSES
int config_mouse = 0;
#endif
char *config_nick = NULL;
int config_display_welcome = 1;
int config_auto_back = 0;
int config_display_crap = 1;
char *config_display_color_map = NULL;
int config_windows_save = 0;
char *config_windows_layout = NULL;
char *config_profile = NULL;
int config_header_size = 0;
int config_status_window = 0;
int config_statusbar_size = 1;
int config_statusbar_fgcolor = 7;
int config_statusbar_bgcolor = 4;
char *config_proxy_forwarding = NULL;
int config_password_cp1250 = 0;
char *config_interface = NULL;
int config_reason_limit = 0;
char *config_dcc_limit = NULL;
int config_ignore_unknown_sender = 0;
int config_ignore_empty_msg = 0;
#ifdef WITH_WAP
int config_wap_enabled = 2;
#endif
#if defined HAVE_LIBJPEG || defined HAVE_LIBUNGIF
int config_display_token = 1;
#endif
#ifdef WITH_IOCTLD
int config_ioctld_enable = 1;
int config_ioctld_net_port = 22004;
#endif
int config_dcc_filter = 1;

char *last_search_first_name = NULL;
char *last_search_last_name = NULL;
char *last_search_nickname = NULL;
uin_t last_search_uin = 0;

char *last_tokenid = NULL;

struct event_label event_labels[EVENT_LABELS_COUNT + 2] = {
	{ EVENT_MSG, "msg" },
	{ EVENT_CHAT, "chat" },
	{ EVENT_CONFERENCE, "conference" },
	{ EVENT_QUERY, "query" },
	{ EVENT_AVAIL, "avail" },
	{ EVENT_ONLINE, "online" },
	{ EVENT_NOT_AVAIL, "notavail" },
	{ EVENT_AWAY, "away" },
	{ EVENT_INVISIBLE, "invisible" },
	{ EVENT_DESCR, "descr" },
	{ EVENT_DCC, "dcc" },
	{ EVENT_SIGUSR1, "sigusr1" },
	{ EVENT_SIGUSR2, "sigusr2" },
	{ EVENT_DELIVERED, "delivered" },
	{ EVENT_QUEUED, "queued" },
	{ EVENT_FILTERED, "filtered" },
	{ EVENT_MBOXFULL, "mboxfull" },
	{ EVENT_NOT_DELIVERED, "not_delivered" },
	{ EVENT_NEWMAIL, "newmail" },
	{ EVENT_BLOCKED, "blocked" },
	{ EVENT_DCCFINISH, "dccfinish" },
	{ EVENT_CONNECTED, "connected" },
	{ EVENT_DISCONNECTED, "disconnected" },
	{ EVENT_CONNECTIONLOST, "connectionlost" },
	{ EVENT_IMAGE, "image" },

	{ INACTIVE_EVENT, NULL },
	{ 0, NULL }
};

/*
 * alias_add()
 *
 * dopisuje alias do listy aliasów.
 *
 *  - string - linia w formacie 'alias cmd',
 *  - quiet - czy wypluwaæ mesgi na stdout,
 *  - append - czy dodajemy kolejn± komendê?
 *
 * 0/-1
 */
int alias_add(const char *string, int quiet, int append)
{
	char *cmd;
	list_t l;
	struct alias a;
	char *params = NULL;

	if (!string || !(cmd = strchr(string, ' ')))
		return -1;

	*cmd++ = 0;

	for (l = aliases; l; l = l->next) {
		struct alias *j = l->data;

		if (!strcasecmp(string, j->name)) {
			if (!append) {
				printq("aliases_exist", string);
				return -1;
			} else {
				list_t l;

				list_add(&j->commands, cmd, strlen(cmd) + 1);
				
				/* przy wielu komendach trudno dope³niaæ, bo wed³ug której? */
				for (l = commands; l; l = l->next) {
					struct command *c = l->data;

					if (!strcasecmp(c->name, j->name)) {
						xfree(c->params);
						c->params = xstrdup("?");
						break;
					}
				}
			
				printq("aliases_append", string);

				return 0;
			}
		}
	}

	for (l = commands; l; l = l->next) {
		struct command *c = l->data;
		char *tmp = ((*cmd == '/') ? cmd + 1 : cmd);

		if (!strcasecmp(string, c->name) && !c->alias) {
			printq("aliases_command", string);
			return -1;
		}

		if (!strncasecmp(tmp, c->name, strlen(c->name))) {
			params = xstrdup(c->params);
			break;
		}
	}

	a.name = xstrdup(string);
	a.commands = NULL;
	list_add(&a.commands, cmd, strlen(cmd) + 1);
	list_add(&aliases, &a, sizeof(a));

	command_add(a.name, ((params) ? params: "?"), cmd_alias_exec, 1, "", "", "");
	
	xfree(params);

	printq("aliases_add", a.name, "");

	return 0;
}

/*
 * alias_remove()
 *
 * usuwa alias z listy aliasów.
 *
 *  - name - alias lub NULL,
 *  - quiet.
 *
 * 0/-1
 */
int alias_remove(const char *name, int quiet)
{
	list_t l;
	int removed = 0;

	for (l = aliases; l; ) {
		struct alias *a = l->data;

		l = l->next;

		if (!name || !strcasecmp(a->name, name)) {
			if (name)
				printq("aliases_del", name);
			command_remove(a->name);
			xfree(a->name);
			list_destroy(a->commands, 1);
			list_remove(&aliases, a, 1);
			removed = 1;
		}
	}

	if (!removed) {
		if (name)
			printq("aliases_noexist", name);
		else
			printq("aliases_list_empty");

		return -1;
	}

	if (removed && !name)
		printq("aliases_del_all");

	return 0;
}

/*
 * alias_free()
 *
 * zwalnia pamiêæ zajêt± przez aliasy.
 */
void alias_free()
{
	list_t l;

	if (!aliases)
		return;

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;
		
		xfree(a->name);
		list_destroy(a->commands, 1);
	}

	list_destroy(aliases, 1);
	aliases = NULL;
}

/*
 * base64_encode()
 *
 * zapisuje ci±g znaków w base64. alokuje pamiêæ. 
 */
char *base64_encode(const char *buf)
{
	char *tmp = gg_base64_encode(buf);

	if (!buf)
		return xstrdup("");

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

/*
 * base64_decode()
 *
 * wczytuje ci±g znaków base64, zwraca zaalokowany buforek.
 */
char *base64_decode(const char *buf)
{
	char *tmp = gg_base64_decode(buf);

	if (!buf)
		return xstrdup("");

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

/*
 * binding_list()
 *
 * wy¶wietla listê przypisanych komend.
 */
void binding_list(int quiet, const char *name, int all) 
{
	list_t l;
	int found = 0;

	if (!bindings)
		printq("bind_seq_list_empty");

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;

		if (name) {
			if (strcasestr(b->key, name)) {
				printq("bind_seq_list", b->key, b->action);
				found = 1;
			}
			continue;
		}

		if (!b->internal || (all && b->internal))
			printq("bind_seq_list", b->key, b->action);
	}

	if (name && !found) {
		for (l = bindings; l; l = l->next) {
			struct binding *b = l->data;

			if (strcasestr(b->action, name))
				printq("bind_seq_list", b->key, b->action);
		}
	}
}


/*
 * binding_free()
 *
 * zwalnia pamiêæ po li¶cie przypisanych klawiszy.
 */
void binding_free() 
{
	list_t l;

	if (!bindings)
		return;

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;

		xfree(b->key);
		xfree(b->action);
		xfree(b->arg);
		xfree(b->default_action);
		xfree(b->default_arg);
	}

	list_destroy(bindings, 1);
	bindings = NULL;
}

/*
 * buffer_add()
 *
 * dodaje linijkê do danego typu bufora. je¶li max_lines > 0
 * to pilnuje, aby w buforze by³o maksymalnie tyle linii.
 *
 *  - type,
 *  - line,
 *  - max_lines.
 *
 * 0/-1
 */
int buffer_add(int type, const char *target, const char *line, int max_lines)
{
	struct buffer b;

	if (max_lines && buffer_count(type) >= max_lines) {
		struct buffer *foo = buffers->data;

		xfree(foo->line);
		list_remove(&buffers, foo, 1);
	}

	b.type = type;
	b.target = xstrdup(target);
	b.line = xstrdup(line);

	return ((list_add(&buffers, &b, sizeof(b)) ? 0 : -1));
}

/* 
 * buffer_flush()
 *
 * zwraca zaalokowany ³ancuch zawieraj±cy wszystkie linie
 * z bufora danego typu.
 *
 *  - type,
 *  - target - dla kogo by³ bufor? NULL, je¶li olewamy.
 */
char *buffer_flush(int type, const char *target)
{
	string_t str = string_init(NULL);
	list_t l;

	for (l = buffers; l; ) {
		struct buffer *b = l->data;

		l = l->next;

		if (type != b->type)
			continue;

		if (target && b->target && strcmp(target, b->target))
			continue;

		string_append(str, b->line);
                string_append_c(str, '\r');
		string_append_c(str, '\n');

		xfree(b->line);
		xfree(b->target);
		list_remove(&buffers, b, 1);
	}

	return string_free(str, 0);
}

/*
 * buffer_count()
 *
 * zwraca liczbê linii w buforze danego typu.
 */
int buffer_count(int type)
{
	list_t l;
	int count = 0;

	for (l = buffers; l; l = l->next) {
		struct buffer *b = l->data;

		if (b->type == type)
			count++;
	}	

	return count;
}

/*
 * buffer_tail()
 *
 * zwraca najstarszy element buforowej kolejki, który
 * nale¿y zwolniæ. usuwa go z kolejki. zwraca NULL,
 * gdy kolejka jest pusta.
 */
char *buffer_tail(int type)
{
	char *str = NULL;
	list_t l;

	for (l = buffers; l; l = l->next) {
		struct buffer *b = l->data;

		if (type != b->type)
			continue;
		
		str = xstrdup(b->line);

		xfree(b->target);
		list_remove(&buffers, b, 1);

		break;
	}

	return str;
}

/*
 * buffer_free()
 * 
 * zwalnia pamiêæ po buforach.
 */
void buffer_free()
{
	list_t l;

	if (!buffers)
		return;

	for (l = buffers; l; l = l->next) {
		struct buffer *b = l->data;

		xfree(b->line);
		xfree(b->target);
	}

	list_destroy(buffers, 1);
	buffers = NULL;
}

/*
 * changed_auto_save()
 *
 * wywo³ywane po zmianie warto¶ci zmiennej ,,auto_save''.
 */
void changed_auto_save(const char *var)
{
	/* oszukujemy, ale takie zachowanie wydaje siê byæ
	   bardziej ,,naturalne'' */
	last_save = time(NULL);
}

/*
 * changed_aspell()
 *
 * wywo³ywane po zmianie warto¶ci zmiennej ,,aspell'' lub ,,aspell_lang'' lub ,,aspell_encoding''.
 */
#ifdef WITH_ASPELL
void changed_aspell(const char *var)
{
	if (config_aspell == 1) {
		spellcheck_init();
	} else {
		spellcheck_deinit();
	}
}
#endif


/*
 * changed_backlog_size()
 *
 * wywo³ywane po zmianie warto¶ci zmiennej ,,backlog_size''.
 */
void changed_backlog_size(const char *var)
{
#ifdef WITH_UI_NCURSES
	if (config_backlog_size < ui_screen_height)
		config_backlog_size = ui_screen_height;
#endif
}

/*
 * changed_dcc()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,dcc''.
 */
void changed_dcc(const char *var)
{
	struct gg_dcc *dcc = NULL;
	list_t l;
	
	if (!config_uin)
		return;
	
	if (!strcmp(var, "dcc")) {
		for (l = watches; l; l = l->next) {
			struct gg_common *c = l->data;
	
			if (c->type == GG_SESSION_DCC_SOCKET)
				dcc = l->data;
		}
	
		if (!config_dcc && dcc) {
			list_remove(&watches, dcc, 0);
			gg_free_dcc(dcc);
			gg_dcc_ip = 0;
			gg_dcc_port = 0;
		}
	
		if (config_dcc && !dcc) {
			if (!(dcc = gg_dcc_socket_create(config_uin, config_dcc_port))) {
				print("dcc_create_error", strerror(errno));
			} else {
				list_add(&watches, dcc, 0);
			}
		}
	}

	if (!strcmp(var, "dcc_ip")) {
		if (config_dcc_ip) {
			if (!strcasecmp(config_dcc_ip, "auto")) {
				gg_dcc_ip = inet_addr("255.255.255.255");
			} else {
				if (inet_addr(config_dcc_ip) != INADDR_NONE)
					gg_dcc_ip = inet_addr(config_dcc_ip);
				else {
					print("dcc_invalid_ip");
					xfree(config_dcc_ip);
					config_dcc_ip = NULL;
					gg_dcc_ip = 0;
				}
			}
		} else
			gg_dcc_ip = 0;
	}

	if (!strcmp(var, "dcc_port")) {
		for (l = watches; l; l = l->next) {
			struct gg_common *c = l->data;
	
			if (c->type == GG_SESSION_DCC_SOCKET)
				dcc = l->data;
		}
		
		if (config_dcc && dcc && (dcc->port != config_dcc_port)) {
			list_remove(&watches, dcc, 0);
			gg_free_dcc(dcc);
			gg_dcc_ip = 0;
			gg_dcc_port = 0;
		
			if (!(dcc = gg_dcc_socket_create(config_uin, config_dcc_port))) {
				print("dcc_create_error", strerror(errno));
			} else {
				list_add(&watches, dcc, 0);
			}
		}
	}

	if (sess && sess->state == GG_STATE_CONNECTED)
		print("dcc_must_reconnect");
	
	update_status_myip();
}

/*
 * changed_local_ip()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,local_ip''.
 */
void changed_local_ip(const char *var)
{
	if (config_local_ip == NULL)
		gg_local_ip = htonl(INADDR_ANY);
	else {
#ifdef HAVE_INET_PTON
		int tmp = inet_pton(AF_INET, config_local_ip, &gg_local_ip);
		
		if (tmp == 0 || tmp == -1) {
		    	print("invalid_local_ip");
			xfree(config_local_ip);
		        config_local_ip = NULL;
			gg_local_ip = htonl(INADDR_ANY);
		}
#else
		gg_local_ip = inet_addr(config_local_ip);
#endif
	}
}

/*
 * changed_mesg()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,mesg''.
 */
void changed_mesg(const char *var)
{
	if (config_mesg == MESG_DEFAULT)
		mesg_set(mesg_startup);
	else
		mesg_set(config_mesg);
}
	
/*
 * changed_proxy()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,proxy''.
 */
void changed_proxy(const char *var)
{
	char **auth, **userpass = NULL, **hostport = NULL;
	
	gg_proxy_port = 0;
	xfree(gg_proxy_host);
	gg_proxy_host = NULL;
	xfree(gg_proxy_username);
	gg_proxy_username = NULL;
	xfree(gg_proxy_password);
	gg_proxy_password = NULL;
	gg_proxy_enabled = 0;	

	if (!config_proxy)
		return;

	auth = array_make(config_proxy, "@", 0, 0, 0);

	if (!auth[0] || !strcmp(auth[0], "")) {
		array_free(auth);
		return; 
	}
	
	gg_proxy_enabled = 1;

	if (auth[0] && auth[1]) {
		userpass = array_make(auth[0], ":", 0, 0, 0);
		hostport = array_make(auth[1], ":", 0, 0, 0);
	} else
		hostport = array_make(auth[0], ":", 0, 0, 0);
	
	if (userpass && userpass[0] && userpass[1]) {
		gg_proxy_username = xstrdup(userpass[0]);
		gg_proxy_password = xstrdup(userpass[1]);
	}

	gg_proxy_host = xstrdup(hostport[0]);
	gg_proxy_port = (hostport[1]) ? atoi(hostport[1]) : 8080;

	array_free(hostport);
	array_free(userpass);
	array_free(auth);
}

/*
 * changed_theme()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,theme''.
 */
void changed_theme(const char *var)
{
	if (!config_theme) {
		theme_free();
		theme_init();
		ui_event("theme_init");
	} else {
		if (!theme_read(config_theme, 1)) {
			if (!in_autoexec)
				print("theme_loaded", config_theme);
		} else
			print("error_loading_theme", strerror(errno));
	}
}

/*
 * changed_uin()
 *
 * funkcja wywo³ywana przy zmianie zmiennej uin.
 */
void changed_uin(const char *var)
{
	ui_event("xterm_update");
}

/*
 * changed_xxx_reason()
 *
 * funkcja wywo³ywana przy zmianie domy¶lnych powodów.
 */
void changed_xxx_reason(const char *var)
{
	char *tmp = NULL;

	if (!strcmp(var, "away_reason"))
		tmp = config_away_reason;
	if (!strcmp(var, "back_reason"))
		tmp = config_back_reason;
	if (!strcmp(var, "dnd_reason"))
		tmp = config_dnd_reason;
	if (!strcmp(var, "ffc_reason"))
		tmp = config_ffc_reason;
	if (!strcmp(var, "quit_reason"))
		tmp = config_quit_reason;

	if (!tmp)
		return;

	if (strlen(tmp) > GG_STATUS_DESCR_MAXSIZE)
		print("descr_too_long", itoa(strlen(tmp) - GG_STATUS_DESCR_MAXSIZE));
}

/*
 * changed_files_mode()
 *
 * funkcja wywo³wana przy zmianie config_files_mode_*_int
 */
void changed_files_mode(const char *var)
{
	char mode[16];

	snprintf(mode, sizeof(mode), "%d", config_files_mode_config_int);
	config_files_mode_config = strtol(mode, NULL, 8);

	snprintf(mode, sizeof(mode), "%d", config_files_mode_received_int);
	config_files_mode_received = strtol(mode, NULL, 8);
}

/*
 * conference_add()
 *
 * dopisuje konferencje do listy konferencji.
 *
 *  - name - nazwa konferencji,
 *  - nicklist - lista nicków, grup, czegokolwiek,
 *  - quiet - czy wypluwaæ mesgi na stdout.
 *
 * zaalokowan± struct conference lub NULL w przypadku b³êdu.
 */
struct conference *conference_add(const char *name, const char *nicklist, int quiet)
{
	struct conference c, *ret;
	char **nicks, **p;
	list_t l;
	int i, count;

	memset(&c, 0, sizeof(c));

	if (!name || !nicklist)
		return NULL;

	if (nicklist[0] == ',' || nicklist[strlen(nicklist) - 1] == ',') {
		printq("invalid_params", "chat");
		return NULL;
	}

	nicks = array_make(nicklist, " ,", 0, 1, 1);

	/* grupy zamieniamy na niki */
	for (i = 0; nicks[i]; i++) {
		if (nicks[i][0] == '@') {
			char *gname = xstrdup(nicks[i] + 1);
			int first = 0;
			int nig = 0; /* nicks in group */

		        for (l = userlist; l; l = l->next) {
				struct userlist *u = l->data;
				list_t m;

				if (!u->display)
					continue;

				for (m = u->groups; m; m = m->next) {
					struct group *g = m->data;

					if (!strcasecmp(gname, g->name)) {
						if (first++) {
							if (!array_contains(nicks, u->display, 0))
								array_add(&nicks, xstrdup(u->display));
						} else {
							xfree(nicks[i]);
							nicks[i] = xstrdup(u->display);
						}

						nig++;

						break;
					}
				}
			}

			if (!nig) {
				printq("group_empty", gname);
				printq("conferences_not_added", name);
				xfree(gname);
				array_free(nicks);
				return NULL;
			}

			xfree(gname);
		}
	}

	count = array_count(nicks);

	for (l = conferences; l; l = l->next) {
		struct conference *cf = l->data;
		
		if (!strcasecmp(name, cf->name)) {
			printq("conferences_exist", name);

			array_free(nicks);

			return NULL;
		}
	}

	for (p = nicks, i = 0; *p; p++) {
		uin_t uin;
		list_t l;

		if (!strcmp(*p, ""))
		        continue;

		if (!(uin = get_uin(*p))) {
			printq("user_not_found", *p);
			break;
		}

		if (uin == config_uin)
			break;

		for (l = c.recipients; l; l = l->next) {
			uin_t tmp = *((uin_t *)l->data);

			if (tmp == uin) {
				uin = 0;
				break;
			}
		}

		if (!uin)
			break;

		list_add(&(c.recipients), &uin, sizeof(uin));
		i++;
	}

	array_free(nicks);

	if (i != count) {
		printq("conferences_not_added", name);
		if (c.recipients)
			list_destroy(c.recipients, 1);
		return NULL;
	}

	printq("conferences_add", name);

	c.name = xstrdup(name);

	add_send_nick(name);

	ret = list_add(&conferences, &c, sizeof(c));

	event_check(EVENT_CONFERENCE, 0, name);

	/* je¶li na li¶cie zdarzeñ jest usuniêcie konferencji, to ret ju¿ nie 
	 * wskazuje na nic sensownego. nie tworzymy konferencji. mo¿e trochê 
	 * ma³o eleganckie rozwi±zanie, ale nie przychodzi mi do g³owy nic 
	 * lepszego. -- gophi */

	{
		list_t cur = conferences;

		while (cur) {
			if ((void *) ret == cur->data)
				return ret;
			cur = cur->next;
		}
	}

	return (struct conference *) NULL;
}

/*
 * conference_remove()
 *
 * usuwa konferencjê z listy konferencji.
 *
 *  - name - konferencja lub NULL dla wszystkich,
 *  - quiet.
 *
 * 0/-1
 */
int conference_remove(const char *name, int quiet)
{
	list_t l;
	int removed = 0;

	for (l = conferences; l; ) {
		struct conference *c = l->data;

		l = l->next;

		if (!name || !strcasecmp(c->name, name)) {
			if (name)
				printq("conferences_del", name);
			remove_send_nick(c->name);
			xfree(c->name);
			list_destroy(c->recipients, 1);
			list_remove(&conferences, c, 1);
			removed = 1;
		}
	}

	if (!removed) {
		if (name)
			printq("conferences_noexist", name);
		else
			printq("conferences_list_empty");
		
		return -1;
	}

	if (removed && !name)
		printq("conferences_del_all");

	return 0;
}

/*
 * conference_create()
 *
 * tworzy now± konferencjê z wygenerowan± nazw±.
 *
 *  - nicks - lista ników tak, jak dla polecenia conference.
 */
struct conference *conference_create(const char *nicks)
{
	struct conference *c;
	static int count = 1;
	char *name = saprintf("#conf%d", count);

	if ((c = conference_add(name, nicks, 0)))
		count++;

	xfree(name);

	return c;
}

/*
 * conference_find()
 *
 * znajduje i zwraca wska¼nik do konferencji lub NULL.
 *
 *  - name - nazwa konferencji.
 */
struct conference *conference_find(const char *name) 
{
	list_t l;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;

		if (!strcmp(c->name, name))
			return c;
	}
	
	return NULL;
}

/*
 * conference_participant()
 *
 * sprawdza, czy dany numer jest uczestnikiem konferencji.
 *
 *  - c - konferencja,
 *  - uin - numer.
 *
 * 1 je¶li jest, 0 je¶li nie.
 */
int conference_participant(struct conference *c, uin_t uin)
{
	list_t l;
	
	for (l = c->recipients; l; l = l->next) {
		uin_t *u = l->data;

		if (uin == *u)
			return 1;
	}

	return 0;

}

/*
 * conference_find_by_uins()
 *
 * znajduje konferencjê, do której nale¿± podane uiny. je¿eli nie znaleziono,
 * zwracany jest NULL. je¶li numerów jest wiêcej, zostan± dodane do
 * konferencji, bo najwyra¼niej kto¶ do niej do³±czy³.
 * 
 *  - from - kto jest nadawc± wiadomo¶ci,
 *  - recipients - tablica numerów nale¿±cych do konferencji,
 *  - count - ilo¶æ numerów,
 *  - quiet.
 */
struct conference *conference_find_by_uins(uin_t from, uin_t *recipients, int count, int quiet) 
{
	int i;
	list_t l;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;
		int matched = 0;

		for (i = 0; i < count; i++)
			if (conference_participant(c, recipients[i]))
				matched++;

		if (conference_participant(c, from))
			matched++;

		gg_debug(GG_DEBUG_MISC, "// conference_find_by_uins(): from=%d, rcpt count=%d, matched=%d, list_count(c->recipients)=%d\n", from, count, matched, list_count(c->recipients));

		if (matched == list_count(c->recipients) && matched <= (from == config_uin ? count : count + 1)) {
			string_t new = string_init(NULL);
			int comma = 0;

			if (from != config_uin && !conference_participant(c, from)) {
				list_add(&c->recipients, &from, sizeof(from));

				comma++;
				string_append(new, format_user(from));
			}

			for (i = 0; i < count; i++) {
				if (recipients[i] != config_uin && !conference_participant(c, recipients[i])) {
					list_add(&c->recipients, &recipients[i], sizeof(recipients[0]));
			
					if (comma++)
						string_append(new, ", ");
					string_append(new, format_user(recipients[i]));
				}
			}

			if (strcmp(new->str, "") && !c->ignore)
				printq("conferences_joined", new->str, c->name);
			string_free(new, 1);

			gg_debug(GG_DEBUG_MISC, "// conference_find_by_uins(): matching %s\n", c->name);

			return c;
		}
	}

	return NULL;
}

/*
 * conference_set_ignore()
 *
 * ustawia stan konferencji na ignorowany lub nie.
 *
 *  - name - nazwa konferencji,
 *  - flag - 1 ignorowaæ, 0 nie ignorowaæ,
 *  - quiet.
 *
 * 0/-1
 */
int conference_set_ignore(const char *name, int flag, int quiet)
{
	struct conference *c = conference_find(name);

	if (!c) {
		printq("conferences_noexist", name);
		return -1;
	}

	c->ignore = flag;
	printq((flag ? "conferences_ignore" : "conferences_unignore"), name);

	return 0;
}

/*
 * conference_rename()
 *
 * zmienia nazwê instniej±cej konferencji.
 * 
 *  - oldname - stara nazwa,
 *  - newname - nowa nazwa,
 *  - quiet.
 *
 * 0/-1
 */
int conference_rename(const char *oldname, const char *newname, int quiet)
{
	struct conference *c;
	
	if (conference_find(newname)) {
		printq("conferences_exist", newname);
		return -1;
	}

	if (!(c = conference_find(oldname))) {
		printq("conference_noexist", oldname);
		return -1;
	}

	xfree(c->name);
	c->name = xstrdup(newname);
	remove_send_nick(oldname);
	add_send_nick(newname);
	
	printq("conferences_rename", oldname, newname);

	ui_event("conference_rename", oldname, newname, NULL);
	
	return 0;
}

/*
 * conference_free()
 *
 * zwalnia pamiêæ zajêt± przez konferencje.
 */
void conference_free()
{
	list_t l;

	if (!conferences)
		return;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;
		
		xfree(c->name);
		list_destroy(c->recipients, 1);
	}

	list_destroy(conferences, 1);
	conferences = NULL;
}

/*
 * ekg_connect()
 *
 * przygotowuje wszystko pod po³±czenie gg_login i ³±czy siê.
 */
void ekg_connect()
{
	list_t l;
	struct gg_login_params p;

	for (l = watches; l; l = l->next) {
		struct gg_dcc *d = l->data;
		
		if (d->type == GG_SESSION_DCC_SOCKET) {
			gg_dcc_port = d->port;
			
		}
	}

	memset(&p, 0, sizeof(p));

	p.uin = config_uin;
	p.password = config_password;
	p.status = config_status;
	p.status_descr = config_reason;
	p.async = 1;
	p.image_size = config_image_size < 255 ? config_image_size : 255;
#ifdef HAVE_VOIP
	p.has_audio = 1;
#endif
	p.protocol_version = config_protocol;
	p.last_sysmsg = config_last_sysmsg;
	p.protocol_features = GG_FEATURE_STATUS80 | GG_FEATURE_DND_FFC;	/* bez GG_FEATURE_MSG80 */

	if (config_server) {
		char *server, *sserver, *tmp, **servers = array_make(config_server, ",; ", 0, 1, 0);

#ifdef __GG_LIBGADU_HAVE_OPENSSL
		if (!strcasecmp(config_server, "tls")) {
			p.tls = 1;
			goto skip_server;
		}
#endif

		if (server_index >= array_count(servers))
			server_index = 0;

		if (!servers[server_index])
			goto skip_server;

		server = sserver = xstrdup(servers[server_index++]);

		if (!strncasecmp(server, "tls:", 4)) {
#ifdef __GG_LIBGADU_HAVE_OPENSSL
			p.tls = 1;
#endif
			server += 4;
		}
			
		tmp = strchr(server, ':');
			
		if (tmp) {
			p.server_port = atoi(tmp + 1);
			*tmp = 0;
			p.server_addr = inet_addr(server);
		} else {
			p.server_port = GG_DEFAULT_PORT;
			p.server_addr = inet_addr(server);
		}

		xfree(sserver);

skip_server:
		array_free(servers);
	}

	if (config_proxy_forwarding) {
		char *fwd = xstrdup(config_proxy_forwarding), *tmp = strchr(fwd, ':');

		if (!tmp) {
			p.external_addr = inet_addr(fwd);
			p.external_port = gg_dcc_port;
		} else {
			*tmp = 0;
			p.external_addr = inet_addr(fwd);
			p.external_port = atoi(tmp + 1);
		}

		xfree(fwd);
	}

	if (!config_password_cp1250)
		iso_to_cp((unsigned char *) p.password);
	if (p.status_descr)
		iso_to_cp((unsigned char *) p.status_descr);

	if (!(sess = gg_login(&p))) {
		print("conn_failed", format_find((errno == ENOMEM) ? "conn_failed_memory" : "conn_failed_connecting"));
		ekg_reconnect();
	} else
		list_add(&watches, sess, 0);

	if (!config_password_cp1250)
		cp_to_iso((unsigned char *) p.password);
	if (p.status_descr)
		cp_to_iso((unsigned char *) p.status_descr);
}

/*
 * ekg_reconnect()
 *
 * je¶li jest w³±czony autoreconnect, wywo³uje timer, który za podan±
 * ilo¶æ czasu spróbuje siê po³±czyæ jeszcze raz.
 */
void ekg_reconnect()
{
	if (config_auto_reconnect && connecting)
		reconnect_timer = time(NULL);
}

/*
 * ekg_logoff()
 *
 * roz³±cza siê, zmieniaj±c uprzednio stan na niedostêpny z opisem.
 *
 *  - sess - opis sesji,
 *  - reason - powód, mo¿e byæ NULL.
 */
void ekg_logoff(struct gg_session *sess, const char *reason)
{
	if (!sess)
		return;

	if (sess->state != GG_STATE_CONNECTED || GG_S_NA(sess->status))
		return;

	if (reason) {
		char *tmp = xstrdup(reason);
		iso_to_cp((unsigned char *) tmp);
		gg_change_status_descr(sess, GG_STATUS_NOT_AVAIL_DESCR, tmp);
		xfree(tmp);
	} else
		gg_change_status(sess, GG_STATUS_NOT_AVAIL);

	gg_logoff(sess);

	update_status();

	last_conn_event = time(NULL);
}

/*
 * ekg_hash()
 *
 * liczy prosty hash z nazwy, wykorzystywany przy przeszukiwaniu list
 * zmiennych, formatów itp.
 *
 *  - name - nazwa.
 */
int ekg_hash(const char *name)
{
	int hash = 0;

	for (; *name; name++) {
		hash ^= *name;
		hash <<= 1;
	}

	return hash;
}

/*
 * event_add()
 *
 * dodaje zdarzenie do listy zdarzeñ.
 *
 *  - flags,
 *  - target,
 *  - action,
 *  - quiet.
 *
 * 0/-1
 */
int event_add(int flags, const char *target, const char *action, int quiet)
{
        int f;
        list_t l;
        struct event e;
	uin_t uin;
	struct userlist *u;
	char **arr = NULL;

	if (!target || !action)
		return -1;

	uin = str_to_uin(target);

	if ((u = userlist_find(uin, target)))
		uin = u->uin;

	for (l = events; l; l = l->next) {
		struct event *ev = l->data;

		if (ev->target[0] == '@')
			array_add(&arr, xstrdup(ev->target));
	}

        for (l = events; l; l = l->next) {
                struct event *ev = l->data;
		int match = 0, i;

		if (target[0] == '@') {
			struct userlist *uu = userlist_find(str_to_uin(ev->target), ev->target);

			if (uu && group_member(uu, target + 1))
				match = 1;
		}

		for (i = 0; arr && arr[i]; i++) {
			if (u && group_member(u, arr[i] + 1)) {
				match = 1;
				break;
			}
		}

		if (!strcasecmp(ev->target, target) || !strcmp(ev->target, itoa(uin)) || (u && u->display && !strcasecmp(ev->target, u->display)))
			match = 1;

		if (match && (f = ev->flags & flags)) {
			printq("events_exist", event_format(f), event_format_target(target));
			array_free(arr);
			return -1;
		}
        }

	array_free(arr);

	for (f = 1; ; f++) {
		int taken = 0;
		
		for (l = events; l; l = l->next) {
			struct event *e = l->data;

			if (!strcmp(e->name, itoa(f))) {
				taken = 1;
				break;
			}
		}

		if (!taken)
			break;
	}

	e.name = xstrdup(itoa(f));
	e.target = xstrdup(target);
	e.flags = flags;
	e.action = xstrdup(action);

	list_add(&events, &e, sizeof(e));

	printq("events_add", e.name);

	return 0;
}

/*
 * event_remove()
 *
 * usuwa zdarzenie z listy zdarzeñ.
 *
 *  - name.
 *
 * 0/-1
 */
int event_remove(const char *name, int quiet)
{
	list_t l;
	int removed = 0, remove_all;
	
	if (!name)
		return -1;

	remove_all = !strcmp(name, "*");

	for (l = events; l; ) {
		struct event *e = l->data;

		l = l->next;

		if (remove_all || (name && e->name && !strcasecmp(name, e->name))) {
			if (!remove_all)
				printq("events_del", name);
			xfree(e->action);
			xfree(e->target);
			xfree(e->name);
			list_remove(&events, e, 1);
			removed = 1;
        	}
	}

	if (!removed) {
		if (remove_all)
			printq("events_list_empty");
		else
        		printq("events_del_noexist", name);

        	return -1;
	} else {
		if (remove_all)
			printq("events_del_all");
		else {
			int num = 1;

			for (l = events; l; l = l->next) {
				struct event *e = l->data;

				xfree(e->name);
				e->name = xstrdup(itoa(num++));
			}
		}

		return 0;
	}
}

/*
 * event_format()
 *
 * zwraca ³añcuch zdarzeñ w oparciu o flagi. statyczny bufor.
 *
 *  - flags.
 */
const char *event_format(int flags)
{
        static char buf[200];
	int i, first = 1;

	buf[0] = 0;

	if (flags == EVENT_ALL)
		return "*";

	for (i = 0; event_labels[i].name; i++) {
		if ((flags & event_labels[i].event)) {
			if (!first)
				strlcat(buf, ",", sizeof(buf));
			strlcat(buf, event_labels[i].name, sizeof(buf));
			first = 0;
		}
	}

	return buf;
}

/*
 * event_format_target()
 *
 * zajmuje siê sformatowaniem uina/aliasu w ³añcuchu
 * znaków ze zdarzenia. nie zwalniaæ.
 */
const char *event_format_target(const char *target)
{
	struct userlist *u;
	static char buf[200];
	uin_t uin;

	if (!target)
		return "";

	uin = str_to_uin(target);
	u = userlist_find(0, target);

	if (u)
		return format_user(u->uin);

	if (uin)
		return format_user(uin);
	else {
		strlcpy(buf, target, sizeof(buf));
		return buf;
	}
}

/*
 * event_flags()
 *
 * zwraca flagi na podstawie ³añcucha.
 *
 *  - events.
 */
int event_flags(const char *events)
{
	int i, j, flags = 0;
	char **a;

	if (!events)
		return flags;

	a = array_make(events, "|,:", 0, 1, 0);

	for (j = 0; a[j]; j++) {
		if (!strcmp(a[j], "*")) {
			flags = EVENT_ALL;
			break;
		}

		for (i = 0; event_labels[i].name; i++)
			if (!strcasecmp(a[j], event_labels[i].name))
				flags |= event_labels[i].event;
	}

	array_free(a);

	return flags;
}

/*
 * event_check()
 *
 * sprawdza i ewentualnie uruchamia akcjê na podane zdarzenie.
 *
 *  - event,
 *  - uin,
 *  - data.
 *
 * 0/-1
 */
int event_check(int event, uin_t uin, const char *data)
{
	const char *uin_number = NULL, *uin_display = NULL;
        char *action = NULL, **actions, *edata = NULL;
	struct userlist *u;
	int i;
        list_t l;

	uin_number = itoa(uin);

	if ((u = userlist_find(uin, NULL)) && u->display)
		uin_display = u->display;
	else
		uin_display = uin_number;

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

		if (e->flags & INACTIVE_EVENT)
			return 0;
		
                if (e->flags & event) {

			if (!strcmp(e->target, "*") || uin == 0)
				action = e->action;

			if (!strcmp(e->target, uin_number) || !strcasecmp(e->target, uin_display) || (u && group_member(u, e->target + 1))) {
				action = e->action;
				break;
			}
        	}
	}

        if (!action)
                return -1;

	if (data) {
		int size = 1;
		const char *p;
		char *q;

		for (p = data; *p; p++) {
			if (strchr("`!#$&*?|\\\'\"{}[]<>()\n\r", *p))
				size += 2;
			else
				size++;
		}
		
		edata = xmalloc(size);

		for (p = data, q = edata; *p; p++, q++) {
			if (strchr("`!#$&*?|\\\'\"{}[]<>()", *p))
				*q++ = '\\';
			if (*p == '\n') {
				*q++ = '\\';
				*q = 'n';
				continue;
			}
			if (*p == '\r') {
				*q++ = '\\';
				*q = 'r';
				continue;
			}
			*q = *p;
		}

		*q = 0;
	}

	actions = array_make(action, ";", 0, 0, 1);

	for (i = 0; actions && actions[i]; i++) {	
		char *tmp = format_string(actions[i], uin_number, uin_display, ((data) ? data : ""), ((edata) ? edata : ""));

		gg_debug(GG_DEBUG_MISC, "// event_check() uin: %s event: \"%s\" doing: \"%s\"\n", uin_number, event_format(event), tmp);
		command_exec(NULL, tmp, 0);
		xfree(tmp);
	}

	array_free(actions);
	xfree(edata);

        return 0;
}

/*
 * event_free()
 *
 * zwalnia pamiêæ zwi±zan± ze zdarzeniami.
 */
void event_free()
{
	list_t l;

	if (!events)
		return;

	for (l = events; l; l = l->next) {
		struct event *e = l->data;

		xfree(e->name);
		xfree(e->action);
		xfree(e->target);
	}

	list_destroy(events, 1);
	events = NULL;
}

/*
 * mesg_set()
 *
 * w³±cza/wy³±cza/sprawdza mo¿liwo¶æ pisania do naszego terminala.
 *
 *  - what - MESG_ON, MESG_OFF lub MESG_CHECK
 * 
 * -1 je¶li b³ad, lub aktualny stan: MESG_ON/MESG_OFF
*/
int mesg_set(int what)
{
	const char *tty;
	struct stat s;

	if (!(tty = ttyname(old_stderr)) || stat(tty, &s)) {
		gg_debug(GG_DEBUG_MISC, "// mesg_set() error: %s\n", strerror(errno));
		return -1;
	}

	switch (what) {
		case MESG_OFF:
			chmod(tty, s.st_mode & ~S_IWGRP);
			break;
		case MESG_ON:
			chmod(tty, s.st_mode | S_IWGRP);
			break;
		case MESG_CHECK:
			return ((s.st_mode & S_IWGRP) ? MESG_ON : MESG_OFF);
	}
	
	return 0;
}

/*
 * msg_encrypt()
 * 
 * je¶li mo¿na, podmienia wiadomo¶æ na wiadomo¶æ
 * zaszyfrowan±.
 */
int msg_encrypt(uin_t uin, unsigned char **msg)
{
#ifdef HAVE_OPENSSL
	if (config_encryption == 1 || config_encryption == 3) {
		unsigned char *res = (unsigned char *) sim_message_encrypt(*msg, uin);

		if (res) {
			int ret = 1;

			xfree(*msg);
			*msg = res;
			
			if (strlen((char *) *msg) > 1989) {
				(*msg)[1989] = 0;
				ret = 2;
			}
			
			gg_debug(GG_DEBUG_MISC, "// ekg: simlite encrypted: %s\n", res);
			return ret;
		}

		gg_debug(GG_DEBUG_MISC, "// ekg: simlite encryption failed: %s\n", sim_strerror(sim_errno));
		return 0;
	}
#endif
	return 0;
}

/*
 * charmap_prepare()
 *
 * przygotowuje mapê znaków do u¿ycia w funkcji charmap_convert() zgodnie ze 
 * wzorcem convtab. wzorzec musi mieæ parzyst± liczbê pozycji.
 *
 * - charmap - bufor do wype³nienia (musi mieæ 256 bajtów),
 * - convtab - wzorzec konwersji (przyk³ad w cp_to_iso()),
 * - convtab_sz - rozmiar wzoraca konwersji,
 * - def - czy przygotowaæ domy¶ln± mapê
 */
static void charmap_prepare(unsigned char *charmap, const unsigned char *convtab, size_t convtab_sz, int def)
{
	size_t i;

	if (convtab_sz & 1)
		return;

	if (def)
		for (i = 0; i < 256; i++)
			charmap[i] = (unsigned char) i;

	for (i = 0; i < convtab_sz;) {
		unsigned char from, to;

		from = convtab[i++];
		to = convtab[i++];

		charmap[from] = to;
	}
}

/*
 * charmap_convert()
 *
 * konwertuje bufor zgodnie z podan± map± konwersji.
 *
 * - charmap - mapa konwersji (256 bajtów),
 * - buf - bufor
 */
static void charmap_convert(const unsigned char *charmap, unsigned char *buf)
{
	if (!buf)
		return;

	for (; *buf; buf++)
		*buf = charmap[*buf];
}

/*
 * cp_to_iso()
 *
 * zamienia krzaczki pisane w cp1250 na iso-8859-2, przy okazji maskuj±c
 * znaki, których nie da siê wy¶wietliæ, za wyj±tkiem \r i \n, oraz 
 * zamieniaj±c tabulacje na spacje.
 *
 *  - buf.
 */
void cp_to_iso(unsigned char *buf)
{
	static unsigned char charmap[256];
	static int valid = 0;

	if (!valid) {
		static const unsigned char conv[] = {
			0x09, 0x20,	/* \t */
			0x0A, 0x0A,	/* \n */
			0x0D, 0x0D,	/* \r */
			0xB9, 0xB1,	/* ± */
			0x9C, 0xB6,	/* ¶ */
			0x9F, 0xBC,	/* ¼ */
			0xA5, 0xA1,	/* ¡ */
			0x8C, 0xA6,	/* ¦ */
			0x8F, 0xAC,	/* ¬ */
			0x9A, 0xB9,	/* ¹ */
			0x8A, 0xA9,	/* © */
			0x9E, 0xBE,	/* ¾ */
			0x8E, 0xAE,	/* ® */
			0x9D, 0xBB,	/* » */
			0x8D, 0xAB,	/* « */
			0xBE, 0xB5,	/* µ */
			0xBC, 0xA5,	/* ¥ */
		};
		size_t i;

		for (i = 0; i < 0x20; i++)
			charmap[i] = '?';

		for (i = 0x20; i < 0x80; i++)
			charmap[i] = (unsigned char) i;

		for (i = 0x80; i < 0xA0; i++)
			charmap[i] = '?';

		for (i = 0xA0; i < 0x100; i++)
			charmap[i] = (unsigned char) i;

		charmap_prepare(charmap, conv, sizeof(conv), 0);
		valid = 1;
	}

	charmap_convert(charmap, buf);
}

static const unsigned short table_iso_8859_2[] = {
	'?', '?', '?', '?', '?', '?', '?', '?',					/* 0x80 -      */
	'?', '?', '?', '?', '?', '?', '?', '?',					/*      - 0x8F */
	'?', '?', '?', '?', '?', '?', '?', '?',					/* 0x90 -      */
	'?', '?', '?', '?', '?', '?', '?', '?',					/*      - 0x9F */
	0x00a0, 0x0104, 0x02d8, 0x0141, 0x00a4, 0x013d, 0x015a, 0x00a7,		/* 0xA0 -      */
	0x00a8, 0x0160, 0x015e, 0x0164, 0x0179, 0x00ad, 0x017d, 0x017b,		/*      - 0xAF */
	0x00b0, 0x0105, 0x02db, 0x0142, 0x00b4, 0x013e, 0x015b, 0x02c7,		/* 0xB0 -      */
	0x00b8, 0x0161, 0x015f, 0x0165, 0x017a, 0x02dd, 0x017e, 0x017c,		/*      - 0xBF */
	0x0154, 0x00c1, 0x00c2, 0x0102, 0x00c4, 0x0139, 0x0106, 0x00c7,		/* 0xC0 -      */
	0x010c, 0x00c9, 0x0118, 0x00cb, 0x011a, 0x00cd, 0x00ce, 0x010e,		/*      - 0xCF */
	0x0110, 0x0143, 0x0147, 0x00d3, 0x00d4, 0x0150, 0x00d6, 0x00d7,		/* 0xD0 -      */
	0x0158, 0x016e, 0x00da, 0x0170, 0x00dc, 0x00dd, 0x0162, 0x00df,		/*      - 0xDF */
	0x0155, 0x00e1, 0x00e2, 0x0103, 0x00e4, 0x013a, 0x0107, 0x00e7,		/* 0xE0 -      */
	0x010d, 0x00e9, 0x0119, 0x00eb, 0x011b, 0x00ed, 0x00ee, 0x010f,		/*      - 0xEF */
	0x0111, 0x0144, 0x0148, 0x00f3, 0x00f4, 0x0151, 0x00f6, 0x00f7,		/* 0xF0 -      */
	0x0159, 0x016f, 0x00fa, 0x0171, 0x00fc, 0x00fd, 0x0163, 0x02d9		/*      - 0xFF */
};

/* utf8_to_iso() && iso_to_utf8() based on ekg2's gg_locale_to_cp() && gg_cp_to_locale() */

/* some code based on libiconv utf8_mbtowc() && utf8_wctomb() from utf8.h under LGPL-2.1 */

static int utf8_helper(unsigned char *s, int n, unsigned short *ch)
{
	unsigned char c = s[0];

	if (c < 0x80) {
		*ch = c;
		return 1;
	}

	if (c < 0xc2) 
		goto invalid;

	if (c < 0xe0) {
		if (n < 2)
			goto invalid;
		if (!((s[1] ^ 0x80) < 0x40))
			goto invalid;
		*ch = ((unsigned short) (c & 0x1f) << 6) | (unsigned short) (s[1] ^ 0x80);
		return 2;
	} 
	
	if (c < 0xf0) {
		if (n < 3)
			goto invalid;
		if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (c >= 0xe1 || s[1] >= 0xa0)))
			goto invalid;
		*ch = ((unsigned short) (c & 0x0f) << 12) | ((unsigned short) (s[1] ^ 0x80) << 6) | (unsigned short) (s[2] ^ 0x80);
		return 3;
	}

invalid:
	*ch = '?';
	return 1;
}

/*
 * utf8_to_iso()
 *
 * konwertuje utf8 na iso, zwalnia podany bufor i zwraca nowy.
 */

char *utf8_to_iso(char *b)
{
	unsigned char *buf = (unsigned char *) b;

	char *newbuf;
	int newlen = 0;
	int len;
	int i, j;

	if (!buf)
		return NULL;

	len = strlen(b);

	for (i = 0; i < len; newlen++) {
		unsigned short discard;

		i += utf8_helper(&buf[i], len - i, &discard);
	}

	newbuf = xmalloc(newlen+1);

	for (i = 0, j = 0; buf[i]; j++) {
		unsigned short znak;
		int k;

		i += utf8_helper(&buf[i], len - i, &znak);

		if (znak < 0x80) {
			newbuf[j] = znak;
			continue;
		}

		newbuf[j] = '?';

		for (k = 0; k < (sizeof(table_iso_8859_2)/sizeof(table_iso_8859_2[0])); k++) {
			if (table_iso_8859_2[k] == znak) {
				newbuf[j] = (0x80 | k);
				break;
			}
		}
	}
	newbuf[j] = '\0';

	xfree(buf);

	return newbuf;
}

/*
 * iso_to_utf8()
 *
 * konwertuje iso na utf8, zwalnia podany bufor i zwraca nowy.
 */

char *iso_to_utf8(char *b)
{
	unsigned char *buf = (unsigned char *) b;
	char *newbuf;
	int newlen = 0;
	int i, j;

	if (!buf)
		return NULL;

	for (i = 0; buf[i]; i++) {
		unsigned short znak = (buf[i] < 0x80) ? buf[i] : table_iso_8859_2[buf[i]-0x80];

		if (znak < 0x80)	newlen += 1;
		else if (znak < 0x800)	newlen += 2;
		else			newlen += 3;
	}

	newbuf = xmalloc(newlen+1);

	for (i = 0, j = 0; buf[i]; i++) {
		unsigned short znak = (buf[i] < 0x80) ? buf[i] : table_iso_8859_2[buf[i]-0x80];
		int count;

		if (znak < 0x80)	count = 1;
		else if (znak < 0x800)	count = 2;
		else			count = 3;

		switch (count) {
			case 3: newbuf[j+2] = 0x80 | (znak & 0x3f); znak = znak >> 6; znak |= 0x800;
			case 2: newbuf[j+1] = 0x80 | (znak & 0x3f); znak = znak >> 6; znak |= 0xc0;
			case 1: newbuf[j] = znak;
		}
		j += count;
	}
	newbuf[j] = '\0';

	xfree(buf);

	return newbuf;
}


/*
 * iso_to_cp()
 *
 * zamienia sensowny format kodowania polskich znaczków na bezsensowny.
 *
 *  - buf.
 */
void iso_to_cp(unsigned char *buf)
{
	static unsigned char charmap[256];
	static int valid = 0;

	if (!valid) {
		static const unsigned char conv[] = {
			0xB1, 0xB9,	/* ± */
			0xB6, 0x9C,	/* ¶ */
			0xBC, 0x9F,	/* ¼ */
			0xA1, 0xA5,	/* ¡ */
			0xA6, 0x8C,	/* ¦ */
			0xAC, 0x8F,	/* ¬ */
			0xB9, 0x9A,	/* ¹ */
			0xA9, 0x8A,	/* © */
			0xBE, 0x9E,	/* ¾ */
			0xAE, 0x8E,	/* ® */
			0xBB, 0x9D,	/* » */
			0xAB, 0x8D,	/* « */
			0xB5, 0xBE,	/* µ */
			0xA5, 0xBC,	/* ¥ */
		};

		charmap_prepare(charmap, conv, sizeof(conv), 1);
		valid = 1;
	}

	charmap_convert(charmap, buf);
}

/*
 * iso_to_ascii()
 *
 * usuwa polskie litery z tekstu.
 *
 *  - buf.
 */
void iso_to_ascii(unsigned char *buf)
{
	static unsigned char charmap[256];
	static int valid = 0;

	if (!valid) {
		static const unsigned char conv[] = {
			'ê', 'e', 
			'ó', 'o', 
			'±', 'a', 
			'¶', 's', 
			'³', 'l', 
			'¿', 'z', 
			'¼', 'z', 
			'æ', 'c', 
			'ñ', 'n', 
			'Ê', 'E', 
			'Ó', 'O', 
			'¡', 'A', 
			'¦', 'S', 
			'£', 'L',
			'¯', 'Z',
			'¬', 'Z', 
			'Æ', 'C', 
			'Ñ', 'N'
		};

		charmap_prepare(charmap, conv, sizeof(conv), 1);
		valid = 1;
	}

	charmap_convert(charmap, buf);
}

/*
 * strip_chars()
 *
 * pozbywa siê podanego znaku na pocz±tku i koñcu ³ancucha.
 */
char *strip_chars(char *line, unsigned char what)
{
	while (*line == what)
		line++;

        while (strlen(line) > 1 && line[strlen(line) - 1] == what)
        	line[strlen(line) - 1] = 0;

        return line;
}

/*
 * strip_spaces()
 *
 * pozbywa siê spacji na pocz±tku i koñcu ³añcucha.
 */
char *strip_spaces(char *line)
{
	return strip_chars(line, ' ');
}

/*
 * play_sound()
 *
 * odtwarza dzwiêk o podanej nazwie.
 *
 * 0/-1
 */
int play_sound(const char *sound_path)
{
	char *params[2];
	int res;

	if (!config_sound_app || !sound_path) {
		errno = EINVAL;
		return -1;
	}

	params[0] = saprintf("%s %s", config_sound_app, sound_path);
	params[1] = NULL;

	res = cmd_exec("exec", (const char**) params, NULL, 1);

	xfree(params[0]);

	return res;
}

/*
 * process_add()
 *
 * dopisuje do listy uruchomionych dzieci procesów.
 *
 *  - pid.
 *  - name.
 *
 * 0/-1
 */
int process_add(int pid, const char *name)
{
	struct process p;

	p.pid = pid;
	p.name = xstrdup(name);
	p.died = 0;
	
	return (list_add(&children, &p, sizeof(p)) ? 0 : -1);
}

/*
 * process_remove()
 *
 * usuwa proces z listy dzieciaków.
 *
 *  - pid.
 * 
 * 0/-1
 */
int process_remove(int pid)
{
	list_t l;

	for (l = children; l; l = l->next) {
		struct process *p = l->data;

		if (p->pid == pid) {
			xfree(p->name);
			list_remove(&children, p, 1);
			return 0;
		}
	}

	return -1;
}

/*
 * prepare_path()
 *
 * zwraca pe³n± ¶cie¿kê do podanego pliku katalogu ~/.gg/
 *
 *  - filename - nazwa pliku,
 *  - do_mkdir - czy tworzyæ katalog ~/.gg ?
 */
const char *prepare_path(const char *filename, int do_mkdir)
{
	static char path[PATH_MAX + 1];

	if (do_mkdir) {
		if (config_profile) {
			char *cd = xstrdup(config_dir);

			if (mkdir(dirname(cd), 0700) && errno != EEXIST) {
				xfree(cd);
				return NULL;
			}

			xfree(cd);
		}

		if (mkdir(config_dir, 0700) && errno != EEXIST)
			return NULL;
	}
	
	if (!filename || !*filename)
		snprintf(path, sizeof(path), "%s", config_dir);
	else
		snprintf(path, sizeof(path), "%s/%s", config_dir, filename);
	
	return path;
}

char *random_line(const char *path)
{
	int max = 0, item, tmp = 0;
	char *line;
	FILE *f;

	if (!path)
		return NULL;

	if ((f = fopen(path, "r")) == NULL)
		return NULL;
	
	while ((line = read_file(f))) {
		xfree(line);
		max++;
	}

	rewind(f);

	if (max) {
		item = rand() / (RAND_MAX / max + 1);

		while ((line = read_file(f))) {
			if (tmp == item) {
				fclose(f);
				return line;
			}
			xfree(line);
			tmp++;
		}
	}

	fclose(f);
	return NULL;
}

/*
 * read_file()
 *
 * czyta i zwraca linijkê tekstu z pliku, alokuj±c przy tym odpowiedni buforek.
 * usuwa znaki koñca linii.
 */
char *read_file(FILE *f)
{
	char buf[1024], *res = NULL;

	while (fgets(buf, sizeof(buf), f)) {
		int first = (res) ? 0 : 1;
		size_t new_size = ((res) ? strlen(res) : 0) + strlen(buf) + 1;

		res = xrealloc(res, new_size);
		if (first)
			*res = 0;
		strlcpy(res + strlen(res), buf, new_size - strlen(res));

		if (strchr(buf, '\n'))
			break;
	}

	if (res && strlen(res) > 0 && res[strlen(res) - 1] == '\n')
		res[strlen(res) - 1] = 0;
	if (res && strlen(res) > 0 && res[strlen(res) - 1] == '\r')
		res[strlen(res) - 1] = 0;

	return res;
}

/*
 * send_sms()
 *
 * wysy³a sms o podanej tre¶ci do podanej osoby.
 *
 * 0/-1
 */
int send_sms(const char *recipient, const char *message, int quiet)
{
	int pid, fd[2] = { 0, 0 };
	struct gg_exec s;
	char *tmp;

	if (!config_sms_app) {
		errno = EINVAL;
		return -1;
	}

	if (!recipient || !message) {
		errno = EINVAL;
		return -1;
	}

	if (pipe(fd))
		return -1;
		
	if (!(pid = fork())) {
		dup2(open("/dev/null", O_RDONLY), 0);

		if (fd[1]) {
			close(fd[0]);
			dup2(fd[1], 2);
			dup2(fd[1], 1);
			close(fd[1]);
		}	

		execlp(config_sms_app, config_sms_app, recipient, message, (void *) NULL);
		exit(1);
	}

	if (pid < 0) {
		close(fd[0]);
		close(fd[1]);
		return -1;
	}

	memset(&s, 0, sizeof(s));
	
	s.fd = fd[0];
	s.check = GG_CHECK_READ;
	s.state = GG_STATE_READING_DATA;
	s.type = GG_SESSION_USER3;
	s.id = pid;
	s.timeout = -1;
	s.buf = string_init(NULL);

	fcntl(s.fd, F_SETFL, O_NONBLOCK);

	list_add(&watches, &s, sizeof(s));
	close(fd[1]);
	
	tmp = saprintf(((quiet) ? "\002%s" : "\001%s"), recipient);
	process_add(pid, tmp);
	xfree(tmp);

	return 0;
}

/*
 * sms_away_add()
 *
 * dodaje osobê do listy delikwentów, od których wiadomo¶æ wys³ano sms'em
 * podczas naszej nieobecno¶ci. je¶li jest ju¿ na li¶cie, to zwiêksza 
 * przyporz±dkowany mu licznik.
 *
 *  - uin.
 */
void sms_away_add(uin_t uin)
{
	struct sms_away sa;
	list_t l;

	if (!config_sms_away_limit)
		return;

	sa.uin = uin;
	sa.count = 1;

	for (l = sms_away; l; l = l->next) {
		struct sms_away *s = l->data;

		if (s->uin == uin) {
			s->count += 1;
			return;
		}
	}

	list_add(&sms_away, &sa, sizeof(sa));
}

/*
 * sms_away_check()
 *
 * sprawdza czy wiadomo¶æ od danej osoby mo¿e zostaæ przekazana
 * na sms podczas naszej nieobecno¶ci, czy te¿ mo¿e przekroczono
 * ju¿ limit.
 *
 *  - uin
 *
 * 1 je¶li tak, 0 je¶li nie.
 */
int sms_away_check(uin_t uin)
{
	int x = 0;
	list_t l;

	if (!config_sms_away_limit || !sms_away)
		return 1;

	/* limit dotyczy ³±cznej liczby sms'ów */
	if (config_sms_away == 1) {
		for (l = sms_away; l; l = l->next) {
			struct sms_away *s = l->data;

			x += s->count;
		}
		
		if (x > config_sms_away_limit)
			return 0;
		else
			return 1;
	}

	/* limit dotyczy liczby sms'ów od jednej osoby */
	for (l = sms_away; l; l = l->next) {
		struct sms_away *s = l->data;

		if (s->uin == uin) {
			if (s->count > config_sms_away_limit)
				return 0;
			else 
				return 1;
		}
	}

	return 1;
}

/*
 * sms_away_free()
 *
 * zwalnia pamiêæ po li¶cie ,,sms_away''
 */
void sms_away_free()
{
	if (!sms_away)
		return;

	list_destroy(sms_away, 1);
	sms_away = NULL;
}

#ifdef WITH_IOCTLD

/*
 * ioctld_parse_seq()
 *
 * zamieñ string na odpowiedni± strukturê dla ioctld.
 *
 *  - seq,
 *  - data.
 *
 * 0/-1.
 */
int ioctld_parse_seq(const char *seq, struct action_data *data)
{
	char **entries;
	int i;

        if (!data || !seq)
                return -1;

	memset(data, -1, sizeof(struct action_data));

	entries = array_make(seq, ",", 0, 0, 1);

	for (i = 0; entries[i] && i < IOCTLD_MAX_ITEMS; i++) {
		int delay;
		char *tmp;
		
		if ((tmp = strchr(entries[i], '/'))) {
			*tmp = 0;
			delay = atoi(tmp + 1);
		} else
			delay = IOCTLD_DEFAULT_DELAY;
			
		data->value[i] = atoi(entries[i]);
		data->delay[i] = delay;
	}

	array_free(entries);

	return 0;
}



/*
 * ioctld_socket()
 *
 * inicjuje gniazdo dla ioctld.
 *
 *  - path - ¶cie¿ka do gniazda.
 *
 * 0/-1.
 */
int ioctld_socket(const char *path)
{
	struct sockaddr_un sockun;
	int i, usecs = 50000;

	if (ioctld_sock != -1)
		close(ioctld_sock);

	if ((ioctld_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
		return -1;

	sockun.sun_family = AF_UNIX;
	strlcpy(sockun.sun_path, path, sizeof(sockun.sun_path));

	for (i = 5; i; i--) {
		if (connect(ioctld_sock, (struct sockaddr*) &sockun, sizeof(sockun)) != -1)
			return 0;

		usleep(usecs);
	}

	close(ioctld_sock);
	ioctld_sock = -1;

        return -1;
}

/*
 * ioctld_send()
 *
 * wysy³a do ioctld polecenie uruchomienia danej akcji.
 *
 *  - seq - sekwencja danych,
 *  - act - rodzaj akcji.
 *
 * 0/-1.
 */
int ioctld_send(const char *seq, int act, int quiet)
{
	struct action_data data;

	if (*seq == '$')	/* dla kompatybilno¶ci ze starym zachowaniem */
		seq++;

	if (!xisdigit(*seq)) {
		const char *tmp = format_find(seq);

		if (!strcmp(tmp, "")) {
			printq("events_seq_not_found", seq);
			return -1;
		}

		seq = tmp;
	}

	if (ioctld_parse_seq(seq, &data)) {
		printq("events_seq_incorrect", seq);
		return -1;
	}

	data.act = act;

	return send(ioctld_sock, &data, sizeof(data), 0);
}

#endif /* WITH_IOCTLD */

/*
 * init_control_pipe()
 *
 * inicjuje potok nazwany do zewnêtrznej kontroli ekg.
 *
 *  - pipe_file.
 *
 * zwraca deskryptor otwartego potoku lub -1.
 */
int init_control_pipe(const char *pipe_file)
{
	int fd;
	struct stat st;
	char *err_str = NULL;

	if (!pipe_file)
		return 0;

	if (!stat(pipe_file, &st) && !S_ISFIFO(st.st_mode))
		err_str = saprintf("Plik %s nie jest potokiem. Ignorujê.\n", pipe_file);

	if (mkfifo(pipe_file, config_files_mode_config) < 0 && errno != EEXIST)
		err_str = saprintf("Nie mogê stworzyæ potoku %s: %s. Ignorujê.\n", pipe_file, strerror(errno));

	if ((fd = open(pipe_file, O_RDWR | O_NONBLOCK)) < 0)
		err_str = saprintf("Nie mogê otworzyæ potoku %s: %s. Ignorujê.\n", pipe_file, strerror(errno));

	if (err_str) {
		print("generic_error", err_str);
		xfree(err_str);
		return -1;
	}

	return fd;
}

/*
 * timestamp()
 *
 * zwraca statyczny buforek z ³adnie sformatowanym czasem.
 *
 *  - format.
 */
const char *timestamp(const char *format)
{
	static char buf[100];
	time_t t;
	struct tm *tm;

	time(&t);
	tm = localtime(&t);
	strftime(buf, sizeof(buf), format, tm);

	return buf;
}

/*
 * timestamp_time()
 *
 * jak wyzej, ale na podstawie podanego czasu.
 *
 * - format
 * - czas
 */
const char *timestamp_time(const char *format, time_t t)
{
	struct tm *tm;
	static char buf[100];

	if (!format || format[0] == '\0')
		return itoa(t);

	tm = localtime(&t);

	if (!strftime(buf, sizeof(buf), format, tm))
		return "TOOLONG";
	return buf;
}

/*
 * unidle()
 *
 * uaktualnia licznik czasu ostatniej akcji, ¿eby przypadkiem nie zrobi³o
 * autoawaya, kiedy piszemy.
 */
void unidle()
{
	time(&last_action);
}

/*
 * transfer_id()
 *
 * zwraca pierwszy wolny identyfikator transferu dcc.
 */
int transfer_id()
{
	list_t l;
	int id = 1;

	for (l = transfers; l; l = l->next) {
		struct transfer *t = l->data;

		if (t->id >= id)
			id = t->id + 1;
	}

	return id;
}

/*
 * on_off()
 *
 * zwraca 1 je¶li tekst znaczy w³±czyæ, 0 je¶li wy³±czyæ, -1 je¶li co innego.
 *
 *  - value.
 */
int on_off(const char *value)
{
	if (!value)
		return -1;

	if (!strcasecmp(value, "on") || !strcasecmp(value, "true") || !strcasecmp(value, "yes") || !strcasecmp(value, "tak") || !strcmp(value, "1"))
		return 1;

	if (!strcasecmp(value, "off") || !strcasecmp(value, "false") || !strcasecmp(value, "no") || !strcasecmp(value, "nie") || !strcmp(value, "0"))
		return 0;

	return -1;
}

/*
 * timer_add()
 *
 * dodaje timera.
 *
 *  - period - za jaki czas w sekundach ma byæ uruchomiony,
 *  - persistent - czy sta³y timer,
 *  - type - rodzaj timera,
 *  - at - zwyk³y timer czy at?
 *  - name - nazwa timera w celach identyfikacji. je¶li jest równa NULL,
 *           zostanie przyznany pierwszy numerek z brzegu.
 *  - command - komenda wywo³ywana po up³yniêciu czasu.
 *
 * zwraca zaalokowan± struct timer lub NULL.
 */
struct timer *timer_add(time_t period, int persistent, int type, int at, const char *name, const char *command)
{
	struct timer t;
	struct timeval tv;
	struct timezone tz;

	if (!name) {
		int i;

		for (i = 1; ; i++) {
			int gotit = 0;
			list_t l;

			for (l = timers; l; l = l->next) {
				struct timer *tt = l->data;

				if (!strcmp(tt->name, itoa(i))) {
					gotit = 1;
					break;
				}
			}

			if (!gotit)
				break;
		}

		name = itoa(i);
	}

	memset(&t, 0, sizeof(t));

	gettimeofday(&tv, &tz);
	tv.tv_sec += period;
	memcpy(&t.ends, &tv, sizeof(tv));
	t.period = period;
	t.name = xstrdup(name);
	t.command = xstrdup(command);
	t.type = type;
	t.at = at;
	t.persistent = persistent;

	return list_add(&timers, &t, sizeof(t));
}

/*
 * timer_remove()
 *
 * usuwa timer.
 *
 *  - name - nazwa timera, mo¿e byæ NULL,
 *  - at - zwyk³y timer czy at?
 *  - command - komenda timera, mo¿e byæ NULL.
 *
 * 0/-1
 */
int timer_remove(const char *name, int at, const char *command)
{
	list_t l;
	int removed = 0;

	for (l = timers; l; ) {
		struct timer *t = l->data;

		l = l->next;

		if ((at == t->at) && ((name && !strcasecmp(name, t->name)) || (command && !strcasecmp(command, t->command)))) { 
			xfree(t->name);
			xfree(t->command);
			xfree(t->id);
			list_remove(&timers, t, 1);
			removed = 1;
		}
	}

	return ((removed) ? 0 : -1);
}

/*
 * timer_remove_user()
 *
 * usuwa wszystkie timery u¿ytkownika.
 *
 *  - at - czy to at? nie ma znaczenia, je¶li równe -1
 *
 * 0/-1
 */
int timer_remove_user(int at)
{
	list_t l;
	int removed = 0;

	for (l = timers; l; ) {
		struct timer *t = l->data;

		l = l->next;

		if (t->type == TIMER_COMMAND && (at == -1 || at == t->at)) { 
			xfree(t->name);
			xfree(t->command);
			xfree(t->id);
			list_remove(&timers, t, 1);
			removed = 1;
		}
	}

	return ((removed) ? 0 : -1);
}

/*
 * timer_free()
 *
 * zwalnia pamiêæ po timerach.
 */
void timer_free()
{
	list_t l;

	for (l = timers; l; l = l->next) {
		struct timer *t = l->data;
		
		xfree(t->name);
		xfree(t->command);
		xfree(t->id);
	}

	list_destroy(timers, 1);
	timers = NULL;
}

/* 
 * xstrmid()
 *
 * wycina fragment tekstu alokuj±c dla niego pamiêæ.
 *
 *  - str - tekst ¼ród³owy,
 *  - start - pierwszy znak,
 *  - length - d³ugo¶æ wycinanego tekstu, je¶li -1 do koñca.
 */
char *xstrmid(const char *str, int start, int length)
{
	char *res, *q;
	const char *p;

	if (!str)
		return xstrdup("");

	if (start > strlen(str))
		start = strlen(str);

	if (length == -1)
		length = strlen(str) - start;

	if (length < 1)
		return xstrdup("");

	if (length > strlen(str) - start)
		length = strlen(str) - start;
	
	res = xmalloc(length + 1);
	
	for (p = str + start, q = res; length; p++, q++, length--)
		*q = *p;

	*q = 0;

	return res;
}

/*
 * http_error_string()
 *
 * zwraca tekst opisuj±cy powód b³êdu us³ug po http.
 *
 *  - h - warto¶æ gg_http->error lub 0, gdy funkcja zwróci³a NULL.
 */
const char *http_error_string(int h)
{
	switch (h) {
		case 0:
			return format_find((errno == ENOMEM) ? "http_failed_memory" : "http_failed_connecting");
		case GG_ERROR_RESOLVING:
			return format_find("http_failed_resolving");
		case GG_ERROR_CONNECTING:
			return format_find("http_failed_connecting");
		case GG_ERROR_READING:
			return format_find("http_failed_reading");
		case GG_ERROR_WRITING:
			return format_find("http_failed_writing");
	}

	return "?";
}

/*
 * update_status()
 *
 * uaktualnia w³asny stan w li¶cie kontaktów, je¶li dopisali¶my siebie.
 */
void update_status()
{
	struct userlist *u = userlist_find(config_uin, NULL);

	if (!u)
		return;

	if ((u->descr && config_reason && strcmp(u->descr, config_reason)) || (!u->descr && config_reason) || (u->descr && !config_reason))
		event_check(EVENT_DESCR, config_uin, config_reason);

	xfree(u->descr);
	u->descr = xstrdup(config_reason);

	if (ignored_check(u->uin) & IGNORE_STATUS) {
		u->status = GG_STATUS_NOT_AVAIL;
		return;
	}

	if (!sess || sess->state != GG_STATE_CONNECTED)
		u->status = (u->descr) ? GG_STATUS_NOT_AVAIL_DESCR : GG_STATUS_NOT_AVAIL;
	else
		u->status = GG_S(config_status);

	if (ignored_check(u->uin) & IGNORE_STATUS_DESCR)
		u->status = ekg_hide_descr_status(u->status);
}

/*
 * update_status_myip()
 *
 * zajmuje siê wpisaniem w³asnego adresu IP i portu na li¶cie.
 */
void update_status_myip()
{
	struct userlist *u = userlist_find(config_uin, NULL);

	if (!u)
		return;

	if (!sess || sess->state != GG_STATE_CONNECTED)
		goto fail;
	
	if (config_dcc && config_dcc_ip && config_dcc_port) {
		list_t l;

		if (strcmp(config_dcc_ip, "auto"))
			u->ip.s_addr = inet_addr(config_dcc_ip);
		else {
			struct sockaddr_in foo;
			socklen_t bar = sizeof(foo);

			if (getsockname(sess->fd, (struct sockaddr *) &foo, &bar))
				goto fail;

			u->ip.s_addr = foo.sin_addr.s_addr; 
		}

		u->port = 0;

		for (l = watches; l; l = l->next) {
			struct gg_dcc *d = l->data;

			if (d->type == GG_SESSION_DCC_SOCKET) {
				u->port = d->port;
				break;
			}
		}

		return;
	}

fail:
	memset(&u->ip, 0, sizeof(struct in_addr));
	u->port = 0;
}

/*
 * change_status()
 *
 * zmienia stan sesji.
 *
 *  - status - nowy stan. warto¶æ stanu libgadu, bez lub z _DESCR.
 *  - reason - opis stanu, mo¿e byæ NULL.
 *  - autom - czy zmiana jest automatyczna?
 */
void change_status(int status, const char *xarg, int autom)
{
	const char *filename, *config_x_reason, *format, *format_descr, *auto_format = NULL, *auto_format_descr = NULL;
	int random_mask, status_descr;
	char *reason = NULL, *tmp = NULL, *arg;
	int private_mask = (GG_S_F(config_status) ? GG_STATUS_FRIENDS_MASK : 0);

	status = GG_S(status);
	arg = xstrdup(xarg);

	switch (status) {
		case GG_STATUS_BUSY:
		case GG_STATUS_BUSY_DESCR:
			status_descr = GG_STATUS_BUSY_DESCR;
			random_mask = 1;
			filename = "away.reasons";
			config_x_reason = config_away_reason;
			format = "away";
			format_descr = "away_descr";
			auto_format = "auto_away";
			auto_format_descr = "auto_away_descr";
			break;
		case GG_STATUS_AVAIL:
		case GG_STATUS_AVAIL_DESCR:
			status_descr = GG_STATUS_AVAIL_DESCR;
			random_mask = 4;
			filename = "back.reasons";
			config_x_reason = config_back_reason;
			format = "back";
			format_descr = "back_descr";
			auto_format = "auto_back";
			auto_format_descr = "auto_back_descr";
			break;
		case GG_STATUS_DND:
		case GG_STATUS_DND_DESCR:
			status_descr = GG_STATUS_DND_DESCR;
			random_mask = 4;
			filename = "dnd.reasons";
			config_x_reason = config_dnd_reason;
			format = "dnd";
			format_descr = "dnd_descr";
			auto_format = "auto_dnd";
			auto_format_descr = "auto_dnd_descr";
			break;
		case GG_STATUS_FFC:
		case GG_STATUS_FFC_DESCR:
			status_descr = GG_STATUS_FFC_DESCR;
			random_mask = 4;
			filename = "ffc.reasons";
			config_x_reason = config_ffc_reason;
			format = "ffc";
			format_descr = "ffc_descr";
			auto_format = "auto_ffc";
			auto_format_descr = "auto_ffc_descr";
			break;
		case GG_STATUS_INVISIBLE:
		case GG_STATUS_INVISIBLE_DESCR:
			status_descr = GG_STATUS_INVISIBLE_DESCR;
			random_mask = 8;
			filename = "quit.reasons";
			config_x_reason = config_quit_reason;
			format = "invisible";
			format_descr = "invisible_descr";
			break;
		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_NOT_AVAIL_DESCR:
			status_descr = GG_STATUS_NOT_AVAIL_DESCR;
			random_mask = 8;
			filename = "quit.reasons";
			config_x_reason = config_quit_reason;
			format = "invisible";
			format_descr = "invisible_descr";
			break;
		default:
			return;
	}

#ifdef WITH_PYTHON
	PYTHON_HANDLE_HEADER(status_own, "(is)", status, arg)
	{
		char *a;

		PYTHON_HANDLE_RESULT("is", &status, &a)
		{
			xfree(arg);
			arg = xstrdup(a);
		}
	}

	PYTHON_HANDLE_FOOTER()

	if (!python_handle_result) {
		xfree(arg);
		return;
	}
#endif

	if (!(config_auto_away % 60))
		tmp = saprintf("%dm", config_auto_away / 60);
	else
		tmp = saprintf("%ds", config_auto_away);
								
	if (!arg) {
		if (config_random_reason & random_mask) {
			reason = random_line(prepare_path(filename, 0));
			if (!reason && config_x_reason)
			    	reason = xstrdup(config_x_reason);
		} else if (config_x_reason)
		    	reason = xstrdup(config_x_reason);
	} else
		reason = xstrdup(arg);

	if (!reason && config_keep_reason && config_reason)
		reason = xstrdup(config_reason);
	
	if (arg && !strcmp(arg, "-")) {
		xfree(reason);
		reason = NULL;
		status = ekg_hide_descr_status(status);
	}

	if (reason)
		status = status_descr;

	if (reason) {
		char *r1, *r2;

		r1 = xstrmid(reason, 0, GG_STATUS_DESCR_MAXSIZE);
		r2 = xstrmid(reason, GG_STATUS_DESCR_MAXSIZE, -1);

		switch (autom) {
			case 0:
				print(format_descr, r1, r2);
				break;
			case 1:
				print(auto_format_descr, tmp, r1, r2);
				break;
			case 2:
				break;
		}
	
		xfree(reason);
		reason = r1;

		xfree(r2);
	} else {
		switch (autom) {
			case 0:
				print(format);
				break;
			case 1:
				print(auto_format, tmp);
				break;
			case 2:
				break;
		}
	}

	ui_event("my_status", (reason) ? format_descr : format, reason, NULL);
	ui_event("my_status_raw", status, reason, NULL);

	xfree(config_reason);
	config_reason = reason;
	config_status = status | private_mask;

	if (sess && sess->state == GG_STATE_CONNECTED) {
		if (config_reason) {
		    	iso_to_cp((unsigned char *) config_reason);
			gg_change_status_descr(sess, config_status, config_reason);
			cp_to_iso((unsigned char *) config_reason);
		} else
		    	gg_change_status(sess, config_status);
	}
	
	xfree(tmp);
	xfree(arg);

	update_status();
}

/*
 * ekg_status_label()
 *
 * tworzy etykietê opisuj±c± stan, z podanym prefiksem.
 *
 *  - status,
 *  - prefix.
 *
 * zwraca statyczny tekst.
 */
const char *ekg_status_label(int status, const char *prefix)
{
	static char buf[100];
	const char *label = "unknown";
	
	switch (GG_S(status)) {
		case GG_STATUS_AVAIL:
			label = "avail";
			break;
		case GG_STATUS_AVAIL_DESCR:
			label = "avail_descr";
			break;
		case GG_STATUS_NOT_AVAIL:
			label = "not_avail";
			break;
		case GG_STATUS_NOT_AVAIL_DESCR:
			label = "not_avail_descr";
			break;
		case GG_STATUS_BUSY:
			label = "busy";
			break;
		case GG_STATUS_BUSY_DESCR:
			label = "busy_descr";
			break;
		case GG_STATUS_FFC:
			label = "ffc";
			break;
		case GG_STATUS_FFC_DESCR:
			label = "ffc_descr";
			break;
		case GG_STATUS_DND:
			label = "dnd";
			break;
		case GG_STATUS_DND_DESCR:
			label = "dnd_descr";
			break;
		case GG_STATUS_INVISIBLE:
			label = "invisible";
			break;
		case GG_STATUS_INVISIBLE_DESCR:
			label = "invisible_descr";
			break;
		case GG_STATUS_BLOCKED:
			label = "blocked";
			break;
	}

	snprintf(buf, sizeof(buf), "%s%s", (prefix) ? prefix : "", label);

	return buf;
}

/*
 * ekg_hide_descr_status()
 *
 * je¶li dany status jest opisowy, zwraca
 * taki sam status bez opisu.
 */
int ekg_hide_descr_status(int status)
{
	int ret = status, private_mask = (GG_S_F(status) ? GG_STATUS_FRIENDS_MASK : 0);

	switch (GG_S(status)) {
		case GG_STATUS_AVAIL_DESCR:
			ret = GG_STATUS_AVAIL;
			break;
		case GG_STATUS_BUSY_DESCR:
			ret = GG_STATUS_BUSY;
			break;
		case GG_STATUS_INVISIBLE_DESCR:
			ret = GG_STATUS_INVISIBLE;
			break;
		case GG_STATUS_NOT_AVAIL_DESCR:
			ret = GG_STATUS_NOT_AVAIL;
			break;
	}

	return (ret | private_mask);
}

struct color_map default_color_map[26] = {
	{ 'k', 0, 0, 0 },
	{ 'r', 168, 0, 0, },
	{ 'g', 0, 168, 0, },
	{ 'y', 168, 168, 0, },
	{ 'b', 0, 0, 168, },
	{ 'm', 168, 0, 168, },
	{ 'c', 0, 168, 168, },
	{ 'w', 168, 168, 168, },
	{ 'K', 96, 96, 96 },
	{ 'R', 255, 0, 0, },
	{ 'G', 0, 255, 0, },
	{ 'Y', 255, 255, 0, },
	{ 'B', 0, 0, 255, },
	{ 'M', 255, 0, 255, },
	{ 'C', 0, 255, 255, },
	{ 'W', 255, 255, 255, },

	/* dodatkowe mapowanie ró¿nych kolorów istniej±cych w GG */
	{ 'C', 128, 255, 255, },
	{ 'G', 128, 255, 128, },
	{ 'M', 255, 128, 255, },
	{ 'B', 128, 128, 255, },
	{ 'R', 255, 128, 128, },
	{ 'Y', 255, 255, 128, }, 
	{ 'm', 168, 128, 168, },
	{ 'c', 128, 168, 168, },
	{ 'g', 64, 168, 64, },
	{ 'm', 128, 64, 128, }
};

/*
 * color_map()
 *
 * funkcja zwracaj±ca kod koloru z domy¶lnej 16-kolorowej palety terminali
 * ansi odpadaj±cemu podanym warto¶ciom RGB.
 */
char color_map(unsigned char r, unsigned char g, unsigned char b)
{
	unsigned long mindist = 255 * 255 * 255;
	struct color_map *map = default_color_map;
	char ch = 0;
	int i;

/*	gg_debug(GG_DEBUG_MISC, "color=%.2x%.2x%.2x\n", r, g, b); */

#define __sq(x) ((x)*(x))
	for (i = 0; i < 26; i++) {
		unsigned long dist = __sq(r - map[i].r) + __sq(g - map[i].g) + __sq(b - map[i].b);

/*		gg_debug(GG_DEBUG_MISC, "%d(%c)=%.2x%.2x%.2x, dist=%ld\n", i, map[i].color, map[i].r, map[i].g, map[i].b, dist); */

		if (dist < mindist) {
			ch = map[i].color;
			mindist = dist;
		}
	}
#undef __sq

/*	gg_debug(GG_DEBUG_MISC, "mindist=%ld, color=%c\n", mindist, ch); */

	return ch;	
}

/*
 * strcasestr()
 *
 * robi to samo co strstr() tyle ¿e bez zwracania uwagi na wielko¶æ
 * znaków.
 */
char *strcasestr(const char *haystack, const char *needle)
{
	int i, hlen = strlen(haystack), nlen = strlen(needle);

	for (i = 0; i <= hlen - nlen; i++) {
		if (!strncasecmp(haystack + i, needle, nlen))
			return (char*) (haystack + i);
	}

	return NULL;
}

/*
 * say_it()
 *
 * zajmuje siê wypowiadaniem tekstu, uwa¿aj±c na ju¿ dzia³aj±cy
 * syntezator w tle.
 *
 * 0/-1/-2. -2 w przypadku, gdy dodano do bufora.
 */
int say_it(const char *str)
{
	pid_t pid;

	if (!config_speech_app || !str || !strcmp(str, ""))
		return -1;

	if (speech_pid) {
		buffer_add(BUFFER_SPEECH, NULL, str, 50);
		return -2;
	}

	if ((pid = fork()) < 0)
		return -1;

	speech_pid = pid;

	if (!pid) {
		char *tmp = saprintf("%s 2>/dev/null 1>&2", config_speech_app);
		FILE *f = popen(tmp, "w");
		int status = -1;

		xfree(tmp);

		if (f) {
			fprintf(f, "%s.", str);
			status = pclose(f);	/* dzieciak czeka na dzieciaka */
		}

		exit(status);
	}

	process_add(pid, "\003");
	return 0;
}

/*
 * parsetimestr()
 *
 * przekszta³ca zapis czasu w formacie [[[yyyy]mm]dd]HH[:]MM[.SS]
 * na ,,time_t''. zwraca -1 w przypadku b³êdu.
 */
time_t parsetimestr(const char *p)
{
	struct tm *lt;
	time_t now = time(NULL);
	char *tmp, *foo = xstrdup(p);
	int wrong = 0;

	lt = localtime(&now);
	lt->tm_isdst = -1;

	/* wyci±gamy sekundy, je¶li s± i obcinamy */
	if ((tmp = strchr(foo, '.')) && !(wrong = (strlen(tmp) < 2))) {
		sscanf(tmp + 1, "%2d", &lt->tm_sec);
		tmp[0] = 0;
	} else
		lt->tm_sec = 0;

	/* pozb±d¼my siê dwukropka */
	if ((tmp = strchr(foo, ':')) && !(wrong = (strlen(tmp) != 3))) {
		tmp[0] = tmp[1];
		tmp[1] = tmp[2];
		tmp[2] = 0;
	}

	/* jedziemy ... */
	if (!wrong) {
		switch (strlen(foo)) {
			int ret;

			case 12:
				ret = sscanf(foo, "%4d%2d%2d%2d%2d", &lt->tm_year, &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
				if (ret != 5)
					wrong = 1;
				lt->tm_year -= 1900;
				lt->tm_mon -= 1;
				break;
			case 10:
				ret = sscanf(foo, "%2d%2d%2d%2d%2d", &lt->tm_year, &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
				if (ret != 5)
					wrong = 1;
				lt->tm_year += 100;
				lt->tm_mon -= 1;
				break;
			case 8:
				ret = sscanf(foo, "%2d%2d%2d%2d", &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
				if (ret != 4)
					wrong = 1;
				lt->tm_mon -= 1;
				break;
			case 6:
				ret = sscanf(foo, "%2d%2d%2d", &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
				if (ret != 3)
					wrong = 1;
				break;	
			case 4:
				ret = sscanf(foo, "%2d%2d", &lt->tm_hour, &lt->tm_min);
				if (ret != 2)
					wrong = 1;
				break;
			default:
				wrong = 1;
		}
	}

	xfree(foo);

	/* nie ma b³êdów ? */
	if (wrong || lt->tm_hour > 23 || lt->tm_min > 59 || lt->tm_sec > 59 || lt->tm_mday > 31 || !lt->tm_mday || lt->tm_mon > 11)
		return -1;
	else
		return mktime(lt);
}

/*
 * unique_name()
 *
 * je¶li jest w³±czone config_dcc_backups, to tworzy unikaln± ¶cie¿kê 
 * do pliku, dodaj±c sufiksy od 1 do 1000.
 *
 * - path: ¶cie¿ka do zmiany
 *
 * zwraca:
 *  - NULL: nie uda³o siê utworzyæ backupu
 *  - path: backup nie by³ potrzebny lub config_dcc_backups == 0
 *  - pozosta³e warto¶ci: wska¼nik do nowej ¶cie¿ki
 */
unsigned char *unique_name (unsigned char *path)
{
	struct stat st;
	int num;
	unsigned char *newpath = NULL;	/* ¿eby nie by³o warninga */

	if (!config_dcc_backups || stat((char *) path, &st))
		return path;

	for (num = 1; num < 1000; num++) {
		newpath = (unsigned char *) saprintf("%s.%d", path, num);

		if (stat((char *) newpath, &st) == -1)
			break;

		xfree(newpath);
	}

	return (num == 1000) ? NULL : newpath;
}

