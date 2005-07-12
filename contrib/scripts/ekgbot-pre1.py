#!/usr/bin/env python
# -*- coding: ISO-8859-2 -*-
# ekg-bot 0.1-pre1
# Copyright (C) 2003 Andrzej Lindna�
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#
# http://www.gnu.org/copyleft/gpl.html
#
# New releases: http://bot.czad.org/
# Looking for erecoder?: http://eleet.czad.org/

import ekg,re,os,string,types,random,math,base64	# Importy
from urllib import *					# Obs�uga www
from time import *					# Operacje czasowe
from random import Random				# Random
from os import *					# do implementacji popen
#import sys						# debugowanie implementacji popen

# Konfiguracja
owner = twojnumer					# Numer gg ownera

# Check with: $ whereis
path_cat = "/bin/cat"
path_bot = "SCIEZKA DO PLIKU *.PY BOTA"
path_wc = "/usr/bin/wc"
path_uptime = "/usr/bin/uptime"
path_erecoder = "/usr/local/bin/erecoder" # [ http://eleet.czad.org ]
path_uname = "/bin/uname"
path_fetchmail = "/usr/bin/fetchmail"
path_host = "/usr/bin/host"
path_free = "/usr/bin/free"
path_df = "/bin/df"

#################################################################
#     Below is the script's source code.                        #
#     You are allowed to modify it under conditions of GNU GPL. #
#     If you know, what are you doing, of course. :)            #
#################################################################
#     Poni�ej jest kod �r�d�owy skryptu.                        #
#     Mo�esz go modyfikowa� pod warunkami GNU GPL.              #
#     Je�li wiesz, co robisz, oczywi�cie. :)                    #
#################################################################

owner = int(owner)
ver = 'ekg-bot 0.1-pre1 "qm"'
wwwpage = "bot.czad.org"
def handle_msg(uin, name, msgclass, text, time, secure):
	uin = int(uin)
	if len(text) == 0:
		ekg.command("msg %d M�g�by� co� napisa�?;)" % uin)
		return
	elif text[0] in ["!", "@"]: komenda(uin, text)
	elif text[0] == "?": helpf(uin, text)

def komenda(uin, text):
	ownerz = {"private": (private, 1), "owner": (chowner, 1),
			  "save": (saveall, 0), "add": (addnew, 2), "free": (freestats, 0),
			  "dysk": (hddstats, 0), "refstatus": (refstatus, 0), "msg": (mesgtouin, 2), 
			  "checkurl": (ciekurl, 1), "host": (ciekhost, 1), "fetchmail": (fetchmail, 0), 
			  "reconnect": (reconnect, 0), "block": (killfile, 1), "ignore": (ignore, 1), 
			  "msgid": (googleid, 1), "invisible": (invis, 1), "unblock": (unblock, 1), 
			  "unignore": (unignore, 1), "kill": (killme, 0)}
	userz = {"status": (ciekstatus, 1), "uname": (uname, 0), "krot": (krot, 1),
			 "drot": (drot, 1), "kbase": (kbase, 0), "dbase": (dbase, 0), "time": (ciektime, 0),
	                 "uptime": (uptime, 0), "kod": (ciekkod, 0), "hello": (hellou, 2),
			 "rand": (randomik, 2), "sim": (wyslij_klucz, 0), "ile": (ile, 2), "odliczanie": (odlicz, 1),
			 "lotto": (lottomat, 1), "zycie": (zycie, 3), "sin": (sin, 1), "cos": (cos, 1),
			 "tg": (tg, 1), "ctg": (ctg, 1), "help": (helpuj, 0), "kmorse": (kmorse, 0),
			 "dmorse": (dmorse, 0), "bmi": (bmi, 2) }

	if text[0] == "@":
		if uin != owner:
			ekg.command("msg %d Czy Ty aby na pewno jeste� w�a�cicielem tego bota?;)" % uin)
			return
		tablica = ownerz
	elif text[0] == "!": tablica = userz
	else: return

	pozycja = text.find(" ")
	if pozycja == -1:
		kom = text[1:]
		kom = kom.lower()
		arg = ""
	else:
		kom = text[1:pozycja]
		kom = kom.lower()
		arg = text[pozycja+1:]

	try: funkcja, ilosc = tablica[kom]
	except:	return

	# jesli komenda wymaga argumentow
	if ilosc > 0:
		splitarg = arg.split()
		args = splitarg[:ilosc]
		if len(splitarg) > ilosc:
			args.append(string.join(splitarg[ilosc:]))
	# jesli komenda nie wymaga argumentow
	elif ilosc == 0:
		if len(arg) > 0: args = [arg]
		else: args = []
	# bledny wpis w liscie komend
	else: return

	try: funkcja(uin, *args)
	except TypeError, exc:
		emsg = exc.args[0]
		if emsg.find(funkcja.__name__ + "()") != -1:
			ekg.command("msg %d Z�a ilo�� parametr�w. Wpisz polecenie: ?%s" % (uin, kom))
		else:
			ekg.command("msg %d B��d wykonania: %s" % (uin, emsg))
		return
	except Exception, exc:
		emsg = exc.args[0]
		ekg.command("msg %d B��d wykonania: %s" % (uin, emsg))
		return


def init():												
	ekg.printf("generic", "Zaladowano ekg-bot %s!" % ver)
	ekg.command("away %s @ %s" % (ver, wwwpage))

def deinit():
	ekg.printf("generic", "Usunieto ekg-bota %s!" % ver)

def odliczanie(dzien, miesiac):
	teraz = mktime((localtime()[0],int(miesiac),int(dzien)+1,0,0,0,0,0,1))-time()
	if teraz < 0:
		teraz = mktime((localtime()[0]+1,int(miesiac),int(dzien),0,0,0,0,0,1))-time()
	elif teraz == 0:
		zostanie = 0
		return int(zostanie)
	zostanie = teraz/86400
	return int(zostanie)

def lotto(ilosc_kulek, ilosc_losowanych_kulek):
	komora = range(1,ilosc_kulek)
	wylosowane = []
	for x in range(ilosc_losowanych_kulek):
		liczba = random.choice(komora)
		komora.remove(liczba)
		wylosowane.append(liczba)
	wylosowane.sort()
	wynik = ""
	for x in wylosowane:
		wynik += "%d" % x + ", "
	return wynik[:-2]

def czyPrzestepny(rok):
	przestepny = 0
	if rok % 4 == 0: przestepny = 1
	if rok % 100 == 0: przestepny = 0
	if rok % 400 == 0: przestepny = 1
	return przestepny

def poprawna_data(dzien, miesiac, rok):
	try:
		dzien = int(dzien)
		miesiac = int(miesiac)
		rok = int(rok)
		if (dzien > 31 or dzien < 1) or (miesiac > 12 or miesiac < 1) or rok < 1900:
			return 0
		mce = {1: 31, 2: 28, 3: 31, 4: 30, 5: 31, 6: 30, 7: 31, 8: 31, 9: 30, 10: 31, 11: 30, 12: 31}
		przestepny = czyPrzestepny(rok)
		if miesiac != 2 and dzien > mce[miesiac]: return 0
		if miesiac == 2 and ((przestepny and dzien > 29) or (not przestepny and dzien > 28)):
			return 0
		return 1
	except: return 0

def zycie(uin, dzien, miesiac, rok):
	try:
		dzien = int(dzien)
		miesiac = int(miesiac)
		rok = int(rok)
	except:	
		ekg.command("msg %d Co� nie tak z dat�, ech." % uin)
		return
	if not poprawna_data(dzien, miesiac, rok):
		ekg.command("msg %d Niepoprawny dzie�, miesi�c lub rok. Czy to takie trudne?:)" % uin)
		return
	(y,m,d) = localtime()[:3]
	qmk = time()-mktime((rok,miesiac,dzien,0,0,0,0,0,1))
	if rok > y:
		ekg.command("msg %d Niepoprawny rok..." % uin)
		return
	if qmk < 0:
		ekg.command("msg %d Niepoprawny dzie� lub miesi��." % uin)
		return
	if rok == y and dzien == d and miesiac == m:
		ekg.command("msg %d Urodzi�e� si� dzisiaj? Zdolny bobas." % uin)
		return
	dni = qmk/86400
	lat = qmk/31556736
	ekg.command("msg %d To jest ju� Tw�j %.0f dzie� �ycia. �yjesz ju� %.0f lat (%.0f sekund). %s" % (uin, dni, math.floor(lat), qmk, zodiak(dzien, miesiac)))

def zodiak(dzien, miesiac):
	if (miesiac == 3 and dzien >= 21) or (miesiac == 4 and dzien <= 20): qmju = "Tw�j znak zodiaku to baran."
	elif (miesiac == 4 and dzien >= 21) or (miesiac == 5 and dzien <= 20): qmju = "Tw�j znak zodiaku to byk."
	elif (miesiac == 5 and dzien >= 21) or (miesiac == 6 and dzien <= 21): qmju = "Tw�j znak zodiaku to bli�ni�ta."
	elif (miesiac == 6 and dzien >= 22) or (miesiac == 7 and dzien <= 22): qmju = "Tw�j znak zodiaku to rak."
	elif (miesiac == 7 and dzien >= 23) or (miesiac == 8 and dzien <= 23): qmju = "Tw�j znak zodiaku to lew."
	elif (miesiac == 8 and dzien >= 24) or (miesiac == 9 and dzien <= 22): qmju = "Tw�j znak zodiaku to panna."
	elif (miesiac == 9 and dzien >= 23) or (miesiac == 10 and dzien <= 23): qmju = "Tw�j znak zodiaku to waga."
	elif (miesiac == 10 and dzien >= 24) or (miesiac == 11 and dzien <= 21): qmju = "Tw�j znak zodiaku to skorpion."
	elif (miesiac == 11 and dzien >= 23) or (miesiac == 12 and dzien <= 21): qmju = "Tw�j znak zodiaku to strzelec."
	elif (miesiac == 12 and dzien >= 22) or (miesiac == 1 and dzien <= 20): qmju = "Tw�j znak zodiaku to kozioro�ec."
	elif (miesiac == 1 and dzien >= 21) or (miesiac == 2 and dzien <= 19): qmju = "Tw�j znak zodiaku to wodnik."
	elif (miesiac == 2 and dzien >= 20) or (miesiac == 3 and dzien <= 20): qmju = "Tw�j znak zodiaku to ryby."
	else: qmju = " "
	return qmju

def trygonometria(dzialanie, wartosc):
	dzial = int(wartosc) / 360.0 * math.pi * 2
	if dzialanie == "sin": wynik = math.sin(dzial)
	elif dzialanie == "cos": wynik = math.cos(dzial)
	elif dzialanie == "tg": wynik = math.tan(dzial)
	elif dzialanie == "ctg": wynik = 1 / math.tan(dzial)
	return wynik

def helpf(uin, text):
	if text.find(" ") == 1:
		ekg.command("msg %d Nie nale�y podawa� argument�w." % uin)
		return
	pol = text[1:]
	helpy = {"private": "Wywo�anie:\r\n@private on/off - ustawia tryb \"Tylko dla znajomych\" na bocie.", 
			 "owner": "Wywo�anie:\r\n@owner on/off - ustawia tryb \"Dost�pny\" na w��czony lub wy��czony.", 
			 "save": "Wywo�anie:\r\n@save - zapisuje i wysy�a plik konfiguracyjny razem z list� kontakt�w na serwer GaduGadu",
			 "add": "Wywo�anie:\r\n@add uin nazwa - dodaje numer do listy kontakt�w", 
			 "free": "Wywo�anie:\r\n@free - wysy�a statystyki pami�ci na serwerze, gdzie uruchomiony jest bot", 
			 "dysk": "Wywo�anie:\r\n@df -h - wysy�a statystyki dysku twardego, na kt�rym uruchomiony jest bot", 
			 "refstatus": "Wywo�anie:\r\n@refstatus - od�wie�a status",
			 "msg": "Wywo�anie:\r\n@msg uin tre�� - wysy�a pod numer GaduGadu tre�� wiadomo�ci", 
			 "checkurl": "Wywo�anie:\r\n@checkurl adres_strony - sprawdza na jakim serwerze serwowana jest:) podana strona. UWAGA! Adres NIE MO�E zawiera� http:// ani \"/\" na ko�cu! np. @checkurl czad.org", 
			 "host": "Wywo�anie:\r\n@host ip/host - sprawdza numer IP hosta, lub host numeru IP:)", 
			 "fetchmail": "Wywo�anie:\r\n@fetchmail - wywo�uje \"fetchmail\"'a, program �ci�gaj�cy poczt� z innych serwer�w. Aby z tego skorzysta�, u�ytkownik musi skontaktowa� si� z administratorem lub konfiguruj�c program wcze�niej samemu.",
			 "reconnect": "Wywo�anie:\r\n@reconnect - ��czy ponownie z serwerem GG", 
			 "block": "Wywo�anie:\r\n@block uin - blokuje podany numer GaduGadu", 
			 "ignore": "Wywo�anie:\r\n@ignore uin - ignoruje podany numer GaduGadu", 
			 "msgid": "Wywo�anie:\r\n@msgid msg-id - wysy�a linka, na kt�ry nale�y wej�c, aby przeczyta� podany Message-ID (z usenetu)",
			 "status": "Wywo�anie:\r\n!status uin - sprawdza status podanego numeru GaduGadu [dane pobiera ze skryptu na stronie programu GaduGadu]",
			 "uname": "Wywo�anie:\r\n!uname - podaje system operacyjny, na jakim chodzi bot",
			 "krot": "Wywo�anie:\r\n!krot 2-23 tekst - koduje podany tekst systemem ROT. Liczba musi by� z przedzia�u od 2 do 23",
			 "drot": "Wywo�anie:\r\n!drot 2-23 tekst - dekoduje podany tekst w systemie ROT. Liczba musi by� z przedzia�u 2-23",
			 "kbase": "Wywo�anie:\r\n!kbase tekst - koduje podany tekst do systemu Base64",
			 "dbase": "Wywo�anie:\r\n!dbase tekst - dekoduje tekst podany w systemie Base64",
			 "time": "Wywo�anie:\r\n!time - podaje aktualn� dat�, godzin� i czas [letni lub zimowy]",
			 "uptime": "Wywo�anie:\r\n!uptime - podaje aktualny UpTime serwera, na kt�rym stoi bot", 
			 "kod": "Wywo�anie:\r\n!kod - podaje aktualn� ilo�� linii kodu bota oraz ilo�� bajt�w jak� on zajmuje",
			 "hello": "Wywo�anie:\r\n!hello uin nick - wysy�a do ownera bota pro�b�, o dodanie do listy kontakt�w. Owner bota otrzyma wiadomo�� zawieraj�c� UIN osoby wysy�aj�cej, UIN osoby zg�aszanej i nick.",
			 "rand": "Wywo�anie:\r\n!rand liczba1 liczba2 - losuje liczbe z podanego przedzia�u. Pierwsza liczba musi by� wi�ksza od drugiej. Liczby mog� by� ujemne",  
			 "sim": "Wywo�anie:\r\n!sim - wysy�a pod numer klucz bota do rozm�w szyfrowanych. Przydatne u�ytkownikom programu PowerGG + wtyczki GaduCrypt", 
			 "ile": "Wywo�anie:\r\n!ile dzie� miesi�c - wylicza ile pozosta�o dni do podanej przez u�ytkownika daty",
			 "odliczanie": "Wywo�anie:\r\n!odliczanie parametr - podaje ile zosta�o dni do ustalonego \"terminu\". Obs�ugiwane terminy: wakacje, rok.",
			 "lotto": "Wywo�anie:\r\n!lotto parametr - bot losuje liczby do losowa� Lotto. Obs�ugiwane losowania: multi, duzy, express, zaklady",
			 "zycie": "Wywo�anie:\r\n!zycie dzie� miesi�c rok - bot podaje informacje wykorzystuj�c podan� dat� urodzenia", 
			 "sin": "Wywo�anie:\r\n!sin parametr - podaje warto�� funkcji sinus dla k�ta",
			 "cos": "Wywo�anie:\r\n!cos parametr - podaje warto�� funkcji cosinus dla k�ta",
			 "tg": "Wywo�anie:\r\n!tg parametr - podaje warto�� funkcji tangens dla k�ta",
			 "ctg": "Wywo�anie:\r\n!ctg parametr - podaje warto�� funkcji cotangens dla k�ta",
			 "kmorse": "Wywo�anie:\r\n!kmorse tekst - koduje podany tekst do alfabetu Morse'a. Tekst nie mo�e zawiera� znak�w specjalnych i polskich literek.", 
			 "dmorse": "Wywo�anie:\r\n!dmorse tekst - dekoduje podany alfabet Morse'a do zwyk�ego tekstu. Je�li tekst nie b�dzie alfabetem morse'a bot odpisze tym samym tekstem.",
			 "bmi": "Wywo�anie:\r\n!bmi waga wzrost - podaje Body Mass Index. Waga musi by� podana w kilogramach, wzrost w centymetrach",
			 "help": "Wywo�anie:\r\n!help - podaje wszystkie polecenia obs�ugiwane przez bota.",
			 "invisible": "Wywo�anie:\r\n!invisible on/off - ustawia tryb \"niewidoczny\" na bocie.",
			 "unblock": "Wywo�anie:\r\n@unblock uin - usuwa blokad� z podanego uin'u",
			 "unignore": "Wywo�anie:\r\n@unignore uin - usuwa ignorowanie z podanego uin'u",
			 "kill": "Wywo�anie:\r\n@kill - wy��cza bota razem z klientem GG"}

	if helpy.has_key(pol.lower()):
		ekg.command("msg %d %s" % (uin, helpy[pol.lower()]))
	else: ekg.command("msg %d Nieznane polecenie ?%s" % (uin, pol))
	
def private(uin, tryb):
	tryb = tryb.lower()
	if tryb == "on":
		ekg.command("private on")
		ekg.command("msg %d Zmieniono tryb \"Tylko dla znajomych\" na w��czony" % uin)
	elif tryb == "off":
		ekg.command("private off")
		ekg.command("msg %d Zmieniono tryb \"Tylko dla znajomych\" na wy��czony" % uin)
	else: ekg.command("msg %d Funkcja @private przyjmuje tylko warto�ci \"on\" lub \"off\"" % uin)

def chowner(uin, tryb):
	status_ref = strftime("%a, %d %b %Y %H:%M:%S %Z")
	tryb = tryb.lower()
	if tryb == "on": ekg.command("back %s @ %s" % (ver, wwwpage))
	elif tryb == "off": ekg.command("away %s @ %s" % (ver, wwwpage))
	else: ekg.command("msg %d Funkcja @owner przyjmuje tylko warto�ci \"on\" lub \"off\"" % uin)

def saveall(uin):
	ekg.command("echo Zapisuj�...")
	ekg.command("save")
	ekg.command("echo Pr�buj� wys�a� na serwer...")
	ekg.command("list -P")
	ekg.command("msg %d Zapisano ustawienia i spr�bowano je wys�a� na serwer razem z list� kontakt�w" % uin)
	# Dlaczego spr�bowano? Bo nie wiadomo czy dotar�y one na serwer GG.

def addnew(uin, duin, nazwa):
	try: 
		ekg.command("add %d %s" % (int(duin), nazwa))
		ekg.command("msg %d Doda�em %s (%s) do listy kontakt�w." % (uin, duin, nazwa))
	except:	ekg.command("msg %d Pierwszy parametr to UIN, a drugi NAZWA." % uin)

def freestats(uin):
	free = os.popen("%s -m" % path_free).read()
	ekg.command("msg %d %s" % (uin, free))

def hddstats(uin):
	df = os.popen("%s -h" % path_df).read()
	ekg.command("msg %d %s" % (uin, df))

def refstatus(uin):
	status_ref = strftime("%a, %d %b %Y %H:%M:%S %Z")
	ekg.command("away %s @ %s" % (ver, wwwpage))

def mesgtouin(uin, to, tresc):
	try: ekg.command("msg %d %s" % (int(to), tresc))
	except:	ekg.command("msg %d Pierwszy parametr to UIN odbiorcy, drugi to tre��." % uin)

def ciekurl(uin, url):
	checkurl = URLopener().open(("http://%s/" % url))
	ekg.command("msg %d %s" % (uin, checkurl.info().getheader('Server')))

def ciekhost(uin, host):
	qmer = safepopen([path_host, host])
	ekg.command("msg %d %s" % (uin, qmer))

def fetchmail(uin):
	os.popen("%s" % path_fetchmail)
	ekg.command("msg %d Wywo�a�em fetchmaila." % uin)

def reconnect(uin):
	ekg.disconnect("Reconnecting...")
	ekg.connect()
	ekg.command("away %s @ %s" % (ver, wwwpage))

def killfile(uin, ktos):
	try:
		ktos = int(ktos)
		ekg.command("block %d" % ktos)
		ekg.command("msg %d %d zosta� zablokowany" % (uin, ktos))
	except: ekg.command("msg %d Musisz poda� parametr jako UIN do zablokowania" % uin)

def unblock(uin, numer):
	try:
		numer = int(numer)
		ekg.command("unblock %d" % numer)
		ekg.command("msg %d %d zosta� odblokowany" % (uin, numer))
	except:	ekg.command("msg %d Musisz poda� parametr jako UIN do odblokowania" % uin)

def ignore(uin, ktos):
	try:
		ktos = int(ktos)
		ekg.command("ignore %d" % ktos)
		ekg.command("msg %d %d zosta� ignorowany" % (uin, ktos))
	except:	ekg.command("msg %d Musisz poda� parametr jako UIN do ignorowania" % uin)

def unignore(uin, numer):
	try:
		numer = int(numer)
		ekg.command("unignore %d" % numer)
		ekg.command("msg %d %d zosta� odignorowany" % (uin, numer))
	except:	ekg.command("msg %d Musisz poda� parametr jako UIN do odignorowania" % uin)

def googleid(uin, msgid):
	ekg.command("msg %d Link do MessageID, kt�ry poda�e�: http://groups.google.pl/groups?selm=%s" % (uin, str(msgid)))

def ciekstatus(uin, kto):
	try:
		checkurl = URLopener().open(("http://www.gadu-gadu.pl/users/status.asp?id=%d&styl=2" % int(kto)))
		ciek = checkurl.read()
		if ciek == "3":
			sztatus = "zaraz wraca"
		elif ciek == "2":
			sztatus = "jest dost�pny"
		elif ciek == "1":
			sztatus = "jest niedost�pny"
		else:
			sztatus = "[b��d, nie mog� poda� statusu]"
		ekg.command("msg %d Numer %d %s" % (uin, int(kto), sztatus))
	except:
		ekg.command("msg %d Przyda�o by si� jako parametr poda� UIN;)" % uin)

def uname(uin):
	ekg.command("msg %d %s" % (uin, os.popen("%s -mnrs" % path_uname).read()))

def krot(uin, ile, tekst):
	ile = int(ile)
	if ile < 2 or ile > 23:	ekg.command("msg %d Jako pierwszy parametr nale�y poda� liczb� z zakresu 2-23." % uin)
	else:
		try:
			ekg.command("msg %d %s" % (uin, safepopen([path_erecoder, '-ear'+ile, '--', tekst])))
		except:	ekg.command("msg %d Jako pierwszy parametr nale�y poda� liczb� z zakresu 2-23." % uin)

def drot(uin, ile, tekst):
	ile = int(ile)
	if ile < 2 or ile > 23: ekg.command("msg %d Jako pierwszy parametr nale�y poda� liczb� z zakresu 2 - 23." % uin)
	else:
		try:
			ekg.command("msg %d %s" % (uin, safepopen([path_erecoder, '-dar'+ile, '--', tekst])))
		except:	ekg.command("msg %d Jako pierwszy parametr nale�y poda� liczb� z zakresu 2-23" % uin)

def kbase(uin, tekst):
	zak = base64.encodestring(tekst)
	ekg.command("msg %d %s" % (uin, zak))

def dbase(uin, tekst):
	dek = base64.decodestring(tekst)
	ekg.command("msg %d %s" % (uin, dek))

def ciektime(uin):
	dni = {0: "poniedzia�ek", 1: "wtorek", 2: "�roda", 3: "czwartek", 4: "pi�tek", 5: "sobota", 6: "niedziela"}
	miesiace = {1: "stycznia", 2: "luty", 3: "marca", 4: "kwietnia", 5: "maja", 6: "czerwca", 7: "lipca", 8: "sierpnia", 9: "wrze�nia", 10: "pa�dziernika", 11: "listopada", 12: "grudnia"}
	czasy = {1: "letni", 2: "zimowy"}
	tajm = localtime()
	ekg.command("msg %d Dzisiaj jest %s, %d %s %d roku. Jest godzina %d:%d:%d, czas %s." % (uin, dni[tajm[6]], tajm[2], miesiace[tajm[1]], tajm[0], tajm[3], tajm[4], tajm[5], czasy[tajm[8]]))

def uptime(uin):
	up = os.popen("%s" % path_uptime).read()
	inter = string.join(up.split()[1:4])
	ekg.command("msg %d %s" % (uin, inter))

def ciekkod(uin):
	cmd = os.popen("%s %s | %s -l" % (path_cat, path_bot, path_wc))
	cmd2 = os.stat("%s" % path_bot)
	ekg.command("msg %d Aktualnie bot posiada %s linii kodu oraz zajmuje %s bajt�w." % (uin, cmd.read().replace(" ", ""), cmd2[6]))

def hellou(uin, kto, nick):
	try:
		ekg.command("msg %d Ok, przyj��em." % uin)
		ekg.command("msg %d Zg�osi� si�:\r\nUIN wysy�aj�cego: %d\r\nUIN zg�aszanego: %d\r\nImi� zg�aszanego: %s\r\n" % (owner, int(uin), int(kto), re.escape(nick)))
	except:	ekg.command("msg %d Pierwszy musi by� numer. Nie przyj��em." % uin)

def randomik(uin, p, d):
	if abs(int(p)) > abs(int(d)):
		ekg.command("msg %d Pierwsza liczba musi by� mniejsza od drugiej." % uin)
	elif abs(int(p)) > 2147483646 or abs(int(d)) > 2147483646:
		ekg.command("msg %d Ciutke za du�a liczba." % uin)
	elif int(p) == int(d):
		ekg.command("msg %d Wylosowana z tych samych liczb zosta�a liczba %d" % (uin, int(p)))
	else:
		a = Random()
		wyn = a.randint(int(p), int(d))
		try: ekg.command("msg %d %d" % (uin, wyn))
		except:	ekg.command("msg %d Nale�y poda� dwie LICZBY." % uin)

def wyslij_klucz(uin):
	ekg.command("key -s %d" % uin)
	ekg.command("msg %d Klucz zosta� wys�any." % uin)

def ile(uin, dzien, miesiac):
	try:
		dzien = int(dzien)
		miesiac = int(miesiac)
		if not poprawna_data(dzien, miesiac, localtime()[0]):
			ekg.command("msg %d Fajne dni i miesi�ce, nie ma co." % uin)
			return
		ekg.command("msg %d Do daty %d.%d pozosta�o %s dni." % (uin, dzien, miesiac, odliczanie(dzien, miesiac)))
	except:	ekg.command("msg %d Musisz poda� dwie liczby:\r\npierwsza - dzie� miesi�ca\r\ndruga - miesi�c." % uin)

def odlicz(uin, termin):
	try:
		termin = termin.lower()
		if termin == "wakacje":
			ekg.command("msg %d Do wakacji (21 czerwca) pozosta�o %s dni." % (uin, odliczanie(21,6)))
		elif termin == "rok":
			ekg.command("msg %d Do ko�ca roku kalendarzowego pozosta�o %s dni." % (uin, odliczanie(1,1)))
		else:
			ekg.command("msg %d Nieznany parametr." % uin)
	except:	ekg.command("msg %d Nieznany termin. Obs�ugiwane terminy:\r\nwakacje\r\neuropa\r\nrok" % uin)

def lottomat(uin, typ):
	typ = typ.lower()
	if typ == "multi":
		ekg.command("msg %d Wylosowane przez komputer liczby do losowa� MultiLotka to: %s" % (uin, lotto(81, 10)))
	elif typ == "duzy" or typ == "du�y":
		ekg.command("msg %d Wylosowane przez komputer liczby do losowa� Du�egoLotka to: %s" % (uin, lotto(49, 6)))
	elif typ == "express" or typ == "ekspress":
		ekg.command("msg %d Wylosowane przez komputer licbzy do losowa� ExpressLotka to: %s" % (uin, lotto(42, 5)))
	elif typ == "zaklady" or typ == "zak�ady":
		ekg.command("msg %d Wylosowane przez komputer liczby do losowa� Zak�ad�w Specjalnych to: %s" % (uin, lotto(42,5)))
	else:
		ekg.command("msg %d Nieznany parametr! Parametry:\r\nmulti, duzy, express, zaklady" % uin)

def sin(uin, liczba):
	try: ekg.command("msg %d %s" % (uin, trygonometria('sin', liczba)))
	except:	ekg.command("msg %d B��dne wywo�anie." % uin)

def cos(uin, liczba):
	try: ekg.command("msg %d %s" % (uin, trygonometria('cos', liczba)))
	except:	ekg.command("msg %d B��dne wywo�anie." % uin)

def tg(uin, liczba):
	try: ekg.command("msg %d %s" % (uin, trygonometria('tg', liczba)))
	except:	ekg.command("msg %d B��dne wywo�anie." % uin)

def ctg(uin, liczba):
	try: ekg.command("msg %d %s" % (uin, trygonometria('ctg', liczba)))
	except:	ekg.command("msg %d B��dne wywo�anie." % uin)

def helpuj(uin):
	ekg.command("""msg %d Polecenia ownera:\r\n@private, @invisible, @owner, 
		@save, @add, @free, @dysk, @refstatus, @msg, @checkurl, @host, 
		@fetchmail, @reconnect, @block, @ignore, @msgid.\r\nPolecenia 
		u�ytkownik�w:\r\n!status, !uname, !krot, !drot, !kbase, !dbase, 
		!time, !uptime, !kod, !hello, !rand, !sim, !ile, !odliczanie, 
		!lotto, !zycie, !sin, !cos, !tg, !ctg, !kmorse, !dmorse, !bmi
		\r\nPomoc do ka�dego z polece� mo�na uzyska� pisz�c wiadomo�� 
		?polecenie. Na przyk�ad: ?status.\r\nAutorem tego bota jest Andrzej Lindna�""" % uin)

def kmorse(uin, tekst):
	ekg.command("msg %d %s" % (uin, safepopen([path_erecoder, '-eam', '--', tekst])))

def dmorse(uin, tekst):
	ekg.command("msg %d %s" % (uin, safepopen([path_erecoder, '-dam', '--', tekst])))

def bmi(uin, masa, wzrost):
	try:
		masa = float(masa)
		wzrost = float(wzrost)
	except:
		ekg.command("msg %d Cyferki, cyferki:)" % uin)
		return
	if masa <= 0 or wzrost <= 0:
		ekg.command("msg %d Kiepsko u Ciebie z wag�..." % uin)
		return
	bmi = masa / (wzrost / 100.0) ** 2
	if bmi <= 18.5: qm = "niedowag�"
	elif bmi > 18.5 and bmi < 25.0: qm = "normaln� wag�"
	elif bmi >= 25.0: qm = "nadwag�"
	ekg.command("msg %d Tw�j BMI to %.2f. Masz %s." % (uin, bmi, qm))

def invis(uin, tryb):
	tryb = tryb.lower()
	if tryb == "on":
		ekg.command("invisible")
		ekg.command("msg %d Zmieniono tryb \"niewidoczny\" na w��czony" % uin)
	elif tryb == "off":
		ekg.command("away")
		ekg.command("msg %d Zmieniono tryb \"niewidoczny\" na wy��czony" % uin)
	else: ekg.command("msg %d Funkcja @invisible przyjmuje tylko warto�ci \"on\" lub \"off\"" % uin)

def killme(uin):
	ekg.command("msg %d Wy��czam EKG. Aby mnie ponownie w��czy�, b�dziesz musia� zalogowa� si� na shella niestety;))" % uin)
	ekg.command("quit")


def safepopen(cmd):
	rfd, wfd = pipe()
	ret = fork()
	# child
	if ret == 0:
		close(rfd)
		dup2(wfd, 1)
		execv(cmd[0], cmd)
		exit(1)
	# parent
	else:
		close(wfd)
#		sys.stderr.write('reading..')
		readstring = read(rfd, 4094)
#		sys.stderr.write('got it!')
		while read(rfd, 4096) != '':
			pass
#		sys.stderr.write('EOF')
		waitpid(ret, 0)
#		sys.stderr.write('reaped')
		return readstring

