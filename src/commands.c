/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo�ny <speedy@ziew.org>
 *                          Pawe� Maziarz <drg@infomex.pl>
 *                          Wojciech Bojdo� <wojboj@htc.net.pl>
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
#include <dirent.h>
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
#  include "simlite.h"
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
 * dodaje do listy nick�w dope�nianych automagicznie tabem.
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
 * usuwa z listy dope�nianych automagicznie tabem.
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

	if (!params[0]) {
		for (i = 0; i < send_nicks_count; i++) {
			xfree(send_nicks[i]);
			send_nicks[i] = NULL;
		}

		send_nicks_count = 0;
		send_nicks_index = 0;

		return 0;
	}

	if (match_arg(params[0], 'o', "offline", 2)) {
		for (i = 0; i < send_nicks_count; i++) {
			struct userlist *u = NULL;

			if (send_nicks[i])
				u = userlist_find(atoi(send_nicks[i]), send_nicks[i]);

			if (!u || !GG_S_NA(u->status))
				continue;

			remove_send_nick(send_nicks[i]);
		}

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_add)
{
	int params_free = 0, result = 0;
	struct userlist *u;
	uin_t uin;

	if (params[0] && !match_arg(params[0], 'f', "find", 2) && !params[1]) {
		ui_event("command", quiet, "add", params[0], NULL);
		return 0;
	}

	if (params[0] && match_arg(params[0], 'f', "find", 2)) {
		int nonick = 0;
		char *nickname, *tmp;

		if (!last_search_uin || !last_search_nickname) {
			printq("search_no_last");
			return -1;
		}

		tmp = strip_spaces(last_search_nickname);

		if ((nonick = !strcmp(tmp, "")) && !params[1]) {
			printq("search_no_last_nickname");
			return -1;
		}

		if (nonick || params[1])
			nickname = (char *) params[1];
		else
			nickname = tmp;

		params_free = 1;

		params = xmalloc(4 * sizeof(char*));
		params[0] = itoa(last_search_uin);
		params[1] = nickname;
		params[2] = saprintf("-f \"%s\" -l \"%s\"", (last_search_first_name) ? last_search_first_name : "", (last_search_last_name) ? last_search_last_name : "");
		params[3] = NULL;
	}

	if (!params[0] || !params[1]) {
		printq("not_enough_params", name);
		result = -1;
		goto cleanup;
	}

	if (!(uin = str_to_uin(params[0]))) {
		printq("invalid_uin");
		result = -1;
		goto cleanup;
	}

	if (!valid_nick(params[1])) {
		printq("invalid_nick");
		result = -1;
		goto cleanup;
	}

	if ((u = userlist_find(uin, params[1])) && u->display) {
		if (!strcasecmp(params[1], u->display) && u->uin == uin) {
			printq("user_exists", params[1]);
		} else {
			printq("user_exists_other", params[1], format_user(u->uin));
		}

		result = -1;
		goto cleanup;
	}

	/* kto� by� tylko ignorowany/blokowany, nadajmy mu nazw� */
	if (u)
		u->display = xstrdup(params[1]);	

	if (u || userlist_add(uin, params[1])) {
		printq("user_added", params[1]);
		if (sess)
			gg_add_notify_ex(sess, uin, userlist_type(u));
		remove_send_nick(itoa(uin));
		config_changed = 1;
		ui_event("userlist_changed", itoa(uin), params[1], NULL);
	}

	if (params[2])
		cmd_modify("add", &params[1], NULL, quiet);

cleanup:
	if (params_free) {
		xfree((char*) params[2]);
		xfree(params);
	}

	return result;
}

COMMAND(cmd_alias)
{
	if (match_arg(params[0], 'a', "add", 2)) {
		if (!params[1] || !strchr(params[1], ' ')) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!alias_add(params[1], 0, 0)) {
			config_changed = 1;
			return 0;
		}

		return -1;
	}

	if (match_arg(params[0], 'A', "append", 2)) {
		if (!params[1] || !strchr(params[1], ' ')) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!alias_add(params[1], quiet, 1)) {
			config_changed = 1;
			return 0;
		}

		return -1;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		int ret;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!strcmp(params[1], "*"))
			ret = alias_remove(NULL, quiet);
		else
			ret = alias_remove(params[1], quiet);

		if (!ret) {
			config_changed = 1;
			return 0;
		}

		return -1;
	}
	
	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		list_t l;
		int count = 0;
		const char *aname = NULL;

		if (params[0] && params[0][0] != '-')
			aname = params[0];
		else if (params[0] && match_arg(params[0], 'l', "list", 2))
			aname = params[1];

		for (l = aliases; l; l = l->next) {
			struct alias *a = l->data;
			list_t m;
			int first = 1, i;
			char *tmp = xcalloc(strlen(a->name) + 1, 1);
			
			if (aname && strcasecmp(aname, a->name))
				continue;

			for (i = 0; i < strlen(a->name); i++)
				strcat(tmp, " ");

			for (m = a->commands; m; m = m->next) {
				printq((first) ? "aliases_list" : "aliases_list_next", a->name, (char*) m->data, tmp);
				first = 0;
				count++;
			}

			xfree(tmp);
		}

		if (!count) {
			if (aname) {
				printq("aliases_noexist", aname);
				return -1;
			}

			printq("aliases_list_empty");
		}

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_away)
{
	in_auto_away = 0;

	if (params[0] && strlen(params[0]) > GG_STATUS_DESCR_MAXSIZE) {
		printq("descr_too_long", itoa(strlen(params[0]) - GG_STATUS_DESCR_MAXSIZE));
		if (config_reason_limit)
			return -1;
	}

	if (!strcasecmp(name, "away"))
		change_status(GG_STATUS_BUSY, params[0], 0);

	if (!strcasecmp(name, "invisible"))
		change_status(GG_STATUS_INVISIBLE, params[0], 0);

	if (!strcasecmp(name, "back")) {
		change_status(GG_STATUS_AVAIL, params[0], 0);
		sms_away_free();
	}

	/* XXX mo�e by tak do osobnej funkcji? */

	if (!strcasecmp(name, "private")) {
		int tmp;

		if (!params[0]) {
			printq(GG_S_F(config_status) ? "private_mode_is_on" : "private_mode_is_off");
			return 0;
		}
		
		if ((tmp = on_off(params[0])) == -1) {
			printq("private_mode_invalid");
			return -1;
		}

		if (tmp == GG_S_F(config_status)) {
			printq(GG_S_F(config_status) ? "private_mode_is_on" : "private_mode_is_off");
			return 0;
		}

		printq((tmp) ? "private_mode_on" : "private_mode_off");
		ui_event("my_status", "private", (tmp) ? "on" : "off", NULL);

		config_status = GG_S(config_status);
		config_status |= ((tmp) ? GG_STATUS_FRIENDS_MASK : 0);

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

	unidle();

	return 0;
}

COMMAND(cmd_status)
{
	struct userlist *u;
	struct in_addr i;
	struct tm *lce, *est;
	int mqc;
	char *tmp, *priv, *r1, *r2, buf[100], buf1[100];

	if (config_profile) {
		printq("show_status_profile", config_profile);
	}

	if ((u = userlist_find(config_uin, NULL)) && u->display) {
		printq("show_status_uin_nick", itoa(config_uin), u->display);
	} else {
		printq("show_status_uin", itoa(config_uin));
	}
	
	lce = localtime(&last_conn_event);
	strftime(buf, sizeof(buf), format_find((last_conn_event / 86400 == time(NULL) / 86400) ? "show_status_last_conn_event_today" : "show_status_last_conn_event"), (const struct tm*) lce);

	est = localtime(&ekg_started);
	strftime(buf1, sizeof(buf1), format_find((ekg_started / 86400 == time(NULL) / 86400) ? "show_status_ekg_started_today" : "show_status_ekg_started"), (const struct tm*) est);
	
	if (!sess || sess->state != GG_STATE_CONNECTED) {
		char *tmp = format_string(format_find("show_status_not_avail"));

		printq("show_status_status", tmp, "");
		printq("show_status_ekg_started_since", buf1);

		if (last_conn_event)
			printq("show_status_disconnected_since", buf);
		if ((mqc = msg_queue_count()))
			printq("show_status_msg_queue", itoa(mqc)); 

		xfree(tmp);

		return 0;
	}

	if (GG_S_F(config_status))
		priv = format_string(format_find("show_status_private_on"));
	else
		priv = format_string(format_find("show_status_private_off"));

	r1 = xstrmid(config_reason, 0, GG_STATUS_DESCR_MAXSIZE);
	r2 = xstrmid(config_reason, GG_STATUS_DESCR_MAXSIZE, -1);

	tmp = format_string(format_find(ekg_status_label(config_status, "show_status_")), r1, r2);

	xfree(r1);
	xfree(r2);
	
	i.s_addr = sess->server_addr;

	printq("show_status_status", tmp, priv);
	printq("show_status_ekg_started_since", buf1);
	printq("show_status_server", inet_ntoa(i), itoa(sess->port));
	printq("show_status_connected_since", buf);

	xfree(tmp);
	xfree(priv);

	return 0;
}

COMMAND(cmd_connect)
{
	if (!strcasecmp(name, "connect")) {
		if (sess) {
			printq((sess->state == GG_STATE_CONNECTED) ? "already_connected" : "during_connect");
			return -1;
		}

		if (params && params[0] && params[1]) {
			variable_set("uin", params[0], 0);
			variable_set("password", params[1], 0);
		}

		if (params && params[0] && !params[1])
			variable_set("password", params[0], 0);
			
                if (config_uin && config_password) {
			printq("connecting");
			connecting = 1;
			do_connect();
		} else {
			printq("no_config");
			return -1;
		}
	} else if (!strcasecmp(name, "reconnect")) {
		cmd_connect("disconnect", NULL, NULL, quiet);
		cmd_connect("connect", NULL, NULL, quiet);
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
				printq("disconnected_descr", r1, r2);
				xfree(r1);
				xfree(r2);
			} else
				printq("disconnected");
			
		} else if (sess->state != GG_STATE_IDLE)
			printq("conn_stopped");

		ekg_logoff(sess, tmp);
		xfree(tmp);
		list_remove(&watches, sess, 0);
		gg_free_session(sess);
		userlist_clear_status(0);
		sess = NULL;
		reconnect_timer = 0;
		ui_event("disconnected", NULL);
	}

	return 0;
}

COMMAND(cmd_del)
{
	struct userlist *u;
	const char *tmp;
	char *nick;
	uin_t uin = 0;
	int del_all = ((params[0] && !strcmp(params[0], "*")) ? 1 : 0);

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!del_all && !(u = userlist_find((uin = get_uin(params[0])), NULL))) {
		printq("user_not_found", params[0]);
		return -1;
	}

	if (del_all) {
		list_t l;

		for (l = userlist; l; ) {
			struct userlist *u = l->data;
		
			l = l->next;

			if (sess)
				gg_remove_notify(sess, uin);
			userlist_remove(u);
		}

		printq("user_cleared_list");
		command_exec(NULL, "^cleartab");
		config_changed = 1;
		return 0;
	}

	nick = xstrdup(u->display);
	tmp = format_user(u->uin);

	if (!userlist_remove(u)) {
		printq("user_deleted", tmp);
		if (sess)
			gg_remove_notify(sess, uin);
		remove_send_nick(itoa(uin));
		remove_send_nick(nick);
		config_changed = 1;
		ui_event("userlist_changed", nick, itoa(uin), NULL);
	}

	xfree(nick);
	
	return 0;
}

COMMAND(cmd_exec)
{
	list_t l;
	int pid;

	if (params[0]) {
		char **args = NULL, *tmp, *tg = xstrdup(target);
		int fd[2] = { 0, 0 }, buf = 0, msg = 0;
		const char *command = NULL;
		struct gg_exec s;

		if (params[0][0] == '-') {
			args = array_make(params[0], " \t", 3, 1, 1);

			if (match_arg(args[0], 'm', "msg", 2) || (buf = match_arg(args[0], 'b', "bmsg", 2))) {
				struct userlist *u;
				int uin;

				if (!args[1] || !args[2]) {
					printq("not_enough_params", name);
					array_free(args);
					return -1;
				}

				if (strcmp(args[1], "$")) {
					if (!(uin = get_uin(args[1]))) {
						printq("user_not_found", args[1]);
						array_free(args);
						return -1;
					}

					if ((u = userlist_find(uin, NULL)))
						tg = xstrdup(u->display);
					else
						tg = xstrdup(itoa(uin));
				}

				msg = (buf) ? 2 : 1;
				command = args[2];
			} else {
				printq("invalid_params", name);
				return -1;
			}
		} else
			command = params[0];

		if (pipe(fd)) {
			printq("exec_error", strerror(errno));
			xfree(tg);
			return -1;
		}

		if (!(pid = fork())) {
			if (fd[1]) {
				close(fd[0]);
				dup2(fd[1], 2);
				dup2(fd[1], 1);
				close(fd[1]);
			}	
			execl("/bin/sh", "sh", "-c", ((command[0] == '^' && strlen(command) > 1) ? command + 1 : command), (void *) NULL);
			exit(1);
		}

		if (pid < 0) {
			printq("exec_error", strerror(errno));
			xfree(tg);
			return -1;
		}
	
		s.fd = fd[0];
		s.check = GG_CHECK_READ;
		s.state = GG_STATE_READING_DATA;
		s.type = GG_SESSION_USER3;
		s.id = pid;
		s.timeout = 60;
		s.buf = string_init(NULL);
		s.target = xstrdup(tg);
		s.msg = msg;
		s.quiet = quiet;

		fcntl(s.fd, F_SETFL, O_NONBLOCK);

		list_add(&watches, &s, sizeof(s));
		close(fd[1]);
		
		if (quiet || command[0] == '^')
			tmp = saprintf("\002%s", command + 1);
		else
			tmp = xstrdup(command);

		process_add(pid, tmp);

		if (args)
			array_free(args);

		xfree(tmp);
		xfree(tg);
	} else {
		for (l = children; l; l = l->next) {
			struct process *p = l->data;
			char *tmp = NULL;
			
			switch (p->name[0]) {
				case '\001':
					tmp = saprintf("wysy�anie sms do %s", p->name + 1);
					break;
				case '\002':
					tmp = saprintf("^%s", p->name + 1);
					break;
				case '\003':
					break;
				default:
					tmp = xstrdup(p->name);
			}

			if (tmp)
				printq("process", itoa(p->pid), tmp);
			xfree(tmp);
		}

		if (!children) {
			printq("no_processes");
			return -1;
		}
	}

	return 0;
}

COMMAND(cmd_find)
{
	char **argv = NULL;
	gg_pubdir50_t req;
	int i, res = 0, all = 0, match = 0;

	if (!sess || sess->state != GG_STATE_CONNECTED) {
		printq("not_connected");
		return -1;
	}

	if (params[0] && match_arg(params[0], 'S', "stop", 3)) {
		list_t l;

		for (l = searches; l; ) {
			gg_pubdir50_t s = l->data;

			l = l->next;
			gg_pubdir50_free(s);
			list_remove(&searches, s, 0);
		}

		printq("search_stopped");

		return 0;
	}
		
	if (!params[0] || !(argv = array_make(params[0], " \t", 0, 1, 1)) || !argv[0]) {
		ui_event("command", quiet, "find", NULL);
		return -1;
	}

	if (argv[0] && !argv[1] && argv[0][0] == '#') { /* konferencja */
		char *tmp = saprintf("%sconference --find %s", ((quiet) ? "^" : ""), argv[0]);
		int res = command_exec(NULL, tmp);
		xfree(tmp);
		array_free(argv);
		return res;
	}

	for (i = 0; argv[i]; i++)
		iso_to_cp(argv[i]);

	if (!(req = gg_pubdir50_new(GG_PUBDIR50_SEARCH)))
		return -1;
	
	if (argv[0] && !argv[1] && argv[0][0] != '-') {
		uin_t uin = get_uin(params[0]);

		if (!uin) {
			printq("user_not_found", params[0]);
			array_free(argv);
			return -1;
		}

		gg_pubdir50_add(req, GG_PUBDIR50_UIN, itoa(uin));

		goto search;
	}

	for (i = 0; argv[i]; i++) {
		char *arg = argv[i];
				
		if (match_arg(arg, 'f', "first", 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_FIRSTNAME, argv[++i]);
			match = 1;
		}

		if (match_arg(arg, 'l', "last", 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_LASTNAME, argv[++i]);
			match = 1;
		}

		if (match_arg(arg, 'n', "nickname", 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_NICKNAME, argv[++i]);
			match = 1;
		}
		
		if (match_arg(arg, 'c', "city", 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_CITY, argv[++i]);
			match = 1;
		}

		if (match_arg(arg, 'u', "uin", 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_UIN, argv[++i]);
			match = 1;
		}
		
		if (match_arg(arg, 's', "start", 3) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_START, argv[++i]);
			match = 1;
		}
		
		if (match_arg(arg, 'F', "female", 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_FEMALE);
			match = 1;
		}

		if (match_arg(arg, 'M', "male", 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_MALE);
			match = 1;
		}

		if (match_arg(arg, 'a', "active", 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_ACTIVE, GG_PUBDIR50_ACTIVE_TRUE);
			match = 1;
		}

		if (match_arg(arg, 'b', "born", 2) && argv[i + 1]) {
			char *foo = strchr(argv[++i], ':');
		
			if (foo)
				*foo = ' ';

			gg_pubdir50_add(req, GG_PUBDIR50_BIRTHYEAR, argv[i]);
			match = 1;
		}

		if (match_arg(arg, 'A', "all", 3)) {
			if (!gg_pubdir50_get(req, 0, GG_PUBDIR50_START))
				gg_pubdir50_add(req, GG_PUBDIR50_START, "0");
			all = 1;
			match = 1;
		}
	}

	if (!match) {
		printq("invalid_params", name);
		return -1;
	}

search:
	if (!gg_pubdir50(sess, req)) {
		printq("search_failed", http_error_string(0));
		res = -1;
	}

	if (all)
		list_add(&searches, req, 0);
	else
		gg_pubdir50_free(req);

	array_free(argv);

	return res;
}

COMMAND(cmd_change)
{
	struct gg_change_info_request *r;
	struct gg_http *h;
	char **argv = NULL;
	int i, match = 0;

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	argv = array_make(params[0], " \t", 0, 1, 1);

	r = xcalloc(1, sizeof(*r));
	
	for (i = 0; argv[i]; i++) {
		
		if (match_arg(argv[i], 'f', "first", 2) && argv[i + 1]) {
			xfree(r->first_name);
			r->first_name = xstrdup(argv[++i]);
			match = 1;
		}
		
		if (match_arg(argv[i], 'l', "last", 2) && argv[i + 1]) {
			xfree(r->last_name);
			r->last_name = xstrdup(argv[++i]);
			match = 1;
		}
		
		if (match_arg(argv[i], 'n', "nickname", 2) && argv[i + 1]) {
			xfree(r->nickname);
			r->nickname = xstrdup(argv[++i]);
			match = 1;
		}
		
		if (match_arg(argv[i], 'e', "email", 2) && argv[i + 1]) {
			xfree(r->email);
			r->email = xstrdup(argv[++i]);
			match = 1;
		}

		if (match_arg(argv[i], 'c', "city", 2) && argv[i + 1]) {
			xfree(r->city);
			r->city = xstrdup(argv[++i]);
			match = 1;
		}
		
		if (match_arg(argv[i], 'b', "born", 2) && argv[i + 1]) {
			r->born = atoi(argv[++i]);
			match = 1;
		}
		
		if (match_arg(argv[i], 'F', "female", 2)) {
			r->gender = GG_GENDER_FEMALE;
			match = 1;
		}

		if (match_arg(argv[i], 'M', "male", 2)) {
			r->gender = GG_GENDER_MALE;
			match = 1;
		}
	}

	if (!match && strcmp(params[0], "-")) {
		printq("invalid_params", name);
		return -1;
	}

	if (!r->first_name)
		r->first_name = xstrdup("");

	if (!r->last_name)
		r->last_name = xstrdup("");

	if (!r->nickname)
		r->nickname = xstrdup("");

	if (!r->email)
		r->email = xstrdup("");

	if (!r->city)
		r->city = xstrdup("");

	iso_to_cp(r->first_name);
	iso_to_cp(r->last_name);
	iso_to_cp(r->nickname);
	iso_to_cp(r->email);
	iso_to_cp(r->city);

	if ((h = gg_change_info(config_uin, config_password, r, 1)))
		list_add(&watches, h, 0);

	gg_change_info_request_free(r);
	array_free(argv);

	return 0;
}

COMMAND(cmd_modify)
{
	struct userlist *u;
	char **argv = NULL;
	int i, res = 0, modified = 0;

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!(u = userlist_find(get_uin(params[0]), NULL))) {
		printq("user_not_found", params[0]);
		return -1;
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
			list_t l;

			i++;

			for (l = userlist; l; l = l->next) {
				struct userlist *u = l->data;

				if (u->display && !strcasecmp(u->display, argv[i])) {
					printq("user_exists", u->display);
					array_free(argv);
					return -1;
				}
			}
			
			ui_event("userlist_changed", u->display, argv[i], NULL);
			xfree(u->display);
			u->display = xstrdup(argv[i]);
			userlist_replace(u);
			modified = 1;
		}
		
		if (match_arg(argv[i], 'g', "group", 2) && argv[i + 1]) {
			char **tmp = array_make(argv[++i], ",", 0, 1, 1);
			int x, off;	/* je�li zaczyna si� od '@', pomijamy pierwszy znak */
			
			for (x = 0; tmp[x]; x++)
				switch (*tmp[x]) {
					case '-':
						off = (tmp[x][1] == '@' && strlen(tmp[x]) > 1) ? 1 : 0;

						if (group_member(u, tmp[x] + 1 + off)) {
							group_remove(u, tmp[x] + 1 + off);
							modified = 1;
						} else {
							printq("group_member_not_yet", format_user(u->uin), tmp[x] + 1);
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
							printq("group_member_already", format_user(u->uin), tmp[x] + 1);
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
							printq("group_member_already", format_user(u->uin), tmp[x]);
							if (!modified)
								modified = -1;
						}
				}

			array_free(tmp);
		}
		
		if (match_arg(argv[i], 'u', "uin", 2) && argv[i + 1]) {
			uin_t new_uin = str_to_uin(argv[++i]);
			list_t l;

			if (!new_uin) {
				printq("invalid_uin");
				array_free(argv);
				return -1;
			}

			for (l = userlist; l; l = l->next) {
				struct userlist *_u = l->data;
				
				if (new_uin == _u->uin) {
					if (_u->display) {
						printq("user_exists_other", argv[i], format_user(_u->uin));
						array_free(argv);
						return -1;
					} else {
						/* zmieniamy komu� uin, a taki uin jest ju� blokowany/ignorowany
					 	   ale nie jest widoczny na naszej li�cie */
						int level;

						if (group_member(_u, "__blocked"))
							blocked_add(u->uin);

						if (group_member(_u, "__offline"))
							group_add(u, "__offline");

						if ((level = ignored_check(_u->uin)))
							ignored_add(u->uin, level);

						userlist_remove(_u);
						break;
					}
				}
			}

			gg_remove_notify(sess, u->uin);
			userlist_clear_status(u->uin);

			u->uin = new_uin;

			gg_add_notify_ex(sess, u->uin, userlist_type(u));

			ui_event("userlist_changed", u->display, u->display, NULL);
			modified = 1;
		}

		if (match_arg(argv[i], 'o', "offline", 2)) {
			gg_remove_notify_ex(sess, u->uin, userlist_type(u));
			group_add(u, "__offline");
			printq("modify_offline", format_user(u->uin));
			modified = 2;
			gg_add_notify_ex(sess, u->uin, userlist_type(u));
		}

		if (match_arg(argv[i], 'O', "online", 2)) {
			gg_remove_notify_ex(sess, u->uin, userlist_type(u));
			group_remove(u, "__offline");
			printq("modify_online", format_user(u->uin));
			modified = 2;
			gg_add_notify_ex(sess, u->uin, userlist_type(u));
		}

	}

	if (strcasecmp(name, "add")) {
		switch (modified) {
			case 0:
				printq("not_enough_params", name);
				res = -1;
				break;
			case 1:
				printq("modify_done", params[0]);
			case 2:
				config_changed = 1;
				break;
		}
	} else
		config_changed = 1;

	array_free(argv);

	return res;
}

COMMAND(cmd_help)
{
	list_t l;
	
	if (params[0]) {
		const char *p = (params[0][0] == '/' && strlen(params[0]) > 1) ? params[0] + 1 : params[0];

		if (!strcasecmp(p, "set") && params[1]) {
			if (!quiet)
				variable_help(params[1]);
			return 0;
		}
			
		for (l = commands; l; l = l->next) {
			struct command *c = l->data;
			
			if (!strcasecmp(c->name, p) && c->alias) {
				printq("help_alias", p);
				return -1;
			}

			if (!strcasecmp(c->name, p) && !c->alias) {
			    	char *tmp = NULL;

				if (strstr(c->brief_help, "%"))
				    	tmp = format_string(c->brief_help);
				
				printq("help", c->name, c->params_help, tmp ? tmp : c->brief_help, "");
				xfree(tmp);

				if (c->long_help && strcmp(c->long_help, "")) {
					char *foo, *tmp, *bar = format_string(c->long_help);

					foo = bar;

					while ((tmp = gg_get_line(&foo)))
						printq("help_more", tmp);

					xfree(bar);
				}

				return 0;
			}
		}
	}

	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (isalnum(*c->name) && !c->alias) {
		    	char *blah = NULL;

			if (strstr(c->brief_help, "%"))
			    	blah = format_string(c->brief_help);
	
			printq("help", c->name, c->params_help, blah ? blah : c->brief_help, (c->long_help && strcmp(c->long_help, "")) ? "\033[1m *\033[0m" : "");
			xfree(blah);
		}
	}

	printq("help_footer");
	printq("help_quick");

	return 0;
}

COMMAND(cmd_ignore)
{
	char *tmp;
	uin_t uin = 0;

	if (*name == 'i' || *name == 'I') {
		if (!params[0]) {
			list_t l;
			int i = 0;

			for (l = userlist; l; l = l->next) {
				struct userlist *u = l->data;

				if (!ignored_check(u->uin))
					continue;

				i = 1;

				printq("ignored_list", format_user(u->uin));
			}

			if (!i)
				printq("ignored_list_empty");

			return 0;
		}

		if (params[0][0] == '#') {
			int res;
			
			tmp = saprintf("%sconference --ignore %s", ((quiet) ? "^" : ""), params[0]);
			res = command_exec(NULL, tmp);
			xfree(tmp);
			return res;
		}

		if (!(uin = get_uin(params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}
		
		if (!ignored_add(uin, IGNORE_ALL)) {
			printq("ignored_added", params[0]);
			userlist_clear_status(uin);
			config_changed = 1;
		}

	} else {
		int unignore_all = ((params[0] && !strcmp(params[0], "*")) ? 1 : 0);

		if (!params[0]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[0][0] == '#') {
			int res;
			
			tmp = saprintf("%sconference --unignore %s", ((quiet) ? "^" : ""), params[0]);
			res = command_exec(NULL, tmp);
			xfree(tmp);
			return res;
		}
		
		if (!unignore_all && !(uin = get_uin(params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}

		if (unignore_all) {
			list_t l;
			int x = 0;

			for (l = userlist; l; ) {
				struct userlist *u = l->data;

				l = l->next;

				if (!ignored_remove(u->uin))
					x = 1;

				if (sess) {
					gg_remove_notify(sess, uin);
					gg_add_notify(sess, uin);
				}

				if (uin == config_uin)
					update_status();
			}

			if (x) {
				printq("ignored_deleted_all");
				config_changed = 1;
			} else {
				printq("ignored_list_empty");
				return -1;
			}
			
			return 0;
		}
		
		if (!ignored_remove(uin)) {
			printq("ignored_deleted", format_user(uin));

			if (sess) {
				gg_remove_notify(sess, uin);
				gg_add_notify(sess, uin);
			}

			if (uin == config_uin)
				update_status();

			config_changed = 1;
		} else {
			printq("error_not_ignored", format_user(uin));
			return -1;
		}
	
	}

	return 0;
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

				printq("blocked_list", format_user(u->uin));
			}

			if (!i) 
				printq("blocked_list_empty");

			return 0;
		}

		if (!(uin = get_uin(params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}
		
		blocked_add(uin);
		printq("blocked_added", params[0]);
		config_changed = 1;
	} else {
		int unblock_all = ((params[0] && !strcmp(params[0], "*")) ? 1 : 0);

		if (!params[0]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!unblock_all && !(uin = get_uin(params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}

		if (unblock_all) {
			list_t l;
			int x = 0;

			for (l = userlist; l; ) {
				struct userlist *u = l->data;
			
				l = l->next;
	
				if (!blocked_remove(u->uin))
					x = 1;
			}

			if (x) {
				printq("blocked_deleted_all");
				config_changed = 1;
			} else {
				printq("blocked_list_empty");
				return -1;
			}

			return 0;
		}
		
		if (!blocked_remove(uin)) {
			printq("blocked_deleted", format_user(uin));
			config_changed = 1;
		} else {
			printq("error_not_blocked", format_user(uin));
			return -1;
		}
	
	}

	return 0;
}

COMMAND(cmd_list)
{
	list_t l;
	int count = 0, show_all = 1, show_busy = 0, show_active = 0, show_inactive = 0, show_invisible = 0, show_descr = 0, show_blocked = 0, show_offline = 0, j;
	char **argv = NULL, *show_group = NULL, *ip_str;
	const char *tmp;

	if (params[0] && *params[0] != '-') {
		char *status, *groups;
		struct userlist *u;

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
			
			if (count) {
				printq("group_members", params[0], members->str);
			} else {
				printq("group_empty", params[0]);
			}

			string_free(members, 1);

			return 0;
		}

		if (!(u = userlist_find(get_uin(params[0]), NULL)) || (u && !u->display)) {
			printq("user_not_found", params[0]);
			return -1;
		}

		/* list <alias> [opcje] */
		if (params[1])
			return cmd_modify("list", params, NULL, quiet);

		status = format_string(format_find(ekg_status_label(u->status, "user_info_")), (u->first_name) ? u->first_name : u->display, u->descr);

		groups = group_to_string(u->groups, 0, 1);

		ip_str = saprintf("%s:%s", inet_ntoa(u->ip), itoa(u->port));

		printq("user_info_header", u->display, itoa(u->uin));
		if (u->nickname && strcmp(u->nickname, u->display)) 
			printq("user_info_nickname", u->nickname);

		if (u->first_name && strcmp(u->first_name, "") && u->last_name && u->last_name && strcmp(u->last_name, ""))
			printq("user_info_name", u->first_name, u->last_name);
		if (u->first_name && strcmp(u->first_name, "") && (!u->last_name || !strcmp(u->last_name, "")))
			printq("user_info_name", u->first_name, "");
		if ((!u->first_name || !strcmp(u->first_name, "")) && u->last_name && strcmp(u->last_name, ""))
			printq("user_info_name", u->last_name, "");

		printq("user_info_status", status);
		if (u->ip.s_addr)
			printq("user_info_ip", ip_str);
		if (u->mobile && strcmp(u->mobile, ""))
			printq("user_info_mobile", u->mobile);
		if (strcmp(groups, ""))
			printq("user_info_groups", groups);
		printq("user_info_footer", u->display, itoa(u->uin));
		
#if 0
		printq("user_info", (u->nickname) ? u->nickname : u->display, itoa(u->uin), status, ip_str, u->first_name, u->last_name, u->display, u->mobile, groups);
#endif
		
		xfree(ip_str);
		xfree(groups);
		xfree(status);

		return 0;
	}

	/* list --get */
	if (params[0] && (match_arg(params[0], 'g', "get", 2) || match_arg(params[0], 'G', "get-config", 5))) {
		struct gg_http *h;
		
		if (!(h = gg_userlist_get(config_uin, config_password, 1))) {
			printq("userlist_get_error", strerror(errno));
			return -1;
		}

		if (match_arg(params[0], 'G', "get-config", 5))
			h->user_data = (char*) 1;
		
		list_add(&watches, h, 0);
		
		return 0;
	}

	/* list --clear */
	if (params[0] && (match_arg(params[0], 'c', "clear", 2) || match_arg(params[0], 'C', "clear-config", 8))) {
		struct gg_http *h;
		char *contacts = NULL;
		
		if (match_arg(params[0], 'c', "clear", 2)) {
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

			xfree(vars);
			xfree(contacts);
			
			contacts = string_free(s, 0);
		} else
			contacts = xstrdup("");
			
		if (!contacts) {
			printq("userlist_clear_error", strerror(ENOMEM));
			return -1;
		}
		
		if (!(h = gg_userlist_put(config_uin, config_password, contacts, 1))) {
			printq("userlist_clear_error", strerror(errno));
			return -1;
		}

		if (match_arg(params[0], 'c', "clear", 2))
			h->user_data = (char *) 2;	/* --clear */
		else
			h->user_data = (char *) 3;	/* --clear-config */
		
		xfree(contacts);
		
		list_add(&watches, h, 0);

		return 0;
	}
	
	/* list --put */
	if (params[0] && (match_arg(params[0], 'p', "put", 2) || match_arg(params[0], 'P', "put-config", 5))) {
		struct gg_http *h;
		char *contacts = userlist_dump();

		if (contacts)
			iso_to_cp(contacts);

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

			xfree(vars);
			xfree(contacts);
			
			contacts = string_free(s, 0);
		}
		
		if (!contacts) {
			printq("userlist_put_error", strerror(ENOMEM));
			return -1;
		}
		
		if (!(h = gg_userlist_put(config_uin, config_password, contacts, 1))) {
			printq("userlist_put_error", strerror(errno));
			return -1;
		}

		if (match_arg(params[0], 'P', "put-config", 5))
			h->user_data = (char*) 1;
		
		xfree(contacts);
		
		list_add(&watches, h, 0);

		return -1;
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
		int show;

		if (!u->display)
			continue;

		tmp = ekg_status_label(u->status, "list_");

		if (u->uin == config_uin && sess && sess->state == GG_STATE_CONNECTED && !ignored_check(config_uin))
			tmp = ekg_status_label(config_status, "list_");

		show = show_all;

		if (show_busy && GG_S_B(u->status))
			show = 1;

		if (show_active && GG_S_A(u->status))
			show = 1;

		if (show_inactive && GG_S_NA(u->status))
			show = 1;

		if (show_invisible && GG_S_I(u->status))
			show = 1;

		if (show_blocked && GG_S_BL(u->status))
			show = 1;

		if (show_descr && !GG_S_D(u->status))
			show = 0;

		if (show_group && !group_member(u, show_group))
			show = 0;

		if (show_offline && group_member(u, "__offline"))
			show = 1;

		if (show) {
			printq(tmp, format_user(u->uin), (u->first_name) ? u->first_name : u->display, inet_ntoa(u->ip), itoa(u->port), u->descr);
			count++;
		}
	}

	if (!count && !(show_descr || show_group) && show_all)
		printq("list_empty");

	return 0;
}

COMMAND(cmd_msg)
{
	struct userlist *u;
	char **nicks = NULL, *nick = NULL, **p = NULL, *add_send = NULL;
	unsigned char *msg = NULL, *raw_msg = NULL, *escaped = NULL, *format = NULL;
	uin_t uin;
	int count, valid = 0, chat = (!strcasecmp(name, "chat")), secure = 0, msg_seq, formatlen = 0;


	if (!params[0] || !params[1]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (config_auto_back == 1 && GG_S_B(config_status) && in_auto_away)
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
			
			add_send = xstrdup(c->name);
		}

	} else if (*nick == '#') {
		struct conference *c = conference_find(nick);
		list_t l;

		if (!c) {
			printq("conferences_noexist", nick);
			xfree(nick);
			return -1;
		}

		for (l = c->recipients; l; l = l->next)
			array_add(&nicks, xstrdup(itoa(*((uin_t *) (l->data)))));

		add_send = xstrdup(c->name);
	} else {
		char **tmp = array_make(nick, ",", 0, 0, 0);
		int i;

		/* XXX nie s� wykrywane duplikaty */
		
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
				printq("group_empty", tmp[i] + 1);
		}

		array_free(tmp);
	}

	if (!nicks) {
		xfree(nick);
		return 0;
	}

	msg = xstrdup(params[1]);

	/* analiz� tekstu zrobimy w osobnym bloku dla porz�dku */
	{
		unsigned char attr = 0, last_attr = 0;
		const unsigned char *p = params[1];
		int msglen = 0, i;

		for (i = 0; i < strlen(params[1]); i++, p++) {
			unsigned char rgb[3];

			if (*p == 18) {
				if (!(attr & GG_FONT_COLOR)) {
					int num = atoi(p + 1);
					
					if (num < 0 || num > 15)
						num = 0;

					p++;

					if (isdigit(*p)) {
						p++;
						i++;
					}

					if (isdigit(*p)) {
						p++;
						i++;
					}

					rgb[0] = default_color_map[num].r;
					rgb[1] = default_color_map[num].g;
					rgb[2] = default_color_map[num].b;
				}

				attr ^= GG_FONT_COLOR;
			}

			if (*p == 2)
				attr ^= GG_FONT_BOLD;
			if (*p == 20)
				attr ^= GG_FONT_ITALIC;
			if (*p == 31)
				attr ^= GG_FONT_UNDERLINE;

			if (*p >= 32 || *p == 13 || *p == 10 || *p == 9) {
				if (attr != last_attr) {
					int color = 0;

					if (!format) {
						format = xmalloc(3);
						format[0] = 2;
						formatlen = 3;
					}

					if ((attr & GG_FONT_COLOR))
						color = 1;

					if ((last_attr & GG_FONT_COLOR) && !(attr & GG_FONT_COLOR)) {
						color = 1;
						memset(rgb, 0, 3);
					}

					format = xrealloc(format, formatlen + ((color) ? 6 : 3));
					format[formatlen] = (msglen & 255);
					format[formatlen + 1] = ((msglen >> 8) & 255);
					format[formatlen + 2] = attr | ((color) ? GG_FONT_COLOR : 0);

					if (color) {
						memcpy(format + formatlen + 3, rgb, 3);
						formatlen += 6;
					} else
						formatlen += 3;

					last_attr = attr;
				}

				msg[msglen++] = *p;
			}
		}

		msg[msglen] = 0;

		if (format && formatlen) {
			format[1] = (formatlen - 3) & 255;
			format[2] = ((formatlen - 3) >> 8) & 255;
		}
	}

	raw_msg = xstrdup(msg);
	escaped = log_escape(msg);
	iso_to_cp(msg);

	count = array_count(nicks);

#ifdef HAVE_OPENSSL
	if (config_encryption && array_count(nicks) == 1 && (uin = get_uin(nicks[0])) && (msg_encrypt(uin, &msg) > 0))
		secure = 1;
#endif

	for (p = nicks; *p; p++) {
		if (!strcmp(*p, ""))
			continue;

		if (!(uin = get_uin(*p))) {
			printq("user_not_found", *p);
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

	if (!add_send)
		add_send = xstrdup(nick);

	if (valid)
		add_send_nick(add_send);

	xfree(add_send);

	if (valid && (!sess || sess->state != GG_STATE_CONNECTED))
		printq("not_connected_msg_queued");

	if (valid && config_display_sent) {
		struct gg_event e;
		struct userlist u;
		
		memset(&e, 0, sizeof(e));
		e.type = GG_EVENT_MSG;
		e.event.msg.sender = config_uin;
		e.event.msg.message = xstrdup(raw_msg);
		e.event.msg.time = time(NULL);
		e.event.msg.formats = format + 3;
		e.event.msg.formats_length = formatlen - 3;

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

	return 0;
}

COMMAND(cmd_save)
{
	last_save = time(NULL);

	if (!userlist_write(NULL) && !config_write(params[0])) {
		printq("saved");
		config_changed = 0;
	} else {
		printq("error_saving");
		return -1;
	}

	return 0;
}

COMMAND(cmd_set)
{
	const char *arg = NULL, *val = NULL;
	int unset = 0, show_all = 0, res = 0;
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
					printq("variable", v->name, "(...)");
					displayed = 1;
					continue;
				}

				if (v->type == VAR_STR) {
					char *tmp = (string) ? saprintf("\"%s\"", string) : "(none)";

					printq("variable", v->name, tmp);
					
					if (string)
						xfree(tmp);
				}

				if (v->type == VAR_BOOL)
					printq("variable", v->name, (value) ? "1 (on)" : "0 (off)");
				
				if ((v->type == VAR_INT || v->type == VAR_MAP) && !v->map)
					printq("variable", v->name, itoa(value));

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

					printq("variable", v->name, tmp);

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

					printq("variable", v->name, s->str);

					string_free(s, 1);
				}

				displayed = 1;
			}
		}

		if (!displayed && params[0]) {
			printq("variable_not_found", params[0]);
			return -1;
		}
	} else {
		theme_cache_reset();
		switch (variable_set(arg, (unset) ? NULL : value, 0)) {
			case 0:
			{
				const char *my_params[2] = { (!unset) ? params[0] : params[0] + 1, NULL };

				cmd_set("set-show", my_params, NULL, quiet);
				config_changed = 1;
				last_save = time(NULL);
				break;
			}
			case -1:
				printq("variable_not_found", arg);
				res = -1;
				break;
			case -2:
				printq("variable_invalid", arg);
				res = -1;
				break;
		}
	}

	xfree(value);

	return res;
}

COMMAND(cmd_sms)
{
	struct userlist *u;
	const char *number = NULL;

	if (!params[0] || !params[1] || !config_sms_app) {
		printq("not_enough_params", name);
		return -1;
	}

	if ((u = userlist_find(get_uin(params[0]), NULL))) {
		if (!u->mobile || !strcmp(u->mobile, "")) {
			printq("sms_unknown", format_user(u->uin));
			return -1;
		}
		number = u->mobile;
	} else
		number = params[0];

	if (send_sms(number, params[1], 1) == -1) {
		printq("sms_error", strerror(errno));
		return -1;
	}

	return 0;
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

			printq("quit_descr", r1, r2);

			xfree(r1);
			xfree(r2);
		} else
			printq("quit");

		quit_message_send = 1;
	}

	ekg_logoff(sess, tmp);

	if (tmp) {
		xfree(config_reason);
		config_reason = tmp;
		tmp = NULL;
	}
		
	ui_event("disconnected", NULL);

	/* nie wychodzimy tutaj, �eby command_exec() mia�o szans� zwolni�
	 * u�ywan� przez siebie pami��. */
	quit_command = 1;

	return 0;
}

COMMAND(cmd_dcc)
{
	struct transfer t;
	list_t l;
	uin_t uin;

	if (!params[0] || !strncasecmp(params[0], "li", 2)) {	/* list */
		int pending = 0, active = 0;

		if (params[0] && params[1] && params[1][0] == 'd') {	/* list debug */
			for (l = transfers; l; l = l->next) {
				struct transfer *t = l->data;
				
				printq("dcc_show_debug", itoa(t->id), (t->type == GG_SESSION_DCC_SEND) ? "SEND" : "GET", t->filename, format_user(t->uin), (t->dcc) ? "yes" : "no", (t->dcc) ? itoa(t->dcc->offset) : "0", (t->dcc) ? itoa(t->dcc->file_info.size) : "0", (t->dcc) ? itoa(100*t->dcc->offset/t->dcc->file_info.size) : "0");
			}

			return 0;
		}

		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (!t->dcc || !t->dcc->established) {
				if (!pending) {
					printq("dcc_show_pending_header");
					pending = 1;
				}
				switch (t->type) {
					case GG_SESSION_DCC_SEND:
						printq("dcc_show_pending_send", itoa(t->id), format_user(t->uin), t->filename);
						break;
					case GG_SESSION_DCC_GET:
						printq("dcc_show_pending_get", itoa(t->id), format_user(t->uin), t->filename);
						break;
					case GG_SESSION_DCC_VOICE:
						printq("dcc_show_pending_voice", itoa(t->id), format_user(t->uin));
				}
			}
		}

		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (t->dcc && t->dcc->established) {
				if (!active) {
					printq("dcc_show_active_header");
					active = 1;
				}
				switch (t->type) {
					case GG_SESSION_DCC_SEND:
						printq("dcc_show_active_send", itoa(t->id), format_user(t->uin), t->filename);
						break;
					case GG_SESSION_DCC_GET:
						printq("dcc_show_active_get", itoa(t->id), format_user(t->uin), t->filename);
						break;
					case GG_SESSION_DCC_VOICE:
						printq("dcc_show_active_voice", itoa(t->id), format_user(t->uin));
				}
			}
		}

		if (!active && !pending)
			printq("dcc_show_empty");
		
		return 0;
	}
	
	if (!strncasecmp(params[0], "se", 2)) {		/* send */
		struct userlist *u;
		struct stat st;
		int fd;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		uin = get_uin(params[1]);	

		if (!(u = userlist_find(uin, params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (!sess || sess->state != GG_STATE_CONNECTED) {
			printq("not_connected");
			return -1;
		}

		if (!(GG_S_A(u->status) || GG_S_B(u->status)) && !(ignored_check(uin) & IGNORE_STATUS)) {
			printq("dcc_user_not_avail", format_user(u->uin));
			return -1;
		}

		if ((fd = open(params[2], O_RDONLY)) == -1 || stat(params[2], &st)) {
			printq("dcc_open_error", params[2], strerror(errno));
			return -1;
		} else {
			close(fd);
			if (S_ISDIR(st.st_mode)) {
				printq("dcc_open_directory", params[2]);
				return -1;
			}
		}

		t.uin = uin;
		t.id = transfer_id();
		t.type = GG_SESSION_DCC_SEND;
		t.filename = xstrdup(params[2]);
		t.dcc = NULL;

		if (u->port < 10 || (params[3] && !strcmp(params[3], "--reverse"))) {
			/* nie mo�emy si� z nim po��czy�, wi�c on spr�buje */
			gg_dcc_request(sess, uin);
		} else {
			struct gg_dcc *d;
			
			if (!(d = gg_dcc_send_file(u->ip.s_addr, u->port, config_uin, uin))) {
				printq("dcc_error", strerror(errno));
				return -1;
			}

			if (gg_dcc_fill_file_info(d, params[2]) == -1) {
				printq("dcc_open_error", params[2], strerror(errno));
				gg_free_dcc(d);
				return -1;
			}

			list_add(&watches, d, 0);

			t.dcc = d;
		}

		list_add(&transfers, &t, sizeof(t));

		return 0;
	}

	if (params[0][0] == 'v') {			/* voice */
#ifdef HAVE_VOIP
		struct userlist *u;
		struct transfer *t, tt;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}
		
		/* sprawdzamy najpierw przychodz�ce po��czenia */
		
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
			return 0;
		}

		/* sprawd�, czy ju� nie wo�ano o rozmow� g�osow� */

#if 0
		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (t->type == GG_SESSION_DCC_VOICE) {
				printq("dcc_voice_running");
				return 0;
			}
		}

		for (l = watches; l; l = l->next) {
			struct gg_session *s = l->data;

			if (s->type == GG_SESSION_DCC_VOICE) {
				printq("dcc_voice_running");
				return 0;
			}
		}
#endif
		/* je�li nie by�o, to pr�bujemy sami zainicjowa� */

		uin = get_uin(params[1]);

		if (!(u = userlist_find(uin, params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (!sess || sess->state != GG_STATE_CONNECTED) {
			printq("not_connected");
			return -1;
		}

		memset(&tt, 0, sizeof(tt));
		tt.uin = uin;
		tt.id = transfer_id();
		tt.type = GG_SESSION_DCC_VOICE;

		if (u->port < 10 || (params[2] && !strcmp(params[2], "--reverse"))) {
			/* nie mo�emy si� z nim po��czy�, wi�c on spr�buje */
			gg_dcc_request(sess, uin);
		} else {
			struct gg_dcc *d;
			
			if (!(d = gg_dcc_voice_chat(u->ip.s_addr, u->port, config_uin, uin))) {
				printq("dcc_error", strerror(errno));
				return -1;
			}

			list_add(&watches, d, 0);

			tt.dcc = d;
		}

		list_add(&transfers, &tt, sizeof(tt));
		voice_open();
#else
		printq("dcc_voice_unsupported");
#endif
		return -1;
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
			printq("dcc_get_not_found", (params[1]) ? params[1] : "");
			return -1;
		}

		if (config_dcc_dir) 
		    	path = saprintf("%s/%s", config_dcc_dir, t->filename);
		else
		    	path = xstrdup(t->filename);
		
		/* XXX wi�cej sprawdzania */
		if ((t->dcc->file_fd = open(path, O_WRONLY | O_CREAT, 0600)) == -1) {
			printq("dcc_get_cant_create", path);
			gg_free_dcc(t->dcc);
			list_remove(&transfers, t, 1);
			xfree(path);
			
			return -1;
		}
		
		xfree(path);
		
		printq("dcc_get_getting", format_user(t->uin), t->filename);
		
		list_add(&watches, t->dcc, 0);

		return 0;
	}
	
	if (!strncasecmp(params[0], "c", 1)) {		/* close */
		struct transfer *t;
		uin_t uin;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}
		
		for (t = NULL, l = transfers; l; l = l->next) {
			t = l->data;

			if (params[1][0] == '#' && atoi(params[1] + 1) == t->id)
				break;
		}

		if (!t) {
			printq("dcc_close_notfound");
			return -1;
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

		printq("dcc_close", format_user(uin));
		
		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_version) 
{
	char buf[10];

	snprintf(buf, sizeof(buf), "0x%.2x", GG_DEFAULT_PROTOCOL_VERSION);
    	printq("ekg_version", VERSION, buf, GG_DEFAULT_CLIENT_VERSION, compile_time());

	return 0;
}

#ifdef HAVE_OPENSSL
COMMAND(cmd_key)
{
	if (match_arg(params[0], 'g', "generate", 2)) {
		char *tmp, *tmp2;
		struct stat st;

		if (mkdir(prepare_path("keys", 1), 0700) && errno != EEXIST) {
			printq("key_generating_error", strerror(errno));
			return -1;
		}

		tmp = saprintf("%s/%d.pem", prepare_path("keys", 0), config_uin);
		tmp2 = saprintf("%s/private.pem", prepare_path("keys", 0));

		if (!stat(tmp, &st) && !stat(tmp2, &st)) {
			printq("key_private_exist");
			xfree(tmp);
			xfree(tmp2);
			return -1;
		} 

		xfree(tmp);
		xfree(tmp2);

		printq("key_generating");

		if (sim_key_generate(config_uin)) {
			printq("key_generating_error", "sim_key_generate()");
			return -1;
		}

		printq("key_generating_success");

		return 0;
	}

	if (match_arg(params[0], 's', "send", 2)) {
		string_t s = string_init(NULL);
		char *tmp, buf[128];
		uin_t uin;
		FILE *f;
		
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(uin = get_uin(params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (!sess || sess->state != GG_STATE_CONNECTED) {
			printq("not_connected");
			return -1;
		}

		tmp = saprintf("%s/%d.pem", prepare_path("keys", 0), config_uin);
		f = fopen(tmp, "r");
		xfree(tmp);

		if (!f) {
			printq("key_public_not_found", format_user(config_uin));
			return -1;
		}

		while (fgets(buf, sizeof(buf), f))
			string_append(s, buf);

		fclose(f);

		if (gg_send_message(sess, GG_CLASS_MSG, uin, s->str) == -1) {
			printq("key_send_error");
			string_free(s, 1);
			return -1;
		}
		
		printq("key_send_success", format_user(uin));
		string_free(s, 1);

		return 0;
	}

 	if (match_arg(params[0], 'd', "delete", 2)) {
		char *tmp;
		uin_t uin;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(uin = get_uin(params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (uin == config_uin) {
			char *tmp = saprintf("%s/private.pem", prepare_path("keys", 0));
			unlink(tmp);
			xfree(tmp);
		}

		tmp = saprintf("%s/%d.pem", prepare_path("keys", 0), uin);
		
		if (unlink(tmp)) {
			printq("key_public_not_found", format_user(uin));
		} else {
			printq("key_public_deleted", format_user(uin));
		}
		
		xfree(tmp);

		SIM_KC_Free(SIM_KC_Find(uin));

		return 0;
	}

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		DIR *dir;
		struct dirent *d;
		int count = 0, list_uin = 0;
		const char *path = prepare_path("keys", 0);
		const char *x = NULL;

		if (!(dir = opendir(path))) {
			printq("key_public_noexist");
			return 0;
		}

		if (params[0] && params[0][0] != '-')
			x = params[0];
		else if (params[0] && match_arg(params[0], 'l', "list", 2))
			x = params[1];

		if (x && !(list_uin = get_uin(x))) {
			printq("user_not_found", x);
			closedir(dir);
			return -1;
		}
		
		while ((d = readdir(dir))) {
			struct stat st;
			char *name = saprintf("%s/%s", path, d->d_name);
			struct tm *tm;
			const char *tmp;

			if ((tmp = strstr(d->d_name, ".pem")) && !tmp[4] && !stat(name, &st) && S_ISREG(st.st_mode)) {
				int uin = atoi(d->d_name);

				if (list_uin && uin != list_uin)
					continue;

				if (uin) {
					char *fp = sim_key_fingerprint(uin);
					char ts[100];

					tm = localtime(&st.st_mtime);
					strftime(ts, sizeof(ts), format_find("key_list_timestamp"), tm);

					print("key_list", format_user(uin), (fp) ? fp : "", ts);
					count++;

					xfree(fp);
				}
			}

			xfree(name);
		}

		closedir(dir);

		if (!count)
			printq("key_public_noexist");

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}
#endif

COMMAND(cmd_test_segv)
{
	char *foo = NULL;

	*foo = 'A';

	return 0;
}

COMMAND(cmd_test_ping)
{
	if (sess)
		gg_ping(sess);

	return 0;
}

COMMAND(cmd_test_send)
{
	struct gg_event *e = xmalloc(sizeof(struct gg_event));

	if (!params[0] || !params[1])
		return -1;

	memset(e, 0, sizeof(*e));
	e->type = GG_EVENT_MSG;
	e->event.msg.sender = get_uin(params[0]);
	e->event.msg.message = xstrdup(params[1]);
	e->event.msg.msgclass = GG_CLASS_MSG;
	e->event.msg.time = time(NULL);
	
	handle_msg(e);
	event_free(e);

	return 0;
}

COMMAND(cmd_test_add)
{
	if (params[0])
		add_send_nick(params[0]);

	return 0;
}

COMMAND(cmd_test_del)
{
	if (params[0])
		remove_send_nick(params[0]);

	return 0;
}

COMMAND(cmd_test_debug)
{
	if (params[0])
		gg_debug(GG_DEBUG_MISC, "%s\n", params[0]);

	return 0;
}

COMMAND(cmd_test_debug_dump)
{
	char *tmp = saprintf("Zapisa�em debug do pliku debug.%d", getpid());

	debug_write_crash();
	printq("generic", tmp);
	xfree(tmp);

	return 0;
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
		printq("generic", buf);
	}

	return 0;
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

		printq("generic", buf);
	}
#endif
	return 0;
}

COMMAND(cmd_beep)
{
	ui_beep();

	return 0;
}

#ifdef WITH_IOCTLD

COMMAND(cmd_beeps_spk)
{
	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	return ((ioctld_send(params[0], ACT_BEEPS_SPK, quiet) == -1) ? -1 : 0);
}

COMMAND(cmd_blink_leds)
{
	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	return ((ioctld_send(params[0], ACT_BLINK_LEDS, quiet) == -1) ? -1 : 0);
}

#endif

COMMAND(cmd_play)
{
	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	return play_sound(params[0]);
}

COMMAND(cmd_register)
{
	struct gg_http *h;
	list_t l;

	if (name[0] == 'r') {
		char *passwd;

		if (registered_today) {
			printq("registered_today");
			return -1;
		}
	
		if (!params[0] || !params[1]) {
			printq("not_enough_params", name);
			return -1;
		}
	
		for (l = watches; l; l = l->next) {
			struct gg_common *s = l->data;

			if (s->type == GG_SESSION_REGISTER) {
				printq("register_pending");
				return -1;
			}
		}

		passwd = xstrdup(params[1]);
		iso_to_cp(passwd);
	
		if (!(h = gg_register(params[0], passwd, 1))) {
			xfree(passwd);
			printq("register_failed", strerror(errno));
			return -1;
		}

		list_add(&watches, h, 0);

		reg_password = passwd;
		reg_email = xstrdup(params[0]);
	} else {
		uin_t uin = 0;
		const char *pwd = NULL;
		
		if (!params[0] || (!params[1] && !config_password) || (!params[2] && !config_uin)) {
			printq("not_enough_params", name);
			return -1;
		} 
		
		pwd = (params[1] ? params[1] : config_password);
		uin = ((params[1] && params[2]) ? get_uin(params[2]) : config_uin);

		if (uin <= 0) {
			printq("unregister_bad_uin", uin);
			return -1;
		}

		if (!(h = gg_unregister(uin, pwd, params[0], 1))) {
			printq("unregister_failed", strerror(errno));
			return -1;
		}

		list_add(&watches, h, 0);
	}

	return 0;
}

COMMAND(cmd_reload)
{
	const char *filename = NULL;
	int res = 0;

	if (params[0])
		filename = params[0];

	if ((res = config_read(filename, NULL)))
		printq("error_reading_config", strerror(errno));

	if (res != -1) {
		printq("config_read_success", (res != -2 && filename) ? filename : prepare_path("config", 0));
		config_changed = 0;
		update_status();
	}

	return res;
}

COMMAND(cmd_passwd)
{
	struct gg_http *h;
	char *oldpasswd, *newpasswd;
	
	if (!params[0] || (!params[1] && !config_email)) {
		printq("not_enough_params", name);
		return -1;
	}

	oldpasswd = xstrdup(config_password);
	if (oldpasswd && !config_password_cp1250)
		iso_to_cp(oldpasswd);
	newpasswd = xstrdup(params[0]);
	iso_to_cp(newpasswd);

	if (!(h = gg_change_passwd2(config_uin, (oldpasswd) ? oldpasswd : "", newpasswd, (config_email) ? config_email : "", (params[1]) ? params[1] : config_email, 1))) {
		xfree(newpasswd);
		xfree(oldpasswd);
		printq("passwd_failed", strerror(errno));
		return -1;
	}

	list_add(&watches, h, 0);

	reg_password = newpasswd;
	reg_email = xstrdup((params[1]) ? params[1] : config_email);

	return 0;
}

COMMAND(cmd_remind)
{
	struct gg_http *h;
	
	if (!(h = gg_remind_passwd(config_uin, 1))) {
		printq("remind_failed", strerror(errno));
		return -1;
	}

	list_add(&watches, h, 0); 

	return 0;
}

COMMAND(cmd_query)
{
	char **p = xcalloc(3, sizeof(char*));
	int i, res = 0;

	for (i = 0; params[i]; i++)
		p[i] = xstrdup(params[i]);

	p[i] = NULL;

	if (params[0] && (params[0][0] == '@' || strchr(params[0], ','))) {
		struct conference *c = conference_create(params[0]);
		
		if (!c) {
			res = -1;
			goto cleanup;
		}

		ui_event("command", quiet, "query", c->name, NULL);

		xfree(p[0]);
		p[0] = xstrdup(c->name);

	} else {
		if (params[0] && params[0][0] == '#') {
			struct conference *c = conference_find(params[0]);

			if (!c) {
				printq("conferences_noexist", params[0]);
				res = -1;
				goto cleanup;
			}
		
			ui_event("command", quiet, "query", c->name, NULL);

			xfree(p[0]);
			p[0] = xstrdup(c->name);

		} else {

			if (params[0] && !get_uin(params[0])) {
				printq("user_not_found", params[0]);
				res = -1;
				goto cleanup;
			} else
				ui_event("command", quiet, "query", params[0], NULL);
		}
	}
		
	if (params[0] && params[1])
		cmd_msg("chat", (const char **) p, NULL, quiet);

cleanup:
	for (i = 0; p[i]; i++)
		xfree(p[i]);

	xfree(p);

	return res;
}

COMMAND(cmd_on)
{
	if (match_arg(params[0], 'a', "add", 2)) {
		int flags, res;

		if (!params[1] || !params[2] || !params[3]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(flags = event_flags(params[1]))) {
			printq("events_incorrect");
			return -1;
		}

		if (!get_uin(params[2]) && strcmp(params[2], "*") && params[2][0] != '@') {
			printq("user_not_found", params[2]);
			return -1;
		}

		if (!(res = event_add(flags, params[2], params[3], quiet)))
			config_changed = 1;

		return res;
	}

	if (match_arg(params[0], 'd', "del", 2)) {

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!event_remove(params[1], quiet)) {
			config_changed = 1;
			return 0;
		} else
			return -1;
	}

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		list_t l;
                int count = 0;
		const char *ename = NULL;

		if (params[0] && params[0][0] != '-')
			ename = params[0];
		else if (params[0] && match_arg(params[0], 'l', "list", 2))
			ename = params[1];

		for (l = events; l; l = l->next) {
			struct event *ev = l->data;

			if (ename && strcasecmp(ev->name, ename))
				continue;

			printq((ev->flags & INACTIVE_EVENT) ? "events_list_inactive" : "events_list", event_format(abs(ev->flags)), event_format_target(ev->target), ev->action, ev->name);
			count++;
		}

		if (!count)
			printq("events_list_empty");

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_echo)
{
	printq("generic", (params && params[0]) ? params[0] : "");

	return 0;
}

COMMAND(cmd_window)
{
	ui_event("command", quiet, "window", (params) ? params[0] : NULL, (params && params[0]) ? params[1] : NULL, NULL);
	
	return 0;
}

COMMAND(cmd_bind)
{
	ui_event("command", quiet, "bind", (params) ? params[0] : NULL, (params && params[0]) ? params[1] : NULL, (params && params[1]) ? params[2] : NULL, NULL); 

	return 0;
}

COMMAND(cmd_test_vars)
{
	char *tmp = variable_digest();
	printq("generic", tmp);
	xfree(tmp);

	return 0;
}

COMMAND(cmd_test_ctcp)
{
	if (!sess || !params[0])
		return -1;

	gg_dcc_request(sess, get_uin(params[0]));

	return 0;
}

/*
 * command_exec()
 * 
 * wykonuje polecenie zawarte w linii tekstu. 
 *
 *  - target - w kt�rym oknie nast�pi�o (NULL je�li to nie query)
 *  - xline - linia tekstu.
 *
 * 0/-1.
 */
int command_exec(const char *target, const char *xline)
{
	char *cmd = NULL, *tmp, *p = NULL, short_cmd[2] = ".", *last_name = NULL, *last_params = NULL, *line_save = NULL, *line = NULL;
	command_func_t *last_abbr = NULL;
	int abbrs = 0;
	int correct_command = 0;
	list_t l;

	int quiet = 0;	/* XXX p�ki nie stanie si� argumentem funkcji */

	if (!xline)
		return 0;

	if (!strcmp(xline, "")) {
		if (batch_mode && !batch_line) {
			quit_message_send = 1;
			ekg_exit();
		}
		return 0;
	}

	if (target && *xline != '/') {
	
		/* wykrywanie przypadkowo wpisanych polece� */
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
				cmd_msg("chat", params, NULL, quiet);

			return 0;
		}
	}
	
	send_nicks_index = 0;

	line = line_save = xstrdup(xline);
	line = strip_spaces(line);
	
	if (*line == '/')
		line++;

	if (*line == '^') {
		quiet = 1;
		line++;
	}

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
			last_params = (c->alias) ? "?" : c->params;
			abbrs = 1;		
			break;
		}
		if (!strncasecmp(c->name, cmd, strlen(cmd))) {
			abbrs++;
			last_abbr = c->function;
			last_name = c->name;
			last_params = (c->alias) ? "?" : c->params;
		} else {
			if (last_abbr && abbrs == 1)
				break;
		}
	}

	if (last_abbr && abbrs == 1) {
		char **par;
		int res, len = strlen(last_params);

		par = array_make(p, " \t", len, 1, 1);
		res = (last_abbr)(last_name, (const char**) par, target, quiet);
		array_free(par);

		xfree(line_save);

		if (quit_command)
			ekg_exit();

		return res;
	}

	if (strcmp(cmd, ""))
		printq("unknown_command", cmd);

	xfree(line_save);

	return -1;
}

int binding_help(int a, int b)  
{
	print("help_quick");  

	return 0;  
}

/*
 * binding_quick_list()
 *
 * wy�wietla kr�tk� i zwi�z�a list� dost�pnych, zaj�tych i niewidocznych
 * ludzi z listy kontakt�w.
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

	ui_event("variable_changed", "contacts", NULL);
	config_changed = 1;
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
		char *tmp = saprintf("%s%s%s%s%s", ((*((char *) m->data) == '/') ? "" : "/"), ((quiet) ? "^" : ""), (char *) m->data, ((params[0]) ? " " : ""), ((params[0]) ? params[0] : ""));
		m = m->next;
		command_exec(target, tmp);
		xfree(tmp);
	}

	return 0;
}

COMMAND(cmd_timer)
{
	int count = 0, res = 0;
	int at = !strcasecmp(name, "at");
	list_t l;

	if (match_arg(params[0], 'a', "add", 2)) {
		const char *p = params[1];
		char *t_command = NULL, *t_name = NULL;
		time_t period = 0;
		struct timer *t;
		int persistent = 0;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (isdigit(params[2][0]) || !strncmp(params[2], "*/", 2)) {
			t_name = xstrdup(p);

			if (!strcmp(t_name, "(null)")) {
				printq("invalid_params", name);
				res = -1;
				goto cleanup;
			}

			for (l = timers; l; l = l->next) {
				t = l->data;

				if (t->name && !strcasecmp(t->name, t_name)) {
					printq((at) ? "at_exist" : "timer_exist", t_name);
					res = -1;
					goto cleanup;
				}
			}

			p = params[2];
			t_command = xstrdup(params[3]);
		} else
			t_command = array_join((char **) params + 2, " ");

		if (at) {
			struct tm *lt;
			time_t now = time(NULL);
			char *tmp, *foo = xstrdup(p);
			int wrong = 0;

			lt = localtime(&now);
			lt->tm_isdst = -1;

			/* wyci�gamy sekundy, je�li s� i obcinamy */
			if ((tmp = strchr(foo, '.')) && !(wrong = (strlen(tmp) != 3))) {
				sscanf(tmp + 1, "%2d", &lt->tm_sec);
				tmp[0] = 0;
			} else
				lt->tm_sec = 0;

			/* pozb�d�my si� dwukropka */
			if ((tmp = strchr(foo, ':')) && !(wrong = (strlen(tmp) != 3))) {
				tmp[0] = tmp[1];
				tmp[1] = tmp[2];
				tmp[2] = 0;
			}

			/* jedziemy ... */
			if (!wrong)
				switch (strlen(foo)) {
					case 12:
						sscanf(foo, "%4d%2d%2d%2d%2d", &lt->tm_year, &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						lt->tm_year -= 1900;
						lt->tm_mon -= 1;
						break;
					case 10:
						sscanf(foo, "%2d%2d%2d%2d%2d", &lt->tm_year, &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						lt->tm_year += 100;
						lt->tm_mon -= 1;
						break;
					case 8:
						sscanf(foo, "%2d%2d%2d%2d", &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						lt->tm_mon -= 1;
						break;
					case 6:
						sscanf(foo, "%2d%2d%2d", &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						break;	
					case 4:
						sscanf(foo, "%2d%2d", &lt->tm_hour, &lt->tm_min);
						break;
					default:
						wrong = 1;
				}
	
			xfree(foo);

			/* nie ma b��d�w ? */
			if (wrong || lt->tm_hour > 23 || lt->tm_min > 59 || lt->tm_sec > 59 || lt->tm_mday > 31 || lt->tm_mon > 12) {
				printq("invalid_params", name);
				res = -1;
				goto cleanup;
			}

			/* plany na przesz�o��? */
			if ((period = mktime(lt) - now) <= 0) {
				printq("at_back_to_past");
				res = -1;
				goto cleanup;
			}

		} else {

			if (!strncmp(p, "*/", 2)) {
				p += 2;
				persistent = 1;
			}

			for (;;) {
				time_t _period = 0;

				if (isdigit(*p))
					_period = atoi(p);
				else {
					printq("invalid_params", name);
					res = -1;
					goto cleanup;
				}

				p += strlen(itoa(_period));

				if (strlen(p)) {
					switch (tolower(*p++)) {
						case 'd':
							_period *= 86400;
							break;
						case 'h':
							_period *= 3600;
							break;
						case 'm':
							_period *= 60;
							break;
						case 's':
							break;
						default:
							printq("invalid_params", name);
							res = -1;
							goto cleanup;
					}
				}

				period += _period;
				
				if (!*p)
					break;
			}
		}

		if ((t = timer_add(period, persistent, TIMER_COMMAND, at, t_name, t_command))) {
			printq((at) ? "at_added" : "timer_added", t->name);
			if (!in_autoexec)
				config_changed = 1;
		}

cleanup:
		xfree(t_name);
		xfree(t_command);

		return res;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		int del_all = 0;
		int ret = 1;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!strcmp(params[1], "*")) {
			del_all = 1;
			ret = timer_remove_user(at);
		} else
			ret = timer_remove(params[1], at, NULL);
		
		if (!ret) {
			if (del_all) {
				printq((at) ? "at_deleted_all" : "timer_deleted_all");
			} else {
				printq((at) ? "at_deleted" : "timer_deleted", params[1]);
			}
			
			config_changed = 1;
		} else {
			if (del_all) {
				printq((at) ? "at_empty" : "timer_empty");
			} else {
				printq((at) ? "at_noexist" : "timer_noexist", params[1]);
				res = -1;
			}
		}

		return res;
	}

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		const char *tname = NULL;

		if (params[0] && params[0][0] != '-')
			tname = params[0];
		else if (params[0] && match_arg(params[0], 'l', "list", 2))
			tname = params[1];

		for (l = timers; l; l = l->next) {
			struct timer *t = l->data;
			struct timeval tv;
			char *tmp;
			const char *type_str;
			int usec, sec, minutes = 0, hours = 0, days = 0;

			if (at != t->at)
				continue;

			if (tname && strcasecmp(tname, t->name))
				continue;

			count++;

			gettimeofday(&tv, NULL);

			if (at) {
				struct tm *foo = localtime((time_t *) &t->ends.tv_sec);

				tmp = xmalloc(100);
				strftime(tmp, 100, format_find("at_timestamp"), foo);

			} else {

				if (t->ends.tv_usec < tv.tv_usec) {
					sec = t->ends.tv_sec - tv.tv_sec - 1;
					usec = (t->ends.tv_usec - tv.tv_usec + 1000000) / 1000;
				} else {
					sec = t->ends.tv_sec - tv.tv_sec;
					usec = (t->ends.tv_usec - tv.tv_usec) / 1000;
				}

				if (sec > 86400) {
					days = sec / 86400;
					sec -= days * 86400;
				}

				if (sec > 3600) {
					hours = sec / 3600;
					sec -= hours * 3600;
				}
			
				if (sec > 60) {
					minutes = sec / 60;
					sec -= minutes * 60;
				}

				if (days)
					tmp = saprintf("%dd %dh %dm %d.%.3d", days, hours, minutes, sec, usec);
				else
					if (hours)
						tmp = saprintf("%dh %dm %d.%.3d", hours, minutes, sec, usec);
					else
						if (minutes)
							tmp = saprintf("%dm %d.%.3d", minutes, sec, usec);
						else
							tmp = saprintf("%d.%.3d", sec, usec);
			}

			switch (t->type) {
				case TIMER_UI:
					type_str = "ui";
					break;
				case TIMER_SCRIPT:
					type_str = "script";
					break;
				case TIMER_COMMAND:
					type_str = "command";
					break;
				default:
					type_str = "unknown";
			}
			
			if (at) {
				printq("at_list", t->name, tmp, t->command, type_str);
			} else {
				printq("timer_list", t->name, tmp, t->command, type_str, (t->persistent) ? "*" : "");
			}

			xfree(tmp);

		}

		if (!count) {
			if (tname) {
				printq((at) ? "at_noexist" : "timer_noexist", tname);
				res = -1;
			} else {
				printq((at) ? "at_empty" : "timer_empty");
			}
		}

		return res;
	}

	printq("invalid_params", name);

	return -1;
}

#ifdef WITH_PYTHON

COMMAND(cmd_python)
{
	if (!params[0]) {
		python_list();
		return 0;
	}

	if (!strncasecmp(params[0], "lo", 2)) {
		python_load(params[1]);
		return 0;
	}

	if (!strncasecmp(params[0], "u", 1)) {
		python_unload(params[1]);
		return 0;
	}

	if (!strncasecmp(params[0], "r", 1)) {
		python_run(params[1]);
		return 0;
	}

	if (!strncasecmp(params[0], "e", 1)) {
		python_exec(params[1]);
		return 0;
	}

	if (!strncasecmp(params[0], "li", 2)) {
		python_list();
		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

#endif

COMMAND(cmd_conference) 
{
	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] == '#') {
		list_t l, r;
		int count = 0;
		const char *cname = NULL;
	
		if (params[0] && params[0][0] == '#')
			cname = params[0];
		else if (params[0] && match_arg(params[0], 'l', "list", 2))
			cname = params[1];

		for (l = conferences; l; l = l->next) {
			struct conference *c = l->data;
			string_t recipients;
			const char *recipient;
			int first = 0;

			recipients = string_init(NULL);

			if (cname && strcasecmp(cname, c->name))
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

		if (!count) {
			if (params[0] && params[0][0] == '#') {
				printq("conferences_noexist", params[0]);
				return -1;
			} else {
				printq("conferences_list_empty");
			}
		}

		return 0;
	}

	if (match_arg(params[0], 'a', "add", 2)) {
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[2]) {
			if (params[1][0] != '#') {
				printq("conferences_name_error");
				return -1;
			} else
				conference_add(params[1], params[2], quiet);
		} else
			conference_create(params[1]);

		return 0;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!strcmp(params[1], "*"))
			conference_remove(NULL, quiet);
		else {
			if (params[1][0] != '#') {
				printq("conferences_name_error");
				return -1;
			}

			conference_remove(params[1], quiet);
		}

		return 0;
	}

	if (match_arg(params[0], 'r', "rename", 2)) {
		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#' || params[2][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		conference_rename(params[1], params[2], quiet);

		return 0;
	}
	
	if (match_arg(params[0], 'i', "ignore", 2)) {
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		conference_set_ignore(params[1], 1, quiet);

		return 0;
	}

	if (match_arg(params[0], 'u', "unignore", 2)) {
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		conference_set_ignore(params[1], 0, quiet);

		return 0;
	}

	if (match_arg(params[0], 'f', "find", 2)) {
		struct conference *c;
		char *tmp = NULL;
		list_t l;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		c = conference_find(params[1]);

		if (c) {
			for (l = c->recipients; l; l = l->next) {
				tmp = saprintf("%sfind --uin %d", ((quiet) ? "^" : ""), *((uin_t *) (l->data)));
				command_exec(NULL, tmp);
				xfree(tmp);
			}
		} else {
			printq("conferences_noexist", params[1]);
			return -1;
		}


		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_last)
{
        list_t l;
	uin_t uin = 0;
	int show_sent = 0;

	if (match_arg(params[0], 'c', "clear", 2)) {
		if (params[1] && !(uin = get_uin(params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if ((uin && !last_count(uin)) || !list_count(lasts)) {
			if (uin) {
				printq("last_list_empty_nick", format_user(uin));
			} else {
				printq("last_list_empty");
			}
			return -1;
		}

		if (uin) {
			last_del(uin);	
			printq("last_clear_uin", format_user(uin));
		} else {
			last_free();
			printq("last_clear");
		}

		return 0;
	}		

	if (match_arg(params[0], 's', "stime", 2)) {
		show_sent = 1;

		if (params[1] && !(uin = get_uin(params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}
	} else
		if (params[0] && !(uin = get_uin(params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}
		
	if (!((uin > 0) ? last_count(uin) : list_count(lasts))) {
		if (uin) {
			printq("last_list_empty_nick", format_user(uin));
			return -1;
		}

		printq("last_list_empty");
		return 0;
	}

        for (l = lasts; l; l = l->next) {
                struct last *ll = l->data;
		struct tm *tm, *st;
		char buf[100], buf2[100], *time_str = NULL;

		if (uin == 0 || uin == ll->uin) {
			tm = localtime(&ll->time);
			strftime(buf, sizeof(buf), format_find("last_list_timestamp"), tm);

			if (show_sent && ll->type == 0 && !(ll->sent_time - config_time_deviation <= ll->time && ll->time <= ll->sent_time + config_time_deviation)) {
				st = localtime(&ll->sent_time);
				strftime(buf2, sizeof(buf2), format_find((ll->sent_time / 86400 == time(NULL) / 86400) ? "last_list_timestamp_today" : "last_list_timestamp"), st);
				time_str = saprintf("%s/%s", buf, buf2);
			} else
				time_str = xstrdup(buf);

			if (config_last & 4 && ll->type == 1) {
				printq("last_list_out", time_str, format_user(ll->uin), ll->message);
			} else {
				printq("last_list_in", time_str, format_user(ll->uin), ll->message);
			}

			xfree(time_str);
		}
        }

	return 0;
}

COMMAND(cmd_queue)
{
	list_t l;
	uin_t uin = 0;

	if (strcasecmp(name, "_queue") && sess && sess->state == GG_STATE_CONNECTED) {
		printq("queue_wrong_use");
		return -1;
	}

	if (match_arg(params[0], 'c', "clear", 2)) {
		if (params[1] && !(uin = get_uin(params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if ((uin && !msg_queue_count_uin(uin)) || !msg_queue_count()) {
			if (uin) {
				printq("queue_empty_uin", format_user(uin));
			} else {
				printq("queue_empty");
			}
			return 0;
		}

		if (uin) {
			msg_queue_remove_uin(uin);
			printq("queue_clear_uin", format_user(uin));
		} else {
			msg_queue_free();
			printq("queue_clear");
		}

		return 0;
	}		

	if (params[0] && !(uin = get_uin(params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	}  
		
	if ((uin && !msg_queue_count_uin(uin)) || !msg_queue_count()) {
		if (uin) {
			printq("queue_empty_uin", format_user(uin));
		} else {
			printq("queue_empty");
		}
		return 0;
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
				
				fu = string_free(s, 0);
			} else
				fu = xstrdup(format_user(*(m->uins)));
				
			printq("queue_list_message", buf, fu, m->msg);

			xfree(fu);
		}
	}

	return 0;
}

/*
 * command_add_compare()
 *
 * funkcja por�wnuj�ca nazwy komend, by wyst�powa�y alfabetycznie w li�cie.
 *
 *  - data1, data2 - dwa wpisy komend do por�wnania.
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
 * dodaje komend�. 
 *
 *  - name - nazwa komendy,
 *  - params - definicja parametr�w (szczeg�y poni�ej),
 *  - function - funkcja obs�uguj�ca komend�,
 *  - help_params - opis parametr�w,
 *  - help_brief - kr�tki opis komendy,
 *  - help_long - szczeg�owy opis komendy.
 *
 * 0 je�li si� uda�o, -1 je�li b��d.
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
 * usuwa komend� z listy.
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
 * rodzaje parametr�w komend:
 *
 * '?' - olewamy,
 * 'U' - r�cznie wpisany uin, nadawca mesg�w,
 * 'u' - nazwa lub uin z kontakt�w, nazwa konferencji, r�cznie wpisany uin, nadawca mesg�w,
 * 'c' - komenda,
 * 'i' - nicki z listy ignorowanych os�b,
 * 'd' - komenda dcc,
 * 'p' - komenda python,
 * 'w' - komenda window,
 * 'e' - events,
 * 'f' - plik.
 */

/*
 * command_init()
 *
 * inicjuje list� domy�lnych komend.
 */
void command_init()
{
	command_add
	( "add", "U??", cmd_add, 0,
	  " [numer] [alias] [opcje]", "dodaje u�ytkownika do listy kontakt�w",
	  "\n"
	  "  -f, --find [alias]  Dodanie do listy kontakt�w u�ytkownika ostatnio\n"
	  "                      znalezionego w katalogu publicznym. Nale�y poda�\n"
	  "                      alias, je�li nie znaleziono pseudonimu.\n"
	  "\n"
	  "Pozosta�e opcje identyczne jak dla polecenia %Tlist%n (dotycz�ce "
	  "wpisu). W oknie rozmowy z kim� spoza naszej listy kontakt�w jako "
	  "parametr mo�na poda� sam alias.\n");
	  
	command_add
	( "alias", "??", cmd_alias, 0,
	  " [opcje]", "zarz�dzanie aliasami",
	  "\n"
	  "  -a, --add <alias> <komenda>     dodaje alias\n"
          "  -A, --append <alias> <komenda>  dodaje komend� do aliasu\n"
	  "  -d, --del <alias>|*             usuwa alias lub wszystkie\n"
	  " [-l, --list] [alias]             wy�wietla list� alias�w\n");
	  
	command_add
	( "away", "?", cmd_away, 0,
	  " [pow�d/-]", "zmienia stan na zaj�ty",
	  "\n"
	  "Je�li w��czona jest odpowiednia opcja %Trandom_reason%n i nie "
          "podano powodu, zostanie wylosowany z pliku %Taway.reasons%n. "
 	  "Podanie %T-%n zamiast powodu spowoduje wyczyszczenie bez "
	  "wzgl�du na ustawienia zmiennych.");

	command_add
	( "at", "???c", cmd_timer, 0,
	  " [opcje]", "planuje wykonanie komend",
	  "\n"
	  "  -a, --add [nazwa] <czas> <komenda>  tworzy nowy plan\n"
	  "  -d, --del <nazwa>|*                 usuwa plan lub wszystkie\n"
	  " [-l, --list] [nazwa]                 wy�wietla list� plan�w\n"
	  "\n"
	  "Czas podaje si� w formacie [[[yyyy]mm]dd]HH[:]MM[.SS], gdzie "
	  "%Tyyyy%n to rok, %Tmm%n to miesi�c, %Tdd%n to dzie�, %THH:MM%n "
	  "to godzina, a %T.SS%n to sekundy. "
	  "Minimalny format to %THH:MM%n (dwukropek mo�na pomin��). "
	  "Po kropce mo�na poda� sekundy, a przed godzin� odpowiednio: dzie� "
	  "miesi�ca, miesi�c, rok.");
 
	command_add
	( "back", "?", cmd_away, 0,
	  " [pow�d/-]", "zmienia stan na dost�pny",
	  "\n"
          "Je�li w��czona jest odpowiednia opcja %Trandom_reason%n i nie "
	  "podano powodu, zostanie wylosowany z pliku %Tback.reasons%n. "
	  "Podanie %T-%n zamiast powodu spowoduje wyczyszczenie bez "
	  "wzgl�du na ustawienia zmiennych.");

	command_add
	( "beep", "", cmd_beep, 0,
	  "", "wydaje d�wi�k", "");

#ifdef WITH_IOCTLD
	command_add
	( "beeps_spk", "?", cmd_beeps_spk, 0,
	  " <sekwencja>", "wydaje d�wi�ki zgodnie z sekwencj�",
	  "\n"
	  "Kolejne d�wi�ki oddzielone s� przecinkami. D�wi�k sk�ada si� "
	  "z tonu w hercach i d�ugo�ci trwania w mikrosekundach oddzielonej "
	  "uko�nikiem (%T/%n). Je�li nie podano czasu trwania, domy�lna "
	  "warto�� to 0,1s.\n"
	  "\n"
	  "Zamiast sekwencji mo�na poda� nazw� formatu z themu.");

	command_add
	( "blink_leds", "?", cmd_blink_leds, 0,
	  " <sekwencja>", "odtwarza sekwencj� na diodach LED",
	  "\n"
	  "Kombinacje diod oddzielone s� przecinkami. Je�li po kombinacji "
	  "wyst�puje znak uko�nika (%T/%n), po nim podany jest czas trwania "
	  "w mikrosekundach. Domy�lny czas trwania to 0,1s. Kombinacja jest "
	  "map� bitow� o nast�puj�cych "
	  "warto�ciach: 1 - NumLock, 2 - ScrollLock, 4 - CapsLock. Na "
	  "przyk�ad w��czenie NumLock i CapsLock jednocze�nie to 1+4 czyli "
	  "5.\n"
	  "\n"
	  "Zamiast sekwencji mo�na poda� nazw� formatu z themu.");
#endif
	  
	command_add
	( "block", "u", cmd_block, 0,
	  " [numer/alias]", "dodaje do listy blokowanych",
	  "");

	command_add
	( "bind", "???", cmd_bind, 0,
	  " [opcje]", "przypisywanie akcji klawiszom",
	  "\n"
	  "  -a, --add <sekwencja> <akcja>   przypisuje now� sekwencj�\n"
	  "  -d, --del <sekwencja>           usuwa podan� sekwencj�\n"
	  " [-l, --list] [sekwencja]         wy�wietla przypisane sekwencje\n"
	  "  -L, --list-default [sekwencja]  j.w. plus domy�lne sekwencje\n"
	  "\n"
	  "Dost�pne sekwencje to: Ctrl-<znak>, Alt-<znak>, F<liczba>, Enter, "
	  "Backspace, Delete, Insert, Home, End, Left, Right, Up, Down, "
	  "PageUp, PageDown.\n"
	  "\n"
	  "Dost�pne akcje to: backward-word, forward-word, kill-word, toggle-input, cancel-input, backward-delete-char, beginning-of-line, end-of-line, delete-char, backward-page, forward-page, kill-line, yank, accept-line, line-discard, quoted-insert, word-rubout, backward-char, forward-char, previous-history, next-history, complete, quick-list, toggle-contacts, ignore-query. "
	  "Ka�da inna akcja b�dzie traktowana jako komenda do wykonania.");

	command_add
	( "change", "?", cmd_change, 0,
	  " <opcje>", "zmienia informacje w katalogu publicznym",
	  "\n"
	  "  -f, --first <imi�>\n"
          "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>\n"
	  "  -e, --email <adres>\n"
	  "  -b, --born <rok urodzenia>\n"
	  "  -c, --city <miasto>\n"
	  "  -F, --female                 kobieta\n"
	  "  -M, --male                   m�czyzna\n"
	  "\n"
	  "Je�li kt�ry� z parametr�w nie zostanie podany, jego warto�� "
	  "zostanie wyczyszczona w katalogu publicznym. Podanie parametru "
	   ",,-'' wyczy�ci %Twszystkie%n pola.");
	  
	command_add
	( "chat", "u?", cmd_msg, 0,
	  " <numer/alias/@grupa> <wiadomo��>", "wysy�a wiadomo�� w rozmowie",
	  "\n"
	  "Mo�na poda� wi�ksz� ilo�� odbiorc�w oddzielaj�c ich numery lub "
	  "pseudonimy przecinkiem (ale bez odst�p�w). W takim wypadku "
	  "zostanie rozpocz�ta rozmowa grupowa.");
	  
	command_add
	( "cleartab", "?", cmd_cleartab, 0,
	  " [opcje]", "czy�ci list� nick�w do dope�nienia",
	  "\n"
	  "  -o, --offline  usuwa tylko nieobecnych");
	  
	command_add
	( "connect", "??", cmd_connect, 0,
	  " [[numer] has�o]", "��czy si� z serwerem",
	  "\n"
	  "Je�li podano jeden parametr, jest on traktowany jako has�o, "
	  "a je�li podano dwa, s� to kolejno numer i has�o. Dane te s� "
	  "ustawiane w konfiguracji i zostan� utrwalone po wydaniu komendy "
	  "%Tsave%n.");
	  
	command_add
	( "dcc", "duf?", cmd_dcc, 0,
	  " <komenda> [opcje]", "obs�uga bezpo�rednich po��cze�",
	  "\n"
	  "  send <numer/alias> <�cie�ka>  wysy�a podany plik\n"
	  "  get [numer/alias/#id]         akceptuje przysy�any plik\n"
	  "  voice <numer/alias/#id>       rozpoczyna rozmow� g�osow�\n"
	  "  close <numer/alias/#id>       zamyka po��czenie\n"
	  "  list                          wy�wietla list� po��cze�\n"
	  "\n"
	  "Po��czenia bezpo�rednie wymagaj� w��czonej opcji %Tdcc%n. "
	  "Dok�adny opis znajduje si� w pliku %Tdocs/dcc.txt%n");
	  
	command_add
	( "del", "u", cmd_del, 0,
	  " <numer/alias>|*", "usuwa u�ytkownika z listy kontakt�w",
	  "");
	
	command_add
	( "disconnect", "?", cmd_connect, 0,
	  " [pow�d/-]", "roz��cza si� z serwerem",
	  "\n"
	  "Parametry identyczne jak dla komendy %Tquit%n.\n"
	  "\n"
	  "Je�li w��czona jest opcja %Tauto_reconnect%n, po wywo�aniu "
	  "tej komendy, program nadal b�dzie pr�bowa� si� automatycznie "
	  "��czy� po okre�lonym czasie.");
	  
	command_add
	( "echo", "?", cmd_echo, 0,
	  " <tekst>", "wy�wietla podany tekst",
	  "");
	  
	command_add
	( "exec", "?", cmd_exec, 0,
	  " [opcje] <polecenie>", "uruchamia polecenie systemowe",
	  "\n"
	  "  -m, --msg  [numer/alias] wysy�a wynik do danej osoby\n"
	  "  -b, --bmsg [numer/alias] wysy�a wynik w jednej wiadomo�ci\n"
	  "\n"
	  "Poprzedzenie polecenia znakiem %T^%n ukryje informacj� o "
	  "zako�czeniu. Je�li jako alias podamy %T$%n, wynik b�dzie "
	  "wys�any do rozm�wcy z aktualnego okna. Ze wzgl�du na budow� "
	  "klienta, numery i aliasy %Tnie b�d�%n dope�niane Tabem.");
	  
	command_add
	( "!", "?", cmd_exec, 0,
	  " [opcje] <polecenie>", "synonim dla %Texec%n",
	  "");

	command_add
	( "find", "u", cmd_find, 0,
	  " [opcje]", "przeszukiwanie katalogu publicznego",
	  "\n"
	  "  -u, --uin <numerek>\n"
	  "  -f, --first <imi�>\n"
	  "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>\n"
	  "  -c, --city <miasto>\n"
	  "  -b, --born <min:max>    zakres roku urodzenia\n"
	  "  -a, --active            tylko dost�pni\n"
	  "  -F, --female            kobiety\n"
	  "  -M, --male              m�czy�ni\n"
	  "  -s, --start <n>         wy�wietla od n-tego numeru\n"
	  "  -A, --all               wy�wietla wszystkich\n"
	  "  -S, --stop              zatrzymuje wszystkie poszukiwania");
	  
	command_add
	( "help", "cv", cmd_help, 0,
	  " [polecenie] [zmienna]", "wy�wietla informacj� o poleceniach",
	  "\n"
	  "Mo�liwe jest wy�wietlenie informacji o zmiennych, je�li jako "
	  "polecenie poda si� %Tset%n");
	  
	command_add
	( "?", "cv", cmd_help, 0,
	  " [polecenie] [zmienna]", "synonim dla %Thelp%n",
	  "");
	 
	command_add
	( "ignore", "u", cmd_ignore, 0,
	  " [numer/alias]", "dodaje do listy ignorowanych",
	  "\n"
	  "W przysz�o�ci b�dzie mo�liwe wybranie zdarze� zwi�zanych z dan� "
	  "osob�, takich jak wiadomo�ci, zmiana stanu, zmiana opisu itp.");
	  
	command_add
	( "invisible", "?", cmd_away, 0,
	  " [pow�d/-]", "zmienia stan na niewidoczny",
	  "\n"
          "Je�li w��czona jest odpowiednia opcja %Trandom_reason%n i nie "
	  "podano powodu, zostanie wylosowany z pliku %Tquit.reasons%n. "
	  "Podanie %T-%n zamiast powodu spowoduje wyczyszczenie bez "
	  "wzgl�du na ustawienia zmiennych.");

#ifdef HAVE_OPENSSL
	command_add
	( "key", "uu", cmd_key, 0,
	  " [opcje]", "zarz�dzanie kluczami dla SIM",
	  "\n"
	  "  -g, --generate              generuje par� kluczy u�ytkownika\n"
	  "  -s, --send <numer/alias>    wysy�a nasz klucz publiczny\n"
	  "  -d, --delete <numer/alias>  usuwa klucz publiczny\n"
	  "  [-l, --list] [numer/alias]  wy�wietla posiadane klucze publiczne\n");
#endif

	command_add
	( "last", "uu", cmd_last, 0,
	  " [opcje]", "wy�wietla lub czy�ci ostatnie wiadomo�ci",
	  "\n"
	  "  -s, --stime [numer/alias]  wy�wietla czas wys�ania wiadomo�ci\n"
	  "  -c, --clear [numer/alias]  czy�ci podane wiadomo�ci lub wszystkie\n"
	  "  [numer/alias]              wy�wietla ostatnie wiadomo�ci\n"
	  "\n"
	  "W przypadku opcji %T--stime%n czas wy�wietlany jest "
	  ",,inteligentnie'' zgodnie ze zmienn� %Ttime_deviation.%n");

	command_add
	( "list", "u?", cmd_list, 0,
          " [alias|@grupa|opcje]", "zarz�dzanie list� kontakt�w",
	  "\n"
	  "Wy�wietlanie os�b o podanym stanie \"list [-a|-b|-i|-B|-d|-m|-o]\":\n"
	  "  -a, --active          dost�pne\n"
	  "  -b, --busy            zaj�te\n"
	  "  -i, --inactive        niedost�pne\n"
	  "  -B, --blocked         blokuj�ce nas\n"
	  "  -d, --description     osoby z opisem\n"
	  "  -m, --member <@grupa> osoby nale��ce do danej grupy\n"
	  "  -o, --offline         osoby dla kt�rych jeste�my niedost�pni\n"
	  "\n"
	  "Wy�wietlanie cz�onk�w grupy: \"list @grupa\"\n"
	  "\n"
	  "Zmiana wpis�w listy kontakt�w \"list <alias> <opcje...>\":\n"
	  "  -f, --first <imi�>\n"
	  "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>     pseudonim (nie jest u�ywany)\n"
	  "  -d, --display <nazwa>      wy�wietlana nazwa\n"
	  "  -u, --uin <numerek>\n"
	  "  -g, --group [+/-]<@grupa>  dodaje lub usuwa z grupy\n"
	  "                             mo�na poda� wi�cej grup po przecinku\n"
	  "  -p, --phone <numer>        numer telefonu kom�rkowego\n"
	  "  -o, --offline              b�d� niedost�pny dla danej osoby\n"
	  "  -O, --online               b�d� widoczny dla danej osoby\n"
	  "\n"
	  "Dwie ostatnie opcje dzia�aj� tylko, gdy w��czony jest tryb ,,tylko "
	  "dla znajomych''.\n"
	  "\n"
	  "Lista kontakt�w na serwerze \"list [-p|-g|-c|-P|-G|-C]\":\n"
	  "  -p, --put          umieszcza na serwerze\n"
	  "  -P, --put-config   umieszcza na serwerze razem z konfiguracj�\n"
	  "  -g, --get          pobiera z serwera\n"
	  "  -G, --get-config   pobiera z serwera razem z konfiguracj�\n"
	  "  -c, --clear        usuwa list� z serwera\n"
	  "  -C, --clear-config usuwa list� wraz z konfiguracj� z serwera");
	  
	command_add
	( "msg", "u?", cmd_msg, 0,
	  " <numer/alias/@grupa> <wiadomo��>", "wysy�a wiadomo��",
	  "\n"
	  "Mo�na poda� wi�ksz� ilo�� odbiorc�w oddzielaj�c ich numery lub "
	  "pseudonimy przecinkiem (ale bez odst�p�w).");

	command_add
        ( "on", "?euc", cmd_on, 0,
	  " [opcje]", "zarz�dzanie zdarzeniami",
	  "\n"
	  "  -a, --add <zdarzenie> <numer/alias/@grupa> <komenda>  dodaje zdarzenie\n"
	  "  -d, --del <numer>|*         usuwa zdarzenie o podanym numerze\n"
	  " [-l, --list] [numer]         wy�wietla list� zdarze�\n"
	  "\n"
	  "Dost�pne zdarzenia to:\n"
	  "  - avail, away, notavail - zmiana stanu na podany (bez przypadku ,,online'')\n"
	  "  - online - zmiana stanu z ,,niedost�pny'' na ,,dost�pny''\n"
	  "  - descr - zmiana opisu\n"
	  "  - blocked - zostali�my zablokowani\n"
	  "  - msg, chat - wiadomo��\n"
	  "  - query - nowa rozmowa\n"
	  "  - delivered, queued - wiadomo�� dostarczona lub zakolejkowana na serwerze\n"
	  "  - dcc - otrzymanie dcc\n"
	  "  - sigusr1, sigusr2 - otrzymanie przez ekg danego sygna�u\n"
	  "  - newmail - otrzymanie nowej wiadomo�ci e-mail\n"
	  "\n"
	  "    W przypadku sigusr i newmail nale�y poda� ,,*'' jako sprawc� zdarzenia\n"
	  "\n"
	  "  - * - wszystkie zdarzenia\n"
	  "\n"
	  "Zdarzenia mo�na ��czy� ze sob� za pomoc� przecinka lub ,,|''. Jako numer/alias "
	  "mo�na poda� ,,*'', dzi�ki czemu zdarzenie b�dzie dotyczy� ka�dego u�ytkownika. "
	  "Je�li kto� posiada indywidualn� akcj� na dane zdarzenie, to tylko ona zostanie "
	  "wykonana. Mo�na poda� wi�cej komend, oddzielaj�c je �rednikiem. W komendzie, %T\\%1%n "
	  "zostanie zast�pione numerkiem sprawcy zdarzenia, a je�li istnieje on na naszej "
	  "li�cie kontakt�w, %T\\%2%n b�dzie zast�pione jego pseudonimem. Zamiast %T\\%3%n i "
	  "%T\\%4%n wpisana b�dzie tre�� wiadomo�ci, opis u�ytkownika lub ca�kowita ilo�� "
	  "nowych wiadomo�ci e-mail - w zale�no�ci od zdarzenia. "
	  "Format %T\\%4%n r�ni si� od %T\\%3%n tym, �e wszystkie niebiezpieczne znaki, "
	  "kt�re mog�yby zosta� zinterpretowane przez shell, zostan� poprzedzone backslashem. "
	  "U�ywanie %T\\%3%n w przypadku komendy ,,exec'' jest %Tniebezpieczne%n i, je�li naprawd� "
	  "musisz wykorzysta� tre�� wiadomo�ci lub opis, u�yj %T\"\\%4\"%n (w cudzys�owach).");
	 
	command_add
	( "passwd", "??", cmd_passwd, 0,
	  " <has�o> [e-mail]", "zmienia has�o i adres e-mail u�ytkownika",
	  "\n"
	  "Do zmiany has�a niezb�dne jest ustawienie prawid�owego (podanego "
	  "podczas rejestracji lub p�niej zmienionego) adresu e-mail w "
	  "zmiennej %Temail%n. Bez tego zmiana has�a si� nie powiedzie.");

	command_add
	( "play", "f", cmd_play, 0,
	  " <plik>", "odtwarza plik d�wi�kowy", "");

	command_add
	( "private", "", cmd_away, 0,
	  " [on/off]", "w��cza/wy��cza tryb ,,tylko dla znajomych''",
	  "");

#ifdef WITH_PYTHON
	command_add
	( "python", "p?", cmd_python, 0,
	  " <komenda> [opcje]", "obs�uga skrypt�w",
	  "\n"
	  "  load <skrypt>    �aduje skrypt\n"
	  "  unload <skrypt>  usuwa skrypt z pami�ci\n"
	  "  run <plik>       uruchamia skrypt\n"
	  "  exec <komenda>   uruchamia komend�\n"
	  "  list             wy�wietla list� za�adowanych skrypt�w\n");
#endif

	command_add
	( "query", "u?", cmd_query, 0,
	  " <numer/alias/@grupa> [wiadomo��]", "w��cza rozmow�",
	  "\n"
	  "Mo�na poda� wi�ksz� ilo�� odbiorc�w oddzielaj�c ich numery lub "
	  "pseudonimy przecinkiem (ale bez odst�p�w). W takim wypadku "
          "zostanie rozpocz�ta rozmowa grupowa.");

	command_add
	( "queue", "uu", cmd_queue, 0,
	  " [opcje]", "zarz�dzanie wiadomo�ciami do wys�ania po po��czeniu",
	  "\n"
	  "  -c, --clear [numer/alias]  usuwa podane wiadomo�ci lub wszystkie\n"
	  "  [numer/alias]              wy�wietla kolejk� wiadomo�ci\n"
	  "\n"
	  "Mo�na u�y� tylko wtedy, gdy nie jeste�my po��czeni. W przypadku "
	  "konferencji wy�wietla wszystkich uczestnik�w.");
	  
	command_add
	( "quit", "?", cmd_quit, 0,
	  " [pow�d/-]", "wychodzi z programu",
	  "\n"
          "Je�li w��czona jest odpowiednia opcja %Trandom_reason%n i nie "
	  "podano powodu, zostanie wylosowany z pliku %Tquit.reasons%n. "
	  "Podanie %T-%n zamiast powodu spowoduje wyczyszczenie bez "
	  "wzgl�du na ustawienia zmiennych.");
	  
	command_add
	( "reconnect", "", cmd_connect, 0,
	  "", "roz��cza i ��czy ponownie",
	  "");
	  
	command_add
	( "register", "??", cmd_register, 0,
	  " <email> <has�o>", "rejestruje nowe konto",
	  "");

	command_add
	( "reload", "f", cmd_reload, 0,
	  " [plik]", "wczytuje plik konfiguracyjny u�ytkownika lub podany",
	  "");
	  
	command_add
	( "remind", "", cmd_remind, 0,
	  "", "wysy�a has�o na skrzynk� pocztow�",
	  "");
	  
	command_add
	( "save", "?", cmd_save, 0,
	  " [plik]", "zapisuje ustawienia programu",
	  "\n"
	  "Aktualny stan zostanie zapisany i zostanie przywr�cony przy "
	  "nast�pnym uruchomieniu programu. Mo�na poda� plik, do kt�rego "
	  "ma by� zapisana konfiguracja.");
	  
	command_add
	( "set", "v?", cmd_set, 0,
  	  " [-]<zmienna> [[+/-]warto��]", "wy�wietla lub zmienia ustawienia",
	  "\n"
	  "U�ycie %Tset -zmienna%n czy�ci zawarto�� zmiennej. Dla zmiennych "
	  "b�d�cymi mapami bitowymi mo�na okre�li�, czy warto�� ma by� "
	  "dodana (poprzedzone plusem), usuni�ta (minusem) czy ustawiona "
	  "(bez prefiksu). Warto�� zmiennej mo�na wzi�� w cudzys��w. "
	  "Poprzedzenie opcji parametrem %T-a%n lub %T--all%n spowoduje "
	  "wy�wietlenie wszystkich, nawet aktualnie nieaktywnych zmiennych.");

	command_add
	( "sms", "u?", cmd_sms, 0,
	  " <numer/alias> <tre��>", "wysy�a smsa do podanej osoby",
	  "\n"
	  "Polecenie wymaga zdefiniowana zmiennej %Tsms_send_app%n");

	command_add
	( "status", "", cmd_status, 0,
	  "", "wy�wietla aktualny stan",
	  "");

	command_add
	( "unregister", "???", cmd_register, 0,
	  " <email> [<has�o> [<uin/nick>]]", "usuwa konto z serwera",
	  "");

	command_add
	( "conference", "??u", cmd_conference, 0,
	  " [opcje]", "zarz�dzanie konferencjami",
	  "\n"
	  "  -a, --add [#nazwa] <numer/alias/@grupa>  tworzy now� konferencj�\n"
	  "  -d, --del <#nazwa>|*        usuwa konferencj� lub wszystkie\n"
	  "  -i, --ignore <#nazwa>       oznacza konferencj� jako ingorowan�\n"
	  "  -u, --unignore <#nazwa>     oznacza konferencj� jako nieingorowan�\n"
	  "  -r, --rename <#old> <#new>  zmienia nazw� konferencji\n"
	  "  -f, --find <#nazwa>         wyszukuje uczestnikow w katalogu\n"
	  " [-l, --list] [#nazwa]        wy�wietla list� konferencji\n"
	  "\n"
	  "Dodaje nazw� konferencji i definiuje, kto bierze w niej udzia�. "
	  "Kolejne numery, pseudonimy lub grupy mog� byc odzielone "
	  "przecinkiem lub spacj�.");

	command_add
	( "timer", "???c", cmd_timer, 0,
	  " [opcje]", "zarz�dzanie timerami",
	  "\n"
	  "  -a, --add [nazwa] [*/]<czas> <komenda>  tworzy nowy timer\n"
	  "  -d, --del <nazwa>|*                 zatrzymuje timer lub wszystkie\n"
	  " [-l, --list] [nazwa]                 wy�wietla list� timer�w\n"
	  "\n"
	  "Czas podaje si� w sekundach. Mo�na te� u�y� przyrostk�w %Td%n, %Th%n, %Tm%n, %Ts%n, "
	  "oznaczaj�cych dni, godziny, minuty, sekundy, np. 5h20m. Timer po "
	  "jednorazowym uruchomieniu jest usuwany, chyba �e czas poprzedzimy "
	  "wyra�eniem ,,*/''. Wtedy timer b�dzie uruchamiany w zadanych odst�pach "
	  "czasu, a na li�cie b�dzie oznaczony gwiazdk�.");

	command_add
	( "unignore", "i", cmd_ignore, 0,
	  " <numer/alias>|*", "usuwa z listy ignorowanych os�b",
	  "");
	  
	command_add
	( "unblock", "b", cmd_block, 0,
	  " <numer/alias>|*", "usuwa z listy blokowanych os�b",
	  "");
	  
	command_add
	( "version", "", cmd_version, 0,
	  "", "wy�wietla wersj� programu",
	  "");
	  
	command_add
	( "window", "w?", cmd_window, 0,
	  " <komenda> [numer_okna]", "zarz�dzanie okienkami",
	  "\n"
	  "  new [*opcje]         tworzy nowe okno\n"
	  "  kill [numer_okna]    zamyka aktualne lub podane okno\n"
	  "  next                 prze��cza do nast�pnego okna\n"
	  "  prev                 prze��cza do poprzedniego okna\n"
	  "  switch <numer_okna>  prze��cza do podanego okna\n"
	  "  clear                czy�ci aktualne okno\n"
	  "  refresh              od�wie�a aktualne okno\n"
	  "  list                 wy�wietla list� okien\n"
	  "\n"
	  "Argumenty dla %Tnew%n to %T*x,y,w,h[,f],/komenda%n, gdzie %Tx%n i "
	  "%Ty%n to "
	  "pozycja okna na ekranie, %Tw%n i %Th%n to odpowiednio szeroko�� "
	  "i wysoko�� okna w znakach, a %Tf%n jest map� bitow� okre�laj�c� "
	  "z kt�rej strony wyst�puj� ramki (1 - lewo, 2 - g�ra, 4 - prawo, "
          "8 - d�), a komenda okre�la, jakiej komendy wynik ma by� "
	  "wy�wietlany regularnie w oknie.");


	command_add
	( "_add", "?", cmd_test_add, 0, "",
	  "dodaje do listy dope�niania TABem", "");
	command_add
	( "_del", "?", cmd_test_del, 0, "",
	  "usuwa z listy dope�niania TABem", "");
#if 0
	command_add
	( "_fds", "", cmd_test_fds, 0, "", 
	  "wy�wietla otwarte deskryptory", "");
#endif
	command_add
	( "_msg", "u?", cmd_test_send, 0, "",
	  "udaje, �e wysy�a wiadomo��", "");
	command_add
	( "_ping", "", cmd_test_ping, 0, "", 
	  "wysy�a pakiet ping do serwera", "");
	command_add
	( "_segv", "", cmd_test_segv, 0, "", 
	  "wywo�uje naruszenie segmentacji pami�ci", "");
	command_add
	( "_watches", "", cmd_test_watches, 0, "", 
	  "wy�wietla list� sprawdzanych deskryptor�w", "");
	command_add
	( "_debug", "", cmd_test_debug, 0, "",
	  "wy�wietla tekst w oknie debug", "");
	command_add
	( "_debug_dump", "", cmd_test_debug_dump, 0, "",
	  "zrzuca debug do pliku", "");
	command_add
	( "_vars", "", cmd_test_vars, 0, "",
	  "wy�wietla skr�t zmiennych", "");
	command_add
	( "_queue", "uu", cmd_queue, 0, " [opcje]",
	  "pozwala obserwowa� kolejk� wiadomo�ci podczas po��czenia", "");
	command_add
	( "_ctcp", "u", cmd_test_ctcp, 0, " <numer/alias>",
	  "wysy�a ��danie bezpo�redniego po��czenia", "");
}

/*
 * command_free()
 *
 * usuwa list� komend z pami�ci.
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
