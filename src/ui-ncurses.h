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

#ifndef __UI_NCURSES_H
#define __UI_NCURSES_H

void ui_ncurses_init();
static void ui_ncurses_loop();
static void ui_ncurses_print(const char *target, const char *line);
static void ui_ncurses_beep();
static void ui_ncurses_new_target(const char *target);
static void ui_ncurses_query(const char *param);
static void ui_ncurses_deinit();

#endif /* __UI_NCURSES_H */

