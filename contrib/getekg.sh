#!/bin/bash
# getekg.sh - skrypt ¶ci±gaj±cy zawsze najnowsz± wersjê EKG.
# Autor: Arim.
# Ostatnie modyfikacja: 28/05/2002 14:28

if [ "$1" == "" ]; then 
	echo "Podaj zmienne (\"--prefix=/usr\" itp.) dla configure'a!"; 
	exit 1; 
fi
echo "Wchodzê do katalogu tymczasowego."
cd /tmp
echo -n "¦ci±gam najnowsz± wersjê."
wget -o wget.log http://dev.null.pl/ekg/ekg-current.tar.gz
rm wget.log
echo -n " Rozpakowujê..."
gunzip ekg-current.tar.gz
tar xvf ekg-current.tar > /dev/null
rm ekg-current.tar
cd `ls -1 | grep ekg-`
echo -n " Konfigurujê..."
./configure $* > /dev/null
echo " Instalujê..."
make install > /dev/null
cd ..
rm -r `ls -1 | grep ekg-`
echo "Zrobione."
