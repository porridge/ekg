# aclocal.m4 generated automatically by aclocal 1.5

# Copyright 1996, 1997, 1998, 1999, 2000, 2001
# Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

dnl Based on AC_NEED_STDINT_H by Guido Draheim <guidod@gmx.de> that can be
dnl found at http://www.gnu.org/software/ac-archive/. Do not complain him
dnl about this macro.
dnl
dnl $Id$

AC_DEFUN([AC_NEED_STDINT_H],
 [AC_MSG_CHECKING([for uintXX_t types])

  if test "x$1" = "x"; then
    ac_stdint_h="stdint.h"
  else
    ac_stdint_h="$1"
  fi

  rm -f $ac_stdint_h

  ac_header_stdint=""
  for i in stdint.h inttypes.h sys/inttypes.h sys/int_types.h sys/types.h; do
    if test "x$ac_header_stdint" = "x"; then
      AC_TRY_COMPILE([#include <$i>], [uint32_t foo], [ac_header_stdint=$i])
    fi
  done

  if test "x$ac_header_stdint" != "x" ; then
    AC_MSG_RESULT([found in <$ac_header_stdint>])
    if test "x$ac_header_stdint" != "xstdint.h" ; then
      echo "#include <$ac_header_stdint>" > $ac_stdint_h
    fi
  else
    AC_MSG_RESULT([not found, using reasonable defaults])
    
    cat > $ac_stdint_h << EOF
#ifndef __AC_STDINT_H
#define __AC_STDINT_H 1

/* ISO C 9X: 7.18 Integer types <stdint.h> */

#define __int8_t_defined
typedef   signed char    int8_t;
typedef unsigned char   uint8_t;
typedef   signed short  int16_t;
typedef unsigned short uint16_t;
typedef   signed int    int32_t;
typedef unsigned int   uint32_t;

#endif /* __AC_STDINT_H */
EOF
  fi
])



dnl Rewritten from scratch. --wojtekka
dnl $Id$

AC_DEFUN(AC_CHECK_NCURSES,[
  AC_SUBST(CURSES_LIBS)
  AC_SUBST(CURSES_INCLUDES)

  AC_ARG_WITH(ncurses,
    [[  --with-ncurses[=dir]    Compile with ncurses/locate base dir]],
      if test "x$withval" = "xno" ; then
        without_ncurses=yes
      elif test "x$withval" != "xyes" ; then
        with_arg=$withval/include:-L$withval/lib
      fi)

  if test "x$without_ncurses" != "xyes" ; then
    AC_MSG_CHECKING(for ncurses.h)

    for i in $with_arg \
    		/usr/include: \
		/usr/local/include:"-L/usr/local/lib -L/usr/local/lib/ncurses" \
		/usr/pkg/include:-L/usr/pkg/lib \
		/usr/contrib/include:-L/usr/contrib/lib \
		/usr/freeware/include:-L/usr/freeware/lib32 \
    		/sw/include:-L/sw/lib \
    		/cw/include:-L/sw/lib; do
	
      incl=`echo "$i" | sed 's/:.*//'`
      lib=`echo "$i" | sed 's/.*://'`
		
      if test -f $incl/ncurses/ncurses.h; then
        include=$incl/ncurses
      elif test -f $incl/ncurses.h; then
        include=$incl
      fi

      if test "x$include" != "x"; then
        AC_MSG_RESULT($include/ncurses.h)
	CURSES_LIBS="$lib"
	CURSES_INCLUDES="-I$include"
	have_ncurses=true
	AC_DEFINE(HAVE_NCURSES)
	AC_CHECK_LIB(ncurses, initscr,
	  [CURSES_LIBS="$CURSES_LIBS -lncurses"],
	  [AC_CHECK_LIB(curses, initscr,
	    [CURSES_LIBS="$CURSES_LIBS -lcurses"])])
	break
      fi
    done
  fi

  if test "x$have_ncurses" != "xtrue"; then
    AC_MSG_RESULT(not found)
  fi
])



dnl Rewritten from scratch. --wojtekka
dnl $Id$

AC_DEFUN(AC_CHECK_READLINE,[
  AC_SUBST(READLINE_LIBS)
  AC_SUBST(READLINE_INCLUDES)

  AC_ARG_WITH(readline,
    [[  --with-readline[=dir]   Compile with readline/locate base dir]],
    if test "x$withval" = "xno" ; then
      without_readline=yes
    elif test "x$withval" != "xyes" ; then
      with_arg="$withval/include:-L$withval/lib $withval/include/readline:-L$withval/lib"
    fi)

  AC_MSG_CHECKING(for readline.h)

  if test "x$without_readline" != "xyes"; then
    for i in $with_arg \
	     /usr/include: \
	     /usr/local/include:-L/usr/local/lib \
             /usr/freeware/include:-L/usr/freeware/lib32 \
	     /usr/pkg/include:-L/usr/pkg/lib \
	     /sw/include:-L/sw/lib \
	     /cw/include:-L/sw/lib \
	     /net/caladium/usr/people/piotr.nba/temp/pkg/include:-L/net/caladium/usr/people/piotr.nba/temp/pkg/lib; do
    
      incl=`echo "$i" | sed 's/:.*//'`
      lib=`echo "$i" | sed 's/.*://'`

      if test -f $incl/readline/readline.h ; then
        AC_MSG_RESULT($incl/readline/readline.h)
        READLINE_LIBS="$lib -lreadline"
        READLINE_INCLUDES="-I$incl/readline -I$incl"
        AC_DEFINE(HAVE_READLINE)
        have_readline=true
        break
      elif test -f $incl/readline.h ; then
        AC_MSG_RESULT($incl/readline.h)
        READLINE_LIBS="$lib -lreadline"
        READLINE_INCLUDES="-I$incl"
        AC_DEFINE(HAVE_READLINE)
        have_readline=true
        break
      fi
    done
  fi

  if test "x$have_readline" != "xtrue"; then
    AC_MSG_RESULT(not found)
  fi
])


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
		
		AC_PATH_PROG(PYTHON, python python2.2 python2.1 python2.0 python1.6)
		
		if test "$PYTHON" != ""; then 
			PYTHON_VERSION=`$PYTHON -c "import sys; print sys.version[[0:3]]"`
			echo Found Python version $PYTHON_VERSION
		fi

		AC_MSG_CHECKING(for Python.h)
		
		if test "$PYTHON_VERSION" != ""; then 
			for i in $with_arg \
				/usr/include: \
				/usr/local/include:-L/usr/local/lib \
				/usr/freeware/include:-L/usr/freeware/lib32 \
				/usr/pkg/include:-L/usr/pkg/lib \
				/sw/include:-L/sw/lib \
				/cw/include:-L/sw/lib; do
				
				incl=`echo "$i" | sed 's/:.*//'`
				lib=`echo "$i" | sed 's/.*://'`
				
				if test -f $incl/python$PYTHON_VERSION/Python.h ; then 
					AC_MSG_RESULT($incl/python$PYTHON_VERSION/Python.h)
					PYTHON_LIBS="$lib -lpython$PYTHON_VERSION"
					PYTHON_INCLUDES="-I$incl/python$PYTHON_VERSION"
					AC_DEFINE(WITH_PYTHON)
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


