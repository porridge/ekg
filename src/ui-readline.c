/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *                          Adam Osuchowski <adwol@polsl.gliwice.pl>
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
#include <stdlib.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <readline.h>
#include <history.h>
#include <stdarg.h>
#include <signal.h>
#include "config.h"
#include "userlist.h"
#include "stuff.h"
#include "commands.h"
#include "xmalloc.h"
#include "themes.h"
#include "vars.h"
#include "ui.h"

static void ui_readline_loop();
static void ui_readline_print(const char *target, const char *line);
static void ui_readline_beep();
static void ui_readline_new_target(const char *target);
static void ui_readline_query(const char *param);
static void ui_readline_deinit();

static int in_readline = 0, no_prompt = 0, pager_lines = -1, screen_lines = 24, screen_columns = 80;
static char *query_nick = NULL;

static void sigcont_handler()
{
	rl_forced_update_display();
	signal(SIGCONT, sigcont_handler);
}

static void sigint_handler()
{
	rl_delete_text(0, rl_end);
	rl_point = rl_end = 0;
	putchar('\n');
	rl_forced_update_display();
	signal(SIGINT, sigint_handler);
}

static void sigwinch_handler()
{
#ifdef HAVE_RL_GET_SCREEN_SIZE
	rl_get_screen_size(&screen_lines, &screen_columns);
#endif
#ifdef SIGWINCH
	signal(SIGWINCH, sigwinch_handler);
#endif
}

/*
 * my_getc()
 *
 * pobiera znak z klawiatury obs³uguj±c w miêdzyczasie sieæ.
 */
int my_getc(FILE *f)
{
	ekg_wait_for_key();
	return rl_getc(f);
}

/*
 * g³upie readline z wersji na wersjê ma inne include'y, grr.
 */
extern void rl_extend_line_buffer(int len);
extern char **completion_matches();

static char *command_generator(char *text, int state)
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
		if (send_nicks_count < 1)
			return xstrdup((query_nick) ? "/msg" : "msg");
		send_nicks_index = (send_nicks_count > 1) ? 1 : 0;

		return saprintf((query_nick) ? "/chat %s" : "chat %s", send_nicks[0]);
	}

	if (!state) {
		index = 0;
		len = strlen(text);
	}

	while ((name = commands[index++].name))
		if (!strncasecmp(text, name, len))
			return (query_nick) ? saprintf("/%s", name) : xstrdup(name);

	return NULL;
}

static char *known_uin_generator(char *text, int state)
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
			return xstrdup(u->display);
	}

	return NULL;
}

static char *unknown_uin_generator(char *text, int state)
{
	static int index = 0, len;

	if (!state) {
		index = 0;
		len = strlen(text);
	}

	while (index < send_nicks_count)
		if (isdigit(send_nicks[index++][0]))
			if (!strncasecmp(text, send_nicks[index - 1], len))
				return xstrdup(send_nicks[index - 1]);

	return NULL;
}

static char *variable_generator(char *text, int state)
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
		
		if (v->type == VAR_FOREIGN)
			continue;

		if (*text == '-') {
			if (!strncasecmp(text + 1, v->name, len - 1))
				return saprintf("-%s", v->name);
		} else {
			if (!strncasecmp(text, v->name, len))
				return xstrdup(v->name);
		}
	}

	return NULL;
}

static char *ignored_uin_generator(char *text, int state)
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
				return xstrdup(itoa(i->uin));
		} else {
			if (u->display && !strncasecmp(text, u->display, len))
				return xstrdup(u->display);
		}
	}

	return NULL;
}

static char *dcc_generator(char *text, int state)
{
	char *commands[] = { "close", "get", "send", "show", "voice", NULL };
	static int len, i;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while (commands[i]) {
		if (!strncasecmp(text, commands[i], len))
			return xstrdup(commands[i++]);
		i++;
	}

	return NULL;
}

static char *empty_generator(char *text, int state)
{
	return NULL;
}

static char **my_completion(char *text, int start, int end)
{
	struct command *c;
	char *params = NULL;
	int word = 0, i, abbrs = 0;
	CPFunction *func = known_uin_generator;

	if (start) {
		if (!strncasecmp(rl_line_buffer, "chat ", 5) || !strncasecmp(rl_line_buffer, "/chat ", 6)) {
			word = 0;
			for (i = 0; i < strlen(rl_line_buffer); i++) {
				if (isspace(rl_line_buffer[i]))
					word++;
			}
			if (word == 2 && isspace(rl_line_buffer[strlen(rl_line_buffer) - 1])) {
				if (send_nicks_count > 0) {
					char buf[100];

					snprintf(buf, sizeof(buf), "chat %s ", send_nicks[send_nicks_index++]);
					rl_extend_line_buffer(strlen(buf));
					strcpy(rl_line_buffer, buf);
					rl_end = strlen(buf);
					rl_point = rl_end;
					rl_redisplay();
				}

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
			char *cmd = (*rl_line_buffer == '/') ? rl_line_buffer + 1 : rl_line_buffer;

			if (!strncasecmp(cmd, c->name, len) && isspace(cmd[len])) {
				params = c->params;
				abbrs = 1;
				break;
			}
			
			for (len = 0; cmd[len] && cmd[len] != ' '; len++);

			if (!strncasecmp(cmd, c->name, len)) {
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
#ifdef HAVE_RL_FILENAME_COMPLETION_FUNCTION
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

/*
 * ui_readline_print()
 *
 * wy¶wietla dany tekst na ekranie, uwa¿aj±c na trwaj±ce w danych chwili
 * readline().
 */
void ui_readline_print(const char *target, const char *line)
{
        int old_end = rl_end, i;
	char *old_prompt = rl_prompt;

        if (in_readline) {
                rl_end = 0;
                rl_prompt = "";
                rl_redisplay();
                printf("\r");
                for (i = 0; i < strlen(old_prompt); i++)
                        printf(" ");
                printf("\r");
        }

	printf("%s", line);

	if (pager_lines >= 0) {
		const char *p;

		for (p = line; *p; p++)
			if (*p == '\n')
				pager_lines++;

		if (pager_lines > screen_lines - 2) {
			char *tmp;
			const char *prompt = find_format("readline_more");
			
			in_readline = 1;
#ifdef HAVE_RL_SET_PROMPT
		        rl_set_prompt(prompt);
#endif				
			pager_lines = -1;
			tmp = readline(prompt);
			in_readline = 0;
			if (tmp) {
				free(tmp);
				pager_lines = 0;
			} else {
				printf("\n");
				pager_lines = -2;
			}
			printf("\033[A\033[K");		/* XXX */
		}
	}
	
        if (in_readline) {
                rl_end = old_end;
                rl_prompt = old_prompt;
		rl_forced_update_display();
        }
}

void ui_readline_beep()
{
	printf("\a");
}

/*
 * current_prompt()
 *
 * zwraca wska¼nik aktualnego prompta. statyczny bufor, nie zwalniaæ.
 */
static const char *current_prompt()
{
	static char buf[80];	/* g³upio strdup()owaæ wszystko */
	const char *prompt;
	
	if (query_nick) {
		if ((prompt = format_string(find_format("readline_prompt_query"), query_nick, NULL))) {
			strncpy(buf, prompt, sizeof(buf)-1);
			prompt = buf;
		}
	} else {
		switch (away) {
			case 1:
			case 3:
				prompt = find_format("readline_prompt_away");
				break;
			case 2:
			case 5:
				prompt = find_format("readline_prompt_invisible");
				break;
			default:
				prompt = find_format("readline_prompt");
		}
	}

	if (no_prompt || !prompt)
		prompt = "";

	return prompt;
}

/*
 * my_readline()
 *
 * malutki wrapper na readline(), który przygotowuje odpowiedniego prompta
 * w zale¿no¶ci od tego, czy jeste¶my zajêci czy nie i informuje resztê
 * programu, ¿e jeste¶my w trakcie czytania linii i je¶li chc± wy¶wietlaæ,
 * powinny najpierw sprz±tn±æ.
 */
static char *my_readline()
{
        char *res;
	const char *prompt = current_prompt();

        in_readline = 1;
#ifdef HAVE_RL_SET_PROMPT
	rl_set_prompt(prompt);
#endif
        res = readline(prompt);
        in_readline = 0;

        return res;
}

/*
 * ui_readline_init()
 *
 * inicjalizacja interfejsu readline.
 */
void ui_readline_init()
{
	ui_print = ui_readline_print;
	ui_loop = ui_readline_loop;
	ui_beep = ui_readline_beep;
	ui_new_target = ui_readline_new_target;
	ui_query = ui_readline_query;
	ui_deinit = ui_readline_deinit;
		
	rl_initialize();
	rl_getc_function = my_getc;
	rl_readline_name = "gg";
	rl_attempted_completion_function = (CPPFunction *) my_completion;
	rl_completion_entry_function = (void*) empty_generator;
#ifdef HAVE_RL_SET_KEY
	rl_set_key("\033[[A", binding_help, rl_get_keymap());
	rl_set_key("\033OP", binding_help, rl_get_keymap());
	rl_set_key("\033[11~", binding_help, rl_get_keymap());
	rl_set_key("\033[M", binding_help, rl_get_keymap());
	rl_set_key("\033[[B", binding_quick_list, rl_get_keymap());
	rl_set_key("\033OQ", binding_quick_list, rl_get_keymap());
	rl_set_key("\033[12~", binding_quick_list, rl_get_keymap());
	rl_set_key("\033[N", binding_quick_list, rl_get_keymap());
	
	rl_set_key("\033[24~", binding_toggle_debug, rl_get_keymap());
#endif
	
	signal(SIGINT, sigint_handler);
	signal(SIGCONT, sigcont_handler);

#ifdef HAVE_RL_GET_SCREEN_SIZE
#  ifdef SIGWINCH
	signal(SIGWINCH, sigwinch_handler);
#  endif
	rl_get_screen_size(&screen_lines, &screen_columns);
#endif
	if (screen_lines < 1)
		screen_lines = 24;
}

/*
 * ui_readline_deinit()
 *
 * zamyka to, co zwi±zane z interfejsem.
 */
void ui_readline_deinit()
{

}

/*
 * ui_readline_loop()
 *
 * g³ówna pêtla programu. wczytuje dane z klawiatury w miêdzyczasie
 * obs³uguj±c sieæ i takie tam.
 */
void ui_readline_loop()
{
	for (;;) {
		char *line = my_readline();

		if (!line && query_nick) {
			ui_readline_query(NULL);
			continue;
		}

		if (!line)
			break;

		if (strlen(line) > 1 && line[strlen(line) - 1] == '\\' && line[strlen(line) - 2] == ' ') {
			struct string *s = string_init(line);

			free(line);

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
				continue;
			}

			line = string_free(s, 0);
		}

		pager_lines = 0;
		
		if (ekg_execute(query_nick, line)) {
			xfree(line);
			break;
		}

		pager_lines = -1;

		xfree(line);
	}

	if (!batch_mode && !quit_message_send) {
		putchar('\n');
		print("quit");
		putchar('\n');
		quit_message_send = 1;
	}

	if (config_changed) {
		char *line;
		const char *prompt = find_format("config_changed");

		if ((line = readline(prompt))) {
			if (!strcasecmp(line, "tak") || !strcasecmp(line, "yes") || !strcasecmp(line, "t") || !strcasecmp(line, "y")) {
				if (!userlist_write(NULL) && !config_write(NULL))
					print("saved");
				else
					print("error_saving");

			}
			free(line);
		} else
			printf("\n");
	}
}

void ui_readline_new_target(const char *target)
{

}

void ui_readline_query(const char *param)
{
	if (!param && !query_nick)
		return;

	if (param) {
		print("query_started", param);
		free(query_nick);
		query_nick = xstrdup(param);
	} else {
		print("query_finished", query_nick);
		xfree(query_nick);
		query_nick = NULL;
	}
}
