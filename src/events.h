/* $Id$ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>
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

#ifndef __EVENTS_H
#define __EVENTS_H

#include "libgadu.h"

struct handler {
	int type;
	void (*handler)(struct gg_event *e);
};

void handle_event(struct gg_session *s);
void handle_dcc(struct gg_dcc *s);
void handle_msg(struct gg_event *e);

void handle_search(struct gg_http *s);
void handle_pubdir(struct gg_http *s);

void handle_disconnect(struct gg_event *e);

#endif
