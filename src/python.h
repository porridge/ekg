/* $Id: python.h 2590 2005-12-05 20:20:10Z wojtekka $ */

/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
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
	PyObject *handle_redraw_header;
	PyObject *handle_redraw_statusbar;
	PyObject *handle_keypress;
	PyObject *handle_command_line;
	PyObject *handle_msg_own;
	PyObject *handle_status_own;
};

#define PYTHON_HANDLE_HEADER(event, args...) \
{ \
	list_t __py_l; \
	\
	python_handle_result = -1;\
	\
	for (__py_l = modules; __py_l; __py_l = __py_l->next) { \
		struct module *__py_m = __py_l->data; \
		PyObject *__py_r; \
		\
		if (!__py_m->handle_##event) \
			continue; \
		\
		__py_r = PyObject_CallFunction(__py_m->handle_##event, args); \
		\
		if (!__py_r) \
			PyErr_Print(); \
		\
		python_handle_result = -1; \
		\
		if (__py_r && PyInt_Check(__py_r)) { \
			int tmp = PyInt_AsLong(__py_r); \
			\
			if (python_handle_result != 2 && tmp != 1) \
				python_handle_result = tmp; \
		} \
		\
		if (__py_r && PyTuple_Check(__py_r))

#define PYTHON_HANDLE_RESULT(args...) \
			if (!PyArg_ParseTuple(__py_r, args)) \
				PyErr_Print(); \
			else
				
#define PYTHON_HANDLE_FOOTER() \
		\
		Py_XDECREF(__py_r); \
		\
		if (python_handle_result == 0) \
			break; \
	} \
}

int python_handle_result;

list_t modules;

int python_initialize(void);
int python_finalize(void);
int python_load(const char *name, int quiet);
int python_unload(const char *name, int quiet);
int python_exec(const char *command);
int python_run(const char *filename, int quiet);
int python_list(int quiet);
int python_function(const char *function, const char *arg);
void python_autorun(void);

#endif /* __PYTHON_H */
