/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "configfile.h"
#include "stuff.h"
#include "userlist.h"
#include "libgadu.h"

void ekg_oom_handler()
{
	if (old_stderr)
		dup2(old_stderr, 2);

	fprintf(stderr,
"\r\n"
"\r\n"
"*** Brak pamiêci ***\r\n"
"\r\n"
"Próbujê zapisaæ ustawienia do pliku %s/config.%d i listê kontaktów\r\n"
"do pliku %s/userlist.%d, ale nie obiecujê, ¿e cokolwiek z tego\r\n"
"wyjdzie.\r\n"
"\r\n"
"Do pliku %s/debug.%d zapiszê ostatanie komunikaty\r\n"
"z okna debugowania.\r\n"
"\r\n", config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, (int) getpid());

	config_write_crash();
	userlist_write_crash();
	debug_write_crash();

	exit(1);
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *tmp = calloc(nmemb, size);

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

void *xmalloc(size_t size)
{
	void *tmp = malloc(size);

	if (!tmp)
		ekg_oom_handler();

	/* na wszelki wypadek wyczy¶æ bufor */
	memset(tmp, 0, size);
	
	return tmp;
}

void xfree(void *ptr)
{
	if (ptr)
		free(ptr);
}

void *xrealloc(void *ptr, size_t size)
{
	void *tmp = realloc(ptr, size);

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

char *xstrdup(const char *s)
{
	char *tmp;

	if (!s)
		return NULL;

	if (!(tmp = strdup(s)))
		ekg_oom_handler();

	return tmp;
}

char *saprintf(const char *format, ...)
{
	va_list ap;
	char *res;
	
	va_start(ap, format);
	res = gg_vsaprintf(format, ap);
	va_end(ap);

	if (!res)
		ekg_oom_handler();
	
	return res;
}
