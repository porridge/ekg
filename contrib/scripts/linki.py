# Skrypt ten uruchamia siê z poziomu ekg (polecenie python) a zadanie jego to
# wy³apywanie adresów URL w otzymanych wiadomo¶ciach. Mozna by w tym miejscu poopowiadaæ o dzia³aniu skryptu
# ale my¶le ze skypt jest dosæ rozmowny pozatym by poczytaæ helpa (i nie tylko) wystarczy nacisn±æ F8
# wszelkie pretensje mo¿na kierowaæ na adres: rmrmg(at)wp(dot)pl
import re
import ekg
import string
import os

link=re.compile(".*http.*")
linka=re.compile("http.*")
def init ():
 ekg.printf("generic", "linkownik")
 return 1

def deinit ():
 ekg.printf("generic", "linkownik poszed³")
 return 1 

def handle_msg(uin, name, msgclass, text, time, secure):
    #ekg.printf("generic", "echo dzia³a")
    if link.match(text):
	linki=string.split(text)
	for x in linki:
	    if linka.match(x): 
		ekg.printf("generic", "znaleziono link: %s" %(x)) 
		ekg.printf("generic", "by otworzyæ w: nowym oknie wcisnij F7, nowej zak³adce F5, by nie otwierac wci¶nijF6.")
		ekg.printf("generic", "F8 pokazuje liste przechwyconych linków; F5-F7 dzia³a na pierwszym linku z listy")
		os.system("echo \"%s\" >> /tmp/rmrmg_ekg_url" %(x))
	#ekg.printf("generic","echo tada")
	return 1
    else:
	return 1

def handle_keypress(meta, key):
    if key == 269:
	ekg.printf("generic", "wci¶nieto F5")
	nurl=czyjest()
	if nurl == 0:
	    ekg.printf("generic", "nie ma zadnego adresu URL")
	else:
	    dlug=len(nurl)
	    if dlug == 1:
		ekg.printf("generic", "otwieram %s w nowej zak³adce" %(nurl[0]))
		os.system("MozillaFirebird -remote 'openURL(%s, new-tab)'" %(nurl[0]))
		os.system('rm /tmp/rmrmg_ekg_url')
	    else:
		ekg.printf("generic", "linków mam %d" %(dlug))
		wielejest(nurl)
		ekg.printf("generic", "otwieram %s w nowej zak³adce" %(nurl[0]))
		os.system("MozillaFirebird -remote 'openURL(%s, new-tab)'" %(nurl[0]))
    elif key == 270:
	ekg.printf("generic", "wcisniêto F6")
	nurl=czyjest()
	if nurl == 0:
	    ekg.printf("generic", "nic nie moge skasowaæ - nie ma zadnego adresu URL")
	else:
	    dlug=len(nurl)
	    if dlug == 1:
		ekg.printf("generic", "kasuje adres %s" %(nurl[0]))	    
		os.system('rm /tmp/rmrmg_ekg_url')
	    else:
		ekg.printf("generic", "jest wiele linków")
		wielejest(nurl)
		ekg.printf("generic", "kasuje pierwszy czyli:  %s" %(nurl[0]))
    elif key == 271:
    	ekg.printf("generic", "wcisniêto F7")
    	nurl=czyjest()
	if nurl == 0:
	    ekg.printf("generic", "nie ma zadnego adresu URL")
	else:
	    dlug=len(nurl)
	    if dlug == 1:
		ekg.printf("generic", "otwieram %s w nowym oknie" %(nurl[0]))
		os.system("MozillaFirebird %s" %(nurl[0]))
		os.system('rm /tmp/rmrmg_ekg_url')		
	    else:
		ekg.printf("generic", "linków mam %d" %(dlug))
		wielejest(nurl)
		ekg.printf("generic", "otwieram %s w nowym oknie" %(nurl[0]))
    elif key == 272:
	ekg.printf("generic", "wcisniêto F8")
    	nurl=czyjest()
	ekg.printf("generic", "F5 - otwiera w nowej zak³adce; F7 w nowym oknie, a F6 kasuje, wszystko tyczy siê pierwszej pozycji z listy")
	if nurl == 0:
	    ekg.printf("generic", "nie ma zadnego adresu URL")
	else:	
	    dlug=len(nurl)
	    ekg.printf("generic", "linków mam %d oto one:" %(dlug))
	    for po in nurl:
		ekg.printf("generic", "%s" %(po))
    return 1
###########################################################

def czyjest ():
    if os.path.exists('/tmp/rmrmg_ekg_url'):
	wejsc= open ('/tmp/rmrmg_ekg_url')
	file = wejsc.readlines()
	dlug=len(file)
	wejsc.close()
	#ekg.printf("generic", "liczno¶æ %d" %(dlug))
	return file
    else:
	return 0
	
def wielejest (buff):
    file=open('/tmp/rmrmg_ekg_url' , 'w')		
    #buff= file.readlines()
    #file.truncate()
    #file.writelines
    file.writelines('\n'.join (buff[1:]))
    file.close()