dnl Rewritten from scratch. --wojtekka

AC_DEFUN(AC_CHECK_NCURSES,[
  AC_SUBST(CURSES_LIBS)
  AC_SUBST(CURSES_INCLUDEDIR)

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
    		/usr/include/ncurses: \
		/usr/local/include:-L/usr/local/lib \
		/usr/pkg/include:-L/usr/pkg/lib \
		/usr/contrib/include:-L/usr/contrib/lib \
		"/usr/local/include/ncurses:-L/usr/local/lib -L/usr/local/lib/ncurses" \
		/usr/freeware/include/ncurses:-L/usr/freeware/lib32 \
    		/sw/include:-L/sw/lib; do
	
      incl=`echo "$i" | sed 's/:.*//'`
      lib=`echo "$i" | sed 's/.*://'`
		
      if test -f $incl/ncurses.h; then
        AC_MSG_RESULT($incl/ncurses.h)
	CURSES_LIBS="$lib -lncurses"
	CURSES_INCLUDEDIR="-I$incl"
	has_ncurses=yes
	break
      fi
    done
  fi
])


