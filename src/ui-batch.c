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

#include "stuff.h"
#include "ui.h"

static void loop()
{
	while (batch_line)
		ekg_wait_for_key();
}

static void nop()
{

}

static int event(const char *foo, ...)
{
	return 0;
}

static void print(const char *target, int separate, const char *line)
{
	printf("%s", line);
}

void ui_batch_init()
{
	ui_postinit = nop;
	ui_print = print;
	ui_loop = loop;
	ui_beep = nop;
	ui_event = event;
	ui_deinit = nop;
}
