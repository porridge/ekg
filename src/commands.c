/* $Id$ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>
 *		        Robert J. Wo¼ny <speedy@ziew.org>
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libgg.h"
#include "stuff.h"
#include "dynstuff.h"
#include "commands.h"
#include "events.h"
#include "themes.h"
#include "vars.h"

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
	command_test_add(), command_theme(), command_set(), command_connect(),
	command_sms(), command_find(), command_modify(), command_cleartab(),
	command_status(), command_register();

/*
 * drugi parametr definiuje ilo¶æ oraz rodzaje parametrów (tym samym
 * ich dope³nieñ)
 *
 * '?' - olewamy,
 * 'U' - rêcznie wpisany uin, nadawca mesgów,
 * 'u' - nazwa lub uin z kontaktów, rêcznie wpisany uin, nadawca mesgów,
 * 'c' - komenda,
 * 'i' - nicki z listy ignorowanych osób.
 */

struct command commands[] = {
	{ "add", "U??", command_add, " <numer> <alias> [opcje]", "Dodaje u¿ytkownika do listy kontaktów", "Opcje identyczne jak dla polecenia %Wmodify%n" },
	{ "alias", "??", command_alias, " [opcje]", "Zarz±dzanie aliasami", "  --add <alias> <komenda>\n  --del <alias>\n  [--list]\n" },
	{ "away", "", command_away, "", "Zmienia stan na zajêty", "" },
	{ "back", "", command_away, "", "Zmienia stan na dostêpny", "" },
	{ "chat", "u?", command_msg, " <numer/alias> <wiadomo¶æ>", "Wysy³a wiadomo¶æ w ramach rozmowy", "" },
	{ "cleartab", "", command_cleartab, "", "Czy¶ci listê nicków do dope³nienia", "" },
	{ "connect", "", command_connect, "", "£±czy siê z serwerem", "" },
	{ "del", "u", command_del, " <numer/alias>", "Usuwa u¿ytkownika z listy kontaktów", "" },
	{ "disconnect", "", command_connect, "", "Roz³±cza siê z serwerem", "" },
	{ "exec", "?", command_exec, " <polecenie>", "Uruchamia polecenie systemowe", "" },
	{ "!", "?", command_exec, " <polecenie>", "Synonim dla %Wexec%n", "" },
	{ "find", "u", command_find, " [opcje]", "Interfejs do katalogu publicznego", "  --uin <numerek>\n  --first <imiê>\n  --last <nazwisko>\n  --nick <pseudonim>\n  --city <miasto>\n  --birth <min:max>\n  --phone <telefon>\n  --email <e-mail>\n  --active\n  --female\n  --male\n" },
	{ "info", "u", command_find, " <numer/alias>", "Interfejs do katalogu publicznego", "" },
	{ "help", "c", command_help, " [polecenie]", "Wy¶wietla informacjê o poleceniach", "" },
	{ "?", "c", command_help, " [polecenie]", "Synonim dla %Whelp%n", "" },
	{ "ignore", "u", command_ignore, " [numer/alias]", "Dodaje do listy ignorowanych lub j± wy¶wietla", "" },
	{ "invisible", "", command_away, "", "Zmienia stan na niewidoczny", "" },
	{ "list", "", command_list, " [opcje]", "Wy¶wietla listê kontaktów", "  --active\n  --busy\n  --inactive\n" },
	{ "msg", "u?", command_msg, " <numer/alias> <wiadomo¶æ>", "Wysy³a wiadomo¶æ do podanego u¿ytkownika", "" },
	{ "modify", "u?", command_modify, " <alias> [opcje]", "Zmienia informacje w li¶cie kontaktów", "  --first <imiê>\n  --last <nazwisko>\n  --nick <pseudonim>  // tylko informacja\n  --alias <alias>  // nazwa w li¶cie kontaktów\n  --phone <telefon>\n  --uin <numerek>\n" },
	{ "private", "", command_away, " [on/off]", "W³±cza/wy³±cza tryb ,,tylko dla przyjació³''", "" },
	{ "save", "", command_save, "", "Zapisuje ustawienia programu", "" },
	{ "set", "v?", command_set, " <zmienna> <warto¶æ>", "Wy¶wietla lub zmienia ustawienia", "" },
	{ "sms", "u?", command_sms, " <numer/alias> <tre¶æ>", "Wysy³a SMSa do podanej osoby", "" },
	{ "status", "", command_status, "", "Wy¶wietla aktualny stan", "" },
	{ "theme", "f", command_theme, " <plik>", "£aduje opis wygl±du z podanego pliku", "" },
	{ "quit", "", command_quit, "", "Wychodzi z programu", "" },
	{ "unignore", "i", command_ignore, " <numer/alias>", "Usuwa z listy ignorowanych osób", "" },
	{ "register", "??", command_register, " <email> <has³o>", "Rejestruje nowy uin", "" },
	{ "_send", "u?", command_test_send, "", "", "" },
	{ "_add", "?", command_test_add, "", "", "" },
	{ NULL, NULL, NULL, NULL, NULL }
};

char *command_generator(char *text, int state)
{
	static int index = 0, len;
	char *name;

	if (*text == '/')
		text++;

	if (!*rl_line_buffer) {
		if (state)
			return NULL;
		if (!send_nicks_count)
			return strdup("msg");
		send_nicks_index = (send_nicks_count > 1) ? 1 : 0;
		if (!(name = malloc(6 + strlen(send_nicks[0]))))
			return NULL;
		sprintf(name, "chat %s", send_nicks[0]);
		
		return name;
	}

	if (!state) {
		index = 0;
		len = strlen(text);
	}

	while ((name = commands[index++].name))
		if (!strncasecmp(text, name, len))
			return strdup(name);

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

		if (!strncasecmp(text, u->comment, len))
			return strdup(u->comment);
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
	static int index = 0, len;

	if (!state) {
		index = 0;
		len = strlen(text);
	}

	while (variables[index++].name)
		if (!strncasecmp(text, variables[index - 1].name, len))
			return strdup(variables[index - 1].name);

	return NULL;
}

char *ignored_uin_generator(char *text, int state)
{
	static struct list *l;
	static int len;
	struct userlist *u;
	char buf[16];

	if (!state) {
		l = ignored;
		len = strlen(text);
	}

	while (l) {
		struct ignored *i = l->data;

		l = l->next;

		if (!(u = find_user(i->uin, NULL))) {
			snprintf(buf, sizeof(buf), "%lu", i->uin);
			if (!strncasecmp(text, buf, len))
				return strdup(buf);
		} else {
			if (!strncasecmp(text, u->comment, len))
				return strdup(u->comment);
		}
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

	if (find_user(atoi(params[0]), params[1])) {
		my_printf("user_exists", params[1]);
		return 0;
	}

	if (!(uin = atoi(params[0]))) {
		my_printf("invalid_uin");
		return 0;
	}

	if (!add_user(uin, params[1])) {
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
	if (params[0] && params[0][0] == '-' && params[0][1] == '-')
		params[0]++;

	if (!params[0] || !strncasecmp(params[0], "-l", 2)) {
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

	if (!strncasecmp(params[0], "-a", 2)) {
		if (!add_alias(params[1], 0))
			config_changed = 1;

		return 0;
	}

	if (!strncasecmp(params[0], "-d", 2)) {
		if (!del_alias(params[1]))
			config_changed = 1;

		return 0;
	}

	my_printf("aliases_invalid");
	return 0;
}

COMMAND(command_away)
{
	int status_table[3] = { GG_STATUS_AVAIL, GG_STATUS_BUSY, GG_STATUS_INVISIBLE };	unidle();
	
	if (!strcasecmp(name, "away")) {
		away = 1;
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

	default_status = status_table[away] | ((private_mode) ? GG_STATUS_FRIENDS_MASK : 0);

	if (sess && sess->state == GG_STATE_CONNECTED)
		gg_change_status(sess, default_status);

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
			if (!(sess = gg_login(config_uin, config_password, 1))) {
	                        my_printf("conn_failed", strerror(errno));
	                        do_reconnect();
	                } else {
				sess->initial_status = default_status;
			}
		} else
			my_printf("no_config");
	} else if (sess) {
		connecting = 0;
		if (sess->state == GG_STATE_CONNECTED)
			my_printf("disconnected");
		gg_logoff(sess);
		gg_free_session(sess);
		sess = NULL;
		reconnect_timer = 0;
	}

	return 0;
}

COMMAND(command_del)
{
	uin_t uin;
	char *tmp;

	if (!params[0]) {
		my_printf("not_enough_params");
		return 0;
	}

	if (!(uin = get_uin(params[0])) || !find_user(uin, NULL)) {
		my_printf("user_not_found", params[0]);
		return 0;
	}

	tmp = format_user(uin);

	if (!del_user(uin)) {
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
			char buf[10];

			snprintf(buf, sizeof(buf), "%d", p->pid);
			my_printf("process", buf, p->name);
		}
		if (!children)
			my_printf("no_processes");
	}

	return 0;
}

COMMAND(command_find)
{
	struct gg_search_request r;
	char **argv;
	int i;

	memset(&r, 0, sizeof(r));

	if (!strcasecmp(name, "info") && !params[0]) {
	    search_type = 1;
	    r.uin = config_uin;
	    if (!(search = gg_search(&r, 1))) {
		my_printf("search_failed", strerror(errno));
		return 0;
	    }
	    return 0;
	};
	
	if (!params[0] || !(argv = split_params(params[0], -1)) || !argv[0]) {
		my_printf("not_enough_params");
		return 0;
	}

	if (!strncasecmp(argv[0], "--s", 3) || !strncasecmp(argv[0], "-s", 2)) {
		if (search)
			gg_search_cancel(search);
		gg_free_search(search);
		search = NULL;
		my_printf("search_stopped");
		return 0;
	}

	if (search) {
		my_printf("already_searching");
		return 0;
	}

	if (!argv[1]) {
		search_type = 1;

		if (!(r.uin = get_uin(params[0]))) {
			my_printf("user_not_found", params[0]);
			return 0;
		}
	} else {
		search_type = 2;

		for (i = 0; argv[i]; i++) {
			if (argv[i][0] == '-' && argv[i][1] == '-')
				argv[i]++;
			if (!strncmp(argv[i], "-f", 2) && argv[i][2] != 'e' && argv[i + 1])
				r.first_name = argv[++i];
			if (!strncmp(argv[i], "-l", 2) && argv[i + 1])
				r.last_name = argv[++i];
			if (!strncmp(argv[i], "-n", 2) && argv[i + 1])
				r.nickname = argv[++i];
			if (!strncmp(argv[i], "-c", 2) && argv[i + 1])
				r.city = argv[++i];
			if (!strncmp(argv[i], "-p", 2) && argv[i + 1])
				r.phone = argv[++i];
			if (!strncmp(argv[i], "-e", 2) && argv[i + 1])
				r.email = argv[++i];
			if (!strncmp(argv[i], "-u", 2) && argv[i + 1])
				r.uin = strtol(argv[++i], NULL, 0);
			if (!strncmp(argv[i], "-fe", 3))
				r.gender = GG_GENDER_FEMALE;
			if (!strncmp(argv[i], "-m", 2))
				r.gender = GG_GENDER_MALE;
			if (!strncmp(argv[i], "-a", 2))
				r.active = 1;
			if (!strncmp(argv[i], "-b", 2) && argv[i + 1]) {
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

	if (!(search = gg_search(&r, 1))) {
		my_printf("search_failed", strerror(errno));
		return 0;
	}

	return 0;
}

COMMAND(command_modify)
{
	struct userlist *u;
	char **argv;
	uin_t uin;
	int i;

	if (!params[0]) {
		my_printf("not_enough_params");
		return 0;
	}

	if (!(uin = get_uin(params[0])) || !(u = find_user(uin, NULL))) {
		my_printf("user_not_found", params[0]);
		return 0;
	}

	if (!params[1]) {
		my_printf("user_info", u->first_name, u->last_name, u->nickname, u->comment, u->mobile, u->group);
		return 0;
	} else {
		argv = split_params(params[1], -1);
	}

	for (i = 0; argv[i]; i++) {
		if (argv[i][0] == '-' && argv[i][1] == '-')
			argv[i]++;
		if (!strncmp(argv[i], "-f", 2) && argv[i + 1]) {
			free(u->first_name);
			u->first_name = strdup(argv[++i]);
		}
		if (!strncmp(argv[i], "-l", 2) && argv[i + 1]) {
			free(u->last_name);
			u->last_name = strdup(argv[++i]);
		}
		if (!strncmp(argv[i], "-n", 2) && argv[i + 1]) {
			free(u->nickname);
			u->nickname = strdup(argv[++i]);
		}
		if (!strncmp(argv[i], "-a", 2) && argv[i + 1]) {
			free(u->comment);
			u->comment = strdup(argv[++i]);
			replace_user(u);
		}
		if ((!strncmp(argv[i], "-p", 2) || !strncmp(argv[i], "-m", 2) || !strncmp(argv[i], "-s", 2)) && argv[i + 1]) {
			free(u->mobile);
			u->mobile = strdup(argv[++i]);
		}
		if (!strncmp(argv[i], "-g", 2) && argv[i + 1]) {
			free(u->group);
			u->group = strdup(argv[++i]);
		}
		if (!strncmp(argv[i], "-u", 2) && argv[i + 1])
			u->uin = strtol(argv[++i], NULL, 0);
	}

	if (strcasecmp(name, "add"))
		my_printf("modify_done", params[0]);

	config_changed = 1;

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
		
		if (!add_ignored(uin)) {
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
		
		if (!del_ignored(uin)) {
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
	int count = 0, show_all = 1, show_busy = 0, show_active = 0, show_inactive = 0;
	char *tmp, **argv;

        if (params[0] && (argv = split_params(params[0], -1))) {
		int i;

		for (i = 0; argv[i]; i++) {
			if (argv[i][0] == '-' && argv[i][1] == '-')
				argv[i]++;
			if (argv[i][0] == '-' && argv[i][1] == 'a') {
				show_all = 0;
				show_active = 1;
			}
			if (argv[i][0] == '-' && (argv[i][1] == 'i' || argv[i][1] == 'u' || argv[i][1] == 'n')) {
				show_all = 0;
				show_inactive = 1;
			}
			if (argv[i][0] == '-' && argv[i][1] == 'b') {
				show_all = 0;
				show_busy = 1;
			}
		}
	}

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		char __ip[16], __port[16];
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
		}

		in.s_addr = u->ip;
		snprintf(__ip, sizeof(__ip), "%s", inet_ntoa(in));
		snprintf(__port, sizeof(__port), "%d", u->port);

		if (show_all || (show_busy && u->status == GG_STATUS_BUSY) || (show_active && u->status == GG_STATUS_AVAIL) || (show_inactive && u->status == GG_STATUS_NOT_AVAIL)) {
			my_printf(tmp, format_user(u->uin), __ip, __port);
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
	char sender[50], *msg = NULL;
	uin_t uin;
	int free_msg = 0;

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

        if ((u = find_user(uin, NULL)))
                snprintf(sender, sizeof(sender), "%s/%lu", u->comment, u->uin);
        else
                snprintf(sender, sizeof(sender), "%lu", uin);

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

        put_log(uin, "<< %s %s (%s)\n%s\n", (!strcasecmp(name, "chat")) ?
                "Rozmowa do" : "Wiadomo¶æ do", sender, full_timestamp(),
                msg);

	add_send_nick(params[0]);
	iso_to_cp(msg);
	gg_send_message(sess, (!strcasecmp(name, "msg")) ? GG_CLASS_MSG : GG_CLASS_CHAT, uin, msg);

	if (free_msg)
		free(msg);

	unidle();

	return 0;
}

COMMAND(command_save)
{
	if (!write_userlist(NULL) && !write_config(NULL)) {
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
	} else {
		if (!read_theme(params[0], 1)) {
			reset_theme_cache();
			if (!in_autoexec)
				my_printf("theme_loaded", params[0]);
		} else
			my_printf("error_loading_theme", strerror(errno));
	}
	
	return 0;
}

COMMAND(command_set)
{
	struct variable *v = variables;
	int unset = 0;

	if (params[0] && params[0][0] == '-') {
		unset = 1;
		params[0]++;
	}

	if (!params[1] && !unset) {
		while (v->name) {
			if ((!params[0] || !strcasecmp(params[0], v->name)) && v->display != 2) {
				if (v->type == VAR_STR) {
					char *tmp = *(char**)(v->ptr);
					
					if (!tmp)
						tmp = "(brak)";
					if (!v->display)
						tmp = "(...)";
					my_printf("variable", v->name, tmp);
				} else {
					char buf[16];

					snprintf(buf, sizeof(buf) - 1, "%d", *(int*)(v->ptr));
					if (!v->display)
						strcpy(buf, "(...)");
					my_printf("variable", v->name, buf);
				}
			}

			v++;
		}
	} else {
		reset_theme_cache();
		switch (set_variable(params[0], params[1])) {
			case 0:
				if (!in_autoexec) {
					my_printf("variable", params[0], params[1]);
					config_changed = 1;
				}
				break;
			case -1:
				my_printf("variable_not_found", params[0]);
				break;
			case -2:
				my_printf("variable_invalid", params[0]);
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

	if ((u = find_user(0, params[0]))) {
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
	gg_free_session(sess);
	return -1;
}

COMMAND(command_test_send)
{
	struct gg_event *e = malloc(sizeof(struct gg_event));
	
	e->type = GG_EVENT_MSG;
	e->event.msg.sender = get_uin(params[0]);
	e->event.msg.message = strdup(params[1]);
	
	handle_msg(e);

	return 0;
}

COMMAND(command_test_add)
{
	if (params[0])
		add_send_nick(params[0]);

	return 0;
}

COMMAND(command_register)
{

	if (!params[0] || !params[1]) {
		my_printf("not_enough_params");
		return 0;
	}
	
	if (!(reg_req = gg_register(params[0], params[1], 1))) {
		my_printf("register_failed", strerror(errno));
		return 0;
	}
	
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

char **split_params(char *line, int count)
{
	char **res;
	char *p = line;
	int i;

	if (count == -1) {
		char *q;

		for (count = 1, q = line; *q; q++)
			if (isspace(*q))
				count++;
	}

	if (!(res = malloc((count + 2) * sizeof(char*))))
		return NULL;

	for (i = 0; i < count + 2; i++)
		res[i] = 0;
	
	if (!strcmp(strip_spaces(line), ""))
		return res;
	
	if (count < 2) {
		res[0] = p;
		return res;
	}
	
	for (i = 0; *p && i < count; i++) {
		while (isspace(*p))
			p++;

		res[i] = p;
		
		while (*p && !isspace(*p))
			p++;

		if (!*p)
			break;

		if (i != count - 1)
			*p++ = 0;

		while (isspace(*p))
			p++;
	}

	return res;
}

int execute_line(char *line)
{
	char *cmd = NULL, *tmp, *p = NULL, short_cmd[2] = ".", *last_name = NULL, *last_params = NULL;
	struct command *c;
	int (*last_abbr)(char *, char **) = NULL;
	int abbrs = 0;
	
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
		int res;

		c--;
			
		par = split_params(p, strlen(last_params));
		res = (last_abbr)(last_name, par);
		free(par);

		return res;
	}

	if (strcmp(cmd, ""))
		my_printf("unknown_command", cmd);
	
	return 0;
}

