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
			echo Found Python version $PYTHON_VERSION
		fi

		AC_MSG_CHECKING(for Python.h)
		
		if test "$PYTHON_VERSION" != ""; then 
			for i in $with_arg \
				/usr/include:-L/usr/lib \
				/usr/local/include:-L/usr/local/lib \
				/usr/freeware/include:-L/usr/freeware/lib32 \
				/usr/pkg/include:-L/usr/pkg/lib \
				/sw/include:-L/sw/lib \
				/cw/include:-L/cw/lib; do
				
				incl=`echo "$i" | sed 's/:.*//'`
				lib=`echo "$i" | sed 's/.*://'`
				
				if test -f $incl/python$PYTHON_VERSION/Python.h ; then 
					AC_MSG_RESULT($incl/python$PYTHON_VERSION/Python.h)
					PYTHON_LIBS="$lib $lib/python$PYTHON_VERSION/config -lpython$PYTHON_VERSION"
					PYTHON_INCLUDES="-I$incl/python$PYTHON_VERSION"
					AC_DEFINE(WITH_PYTHON, 1, [define if You want python])
					have_python=true
					break
				fi
			done
		fi

		if test "x$have_python" != "xtrue"; then 
			AC_MSG_RESULT(not found)
		fi
	fi
])

