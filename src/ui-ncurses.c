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
 * - dope³nianie tabem,
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
#include "config.h"
#include "stuff.h"
#include "commands.h"
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
char *history[HISTORY_MAX];
int history_index = 0;
int lines = 0, x = 0, y = 0, start = 0;
char line[1000] = "";

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
}

#define adjust() \
{ \
	line_index = strlen(line); \
	if (strlen(line) < 70) \
		line_start = 0; \
	else \
		line_start = strlen(line) - strlen(line) % 70; \
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
			case KEY_DC:
			case 'D' - 64:
				if (line_index < strlen(line)) {
					memmove(line + line_index, line + line_index + 1, sizeof(line) - line_index - 1);
					line[sizeof(line) - 1] = 0;
				}
				break;	
			case KEY_ENTER:
			case 13:
				if (ekg_execute(NULL, line))
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
				line[0] = 0;
				adjust();
				break;
			case 'L' - 64:
				break;
			case 9:
				if (send_nicks_count > 0) {
					snprintf(line, sizeof(line), "chat %s ", send_nicks[send_nicks_index++]);
					line_start = 0;
					line_index = strlen(line);
					if (send_nicks_index >= send_nicks_count)
						send_nicks_index = 0;
				}
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

