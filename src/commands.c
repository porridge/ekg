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

#include <stdio.h>
#include <unistd.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <readline/readline.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libgadu.h"
#include "stuff.h"
#include "dynstuff.h"
#include "commands.h"
#include "events.h"
#include "themes.h"
#include "vars.h"
#include "userlist.h"

/*
 * g³upie readline z wersji na wersjê ma inne include'y, grr.
 */
extern void rl_extend_line_buffer(int len);
extern char **completion_matches();

/*
 * jaka¶ malutka lista tych, do których by³y wysy³ane wiadomo¶ci.
 */

#define SEND_NICKS_MAX 100

char *send_nicks[SEND_NICKS_MAX] = { NULL };
int send_nicks_count = 0, send_nicks_index = 0;

/* 
 * definicje dostêpnych komend.
 */

int command_add(), command_away(), command_del(), command_alias(),
	command_exec(), command_help(), command_ignore(), command_list(),
	command_save(), command_msg(), command_quit(), command_test_send(),
	command_test_add(), command_set(), command_connect(),
	command_sms(), command_find(), command_modify(), command_cleartab(),
	command_status(), command_register(), command_test_watches(),
	command_remind(), command_dcc(), command_query(), command_passwd(),
	command_test_ping(), command_on(), command_change(),
	command_test_fds();

/*
 * drugi parametr definiuje ilo¶æ oraz rodzaje parametrów (tym samym
 * ich dope³nieñ)
 *
 * '?' - olewamy,
 * 'U' - rêcznie wpisany uin, nadawca mesgów,
 * 'u' - nazwa lub uin z kontaktów, rêcznie wpisany uin, nadawca mesgów,
 * 'c' - komenda,
 * 'i' - nicki z listy ignorowanych osób,
 * 'd' - komenda dcc,
 * 'f' - plik.
 */

struct command commands[] = {
	{ "add", "U??", command_add, " <numer> <alias> [opcje]", "Dodaje u¿ytkownika do listy kontaktów", "Opcje identyczne jak dla polecenia %Wmodify%n" },
	{ "alias", "??", command_alias, " [opcje]", "Zarz±dzanie aliasami", "  --add <alias> <komenda>\n  --del <alias>\n  [--list]\n" },
	{ "away", "?", command_away, " [powód]", "Zmienia stan na zajêty", "" },
	{ "back", "", command_away, "", "Zmienia stan na dostêpny", "" },
	{ "change", "?", command_change, " <opcje>", "Zmienia informacje w katalogu publicznym", "  --first <imiê>\n  --last <nazwisko>\n  --nick <pseudonim>\n  --email <adres>\n  --born <rok urodzenia>\n  --city <miasto>\n  --female  \n  --male" },
	{ "chat", "u?", command_msg, " <numer/alias> <wiadomo¶æ>", "Wysy³a wiadomo¶æ w ramach rozmowy", "" },
	{ "cleartab", "", command_cleartab, "", "Czy¶ci listê nicków do dope³nienia", "" },
	{ "connect", "", command_connect, "", "£±czy siê z serwerem", "" },
	{ "dcc", "duf", command_dcc, " [opcje]", "Obs³uga bezpo¶rednich po³±czeñ", "  send <numer/alias> <¶cie¿ka>\n  get [numer/alias]\n  close [numer/alias/id]\n  show\n" },
	{ "del", "u", command_del, " <numer/alias>", "Usuwa u¿ytkownika z listy kontaktów", "" },
	{ "disconnect", "", command_connect, "", "Roz³±cza siê z serwerem", "" },
	{ "exec", "?", command_exec, " <polecenie>", "Uruchamia polecenie systemowe", "" },
	{ "!", "?", command_exec, " <polecenie>", "Synonim dla %Wexec%n", "" },
	{ "find", "u", command_find, " [opcje]", "Interfejs do katalogu publicznego", "  --uin <numerek>\n  --first <imiê>\n  --last <nazwisko>\n  --nick <pseudonim>\n  --city <miasto>\n  --birth <min:max>\n  --phone <telefon>\n  --email <e-mail>\n  --active\n  --female\n  --male\n  --start <od>" },
/*	{ "info", "u", command_find, " <numer/alias>", "Interfejs do katalogu publicznego", "" }, */
	{ "help", "c", command_help, " [polecenie]", "Wy¶wietla informacjê o poleceniach", "" },
	{ "?", "c", command_help, " [polecenie]", "Synonim dla %Whelp%n", "" },
	{ "ignore", "u", command_ignore, " [numer/alias]", "Dodaje do listy ignorowanych lub j± wy¶wietla", "" },
	{ "invisible", "", command_away, "", "Zmienia stan na niewidoczny", "" },
	{ "list", "u?", command_list, " [alias|opcje]", "Zarz±dzanie list± kontaktów", "--active\n  --busy\n  --inactive\n\n  --first <imiê>\n  --last <nazwisko>\n  --nick <pseudonim>  // tylko informacja\n  --display <nazwa>  // wy¶wietlana nazwa\n  --phone <telefon>\n  --uin <numerek>\n  --group [+/-]<grupa>\n\n  --put\n  --get" },
	{ "msg", "u?", command_msg, " <numer/alias> <wiadomo¶æ>", "Wysy³a wiadomo¶æ do podanego u¿ytkownika", "" },
        { "on", "?u?", command_on, " <zdarzenie|...> <numer/alias> <akcja>|clear", "Dodaje lub usuwa zdarzenie", "" },
	{ "passwd", "??", command_passwd, " <has³o> <e-mail>", "Zmienia has³o i adres e-mail u¿ytkownika", "" },
	{ "private", "", command_away, " [on/off]", "W³±cza/wy³±cza tryb ,,tylko dla przyjació³''", "" },
	{ "query", "u", command_query, " <numer/alias>", "W³±cza rozmowê z dan± osob±", "" },
	{ "reconnect", "", command_connect, "", "Roz³±cza i ³±czy ponownie", "" },
	{ "register", "??", command_register, " <email> <has³o>", "Rejestruje nowy uin", "" },
	{ "remind", "", command_remind, "", "Wysy³a has³o na skrzynkê pocztow±", "" },
	{ "save", "", command_save, "", "Zapisuje ustawienia programu", "" },
	{ "set", "v?", command_set, " <zmienna> <warto¶æ>", "Wy¶wietla lub zmienia ustawienia", "" },
	{ "sms", "u?", command_sms, " <numer/alias> <tre¶æ>", "Wysy³a SMSa do podanej osoby", "" },
	{ "status", "", command_status, "", "Wy¶wietla aktualny stan", "" },
	{ "quit", "", command_quit, "", "Wychodzi z programu", "" },
	{ "unignore", "i", command_ignore, " <numer/alias>", "Usuwa z listy ignorowanych osób", "" },
	{ "_msg", "u?", command_test_send, "", "", "" },
	{ "_add", "?", command_test_add, "", "", "" },
	{ "_watches", "", command_test_watches, "", "", "" },
	{ "_ping", "", command_test_ping, "", "", "" },
	{ "_fds", "", command_test_fds, "", "", "" },
	{ NULL, NULL, NULL, NULL, NULL }
};

char *command_generator(char *text, int state)
{
	static int index = 0, len;
	int slash = 0;
	char *name;

	if (*text == '/') {
		slash = 1;
		text++;
	}

	if (!*rl_line_buffer) {
		if (state)
			return NULL;
		if (!send_nicks_count)
			return strdup((query_nick) ? "/msg" : "msg");
		send_nicks_index = (send_nicks_count > 1) ? 1 : 0;
		if (!(name = malloc(6 + strlen(send_nicks[0]))))
			return NULL;
		sprintf(name, (query_nick) ? "/chat %s" : "chat %s", send_nicks[0]);
		
		return name;
	}

	if (!state) {
		index = 0;
		len = strlen(text);
	}

	while ((name = commands[index++].name))
		if (!strncasecmp(text, name, len)) {
			char *tmp = malloc(strlen(name) + 2);
			
			if (!tmp)
				return NULL;
			
			if (query_nick) {
				strcpy(tmp, "/");
				strcat(tmp, name);
			} else
				strcpy(tmp, name);
			
			return tmp;
		}

	return NULL;
}

char *known_uin_generator(char *text, int state)
{
	static struct list *l;
	static int len;

	if (!state) {
		l = userlist;
		len = strlen(text);
	}

	while (l) {
		struct userlist *u = l->data;

		l = l->next;

		if (u->display && !strncasecmp(text, u->display, len))
			return strdup(u->display);
	}

	return NULL;
}

char *unknown_uin_generator(char *text, int state)
{
	static int index = 0, len;

	if (!state) {
		index = 0;
		len = strlen(text);
	}

	while (index < send_nicks_count)
		if (isdigit(send_nicks[index++][0]))
			if (!strncasecmp(text, send_nicks[index - 1], len))
				return strdup(send_nicks[index - 1]);

	return NULL;
}

char *variable_generator(char *text, int state)
{
	static struct list *l;
	static int len;

	if (!state) {
		l = variables;
		len = strlen(text);
	}

	while (l) {
		struct variable *v = l->data;
		
		l = l->next;
		
		if (v->type != VAR_FOREIGN && !strncasecmp(text, v->name, len))
			return strdup(v->name);
	}

	return NULL;
}

char *ignored_uin_generator(char *text, int state)
{
	static struct list *l;
	static int len;
	struct userlist *u;

	if (!state) {
		l = ignored;
		len = strlen(text);
	}

	while (l) {
		struct ignored *i = l->data;

		l = l->next;

		if (!(u = userlist_find(i->uin, NULL))) {
			if (!strncasecmp(text, itoa(i->uin), len))
				return strdup(itoa(i->uin));
		} else {
			if (u->display && !strncasecmp(text, u->display, len))
				return strdup(u->display);
		}
	}

	return NULL;
}

char *dcc_generator(char *text, int state)
{
	char *commands[] = { "close", "get", "send", "show", NULL };
	static int len, i;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while (commands[i]) {
		if (!strncasecmp(text, commands[i], len))
			return strdup(commands[i++]);
		i++;
	}

	return NULL;
}

char *empty_generator(char *text, int state)
{
	return NULL;
}

char **my_completion(char *text, int start, int end)
{
	struct command *c;
	char *params = NULL;
	int word = 0, i, abbrs = 0;
	CPFunction *func = empty_generator;

	if (start) {
		if (!strncasecmp(rl_line_buffer, "chat ", 5)) {
			word = 0;
			for (i = 0; i < strlen(rl_line_buffer); i++) {
				if (isspace(rl_line_buffer[i]))
					word++;
			}
			if (word == 2) {
				char buf[100];
				
				snprintf(buf, sizeof(buf), "chat %s ", send_nicks[send_nicks_index++]);
				rl_extend_line_buffer(strlen(buf));
				strcpy(rl_line_buffer, buf);
				rl_end = strlen(buf);
				rl_point = rl_end;
				rl_redisplay();

				if (send_nicks_index == send_nicks_count)
					send_nicks_index = 0;
					
				return NULL;
			}
			word = 0;
		}
	}

	if (start) {
		for (i = 1; i <= start; i++) {
			if (isspace(rl_line_buffer[i]) && !isspace(rl_line_buffer[i - 1]))
				word++;
		}
		word--;

		for (c = commands; c->name; c++) {
			int len = strlen(c->name);

			if (!strncasecmp(rl_line_buffer, c->name, len) && isspace(rl_line_buffer[len])) {
				params = c->params;
				abbrs = 1;
				break;
			}
			
			for (len = 0; rl_line_buffer[len] && rl_line_buffer[len] != ' '; len++);

			if (!strncasecmp(rl_line_buffer, c->name, len)) {
				params = c->params;
				abbrs++;
			} else
				if (params && abbrs == 1)
					break;
		}

		if (params && abbrs == 1) {
			if (word >= strlen(params))
				func = empty_generator;
			else {
				switch (params[word]) {
					case 'u':
						func = known_uin_generator;
	    					break;
					case 'U':
						func = unknown_uin_generator;
						break;
					case 'c':
						func = command_generator;
						break;
					case 's':	/* XXX */
						func = empty_generator;
						break;
					case 'i':
						func = ignored_uin_generator;
						break;
					case 'v':
						func = variable_generator;
						break;
					case 'd':
						func = dcc_generator;
						break;
					case 'f':
#ifdef HAS_RL_COMPLETION
						func = rl_filename_completion_function;
#endif
						break;
				}
			}
		}
	}

	if (start == 0)
		func = command_generator;

	return completion_matches(text, func);
}

void add_send_nick(char *nick)
{
	int i, count = send_nicks_count, dont_add = 0;

	for (i = 0; i < send_nicks_count; i++)
		if (!strcmp(nick, send_nicks[i])) {
			count = i;
			dont_add = 1;
			break;
		}
	
	if (count == SEND_NICKS_MAX) {
		free(send_nicks[SEND_NICKS_MAX - 1]);
		count--;
	}

	for (i = count; i > 0; i--)
		send_nicks[i] = send_nicks[i - 1];

	if (send_nicks_count != SEND_NICKS_MAX && !dont_add)
		send_nicks_count++;
	
	send_nicks[0] = strdup(nick);	
}

COMMAND(command_cleartab)
{
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		free(send_nicks[i]);
		send_nicks[i] = NULL;
	}
	send_nicks_count = 0;
	send_nicks_index = 0;

	return 0;
}

COMMAND(command_add)
{
	uin_t uin;

	if (!params[0] || !params[1]) {
		my_printf("not_enough_params");
		return 0;
	}

	if (userlist_find(atoi(params[0]), params[1])) {
		my_printf("user_exists", params[1]);
		return 0;
	}

	if (!(uin = atoi(params[0]))) {
		my_printf("invalid_uin");
		return 0;
	}

	if (!userlist_add(uin, params[1])) {
		my_printf("user_added", params[1]);
		gg_add_notify(sess, uin);
		config_changed = 1;
	} else
		my_printf("error_adding");

	if (params[2]) {
		params++;
		command_modify("add", params);
	}

	return 0;
}

COMMAND(command_alias)
{
	char *arg;
	
	if ((arg = params[0]) && *arg == '-' && *(arg + 1) == '-')
		arg++;

	if (!arg || !strncasecmp(arg, "-l", 2)) {
		struct list *l;
		int count = 0;

		for (l = aliases; l; l = l->next) {
			struct alias *a = l->data;

			my_printf("aliases_list", a->alias, a->cmd);
			count++;
		}

		if (!count)
			my_printf("aliases_list_empty");

		return 0;
	}

	if (!strncasecmp(arg, "-a", 2)) {
		if (!add_alias(params[1], 0))
			config_changed = 1;

		return 0;
	}

	if (!strncasecmp(arg, "-d", 2)) {
		if (!del_alias(params[1]))
			config_changed = 1;

		return 0;
	}

	my_printf("aliases_invalid");
	return 0;
}

COMMAND(command_away)
{
	int status_table[4] = { GG_STATUS_AVAIL, GG_STATUS_BUSY, GG_STATUS_INVISIBLE, GG_STATUS_BUSY_DESCR };
	char *reason = NULL;
	
	unidle();
	
	if (!strcasecmp(name, "away")) {
		reason = params[0];
		away = (reason) ? 3 : 1;
		my_printf("away");
	} else if (!strcasecmp(name, "invisible")) {
		away = 2;
		my_printf("invisible");
	} else if (!strcasecmp(name, "back")) {
		away = 0;
		my_printf("back");
	} else {
		int tmp;

		if (!params[0]) {
			my_printf((private_mode) ? "private_mode_is_on" : "private_mode_is_off");
			return 0;
		}
		
		if ((tmp = on_off(params[0])) == -1) {
			my_printf("private_mode_invalid");
			return 0;
		}

		private_mode = tmp;
		my_printf((private_mode) ? "private_mode_on" : "private_mode_off");
	}

	config_status = status_table[away] | ((private_mode) ? GG_STATUS_FRIENDS_MASK : 0);

	if (sess && sess->state == GG_STATE_CONNECTED) {
		if (reason)
			gg_change_status_descr(sess, config_status, reason);
		else
			gg_change_status(sess, config_status);
	}

	return 0;
}

COMMAND(command_status)
{
	char *av, *bs, *na, *in, *pr, *np;

	av = format_string(find_format("show_status_avail"));
	bs = format_string(find_format("show_status_busy"));
	na = format_string(find_format("show_status_not_avail"));
	in = format_string(find_format("show_status_invisible"));
	pr = format_string(find_format("show_status_private_on"));
	np = format_string(find_format("show_status_private_off"));

	if (!sess || sess->state != GG_STATE_CONNECTED) {
		my_printf("show_status", na, "");
	} else {
		char *foo[3] = { av, bs, in };

		my_printf("show_status", foo[away], (private_mode) ? pr : np);
	}

	free(av);
	free(bs);
	free(na);
	free(in);
	free(pr);
	free(np);

	return 0;
}

COMMAND(command_connect)
{
	if (!strcasecmp(name, "connect")) {
		if (sess) {
			my_printf((sess->state == GG_STATE_CONNECTED) ? "already_connected" : "during_connect");
			return 0;
		}
                if (config_uin && config_password) {
			my_printf("connecting");
			connecting = 1;
			prepare_connect();
			if (!(sess = gg_login(config_uin, config_password, 1))) {
	                        my_printf("conn_failed", strerror(errno));
	                        do_reconnect();
	                } else {
				sess->initial_status = config_status;
				list_add(&watches, sess, 0);
			}
		} else
			my_printf("no_config");
	} else if (!strcasecmp(name, "reconnect")) {
		command_connect("disconnect", NULL);
		command_connect("connect", NULL);
	} else if (sess) {
		connecting = 0;
		if (sess->state == GG_STATE_CONNECTED)
			my_printf("disconnected");
		else if (sess->state != GG_STATE_IDLE)
			my_printf("conn_stopped");
		gg_logoff(sess);
		list_remove(&watches, sess, 0);
		gg_free_session(sess);
		userlist_clear_status();
		sess = NULL;
		reconnect_timer = 0;
	}

	return 0;
}

COMMAND(command_del)
{
	struct userlist *u;
	uin_t uin;
	char *tmp;

	if (!params[0]) {
		my_printf("not_enough_params");
		return 0;
	}

	if (!(uin = get_uin(params[0])) || !(u = userlist_find(uin, NULL))) {
		my_printf("user_not_found", params[0]);
		return 0;
	}

	tmp = format_user(uin);

	if (!userlist_remove(u)) {
		my_printf("user_deleted", tmp);
		gg_remove_notify(sess, uin);
	} else
		my_printf("error_deleting");

	return 0;
}

COMMAND(command_exec)
{
	struct list *l;
	int pid;

	if (params[0]) {
		if (!(pid = fork())) {
			execl("/bin/sh", "sh", "-c", params[0], NULL);
			exit(1);
		}
		add_process(pid, params[0]);
	} else {
		for (l = children; l; l = l->next) {
			struct process *p = l->data;

			my_printf("process", itoa(p->pid), p->name);
		}
		if (!children)
			my_printf("no_processes");
	}

	return 0;
}

COMMAND(command_find)
{
	struct gg_search_request r;
	struct gg_http *h;
	struct list *l;
	char **argv = NULL, *query = NULL;
	int i, id = 1;

	/* wybieramy sobie identyfikator sercza */
	for (l = watches; l; l = l->next) {
		struct gg_http *h = l->data;

		if (h->type != GG_SESSION_SEARCH)
			continue;

		if (h->id / 2 >= id)
			id = h->id / 2 + 1;
	}
	
	if (params[0])
		query = strdup(params[0]);
	
	memset(&r, 0, sizeof(r));

	if (!params[0] || !(argv = array_make(params[0], " \t", 0, 1, 1)) || !argv[0]) {
		my_printf("not_enough_params");
		array_free(argv);
		free(query);
		return 0;
	}

/*	XXX przerobiæ to na watches 
	
	if (!strncasecmp(argv[0], "--s", 3) || !strncasecmp(argv[0], "-s", 2)) {
		if (search)
			gg_http_stop(search);
		gg_free_search(search);
		search = NULL;
		my_printf("search_stopped");
		return 0;
	}
*/
	if (argv[0] && !argv[1] && argv[0][0] != '-') {
		id = id * 2;	/* single search */
		if (!(r.uin = get_uin(params[0]))) {
			my_printf("user_not_found", params[0]);
			free(query);
			array_free(argv);
			return 0;
		}
	} else {
		id = id * 2 + 1;	/* multiple search */
		for (i = 0; argv[i]; i++) {
			char *arg = argv[i];
			
			if (*arg == '-' && *(arg + 1) == '-')
				arg++;

			if (!strncmp(arg, "-f", 2) && arg[2] != 'e' && argv[i + 1])
				r.first_name = argv[++i];
			if (!strncmp(arg, "-l", 2) && argv[i + 1])
				r.last_name = argv[++i];
			if (!strncmp(arg, "-n", 2) && argv[i + 1])
				r.nickname = argv[++i];
			if (!strncmp(arg, "-c", 2) && argv[i + 1])
				r.city = argv[++i];
			if (!strncmp(arg, "-p", 2) && argv[i + 1])
				r.phone = argv[++i];
			if (!strncmp(arg, "-e", 2) && argv[i + 1])
				r.email = argv[++i];
			if (!strncmp(arg, "-u", 2) && argv[i + 1])
				r.uin = strtol(argv[++i], NULL, 0);
			if (!strncmp(arg, "-s", 2) && argv[i + 1])
				r.start = strtol(argv[++i], NULL, 0);
			if (!strncmp(arg, "-fe", 3))
				r.gender = GG_GENDER_FEMALE;
			if (!strncmp(arg, "-m", 2))
				r.gender = GG_GENDER_MALE;
			if (!strncmp(arg, "-a", 2))
				r.active = 1;
			if (!strncmp(arg, "-b", 2) && argv[i + 1]) {
				char *foo = strchr(argv[++i], ':');
	
				if (!foo) {
					r.min_birth = atoi(argv[i]);
					r.max_birth = atoi(argv[i]);
				} else {
					*foo = 0;
					r.min_birth = atoi(argv[i]);
					r.max_birth = atoi(++foo);
				}
				if (r.min_birth < 100)
					r.min_birth += 1900;
				if (r.max_birth < 100)
					r.max_birth += 1900;
			}
		}
	}

	if (!(h = gg_search(&r, 1))) {
		my_printf("search_failed", strerror(errno));
		free(query);
		array_free(argv);
		return 0;
	}

	h->id = id;
	h->user_data = query;

	list_add(&watches, h, 0);
	
	array_free(argv);

	return 0;
}

COMMAND(command_change)
{
	struct gg_change_info_request *r;
	struct gg_http *h;
	char **argv = NULL;
	int i;

	if (!params[0]) {
		my_printf("not_enough_params");
		return 0;
	}

	if (!(argv = array_make(params[0], " \t", 0, 1, 1)))
		return 0;

	if (!(r = calloc(1, sizeof(*r)))) {
		array_free(argv);
		return 0;
	}
	
	for (i = 0; argv[i]; i++) {
		char *arg = argv[i];
		
		if (*arg == '-' && *(arg + 1) == '-')
			arg++;
		
		if (!strncmp(arg, "-f", 2) && strncmp(arg, "-fe", 3) && argv[i + 1]) {
			free(r->first_name);
			r->first_name = strdup(argv[++i]);
		}
		
		if (!strncmp(arg, "-l", 2) && argv[i + 1]) {
			free(r->last_name);
			r->last_name = strdup(argv[++i]);
		}
		
		if (!strncmp(arg, "-n", 2) && argv[i + 1]) {
			free(r->nickname);
			r->nickname = strdup(argv[++i]);
		}
		
		if (!strncmp(arg, "-e", 2) && argv[i + 1]) {
			free(r->email);
			r->email = strdup(argv[++i]);
		}

		if (!strncmp(arg, "-c", 2) && argv[i + 1]) {
			free(r->city);
			r->city = strdup(argv[++i]);
		}
		
		if (!strncmp(arg, "-b", 2) && argv[i + 1])
			r->born = atoi(argv[++i]);
		
		if (!strncmp(arg, "-fe", 3))
			r->gender = GG_GENDER_FEMALE;

		if (!strncmp(arg, "-m", 2))
			r->gender = GG_GENDER_MALE;
	}

	if (!r->first_name || !r->last_name || !r->nickname || !r->email || !r->city || !r->born) {
		gg_change_info_request_free(r);
		my_printf("change_not_enough_params");
		array_free(argv);
		return 0;
	}

	if ((h = gg_change_info(config_uin, config_password, r, 1)))
		list_add(&watches, h, 0);

	gg_change_info_request_free(r);
	array_free(argv);

	return 0;
}

COMMAND(command_modify)
{
	struct userlist *u;
	char **argv = NULL;
	uin_t uin;
	int i;

	if (!params[0]) {
		my_printf("not_enough_params");
		return 0;
	}

	if (!(uin = get_uin(params[0])) || !(u = userlist_find(uin, NULL))) {
		my_printf("user_not_found", params[0]);
		return 0;
	}

	if (!params[1]) {
		char *groups = group_to_string(u->groups);
		
		my_printf("user_info", u->first_name, u->last_name, u->nickname, u->display, u->mobile, groups);
		
		free(groups);

		return 0;
	} else {
		argv = array_make(params[1], " \t", 0, 1, 1);
	}

	for (i = 0; argv[i]; i++) {
		char *arg = argv[i];
		
		if (*arg == '-' && *(arg + 1) == '-')
			arg++;
		
		if (!strncmp(arg, "-f", 2) && argv[i + 1]) {
			free(u->first_name);
			u->first_name = strdup(argv[++i]);
		}
		
		if (!strncmp(arg, "-l", 2) && argv[i + 1]) {
			free(u->last_name);
			u->last_name = strdup(argv[++i]);
		}
		
		if (!strncmp(arg, "-n", 2) && argv[i + 1]) {
			free(u->nickname);
			u->nickname = strdup(argv[++i]);
		}
		
		if (!strncmp(arg, "-d", 2) && argv[i + 1]) {
			free(u->display);
			u->display = strdup(argv[++i]);
			userlist_replace(u);
		}
		
		if ((!strncmp(arg, "-p", 2) || !strncmp(arg, "-m", 2) || !strncmp(arg, "-s", 2)) && argv[i + 1]) {
			free(u->mobile);
			u->mobile = strdup(argv[++i]);
		}
		
		if (!strncmp(arg, "-g", 2) && argv[i + 1]) {
			switch (*argv[++i]) {
				case '-':
					group_remove(u, argv[i] + 1);
					break;
				case '+':
					group_add(u, argv[i] + 1);
					break;
				default:
					group_add(u, argv[i]);
			}
		}
		
		if (!strncmp(arg, "-u", 2) && argv[i + 1])
			u->uin = strtol(argv[++i], NULL, 0);
	}

	if (strcasecmp(name, "add"))
		my_printf("modify_done", params[0]);

	config_changed = 1;

	array_free(argv);

	return 0;
}

COMMAND(command_help)
{
	struct command *c;
	
	if (params[0]) {
		for (c = commands; c->name; c++)
			if (!strcasecmp(c->name, params[0])) {
				my_printf("help", c->name, c->params_help, c->brief_help);
				if (c->long_help && strcmp(c->long_help, "")) {
					char *foo, *tmp, *plumk, *bar = strdup(c->long_help);

					if ((foo = bar)) {
						while ((tmp = gg_get_line(&foo))) {
							plumk = format_string(tmp);
							if (plumk) {
								my_printf("help_more", plumk);
								free(plumk);
							}
						}
						free(bar);
					}
				}

				return 0;
			}
	}

	for (c = commands; c->name; c++)
		if (isalnum(*c->name))
			my_printf("help", c->name, c->params_help, c->brief_help);

	return 0;
}

COMMAND(command_ignore)
{
	uin_t uin;

	if (*name == 'i' || *name == 'I') {
		if (!params[0]) {
			struct list *l;

			for (l = ignored; l; l = l->next) {
				struct ignored *i = l->data;

				my_printf("ignored_list", format_user(i->uin));
			}

			if (!ignored) 
				my_printf("ignored_list_empty");

			return 0;
		}
		
		if (!(uin = get_uin(params[0]))) {
			my_printf("user_not_found", params[0]);
			return 0;
		}
		
		if (!ignored_add(uin)) {
			if (!in_autoexec) 
				my_printf("ignored_added", params[0]);
		} else
			my_printf("error_adding_ignored");

	} else {
		if (!params[0]) {
			my_printf("not_enough_params");
			return 0;
		}
		
		if (!(uin = get_uin(params[0]))) {
			my_printf("user_not_found", params[0]);
			return 0;
		}
		
		if (!ignored_remove(uin)) {
			if (!in_autoexec)
				my_printf("ignored_deleted", format_user(uin));
		} else
			my_printf("error_not_ignored", format_user(uin));
	
	}
	
	return 0;
}

COMMAND(command_list)
{
	struct list *l;
	int count = 0, show_all = 1, show_busy = 0, show_active = 0, show_inactive = 0, j;
	char *tmp, **argv = NULL;

	if (params[0] && *params[0] != '-') {
		struct userlist *u;
		char *groups;
		uin_t uin;
		
		if (!(uin = get_uin(params[0])) || !(u = userlist_find(uin, NULL))) {
			my_printf("user_not_found", params[0]);
			return 0;
		}

		/* list <alias> [opcje] */
		if (params[1]) {
			command_modify("modify", params);
			return 0;
		}

		groups = group_to_string(u->groups);
		
		my_printf("user_info", u->first_name, u->last_name, u->nickname, u->display, u->mobile, groups);
		
		free(groups);

		return 0;
	}

	/* list --get */
	if (params[0] && (!strncasecmp(params[0], "-g", 2) || !strncasecmp(params[0], "--g", 3))) {
		struct gg_http *h;
		
		if (!(h = gg_userlist_get(config_uin, config_password, 1))) {
			my_printf("userlist_get_error", strerror(errno));
			return 0;
		}
		
		list_add(&watches, h, 0);
		
		return 0;
	}

	/* list --put */
	if (params[0] && (!strncasecmp(params[0], "-p", 2) || !strncasecmp(params[0], "--p", 3))) {
		struct gg_http *h;
		char *contacts = userlist_dump();
		
		if (!contacts) {
			my_printf("userlist_put_error", strerror(ENOMEM));
			return 0;
		}
		
		if (!(h = gg_userlist_put(config_uin, config_password, contacts, 1))) {
			my_printf("userlist_put_error", strerror(errno));
			return 0;
		}
		
		free(contacts);
		
		list_add(&watches, h, 0);

		return 0;
	}

	/* list --active | --busy | --inactive */
	for (j = 0; params[j]; j++) {
      		if ((argv = array_make(params[j], " \t", 0, 1, 1))) {
			int i;

	 		for (i = 0; argv[i]; i++) {
				char *arg = argv[i];
			
				if (*arg == '-' && *(arg + 1) == '-')
					arg++;
				
				if (!strncasecmp(arg, "-a", 2)) {
					show_all = 0;
					show_active = 1;
				}
				if (!strncasecmp(arg, "-u", 2) || !strncasecmp(arg, "-i", 2) || !strncasecmp(arg, "-n", 2)) {
					show_all = 0;
					show_inactive = 1;
				}
				if (!strncasecmp(arg, "-b", 2)) {
					show_all = 0;
					show_busy = 1;
				}
			}
			array_free(argv);
		}
	}

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		struct in_addr in;

		tmp = "list_unknown";
		switch (u->status) {
			case GG_STATUS_AVAIL:
				tmp = "list_avail";
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
			
		}

		in.s_addr = u->ip.s_addr;

		if (show_all || (show_busy && (u->status == GG_STATUS_BUSY || u->status == GG_STATUS_BUSY_DESCR)) || (show_active && u->status == GG_STATUS_AVAIL) || (show_inactive && (u->status == GG_STATUS_NOT_AVAIL || u->status == GG_STATUS_NOT_AVAIL_DESCR))) {
			my_printf(tmp, format_user(u->uin), inet_ntoa(in), itoa(u->port), u->descr);
			count++;
		}
	}

	if (!count && show_all)
		my_printf("list_empty");

	return 0;
}

COMMAND(command_msg)
{
	struct userlist *u;
	char *msg = NULL;
	uin_t uin;
	int free_msg = 0, chat = (!strcasecmp(name, "chat"));

	if (!sess || sess->state != GG_STATE_CONNECTED) {
		my_printf("not_connected");
		return 0;
	}

	if (!params[0] || !params[1]) {
		my_printf("not_enough_params");
		return 0;
	}
	
	if (!(uin = get_uin(params[0]))) {
		my_printf("user_not_found", params[0]);
		return 0;
	}

	if (!strcmp(params[1], "\\")) {
		struct string *s;
		char *line;

		if (!(s = string_init(NULL)))
			return 0;

		no_prompt = 1;

		while ((line = my_readline())) {
			if (!strcmp(line, ".")) {
				free(line);
				break;
			}
			string_append(s, line);
			string_append(s, "\r\n");
			free(line);
		}

		no_prompt = 0;

		if (!line) {
			printf("\n");
			string_free(s, 1);
			return 0;
		}
		msg = string_free(s, 0);
		free_msg = 1;
	} else
		msg = params[1];

        u = userlist_find(uin, NULL);

	put_log(uin, "%s,%ld,%s,%ld,%s\n", (chat) ? "chatsend" : "msgsend", uin, (u) ? u->display : "", time(NULL), msg);

	if (!query_nick || strcasecmp(query_nick, params[0]))
		add_send_nick(params[0]);
	iso_to_cp(msg);
	gg_send_message(sess, (chat) ? GG_CLASS_CHAT : GG_CLASS_MSG, uin, msg);

	if (free_msg)
		free(msg);

	unidle();

	return 0;
}

COMMAND(command_save)
{
	last_save = time(NULL);

	if (!userlist_write(NULL) && !config_write(NULL)) {
		my_printf("saved");
		config_changed = 0;
	} else
		my_printf("error_saving");

	return 0;
}

COMMAND(command_theme)
{
	if (!params[0]) {
		my_printf("not_enough_params");
		return 0;
	}
	
	if (!strcmp(params[0], "-")) {
		init_theme();
		reset_theme_cache();
		if (!in_autoexec)
			my_printf("theme_default");
		variable_set("theme", NULL, 0);
	} else {
		if (!read_theme(params[0], 1)) {
			reset_theme_cache();
			if (!in_autoexec)
				my_printf("theme_loaded", params[0]);
			variable_set("theme", params[0], 0);
		} else
			my_printf("error_loading_theme", strerror(errno));
	}
	
	return 0;
}

COMMAND(command_set)
{
	struct list *l;
	int unset = 0;
	char *arg;

	if ((arg = params[0]) && *arg == '-') {
		unset = 1;
		arg++;
	}

	if ((!params[0] || !params[1]) && !unset) {
		for (l = variables; l; l = l->next) {
			struct variable *v = l->data;
			
			if ((!arg || !strcasecmp(arg, v->name)) && v->display != 2) {
				if (v->type == VAR_STR) {
					char *tmp = *(char**)(v->ptr);
					
					if (!tmp)
						tmp = "(brak)";
					if (!v->display)
						tmp = "(...)";
					my_printf("variable", v->name, tmp);
				} else {
					my_printf("variable", v->name, (!v->display) ? "(...)" : itoa(*(int*)(v->ptr)));
				}
			}
		}
	} else {
		reset_theme_cache();
		switch (variable_set(arg, params[1], 0)) {
			case 0:
				if (!in_autoexec) {
					my_printf("variable", arg, params[1]);
					config_changed = 1;
				}
				break;
			case -1:
				my_printf("variable_not_found", arg);
				break;
			case -2:
				my_printf("variable_invalid", arg);
				break;
		}
	}

	return 0;
}

COMMAND(command_sms)
{
	struct userlist *u;
	char *number = NULL;

	if (!params[1]) {
		my_printf("not_enough_params");
		return 0;
	}

	if ((u = userlist_find(0, params[0]))) {
		if (!u->mobile || !strcmp(u->mobile, "")) {
			my_printf("sms_unknown", format_user(u->uin));
			return 0;
		}
		number = u->mobile;
	} else
		number = params[0];

	if (send_sms(number, params[1], 1) == -1)
		my_printf("sms_error", strerror(errno));

	return 0;
}

COMMAND(command_quit)
{
	my_printf("quit");
	gg_logoff(sess);
	list_remove(&watches, sess, 0);
	gg_free_session(sess);
	sess = NULL;
	return -1;
}

COMMAND(command_dcc)
{
	struct transfer t;
	struct list *l;
	uin_t uin;

	if (!params[0] || !strncasecmp(params[0], "sh", 2)) {	/* show */
		int pending = 0, active = 0;

		if (params[1] && params[1][0] == 'd') {	/* show debug */
			for (l = transfers; l; l = l->next) {
				struct transfer *t = l->data;
				
				my_printf("dcc_show_debug", itoa(t->id), (t->type == GG_SESSION_DCC_SEND) ? "SEND" : "GET", t->filename, format_user(t->uin), (t->dcc) ? "yes" : "no");
			}

			return 0;
		}

		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (!t->dcc || (t->dcc->state != GG_STATE_SENDING_FILE || t->dcc->state != GG_STATE_GETTING_FILE)) {
				if (!pending) {
					my_printf("dcc_show_pending_header");
					pending = 1;
				}
				my_printf((t->type == GG_SESSION_DCC_SEND) ? "dcc_show_pending_send" : "dcc_show_pending_get", itoa(t->id), format_user(t->uin), (t->filename) ? t->filename : "(?)");
			}
		}

		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (t->dcc && (t->dcc->state == GG_STATE_SENDING_FILE || t->dcc->state == GG_STATE_GETTING_FILE)) {
				if (!active) {
					my_printf("dcc_show_active_header");
					active = 1;
				}
				my_printf((t->type == GG_SESSION_DCC_SEND) ? "dcc_show_active_send" : "dcc_show_active_get", itoa(t->id), format_user(t->uin), t->filename);
			}
		}

		if (!active && !pending)
			my_printf("dcc_show_empty");
		
		return 0;
	}
	
	if (!strncasecmp(params[0], "se", 2)) {		/* send */
		struct userlist *u;
		struct stat st;
		int fd;

		if (!params[1] || !params[2]) {
			my_printf("not_enough_params");
			return 0;
		}
		
		if (!(uin = get_uin(params[1])) || !(u = userlist_find(uin, NULL))) {
			my_printf("user_not_found", params[1]);
			return 0;
		}

		if (!sess || sess->state != GG_STATE_CONNECTED) {
			my_printf("not_connected");
			return 0;
		}

		if ((fd = open(params[2], O_RDONLY)) == -1 || stat(params[2], &st)) {
			my_printf("dcc_open_error", params[2], strerror(errno));
			return 0;
		} else {
			close(fd);
			if (S_ISDIR(st.st_mode)) {
				my_printf("dcc_open_directory", params[2]);
				return 0;
			}
		}

		t.uin = uin;
		t.id = transfer_id();
		t.type = GG_SESSION_DCC_SEND;
		t.filename = strdup(params[2]);
		t.dcc = NULL;

		if (u->port < 10) {
			/* nie mo¿emy siê z nim po³±czyæ, wiêc on spróbuje */
			gg_dcc_request(sess, uin);
		} else {
			struct gg_dcc *d;
			
			if (!(d = gg_dcc_send_file(u->ip.s_addr, u->port, config_uin, uin))) {
				my_printf("dcc_error", strerror(errno));
				return 0;
			}

			if (gg_dcc_fill_file_info(d, params[2]) == -1) {
				my_printf("dcc_open_error", params[2], strerror(errno));
				gg_free_dcc(d);
				return 0;
			}

			list_add(&watches, d, 0);

			t.dcc = d;
		}

		list_add(&transfers, &t, sizeof(t));

		return 0;
	}

	if (!strncasecmp(params[0], "g", 1)) {		/* get */
		struct transfer *t;
		
		for (t = NULL, l = transfers; l; l = l->next) {
			struct userlist *u;
			
			t = l->data;

			if (!t->dcc || t->type != GG_SESSION_DCC_GET || !t->filename)
				continue;
			
			if (!params[1])
				break;

			if (params[1][0] == '#' && atoi(params[1] + 1) == t->id)
				break;

			if ((u = userlist_find(t->uin, NULL))) {
				if (!strcasecmp(params[1], itoa(u->uin)) || !strcasecmp(params[1], u->display))
					break;
			}
		}

		if (!l || !t || !t->dcc) {
			my_printf("dcc_get_not_found", (params[1]) ? params[1] : "");
			return 0;
		}

		/* XXX wiêcej sprawdzania, zrzucaæ do jakiego¶ $dcc_dir */
		if ((t->dcc->file_fd = open(t->filename, O_WRONLY | O_CREAT, 0600)) == -1) {
			my_printf("dcc_get_cant_create", t->filename);
			gg_free_dcc(t->dcc);
			list_remove(&transfers, t, 1);
			
			return 0;
		}
		
		my_printf("dcc_get_getting", format_user(t->uin), t->filename);
		
		list_add(&watches, t->dcc, 0);

		return 0;
	}
	
	if (!strncasecmp(params[0], "c", 1)) {		/* close */
		my_printf("dcc_not_supported", params[0]);
		return 0;
	}

	my_printf("dcc_unknown_command", params[0]);
	
	return 0;
}

COMMAND(command_test_ping)
{
	if (sess)
		gg_ping(sess);

	return 0;
}

COMMAND(command_test_send)
{
	struct gg_event *e = malloc(sizeof(struct gg_event));
	
	e->type = GG_EVENT_MSG;
	e->event.msg.sender = get_uin(params[0]);
	e->event.msg.message = strdup(params[1]);
	e->event.msg.msgclass = GG_CLASS_MSG;
	
	handle_msg(e);

	return 0;
}

COMMAND(command_test_add)
{
	if (params[0])
		add_send_nick(params[0]);

	return 0;
}

COMMAND(command_test_watches)
{
	struct list *l;
	char buf[200], *type, *state, *check;
	int no = 0;

	for (l = watches; l; l = l->next, no++) {
		struct gg_common *s = l->data;
		
		switch (s->type) {
			case GG_SESSION_GG: type = "GG"; break;
			case GG_SESSION_HTTP: type = "HTTP"; break;
			case GG_SESSION_SEARCH: type = "SEARCH"; break;
			case GG_SESSION_REGISTER: type = "REGISTER"; break;
			case GG_SESSION_REMIND: type = "REMIND"; break;
			case GG_SESSION_CHANGE: type = "CHANGE"; break;
			case GG_SESSION_PASSWD: type = "PASSWD"; break;
			case GG_SESSION_DCC: type = "DCC"; break;
			case GG_SESSION_DCC_SOCKET: type = "DCC_SOCKET"; break;
			case GG_SESSION_DCC_SEND: type = "DCC_SEND"; break;
			case GG_SESSION_DCC_GET: type = "DCC_GET"; break;
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
		
		snprintf(buf, sizeof(buf), "%d: type=%s, fd=%d, state=%s, check=%s, id=%d, timeout=%d", no, type, s->fd, state, check, s->id, s->timeout);
		my_printf("generic", buf);
	}

	return 0;
}

COMMAND(command_test_fds)
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

		my_printf("generic", buf);
	}
#endif
	return 0;
}

COMMAND(command_register)
{
	struct gg_http *h;
	struct list *l;
	
	if (!params[0] || !params[1]) {
		my_printf("not_enough_params");
		return 0;
	}

	for (l = watches; l; l = l->next) {
		struct gg_common *s = l->data;

		if (s->type == GG_SESSION_REGISTER) {
			my_printf("register_pending");
			return 0;
		}
	}
	
	if (!(h = gg_register(params[0], params[1], 1))) {
		my_printf("register_failed", strerror(errno));
		return 0;
	}

	list_add(&watches, h, 0);

	reg_password = strdup(params[1]);
	
	return 0;
}

COMMAND(command_passwd)
{
	struct gg_http *h;
	
	if (!params[0] || !params[1]) {
		my_printf("not_enough_params");
		return 0;
	}

	if (!(h = gg_change_passwd(config_uin, config_password, params[0], params[1], 1))) {
		my_printf("passwd_failed", strerror(errno));
		return 0;
	}

	list_add(&watches, h, 0);

	reg_password = strdup(params[0]);
	
	return 0;
}

COMMAND(command_remind)
{
	struct gg_http *h;
	
	if (!(h = gg_remind_passwd(config_uin, 1))) {
		my_printf("remind_failed", strerror(errno));
		return 0;
	}

	list_add(&watches, h, 0);
	
	return 0;
}

COMMAND(command_query)
{
	uin_t uin;

	if (!params[0] && !query_nick)
		return 0;

	if (query_nick) {
		free(query_nick);
		query_nick = NULL;
	}

	if (!params[0]) {
		my_printf("query_finished", format_user(query_uin));
		query_uin = 0;
		return 0;
	}

	if (!(uin = get_uin(params[0]))) {
		my_printf("user_not_found", params[0]);
		return 0;
	}

	query_nick = strdup(params[0]);
	query_uin = uin;
	my_printf("query_started", format_user(uin));

	return 0;
}

COMMAND(command_on)
{
        int flags;
        uin_t uin;

        if (!params[0] || !strncasecmp(params[0], "-l", 2)) {
                struct list *l;
                int count = 0;

                for (l = events; l; l = l->next) {
                        struct event *ev = l->data;

                        my_printf("events_list", format_events(ev->flags), (ev->uin == 1) ? "*" : format_user(ev->uin), ev->action);
                        count++;
                }

                if (!count)
                        my_printf("events_list_empty");

                return 0;
        }

        if (!params[1] || !params[2]) {
                my_printf("not_enough_params");
                return 0;
        }

        if (!(flags = get_flags(params[0]))) {
                my_printf("events_incorrect");
                return 0;
        }

        if (*params[1] == '*')
                uin = 1;
        else
                uin = get_uin(params[1]);

        if (!uin) {
                my_printf("invalid_uin");
                return 0;
        }

        if (!strncasecmp(params[2], "clear", 5)) {
                del_event(flags, uin);
                config_changed = 1;
                return 0;
        }

        if (correct_event(params[2]))
                return 0;

        add_event(flags, uin, params[2]);
        config_changed = 1;
        return 0;
}

char *strip_spaces(char *line)
{
	char *buf;
	
	for (buf = line; isspace(*buf); buf++);

	while (isspace(line[strlen(line) - 1]))
		line[strlen(line) - 1] = 0;
	
	return buf;
}

int execute_line(char *line)
{
	char *cmd = NULL, *tmp, *p = NULL, short_cmd[2] = ".", *last_name = NULL, *last_params = NULL;
	struct command *c;
	int (*last_abbr)(char *, char **) = NULL;
	int abbrs = 0;

	if (query_nick && *line != '/') {
		char *params[] = { query_nick, line, NULL };

		if (strcmp(line, ""))
			command_msg("chat", params);

		return 0;
	}
	
	send_nicks_index = 0;

	line = strdup(strip_spaces(line));
	
	if (*line == '/')
		line++;

	for (c = commands; c->name; c++)
		if (!isalpha(c->name[0]) && strlen(c->name) == 1 && line[0] == c->name[0]) {
			short_cmd[0] = c->name[0];
			cmd = short_cmd;
			p = line + 1;
		}

	if (!cmd) {
		tmp = cmd = line;
		while (*tmp && !isspace(*tmp))
			tmp++;
		p = (*tmp) ? tmp + 1 : tmp;
		*tmp = 0;
		p = strip_spaces(p);
	}
		
	for (c = commands; c->name; c++) {
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
		int res = 0, len = strlen(last_params);

		c--;
			
		if ((par = array_make(p, " \t", len, 1, 1))) {
			res = (last_abbr)(last_name, par);
			array_free(par);
		}

		return res;
	}

	if (strcmp(cmd, ""))
		my_printf("unknown_command", cmd);
	
	return 0;
}

