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

#ifndef __VARS_H
#define __VARS_H

#include "dynstuff.h"

enum {
	VAR_STR,		/* ci±g znaków */
	VAR_INT,		/* liczba ca³kowita */
	VAR_BOOL,		/* 0/1, tak/nie, yes/no, on/off */
	VAR_FOREIGN,		/* nieznana zmienna */
};

struct variable {
	char *name;		/* nazwa zmiennej */
	int name_hash;		/* hash nazwy zmiennej */
	int type;		/* rodzaj */
	int display;		/* 0 bez warto¶ci, 1 pokazuje, 2 w ogóle */
	void *ptr;		/* wska¼nik do zmiennej */
	void (*notify)(const char*);	/* funkcja wywo³ywana przy zmianie */
};

list_t variables;

void variable_init();
struct variable *variable_find(const char *name);
int variable_add(const char *name, int type, int display, void *ptr, void (*notify)(const char *name));
int variable_set(const char *name, const char *value, int allow_foreign);

#endif /* __VARS_H */
