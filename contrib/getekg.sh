#!/bin/bash
# getekg.sh - skrypt �ci�gaj�cy zawsze najnowsz� wersj� EKG.
# Autor: Arim.
# Ostatnie modyfikacja: 28/05/2002 14:28

if [ "$1" == "" ]; then 
	echo "Podaj zmienne (\"--prefix=/usr\" itp.) dla configure'a!"; 
	exit 1; 
fi
DIR="${TMPDIR:-${TMP:-/tmp}}/getekg-$$"
if ! mkdir "$DIR"; then
	echo "Nie udalo sie utworzyc katalogu tymczasowego \"$DIR\"." >&2
	exit 1
fi
echo "Wchodz� do katalogu tymczasowego \"$DIR\"."
cd "$DIR"
echo -n "�ci�gam najnowsz� wersj�."
 if [ ! -x "`which wget`" ]; then \
    if [ ! -x "`which curl`" ]; then \
        if [ ! -x "`which lynx`" ]; then \
            echo "Brak mo�liwo�ci �ci�gni�cia pliku (chyba, �e pod Xami)";
            exit 1;
        else 
	    lynx -dump http://dev.null.pl/ekg/ekg-current.tar.gz | tar zxvf -
	fi # lynx
    else
	curl -s http://dev.null.pl/ekg/ekg-current.tar.gz | tar xzf -
    fi #curl
 else
    wget -q -O - http://dev.null.pl/ekg/ekg-current.tar.gz | tar xzf -
 fi #wget
echo -n " Rozpakowane.";
cd `ls -1 | grep ekg-`
echo -n " Konfiguruj�..."
./configure $* > /dev/null
echo " Instaluj�..."
make install > /dev/null
cd ..
rm -r `ls -1 | grep ekg-`
echo "Zrobione."
