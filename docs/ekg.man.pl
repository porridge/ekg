.TH EKG n "31 pa�dziernika 2001" 
.SH NAZWA
ekg \- Eksperymentalny Klient Gadu-Gadu
.SH SK�ADNIA
.B ekg [
.BI opcje
.B ]

.SH OPIS
.B ekg
Jest to eksperymentalny klient Gadu-Gadu.

.SH OPCJE
.TP
.BI \-d\ [\-\-debug]
W��cza tryb "debug" - jest to specjalny tryb, pokazuj�cy, co robi 
program w danej chwili.
.TP
.BI \-n\ [\-\-no-auto]
Nie ��cz si� automagicznie. Po uruchomieniu nie ��czy si� automatycznie 
z serwerami gg. 
.TP
.BI \-u "\ " username\ [\-\-user\ username]
Zezwala na uruchomienie ekg z innym \"u�ytkownikiem\". 
Standardowo konfiguracja ekg znajduje sie w katalogu ~/.gg (chyba, �e
zdefiniowana jest zmienna �rodowiskowa $CONFGI_DIR - wtedy w ~/$CONFIG_DIR/gg/)
Ta opcja pozwala na stworzenie struktury podkatalog�w z oddzielnymi
konfiguracjami.
.TP
.BI \-t\ theme\ [\-\-theme\ theme]
�aduje opis wygl�du z podanego pliku
.TP
.BI \-a\ [\-\-away]
po po��czeniu zmienia stan na ,,zaj�ty''
.TP
.BI \-b\ [\-\-back]
po po��czeniu zmienia stan na ,,dost�pny''
.TP 
.BI \-i\ [\-\-invisible]
po po��czeniu zmienia stan na ,,niewidoczny''
.TP
.BI \-p\ [\-\-private]
po po��czeniu zmienia stan na ,,tylko dla przyjaci�''
