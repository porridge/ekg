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
    		/sw/include:-L/sw/lib; do
	
      incl=`echo "$i" | sed 's/:.*//'`
      lib=`echo "$i" | sed 's/.*://'`
		
      if test -f $incl/ncurses/ncurses.h; then
        AC_MSG_RESULT($incl/ncurses/ncurses.h)
	CURSES_LIBS="$lib -lncurses"
	CURSES_INCLUDES="-I$incl/ncurses"
	have_ncurses=true
	AC_DEFINE(HAVE_NCURSES)
	break
      elif test -f $incl/ncurses.h; then
        AC_MSG_RESULT($incl/ncurses.h)
	CURSES_LIBS="$lib -lncurses"
	CURSES_INCLUDES="-I$incl"
	have_ncurses=true
	AC_DEFINE(HAVE_NCURSES)
	break
      fi
    done
  fi

  if test "x$have_ncurses" != "xtrue"; then
    AC_MSG_RESULT(not found)
  fi
])


