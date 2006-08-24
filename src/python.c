/* $Id$ */

/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo�ny <speedy@ziew.org>
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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <string.h>

#include "commands.h"
#include "libgadu.h"
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "vars.h"
#include "version.h"
#include "xmalloc.h"

#include <Python.h>
#include "python.h"

int python_handle_result = -1;

static PyObject* ekg_cmd_connect(PyObject *self, PyObject *args)
{
	ekg_connect();

	return Py_BuildValue("");
}

static PyObject* ekg_cmd_disconnect(PyObject *self, PyObject *args)
{
	char *reason = NULL, *tmp;

	if (!PyArg_ParseTuple(args, "|s", &reason))
		return NULL;

	tmp = saprintf("disconnect %s", ((reason) ? reason : ""));

	command_exec(NULL, tmp, 0);
	xfree(tmp);

	return Py_BuildValue("");
}

static PyObject* ekg_cmd_printf(PyObject *self, PyObject *pyargs)
{
	char *format = "generic", *args[9];
	int i;

	for (i = 0; i < 9; i++)
		args[i] = "";

	if (!PyArg_ParseTuple(pyargs, "s|sssssssss:printf", &format, &args[0], &args[1], &args[2], &args[3], &args[4], &args[5], &args[6], &args[7], &args[8]))
		return NULL;

	print(format, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);

	return Py_BuildValue("");
}

static PyObject* ekg_cmd_command(PyObject *self, PyObject *args)
{
	char *command = NULL;

	if (!PyArg_ParseTuple(args, "s", &command))
		return NULL;

	command_exec(NULL, command, 0);

	return Py_BuildValue("");
}

static PyObject* ekg_cmd_print_header(PyObject *self, PyObject *args)
{
	char *text = NULL;
	int x, y;

	if (!PyArg_ParseTuple(args, "iis", &x, &y, &text))
		return NULL;

#ifdef WITH_UI_NCURSES
	if (header)
		window_printat(header, x, y, text, NULL, COLOR_WHITE, 0, COLOR_BLUE, 1);
#endif

	return Py_BuildValue("");
}

static PyObject* ekg_cmd_print_statusbar(PyObject *self, PyObject *args)
{
	char *text = NULL;
	int x, y;

	if (!PyArg_ParseTuple(args, "iis", &x, &y, &text))
		return NULL;

#ifdef WITH_UI_NCURSES
	window_printat(status, x, y, text, NULL, COLOR_WHITE, 0, COLOR_BLUE, 1);
#endif

	return Py_BuildValue("");
}

static PyObject* ekg_cmd_window_printat(PyObject *self, PyObject *args)
{
	char *target = NULL, *text = NULL;
	int id, x, y;

	if (PyArg_ParseTuple(args, "siis", &target, &x, &y, &text)) {
		ui_event("printat", target, 0, x, y, text, NULL);
		return Py_BuildValue("");
	}

	if (PyArg_ParseTuple(args, "iiis", &id, &x, &y, &text)) {
		ui_event("printat", NULL, id, x, y, text, NULL);
		return Py_BuildValue("");
	}

	return NULL;
}

static PyObject* ekg_cmd_window_commit(PyObject *self, PyObject *args)
{
	ui_event("commit", NULL);

	return Py_BuildValue("");
}

static PyObject* ekg_cmd_window_list(PyObject *self, PyObject *args)
{
	int i, start, stop, *windowlist;
	PyObject *wynik, *val;

	if (!PyArg_ParseTuple(args, "ii", &start, &stop))
		return NULL;

	if (start < 1 || start > stop) {
		PyErr_SetString(PyExc_ValueError, "invalid range");
		return NULL;
	}

	windowlist = xmalloc((stop - start + 2) * sizeof(int));

	ui_event("get_window_list", windowlist, start, stop);

	wynik = PyList_New(windowlist[0]);

	for (i = 0; i < windowlist[0]; i++) {
		val = Py_BuildValue("i", windowlist[i + 1]);
		PyList_SetItem(wynik, i, val);
	}

	xfree(windowlist);

	return wynik;
}

static PyMethodDef ekg_methods[] = {
	{ "connect", ekg_cmd_connect, METH_VARARGS, "" },
	{ "disconnect", ekg_cmd_disconnect, METH_VARARGS, "" },
	{ "printf", ekg_cmd_printf, METH_VARARGS, "" },
	{ "command", ekg_cmd_command, METH_VARARGS, "" },
	{ "print_header", ekg_cmd_print_header, METH_VARARGS, "" },
	{ "print_statusbar", ekg_cmd_print_statusbar, METH_VARARGS, "" },
	{ "window_printat", ekg_cmd_window_printat, METH_VARARGS, "" },
	{ "window_commit", ekg_cmd_window_commit, METH_VARARGS, "" },
	{ "window_list", ekg_cmd_window_list, METH_VARARGS, "" },
	{ NULL, NULL, 0, NULL }
};

static PyObject *ekg_config_getattr(PyObject *o, char *name)
{
	struct variable *v = variable_find(name);

	if (!v) {
		PyErr_SetString(PyExc_LookupError, "unknown variable");
		return NULL;
	}

	if (v->type == VAR_BOOL || v->type == VAR_INT || v->type == VAR_MAP)
		return Py_BuildValue("i", *(int*)(v->ptr));
	else
		return Py_BuildValue("s", *(char**)(v->ptr));
}

static int ekg_config_setattr(PyObject *o, char *name, PyObject *value)
{
	struct variable *v = variable_find(name);

	if (!v) {
		PyErr_SetString(PyExc_LookupError, "unknown variable");
		return -1;
	}

        if (value == NULL) {
		PyErr_SetString(PyExc_TypeError, "can't delete config variables");
		return -1;
        }

	if (v->type == VAR_INT || v->type == VAR_BOOL || v->type == VAR_MAP) {
		if (!PyInt_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "invalid type");
			return -1;
		}
		if (variable_set(name, itoa(PyInt_AsLong(value)), 0)) {
			PyErr_SetString(PyExc_ValueError, "invalid value");
			return -1;
                }
	} else {
		if (!PyString_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "invalid type");
			return -1;
		}
		if (variable_set(name, PyString_AsString(value), 0)) {
			PyErr_SetString(PyExc_ValueError, "invalid value");
			return -1;
                }
	}

	return 0;
}

static void ekg_config_dealloc(PyObject *o)
{

}

static PyTypeObject ekg_config_type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"config",
	sizeof(PyObject),
	0,
	ekg_config_dealloc,
	0,
	ekg_config_getattr,
	ekg_config_setattr,
};

int python_initialize()
{
	PyObject *ekg, *ekg_config;

	/* PyImport_ImportModule spodziewa si� nazwy modu�u, kt�ry znajduje
	 * si� w $PYTHONPATH, wi�c dodajemy tam katalog ~/.gg/scripts. mo�na
	 * to zrobi� w bardziej elegancki spos�b, ale po co komplikowa� sobie
	 * �ycie?
	 *
	 * Argument putenv() nie jest zwalniany xfree(), bo powoduje to
	 * problemy na systemach, w kt�rych putenv() jest zgodne z SUSv2 (np
	 * niekt�re SunOS).
	 */

	if (getenv("PYTHONPATH")) {
		char *tmp = saprintf("%s:%s", getenv("PYTHONPATH"), prepare_path("scripts", 0));
#ifdef HAVE_SETENV
		setenv("PYTHONPATH", tmp, 1);
#else
		{
			char *s = saprintf("PYTHONPATH=%s", tmp);
			putenv(s);
		}
#endif
		xfree(tmp);
	} else {
#ifdef HAVE_SETENV
		setenv("PYTHONPATH", prepare_path("scripts", 0), 1);
#else
		{
			char *s = saprintf("PYTHONPATH=%s", prepare_path("scripts", 0));
			putenv(s);
		}
#endif
	}

	Py_Initialize();

	PyImport_AddModule("ekg");

	if (!(ekg = Py_InitModule("ekg", ekg_methods)))
		return -1;

	PyModule_AddStringConstant(ekg, "version", VERSION);

	ekg_config = PyObject_NEW(PyObject, &ekg_config_type);
	PyModule_AddObject(ekg, "config", ekg_config);

	return 0;
}

/*
 * python_finalize()
 *
 * usuwa z pami�ci interpreter, zwalnia pami�� itd.
 *
 * 0/-1
 */
int python_finalize()
{
	list_t l;

	for (l = modules; l; l = l->next) {
		struct module *m = l->data;

		xfree(m->name);

		if (m->deinit) {
			PyObject *res = PyObject_CallFunction(m->deinit, "()");
			Py_XDECREF(res);
			Py_XDECREF(m->deinit);
		}
	}
	
	list_destroy(modules, 1);
	modules = NULL;
	
	Py_Finalize();

	return 0;
}

/*
 * python_unload()
 *
 * usuwa z pami�ci podany skrypt.
 *
 *  - name - nazwa skryptu,
 *  - quiet.
 *
 * 0/-1
 */
int python_unload(const char *name, int quiet)
{
	list_t l;

	if (!name) {
		printq("python_need_name");
		return -1;
	}

	for (l = modules; l; l = l->next) {
		struct module *m = l->data;

		if (strcmp(m->name, name))
			continue;

		gg_debug(GG_DEBUG_MISC, "m->deinit = %p, hmm?\n", m->deinit);
		if (m->deinit) {
			PyObject *res = PyObject_CallFunction(m->deinit, "()");
			Py_XDECREF(res);
			Py_XDECREF(m->deinit);
		}

		Py_XDECREF(m->handle_msg);
		Py_XDECREF(m->handle_msg_own);
		Py_XDECREF(m->handle_connect);
		Py_XDECREF(m->handle_disconnect);
		Py_XDECREF(m->handle_status);
		Py_XDECREF(m->handle_status_own);
		Py_XDECREF(m->handle_redraw_header);
		Py_XDECREF(m->handle_redraw_statusbar);
		Py_XDECREF(m->handle_keypress);
		Py_XDECREF(m->handle_command_line);
		Py_XDECREF(m->module);

		list_remove(&modules, m, 1);

		printq("python_removed");

		return 0;
	}
	
	printq("python_not_found", name);
	
	return -1;
}

/*
 * python_run()
 *
 * uruchamia jednorazowo skrypt pythona.
 *
 * 0/-1
 */
int python_run(const char *filename, int quiet)
{
	FILE *f = fopen(filename, "r");

	if (!f) {
		printq("python_not_found", filename);
		return -1;
	}

	PyRun_SimpleFile(f, (char*) filename);
	fclose(f);

	return 0;
}

/*
 * python_get_func()
 *
 * zwraca dan� funkcj� modu�u.
 */
PyObject *python_get_func(PyObject *module, const char *name)
{
	PyObject *result = PyObject_GetAttrString(module, (char*) name);

	if (result && !PyCallable_Check(result)) {
		Py_XDECREF(result);
		result = NULL;
	}

	return result;
}

/*
 * python_load()
 *
 * �aduje skrypt pythona o podanej nazwie z ~/.gg/scripts
 *
 *  - name - nazwa skryptu,
 *  - quiet.
 *
 * 0/-1
 */
int python_load(const char *name, int quiet)
{
	PyObject *module, *init;
	struct module m;
	char *name2;

	if (!name) {
		printq("python_need_name");
		return -1;
	}
	
	if (strchr(name, '/')) {
		printq("python_wrong_location", prepare_path("scripts", 0));
		return -1;
	}

	name2 = xstrdup(name);

	if (strlen(name2) > 3 && !strcasecmp(name2 + strlen(name2) - 3, ".py"))
		name2[strlen(name2) - 3] = 0;

	module = PyImport_ImportModule(name2);

	if (!module) {
		printq("python_not_found", name2);
		PyErr_Print();
		xfree(name2);
		return -1;
	}

	if ((init = PyObject_GetAttrString(module, "init"))) {
		if (PyCallable_Check(init)) {
			PyObject *result = PyObject_CallFunction(init, "()");

			if (result) {
				int resulti = PyInt_AsLong(result);

				if (!resulti) {
					
				}

				Py_XDECREF(result);
			}
		}

		Py_XDECREF(init);
	}

	memset(&m, 0, sizeof(m));

	m.name = xstrdup(name2);
	m.module = module;
	m.deinit = python_get_func(module, "deinit");
	m.handle_msg = python_get_func(module, "handle_msg");
	m.handle_msg_own = python_get_func(module, "handle_msg_own");
	m.handle_connect = python_get_func(module, "handle_connect");
	m.handle_disconnect = python_get_func(module, "handle_disconnect");
	m.handle_status = python_get_func(module, "handle_status");
	m.handle_status_own = python_get_func(module, "handle_status_own");
	m.handle_redraw_header = python_get_func(module, "handle_redraw_header");
	m.handle_redraw_statusbar = python_get_func(module, "handle_redraw_statusbar");
	m.handle_keypress = python_get_func(module, "handle_keypress");
	m.handle_command_line = python_get_func(module, "handle_command_line");

	PyErr_Clear();

	list_add(&modules, &m, sizeof(m));
	
	xfree(name2);

	return 0;
}

/*
 * python_exec()
 *
 * wykonuje polecenie pythona.
 * 
 *  - command - polecenie.
 * 
 * 0/-1
 */
int python_exec(const char *command)
{
	char *tmp;

	if (!command)
		return 0;

 	tmp = saprintf("import ekg\n%s\n", command);

	PyRun_SimpleString(tmp);
	xfree(tmp);

	return 0;
}

/*
 * python_list()
 *
 * wy�wietla list� za�adowanych skrypt�w.
 *
 * 0/-1
 */
int python_list(int quiet)
{
	list_t l;

	if (!modules)
		printq("python_list_empty");

	for (l = modules; l; l = l->next) {
		struct module *m = l->data;

		printq("python_list", m->name);
	}

	return 0;
}

int python_function(const char *function, const char *arg)
{
	return -1;
}

void python_autorun()
{
	const char *path = prepare_path("scripts/autorun", 0);
	struct dirent *d;
	struct stat st;
	char *tmp;
	DIR *dir;
	
	if (!(dir = opendir(path)))
		return;

	/* nale�y utworzy� plik ~/.gg/scripts/autorun/__init__.py, inaczej
	 * python nie b�dzie mo�na �adowa� skrypt�w przez ,,autorun.nazwa'' */
	
	tmp = saprintf("%s/__init__.py", path);

	if (stat(tmp, &st)) {
		FILE *f = fopen(tmp, "w");
		if (f)
			fclose(f);
	}

	xfree(tmp);

	while ((d = readdir(dir))) {
		tmp = saprintf("%s/%s", path, d->d_name);

		if (stat(tmp, &st) || S_ISDIR(st.st_mode)) {
			xfree(tmp);
			continue;
		}

		xfree(tmp);

		if (!strcmp(d->d_name, "__init__.py"))
			continue;

		if (strlen(d->d_name) < 3 || strcmp(d->d_name + strlen(d->d_name) - 3, ".py"))
			continue;

		tmp = saprintf("autorun.%s", d->d_name);
		tmp[strlen(tmp) - 3] = 0;

		python_load(tmp, 0);

		xfree(tmp);
	}

	closedir(dir);
}
