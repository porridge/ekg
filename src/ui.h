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

#ifndef __UI_H
#define __UI_H

#include "config.h"

void (*ui_loop)(void);
void (*ui_print)(const char *target, int separate, const char *line);
void (*ui_beep)(void);
int (*ui_event)(const char *event, ...);
void (*ui_deinit)(void);

int ui_screen_width;
int ui_screen_height;

extern void ui_none_init();
extern void ui_batch_init();

#ifdef WITH_UI_READLINE

#define MAX_LINES_PER_SCREEN 50

extern void ui_readline_init();
#endif

#ifdef WITH_UI_NCURSES
extern void ui_ncurses_init();
#endif

#endif /* __UI_H */

