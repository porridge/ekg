/*
 * bla, bla, GPL fau dwa. kolejny magiczny, eksperymentalny kod.
 */

#include <Python.h>
#include "xmalloc.h"
#include "stuff.h"
#include "themes.h"
#include "commands.h"
#include "version.h"
#include "vars.h"

struct module {
	char *name;
	PyObject *module;
	PyObject *deinit;
};

list_t modules = NULL;

static PyObject* ekg_connect(PyObject *self, PyObject *args)
{
	do_connect();

	return Py_BuildValue("");
}

static PyObject* ekg_disconnect(PyObject *self, PyObject *args)
{
	char *reason = NULL;

	if (!PyArg_ParseTuple(args, "|s", &reason))
		return NULL;

	command_exec("disconnect", reason);

	return Py_BuildValue("");
}

static PyObject* ekg_printf(PyObject *self, PyObject *pyargs)
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

static PyObject* ekg_command(PyObject *self, PyObject *args)
{
	char *command = NULL;

	if (!PyArg_ParseTuple(args, "s", &command))
		return NULL;

	command_exec(NULL, command);

	return Py_BuildValue("");
}

static PyMethodDef ekg_methods[] = {
	{ "connect", ekg_connect, METH_VARARGS, "" },
	{ "disconnect", ekg_disconnect, METH_VARARGS, "" },
	{ "printf", ekg_printf, METH_VARARGS, "" },
	{ "command", ekg_command, METH_VARARGS, "" },
	{ NULL, NULL, 0, NULL }
};

static PyObject *ekg_config_getattr(PyObject *o, char *name)
{
	list_t l;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		if (!strcmp(v->name, name)) {
			if (v->type == VAR_BOOL || v->type == VAR_INT)
				return Py_BuildValue("i", *(int*)(v->ptr));
			else
				return Py_BuildValue("s", *(char**)(v->ptr));
		}
	}

	return NULL;
}

static int ekg_config_setattr(PyObject *o, char *name, PyObject *value)
{
	struct variable *v = variable_find(name);

	if (!v) {
		PyErr_SetString(PyExc_LookupError, "unknown variable");
		return -1;
	}

	if (v->type == VAR_INT || v->type == VAR_BOOL) {
		if (!PyInt_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "invalid type");
			return -1;
		}
		variable_set(name, itoa(PyInt_AsLong(value)), 0);
	} else {
		if (!PyString_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "invalid type");
			return -1;
		}
		variable_set(name, PyString_AsString(value), 0);
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

	Py_Initialize();

	if (!(ekg = Py_InitModule("ekg", ekg_methods)))
		return -1;

	PyModule_AddStringConstant(ekg, "version", VERSION);

	ekg_config = PyObject_NEW(PyObject, &ekg_config_type);
	PyModule_AddObject(ekg, "config", ekg_config);

	return 0;
}

int python_finalize()
{
	Py_Finalize();

	return 0;
}

int python_run(const char *filename)
{
	FILE *f = fopen(filename, "r");

	if (f) {
		PyRun_SimpleFile(f, (char*) filename);
		fclose(f);
	} else
		return -1;

	return 0;
}

int python_load(const char *filename)
{
	PyObject *mod, *init;
	struct module m;

	if (!(mod = PyImport_ImportModule((char*) filename))) {
		printf("module not found\n");
		return -1;
	}
	
	if ((init = PyObject_GetAttrString(mod, "init"))) {
		PyObject *args = Py_BuildValue("()"), *result;

		result = PyEval_CallObject(init, args);

		Py_XDECREF(result);
		Py_XDECREF(args);
		Py_XDECREF(init);
	}

	memset(&m, 0, sizeof(m));

	m.name = xstrdup(filename);
	m.module = mod;
	m.deinit = PyObject_GetAttrString(mod, "deinit");

	list_add(&modules, &m, sizeof(m));

	return 0;
}

int python_exec(const char *command)
{
	char *tmp = saprintf("import ekg\n%s\n", command);

	PyRun_SimpleString(tmp);
	xfree(tmp);

	return 0;
}

int python_function(const char *name)
{
	return -1;
}
