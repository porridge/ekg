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

/* podstawny ewentualnie brakuj±ce funkcje i definicje readline */

extern void rl_extend_line_buffer(int len);
extern char **completion_matches();

#ifndef HAVE_RL_BIND_KEY_IN_MAP
int rl_bind_key_in_map(int key, void *function, void *keymap)
{
	return -1;
}
#endif

#ifndef HAVE_RL_GET_SCREEN_SIZE
int rl_get_screen_size(int *columns, int *lines)
{
	*columns = 80;
	*lines = 24;
	return 0;
}
#endif

#ifndef HAVE_RL_FILENAME_COMPLETION_FUNCTION
void *rl_filename_completion_function = NULL;
#endif

#ifndef HAVE_RL_SET_PROMPT
int rl_set_prompt(const char *foo)
{
	return -1;
}
#endif

#ifndef HAVE_RL_SET_KEY
int rl_set_key(const char *key, void *function, void *keymap)
{
	return -1;
}
#endif

struct window {
	int id, act;
	char *query_nick;
	char *line[MAX_LINES_PER_SCREEN];
};

/* deklaracje funkcji interfejsu */
static void ui_readline_loop();
static void ui_readline_print(const char *target, int separate, const char *line);
static void ui_readline_beep();
static int ui_readline_event(const char *event, ...);
static void ui_readline_deinit();

static int in_readline = 0, no_prompt = 0, pager_lines = -1, screen_lines = 24, screen_columns = 80;
static list_t windows = NULL;
struct window *window_current = NULL;

/* kod okienek napisany jest na podstawie ekg-windows nilsa */
static struct window *window_add();
static int window_remove(int id);
static int window_switch(int id);
static int window_refresh();
static int window_write(int id, const char *line);
static void window_clear();
static void window_list();
static int window_make_query(const char *nick);
static void window_free();
static struct window *window_find(int id);
static int window_find_query(const char *qnick);
static char *window_activity();

static int bind_sequence(const char *seq, const char *command, int quiet);
static int bind_seq_list();
static int bind_handler_window(int a, int key);

/*
 * sigcont_handler()
 *
 * os³uguje powrót z t³a poleceniem ,,fg'', ¿eby od¶wie¿yæ ekran.
 */
static void sigcont_handler()
{
	rl_forced_update_display();
	signal(SIGCONT, sigcont_handler);
}

/*
 * sigint_handler()
 *
 * obs³uguje wci¶niêcie Ctrl-C.
 */
static void sigint_handler()
{
	rl_delete_text(0, rl_end);
	rl_point = rl_end = 0;
	putchar('\n');
	rl_forced_update_display();
	signal(SIGINT, sigint_handler);
}

/*
 * sigwinch_handler()
 *
 * obs³uguje zmianê rozmiaru okna.
 */
static void sigwinch_handler()
{
	rl_get_screen_size(&screen_lines, &screen_columns);
#ifdef SIGWINCH
	signal(SIGWINCH, sigwinch_handler);
#endif
}

/*
 * my_getc()
 *
 * pobiera znak z klawiatury obs³uguj±c w miêdzyczasie sieæ.
 */
static int my_getc(FILE *f)
{
	ekg_wait_for_key();
	return rl_getc(f);
}

static char *command_generator(char *text, int state)
{
	static int len;
	static list_t l;
	int slash = 0;

	if (*text == '/') {
		slash = 1;
		text++;
	}

	if (!*rl_line_buffer) {
		if (state)
			return NULL;
		if (send_nicks_count < 1)
			return saprintf((window_current->query_nick) ? "/%s" : "%s", (config_tab_command) ? config_tab_command : "msg");
		send_nicks_index = (send_nicks_count > 1) ? 1 : 0;

		return saprintf((window_current->query_nick) ? "/%s %s" : "%s %s", (config_tab_command) ? config_tab_command : "chat", send_nicks[0]);
	}

	if (!state) {
		l = commands;
		len = strlen(text);
	}

	for (; l; l = l->next) {
		struct command *c = l->data;

		if (!strncasecmp(text, c->name, len)) {
			l = l->next;
			return (window_current->query_nick) ? saprintf("/%s", c->name) : xstrdup(c->name);
		}
	}

	return NULL;
}

static char *known_uin_generator(char *text, int state)
{
	static list_t l;
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
	static list_t l;
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
	static list_t l;
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
	char *params = NULL;
	int word = 0, i, abbrs = 0;
	CPFunction *func = known_uin_generator;
	list_t l;
	static int my_send_nicks_count = 0;

	if (start) {
		char *tmp = rl_line_buffer, *cmd = (config_tab_command) ? config_tab_command : "chat";
		int slash = 0;

		if (*tmp == '/') {
			slash = 1;
			tmp++;
		}

		if (!strncasecmp(tmp, cmd, strlen(cmd)) && tmp[strlen(cmd)] == ' ') {
			word = 0;
			for (i = 0; i < strlen(rl_line_buffer); i++) {
				if (isspace(rl_line_buffer[i]))
					word++;
			}
			if (word == 2 && isspace(rl_line_buffer[strlen(rl_line_buffer) - 1])) {
				if (send_nicks_count != my_send_nicks_count) {
					my_send_nicks_count = send_nicks_count;
					send_nicks_index = 0;
				}

				if (send_nicks_count > 0) {
					char buf[100];

					snprintf(buf, sizeof(buf), "%s%s %s ", (slash) ? "/" : "", cmd, send_nicks[send_nicks_index++]);
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

		for (l = commands; l; l = l->next) {
			struct command *c = l->data;
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
						func = rl_filename_completion_function;
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
static void ui_readline_print(const char *target, int separate, const char *line)
{
        int old_end = rl_end, id = 0;
	char *old_prompt = NULL;
	
	/* znajd¼ odpowiednie okienko i ewentualnie je utwórz */
	if (target && separate)
		id = window_find_query(target);

	if (config_make_window > 0 && !id && strncmp(target, "__", 2) && separate)
		id = window_make_query(target);
	
	/* je¶li nie piszemy do aktualnego, to zapisz do bufora i wyjd¼ */
        if (id && id != window_current->id) {
                window_write(id, line);
                /* XXX trzeba jeszcze waln±æ od¶wie¿enie prompta */
                return;
        }

	/* je¶li mamy ukrywaæ wszystko, wychodzimy */
	if (pager_lines == -2)
		return;

	window_write(window_current->id, line);

	/* ukryj prompt, je¶li jeste¶my w trakcie readline */
        if (in_readline) {
		old_prompt = xstrdup(rl_prompt);
                rl_end = 0;
		rl_set_prompt("");
		printf("\r\033[K");
        }

	printf("%s", line);

	if (pager_lines >= 0) {
		const char *p;

		for (p = line; *p; p++)
			if (*p == '\n')
				pager_lines++;

		if (pager_lines >= screen_lines - 2) {
			char *tmp;
			const char *prompt = format_find("readline_more");
			
			in_readline++;
		        rl_set_prompt(prompt);
			pager_lines = -1;
			tmp = readline(prompt);
			in_readline--;
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
	
	/* je¶li jeste¶my w readline, poka¿ z powrotem prompt */
        if (in_readline) {
                rl_end = old_end;
		rl_set_prompt(old_prompt);
		xfree(old_prompt);
		rl_forced_update_display();
        }
}

/*
 * ui_readline_beep()
 *
 * wydaje d¼wiêk na konsoli.
 */
static void ui_readline_beep()
{
	printf("\a");
	fflush(stdout);
}

/*
 * current_prompt()
 *
 * zwraca wska¼nik aktualnego prompta. statyczny bufor, nie zwalniaæ.
 */
static const char *current_prompt()
{
	static char buf[80];
	const char *prompt = buf;
	int count = list_count(windows);
	char *tmp, *act = window_activity();

        if (window_current->query_nick) {
		if (count > 1 || window_current->id != 1) {
			if (act) {
				tmp = format_string(format_find("readline_prompt_query_win_act"), window_current->query_nick, itoa(window_current->id), act);
				xfree(act);
			} else
				tmp = format_string(format_find("readline_prompt_query_win"), window_current->query_nick, itoa(window_current->id));
		} else
			tmp = format_string(format_find("readline_prompt_query"), window_current->query_nick, NULL);
		strncpy(buf, tmp, sizeof(buf) - 1);
		xfree(tmp);
        } else {
		char *format_win = "readline_prompt_win", *format_nowin = "readline_prompt", *format_win_act = "readline_prompt_win_act";
			
		if (away == 1 || away == 3) {
			format_win = "readline_prompt_away_win";
			format_nowin = "readline_prompt_away";
			format_win_act = "readline_prompt_away_win_act";
		}

		if (away == 2 || away == 5) {
			format_win = "readline_prompt_invisible_win";
			format_nowin = "readline_prompt_invisible";
			format_win_act = "readline_prompt_invisible_win_act";
		}
		
		if (count > 1 || window_current->id != 1) {
			if (act) {
				tmp = format_string(format_find(format_win_act), itoa(window_current->id), act);
				xfree(act);
			} else
				tmp = format_string(format_find(format_win), itoa(window_current->id));
			strncpy(buf, tmp, sizeof(buf) - 1);
			xfree(tmp);
		} else
			strncpy(buf, format_find(format_nowin), sizeof(buf) - 1);
        }

        if (no_prompt)
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
	const char *prompt = current_prompt();
        char *res, *tmp;

        in_readline = 1;
	rl_set_prompt(prompt);
        res = readline(prompt);
        in_readline = 0;

	tmp = saprintf("%s%s\n", prompt, (res) ? res : "");
        window_write(window_current->id, tmp);
	xfree(tmp);

        return res;
}

/*
 * ui_readline_init()
 *
 * inicjalizacja interfejsu readline.
 */
void ui_readline_init()
{
	char c;

        window_current = window_add();
        window_refresh();

	ui_print = ui_readline_print;
	ui_loop = ui_readline_loop;
	ui_beep = ui_readline_beep;
	ui_event = ui_readline_event;
	ui_deinit = ui_readline_deinit;
		
	rl_initialize();
	rl_getc_function = my_getc;
	rl_readline_name = "gg";
	rl_attempted_completion_function = (CPPFunction *) my_completion;
	rl_completion_entry_function = (void*) empty_generator;

	rl_set_key("\033[[A", binding_help, emacs_standard_keymap);
	rl_set_key("\033OP", binding_help, emacs_standard_keymap);
	rl_set_key("\033[11~", binding_help, emacs_standard_keymap);
	rl_set_key("\033[M", binding_help, emacs_standard_keymap);
	rl_set_key("\033[[B", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033OQ", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033[12~", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033[N", binding_quick_list, emacs_standard_keymap);
	
	rl_set_key("\033[24~", binding_toggle_debug, emacs_standard_keymap);

	for (c = '0'; c <= '9'; c++)
		rl_bind_key_in_map(c, bind_handler_window, emacs_meta_keymap);
	
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
static void ui_readline_deinit()
{
	if (ekg_segv_handler)
		return;

	window_free();
}

/*
 * ui_readline_loop()
 *
 * g³ówna pêtla programu. wczytuje dane z klawiatury w miêdzyczasie
 * obs³uguj±c sieæ i takie tam.
 */
static void ui_readline_loop()
{
	for (;;) {
		char *line = my_readline();

		/* je¶li wci¶niêto Ctrl-D i jeste¶my w query, wyjd¼my */
		if (!line && window_current->query_nick) {
			ui_event("command", "query", NULL);
			continue;
		}

		/* je¶li wci¶niêto Ctrl-D, to zamknij okienko */
		if (!line && list_count(windows) > 1) {
			window_remove(window_current->id);
			continue;
		}

		if (!line) {
			if (config_ctrld_quits)	
				break;
			else {
				printf("\n");
				continue;
			}
		}

		if (strlen(line) > 1 && line[strlen(line) - 1] == '\\' && line[strlen(line) - 2] == ' ') {
			string_t s = string_init(NULL);

			line[strlen(line) - 1] = 0;

			string_append(s, line);

			free(line);

			no_prompt = 1;
			rl_bind_key(9, rl_insert);

			while ((line = my_readline())) {
				if (!strcmp(line, ".")) {
					free(line);
					break;
				}
				string_append(s, line);
				string_append(s, "\r\n");
				free(line);
			}

			rl_bind_key(9, rl_complete);

			no_prompt = 0;

			if (!line) {
				printf("\n");
				string_free(s, 1);
				continue;
			}

			line = string_free(s, 0);
		}
		
		/* je¶li linia nie jest pusta, dopisz do historii */
		if (*line)
			add_history(line);
		
		pager_lines = 0;
		
		command_exec(window_current->query_nick, line);

		pager_lines = -1;

		xfree(line);
	}

	if (!batch_mode && !quit_message_send) {
		putchar('\n');
		print("quit");
		putchar('\n');
		quit_message_send = 1;
	}

}

/*
 * ui_readline_event()
 *
 * obs³uga zdarzeñ wysy³anych z ekg do interfejsu.
 */
static int ui_readline_event(const char *event, ...)
{
	va_list ap;
	int result = 0;

	va_start(ap, event);
	
        if (!strcmp(event, "variable_changed")) {
		char *name = va_arg(ap, char*);

		if (!strcasecmp(name, "sort_windows") && config_sort_windows) {
			list_t l;
			int id = 1;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				w->id = id++;
			}
		}
	}

	if (!strcasecmp(event, "command")) {
		char *command = va_arg(ap, char*);

		if (!strcasecmp(command, "query")) {
			char *param = va_arg(ap, char*);	
			
			if (!param && !window_current->query_nick)
				goto cleanup;

			if (param) {
				int id;

				if ((id = window_find_query(param))) {
					print("query_exist", param, itoa(id));
					return 1;
				}
				
				print("query_started", param);
				xfree(window_current->query_nick);
				window_current->query_nick = xstrdup(param);
			} else {
				printf("\n");	/* XXX brzydkie */
				print("query_finished", window_current->query_nick);
				xfree(window_current->query_nick);
				window_current->query_nick = NULL;
			}

			result = 1;
		}

		if (!strcasecmp(command, "window")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*);

			if (!p1) {
		                print("not_enough_params", "window");
				result = 1;
				goto cleanup;
		        }

		        if (!strcasecmp(p1, "new")) {
		                window_add();
 
			} else if (!strcasecmp(p1, "next")) {
		                window_switch(window_current->id + 1);

			} else if (!strcasecmp(p1, "prev")) {
		                window_switch(window_current->id - 1);
				
		        } else if (!strcasecmp(p1, "kill")) {
		                int id = (p2) ? atoi(p2) : window_current->id;

				window_remove(id);
				
		        } else if (!strcasecmp(p1, "switch")) {
		                if (!p2)
		                        print("not_enough_params", "window");
		                else
		                	window_switch(atoi(p2));

			} else if (!strcasecmp(p1, "refresh")) {
		                window_refresh();

			} else if (!strcasecmp(p1, "clear")) {
		                window_clear();
				window_refresh();

			} else if (!strcasecmp(p1, "list")) {
				window_list();
				
			} else
				print("window_invalid");

			result = 1;
		}

		if (!strcasecmp(command, "bind")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*), *p3 = va_arg(ap, char*);
			
			if (p1 && (!strcasecmp(p1, "-a") || !strcasecmp(p1, "--add") ||!strcasecmp(p1, "--add-quiet"))) {
				if (!p2 || !p3)
					print("not_enough_params", "bind");
				else
					bind_sequence(p2, p3, (!strcasecmp(p1, "--add-quiet")) ? 1 : 0);
			
			} else if (p1 && (!strcasecmp(p1, "-d") || !strcasecmp(p1, "--del"))) {
				if (!p2)
					print("not_enough_params", "bind");
				else
					bind_sequence(p2, NULL, 0);
			
			} else 
				bind_seq_list();

			result = 1;
		}
	}

cleanup:
	va_end(ap);

	return result;
}

/*
 * window_find()
 *
 * szuka struct window dla okna o podanym numerku.
 *
 *  - id - numer okna.
 *
 * struct window dla danego okna.
 */
static struct window *window_find(int id)
{
	list_t l;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (w->id == id)
			return w;
	}

	return NULL;
}

/*
 * window_add()
 *
 * tworzy nowe okno.
 *
 * zwraca zaalokowan± struct window.
 */
static struct window *window_add()
{
        struct window w;
	int id = 1;

	/* wyczy¶æ. */
	memset(&w, 0, sizeof(w));

	/* znajd¼ pierwszy wolny id okienka. */
	while (window_find(id))
		id++;
	w.id = id;

	/* dopisz, zwróæ. */
        return list_add(&windows, &w, sizeof(w));
}

/*
 * window_remove()
 *
 * usuwa okno o podanym numerze.
 */
static int window_remove(int id)
{
	struct window *w;
	int i;

	/* je¶li zosta³o jedno okienko, nie usuwaj niczego. */
        if (list_count(windows) < 2) {
                print("window_no_windows");
                return -1;
        }

	/* je¶li nie ma takiego okienka, id¼ sobie. */
	if (!(w = window_find(id))) {
        	print("window_noexist");
		return -1;
	}

	/* i sortujemy okienka, w razie potrzeby... */
        if (config_sort_windows) {
		list_t l;
		int wid = w->id;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;
			
			if (w->id > wid)
				w->id--;
		}
	}

	/* je¶li usuwano aktualne okienko, nie kombinuj tylko ustaw 1-sze. */
	if (window_current == w) {
		struct window *newwin = windows->data;

		/* je¶li usuwane jest pierwszy, we¼ drugie. */
		if (newwin == w)
			newwin = windows->next->data;
		
		window_switch(newwin->id);
	}

	/* usuñ dane zajmowane przez okno. */
	for (i = 0; i < MAX_LINES_PER_SCREEN; i++) {
		xfree(w->line[i]);
		w->line[i] = NULL;
	}
	xfree(w->query_nick);
	w->query_nick = NULL;
	
	list_remove(&windows, w, 1);

        return 0;
}

/*
 * window_switch()
 *
 * prze³±cza do okna o podanym id.
 */
static int window_switch(int id)
{
	struct window *w = window_find(id);

	if (!w) {
		print("window_noexist");
		return -1;
	}

	window_current = w;
	w->act = 0;
	window_refresh();
#ifdef HAVE_RL_SET_PROMPT
	rl_set_prompt(current_prompt());
#else
	rl_expand_prompt(current_prompt());
#endif
	rl_initialize();

	return 0;
}

/*
 * window_refresh()
 *
 * wy¶wietla ponownie zawarto¶æ okna.
 *
 * XXX podpi±æ pod Ctrl-L.
 */
static int window_refresh()
{
        int i;

        printf("\033[H\033[J"); /* XXX */

	for (i = 0; i < MAX_LINES_PER_SCREEN; i++)
		if (window_current->line[i])
			printf("%s", window_current->line[i]);

        return 0;
}

/*
 * window_write()
 *
 * dopisuje liniê do bufora danego okna.
 */
static int window_write(int id, const char *line)
{
        struct window *w = window_find(id);
        int i = 1;

        if (!line || !w)
                return -1;

	/* je¶li ca³y bufor zajêty, zwolnij pierwsz± liniê i przesuñ do góry */
	if (w->line[MAX_LINES_PER_SCREEN - 1]) {
		xfree(w->line[0]);
		for (i = 1; i < MAX_LINES_PER_SCREEN; i++)
			w->line[i - 1] = w->line[i];
		w->line[MAX_LINES_PER_SCREEN - 1] = NULL;
	}

	/* znajd¼ pierwsz± woln± liniê i siê wpisz. */
	for (i = 0; i < MAX_LINES_PER_SCREEN; i++)
		if (!w->line[i]) {
			w->line[i] = xstrdup(line);
			break;
		}

	if (w != window_current) {
		w->act = 1;
#ifdef HAVE_RL_SET_PROMPT
		rl_set_prompt(current_prompt());
#else
		rl_expand_prompt(current_prompt());
#endif
	}
	
        return 0;
}

/*
 * window_clear()
 *
 * czy¶ci zawarto¶æ aktualnego okna.
 */
static void window_clear()
{
        int i;

        for (i = 0; i < MAX_LINES_PER_SCREEN; i++) {
		xfree(window_current->line[i]);
                window_current->line[i] = NULL;
	}
}

/*
 * window_find_query()
 *
 * znajduje id okna, w którym prowadzona jest rozmowa z dan± osob±. je¶li
 * nie ma takiego, zwraca zero.
 */
static int window_find_query(const char *nick)
{
        list_t l;

        if (!nick)
                return 0;

        for (l = windows; l; l = l->next) {
                struct window *w = l->data;

		if (w->query_nick && !strcmp(w->query_nick, nick))
			return w->id;
        }

        return 0;
}

/*
 * window_list()
 *
 * wy¶wietla listê okien.
 */
static void window_list()
{
	list_t l;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (w->query_nick)
			print("window_list_query", itoa(w->id), w->query_nick);
		else
			print("window_list_nothing", itoa(w->id));
	}
}		

/*
 * window_make_query()
 *
 * tworzy nowe okno rozmowy w zale¿no¶ci od aktualnych ustawieñ.
 */
static int window_make_query(const char *nick)
{
	/* szuka pierwszego wolnego okienka i je zajmuje */
	if (config_make_window == 1) {
		list_t l;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;
			
			if (!w->query_nick) {
				w->query_nick = xstrdup(nick);
				
				if (w == window_current) {
					print("query_started", nick);
#ifdef HAVE_RL_SET_PROMPT
					rl_set_prompt(current_prompt());
#else
					rl_expand_prompt(current_prompt());
#endif									
				} else
					print("window_id_query_started", itoa(w->id), nick);
				
				return w->id;
			}
		}
	}
	
	if (config_make_window == 1 || config_make_window == 2) {
		struct window *w;

		if (!(w = window_add()))
			return 0;
		
		w->query_nick = xstrdup(nick);
		
		print("window_id_query_started", itoa(w->id), nick);
			
		return w->id;
	}

	return 0;
}

/*
 * window_free()
 *
 * zwalnia pamiêæ po wszystkich strukturach okien.
 */
static void window_free()
{
	list_t l;

	window_current = NULL;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;
		int i;

		xfree(w->query_nick);
		w->query_nick = NULL;

		for (i = 0; i < MAX_LINES_PER_SCREEN; i++) {
			xfree(w->line[i]);
			w->line[i] = NULL;
		}
	}

	list_destroy(windows, 1);
	windows = NULL;
}
/*
 * window_activity()
 *
 * zwraca string z actywnymi oknami 
 */
static char *window_activity() 
{
	string_t s = string_init("");
	int first = 1;
	list_t l;
	char *act = NULL;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;
		
		if (w->act) {
			if (!first)
				string_append_c(s, ',');
			string_append(s, itoa(w->id));
			first = 0;
		}
	}

	if (!first)
		act = strdup(s->str);
	
	string_free(s, 1);
	
	return act;
}
		
/*
 * bind_find_command()
 *
 * szuka komendy, któr± nale¿y wykonaæ dla danego klawisza.
 */
static char *bind_find_command(const char *seq)
{
	list_t l;

	if (!seq)
		return NULL;
	
	for (l = sequences; l; l = l->next) {
		struct sequence *s = l->data;

		if (s->seq && !strcasecmp(s->seq, seq))
			return s->command;
	}

	return NULL;
}

/*
 * bind_handler_ctrl()
 *
 * obs³uguje klawisze Ctrl-A do Ctrl-Z, wywo³uj±c przypisane im akcje.
 */
static int bind_handler_ctrl(int a, int key)
{
	char *tmp = saprintf("Ctrl-%c", 'A' + key - 1);
	int foo = pager_lines;

	if (foo < 0)
		pager_lines = 0;
	command_exec(NULL, bind_find_command(tmp));
	if (foo < 0)
		pager_lines = foo;
	xfree(tmp);

	return 0;
}

/*
 * bind_handler_alt()
 *
 * obs³uguje klawisze z Altem, wywo³uj±c przypisane im akcje.
 */
static int bind_handler_alt(int a, int key)
{
	char *tmp = saprintf("Alt-%c", key);
	int foo = pager_lines;

	if (foo < 0)
		pager_lines = 0;
	command_exec(NULL, bind_find_command(tmp));
	if (foo < 0)
		pager_lines = foo;
	xfree(tmp);

	return 0;
}

/*
 * bind_handler_window()
 *
 * obs³uguje klawisze Alt-1 do Alt-0, zmieniaj±c okna na odpowiednie.
 */
static int bind_handler_window(int a, int key)
{
	if (key > '0' && key <= '9')
		window_switch(key - '0');
	else
		window_switch(10);

	return 0;
}
		
static int bind_sequence(const char *seq, const char *command, int quiet)
{
	char *nice_seq = NULL;
	
	if (!seq)
		return -1;

	if (command && bind_find_command(seq)) {
		if (!quiet)
			print("bind_seq_exist", seq);

		return -1;
	}
	
	if (!strncasecmp(seq, "Ctrl-", 5) && strlen(seq) == 6 && isalpha(seq[5])) {
		int key = CTRL(toupper(seq[5]));

		if (command) {
			rl_bind_key(key, bind_handler_ctrl);
			nice_seq = xstrdup(seq);
			nice_seq[0] = toupper(nice_seq[0]);
			nice_seq[5] = toupper(nice_seq[5]);
		} else
			rl_unbind_key(key);

	} else if (!strncasecmp(seq, "Alt-", 4) && strlen(seq) == 5) {

		if (command) {
			rl_bind_key_in_map(seq[4], bind_handler_alt, emacs_meta_keymap);
			nice_seq = xstrdup(seq);
			nice_seq[0] = toupper(nice_seq[0]);
			nice_seq[4] = toupper(nice_seq[4]);
		} else
			rl_unbind_key_in_map(seq[4], emacs_meta_keymap);
		
	} else {
		if (!quiet)
			print("bind_seq_incorrect", seq);

		return -1;
	}

	if (command) {
		struct sequence s;
		
		s.seq = nice_seq;
		s.command = xstrdup(command);

		list_add(&sequences, &s, sizeof(s));

		if (!quiet) {
			print("bind_seq_add", s.seq);
			config_changed = 1;
		}
	} else {
		list_t l;

		for (l = sequences; l; l = l->next) {
			struct sequence *s = l->data;

			if (s->seq && !strcasecmp(s->seq, seq)) {
				list_remove(&sequences, s, 1);
				if (!quiet) {
					print("bind_seq_remove", seq);
					config_changed = 1;
				}
				return 0;
			}
		}
	}

	return 1;
}

/*
 * bind_seq_list()
 *
 * wy¶wietla listê przypisanych komend.
 */
static int bind_seq_list() 
{
	list_t l;
	int count = 0;

	for (l = sequences; l; l = l->next) {
		struct sequence *s = l->data;

		print ("bind_seq_list", s->seq, s->command);
		count++;
	}

	if (!count)
		print("bind_seq_list_empty");
	
	return 0;
}
