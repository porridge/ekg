/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *                          Wojciech Bojdo³ <wojboj@htc.net.pl>
 *                          Piotr Wysocki <wysek@linux.bydg.org>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#include <stdio.h>
#include <unistd.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "config.h"
#include "libgadu.h"
#include "stuff.h"
#include "dynstuff.h"
#include "commands.h"
#include "events.h"
#include "themes.h"
#include "vars.h"
#include "userlist.h"
#include "version.h"
#include "voice.h"
#include "xmalloc.h"
#include "ui.h"
#ifdef WITH_PYTHON
#  include "python.h"
#endif
#ifdef HAVE_OPENSSL
#  include "sim.h"
#endif
#include "msgqueue.h"

COMMAND(cmd_modify);

char *send_nicks[SEND_NICKS_MAX] = { NULL };
int send_nicks_count = 0, send_nicks_index = 0;
int quit_command = 0;

list_t commands = NULL;

int match_arg(const char *arg, char shortopt, const char *longopt, int longoptlen)
{
	if (!arg || *arg != '-')
		return 0;

	arg++;

	if (*arg == '-') {
		int len = strlen(++arg);

		if (longoptlen > len)
			len = longoptlen;

		return !strncmp(arg, longopt, len);
	}
	
	return (*arg == shortopt) && (*(arg + 1) == 0);
}

/*
 * add_send_nick()
 *
 * dodaje do listy nicków dope³nianych automagicznie tabem.
 */
void add_send_nick(const char *nick)
{
	int i;

	for (i = 0; i < send_nicks_count; i++)
		if (send_nicks[i] && !strcmp(nick, send_nicks[i])) {
			remove_send_nick(nick);
			break;
		}

	if (send_nicks_count == SEND_NICKS_MAX) {
		xfree(send_nicks[SEND_NICKS_MAX - 1]);
		send_nicks_count--;
	}

	for (i = send_nicks_count; i > 0; i--)
		send_nicks[i] = send_nicks[i - 1];

	if (send_nicks_count != SEND_NICKS_MAX)
		send_nicks_count++;
	
	send_nicks[0] = xstrdup(nick);
}

/*
 * remove_send_nick()
 *
 * usuwa z listy dope³nianych automagicznie tabem.
 */
void remove_send_nick(const char *nick)
{
	int i, j;

	for (i = 0; i < send_nicks_count; i++) {
		if (send_nicks[i] && !strcmp(send_nicks[i], nick)) {
			xfree(send_nicks[i]);

			for (j = i + 1; j < send_nicks_count; j++)
				send_nicks[j - 1] = send_nicks[j];

			send_nicks_count--;
			send_nicks[send_nicks_count] = NULL;

			break;
		}
	}
}

COMMAND(cmd_cleartab)
{
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		xfree(send_nicks[i]);
		send_nicks[i] = NULL;
	}

	send_nicks_count = 0;
	send_nicks_index = 0;
}

COMMAND(cmd_add)
{
	uin_t uin;
	struct userlist *u;

	if (!params[0] || !params[1]) {
		print("not_enough_params", name);
		return;
	}

	if (!(uin = str_to_uin(params[0]))) {
		print("invalid_uin");
		return;
	}

	if ((u = userlist_find(uin, params[1]))) {
		if (!strcmp(params[1], u->display))
			print("user_exists", params[1]);
		else
			print("user_exists_other", params[1], format_user(uin));

		return;
	}

	if (userlist_add(uin, params[1])) {
		print("user_added", params[1]);
		gg_add_notify(sess, uin);
		config_changed = 1;
		ui_event("userlist_changed", itoa(uin), params[1]);
	} else
		print("error_adding");

	if (params[2]) {
		params++;
		cmd_modify("add", params, NULL);
	}
}

COMMAND(cmd_alias)
{
	if (!params[0] || match_arg(params[0], 'l', "list", 2)) {
		list_t l;
		int count = 0;

		for (l = aliases; l; l = l->next) {
			struct alias *a = l->data;
			list_t m;
			int first = 1, i;
			char *tmp = xcalloc(strlen(a->name) + 1, 1);
			
			for (i = 0; i < strlen(a->name); i++)
				strcat(tmp, " ");

			for (m = a->commands; m; m = m->next) {
				print((first) ? "aliases_list" : "aliases_list_next", a->name, (char*) m->data, tmp);
				first = 0;
				count++;
			}

			xfree(tmp);
		}

		if (!count)
			print("aliases_list_empty");

		return;
	}

	if (match_arg(params[0], 'a', "add", 2)) {
		if (!alias_add(params[1], 0, 0))
			config_changed = 1;

		return;
	}

	if (match_arg(params[0], 'A', "append", 2)) {
		if (!alias_add(params[1], 0, 1))
			config_changed = 1;

		return;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		if (!alias_remove(params[1]))
			config_changed = 1;

		return;
	}

	print("aliases_invalid");
}

COMMAND(cmd_away)
{
	unidle();

	if (params[0] && strlen(params[0]) > GG_STATUS_DESCR_MAXSIZE)
		print("descr_too_long", itoa(strlen(params[0]) - GG_STATUS_DESCR_MAXSIZE));

	if (!strcasecmp(name, "away"))
		change_status(GG_STATUS_BUSY, params[0], 0);

	if (!strcasecmp(name, "invisible"))
		change_status(GG_STATUS_INVISIBLE, params[0], 0);

	if (!strcasecmp(name, "back")) {
		change_status(GG_STATUS_AVAIL, params[0], 0);
		sms_away_free();
	}

	/* XXX mo¿e by tak do osobnej funkcji? */

	if (!strcasecmp(name, "private")) {
		int tmp;

		if (!params[0]) {
			print((private_mode) ? "private_mode_is_on" : "private_mode_is_off");
			return;
		}
		
		if ((tmp = on_off(params[0])) == -1) {
			print("private_mode_invalid");
			return;
		}

		private_mode = tmp;
		print((private_mode) ? "private_mode_on" : "private_mode_off");
		ui_event("my_status", "private", (private_mode) ? "on" : "off");

		config_status = config_status & ~GG_STATUS_FRIENDS_MASK;
		config_status |= ((private_mode) ? GG_STATUS_FRIENDS_MASK : 0);

		if (sess && sess->state == GG_STATE_CONNECTED) {
			gg_debug(GG_DEBUG_MISC, "-- config_status = 0x%.2x\n", config_status);

			if (config_reason) {
				iso_to_cp(config_reason);
				gg_change_status_descr(sess, config_status, config_reason);
				cp_to_iso(config_reason);
			} else
				gg_change_status(sess, config_status);
		}
	}
}

COMMAND(cmd_status)
{
	struct userlist *u;
	struct in_addr i;
	struct tm *lce;
	int mqc;
	char *tmp, *priv, *r1, *r2, buf[100], *status_table[6] = {
		"show_status_avail",
		"show_status_busy",
		"show_status_invisible",
		"show_status_busy_descr",
		"show_status_avail_descr",
		"show_status_invisible_descr"
	};
	
	if (config_user && strcmp(config_user, ""))
		print("show_status_profile", config_user);

	if ((u = userlist_find(config_uin, NULL)) && u->display)
		print("show_status_uin_nick", itoa(config_uin), u->display);
	else
		print("show_status_uin", itoa(config_uin));
	
	lce = localtime((const time_t*) &last_conn_event);
	strftime(buf, sizeof(buf), format_find((last_conn_event / 86400 == time(NULL) / 86400) ? "show_status_last_conn_event_today" : "show_status_last_conn_event"), (const struct tm*) lce);

	if (!sess || sess->state != GG_STATE_CONNECTED) {
		char *tmp = format_string(format_find("show_status_not_avail"));

		print("show_status_status", tmp, "");

		if (last_conn_event)
			print("show_status_disconnected_since", buf);
		if ((mqc = msg_queue_count()))
			print("show_status_msg_queue", itoa(mqc)); 

		xfree(tmp);

		return;
	}

	if (private_mode)
		priv = format_string(format_find("show_status_private_on"));
	else
		priv = format_string(format_find("show_status_private_off"));

	r1 = xstrmid(config_reason, 0, GG_STATUS_DESCR_MAXSIZE);
	r2 = xstrmid(config_reason, GG_STATUS_DESCR_MAXSIZE, -1);

	tmp = format_string(format_find(status_table[away]), r1, r2);

	xfree(r1);
	xfree(r2);
	
	i.s_addr = sess->server_addr;

	print("show_status_status", tmp, priv);
	print("show_status_server", inet_ntoa(i), itoa(sess->port));
	print("show_status_connected_since", buf);

	xfree(tmp);
	xfree(priv);
}

COMMAND(cmd_connect)
{
	if (!strcasecmp(name, "connect")) {
		if (sess) {
			print((sess->state == GG_STATE_CONNECTED) ? "already_connected" : "during_connect");
			return;
		}

		if (params && params[0] && params[1]) {
			variable_set("uin", params[0], 0);
			variable_set("password", params[1], 0);
		}

		if (params && params[0] && !params[1])
			variable_set("password", params[0], 0);
			
                if (config_uin && config_password) {
			print("connecting");
			connecting = 1;
			do_connect();
		} else
			print("no_config");
	} else if (!strcasecmp(name, "reconnect")) {
		cmd_connect("disconnect", NULL, NULL);
		cmd_connect("connect", NULL, NULL);
	} else if (sess) {
	    	char *tmp = NULL;

		if (!params || !params[0]) {
		    	if (config_random_reason & 2) {
				tmp = random_line(prepare_path("quit.reasons", 0));
				if (!tmp && config_quit_reason)
					tmp = xstrdup(config_quit_reason);
			} else if (config_quit_reason)
				tmp = xstrdup(config_quit_reason);
		} else
		    	tmp = xstrdup(params[0]);

		if (params && params[0] && !strcmp(params[0], "-")) {
			xfree(tmp);
			tmp = NULL;
		}

		connecting = 0;

		if (sess->state == GG_STATE_CONNECTED) {
			if (tmp) {
				char *r1, *r2;

				r1 = xstrmid(tmp, 0, GG_STATUS_DESCR_MAXSIZE);
				r2 = xstrmid(tmp, GG_STATUS_DESCR_MAXSIZE, -1);
				print("disconnected_descr", r1, r2);
				xfree(r1);
				xfree(r2);
			} else
				print("disconnected");
			
		} else if (sess->state != GG_STATE_IDLE)
			print("conn_stopped");

		ekg_logoff(sess, tmp);
		xfree(tmp);
		list_remove(&watches, sess, 0);
		gg_free_session(sess);
		userlist_clear_status();
		sess = NULL;
		reconnect_timer = 0;
		ui_event("disconnected");
	}
}

COMMAND(cmd_del)
{
	struct userlist *u;
	uin_t uin;
	const char *tmp;
	char *nick;

	if (!params[0]) {
		print("not_enough_params", name);
		return;
	}

	if (!(uin = get_uin(params[0])) || !(u = userlist_find(uin, NULL))) {
		print("user_not_found", params[0]);
		return;
	}

	nick = xstrdup(u->display);
	
	tmp = format_user(uin);

	if (!userlist_remove(u)) {
		print("user_deleted", tmp);
		gg_remove_notify(sess, uin);
		config_changed = 1;
		ui_event("userlist_changed", nick, itoa(uin));
	} else
		print("error_deleting");

	xfree(nick);
	
	return;
}

COMMAND(cmd_exec)
{
	list_t l;
	int pid;

	if (params[0]) {
		char *tmp;
		int fd[2] = { 0, 0 };
		struct gg_exec s;

		if (pipe(fd)) {
			print("exec_error", strerror(errno));
			return;
		}
		
		if (!(pid = fork())) {
			if (fd[1]) {
				close(fd[0]);
				dup2(fd[1], 2);
				dup2(fd[1], 1);
				close(fd[1]);
			}	
			execl("/bin/sh", "sh", "-c", (params[0][0] == '^') ? params[0] + 1 : params[0], (void *) NULL);
			exit(1);
		}

		if (pid < 0) {
			print("exec_error", strerror(errno));
			return;
		}
	
		s.fd = fd[0];
		s.check = GG_CHECK_READ;
		s.state = GG_STATE_READING_DATA;
		s.type = GG_SESSION_USER3;
		s.id = pid;
		s.timeout = 60;
		s.buf = string_init(NULL);
		s.target = xstrdup(target);

		fcntl(s.fd, F_SETFL, O_NONBLOCK);

		list_add(&watches, &s, sizeof(s));
		close(fd[1]);
		
		if (params[0][0] == '^')
			tmp = saprintf("\002%s", params[0] + 1);
		else
			tmp = xstrdup(params[0]);

		process_add(pid, tmp);
		xfree(tmp);
	} else {
		for (l = children; l; l = l->next) {
			struct process *p = l->data;

			print("process", itoa(p->pid), p->name);
		}

		if (!children)
			print("no_processes");
	}
}

COMMAND(cmd_find)
{
	struct gg_search_request *r;
	struct gg_http *h;
	list_t l;
	char **argv = NULL;
	int i, id = 1;

	/* wybieramy sobie identyfikator sercza */
	for (l = watches; l; l = l->next) {
		struct gg_http *h = l->data;

		if (h->type != GG_SESSION_SEARCH)
			continue;

		if (h->id / 2 >= id)
			id = h->id / 2 + 1;
	}
	
	r = xcalloc(1, sizeof(*r));

	if (!params[0] || !(argv = array_make(params[0], " \t", 0, 1, 1)) || !argv[0]) {
		xfree(r);
		ui_event("command", "find", NULL);
		return;

	} else {
		if (argv[0] && !argv[1] && argv[0][0] == '#') { /* konferencja */
			char *tmp = saprintf("conference --find %s", argv[0]);
			command_exec(NULL, tmp);
			xfree(tmp);
			xfree(r);
			array_free(argv);
			return;
		} else if (argv[0] && !argv[1] && argv[0][0] != '-') {
			id = id * 2;	/* single search */
			if (!(r->uin = get_uin(params[0]))) {
				print("user_not_found", params[0]);
				xfree(r);
				array_free(argv);
				return;
			}
		} else {
			id = id * 2 + 1;	/* multiple search */
			for (i = 0; argv[i]; i++) {
				char *arg = argv[i];
				
				if (match_arg(arg, 'f', "first", 2) && argv[i + 1])
					r->first_name = xstrdup(argv[++i]);
				if (match_arg(arg, 'l', "last", 2) && argv[i + 1])
					r->last_name = xstrdup(argv[++i]);
				if (match_arg(arg, 'n', "nickname", 2) && argv[i + 1])
					r->nickname = xstrdup(argv[++i]);
				if (match_arg(arg, 'c', "city", 2) && argv[i + 1])
					r->city = xstrdup(argv[++i]);
				if (match_arg(arg, 'e', "email", 2) && argv[i + 1])
					r->email = xstrdup(argv[++i]);
				if (match_arg(arg, 'u', "uin", 2) && argv[i + 1])
					if (!(r->uin = str_to_uin(argv[++i]))) {
						print("invalid_uin");
						gg_search_request_free(r);
						array_free(argv);
						return;
					}
				if (match_arg(arg, 's', "start", 2) && argv[i + 1])
					r->start = strtol(argv[++i], NULL, 0);
				if (match_arg(arg, 'F', "female", 2))
					r->gender = GG_GENDER_FEMALE;
				if (match_arg(arg, 'M', "male", 2))
					r->gender = GG_GENDER_MALE;
				if (match_arg(arg, 'a', "active", 2))
					r->active = 1;
				if (match_arg(arg, 'b', "born", 2) && argv[i + 1]) {
					char *foo = strchr(argv[++i], ':');
		
					if (!foo) {
						r->min_birth = atoi(argv[i]);
						r->max_birth = atoi(argv[i]);
					} else {
						*foo = 0;
						r->min_birth = atoi(argv[i]);
						r->max_birth = atoi(++foo);
					}
					if (r->min_birth < 100)
						r->min_birth += 1900;
					if (r->max_birth < 100)
						r->max_birth += 1900;
				}
				if (match_arg(arg, 'A', "all", 3))
					r->start |= 0x80000000L;
			}
		}
	}

	iso_to_cp(r->first_name);
	iso_to_cp(r->last_name);
	iso_to_cp(r->nickname);
	iso_to_cp(r->city);
	iso_to_cp(r->email);

	if (!(h = gg_search(r, 1))) {
		print("search_failed", http_error_string(0));
		array_free(argv);
		gg_search_request_free(r);
		return;
	}

	h->id = id;
	h->user_data = (char*) r;

	list_add(&watches, h, 0);
	
	array_free(argv);

	return;
}

COMMAND(cmd_change)
{
	struct gg_change_info_request *r;
	struct gg_http *h;
	char **argv = NULL;
	int i;

	if (!params[0]) {
		print("not_enough_params", name);
		return;
	}

	argv = array_make(params[0], " \t", 0, 1, 1);

	r = xcalloc(1, sizeof(*r));
	
	for (i = 0; argv[i]; i++) {
		
		if (match_arg(argv[i], 'f', "first", 2) && argv[i + 1]) {
			xfree(r->first_name);
			r->first_name = xstrdup(argv[++i]);
		}
		
		if (match_arg(argv[i], 'l', "last", 2) && argv[i + 1]) {
			xfree(r->last_name);
			r->last_name = xstrdup(argv[++i]);
		}
		
		if (match_arg(argv[i], 'n', "nickname", 2) && argv[i + 1]) {
			xfree(r->nickname);
			r->nickname = xstrdup(argv[++i]);
		}
		
		if (match_arg(argv[i], 'e', "email", 2) && argv[i + 1]) {
			xfree(r->email);
			r->email = xstrdup(argv[++i]);
		}

		if (match_arg(argv[i], 'c', "city", 2) && argv[i + 1]) {
			xfree(r->city);
			r->city = xstrdup(argv[++i]);
		}
		
		if (match_arg(argv[i], 'b', "born", 2) && argv[i + 1])
			r->born = atoi(argv[++i]);
		
		if (match_arg(argv[i], 'F', "female", 2))
			r->gender = GG_GENDER_FEMALE;

		if (match_arg(argv[i], 'M', "male", 2))
			r->gender = GG_GENDER_MALE;
	}

	if (!r->first_name || !r->last_name || !r->nickname || !r->email || !r->city || !r->born) {
		print("change_not_enough_params");
		gg_change_info_request_free(r);
		array_free(argv);
		return;
	}

	if ((h = gg_change_info(config_uin, config_password, r, 1)))
		list_add(&watches, h, 0);

	gg_change_info_request_free(r);
	array_free(argv);
}

COMMAND(cmd_modify)
{
	struct userlist *u;
	char **argv = NULL;
	uin_t uin;
	int i;
	int modified = 0;

	if (!params[0]) {
		print("not_enough_params", name);
		return;
	}

	if (!(uin = get_uin(params[0])) || !(u = userlist_find(uin, NULL))) {
		print("user_not_found", params[0]);
		return;
	}

	argv = array_make(params[1], " \t", 0, 1, 1);

	for (i = 0; argv[i]; i++) {
		
		if (match_arg(argv[i], 'f', "first", 2) && argv[i + 1]) {
			xfree(u->first_name);
			u->first_name = xstrdup(argv[++i]);
			modified = 1;
		}
		
		if (match_arg(argv[i], 'l', "last", 2) && argv[i + 1]) {
			xfree(u->last_name);
			u->last_name = xstrdup(argv[++i]);
			modified = 1;
		}
		
		if (match_arg(argv[i], 'n', "nickname", 2) && argv[i + 1]) {
			xfree(u->nickname);
			u->nickname = xstrdup(argv[++i]);
			modified = 1;
		}
		
		if ((match_arg(argv[i], 'p', "phone", 2) || match_arg(argv[i], 'm', "mobile", 2)) && argv[i + 1]) {
			xfree(u->mobile);
			u->mobile = xstrdup(argv[++i]);
			modified = 1;
		}
		
		if (match_arg(argv[i], 'd', "display", 2) && argv[i + 1]) {
			ui_event("userlist_changed", u->display, argv[++i]);
			xfree(u->display);
			u->display = xstrdup(argv[i]);
			userlist_replace(u);
			modified = 1;
		}
		
		if (match_arg(argv[i], 'g', "group", 2) && argv[i + 1]) {
			char **tmp = array_make(argv[++i], ",", 0, 1, 1);
			int x, off;	/* je¶li zaczyna siê od '@', pomijamy pierwszy znak */
			
			for (x = 0; tmp[x]; x++)
				switch (*tmp[x]) {
					case '-':
						off = (tmp[x][1] == '@' && strlen(tmp[x]) > 1) ? 1 : 0;

						if (group_member(u, tmp[x] + 1 + off)) {
							group_remove(u, tmp[x] + 1 + off);
							modified = 1;
						} else {
							print("group_member_not_yet", format_user(uin), tmp[x] + 1);
							if (!modified)
								modified = -1;
						}
						break;
					case '+':
						off = (tmp[x][1] == '@' && strlen(tmp[x]) > 1) ? 1 : 0;

						if (!group_member(u, tmp[x] + 1 + off)) {
							group_add(u, tmp[x] + 1 + off);
							modified = 1;
						} else {
							print("group_member_already", format_user(uin), tmp[x] + 1);
							if (!modified)
								modified = -1;
						}
						break;
					default:
						off = (tmp[x][0] == '@' && strlen(tmp[x]) > 1) ? 1 : 0;

						if (!group_member(u, tmp[x] + off)) {
							group_add(u, tmp[x] + off);
							modified = 1;
						} else {
							print("group_member_already", format_user(uin), tmp[x]);
							if (!modified)
								modified = -1;
						}
				}

			array_free(tmp);
		}
		
		if (match_arg(argv[i], 'u', "uin", 2) && argv[i + 1]) {
			uin_t new_uin = str_to_uin(argv[++i]);

			if (!new_uin) {
				print("invalid_uin");
				array_free(argv);
				return;
			}	

			gg_remove_notify(sess, u->uin);
			u->uin = new_uin;
			gg_add_notify(sess, u->uin);

			u->status = GG_STATUS_NOT_AVAIL;

			ui_event("userlist_changed", u->display, u->display);
			modified = 1;
		}

		if (match_arg(argv[i], 'o', "offline", 2)) {
			gg_remove_notify_ex(sess, u->uin, userlist_type(u));
			group_add(u, "__offline");
			print("modify_offline", format_user(u->uin));
			modified = 2;
			gg_add_notify_ex(sess, u->uin, userlist_type(u));
		}

		if (match_arg(argv[i], 'O', "online", 2)) {
			gg_remove_notify_ex(sess, u->uin, userlist_type(u));
			group_remove(u, "__online");
			print("modify_online", format_user(u->uin));
			modified = 2;
			gg_add_notify_ex(sess, u->uin, userlist_type(u));
		}

	}

	if (strcasecmp(name, "add")) {
		switch (modified) {
			case 0:
				print("not_enough_params", name);
				break;
			case 1:
				print("modify_done", params[0]);
			case 2:
				config_changed = 1;
				break;
		}
	} else
		config_changed = 1;

	array_free(argv);
}

COMMAND(cmd_help)
{
	list_t l;
	
	if (params[0]) {
		const char *p = (params[0][0] == '/' && strlen(params[0]) > 1) ? params[0] + 1 : params[0];

		for (l = commands; l; l = l->next) {
			struct command *c = l->data;
			
			if (!strcasecmp(c->name, p) && c->alias) {
				print("help_alias", p);
				return;
			}

			if (!strcasecmp(c->name, p) && !c->alias) {
			    	char *blah = NULL;

				if (strstr(c->brief_help, "%"))
				    	blah = format_string(c->brief_help);
				
				print("help", c->name, c->params_help, blah ? blah : c->brief_help, "");
				xfree(blah);

				if (c->long_help && strcmp(c->long_help, "")) {
					char *foo, *tmp, *plumk, *bar = xstrdup(c->long_help);

					if ((foo = bar)) {
						while ((tmp = gg_get_line(&foo))) {
							plumk = format_string(tmp);
							if (plumk) {
								print("help_more", plumk);
								xfree(plumk);
							}
						}
						xfree(bar);
					}
				}

				return;
			}
		}
	}

	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (isalnum(*c->name) && !c->alias) {
		    	char *blah = NULL;

			if (strstr(c->brief_help, "%"))
			    	blah = format_string(c->brief_help);
	
			print("help", c->name, c->params_help, blah ? blah : c->brief_help, (c->long_help && strcmp(c->long_help, "")) ? "\033[1m *\033[0m" : "");
			xfree(blah);
		}
	}

	print("help_footer");
	print("help_quick");
}

COMMAND(cmd_ignore)
{
	uin_t uin;
	char *tmp;

	if (*name == 'i' || *name == 'I') {
		if (!params[0]) {
			list_t l;
			int i = 0;

			for (l = userlist; l; l = l->next) {
				struct userlist *u = l->data;
				int level = ignored_check(u->uin);

				if (!level)
					continue;

				i = 1;

				print("ignored_list", format_user(u->uin));
			}

			if (!i) 
				print("ignored_list_empty");

			return;
		}

		if (params[0][0] == '#') {
			tmp = saprintf("conference --ignore %s", params[0]);
			command_exec(NULL, tmp);
			xfree(tmp);
			return;
		}
		
		if (!(uin = get_uin(params[0]))) {
			print("user_not_found", params[0]);
			return;
		}
		
		if (!ignored_add(uin, IGNORE_ALL)) {
			print("ignored_added", params[0]);
			config_changed = 1;
		} else
			print("error_adding_ignored");

	} else {
		if (!params[0]) {
			print("not_enough_params", name);
			return;
		}

		if (params[0][0] == '#') {
			tmp = saprintf("conference --unignore %s", params[0]);
			command_exec(NULL, tmp);
			xfree(tmp);
			return;
		}
		
		if (!(uin = get_uin(params[0]))) {
			print("user_not_found", params[0]);
			return;
		}
		
		if (!ignored_remove(uin)) {
			print("ignored_deleted", format_user(uin));
			config_changed = 1;
		} else
			print("error_not_ignored", format_user(uin));
	
	}
}

COMMAND(cmd_block)
{
	uin_t uin;

	if (*name == 'b' || *name == 'B') {
		if (!params[0]) {
			list_t l;
			int i = 0;

			for (l = userlist; l; l = l->next) {
				struct userlist *u = l->data;
				
				if (!group_member(u, "__blocked"))
					continue;

				i = 1;

				print("blocked_list", format_user(u->uin));
			}

			if (!i) 
				print("blocked_list_empty");

			return;
		}

		if (!(uin = get_uin(params[0]))) {
			print("user_not_found", params[0]);
			return;
		}
		
		blocked_add(uin);
		print("blocked_added", params[0]);
		config_changed = 1;
	} else {
		if (!params[0]) {
			print("not_enough_params", name);
			return;
		}

		if (!(uin = get_uin(params[0]))) {
			print("user_not_found", params[0]);
			return;
		}
		
		if (!blocked_remove(uin)) {
			print("blocked_deleted", format_user(uin));
			config_changed = 1;
		} else
			print("error_not_blocked", format_user(uin));
	
	}
}

COMMAND(cmd_list)
{
	list_t l;
	int count = 0, show_all = 1, show_busy = 0, show_active = 0, show_inactive = 0, show_invisible = 0, show_descr = 0, show_blocked = 0, show_offline = 0, j, p;
	char *tmp, **argv = NULL, *show_group = NULL;

	if (params[0] && *params[0] != '-') {
		char *status, *groups;
		struct userlist *u;
		uin_t uin;

		/* list @grupa */
		if (params[0][0] == '@' && strlen(params[0]) > 1) {
			string_t members = string_init(NULL);
			int count = 0;

			for (l = userlist; l; l = l->next) {
				u = l->data;

				if (u->groups) {
					if (group_member(u, params[0]+1)) {
						if (count++)
							string_append(members, ", ");
						string_append(members, u->display);
					}
				}
			}
			
			if (count)
				print("group_members", params[0], members->str);
			else
				print("group_empty", params[0]);

			string_free(members, 1);

			return;
		}

		if (!(uin = get_uin(params[0])) || !(u = userlist_find(uin, NULL))) {
			print("user_not_found", params[0]);
			return;
		}

		/* list <alias> [opcje] */
		if (params[1]) {
			cmd_modify("list", params, NULL);
			return;
		}

		switch (u->status) {
			case GG_STATUS_AVAIL:
				status = format_string(format_find("user_info_avail"), (u->first_name) ? u->first_name : u->display);
				break;
			case GG_STATUS_AVAIL_DESCR:
				status = format_string(format_find("user_info_avail_descr"), (u->first_name) ? u->first_name : u->display, u->descr);
				break;
			case GG_STATUS_BUSY:
				status = format_string(format_find("user_info_busy"), (u->first_name) ? u->first_name : u->display);
				break;
			case GG_STATUS_BUSY_DESCR:
				status = format_string(format_find("user_info_busy_descr"), (u->first_name) ? u->first_name : u->display, u->descr);
				break;
			case GG_STATUS_NOT_AVAIL:
				status = format_string(format_find("user_info_not_avail"), (u->first_name) ? u->first_name : u->display);
				break;
			case GG_STATUS_NOT_AVAIL_DESCR:
				status = format_string(format_find("user_info_not_avail_descr"), (u->first_name) ? u->first_name : u->display, u->descr);
				break;
			case GG_STATUS_INVISIBLE:
				status = format_string(format_find("user_info_invisible"), (u->first_name) ? u->first_name : u->display);
				break;
			case GG_STATUS_INVISIBLE_DESCR:
				status = format_string(format_find("user_info_invisible_descr"), (u->first_name) ? u->first_name : u->display, u->descr);
				break;
			case GG_STATUS_BLOCKED:
				status = format_string(format_find("user_info_blocked"), (u->first_name) ? u->first_name : u->display);
				break;
			default:
				status = format_string(format_find("user_info_unknown"), (u->first_name) ? u->first_name : u->display);
		}
		
		groups = group_to_string(u->groups, 0);

		print("user_info", u->first_name, u->last_name, (u->nickname) ? u->nickname : u->display, u->display, u->mobile, groups, itoa(u->uin), status);
		
		xfree(groups);
		xfree(status);

		return;
	}

	/* list --get */
	if (params[0] && (match_arg(params[0], 'g', "get", 2) || match_arg(params[0], 'G', "get-config", 5))) {
		struct gg_http *h;
		
		if (!(h = gg_userlist_get(config_uin, config_password, 1))) {
			print("userlist_get_error", strerror(errno));
			return;
		}

		if (match_arg(params[0], 'G', "get-config", 5))
			h->user_data = (char*) 1;
		
		list_add(&watches, h, 0);
		
		return;
	}

	/* list --put */
	if (params[0] && (match_arg(params[0], 'p', "put", 2) || match_arg(params[0], 'P', "put-config", 5))) {
		struct gg_http *h;
		char *contacts = userlist_dump();

		if (match_arg(params[0], 'P', "put-config", 5)) {
			string_t s = string_init(contacts);
			char *vars = variable_digest();
			int i, count;

			count = strlen(vars) / 120;

			if (strlen(vars) % 120 != 0)
				count++;

			for (i = 0; i < count; i++) {
				string_append(s, "__config");
				string_append(s, itoa(i));
				string_append_c(s, ';');

				for (j = 0; j < 5; j++) {
					char *tmp;

					if (i * 120 + j * 24 < strlen(vars)) 
						tmp = xstrmid(vars, i * 120 + j * 24, 24);
					else
						tmp = xstrdup("");

					string_append(s, tmp);
					string_append_c(s, ';');
					xfree(tmp);
				}

				string_append(s, itoa(rand() % 3000000 + 10000));
				string_append(s, "\r\n");
			}

			xfree(contacts);
			
			contacts = string_free(s, 0);
		}
		
		if (!contacts) {
			print("userlist_put_error", strerror(ENOMEM));
			return;
		}
		
		if (!(h = gg_userlist_put(config_uin, config_password, contacts, 1))) {
			print("userlist_put_error", strerror(errno));
			return;
		}
		
		xfree(contacts);
		
		list_add(&watches, h, 0);

		return;
	}

	/* list --active | --busy | --inactive | --invisible | --description | --member | --blocked | --offline */
	for (j = 0; params[j]; j++) {
		int i;

		argv = array_make(params[j], " \t", 0, 1, 1);

	 	for (i = 0; argv[i]; i++) {
			if (match_arg(argv[i], 'a', "active", 2)) {
				show_all = 0;
				show_active = 1;
			}
				
			if (match_arg(argv[i], 'i', "inactive", 2) || match_arg(argv[i], 'n', "notavail", 2)) {
				show_all = 0;
				show_inactive = 1;
			}
			
			if (match_arg(argv[i], 'b', "busy", 2)) {
				show_all = 0;
				show_busy = 1;
			}
			
			if (match_arg(argv[i], 'I', "invisible", 2)) {
				show_all = 0;
				show_invisible = 1;
			}

			if (match_arg(argv[i], 'B', "blocked", 2)) {
				show_all = 0;
				show_blocked = 1;
			}

			if (match_arg(argv[i], 'o', "offline", 2)) {
				show_all = 0;
				show_offline = 1;
			}

			if (match_arg(argv[i], 'r', "ignored", 2)) {
				show_all = 0;
				show_offline = 1;
			}

			if (match_arg(argv[i], 'm', "member", 2)) {
				if (j && argv[i+1]) {
					int off = (argv[i+1][0] == '@' && strlen(argv[i+1]) > 1) ? 1 : 0;

					show_group = xstrdup(argv[i+1] + off);
				} else
					if (params[i+1]) {
						char **tmp = array_make(params[i+1], " \t", 0, 1, 1);
						int off = (params[i+1][0] == '@' && strlen(params[i+1]) > 1) ? 1 : 0;

 						show_group = xstrdup(tmp[0] + off);
						array_free(tmp);
					}
			}

			if (match_arg(argv[i], 'd', "description", 2))
				show_descr = 1;
		}
		array_free(argv);
	}

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		struct in_addr in;
		int show;

		if (!u->display)
			continue;

		tmp = "list_unknown";
		switch (u->status) {
			case GG_STATUS_AVAIL:
				tmp = "list_avail";
				break;
			case GG_STATUS_AVAIL_DESCR:
				tmp = "list_avail_descr";
				break;
			case GG_STATUS_NOT_AVAIL:
				tmp = "list_not_avail";
				break;
			case GG_STATUS_BUSY:
				tmp = "list_busy";
				break;
			case GG_STATUS_BUSY_DESCR:
				tmp = "list_busy_descr";
				break;
			case GG_STATUS_NOT_AVAIL_DESCR:
				tmp = "list_not_avail_descr";
				break;
			case GG_STATUS_INVISIBLE:
				tmp = "list_invisible";
				break;
			case GG_STATUS_BLOCKED:
				tmp = "list_blocked";
				break;
		}

		in.s_addr = u->ip.s_addr;
		p = u->port;

		if (u->uin == config_uin) {
			if (config_dcc && config_dcc_ip) {

				if (strcmp(config_dcc_ip, "auto"))
					in.s_addr = inet_addr(config_dcc_ip);
				else {
					struct sockaddr_in foo;
					int bar = sizeof(foo);

					if (!sess || getsockname(sess->fd, (struct sockaddr *) &foo, &bar))
						goto fail;

					in.s_addr = foo.sin_addr.s_addr; 
				}

				p = 1550;

			} else {
fail:
				in.s_addr = inet_addr("0.0.0.0");
				p = 0;
			}
				
			if (sess && sess->state == GG_STATE_CONNECTED)
				switch (config_status) {
					case GG_STATUS_INVISIBLE:
						tmp = "list_invisible";
						break;
					case GG_STATUS_INVISIBLE_DESCR:
						tmp = "list_invisible_descr";
						break;
				}
		}

		show = show_all;

		if (show_busy && GG_S_B(u->status))
			show = 1;

		if (show_active && GG_S_A(u->status))
			show = 1;

		if (show_inactive && GG_S_NA(u->status))
			show = 1;

		if (show_invisible && GG_S_I(u->status))
			show = 1;

		if (show_descr && !GG_S_D(u->status))
			show = 0;

		if (show_group && !group_member(u, show_group))
			show = 0;

		if (show) {
			print(tmp, format_user(u->uin), (u->first_name) ? u->first_name : u->display, inet_ntoa(in), itoa(p), u->descr);
			count++;
		}
	}

	if (!count && !(show_descr || show_group) && show_all)
		print("list_empty");
}

COMMAND(cmd_msg)
{
	struct userlist *u;
	char **nicks = NULL, *nick = NULL, **p = NULL;
	unsigned char *msg = NULL, *raw_msg = NULL, *escaped = NULL, *format = NULL;
	uin_t uin;
	int count, valid = 0, chat = (!strcasecmp(name, "chat")), secure = 0, msg_seq, formatlen = 0;

	if (!params[0] || !params[1]) {
		print("not_enough_params", name);
		return;
	}

	if (config_auto_back == 1 && (away == 1 || away == 3))
		change_status(GG_STATUS_AVAIL, NULL, 1);

	nick = xstrdup(params[0]);

	if ((*nick == '@' || strchr(nick, ',')) && chat) {
		struct conference *c = conference_create(nick);
		list_t l;

		if (c) {
			xfree(nick);
			nick = xstrdup(c->name);

			for (l = c->recipients; l; l = l->next)
				array_add(&nicks, xstrdup(itoa(*((uin_t *) (l->data)))));
		}

	} else if (*nick == '#') {
		struct conference *c = conference_find(nick);
		list_t l;

		if (!c) {
			print("conferences_noexist", nick);
			xfree(nick);
			return;
		}

		for (l = c->recipients; l; l = l->next)
			array_add(&nicks, xstrdup(itoa(*((uin_t *) (l->data)))));
	} else {
		char **tmp = array_make(nick, ",", 0, 0, 0);
		int i;

		/* XXX nie s± wykrywane duplikaty */
		
		for (i = 0; tmp[i]; i++) {
			int count = 0;
			list_t l;

			if (tmp[i][0] != '@') {
				array_add(&nicks, xstrdup(tmp[i]));
				continue;
			}

			for (l = userlist; l; l = l->next) {
				struct userlist *u = l->data;			
				list_t m;

				for (m = u->groups; m; m = m->next) {
					struct group *g = m->data;

					if (!strcasecmp(g->name, tmp[i] + 1)) {
						array_add(&nicks, xstrdup(u->display));
						count++;
					}
				}
			}

			if (!count)
				print("group_empty", tmp[i] + 1);
		}

		array_free(tmp);
	}

	if (!nicks) {
		xfree(nick);
		return;
	}

	msg = xstrdup(params[1]);

	/* analizê tekstu zrobimy w osobnym bloku dla porz±dku */
	{
		unsigned char attr = 0, last_attr = 0;
		const unsigned char *p = params[1];
		int msglen = 0, i;

		for (i = 0; i < strlen(params[1]); i++, p++) {
			if (*p == 2)
				attr ^= GG_FONT_BOLD;
			if (*p == 20)
				attr ^= GG_FONT_ITALIC;
			if (*p == 31)
				attr ^= GG_FONT_UNDERLINE;

			if (*p >= 32 || *p == 13 || *p == 10 || *p == 9) {
				if (attr != last_attr) {
					if (!format) {
						format = xmalloc(3);
						format[0] = 2;
						formatlen = 3;
					}

					format = xrealloc(format, formatlen + 3);
					format[formatlen] = (msglen & 255);
					format[formatlen + 1] = ((msglen >> 8) & 255);
					format[formatlen + 2] = attr;
					formatlen += 3;

					last_attr = attr;
				}

				msg[msglen++] = *p;
			}
		}

		msg[msglen] = 0;

		if (format && formatlen) {
			format[1] = (formatlen - 3) & 255;
			format[2] = ((formatlen - 3) << 8) & 255;
		}
	}

	raw_msg = xstrdup(msg);
	escaped = log_escape(msg);
	iso_to_cp(msg);

	count = array_count(nicks);

#ifdef HAVE_OPENSSL
	if (config_encryption == 1 && array_count(nicks) == 1 && (uin = get_uin(nicks[0])) && (msg_encrypt(uin, &msg) > 0))
			secure = 1;
#endif

	for (p = nicks; *p; p++) {
		if (!strcmp(*p, ""))
			continue;

		if (!(uin = get_uin(*p))) {
			print("user_not_found", *p);
			continue;
		}
		
	        u = userlist_find(uin, NULL);

		put_log(uin, "%s,%ld,%s,%s,%s\n", (chat) ? "chatsend" : "msgsend", uin, (u) ? u->display : "", log_timestamp(time(NULL)), escaped);

		if (config_last & 4)
			last_add(1, uin, time(NULL), 0, raw_msg);

		if (!chat || count == 1) {
			if (sess)
				msg_seq = gg_send_message_richtext(sess, (chat) ? GG_CLASS_CHAT : GG_CLASS_MSG, uin, msg, format, formatlen);
			else
				msg_seq = -1;

			msg_queue_add((chat) ? GG_CLASS_CHAT : GG_CLASS_MSG, msg_seq, 1, &uin, raw_msg, secure, format, formatlen);
			valid++;
		}
	}


	if (count > 1 && chat) {
		uin_t *uins = xmalloc(count * sizeof(uin_t));
		int realcount = 0;

		for (p = nicks; *p; p++)
			if ((uin = get_uin(*p)))
				uins[realcount++] = uin;
		
		if (sess)
			msg_seq = gg_send_message_confer_richtext(sess, GG_CLASS_CHAT, realcount, uins, msg, format, formatlen);
		else
			msg_seq = -1;

		msg_queue_add((chat) ? GG_CLASS_CHAT : GG_CLASS_MSG, msg_seq, count, uins, raw_msg, 0, format, formatlen);
		valid++;

		xfree(uins);
	}

	add_send_nick(nick);

	if (valid && (!sess || sess->state != GG_STATE_CONNECTED))
		print("not_connected_msg_queued");

	if (valid && config_display_sent) {
		struct gg_event e;
		struct userlist u;
		
		memset(&e, 0, sizeof(e));
		e.type = GG_EVENT_MSG;
		e.event.msg.sender = config_uin;
		e.event.msg.message = xstrdup(params[1]);
		e.event.msg.time = time(NULL);

		memset(&u, 0, sizeof(u));
		u.uin = 0;
		u.display = xstrdup(nick);
		
		print_message(&e, &u, (chat) ? 3 : 4, secure);

		xfree(e.event.msg.message);
		xfree(u.display);
	}

	xfree(msg);
	xfree(raw_msg);
	xfree(escaped);
	xfree(format);
	xfree(nick);

	array_free(nicks);

	unidle();
}

COMMAND(cmd_save)
{
	last_save = time(NULL);

	if (!userlist_write(NULL) && !config_write(NULL)) {
		print("saved");
		config_changed = 0;
	} else
		print("error_saving");
}

COMMAND(cmd_set)
{
	const char *arg = NULL, *val = NULL;
	int unset = 0, show_all = 0;
	char *value = NULL;
	list_t l;

	if (match_arg(params[0], 'a', "all", 1)) {
		show_all = 1;
		arg = params[1];
		if (arg)
			val = params[2];
	} else {
		arg = params[0];
		if (arg)
			val = params[1];
	}
	
	if (arg && arg[0] == '-') {
		unset = 1;
		arg++;
	}

	if (arg && val) {
		char **tmp = array_make(val, "", 0, 0, 1);

		value = tmp[0];
		tmp[0] = NULL;
		array_free(tmp);
	}

	if ((!arg || !val) && !unset) {
		int displayed = 0;

		for (l = variables; l; l = l->next) {
			struct variable *v = l->data;
			
			if ((!arg || !strcasecmp(arg, v->name)) && (v->display != 2 || strcmp(name, "set"))) {
				char *string = *(char**)(v->ptr);
				int value = *(int*)(v->ptr);

				if (!show_all && !arg && v->dyndisplay && !(*v->dyndisplay)(v->name))
					continue;

				if (!v->display) {
					print("variable", v->name, "(...)");
					displayed = 1;
					continue;
				}

				if (v->type == VAR_STR) {
					char *tmp = (string) ? saprintf("\"%s\"", string) : "(none)";

					print("variable", v->name, tmp);
					
					if (string)
						xfree(tmp);
				}

				if (v->type == VAR_BOOL)
					print("variable", v->name, (value) ? "1 (on)" : "0 (off)");
				
				if ((v->type == VAR_INT || v->type == VAR_MAP) && !v->map)
					print("variable", v->name, itoa(value));

				if (v->type == VAR_INT && v->map) {
					char *tmp = NULL;
					int i;

					for (i = 0; v->map[i].label; i++)
						if (v->map[i].value == value) {
							tmp = saprintf("%d (%s)", value, v->map[i].label);
							break;
						}

					if (!tmp)
						tmp = saprintf("%d", value);

					print("variable", v->name, tmp);

					xfree(tmp);
				}

				if (v->type == VAR_MAP && v->map) {
					string_t s = string_init(itoa(value));
					int i, first = 1;

					for (i = 0; v->map[i].label; i++) {
						if ((value & v->map[i].value) || (!value && !v->map[i].value)) {
							string_append(s, (first) ? " (" : ",");
							first = 0;
							string_append(s, v->map[i].label);
						}
					}

					if (!first)
						string_append_c(s, ')');

					print("variable", v->name, s->str);

					string_free(s, 1);
				}

				displayed = 1;
			}
		}

		if (!displayed && params[0])
			print("variable_not_found", params[0]);
	} else {
		theme_cache_reset();
		switch (variable_set(arg, (unset) ? NULL : value, 0)) {
			case 0:
			{
				const char *my_params[2] = { (!unset) ? params[0] : params[0] + 1, NULL };

				cmd_set("set-show", my_params, NULL);
				config_changed = 1;
				last_save = time(NULL);
				break;
			}
			case -1:
				print("variable_not_found", arg);
				break;
			case -2:
				print("variable_invalid", arg);
				break;
		}
	}

	xfree(value);
}

COMMAND(cmd_sms)
{
	struct userlist *u;
	const char *number = NULL;

	if (!params[1]) {
		print("not_enough_params", name);
		return;
	}

	if ((u = userlist_find(0, params[0]))) {
		if (!u->mobile || !strcmp(u->mobile, "")) {
			print("sms_unknown", format_user(u->uin));
			return;
		}
		number = u->mobile;
	} else
		number = params[0];

	if (send_sms(number, params[1], 1) == -1)
		print("sms_error", strerror(errno));
}

COMMAND(cmd_quit)
{
    	char *tmp = NULL;

	if (!params[0]) {
	    	if (config_random_reason & 2) {
			tmp = random_line(prepare_path("quit.reasons", 0));
			if (!tmp && config_quit_reason)
			    	tmp = xstrdup(config_quit_reason);
		}
		else if (config_quit_reason)
		    	tmp = xstrdup(config_quit_reason);
	} else
	    	tmp = xstrdup(params[0]);

	if (config_keep_reason && !tmp && config_reason)
		tmp = xstrdup(config_reason);

	if (params[0] && !strcmp(params[0], "-")) {
		xfree(tmp);
		tmp = NULL;
	}
	
	if (!quit_message_send) {
		if (tmp) {
			char *r1, *r2;

			r1 = xstrmid(tmp, 0, GG_STATUS_DESCR_MAXSIZE);
			r2 = xstrmid(tmp, GG_STATUS_DESCR_MAXSIZE, -1);

			print("quit_descr", r1, r2);

			xfree(r1);
			xfree(r2);
		} else
			print("quit");

		quit_message_send = 1;
	}

	ekg_logoff(sess, tmp);

	if (tmp) {
		xfree(config_reason);
		config_reason = tmp;
		tmp = NULL;
	}
		
	ui_event("disconnected");

	/* nie wychodzimy tutaj, ¿eby command_exec() mia³o szansê zwolniæ
	 * u¿ywan± przez siebie pamiêæ. */
	quit_command = 1;

	return;
}

COMMAND(cmd_dcc)
{
	struct transfer t;
	list_t l;
	uin_t uin;

	if (!params[0] || !strncasecmp(params[0], "sh", 2)) {	/* show */
		int pending = 0, active = 0;

		if (params[0] && params[1] && params[1][0] == 'd') {	/* show debug */
			for (l = transfers; l; l = l->next) {
				struct transfer *t = l->data;
				
				print("dcc_show_debug", itoa(t->id), (t->type == GG_SESSION_DCC_SEND) ? "SEND" : "GET", t->filename, format_user(t->uin), (t->dcc) ? "yes" : "no");
			}

			return;
		}

		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (!t->dcc || !t->dcc->established) {
				if (!pending) {
					print("dcc_show_pending_header");
					pending = 1;
				}
				switch (t->type) {
					case GG_SESSION_DCC_SEND:
						print("dcc_show_pending_send", itoa(t->id), format_user(t->uin), t->filename);
						break;
					case GG_SESSION_DCC_GET:
						print("dcc_show_pending_get", itoa(t->id), format_user(t->uin), t->filename);
						break;
					case GG_SESSION_DCC_VOICE:
						print("dcc_show_pending_voice", itoa(t->id), format_user(t->uin));
				}
			}
		}

		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (t->dcc && t->dcc->established) {
				if (!active) {
					print("dcc_show_active_header");
					active = 1;
				}
				switch (t->type) {
					case GG_SESSION_DCC_SEND:
						print("dcc_show_active_send", itoa(t->id), format_user(t->uin), t->filename);
						break;
					case GG_SESSION_DCC_GET:
						print("dcc_show_active_get", itoa(t->id), format_user(t->uin), t->filename);
						break;
					case GG_SESSION_DCC_VOICE:
						print("dcc_show_active_voice", itoa(t->id), format_user(t->uin));
				}
			}
		}

		if (!active && !pending)
			print("dcc_show_empty");
		
		return;
	}
	
	if (!strncasecmp(params[0], "se", 2)) {		/* send */
		struct userlist *u;
		struct stat st;
		int fd;

		if (!params[1] || !params[2]) {
			print("not_enough_params", name);
			return;
		}
		
		if (!(uin = get_uin(params[1])) || !(u = userlist_find(uin, NULL))) {
			print("user_not_found", params[1]);
			return;
		}

		if (!sess || sess->state != GG_STATE_CONNECTED) {
			print("not_connected");
			return;
		}

		if ((fd = open(params[2], O_RDONLY)) == -1 || stat(params[2], &st)) {
			print("dcc_open_error", params[2], strerror(errno));
			return;
		} else {
			close(fd);
			if (S_ISDIR(st.st_mode)) {
				print("dcc_open_directory", params[2]);
				return;
			}
		}

		t.uin = uin;
		t.id = transfer_id();
		t.type = GG_SESSION_DCC_SEND;
		t.filename = xstrdup(params[2]);
		t.dcc = NULL;

		if (u->port < 10 || (params[3] && !strcmp(params[3], "--reverse"))) {
			/* nie mo¿emy siê z nim po³±czyæ, wiêc on spróbuje */
			gg_dcc_request(sess, uin);
		} else {
			struct gg_dcc *d;
			
			if (!(d = gg_dcc_send_file(u->ip.s_addr, u->port, config_uin, uin))) {
				print("dcc_error", strerror(errno));
				return;
			}

			if (gg_dcc_fill_file_info(d, params[2]) == -1) {
				print("dcc_open_error", params[2], strerror(errno));
				gg_free_dcc(d);
				return;
			}

			list_add(&watches, d, 0);

			t.dcc = d;
		}

		list_add(&transfers, &t, sizeof(t));

		return;
	}

	if (params[0][0] == 'v') {			/* voice */
#ifdef HAVE_VOIP
		struct userlist *u;
		struct transfer *t, tt;

		if (!params[1]) {
			print("not_enough_params", name);
			return;
		}
		
		/* sprawdzamy najpierw przychodz±ce po³±czenia */
		
		for (t = NULL, l = transfers; l; l = l->next) {
			struct transfer *f = l->data;
			struct userlist *u;
			
			f = l->data;

			if (!f->dcc || !f->dcc->incoming || f->type != GG_SESSION_DCC_VOICE)
				continue;
			
			if (params[1][0] == '#' && atoi(params[1] + 1) == f->id) {
				t = f;
				break;
			}

			if (t && (u = userlist_find(t->uin, NULL))) {
				if (!strcasecmp(params[1], itoa(u->uin)) || !strcasecmp(params[1], u->display)) {
					t = f;
					break;
				}
			}
		}

		if (t) {
			list_add(&watches, t->dcc, 0);
			voice_open();
			return;
		}

		/* sprawd¼, czy ju¿ nie wo³ano o rozmowê g³osow± */

#if 0
		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (t->type == GG_SESSION_DCC_VOICE) {
				print("dcc_voice_running");
				return 0;
			}
		}

		for (l = watches; l; l = l->next) {
			struct gg_session *s = l->data;

			if (s->type == GG_SESSION_DCC_VOICE) {
				print("dcc_voice_running");
				return 0;
			}
		}
#endif
		/* je¶li nie by³o, to próbujemy sami zainicjowaæ */

		if (!(uin = get_uin(params[1])) || !(u = userlist_find(uin, NULL))) {
			print("user_not_found", params[1]);
			return;
		}

		if (!sess || sess->state != GG_STATE_CONNECTED) {
			print("not_connected");
			return;
		}

		memset(&tt, 0, sizeof(tt));
		tt.uin = uin;
		tt.id = transfer_id();
		tt.type = GG_SESSION_DCC_VOICE;

		if (u->port < 10 || (params[2] && !strcmp(params[2], "--reverse"))) {
			/* nie mo¿emy siê z nim po³±czyæ, wiêc on spróbuje */
			gg_dcc_request(sess, uin);
		} else {
			struct gg_dcc *d;
			
			if (!(d = gg_dcc_voice_chat(u->ip.s_addr, u->port, config_uin, uin))) {
				print("dcc_error", strerror(errno));
				return;
			}

			list_add(&watches, d, 0);

			tt.dcc = d;
		}

		list_add(&transfers, &tt, sizeof(tt));
		voice_open();
#else
		print("dcc_voice_unsupported");
#endif
		return;
	}

	if (!strncasecmp(params[0], "g", 1)) {		/* get */
		struct transfer *t;
		char *path;
		
		for (t = NULL, l = transfers; l; l = l->next) {
			struct transfer *tt = l->data;
			struct userlist *u;
			
			if (!tt->dcc || tt->type != GG_SESSION_DCC_GET || !tt->filename)
				continue;
			
			if (!params[1]) {
				t = tt;
				break;
			}

			if (params[1][0] == '#' && atoi(params[1] + 1) == tt->id) {
				t = tt;
				break;
			}

			if ((u = userlist_find(tt->uin, NULL))) {
				if (!strcasecmp(params[1], itoa(u->uin)) || !strcasecmp(params[1], u->display)) {
					t = tt;
					break;
				}
			}
		}

		if (!l || !t || !t->dcc) {
			print("dcc_get_not_found", (params[1]) ? params[1] : "");
			return;
		}

		if (config_dcc_dir) 
		    	path = saprintf("%s/%s", config_dcc_dir, t->filename);
		else
		    	path = xstrdup(t->filename);
		
		/* XXX wiêcej sprawdzania */
		if ((t->dcc->file_fd = open(path, O_WRONLY | O_CREAT, 0600)) == -1) {
			print("dcc_get_cant_create", path);
			gg_free_dcc(t->dcc);
			list_remove(&transfers, t, 1);
			xfree(path);
			
			return;
		}
		
		xfree(path);
		
		print("dcc_get_getting", format_user(t->uin), t->filename);
		
		list_add(&watches, t->dcc, 0);

		return;
	}
	
	if (!strncasecmp(params[0], "c", 1)) {		/* close */
		struct transfer *t;
		uin_t uin;

		if (!params[1]) {
			print("not_enough_params", name);
			return;
		}
		
		for (t = NULL, l = transfers; l; l = l->next) {
			t = l->data;

			if (params[1][0] == '#' && atoi(params[1] + 1) == t->id)
				break;
		}

		if (!t) {
			print("dcc_close_notfound");
			return;
		}

		if (t->dcc) {
			list_remove(&watches, t->dcc, 0);
			gg_dcc_free(t->dcc);
		}

#ifdef HAVE_VOIP
		if (t->type == GG_SESSION_DCC_VOICE)
			voice_close();
#endif

		uin = t->uin;

		if (t->filename)
			xfree(t->filename);

		list_remove(&transfers, t, 1);

		print("dcc_close", format_user(uin));
		
		return;
	}

	print("dcc_unknown_command", params[0]);
}

COMMAND(cmd_version) 
{
	char buf[10];

	snprintf(buf, sizeof(buf), "0x%.2x", GG_DEFAULT_PROTOCOL_VERSION);
    	print("ekg_version", VERSION, buf, GG_DEFAULT_CLIENT_VERSION);
}

#ifdef HAVE_OPENSSL
COMMAND(cmd_test_keygen)
{
	RSA *key;
	char *fn;
	
	if (!params[0]) {
		print("not_enough_params", name);
		return;
	}
	
	print("generic", "Chwilka, generujê klucze...");
	
	key = SIM_RSA_GenKey(atoi(params[0]));

	if (!key) {
		print("generic", "Nie uda³o siê wygenerowaæ klucza");
		return;
	}

	mkdir(prepare_path("keys", 1), 0700);

	fn = saprintf("%s/private.pem", prepare_path("keys", 0));
	SIM_RSA_WriteKey(key, fn, PRIVATE);
	chmod(fn, 0400);
	xfree(fn);

	fn = saprintf("%s/%d.pem", prepare_path("keys", 0), config_uin);
	SIM_RSA_WriteKey(key, fn, PUBLIC);
	xfree(fn);

	print("generic", "Wygenerowano i zapisano klucze");
}

COMMAND(cmd_test_keysend)
{
	string_t s = string_init(NULL);
	char *tmp, buf[128];
	uin_t uin;
	FILE *f;
	
	if (!params[0]) {
		print("not_enough_params", name);
		return;
	}

	if (!(uin = get_uin(params[0]))) {
		print("user_not_found", params[0]);
		return;
	}

	if (!sess || sess->state != GG_STATE_CONNECTED) {
		print("not_connected");
		return;
	}

	tmp = saprintf("%s/%d.pem", prepare_path("keys", 0), config_uin);
	f = fopen(tmp, "r");
	xfree(tmp);

	if (!f) {
		print("public_key_not_found", format_user(config_uin));
		return;
	}

	while (fgets(buf, sizeof(buf), f))
		string_append(s, buf);

	fclose(f);

	gg_send_message(sess, GG_CLASS_MSG, uin, s->str);

	string_free(s, 1);
}

COMMAND(cmd_test_keydel)
{
	char *tmp;
	uin_t uin;

	if (!params[0]) {
		print("not_enough_params", name);
		return;
	}

	if (!(uin = get_uin(params[0]))) {
		print("user_not_found", params[0]);
		return;
	}
	
	tmp = saprintf("%s/%d.pem", prepare_path("keys", 0), uin);
	if (unlink(tmp) == -1)
		print("public_key_not_found", format_user(uin));
	else
		print("public_key_deleted", format_user(uin));
	xfree(tmp);

	SIM_KC_Free(SIM_KC_Find(uin));
}

#endif

COMMAND(cmd_test_debug)
{
	if (params[0])
		gg_debug(GG_DEBUG_MISC, "%s\n", params[0]);
}

COMMAND(cmd_test_segv)
{
	char *foo = NULL;

	*foo = 'A';
}

COMMAND(cmd_test_ping)
{
	if (sess)
		gg_ping(sess);
}

COMMAND(cmd_test_send)
{
	struct gg_event *e = xmalloc(sizeof(struct gg_event));

	if (!params[0] || !params[1])
		return;

	memset(e, 0, sizeof(*e));
	e->type = GG_EVENT_MSG;
	e->event.msg.sender = get_uin(params[0]);
	e->event.msg.message = xstrdup(params[1]);
	e->event.msg.msgclass = GG_CLASS_MSG;
	
	handle_msg(e);
}

COMMAND(cmd_test_add)
{
	if (params[0])
		add_send_nick(params[0]);
}

COMMAND(cmd_test_watches)
{
	list_t l;
	char buf[200], *type, *state, *check;
	int no = 0, queue = -1;

	for (l = watches; l; l = l->next, no++) {
		struct gg_common *s = l->data;
		
		switch (s->type) {
			case GG_SESSION_GG: type = "GG"; break;
			case GG_SESSION_HTTP: type = "HTTP"; break;
			case GG_SESSION_SEARCH: type = "SEARCH"; break;
			case GG_SESSION_REGISTER: type = "REGISTER"; break;
			case GG_SESSION_UNREGISTER: type = "UNREGISTER"; break;
			case GG_SESSION_REMIND: type = "REMIND"; break;
			case GG_SESSION_CHANGE: type = "CHANGE"; break;
			case GG_SESSION_PASSWD: type = "PASSWD"; break;
			case GG_SESSION_DCC: type = "DCC"; break;
			case GG_SESSION_DCC_SOCKET: type = "DCC_SOCKET"; break;
			case GG_SESSION_DCC_SEND: type = "DCC_SEND"; break;
			case GG_SESSION_DCC_GET: type = "DCC_GET"; break;
			case GG_SESSION_DCC_VOICE: type = "DCC_VOICE"; break;
			case GG_SESSION_USERLIST_PUT: type = "USERLIST_PUT"; break;
			case GG_SESSION_USERLIST_GET: type = "USERLIST_GET"; break;
			case GG_SESSION_USER0: type = "USER0"; break;
			case GG_SESSION_USER1: type = "USER1"; break;
			case GG_SESSION_USER2: type = "USER2"; break;
			case GG_SESSION_USER3: type = "USER3"; break;
			case GG_SESSION_USER4: type = "USER4"; break;
			case GG_SESSION_USER5: type = "USER5"; break;
			case GG_SESSION_USER6: type = "USER6"; break;
			case GG_SESSION_USER7: type = "USER7"; break;
			default: type = "(unknown)"; break;
		}
		switch (s->check) {
			case GG_CHECK_READ: check = "R"; break;
			case GG_CHECK_WRITE: check = "W"; break;
			case GG_CHECK_READ | GG_CHECK_WRITE: check = "RW"; break;
			default: check = "?"; break;
		}
		switch (s->state) {
			/* gg_common */
			case GG_STATE_IDLE: state = "IDLE"; break;
			case GG_STATE_RESOLVING: state = "RESOLVING"; break;
			case GG_STATE_CONNECTING: state = "CONNECTING"; break;
			case GG_STATE_READING_DATA: state = "READING_DATA"; break;
			case GG_STATE_ERROR: state = "ERROR"; break;
			/* gg_session */	     
			case GG_STATE_CONNECTING_GG: state = "CONNECTING_GG"; break;
			case GG_STATE_READING_KEY: state = "READING_KEY"; break;
			case GG_STATE_READING_REPLY: state = "READING_REPLY"; break;
			case GG_STATE_CONNECTED: state = "CONNECTED"; break;
			/* gg_http */
			case GG_STATE_READING_HEADER: state = "READING_HEADER"; break;
			case GG_STATE_PARSING: state = "PARSING"; break;
			case GG_STATE_DONE: state = "DONE"; break;
			/* gg_dcc */
			case GG_STATE_LISTENING: state = "LISTENING"; break;
			case GG_STATE_READING_UIN_1: state = "READING_UIN_1"; break;
			case GG_STATE_READING_UIN_2: state = "READING_UIN_2"; break;
			case GG_STATE_SENDING_ACK: state = "SENDING_ACK"; break;
			case GG_STATE_SENDING_REQUEST: state = "SENDING_REQUEST"; break;
			case GG_STATE_READING_REQUEST: state = "READING_REQUEST"; break;
			case GG_STATE_SENDING_FILE_INFO: state = "SENDING_FILE_INFO"; break;
			case GG_STATE_READING_ACK: state = "READING_ACK"; break;
			case GG_STATE_READING_FILE_ACK: state = "READING_FILE_ACK"; break;
			case GG_STATE_SENDING_FILE_HEADER: state = "SENDING_FILE_HEADER"; break;
			case GG_STATE_GETTING_FILE: state = "SENDING_GETTING_FILE"; break;
			case GG_STATE_SENDING_FILE: state = "SENDING_SENDING_FILE"; break;
			default: state = "(unknown)"; break;
		}

#ifdef FIONREAD
		ioctl(s->fd, FIONREAD, &queue);
#endif
		
		snprintf(buf, sizeof(buf), "%d: type=%s, fd=%d, state=%s, check=%s, id=%d, timeout=%d, queue=%d", no, type, s->fd, state, check, s->id, s->timeout, queue);
		print("generic", buf);
	}
}

COMMAND(cmd_test_fds)
{
#if 0
	struct stat st;
	char buf[1000];
	int i;
	
	for (i = 0; i < 2048; i++) {
		if (fstat(i, &st) == -1)
			continue;

		sprintf(buf, "%d: ", i);

		if (S_ISREG(st.st_mode))
			sprintf(buf + strlen(buf), "file, inode %lu, size %lu", st.st_ino, st.st_size);

		if (S_ISSOCK(st.st_mode)) {
			struct sockaddr sa;
			struct sockaddr_un *sun = (struct sockaddr_un*) &sa;
			struct sockaddr_in *sin = (struct sockaddr_in*) &sa;
			int sa_len = sizeof(sa);
			
			if (getpeername(i, &sa, &sa_len) == -1) {
				strcat(buf, "socket, not connected");
			} else {
				switch (sa.sa_family) {
					case AF_UNIX:
						strcat(buf, "socket, unix, ");
						strcat(buf, sun->sun_path);
						break;
					case AF_INET:
						strcat(buf, "socket, inet, ");
						strcat(buf, inet_ntoa(sin->sin_addr));
						strcat(buf, ":");
						strcat(buf, itoa(ntohs(sin->sin_port)));
						break;
					default:
						strcat(buf, "socket, ");
						strcat(buf, itoa(sa.sa_family));
				}
			}
		}
		
		if (S_ISDIR(st.st_mode))
			strcat(buf, "directory");
		
		if (S_ISCHR(st.st_mode))
			strcat(buf, "char");

		if (S_ISBLK(st.st_mode))
			strcat(buf, "block");

		if (S_ISFIFO(st.st_mode))
			strcat(buf, "fifo");

		if (S_ISLNK(st.st_mode))
			strcat(buf, "symlink");

		print("generic", buf);
	}
#endif
}

COMMAND(cmd_register)
{
	struct gg_http *h;
	list_t l;

	if (name[0] == 'r') {
		if (registered_today) {
			print("registered_today");
			return;
		}
	
		if (!params[0] || !params[1]) {
			print("not_enough_params", name);
			return;
		}
	
		for (l = watches; l; l = l->next) {
			struct gg_common *s = l->data;

			if (s->type == GG_SESSION_REGISTER) {
				print("register_pending");
				return;
			}
		}
	
		if (!(h = gg_register(params[0], params[1], 1))) {
			print("register_failed", strerror(errno));
			return;
		}

		list_add(&watches, h, 0);

		reg_password = xstrdup(params[1]);
		reg_email = xstrdup(params[0]);
	} else {
		uin_t uin = 0;
		const char *pwd = NULL;
		
		if (!params[0] || (!params[1] && !config_password) || (!params[2] && !config_uin)) {
			print("not_enough_params", name);
			return;
		} 
		
		pwd = (params[1] ? params[1] : config_password);
		uin = ((params[1] && params[2]) ? get_uin(params[2]) : config_uin);

		if (uin <= 0) {
			print("unregister_bad_uin", uin);
			return;
		}

		if (!(h = gg_unregister(uin, pwd, params[0], 1))) {
			print("unregister_failed", strerror(errno));
			return;
		}

		list_add(&watches, h, 0);
	}
}

COMMAND(cmd_passwd)
{
	struct gg_http *h;
	
	if (!params[0] || (!params[1] && !config_email)) {
		print("not_enough_params", name);
		return;
	}

	if (!(h = gg_change_passwd2(config_uin, (config_password) ? config_password : "", params[0], (config_email) ? config_email : "", (params[1]) ? params[1] : config_email, 1))) {
		print("passwd_failed", strerror(errno));
		return;
	}

	list_add(&watches, h, 0);

	reg_password = xstrdup(params[0]);
	reg_email = xstrdup((params[1]) ? params[1] : config_email);
}

COMMAND(cmd_remind)
{
	struct gg_http *h;
	
	if (!(h = gg_remind_passwd(config_uin, 1))) {
		print("remind_failed", strerror(errno));
		return;
	}

	list_add(&watches, h, 0); 
}

static int count_params(const char **params) {
	int count = 0;

	if (!params)
		return 0;

	while (*params) {
		count++;
		params++;
	}

	return count;
}

COMMAND(cmd_query)
{
	/* XXX potrzebujemy kopii params, aby móc modyfikowaæ parametry */
	char **p = xmalloc(count_params(params) * sizeof(char *));
	int i;

	for (i = 0; params[i]; i++)
		p[i] = xstrdup(params[i]);

	if (params[0] && (params[0][0] == '@' || strchr(params[0], ','))) {
		struct conference *c = conference_create(params[0]);
		
		if (!c)
			goto cleanup;

		ui_event("command", "query", c->name, NULL);

		xfree(p[0]);
		p[0] = xstrdup(c->name);

	} else {
		if (params[0] && params[0][0] == '#') {
			struct conference *c = conference_find(params[0]);

			if (!c) {
				print("conferences_noexist", params[0]);
				goto cleanup;
			}
		
			ui_event("command", "query", c->name, NULL);

			xfree(p[0]);
			p[0] = xstrdup(c->name);

		} else {

			if (params[0] && !get_uin(params[0])) {
				print("user_not_found", params[0]);
				goto cleanup;
			} else
				ui_event("command", "query", params[0], NULL);
		}
	}
		
	if (params[0] && params[1])
		cmd_msg("chat", (const char **)p, NULL);

cleanup:
	for (i = 0; params[i]; i++)
		xfree(p[i]);

	xfree(p);
}

COMMAND(cmd_on)
{
        int flags;
        uin_t uin;

        if (!params[0] || !strncasecmp(params[0], "-l", 2)) {
                list_t l;
                int count = 0;

                for (l = events; l; l = l->next) {
                        struct event *ev = l->data;

                        print((ev->flags & INACTIVE_EVENT) ? "events_list_inactive" :"events_list", event_format(abs(ev->flags)), (ev->uin == 1) ? "*" : format_user(ev->uin), ev->action);
                        count++;
                }

                if (!count)
                        print("events_list_empty");

                return;
        }

        if (!params[1] || !params[2]) {
                print("not_enough_params", name);
                return;
        }

        if (!(flags = event_flags(params[0]))) {
                print("events_incorrect");
                return;
        }

        if (*params[1] == '*')
                uin = 1;
        else
                uin = get_uin(params[1]);

        if (!uin) {
                print("invalid_uin");
                return;
        }

        if (!strncasecmp(params[2], "clear", 5)) {
                event_remove(flags, uin);
                config_changed = 1;
                return;
        }

        if (event_correct(params[2]))
                return;

	event_add(flags, uin, params[2], 0);
        config_changed = 1;
}

COMMAND(cmd_echo)
{
	print("generic", (params && params[0]) ? params[0] : "");
}

COMMAND(cmd_window)
{
	ui_event("command", "window", (params) ? params[0] : NULL, (params && params[0]) ? params[1] : NULL, NULL);
}

COMMAND(cmd_bind)
{
	ui_event("command", "bind", (params) ? params[0] : NULL, (params && params[0]) ? params[1] : NULL, (params && params[1]) ? params[2] : NULL, NULL); 
}

COMMAND(cmd_test_vars)
{
	char *tmp = variable_digest();
	print("generic", tmp);
	xfree(tmp);
}

/*
 * command_exec()
 * 
 * wykonuje polecenie zawarte w linii tekstu.
 *
 *  - target - w którym oknie nast±pi³o (NULL je¶li to nie query)
 *  - xline - linia tekstu.
 *
 * zmienia zawarto¶æ bufora line.
 */
void command_exec(const char *target, const char *xline)
{
	char *cmd = NULL, *tmp, *p = NULL, short_cmd[2] = ".", *last_name = NULL, *last_params = NULL, *line_save = NULL, *line = NULL;
	command_func_t *last_abbr = NULL;
	int abbrs = 0;
	int correct_command = 0;
	list_t l;

	if (!xline)
		return;

	if (!strcmp(xline, "")) {
		if (batch_mode && !batch_line) {
			quit_message_send = 1;
			ekg_exit();
		}
		return;
	}

	if (target && *xline != '/') {
	
		/* wykrywanie przypadkowo wpisanych poleceñ */
		if (config_query_commands) {
			for (l = commands; l; l = l->next) {
				struct command *c = l->data;
				int l = strlen(c->name);

				if (l > 2 && !strncasecmp(xline, c->name, l)) {
					if (!xline[l] || isspace(xline[l])) {
						correct_command = 1;
						break;
					}
				}
			}		
		}

		if (!correct_command) {
			const char *params[] = { target, xline, NULL };

			if (strcmp(xline, ""))
				cmd_msg("chat", params, NULL);

			return;
		}
	}
	
	send_nicks_index = 0;

	line = line_save = xstrdup(xline);
	line = strip_spaces(line);
	
	if (*line == '/')
		line++;

	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (!isalpha(c->name[0]) && strlen(c->name) == 1 && line[0] == c->name[0]) {
			short_cmd[0] = c->name[0];
			cmd = short_cmd;
			p = line + 1;
		}
	}

	if (!cmd) {
		tmp = cmd = line;
		while (*tmp && !isspace(*tmp))
			tmp++;
		p = (*tmp) ? tmp + 1 : tmp;
		*tmp = 0;
		p = strip_spaces(p);
	}
		
	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (!strcasecmp(c->name, cmd)) {
			last_abbr = c->function;
			last_name = c->name;
			last_params = c->params;
			abbrs = 1;		
			break;
		}
		if (!strncasecmp(c->name, cmd, strlen(cmd))) {
			abbrs++;
			last_abbr = c->function;
			last_name = c->name;
			last_params = c->params;
		} else {
			if (last_abbr && abbrs == 1)
				break;
		}
	}

	if (last_abbr && abbrs == 1) {
		char **par;
		int len = strlen(last_params);

		par = array_make(p, " \t", len, 1, 1);
		(last_abbr)(last_name, (const char**) par, target);
		array_free(par);

		xfree(line_save);

		if (quit_command)
			ekg_exit();

		return;
	}

	if (strcmp(cmd, ""))
		print("unknown_command", cmd);

	xfree(line_save);
}

/*
 * binding_quick_list()
 *
 * wy¶wietla krótk± i zwiêz³a listê dostêpnych, zajêtych i niewidocznych
 * ludzi z listy kontaktów.
 */
int binding_quick_list(int a, int b)
{
	string_t list = string_init(NULL);
	list_t l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		const char *format = NULL;
		char *tmp;
		
		switch (u->status) {
			case GG_STATUS_AVAIL:
			case GG_STATUS_AVAIL_DESCR:
				format = format_find("quick_list_avail");
				break;
			case GG_STATUS_BUSY:
			case GG_STATUS_BUSY_DESCR:
				format = format_find("quick_list_busy");
				break;
			case GG_STATUS_INVISIBLE:
			case GG_STATUS_INVISIBLE_DESCR:
				format = format_find("quick_list_invisible");
				break;
		}

		if (!format)
			continue;

		if (!(tmp = format_string(format, u->display)))
			continue;

		string_append(list, tmp);

		xfree(tmp);
	}
	
	if (strlen(list->str) > 0)
		print("quick_list", list->str);

	string_free(list, 1);

	return 0;
}

int binding_help(int a, int b)
{
	print("help_quick");

	return 0;
}

int binding_toggle_debug(int a, int b)
{
	config_debug = !config_debug;
	changed_debug("debug");
	ui_event("variable_changed", "debug", NULL);

	return 0;
}

int binding_toggle_contacts(int a, int b)
{
#ifdef WITH_UI_NCURSES
	static int last_contacts = -1;

	if (!config_contacts) {
		if ((config_contacts = last_contacts) == -1)
			config_contacts = 2;
	} else {
		last_contacts = config_contacts;
		config_contacts = 0;
	}

	contacts_rebuild();
	ui_event("variable_changed", "contacts", NULL);
#endif

	return 0;
}

COMMAND(cmd_alias_exec)
{	
	list_t l, m = NULL;

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;

		if (!strcasecmp(name, a->name)) {
			m = a->commands;
			break;
		}
	}
	
	for (; m; ) {
		char *tmp = saprintf("%s%s%s%s", (*(char*)(m->data) == '/') ? "" : "/", (char*) m->data, (params[0]) ? " " : "", (params[0]) ? params[0] : "");
		m = m->next;
		command_exec(NULL, tmp);
		xfree(tmp);
	}
}

COMMAND(cmd_timer)
{
	list_t l;

	if (match_arg(params[0], 'a', "add", 2)) {
		if (!params[1] || !params[2]) {
			print("not_enough_params", name);
			return;
		}
		timer_add(atoi(params[1]), NULL, params[2]);

		return;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		if (!params[1]) {
			print("not_enough_params", name);
			return;
		}
		timer_remove(params[1], NULL);
		return;
	}

	for (l = timers; l; l = l->next) {
		struct timer *t = l->data;

		print("timer_list", t->name, itoa(t->period - (time(NULL) - t->started)), t->command);
	}
}

#ifdef WITH_PYTHON

COMMAND(cmd_python)
{
	if (!params[0]) {
		python_list();
		return;
	}

	if (!strncasecmp(params[0], "lo", 2)) {
		python_load(params[1]);
		return;
	}

	if (!strncasecmp(params[0], "u", 1)) {
		python_unload(params[1]);
		return;
	}

	if (!strncasecmp(params[0], "r", 1)) {
		python_run(params[1]);
		return;
	}

	if (!strncasecmp(params[0], "e", 1)) {
		python_exec(params[1]);
		return;
	}

	if (!strncasecmp(params[0], "li", 2)) {
		python_list();
		return;
	}

	print("invalid_params", name);
}

#endif

COMMAND(cmd_conference) 
{
	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] == '#') {
		list_t l, r;
		int count = 0;
		const char *cname = (params[0] && params[0][0] == '#') ? params[0] : NULL;

		for (l = conferences; l; l = l->next) {
			struct conference *c = l->data;
			string_t recipients;
			const char *recipient;
			int first = 0;

			recipients = string_init(NULL);

			if (cname && strcmp(cname, c->name))
				continue;
			
			for (r = c->recipients; r; r = r->next) {
				uin_t uin = *((uin_t *) (r->data));
				struct userlist *u = userlist_find(uin, NULL);

				if (u)
					recipient = u->display;
				else
					recipient = itoa(uin);

				if (first++)
					string_append_c(recipients, ',');
				
				string_append(recipients, recipient);

				count++;
			}

		        print(c->ignore ? "conferences_list_ignored" : "conferences_list", c->name, recipients->str);

			string_free(recipients, 1);
		}

		if (!count)
			print("conferences_list_empty");

		return;
	}

	if (match_arg(params[0], 'a', "add", 2)) {
		if (!params[1] || !params[2]) {
			print("not_enough_params", name);
			return;
		}
		
		conference_add(params[1], params[2], 0);

		return;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		if (!params[1]) {
			print("not_enough_params", name);
			return;
		}

		conference_remove(params[1]);

		return;
	}

	if (match_arg(params[0], 'r', "rename", 2)) {
		if (!params[1] || !params[2]) {
			print("not_enough_params", name);
			return;
		}
		
		conference_rename(params[1], params[2]);

		return;
	}
	
	if (match_arg(params[0], 'i', "ignore", 2)) {
		if (!params[1]) {
			print("not_enough_params", name);
			return;
		}

		conference_set_ignore(params[1], 1);

		return;
	}

	if (match_arg(params[0], 'u', "unignore", 2)) {
		if (!params[1]) {
			print("not_enough_params", name);
			return;
		}

		conference_set_ignore(params[1], 0);

		return;
	}

	if (match_arg(params[0], 'f', "find", 2)) {
		struct conference *c;
		char *tmp = NULL;
		list_t l;

		if (!params[1]) {
			print("not_enough_params", name);
			return;
		}

		c = conference_find(params[1]);

		if (c) {
			for (l = c->recipients; l; l = l->next) {
				tmp = saprintf("find --uin %d", *((uin_t *) (l->data)));
				command_exec(NULL, tmp);
				xfree(tmp);
			}
		} else
			print("conferences_noexist", params[1]);


		return;
	}

	print("conferences_invalid");
}

COMMAND(cmd_last)
{
        list_t l;
	uin_t uin = 0;
	int show_sent = 0;

	if (match_arg(params[0], 'c', "clear", 2)) {
		if (params[1] && !(uin = get_uin(params[1]))) {
			print("user_not_found", params[1]);
			return;
		}

		if ((uin && !last_count_get(uin)) || !list_count(lasts)) {
			(uin) ? print("last_list_empty_nick", format_user(uin)) : print("last_list_empty");
			return;
		}

		last_del(uin);
		
		if (uin)
			print("last_clear_uin", format_user(uin));
		else
			print("last_clear");

		return;
	}		

	if (match_arg(params[0], 's', "stime", 2)) {
		show_sent = 1;

		if (params[1] && !(uin = get_uin(params[1]))) {
			print("user_not_found", params[1]);
			return;
		}
	} else
		if (params[0] && !(uin = get_uin(params[0]))) {
		print("user_not_found", params[0]);
		return;
		}
		
	if (!((uin > 0) ? last_count_get(uin) : list_count(lasts))) {
		(uin) ? print("last_list_empty_nick", format_user(uin)) : print("last_list_empty");
		return;
	}

        for (l = lasts; l; l = l->next) {
                struct last *ll = l->data;
		struct tm *tm, *st;
		char buf[100], buf2[100];

		if (uin == 0 || uin == ll->uin) {
			tm = localtime(&ll->time);
			strftime(buf, sizeof(buf), format_find("last_list_timestamp"), tm);

			if (show_sent && ll->type == 0 && !(ll->sent_time - config_time_deviation <= ll->time && ll->time <= ll->sent_time + config_time_deviation)) {
				st = localtime(&ll->sent_time);
				strftime(buf2, sizeof(buf2), format_find((ll->sent_time / 86400 == time(NULL) / 86400) ? "last_list_timestamp_today" : "last_list_timestamp"), st);

				strncat(buf, "/", sizeof(buf) - strlen(buf) - 1);
				strncat(buf, buf2, sizeof(buf) - strlen(buf) - 1);
			}

			if (config_last & 4 && ll->type == 1)
				print("last_list_out", buf, format_user(ll->uin), ll->message);
			else
				print("last_list_in", buf, format_user(ll->uin), ll->message);
		}
        }
}

COMMAND(cmd_queue)
{
	list_t l;
	uin_t uin = 0;

	if (sess && sess->state == GG_STATE_CONNECTED) {
		print("queue_wrong_use");
		return;
	}

	if (match_arg(params[0], 'c', "clear", 2)) {
		if (params[1] && !(uin = get_uin(params[1]))) {
			print("user_not_found", params[1]);
			return;
		}

		if ((uin && !msg_queue_count_uin(uin)) || !msg_queue_count()) {
			(uin) ? print("queue_empty_uin", format_user(uin)) : print("queue_empty");
			return;
		}

		if (uin) {
			msg_queue_remove_uin(uin);
			print("queue_clear_uin", format_user(uin));
		} else {
			msg_queue_free();
			print("queue_clear");
		}

		return;
	}		

	if (params[0] && !(uin = get_uin(params[0]))) {
		print("user_not_found", params[0]);
		return;
	}  
		
	if ((uin && !msg_queue_count_uin(uin)) || !msg_queue_count()) {
		(uin) ? print("queue_empty_uin", format_user(uin)) : print("queue_empty");
		return;
	}

        for (l = msg_queue; l; l = l->next) {
                struct msg_queue *m = l->data;
		struct tm *tm;
		char *fu = NULL;
		char buf[100];

		if (!uin || find_in_uins(m->uin_count, m->uins, uin)) {
			tm = localtime(&m->time);
			strftime(buf, sizeof(buf), format_find("queue_list_timestamp"), tm);

			if (m->uin_count > 1) {
				string_t s = string_init(format_user(*(m->uins)));
				int i;

				for (i = 1; i < m->uin_count; i++) {
					string_append(s, ",");
					string_append(s, format_user(m->uins[i]));
				}
				
				fu = xstrdup(s->str);
				string_free(s, 1);
			} else
				fu = (char *)format_user(*(m->uins));
				
			print("queue_list_message", buf, fu, m->msg);

			if (m->uin_count > 1)
				xfree(fu);
		}
	}
}

/*
 * command_add_compare()
 *
 * funkcja porównuj±ca nazwy komend, by wystêpowa³y alfabetycznie w li¶cie.
 *
 *  - data1, data2 - dwa wpisy komend do porównania.
 *
 * zwraca wynik strcasecmp() na nazwach komend.
 */
static int command_add_compare(void *data1, void *data2)
{
	struct command *a = data1, *b = data2;

	if (!a || !a->name || !b || !b->name)
		return 0;

	return strcasecmp(a->name, b->name);
}

/*
 * command_add()
 *
 * dodaje komendê. 
 *
 *  - name - nazwa komendy,
 *  - params - definicja parametrów (szczegó³y poni¿ej),
 *  - function - funkcja obs³uguj±ca komendê,
 *  - help_params - opis parametrów,
 *  - help_brief - krótki opis komendy,
 *  - help_long - szczegó³owy opis komendy.
 *
 * 0 je¶li siê uda³o, -1 je¶li b³±d.
 */
int command_add(const char *name, const char *params, command_func_t function, int alias, const char *params_help, const char *brief_help, const char *long_help)
{
	struct command c;

	c.name = xstrdup(name);
	c.params = xstrdup(params);
	c.function = function;
	c.alias = alias;
	c.params_help = xstrdup(params_help);
	c.brief_help = xstrdup(brief_help);
	c.long_help = xstrdup(long_help);

	return (list_add_sorted(&commands, &c, sizeof(c), command_add_compare) != NULL) ? 0 : -1;
}

/*
 * command_remove()
 *
 * usuwa komendê z listy.
 *
 *  - name - nazwa komendy.
 */
int command_remove(const char *name)
{
	list_t l;

	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (!strcasecmp(name, c->name)) {
			xfree(c->name);
			xfree(c->params);
			xfree(c->params_help);
			xfree(c->brief_help);
			xfree(c->long_help);
			list_remove(&commands, c, 1);
			return 0;
		}
	}

	return -1;
}

/*
 * rodzaje parametrów komend:
 *
 * '?' - olewamy,
 * 'U' - rêcznie wpisany uin, nadawca mesgów,
 * 'u' - nazwa lub uin z kontaktów, rêcznie wpisany uin, nadawca mesgów,
 * 'c' - komenda,
 * 'i' - nicki z listy ignorowanych osób,
 * 'd' - komenda dcc,
 * 'f' - plik.
 */

/*
 * command_init()
 *
 * inicjuje listê domy¶lnych komend.
 */
void command_init()
{
	command_add
	( "add", "U??", cmd_add, 0,
	  " <numer> <alias> [opcje]", "dodaje u¿ytkownika do listy kontaktów",
	  "Opcje identyczne jak dla polecenia %Tlist%n (dotycz±ce wpisu)");
	  
	command_add
	( "alias", "??", cmd_alias, 0,
	  " [opcje]", "Zarz±dzanie aliasami",
	  "  -a, --add <alias> <komenda>     dodaje alias\n"
          "  -A, --append <alias> <komenda>  dodaje komendê do aliasu\n"
	  "  -d, --del <alias>               usuwa alias\n"
	  " [-l, --list]                     wy¶wietla listê aliasów\n");
	  
	command_add
	( "away", "?", cmd_away, 0,
	  " [powód/-]", "zmienia stan na zajêty",
	  "Je¶li w³±czona jest odpowiednia opcja %Trandom_reason%n i nie\n"
          "podano powodu, zostanie wylosowany z pliku %Taway.reasons%n.\n"
 	  "Podanie %T-%n zamiast powodu spowoduje wyczyszczenie bez\n"
	  "wzglêdu na ustawienia zmiennych.");
 
	command_add
	( "back", "?", cmd_away, 0,
	  " [powód/-]", "zmienia stan na dostêpny",
          "Je¶li w³±czona jest odpowiednia opcja %Trandom_reason%n i nie\n"
	  "podano powodu, zostanie wylosowany z pliku %Tback.reasons%n.\n"
	  "Podanie %T-%n zamiast powodu spowoduje wyczyszczenie bez\n"
	  "wzglêdu na ustawienia zmiennych.");
	  
	command_add
	( "block", "u", cmd_block, 0,
	  " [numer/alias]", "dodaje do listy blokowanych",
	  "");

	command_add
	( "bind", "???", cmd_bind, 0,
	  " <opcja> [sekwencja] [komenda]", "przypisywanie akcji klawiszom",
	  "  -a, --add <sekwencja> <komenda>  przypisuje now± sekwencjê\n"
	  "  -d, --del <sekwencja>            usuwa podan± sekwencjê\n"
	  "  -l, --list                       wy¶wietla przypisane sekwencje\n"
	  "\n"
	  "Dostêpne sekwencje to: Ctrl-<znak>, Alt-<znak>."
#ifndef WITH_UI_NCURSES
	  " Nale¿y pamiêtaæ, \n"
	  "¿e dla klawisza Alt wielko¶æ znaku ma znaczenie, a dla Ctrl nie."
#endif
	  );

	command_add
	( "change", "?", cmd_change, 0,
	  " <opcje>", "zmienia informacje w katalogu publicznym",
	  "  -f, --first <imiê>\n"
          "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>\n"
	  "  -e, --email <adres>\n"
	  "  -b, --born <rok urodzenia>\n"
	  "  -c, --city <miasto>\n"
	  "  -F, --female                 kobieta\n"
	  "  -M, --male                   mê¿czyzna\n"
	  "\n"
	  "Nale¿y podaæ %Twszystkie%n opcje.");
	  
	command_add
	( "chat", "u?", cmd_msg, 0,
	  " <numer/alias/@grupa> <wiadomo¶æ>", "wysy³a wiadomo¶æ w rozmowie",
	  "Mo¿na podaæ wiêksz± ilo¶æ odbiorców oddzielaj±c ich numery lub\n"
	  "pseudonimy przecinkiem (ale bez odstêpów). W takim wypadku\n"
	  "zostanie rozpoczêta rozmowa grupowa.");
	  
	command_add
	( "cleartab", "", cmd_cleartab, 0,
	  "", "czy¶ci listê nicków do dope³nienia",
	  "");
	  
	command_add
	( "connect", "??", cmd_connect, 0,
	  " [[numer] has³o]", "³±czy siê z serwerem",
	  "Je¶li podano jeden parametr, jest on traktowany jako has³o,\n"
	  "a je¶li podano dwa, s± to kolejno numer i has³o. Dane te s±\n"
	  "ustawiane w konfiguracji i zostan± utrwalone po wydaniu komendy\n"
	  "%Tsave%n.");
	  
	command_add
	( "dcc", "duf?", cmd_dcc, 0,
	  " [opcje]", "obs³uga bezpo¶rednich po³±czeñ",
	  "  send <numer/alias> <¶cie¿ka>  wysy³a podany plik\n"
	  "  get [numer/alias/#id]         akceptuje przysy³any plik\n"
	  "  voice <numer/alias/#id>       rozpoczna rozmowê g³osow±\n"
	  "  close [numer/alias/#id]       zamyka po³±czenie\n"
	  " [show]                         wy¶wietla listê po³±czeñ\n"
	  "\n"
	  "Po³±czenia bezpo¶rednie wymagaj± w³±czonej opcji %Tdcc%n.\n"
	  "dok³adny opis znajduje siê w pliku %Tdocs/dcc.txt%n");
	  
	command_add
	( "del", "u", cmd_del, 0,
	  " <numer/alias>", "usuwa u¿ytkownika z listy kontaktów",
	  "");
	
	command_add
	( "disconnect", "?", cmd_connect, 0,
	  " [powód/-]", "roz³±cza siê z serwerem",
	  "Parametry identyczne jak dla komendy %Tquit%n.\n"
	  "\n"
	  "Je¶li w³±czona jest opcja %Tauto_reconnect%n, po wywo³aniu\n"
	  "tej komendy, program nadal bêdzie próbowa³ siê automatycznie\n"
	  "³±czyæ po okre¶lonym czasie.");
	  
	command_add
	( "echo", "?", cmd_echo, 0,
	  " <tekst>", "wy¶wietla podany tekst",
	  "");
	  
	command_add
	( "exec", "?", cmd_exec, 0,
	  " <polecenie>", "uruchamia polecenie systemowe",
	  "Poprzedzenie znakiem %T^%n ukryje informacjê o zakoñczeniu.");
	  
	command_add
	( "!", "?", cmd_exec, 0,
	  " <polecenie>", "synonim dla %Texec%n",
	  "");

	command_add
	( "find", "u", cmd_find, 0,
	  " [opcje]", "przeszukiwanie katalogu publicznego",
	  "  -u, --uin <numerek>\n"
	  "  -f, --first <imiê>\n"
	  "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>\n"
	  "  -c, --city <miasto>\n"
	  "  -b, --born <min:max>    zakres roku urodzenia\n"
	  "  -e, --email <e-mail>\n"
	  "  -a, --active            tylko dostêpni\n"
	  "  -F, --female            kobiety\n"
	  "  -M, --male              mê¿czy¼ni\n"
	  "  --start <n>             wy¶wietla od n-tego wyniku\n"
	  "  -A, --all               wy¶wietla wszystkich\n"
	  "\n"
	  "  Ze wzglêdu na organizacjê katalogu publicznego, niektórych\n"
	  "  opcji nie mo¿na ze sob± ³±czyæ");
	  
	command_add
	( "help", "c", cmd_help, 0,
	  " [polecenie]", "wy¶wietla informacjê o poleceniach",
	  "");
	  
	command_add
	( "?", "c", cmd_help, 0,
	  " [polecenie]", "synonim dla %Thelp%n",
	  "");
	 
	command_add
	( "ignore", "u", cmd_ignore, 0,
	  " [opcje] [numer/alias]", "dodaje do listy ignorowanych",
	  "\n"
	  "W przysz³o¶ci bêdzie mo¿liwe wybranie zdarzeñ zwi±zanych z dan±\n"
	  "osob±, takich jak wiadomo¶ci, zmiana stanu, zmiana opisu itp.");
	  
	command_add
	( "invisible", "?", cmd_away, 0,
	  " [powód/-]", "zmienia stan na niewidoczny",
          "Je¶li w³±czona jest odpowiednia opcja %Trandom_reason%n i nie\n"
	  "podano powodu, zostanie wylosowany z pliku %Tquit.reasons%n.\n"
	  "Podanie %T-%n zamiast powodu spowoduje wyczyszczenie bez\n"
	  "wzglêdu na ustawienia zmiennych.");

	command_add
	( "last", "uu", cmd_last, 0,
	  " [opcje]", "wy¶wietla lub czy¶ci ostatnie wiadomo¶ci",
	  " [numer/alias]             wy¶wietla ostatnie wiadomo¶ci\n"
	  " -s, --stime [numer/alias] wy¶wietla czas wys³ania wiadomo¶ci przychodz±cych\n"
	  " -c, --clear [numer/alias] czy¶ci wiadomo¶ci od/do numer/alias lub wszystkie\n"
	  "W przypadku opcji %T-s%n lub %T--stime%n czas wy¶wietlany jest ,,inteligentnie''\n"
	  "zgodnie ze zmienn± %Ttime_deviation.%n"
	  );

	command_add
	( "list", "u?", cmd_list, 0,
          " [alias|@grupa|opcje]", "zarz±dzanie list± kontaktów",
	  "\n"
	  "Wy¶wietlanie osób o podanym stanie \"list [-a|-b|-i|-d|-m]\":\n"
	  "  -a, --active          dostêpne\n"
	  "  -b, --busy            zajête\n"
	  "  -i, --inactive        niedostêpne\n"
	  "  -d, --description     osoby z opisem\n"
	  "  -m, --member <@grupa> osoby nale¿±ce do danej grupy\n"
	  "  -o, --offline         osoby dla których jeste¶my niedostêpni\n"
	  "\n"
	  "Wy¶wietlanie cz³onków grupy: \"list @grupa\"\n"
	  "\n"
	  "Zmiana wpisów listy kontaktów \"list <alias> <opcje...>\":\n"
	  "  -f, --first <imiê>\n"
	  "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>     pseudonim (nie jest u¿ywany)\n"
	  "  -d, --display <nazwa>      wy¶wietlana nazwa\n"
	  "  -u, --uin <numerek>\n"
	  "  -g, --group [+/-]<@grupa>  dodaje lub usuwa z grupy\n"
	  "                             mo¿na podaæ wiêcej grup po przecinku\n"
	  "  -p, --phone <numer>        numer telefonu komórkowego\n"
	  "  -o, --offline              b±d¼ niedostêpny dla danej osoby\n"
	  "  -O, --online               b±d¼ dostêpny dla danej osoby\n"
	  "\n"
	  "Lista kontaktów na serwerze \"list [-p|-g]\":\n"
	  "  -p, --put         umieszcza na serwerze\n"
	  "  -P, --put-config  umieszcza na serwerze razem z konfiguracj±\n"
	  "  -g, --get         pobiera z serwera\n"
	  "  -G, --get-config  pobiera z serwera razem z konfiguracj±\n");
	  
	command_add
	( "msg", "u?", cmd_msg, 0,
	  " <numer/alias/@grupa> <wiadomo¶æ>", "wysy³a wiadomo¶æ",
	  "Mo¿na podaæ wiêksz± ilo¶æ odbiorców oddzielaj±c ich numery lub\n"
	  "pseudonimy przecinkiem (ale bez odstêpów).");

	command_add
        ( "on", "?u?", cmd_on, 0,
	  " <zdarzenie|...> <numer/alias> <akcja>|clear", "obs³uga zdarzeñ",
	  "Szczegó³y dotycz±ce tego polecenia w pliku %Tdocs/on.txt%n");
	 
	command_add
	( "passwd", "??", cmd_passwd, 0,
	  " <has³o> [e-mail]", "zmienia has³o i adres e-mail u¿ytkownika",
	  "");

	command_add
	( "private", "", cmd_away, 0,
	  " [on/off]", "w³±cza/wy³±cza tryb ,,tylko dla przyjació³''",
	  "");
	  
	command_add
	( "query", "u?", cmd_query, 0,
	  " <numer/alias/@grupa> [wiadomo¶æ]", "w³±cza rozmowê z dan± osob±",
	  "Mo¿na podaæ wiêksz± ilo¶æ odbiorców oddzielaj±c ich numery lub\n"
	  "pseudonimy przecinkiem (ale bez odstêpów).");

	command_add
	( "queue", "uu", cmd_queue, 0,
	  " [opcje]", "wy¶wietla lub czy¶ci wiadomo¶ci do wys³ania po po³±czeniu",
	  " [numer/alias]             wy¶wietla kolejkê wiadomo¶ci\n"
	  " -c, --clear [numer/alias] usuwa wiadomo¶ci dla numer/alias lub wszystkie\n"
	  "Mo¿na u¿yæ tylko wtedy, gdy nie jeste¶my po³±czeni. W przypadku\n"
	  "konferencji wy¶wietla wszystkich uczestników.\n"
	  );
	  
	command_add
	( "quit", "?", cmd_quit, 0,
	  " [powód/-]", "wychodzi z programu",
          "Je¶li w³±czona jest odpowiednia opcja %Trandom_reason%n i nie\n"
	  "podano powodu, zostanie wylosowany z pliku %Tquit.reasons%n.\n"
	  "Podanie %T-%n zamiast powodu spowoduje wyczyszczenie bez\n"
	  "wzglêdu na ustawienia zmiennych.");
	  
	command_add
	( "reconnect", "", cmd_connect, 0,
	  "", "roz³±cza i ³±czy ponownie",
	  "");
	  
	command_add
	( "register", "??", cmd_register, 0,
	  " <email> <has³o>", "rejestruje nowe konto",
	  "");
	  
	command_add
	( "remind", "", cmd_remind, 0,
	  "", "wysy³a has³o na skrzynkê pocztow±",
	  "");
	  
	command_add
	( "save", "", cmd_save, 0,
	  "", "zapisuje ustawienia programu",
	  "Aktualny stan zostanie zapisany i zostanie przywrócony przy\n"
	  "nastêpnym uruchomieniu programu.");
	  
	command_add
	( "set", "v?", cmd_set, 0,
  	  " [-]<zmienna> [[+/-]warto¶æ]", "wy¶wietla lub zmienia ustawienia",
	  "U¿ycie %Tset -zmienna%n czy¶ci zawarto¶æ zmiennej. Dla zmiennych\n"
	  "bêd±cymi mapami bitowymi mo¿na okre¶liæ, czy warto¶æ ma byæ\n"
	  "dodana (poprzedzone plusem), usuniêta (minusem) czy ustawiona\n"
	  "(bez prefiksu). Warto¶æ zmiennej mo¿na wzi±æ w cudzys³ów.\n"
	  "Poprzedzenie opcji parametrem %T-a%n lub %T--all%n spowoduje\n"
	  "wy¶wietlenie wszystkich, nawet aktualnie nieaktywnych zmiennych.\n");

	command_add
	( "sms", "u?", cmd_sms, 0,
	  " <numer/alias> <tre¶æ>", "wysy³a smsa do podanej osoby",
	  "Polecenie wymaga zdefiniowana zmiennej %Tsms_send_app%n");

	command_add
	( "status", "", cmd_status, 0,
	  "", "wy¶wietla aktualny stan",
	  "");

	command_add
	( "unregister", "???", cmd_register, 0,
	  " <email> [<has³o> [<uin/nick>]]", "usuwa konto z serwera",
	  "");

	command_add
	( "conference", "???", cmd_conference, 0,
	  " [opcje]", "zarz±dzanie konferencjami",
	  "  -a, --add <#nazwa> <numer/alias/@grupa> tworzy now± konferencjê\n"
	  "  -d, --del <#nazwa>                  usuwa konferencjê\n"
	  "  -i, --ignore <#nazwa>               oznacza konferencjê jako ingorowan±\n"
	  "  -u, --unignore <#nazwa>             oznacza konferencjê jako nieingorowan±\n"
	  "  -r, --rename <#old> <#new>          zmienia nazwê konferencji\n"
	  "  -f, --find <#nazwa>                 wyszukuje uczestnikow w katalogu\n"
	  " [-l, --list, <#nazwa>  ]             wy¶wietla listê konferencji\n"
	  "\n"
	  "Dodaje nazwê konferencji i definiuje, kto bierze w niej udzia³.\n"
	  "Kolejne numery, pseudonimy lub grupy mog± byc odzielone\n"
	  "przecinkiem lub spacj±.");

	command_add
	( "timer", "???", cmd_timer, 0,
	  " [opcje]", "zarz±dzanie timerami",
	  "  -a, --add <czas> <komenda>  tworzy nowy timer\n"
	  "  -d, --del <numer>           zatrzymuje timer\n"
	  " [-l, --list]                 wy¶wietla listê timerów\n"
	  "\n"
	  "Czas podaje siê w sekundach. Timer po jednorazowym uruchomieniu\n"
	  "jest usuwany. Odmierzanie czasu nie jest idealne i ma dok³adno¶æ\n"
	  "+/- 1 sekundy.");

	command_add
	( "unignore", "i", cmd_ignore, 0,
	  " <numer/alias>", "usuwa z listy ignorowanych osób",
	  "");
	  
	command_add
	( "unblock", "b", cmd_block, 0,
	  " <numer/alias>", "usuwa z listy blokowanych osób",
	  "");
	  
	command_add
	( "version", "", cmd_version, 0,
	  "", "wy¶wietla wersje programu",
	  "");
	  
	command_add
	( "window", "w?", cmd_window, 0,
	  " <komenda> [numer_okna]", "zarz±dzanie okienkami",
	  "  new\n"
	  "  kill [numer_okna]\n"
	  "  next\n"
	  "  prev\n"
	  "  switch <numer_okna>\n"
	  "  clear\n"
	  "  refresh\n"
	  "  list");

#ifdef WITH_PYTHON
	command_add
	( "python", "p?", cmd_python, 0,
	  " <komenda> [opcje]", "obs³uga skryptów",
	  "  load <skrypt>    ³aduje skrypt\n"
	  "  unload <skrypt>  usuwa skrypt z pamiêci\n"
	  "  run <plik>       uruchamia skrypt\n"
	  "  exec <komenda>   uruchamia komendê\n"
	  "  list             wy¶wietla listê za³adowanych skryptów\n");
#endif

	command_add
	( "_add", "?", cmd_test_add, 0, "",
	  "dodaje do listy dope³niania TABem", "");
	command_add
	( "_fds", "", cmd_test_fds, 0, "", 
	  "wy¶wietla otwarte deskryptory", "");
	command_add
	( "_msg", "u?", cmd_test_send, 0, "",
	  "udaje, ¿e wysy³a wiadomo¶æ", "");
	command_add
	( "_ping", "", cmd_test_ping, 0, "", 
	  "wysy³a pakiet ping do serwera", "");
	command_add
	( "_segv", "", cmd_test_segv, 0, "", 
	  "wywo³uje naruszenie segmentacji pamiêci", "");
	command_add
	( "_watches", "", cmd_test_watches, 0, "", 
	  "wy¶wietla listê sprawdzanych deskryptorów", "");
	command_add
	( "_debug", "", cmd_test_debug, 0, "",
	  "wy¶wietla tekst", "");
	command_add
	( "_vars", "", cmd_test_vars, 0, "",
	  "wy¶wietla skrót zmiennych", "");
#ifdef HAVE_OPENSSL
	command_add
	( "_keygen", "?", cmd_test_keygen, 0, " <ilo¶æ-bitów>",
	  "generuje parê kluczy", "");
	command_add
	( "_keysend", "u", cmd_test_keysend, 0, " <numer/alias>",
	  "wysy³a klucz publiczny do danej osoby", "");
	command_add
	( "_keydel", "u", cmd_test_keydel, 0, " <numer/alias>",
	  "usuwa klucz publiczny danej osoby", "");
#endif
}

/*
 * command_free()
 *
 * usuwa listê komend z pamiêci.
 */
void command_free()
{
	list_t l;

	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		xfree(c->name);
		xfree(c->params);
		xfree(c->params_help);
		xfree(c->brief_help);
		xfree(c->long_help);
	}

	list_destroy(commands, 1);
	commands = NULL;
}
