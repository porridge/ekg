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

#ifndef __PYTHON_H
#define __PYTHON_H

#include <Python.h>

struct module {
	char *name;			/* nazwa skryptu */
	
	PyObject *module;		/* obiekt modu³u */

	PyObject *deinit;		/* funkcja deinicjalizacji */

	PyObject *handle_msg;		/* obs³uga zdarzeñ... */
	PyObject *handle_connect;
	PyObject *handle_disconnect;
	PyObject *handle_status;
};

list_t modules;

int python_initialize();
int python_finalize();
int python_load(const char *name);
int python_unload(const char *name);
int python_exec(const char *command);
int python_run(const char *filename);
int python_list();
int python_function(const char *function, const char *arg);

#endif /* __PYTHON_H */
