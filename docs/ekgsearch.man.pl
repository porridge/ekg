.\"                                      Hey, EMACS: -*- nroff -*-
.TH EKGSEARCH 1 "19 lipca 2002"
.\" Please adjust this date whenever revising the manpage.

.SH NAZWA
ekgsearch \- wyszukuje osoby w publicznym katalogu Gadu-Gadu
.SH SK£ADNIA
.B ekgsearch
.RI [ opcje ] \  ...
.br
.SH OPIS
Ta strona podrêcznika opisuje krótko dzia³anie programu
.B ekgsearch
\.
Zosta³a ona napisana dla dystrybucji Debian, poniewa¿ oryginalny program nie
posiada strony podrêcznika.
.PP
\fBekgsearch\fP to program umo¿liwiaj±cy wyszukiwanie osób w publicznym
katalogu Gadu-Gadu.
.SH OPCJE
Poni¿sze opcje umo¿liwiaj± wyszukiwanie osób wed³ug ró¿nych kryteriów.
.TP
.B \-h, \-\-help
Powoduje wypisanie krótkiej pomocy.
.TP
.B \-u, \-\-uin \fInumerek\fP
Szukaj osoby o numerze \fInumerek\fP.
.TP
.B \-f, \-\-first \fIimiê\fP
Szukaj osób o imieniu \fIimiê\fP.
.TP
.B \-l, \-\-last \fInazwisko\fP
Szukaj osób o nazwisku \fInazwisko\fP.
.TP
.B \-n, \-\-nick \fIpseudonim\fP
Szukaj osób o pseudonimie \fIpseudonim\fP.
.TP
.B \-c, \-\-city \fImiasto\fP
Szukaj osób z miasta \fImiasto\fP.
.TP
.B \-b, \-\-born \fImin:max\fP
Szukaj osób urodzonych przed dat± \fImax\fP, ale po \fImin\fP.
.TP
.B \-p, \-\-phone \fItelefon\fP
Szukaj osoby której numer telefonu to \fItelefon\fP.
.TP
.B \-e, \-\-email \fIe\-mail\fP
Szukaj osoby której adres e\-mail to \fIe\-mail\fP.
.TP
.B \-a, \-\-active
Szukaj tylko aktywnych u¿ytkowników.
.TP
.B \-F, \-\-female
Szukaj tylko osób p³ci ¿eñskiej
.TP
.B \-M, \-\-male
Szukaj tylko osób p³ci mêskiej.
.TP
.B \-s, \-\-start \fInumer\fP
Pokazuj tylko osoby których numer GG jest wiêkszy od lub równy \fInumer\fP.
.TP
.B \-\-all
Poka¿ wszystkie pasuj±ce osoby, nie tylko 20 pierwszych.
.TP
.B \-\-debug
Pokazuj informacje diagnostyczne.
.SH PATRZ TE¯
.BR ekg (1).
.br
.SH AUTHOR
Piotr Wysocki <wysek@linux.bydg.org>.
.br
Ta strona podrêcznika zosta³a napisana przez Marcina Owsianego <porridge@debian.org>
dla systemu Debian GNU/Linux (ale mo¿e byæ u¿ywana w innych).

