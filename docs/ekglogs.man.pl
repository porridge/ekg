.TH EKGLOGS 1 "7 wrze�nia 2003"
.SH NAZWA
ekglogs \- program do wypisywania historii rozm�w EKG
.SH SK�ADNIA
.B ekglogs [
.BI opcje
.B ]
<
.B ~/.gg/history

.SH OPIS
.B ekglogs
pobiera logi ekg ze standardowego wej�cia i wypisuje je w �adnej formie na
standardowe wyj�cie.
	
.SH OPCJE
.TP
.BI \-t
nie wy�wietlaj w og�le czasu
.TP
.BI \-d
nie wy�wietlaj zmiany dni
.TP
.BI \-x
zawsze wy�wietlaj ksywk�
.TP
.BI \-v
nigdy nie wy�wietlaj ksywki 
(domy�lnie: wy�wietlaj je�li nie podano opcji `u' ani `n')
.TP
.BI \-r
wy�wietlaj czas odebrania w wiadomo�ciach przychodz�cych
.TP
.BI \-R
j/w i wyr�wnaj wiadomo�ci wychodz�ce
.TP
.BI \-s
wy�wietlaj sekundy
.TP
.BI \-S
wy�wietlaj sekundy przy czasie odebrania
.TP
.BI \-C
wy�wietlaj dwukropek mi�dzy godzin� a minut�
.TP
.BI \-b
nie wy�wietlaj b��dnych linijek (zaczynaj� si� od !!!)
.TP
.BI \-a
nie wy�wietlaj zmian statusu
.TP
.BI \-c
wy�wietlaj kolorki
.TP
.BI \-h
wy�wietl kr�tk� pomoc
.TP
.BI \-u\ UIN
pokazuj tylko wiadomo�ci od/do \fIUIN\fR
.TP
.BI \-n\ NICK
pokazuj tylko wiadomo�ci od/do \fINICK\fRa
.PP
Opcji nie trzeba poprzedza� `-', mo�na je ��czy� lub nie.
.PP
Opcje `u' oraz `n' powinny by� na ko�cu i nie powinny wyst�powa� razem

.SH AUTOR
Robert Goliasz <rgoliasz@poczta.onet.pl>
.SH "PATRZ TAK�E"
.BR ekg (1)
