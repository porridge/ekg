#!/bin/bash

# EKG new version by drJojo <jojo@slackware.pl>
# 
# -c || --check     Sprawd¼ czy jest nowsza wersja.
# -g || --get	    Sprawd¼ czy jest nowsza wersja i j± ¶ci±gnij.
# -i || --install   Sprawd¼ czy jest nowsza wersj±, ¶ci±gnij j± i zainstaluj.
#
# Zmienne dla ~/.ekgnv :
# WGET= ¶cie¿ka do wget'a
# EKGTMP= gdzie zostanie utworzony podkatalog na pliki tymaczasowe
# EKGWWW= adres strony ekg.
# EKGCONF= co ma podawaæ do configure gdy automatycznie budujesz ekg
# LASTEKG= ostatnia zainstalowana werja ekg


function options {
  echo "EKG new version by drJojo <jojo@slackware.pl>
 ekgnv.sh -c || -g || -i
   -c || --check     Sprawd¼ czy jest nowsza wersja.
   -g || --get 	     Sprawd¼ czy jest nowsza wersja i j± ¶ci±gnij.
   -i || --install   Sprawd¼ czy jest nowsza wersj±, \
¶ci±gnij j± i zainstaluj."
exit 1
}
  
# Czy jest plik z ustawieniami, je¿eli nie to go stwórz.
if [ ! -f ~/.ekgnv ]; then \
 echo -n "Brak pliku z konfiguracj±. Tworze .ekgnv "
  touch ~/.ekgnv
  echo "WGET=`which wget`
EKGTMP=/tmp
EKGWWW=http://ekg.chmurka.net/
EKGCONF=\"--prefix=/usr --with-shared --with-ioctl\"
LASTEKG=ekg-00000000" >> ~/.ekgnv
 echo " Gotowe!"
 echo
fi

# Wczytaj ustawienia.
  . ~/.ekgnv

# wymyslamy bezpieczny podkatalog
EKGTMPS="$EKGTMP/ekgnv-$$"
mkdir "$EKGTMPS"
if [ "$?" != "0" ]; then
	echo "Proba utworzenia katalogu tymczasowego \"$EKGTMPS\" nie powiodla sie." >&2
	echo "Posprzataj \"$EKGTMP\" i sprobuj ponownie." >&2
	exit 1
fi

# Czy w systemie jest wget?
function check_wget {
 if [ ! -x "$WGET" ]; then \
   echo "Nie masz wget'a!"; 
   exit 1; 
 fi
}

# Pobierz strone z downloadem ekg.
function get_list {
 check_wget
 echo -n "¦ci±gam listê wersji EKG. Poczekaj chwilê. "
  wget -q -P $EKGTMPS $EKGWWW/download.php
 # to mozna zamienic na odczytywanie pliku ktory bylby automatycznie po
 # twojej stronie generowany, a w ktorym bylby tylko numer najnowszej 
 # werjsji
 LASTES="`grep ekg-20 $EKGTMPS/download.php | cut -d\  -f6 | \
  	   cut -d\\" -f2 | tail -1 | sed -e s/.tar.gz//`"
  rm -f $EKGTMPS/download.php*
 echo "Gotowe!"
}

# Sprawd¼ czy jest nowsza wersja.
function check_new {
 get_list
 NEW=0
 # ten warunek jest wystraczalny, bo nie ma mozliwosci zeby pojawilas sie
 # jakas starsza wersja
  if [ "$LASTES" != "$LASTEKG" ]; then \
    echo -n "Jest nowsza wersja $LASTES. "
    NEW=1
  else
    echo "Masz najnowsz± wersje EKG."
    exit 1
  fi 
}

# Pobierz, je¿eli jest nowsza wersja.
function get_new {
 check_new
  if [ ! -z "$NEW" ]; then \
   echo -n "¦ci±gam j±, poczekaj chwilê. "
   wget -q -P $EKGTMPS $EKGWWW/$LASTES.tar.gz
   cat ~/.ekgnv | sed -e s/$LASTEKG/$LASTES/ > ~/.ekgtmp
 # lub jak ktos nie ma seda to grep -v "LASTEKG" ~/.ekgnv > ~/.ekgtmp
 # echo "export LASTEKG=$LASTES" >> ~/.ekgtmp
   mv -f ~/.ekgtmp ~/.ekgnv
   echo "Gotowe.
Plik znajduje siê w $EKGTMPS/$LASTES.tar.gz"
  else 
   echo 
  fi
}

# Zbuduj je¿eli jest nowsza wersja.
function build_new {
 get_new
  if [ ! -z "$NEW" ]; then \
   echo -n "Buduje nowe EKG. Poczekaj chwilê. "
   ( cd $EKGTMPS ;
   tar -zxf $LASTES.tar.gz ; cd $LASTES ;
     ./configure $EKGCONF > /dev/null ; # tylko b³êdy bêd± na konsoli.
      make ; make install > /dev/null ;
      cd .. ; rm -rf $LASTES $LATES.tar.gz ; )
   echo "Skoñczy³em. Masz ju¿ najnowsz± wersjê."
  fi
}

  case $1 in
	"-c")
	    check_new
	    echo
	    ;;
	"--check")
	    check_new
	    echo
	    ;;
	"-g")
	    get_new
	    ;;
	"--get")
	    get_new
	    ;;
	"-i")
	    build_new
	    ;;
	"--install")
	    build_new
	    ;;
	*)
	    options
	    ;;
  esac
  
	    
	
