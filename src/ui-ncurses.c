/* $Id$ */

/*
 *  (C) Copyright 2002 Wojtek Kaniewski <wojtekka@irc.pl>
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

/*
 * roadmap:
 * - okienka,
 * - konfigurowalny statusbar,
 * - mo¿liwo¶æ w³±czenia listy kontaktów po prawej,
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include <signal.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <ctype.h>
#include "config.h"
#include "stuff.h"
#include "commands.h"
#include "userlist.h"
#include "xmalloc.h"
#include "themes.h"
#include "vars.h"
#include "ui.h"

static void ui_ncurses_loop();
static void ui_ncurses_print(const char *target, const char *line);
static void ui_ncurses_beep();
static int ui_ncurses_event(const char *event, ...);
static void ui_ncurses_deinit();

static WINDOW *status = NULL, *input = NULL, *output = NULL;
#define HISTORY_MAX 1000
static char *history[HISTORY_MAX];
static int history_index = 0;
static int lines = 0, y = 0, start = 0;
static char line[1000] = "", *yanked = NULL;
static char **completions = NULL;	/* lista dope³nieñ */

#define output_size (stdscr->_maxy - 1)

static void set_cursor()
{
	if (y == lines) {
		if (start == lines - output_size)
			start++;
		wresize(output, y + 1, 80);
		lines++;
	}
	wmove(output, y, 0);
}

static void ui_ncurses_print(const char *target, const char *line)
{
	const char *p;
	int x = 0;

	set_cursor();

	for (p = line; *p; p++) {
		if (*p == 27) {
			p++;
			if (*p == '[') {
				char *q;
				int a1, a2 = -1;
				p++;
				a1 = strtol(p, &q, 10);
				p = q;
				if (*p == ';') {
					p++;
					a2 = strtol(p, &q, 10);
					p = q;
				}
				if (*p == 'm') {
					if (a1 == 0 && a2 == -1)
						wattrset(output, COLOR_PAIR(7));
					else if (a1 == 1 && a2 == -1)
						wattrset(output, COLOR_PAIR(7) | A_BOLD);
					else if (a2 == -1)
						wattrset(output, COLOR_PAIR(a1 - 30));
					else
						wattrset(output, COLOR_PAIR(a2 - 30) | ((a1) ? A_BOLD : A_NORMAL));
				}		
			} else {
				while (*p && ((*p >= '0' && *p <= '9') || *p == ';'))
					p++;
				p++;
			}
		} else if (*p == 10) {
			if (!*(p + 1))
				break;
			y++;
			x = 0;
			set_cursor();
		} else {
			waddch(output, (unsigned char) *p);
			x++;
			if (x == stdscr->_maxx + 1) {
				y++;
				x = 0;
				set_cursor();
			}
		}
	}

	y++;
		
	pnoutrefresh(output, start, 0, 0, 0, output_size - 1, 80);
	wnoutrefresh(status);
	wnoutrefresh(input);
	doupdate();
}

static void ui_ncurses_beep()
{
	beep();
}

void ui_ncurses_init()
{
	ui_print = ui_ncurses_print;
	ui_loop = ui_ncurses_loop;
	ui_beep = ui_ncurses_beep;
	ui_event = ui_ncurses_event;
	ui_deinit = ui_ncurses_deinit;
		
	initscr();
	cbreak();
	noecho();
	nonl();

	lines = stdscr->_maxy - 1;

	output = newpad(lines, 80);
	status = newwin(1, 80, lines, 0);
	input = newwin(1, 80, lines + 1, 0);
	keypad(input, TRUE);

	start_color();
	init_pair(0, COLOR_BLACK, COLOR_BLACK);
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);
	init_pair(7, COLOR_WHITE, COLOR_BLACK);

	init_pair(8, COLOR_WHITE, COLOR_BLUE);
	
	wattrset(status, COLOR_PAIR(8) | A_BOLD);
	mvwaddstr(status, 0, 0, " ekg XP");
	wattrset(status, COLOR_PAIR(8));
	waddstr(status, " :: próbna jazda na gapê                                                 ");
	
	wnoutrefresh(output);
	wnoutrefresh(status);
	wnoutrefresh(input);
	doupdate();

	ui_ncurses_print("__current", "\033[1m
    ************************************************************************
    *** Ten interfejs u¿ytkownika jest jeszcze w bardzo wczesnym stadium ***
    *** rozwoju! NIE PISZ, JE¦LI JAKA¦ OPCJA NIE DZIA£A. Doskonale o tym ***
    *** wiadomo. Po prostu cierpliwie poczekaj, a¿ zostanie napisany.    ***
    ************************************************************************
\033[0m");

	signal(SIGINT, SIG_IGN);
	
	memset(history, 0, sizeof(history));
}

static void ui_ncurses_deinit()
{
	werase(input);
	wnoutrefresh(input);
	doupdate();
	delwin(output);
	delwin(input);
	delwin(status);
	endwin();

	xfree(yanked);
}

#define adjust() \
{ \
	line_index = strlen(line); \
	if (strlen(line) < 70) \
		line_start = 0; \
	else \
		line_start = strlen(line) - strlen(line) % 70; \
}

#define ui_debug(x...) { \
	char *ui_debug_tmp = saprintf(x); \
	ui_ncurses_print(NULL, ui_debug_tmp); \
	xfree(ui_debug_tmp); \
}

void dcc_generator(const char *text, int len)
{
	char *words[] = { "close", "get", "send", "show", "voice", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

void command_generator(const char *text, int len)
{
	char *slash = "";
	list_t l;

	if (*text == '/') {
		slash = "/";
		text++;
		len--;
	}

	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (!strncasecmp(text, c->name, len))
			array_add(&completions, saprintf("%s%s", slash, c->name));
	}
}

void known_uin_generator(const char *text, int len)
{
	list_t l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (u->display && !strncasecmp(text, u->display, len))
			array_add(&completions, xstrdup(u->display));
	}
}

void unknown_uin_generator(const char *text, int len)
{
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		if (isdigit(send_nicks[i][0]) && !strncasecmp(text, send_nicks[i], len))
			array_add(&completions, send_nicks[i]);
	}
}

void variable_generator(const char *text, int len)
{
	list_t l;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		if (v->type == VAR_FOREIGN)
			continue;

		if (*text == '-') {
			if (!strncasecmp(text + 1, v->name, len - 1))
				array_add(&completions, saprintf("-%s", v->name));
		} else {
			if (!strncasecmp(text, v->name, len))
				array_add(&completions, xstrdup(v->name));
		}
	}
}

void ignored_uin_generator(const char *text, int len)
{
	list_t l;

	for (l = ignored; l; l = l->next) {
		struct ignored *i = l->data;
		struct userlist *u;

		if (!(u = userlist_find(i->uin, NULL))) {
			if (!strncasecmp(text, itoa(i->uin), len))
				array_add(&completions, xstrdup(itoa(i->uin)));
		} else {
			if (u->display && !strncasecmp(text, u->display, len))
				array_add(&completions, xstrdup(u->display));
		}
	}
}

void empty_generator(const char *text, int len)
{

}

void file_generator(const char *text, int len)
{

}

static struct {
	char ch;
	void (*generate)(const char *text, int len);
} generators[] = {
	{ 'u', known_uin_generator },
	{ 'U', unknown_uin_generator },
	{ 'c', command_generator },
	{ 's', empty_generator },
	{ 'i', ignored_uin_generator },
	{ 'v', variable_generator },
	{ 'd', dcc_generator },
	{ 'f', file_generator },
	{ 0, NULL }
};

/*
 * complete()
 *
 * funkcja obs³uguj±ca dope³nianie klawiszem tab.
 */
static void complete(int *line_start, int *line_index)
{
	int i, blanks = 0, count;
	char *p, *start = line;

	/* nie obs³ugujemy dope³niania w ¶rodku tekstu */
	if (*line_index != strlen(line))
		return;

	/* je¶li uzbierano ju¿ co¶ */
	if (completions) {
		int maxlen = 0, cols, rows;
		char *tmp;

		for (i = 0; completions[i]; i++)
			if (strlen(completions[i]) + 2 > maxlen)
				maxlen = strlen(completions[i]) + 2;

		cols = (stdscr->_maxx + 1) / maxlen;
		if (cols == 0)
			cols = 1;

		rows = array_count(completions) / cols + 1;

		tmp = xmalloc(cols * maxlen + 2);

		for (i = 0; i < rows; i++) {
			int j;

			strcpy(tmp, "");

			for (j = 0; j < cols; j++) {
				int cell = j * rows + i;

				if (cell < array_count(completions)) {
					int k;

					strcat(tmp, completions[cell]); 

					for (k = 0; k < maxlen - strlen(completions[cell]); k++)
						strcat(tmp, " ");
				}
			}

			if (strcmp(tmp, "")) {
				strcat(tmp, "\n");
				ui_ncurses_print(NULL, tmp);
			}
		}

		xfree(tmp);

		return;
	}
	
	/* policz spacje */
	for (p = line; *p; p++) {
		if (*p == ' ')
			blanks++;
	}

	/* nietypowe dope³nienie nicków przy rozmowach */
	if (!strcmp(line, "") || (!strncasecmp(line, "chat ", 5) && blanks < 3 && send_nicks_count > 0)) {
		if (send_nicks_count)
			snprintf(line, sizeof(line), "chat %s ", send_nicks[send_nicks_index++]);
		else
			snprintf(line, sizeof(line), "chat ");
		*line_start = 0;
		*line_index = strlen(line);
		if (send_nicks_index >= send_nicks_count)
			send_nicks_index = 0;

		array_free(completions);
		completions = NULL;

		return;
	}

	/* pocz±tek komendy? */
	if (!blanks)
		command_generator(line, strlen(line));
	else {
		char *params = NULL;
		int abbrs = 0, word = 0;
		list_t l;

		start = line + strlen(line);

		while (start > line && *(start - 1) != ' ')
			start--;

		for (p = line + 1; *p; p++)
			if (isspace(*p) && !isspace(*(p - 1)))
				word++;
		word--;

		for (l = commands; l; l = l->next) {
			struct command *c = l->data;
			int len = strlen(c->name);
			char *cmd = (line[0] == '/') ? line + 1 : line;

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

		if (params && abbrs == 1 && word < strlen(params)) {
			for (i = 0; generators[i].ch; i++)
				if (generators[i].ch == params[word]) {
					generators[i].generate(start, strlen(start));
					break;
				}
		}
	}

	count = array_count(completions);

	if (count == 1) {
		snprintf(start, sizeof(line) - (start - line), "%s ", completions[0]);
		*line_index = strlen(line);
		array_free(completions);
		completions = NULL;
	}

	if (count > 1) {
		int common = 0, minlen = strlen(completions[0]);

		for (i = 1; i < count; i++) {
			if (strlen(completions[i]) < minlen)
				minlen = strlen(completions[i]);
		}

		for (i = 0; i < minlen; i++, common++) {
			char c = completions[0][i];
			int j, out = 0;

			for (j = 1; j < count; j++) {
				if (completions[j][i] != c) {
					out = 1;
					break;
				}
			}
			
			if (out)
				break;
		}

		if (common > strlen(start) && start - line + common < sizeof(line)) {
			snprintf(start, common + 1, "%s", completions[0]);
			*line_index = strlen(line);
		}
	}

	return;
}

static void ui_ncurses_loop()
{
	int line_start = 0, line_index = 0;

	history[0] = line;

	for (;;) {
		int ch;

		ekg_wait_for_key();
		switch ((ch = wgetch(input))) {
			case KEY_BACKSPACE:
			case 8:
			case 127:
				if (strlen(line) > 0 && line_index > 0) {
					memmove(line + line_index - 1, line + line_index, sizeof(line) - line_index);
					line[sizeof(line) - 1] = 0;
					line_index--;
				}
				break;
			case 'Y' - 64:
				if (yanked && strlen(yanked) + strlen(line) + 1 < sizeof(line)) {
					memmove(line + line_index + strlen(yanked), line + line_index, sizeof(line) - line_index - strlen(yanked));
					memcpy(line + line_index, yanked, strlen(yanked));
					line_index += strlen(yanked);
				}
				break;
			case KEY_DC:
			case 'D' - 64:
				if (line_index < strlen(line)) {
					memmove(line + line_index, line + line_index + 1, sizeof(line) - line_index - 1);
					line[sizeof(line) - 1] = 0;
				}
				break;	
			case KEY_ENTER:
			case 13:
				if (command_exec(NULL, line))
					return;
				if (history[0] != line)
					xfree(history[0]);
				history[0] = xstrdup(line);
				xfree(history[HISTORY_MAX - 1]);
				memmove(&history[1], &history[0], sizeof(history) - sizeof(history[0]));
				history[0] = line;
				history_index = 0;
				line[0] = 0;
				adjust();
				break;	
			case 'U' - 64:
				xfree(yanked);
				yanked = strdup(line);
				line[0] = 0;
				adjust();
				break;
			case 'L' - 64:
				break;
			case 9:
				complete(&line_start, &line_index);

				break;
			case KEY_LEFT:
				if (line_index > 0)
					line_index--;
				break;
			case KEY_RIGHT:
				if (line_index < strlen(line))
					line_index++;
				break;
			case 'E' - 64:
			case KEY_END:
			case KEY_SELECT:
				adjust();
				break;
			case 'A' - 64:
			case KEY_HOME:
			case KEY_FIND:
				line_index = 0;
				line_start = 0;
				break;
			case KEY_UP:
				if (history[history_index + 1]) {
					if (history_index == 0)
						history[0] = xstrdup(line);
					history_index++;
					strcpy(line, history[history_index]);
					adjust();
				}
				break;
			case KEY_DOWN:
				if (history_index > 0) {
					history_index--;
					strcpy(line, history[history_index]);
					adjust();
					if (history_index == 0) {
						xfree(history[0]);
						history[0] = line;
					}
				}
				break;
			case KEY_PPAGE:
				start -= output_size;
				if (start < 0)
					start = 0;
				break;
			case KEY_NPAGE:
				start += output_size;
				if (start > lines - output_size)
					start = lines - output_size;
				break;
			case KEY_F(12):
				binding_toggle_debug(0, 0);
				break;
			default:
				if (ch < 32)
					break;
				if (strlen(line) >= sizeof(line) - 1)
					break;
				memmove(line + line_index + 1, line + line_index, sizeof(line) - line_index - 1);

				line[line_index++] = ch;
		}

		/* je¶li siê co¶ zmieni³o, wygeneruj dope³nienia na nowo */
		if (ch != 9) {
			array_free(completions);
			completions = NULL;
		}

		if (line_index - line_start > 70)
			line_start += 60;
		if (line_index - line_start < 10) {
			line_start -= 60;
			if (line_start < 0)
				line_start = 0;
		}
		pnoutrefresh(output, start, 0, 0, 0, output_size - 1, 80);
		werase(input);
		wattrset(input, COLOR_PAIR(7));
		mvwaddstr(input, 0, 0, line + line_start);
		wattrset(input, COLOR_PAIR(2));
		if (line_start > 0)
			mvwaddch(input, 0, 0, '<');
		if (strlen(line) - line_start > 80)
			mvwaddch(input, 0, 79, '>');
		wattrset(input, COLOR_PAIR(7));
		wmove(input, 0, line_index - line_start);
		wnoutrefresh(status);
		wnoutrefresh(input);
		doupdate();
	}
}

static int ui_ncurses_event(const char *event, ...)
{

	return 0;
}

