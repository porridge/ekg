/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
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
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#ifndef _AIX
#  include <string.h>
#endif
#include "stuff.h"
#include "userlist.h"

static void oom_handler()
{
	fprintf(stderr, "
*** Brak pamiêci ***

Próbujê zapisaæ ustawienia do pliku %s/config.%d i listê kontaktów
do pliku %s/userlist.%d, ale nie obiecujê, ¿e cokolwiek z tego
wyjdzie.

", config_dir, getpid(), config_dir, getpid());

	config_write_crash();
	userlist_write_crash();

	exit(1);
}

void *xcalloc(int nmemb, int size)
{
	void *tmp = calloc(nmemb, size);

	if (!tmp)
		oom_handler();

	return tmp;
}

void *xmalloc(int size)
{
	void *tmp = malloc(size);

	if (!tmp)
		oom_handler();
	
	return tmp;
}

void xfree(void *ptr)
{
	if (ptr)
		free(ptr);
}

void *xrealloc(void *ptr, int size)
{
	void *tmp = realloc(ptr, size);

	if (!tmp)
		oom_handler();

	return tmp;
}

char *xstrdup(const char *s)
{
	char *tmp;

	if (!s)
		return NULL;

	if (!(tmp = strdup(s)))
		oom_handler();

	return tmp;
}

/* nie jest w nag³ówkach, ¿eby <libgadu.h> nie wymaga³o <stdarg.h> */
extern char *gg_vsaprintf(const char *format, va_list ap);

char *saprintf(const char *format, ...)
{
	va_list ap;
	char *res;
	
	va_start(ap, format);
	res = gg_vsaprintf(format, ap);
	va_end(ap);

	if (!res)
		oom_handler();
	
	return res;
}

