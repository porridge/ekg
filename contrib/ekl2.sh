#!/bin/bash

# historia:
# v1 (7.03.2002) grywalny skrypt, spelniajacy jako tako swoja funkcje
#	powstal w oczekiwaniu na pewna osobe na gg
# v1.1 (8.03.2002) poprawa drukowania nietypowych znakow, np. {}%*#$
#	po wyslaniu ascii-rozyczki przez gg, w logach wyraznie wymagala ona naprawy
# v1.2 (9.03.2002) poprawa wydajnosci. skrypt jest juz w miare szybki
# v1.3 (28.05.2002) obsluga trybow logowania 1 i 2, paprawka dotyczaca ip


######

if [ $# -lt 1 ]; then
	echo "$0: nie mo¿na znale¼æ pliku logów. U¿yj: $0 -h"
	exit
fi

color="true"	# defaultowo kolory s± w³±czone, by to zmienic usun ta linie, lub uzyj -n
ggsender="$USER" # domyslny naglowek dla wychodzacych odpowiedzi (czyli twoich)
ggftimeformat="%D %T" # domyslny format wyswietlania daty, zgodny z date(1)
ggtimenow=`date +%s` # aktualny czas w sekundach. wartosc potrzebna tylko w jednym miejscu, 
		     #ale poprawia wydajnosc skryptu
ggmaxtstamplimit=10 # maksymalny odstep czasu miedzy wypowiedziami w trybie -t
ggdir="$HOME/.gg"


###### parametry linii polecen

while [ $# -gt 0 ]; do
        case $1 in
		-h) 
		        echo "U¿yj: $0 [-d format] [-h] [-l] [-m name] [-n] [-s date] [-t] [-v] log_file"
			echo -e "\t-d format\tformat wy¶wietlania daty, zobacz date(1)"
		        echo -e "\t-h\t\tpomoc, któr± teraz widzisz"
			echo -e "\t-L\t\tlista dostêpnych logów z katalogu $ggdir/history"
			echo -e "\t-l\t\tlista sesji w wybranym logu"
		        echo -e "\t-m name\t\tdomoy¶lnie \$USER"
		        echo -e "\t-n\t\tbez kolorów"
			echo -e "\t-s date\t\tstart date; czas, odk±d zacz±æ drukownie historii"
		        echo -e "\t-t\t\tzachowaj odstêpy czasowe miêdzy wypowiedziami (symulacja rozmowy)"
		        echo -e "\t-v\t\tversion"
			exit
		;;
		-t) keeptime="yes";;
		-d) ggftimeformat="$2"; shift;;
		-s) ggsdate="$2"; shift;;
		-L) 
			if [ -f "$ggdir/history" ]; then
				for i in `sed "s/^[a-z]*,\([0-9]*\),.*/\1/;/[^0-9]/d;/^$/d" "$ggdir/history" | sort | uniq`; do
					echo "$i "`sed "/$i/!d;s/;.*//;q" "${ggdir}/userlist"`
				done
			else
				for i in `ls "$ggdir/history/"`; do
					echo "$i "`sed "/$i/!d;s/;.*//;q" "${ggdir}/userlist"`
				done
			fi
			exit
		;;
		-l) gglist="yes"; ggdlimit=10800 ;;
		-m) ggsender="$2"; shift;;
		-n) unset color;;
		-v) 
			echo "ekl2.sh version 1.2, for ekg >20020302"
			echo "Covered by GNU GPL, Copyright (c) 2002 Triteno <tri10o@bsod.org>"
			exit
		;;
		*)
			name=`sed "/^$1;/!d;s/.*;\([0-9]*\)/\1/;s/[^0-9]//;q" "$HOME/.gg/userlist"`
			uin="$1"
			dafcknfile="$HOME/.gg/history/$name"
			if [ -f "$HOME/.gg/history" ]; then	# set log 1
				gglog="$HOME/.gg/history";
			else
				if [ -f "$1" ]; then			# set log 2
					gglog="$1";
					elif [ -f "$ggdir/history/$1" ]; then
						gglog="$ggdir/history/$1"; 
						elif [ -f "$dafcknfile" ]; then
							gglog="$dafcknfile";
							else
				        			echo "$0: log_file doesnt exists!"
				        			exit 1
				fi
			fi
		;;
	esac
        shift
done

IFS=$'\n'
for i in `cat "$gglog" | grep ",$uin,"`; do
	gglasttime=$ggtime;
	gg_4=`echo "$i" | cut -f 4 -d "," `
	if [ -z "`echo "$gg_4" | sed "/[0-9]*\..*/d"`" ]; then
		ggtime=`echo "$i" | cut -f 5 -d "," `;
		ggip="(ip: $gg_4)";
	else
		ggtime=$gg_4;
	fi
	[ -z $gglasttime ] && gglasttime=0


	if [ ! -z $gglist ] && [ ! -z $gglasttime ]; then
		[ `expr $ggtime - $gglasttime` -lt $ggdlimit ] && continue
		ggftime=`date --date "$(expr $ggtime - $ggtimenow)sec" +"$ggftimeformat"`
		echo "Sesja GG z: $ggftime $ggip"
	fi

	if [ ! -z "$ggsdate" ]; then
		ggsdatesec=`date --date "$ggsdate" +%s`
		if [ $ggtime -lt $ggsdatesec ]; then continue; fi
	fi
	
	ggway=`echo "$i" | cut -f 1 -d "," `
	gguid=`echo "$i" | cut -f 3 -d "," `
        if [ "$gguid" == "" ]; then gguid="uin`echo $i | cut -f 2 -d "," `"; fi
	if [ "$ggway" == "chatrecv" ] || [ "$ggway" == "msgrecv" ] ; then ggway="${color:+\x1B[0;32m}"; 
				     else ggway="${color:+\x1B[0;33m}"; 
					  gguid=$ggsender
	fi
	
	if [ ! -z $keeptime ] && [ ! -z $gglasttime ]; then
		ggwaittime=`expr $ggtime - $gglasttime`
		if [ $ggwaittime -gt "$ggmaxtstamplimit" ]; then
			ggwaittime=$ggmaxtstamplimit
		fi
		sleep "$ggwaittime";
	fi

	ggftime=`date --date "$(expr "$ggtime" - "$ggtimenow")sec" +"$ggftimeformat"` # UWAGA! linijka 99 taka sama!
	ggmsg=`echo "$i" | sed 's/.*,[0123456789]*,//'`
	echo -e "${ggway}${gguid} [${ggftime}]: ${color:+\x1B[0;38m}\c"
	printf "%s\n" "${ggmsg}"
done 2>/dev/null
