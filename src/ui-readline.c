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
static int ui_readline_event(const char *event, ...);
static void ui_readline_deinit();

static int in_readline = 0, no_prompt = 0, pager_lines = -1, screen_lines = 24, screen_columns = 80, curr_window = 1, windows_count = 0;
struct list *windows = NULL;
struct window *win;

/* kod okienek napisany jest na podstawie ekg-windows nilsa */
static int window_add();
static int window_del(int id);
static int window_switch(int id);
static int window_refresh();
static int window_write(int id, const char *line);
static int window_clear();
static int windows_sort();
static int window_query_id(const char *qnick);

/* a jak ju¿ przy okienkach jeste¶my... */
static void window_01() { if (curr_window == 1) return; window_switch(1); }
static void window_02() { if (curr_window == 2) return; window_switch(2); }
static void window_03() { if (curr_window == 3) return; window_switch(3); }
static void window_04() { if (curr_window == 4) return; window_switch(4); } 
static void window_05() { if (curr_window == 5) return; window_switch(5); }
static void window_06() { if (curr_window == 6) return; window_switch(6); }
static void window_07() { if (curr_window == 7) return; window_switch(7); }
static void window_08() { if (curr_window == 8) return; window_switch(8); }
static void window_09() { if (curr_window == 9) return; window_switch(9); }

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
static int my_getc(FILE *f)
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
			return xstrdup((win->query_nick) ? "/msg" : "msg");
		send_nicks_index = (send_nicks_count > 1) ? 1 : 0;

		return saprintf((win->query_nick) ? "/chat %s" : "chat %s", send_nicks[0]);
	}

	if (!state) {
		index = 0;
		len = strlen(text);
	}

	while ((name = commands[index++].name))
		if (!strncasecmp(text, name, len))
			return (win->query_nick) ? saprintf("/%s", name) : xstrdup(name);

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
static void ui_readline_print(const char *target, const char *line)
{
        int old_end = rl_end, i, win_id;
	char *old_prompt = rl_prompt;

        if ((win->query_nick && target && !strcmp(win->query_nick, target)) || windows_count <= 1)
                win_id = 0;
        else
                win_id = window_query_id(target);

        if (win_id > 0) {
                window_write(win_id, line);
                /* trzeba jeszcze waln±æ od¶wie¿enie prompta */
                return;
        }

	window_write(curr_window, line);

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
	static char buf[80];	/* g³upio strdup()owaæ wszystko */
	const char *prompt;
	static char w[16]; /* stosunkowo du¿y, mo¿na wsadziæ aktywne okna */

	*w = 0;

        if (windows_count > 1)
                sprintf(w, "%d", curr_window);

        if (win->query_nick) {
                if ((prompt = (*w) ? format_string(find_format("readline_prompt_query_win"), win->query_nick, w) : format_string(find_format("readline_prompt_query"), win->query_nick, NULL))) {
                        strncpy(buf, prompt, sizeof(buf)-1);
                        prompt = buf;
                }
        } else {
                switch (away) {
                        case 1:
                        case 3:
                                prompt = (*w) ? format_string(find_format("readline_prompt_away_win"), w) : find_format("readline_prompt_away");
                                break;
                        case 2:
                        case 5:
                                prompt = (*w) ? format_string(find_format("readline_prompt_invisible_win"), w) : find_format("readline_prompt_invisible");
                                break;
                        default:
                                prompt = (*w) ? format_string(find_format("readline_prompt_win"), w, NULL) : find_format("readline_prompt");
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
        char *res, buff[4096];
	const char *prompt = current_prompt();

        in_readline = 1;
#ifdef HAVE_RL_SET_PROMPT
	rl_set_prompt(prompt);
#endif
        res = readline(prompt);
        in_readline = 0;

        snprintf(buff, 4096, "%s%s\n", current_prompt(), res);
        window_write(curr_window, buff);

        return res;
}

/*
 * ui_readline_init()
 *
 * inicjalizacja interfejsu readline.
 */
void ui_readline_init()
{
        window_add();
        win = (struct window *)windows->data;
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

/*#ifdef HAVE_RL_GENERIC_BIND*/
        rl_generic_bind(ISFUNC, "1", (char *)window_01, emacs_meta_keymap);
        rl_generic_bind(ISFUNC, "2", (char *)window_02, emacs_meta_keymap);
        rl_generic_bind(ISFUNC, "3", (char *)window_03, emacs_meta_keymap);
        rl_generic_bind(ISFUNC, "4", (char *)window_04, emacs_meta_keymap);
        rl_generic_bind(ISFUNC, "5", (char *)window_05, emacs_meta_keymap);
        rl_generic_bind(ISFUNC, "6", (char *)window_06, emacs_meta_keymap);
        rl_generic_bind(ISFUNC, "7", (char *)window_07, emacs_meta_keymap);
        rl_generic_bind(ISFUNC, "8", (char *)window_08, emacs_meta_keymap);
        rl_generic_bind(ISFUNC, "9", (char *)window_09, emacs_meta_keymap);
/*#endif*/
	
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

		if (!line && win->query_nick) {
			ui_event("command", "query", NULL);
			continue;
		}

		if (!line)
			break;

		if (strlen(line) > 1 && line[strlen(line) - 1] == '\\' && line[strlen(line) - 2] == ' ') {
			struct string *s = string_init(NULL);

			line[strlen(line) - 1] = 0;

			string_append(s, line);

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

		add_history(line);
		
		pager_lines = 0;
		
		if (ekg_execute(win->query_nick, line)) {
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

}

static int ui_readline_event(const char *event, ...)
{
	va_list ap;
	int result = 0;

	va_start(ap, event);
	
	if (!strcasecmp(event, "command")) {
		char *command = va_arg(ap, char*);

		if (!strcasecmp(command, "query")) {
			char *param = va_arg(ap, char*);	
		
			if (!param && !win->query_nick)
				goto cleanup;

			if (param) {
				struct list *l;

				for (l = windows; l; l = l->next) {
					struct window *w = l->data;

					if (w->query_nick && !strcmp(param, w->query_nick)) {
						char w_id[4];

						sprintf(w_id, "%d", w->id);
						print("query_exist", w->query_nick, w_id);
						return 1;
					}
				}		
						
				print("query_started", param);
				free(win->query_nick);
				win->query_nick = xstrdup(param);
			} else {
				print("query_finished", win->query_nick);
				xfree(win->query_nick);
				win->query_nick = NULL;
			}

			result = 1;
		}

		if (!strcasecmp(command, "window")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*);

			if (!p1) {
		                print("window_not_enough_params");
				result = 1;
				goto cleanup;
		        }

		        if (!strcasecmp(p1, "new")) {
		                if (!window_add())
		                        print("window_add");
 
			} else if (!strcasecmp(p1, "next")) {
		                window_switch(curr_window + 1);

			} else if (!strcasecmp(p1, "prev")) {
		                window_switch(curr_window - 1);
				
		        } else if (!strcasecmp(p1, "kill")) {
		                int id = (p2) ? atoi(p2) : curr_window;

				window_del(id);
				
		        } else if (!strcasecmp(p1, "switch")) {
		                if (!p2)
		                        print("window_not_enough_params");
		                else
		                	window_switch(atoi(p2));

			} else if (!strcasecmp(p1, "refresh")) {
		                window_refresh();

			} else if (!strcasecmp(p1, "clear")) {
		                window_clear();

			} else
				print("window_invalid");

			result = 1;
		}
	}

cleanup:
	va_end(ap);

	return result;
}

static int window_add()
{
        int j;
        struct window w;

        if (windows_count > MAX_WINDOWS) {
                print("windows_max");
                return 1;
        }

        w.id = windows_count + 1;
        w.query_nick = NULL;
        windows_count++;

        for (j = 0; j <= MAX_LINES_PER_SCREEN; j++)
                w.buff.line[j] = NULL;

        list_add(&windows, &w, sizeof(w));

        return 0;
}

static int window_del(int id)
{
        struct list *l;

        if (windows_count <= 1) {
                print("window_no_windows");
                return 1;
        }

        for (l = windows; l; l = l->next) {
                struct window *w = l->data;

                if (w->id == id) {
                        print("window_del");
                        list_remove(&windows, w, 1);
                        windows_sort();
                        windows_count--;
                        if (curr_window == id)
                                window_switch((id > 1 || curr_window > windows_count) ? id-1 : 1);
                        return 0;

                }
        }

        print("window_noexist");

        return 1;
}

static int window_switch(int id)
{
        struct list *l, *tmp = windows;

        for (l = windows; l; l = l->next) {
                struct window *w = l->data;

                if (w->id == id) {
                        windows = l;
                        win = l->data;
                        window_refresh();
                        curr_window = id;
                        window_refresh();
                        windows = tmp;
#ifdef HAVE_RL_SET_PROMPT
			rl_set_prompt(current_prompt());
#else /*#elif HAVE_RL_EXPAND_PROMPT*/
			rl_expand_prompt(current_prompt());
#endif
                        return 0;
                }
        }

        print("window_noexist");

        return 1;
}

static int window_refresh()
{
        int j = 0;

        printf("\033[H\033[J"); /* blah */

        for (j = win->buff.last; j < MAX_LINES_PER_SCREEN; j++) {
                if (win->buff.line[j])
                        printf("%s", win->buff.line[j]);
        }

        for (j=0; j < win->buff.last; j++) {
                if (win->buff.line[j])
                        printf("%s", win->buff.line[j]);
        }

        return 0;
}

static int window_write(int id, const char *line)
{
        int j = 1;
        struct list *l;
        struct window *w = NULL;

        if (!line)
                return(1);

        if (id == curr_window)
                w = win;
        else
                for (l = windows; l; l = l->next) {
                        w = l->data;
                        if (w->id == id)
                                break;
                }

        j = w->buff.last;
        if (w->buff.line[j])
                xfree(w->buff.line[j]);

        w->buff.line[j] = (char*)xmalloc(strlen(line)+2);

        snprintf(w->buff.line[j], strlen(line)+2, "%s", line);

        w->buff.last++;

        if(w->buff.last == MAX_LINES_PER_SCREEN)
                w->buff.last=0;

        return 0;
}

static int window_clear()
{
        int j;

        for (j = 0; j<=MAX_LINES_PER_SCREEN; j++)
                win->buff.line[j] = NULL;

        return 0;
}

static int windows_sort()
{
        struct list *l;
        int id = 1;

        for (l = windows; l; l = l->next) {
                struct window *w = l->data;

                w->id = id++;
        }
        
	return 0;
}

static int window_query_id(const char *qnick)
{
        struct list *l;

        if (!qnick)
                return -1;

        for (l = windows; l; l = l->next) {
                struct window *w = l->data;

                if (!w->query_nick)
                        continue;

                if (!strcmp(w->query_nick, qnick))
                        return w->id;
        }

        return -2;
}

