/*
 * bla, bla, GPL fau dwa. kolejny magiczny, eksperymentalny kod.
 */

#include <Python.h>
#include "stuff.h"
#include "themes.h"
#include "commands.h"

static PyObject* ekg_connect(PyObject *self, PyObject *args)
{
	do_connect();

	return Py_BuildValue("");
}

static PyObject* ekg_printf(PyObject *self, PyObject *pyargs)
{
	char *format, *args[9];
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
	{ "printf", ekg_printf, METH_VARARGS, "" },
	{ "command", ekg_command, METH_VARARGS, "" },
	{ NULL, NULL, 0, NULL }
};

int python_initialize()
{
	Py_Initialize();
	Py_InitModule("ekg", ekg_methods);

	return 0;
}

int python_finalize()
{
	Py_Finalize();

	return 0;
}

int python_load(const char *filename)
{
	FILE *fp = fopen(filename, "r");

	if (fp) {
		PyRun_SimpleFile(fp, filename);
		fclose(fp);
	}

	return 0;
}
