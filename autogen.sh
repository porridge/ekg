#!/bin/sh
if test $*; then
  ARGS="$*"
else
  ARGS=`grep '^  \$ \./configure ' config.log | sed 's/^  \$ \.\/configure //' 2> /dev/null`
fi
aclocal -I m4
autoheader
autoconf
echo "Running ./configure $ARGS"
test x$NOCONFIGURE = x && ./configure $ARGS
