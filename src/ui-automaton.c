/* $Id$ */

/*
 *  (C) Copyright 2002 Micha³ Moskal <malekith@pld-linux.org>
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

static void print_keyword(const char *s)
{
	printf("%s ", s);
}

static void print_string(const char *s)
{
	putchar('"');
	while (*s) {
		int q = 0;
		switch (*s) {
		case '\n': q = 'n'; break;
		case '\t': q = 't'; break;
		case '\033': q = 'e'; break;
		case '"': q = '"'; break;
		case '\\': q = '\\'; break;
		default: break;
		}
		if (q) {
			putchar('\\');
			putchar(q); 
		} else {
			putchar(*s);
		}
		s++;
	}
	putchar('"');
	putchar(' ');
}

static void print_newline()
{
	putchar('\n');
	fflush(stdout);
}

/*
 * ui_automaton_print()
 *
 * wy¶wietla dany tekst na ekranie, uwa¿aj±c na trwaj±ce w danych chwili
 * automaton().
 */
static void ui_automaton_print(const char *target, int separate, const char *line)
{
	print_keyword("print");

	if (target == NULL)
		print_keyword("(null)");
	else
		print_keyword(target);
		
	if (separate)
		print_keyword("separate");
	else
		print_keyword("no-separate");
	
	print_string(line);
	print_newline();
}

/*
 * ui_automaton_beep()
 *
 * wydaje d¼wiêk na konsoli.
 */
static void ui_automaton_beep()
{
	print_keyword("beep");
	print_newline();
}

/*
 * ui_automaton_postinit()
 *
 * uruchamiana po wczytaniu konfiguracji.
 */
static void ui_automaton_postinit()
{
}

/*
 * ui_automaton_deinit()
 *
 * zamyka to, co zwi±zane z interfejsem.
 */
static void ui_automaton_deinit()
{
}

/*
 * ui_automaton_loop()
 *
 * g³ówna pêtla programu. wczytuje dane z klawiatury w miêdzyczasie
 * obs³uguj±c sieæ i takie tam.
 */
static void ui_automaton_loop()
{
	char buf[1000];
	char *line = NULL;
	int fd = 0, buf_len, linesz = 0, buf_pos;

	for (;;) {
		ekg_wait_for_key();
		
		buf_len = read(fd, buf, sizeof(buf));
		if (buf_len <= 0)
			break;
		buf_pos = 0;
		
		while (buf_pos < buf_len) {
			int i, n;
			for (i = buf_pos; i < buf_len; i++)
				if (buf[i] == '\n')
					break;
			n = i - buf_pos;
			line = xrealloc(line, linesz + n + 1);
			memcpy(line + linesz, buf + buf_pos, n);
			linesz += n;
			line[linesz] = 0;
			buf_pos = i;
			       
			if (buf_pos < buf_len) {
				command_exec(NULL, line);
				xfree(line);
				line = NULL;
				linesz = 0;
				buf_pos++;
			}
		}
	}
}

static void fixup_theme(void)
{
	format_add("prompt", "INFO", 1);
        format_add("prompt2", "NOTICE", 1);
        format_add("error", "ERROR", 1);
	
	format_add("message_header", "MSG_START NORMAL %1 %2\n", 1);
	format_add("message_conference_header", "MSG_START CONFERENCE %1 %2 %3", 1);
	format_add("message_footer", "MSG_END", 1);
	format_add("message_line", "MSG_LINE %1\n", 1);
	format_add("message_line_width", "-8", 1);
	format_add("message_timestamp", "%Y-%m-%d/%H:%M", 1);
	format_add("chat_header", "MSG_START CHAT_NORMAL %1 %2\n", 1);
	format_add("chat_conference_header", "MSG_START CHAT_CONFERENCE %1 %2 %3\n", 1);
	format_add("chat_footer", "MSG_END", 1);
	format_add("chat_line", "MSG_LINE %1\n", 1);
	format_add("chat_line_width", "-8", 1);
	format_add("chat_timestamp", "%Y-%m-%d/%H:%M", 1);
	format_add("sent_header", "MSG_START SENT %1 %#\n", 1);
	format_add("sent_conference_header", "MSG_START SENT_CONFERENCE %1 %# %3\n", 1);
	format_add("sent_footer", "MSG_END\n", 1);
	format_add("sent_line", "MSG_LINE %1\n", 1);
	format_add("sent_line_width", "-8", 1);
	format_add("sent_timestamp", "%Y-%m-%d/%H:%M", 1);
	format_add("sysmsg_header", "MSG_START SYSTEM\n", 1);
	format_add("sysmsg_line", "MSG_LINE %1\n", 1);
	format_add("sysmsg_line_width", "-8", 1);
	format_add("sysmsg_footer", "MSG_END\n", 1);	
}

/*
 * ui_automaton_event()
 *
 * obs³uga zdarzeñ wysy³anych z ekg do interfejsu.
 */
static int ui_automaton_event(const char *event, ...)
{
	va_list ap;
	int result = 0;

	va_start(ap, event);
	
	print_keyword("event");
	print_keyword(event);
	
        if (!strcmp(event, "variable_changed")) {
		print_string(va_arg(ap, char*));
	} else if (!strcmp(event, "command")) {
		char *p;
		
		while ((p = va_arg(ap, char*)) != NULL)
			print_string(p);
	} else if (!strcmp(event, "theme_init")) {
		fixup_theme();
	}

	print_newline();

	va_end(ap);

	return result;
}

/*
 * ui_automaton_init()
 *
 * inicjalizacja interfejsu automaton.
 */
void ui_automaton_init()
{
	ui_postinit = ui_automaton_postinit;
	ui_print = ui_automaton_print;
	ui_loop = ui_automaton_loop;
	ui_beep = ui_automaton_beep;
	ui_event = ui_automaton_event;
	ui_deinit = ui_automaton_deinit;
	
	ui_screen_width = 1000000;
	ui_screen_height = 1000000;

	automaton_color_escapes = 1;
}

