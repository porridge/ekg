dnl Rewritten from scratch. --speedy 
dnl $Id$

PYTHON=
PYTHON_VERSION=
PYTHON_INCLUDES=
PYTHON_LIBS=

AC_DEFUN(AC_CHECK_PYTHON,[
  AC_SUBST(PYTHON_LIBS)
  AC_SUBST(PYTHON_INCLUDES)

  AC_ARG_WITH(python,
    [[  --with-python     	  Compile with Python bindings]],
    if test "x$withval" != "xno" -a "x$withval" != "xyes"; then
	with_arg="$withval/include:-L$withval/lib $withval/include/python:-L$withval/lib"
    fi
  )

	if test "x$with_python" = "xyes"; then			    
		
		AC_PATH_PROG(PYTHON, python)
		if test "$PYTHON" = ""; then AC_PATH_PROG(PYTHON, python2.2) fi
		if test "$PYTHON" = ""; then AC_PATH_PROG(PYTHON, python2.1) fi
		if test "$PYTHON" = ""; then AC_PATH_PROG(PYTHON, python2.0) fi
		
		if test "$PYTHON" != ""; then 
			PYTHON_VERSION=`$PYTHON -c "import sys; print sys.version[[0:3]]"`
			PYTHON_PREFIX=`$PYTHON -c "import sys; print sys.prefix"`
			echo Found Python version $PYTHON_VERSION [$PYTHON_PREFIX]
		fi

		AC_MSG_CHECKING(for Python.h)
		
		PYTHON_EXEC_PREFIX=`$PYTHON -c "import sys; print sys.exec_prefix"`
		
		if test "$PYTHON_VERSION" != ""; then 
		    if test -f $PYTHON_PREFIX/include/python$PYTHON_VERSION/Python.h ; then 
			AC_MSG_RESULT($PYTHON_PREFIX/include/python$PYTHON_VERSION/Python.h)
			PY_LIB_LOC="-L$PYTHON_EXEC_PREFIX/lib/python$PYTHON_VERSION/config"
			PY_CFLAGS="-I$PYTHON_PREFIX/include/python$PYTHON_VERSION"
			PY_MAKEFILE="$PYTHON_EXEC_PREFIX/lib/python$PYTHON_VERSION/config/Makefile"

			PY_LOCALMODLIBS=`sed -n -e 's/^LOCALMODLIBS=\(.*\)/\1/p' $PY_MAKEFILE`
			PY_BASEMODLIBS=`sed -n -e 's/^BASEMODLIBS=\(.*\)/\1/p' $PY_MAKEFILE`
			PY_OTHER_LIBS=`sed -n -e 's/^LIBS=\(.*\)/\1/p' $PY_MAKEFILE`

			PYTHON_LIBS="-L$PYTHON_EXEC_PREFIX/lib $PY_LIB_LOC -lpython$PYTHON_VERSION $PY_LOCALMODLIBS $PY_BASEMODLIBS $PY_OTHER_LIBS"
			PYTHON_INCLUDES="$PY_CFLAGS"
			AC_DEFINE(WITH_PYTHON, 1, [define if You want python])
			have_python=true
		    fi
		fi

		if test "x$have_python" != "xtrue"; then 
			AC_MSG_RESULT(not found)
		fi
	fi
])

