# -*- coding: ISO-8859-2 -*-
# Skrypt ten uruchamia si� z poziomu ekg (polecenie python) a zadanie jego to
# wy�apywanie adres�w URL w otzymanych wiadomo�ciach. Mozna by w tym miejscu poopowiada� o dzia�aniu skryptu
# ale my�le ze skypt jest dos� rozmowny pozatym by poczyta� helpa (i nie tylko) wystarczy nacisn�� F8
# wszelkie pretensje mo�na kierowa� na adres: rmrmg(at)wp(dot)pl
#
# poprawki bezpiecze�stwa: wojtekka (2005-07-11)

import re
import ekg
import string
import os

browser="firefox"
link=re.compile(".*http.*")
linka=re.compile("http.*")
linkfile=os.path.expanduser("~/.gg/rmrmg_ekg_url")

def init ():
 ekg.printf("generic", "linkownik")
 return 1

def deinit ():
 ekg.printf("generic", "linkownik poszed�")
 return 1 

def launch(url, tab):
    url = string.replace(string.replace(url, ",", "%2c"), "'", "%27");
    
    if tab:
	command = "%s -remote 'openURL(%s, new-tab)'" % (browser, url)
    else:
	command = "%s '%s'" % (browser, url)

    #ekg.printf("generic", "[%s]" % (command))
    os.system(command)

def handle_msg(uin, name, msgclass, text, time, secure):
    #ekg.printf("generic", "echo dzia�a")
    if link.match(text):
	linki=string.split(text)
	for x in linki:
	    if linka.match(x): 
		ekg.printf("generic", "znaleziono link: %s" %(x)) 
		ekg.printf("generic", "by otworzy� w: nowym oknie wcisnij F7, nowej zak�adce F5, by nie otwierac wci�nijF6.")
		ekg.printf("generic", "F8 pokazuje liste przechwyconych link�w; F5-F7 dzia�a na pierwszym linku z listy")
		open(linkfile, 'a').write(x + '\n');
	#ekg.printf("generic","echo tada")
	return 1
    else:
	return 1

def handle_keypress(meta, key):
    if key == 269:
	ekg.printf("generic", "wci�nieto F5")
	nurl=czyjest()
	if nurl == 0:
	    ekg.printf("generic", "nie ma zadnego adresu URL")
	else:
	    dlug=len(nurl)
	    if dlug == 1:
		ekg.printf("generic", "otwieram %s w nowej zak�adce" %(nurl[0]))
		launch(nurl[0], True)
		os.unlink(linkfile)
	    else:
		ekg.printf("generic", "link�w mam %d" %(dlug))
		wielejest(nurl)
		ekg.printf("generic", "otwieram %s w nowej zak�adce" %(nurl[0]))
		launch(nurl[0], True)
    elif key == 270:
	ekg.printf("generic", "wcisni�to F6")
	nurl=czyjest()
	if nurl == 0:
	    ekg.printf("generic", "nic nie moge skasowa� - nie ma zadnego adresu URL")
	else:
	    dlug=len(nurl)
	    if dlug == 1:
		ekg.printf("generic", "kasuje adres %s" %(nurl[0]))	    
		os.unlink(linkfile)
	    else:
		ekg.printf("generic", "jest wiele link�w")
		wielejest(nurl)
		ekg.printf("generic", "kasuje pierwszy czyli:  %s" %(nurl[0]))
    elif key == 271:
    	ekg.printf("generic", "wcisni�to F7")
    	nurl=czyjest()
	if nurl == 0:
	    ekg.printf("generic", "nie ma zadnego adresu URL")
	else:
	    dlug=len(nurl)
	    if dlug == 1:
		ekg.printf("generic", "otwieram %s w nowym oknie" %(nurl[0]))
		launch(nurl[0], False)
		os.unlink(linkfile)
	    else:
		ekg.printf("generic", "link�w mam %d" %(dlug))
		wielejest(nurl)
		ekg.printf("generic", "otwieram %s w nowym oknie" %(nurl[0]))
    elif key == 272:
	ekg.printf("generic", "wcisni�to F8")
    	nurl=czyjest()
	ekg.printf("generic", "F5 - otwiera w nowej zak�adce; F7 w nowym oknie, a F6 kasuje, wszystko tyczy si� pierwszej pozycji z listy")
	if nurl == 0:
	    ekg.printf("generic", "nie ma zadnego adresu URL")
	else:	
	    dlug=len(nurl)
	    ekg.printf("generic", "link�w mam %d oto one:" %(dlug))
	    for po in nurl:
		ekg.printf("generic", "%s" %(po))
    return 1
###########################################################

def czyjest ():
    if os.path.exists(linkfile):
	wejsc= open (linkfile)
	file = wejsc.readlines()
	dlug=len(file)
	wejsc.close()
	#ekg.printf("generic", "liczno�� %d" %(dlug))
	return file
    else:
	return 0
	
def wielejest (buff):
    file=open(linkfile , 'w')		
    #buff= file.readlines()
    #file.truncate()
    #file.writelines
    file.writelines('\n'.join (buff[1:]))
    file.close()
