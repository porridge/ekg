/* $Id: voice.h 2590 2005-12-05 20:20:10Z wojtekka $ */

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

#ifndef __VOICE_H
#define __VOICE_H

int voice_fd;

int voice_open(void);
void voice_close(void);
int voice_play(const char *buf, int length, int null);
int voice_record(char *buf, int length, int null);

#endif /* __VOICE_H */
