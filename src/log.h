/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#ifndef __LOG_H
#define __LOG_H

#include <sys/types.h>
#include <time.h>

#include "libgadu.h"

struct last {
	int type;		/* 0 - przychodz±ca, 1 - wychodz±ca */
	uin_t uin;		/* od kogo, lub do kogo przy wysy³anych */
	time_t time;		/* czas */
	time_t sent_time;	/* czas wys³ania wiadomo¶ci przychodz±cej */
	unsigned char *message;	/* wiadomo¶æ */
};

list_t lasts;

void last_add(int type, uin_t uin, time_t t, time_t st, const char *msg);
void last_del(uin_t uin);
int last_count(uin_t uin);
void last_free();

void put_log(uin_t uin, const char *format, ...);
const char *log_timestamp(time_t t);

#endif /* __LOG_H */
