/*

.---------------------------------------------------------------------------.
| ekg - wirtualne okienka 0.9.29   by nils <nils@kki.net.pl>   ekg: 251880  |
`---------------------------------------------------------------------------'
     Lines: 669, Size: 19903, Release date: Mon Feb 18 17:03:53 CET 2002


Compilation:
------------

$ gcc ekg-windows.c -o ekg-windows -ltermcap -lreadline


ToDo list:
----------

Wlasciwie wszystkie zalozenia zostaly zrealizowane. Mozna ewentualnie
troche popracowac nad optymalizacja kodu.

Przydaloby sie jeszcze jakos bindowac wszystkie klawisze alt-1... alt-9
do tej samej funkcji, a nie do 9 osobnych.

Program z powodzeniem przeszedl 5 minutowy test na "glupote uzytkownika".
Nie wysypal sie wiec stwierdzam, ze jest "quasi-stabilny". Czy faktycznie...?


Whats New:
----------

** 0.9.29

  - przepisana funkcja dodaj_do_bufora()
  - przepisana funkcja przywroc_ekran()
  - poprawiony blad w implementacji /wkill
  - inne niewielkie optymalizacje kodu
  - dodane archiwym pseudo-CVS:
	  ftp://cvs:nowewersje@terrorysci.org

** 0.7.99

  - dodana obsluga wirtualnych okienek
  - niewielka optymalizacja
  - zmiana kilku tablic o stalej wielkosci na dynamicznie
	  alokowane zmienne wskaznikowe

** 0.5.99

  - dodana obsluga klawiszy Alt-1, Alt-2, etc..

** 0.3.99

  - pierwsza naprawde dzialajaca wersja
  - obsluga /query

** wersje wczesniejsze niz 0.3.99 nie byly na tyle stabilne
   aby mogly zostac komukolwiek pokazane.


Known Bugs:
-----------

(none)


*/

/* Deklaracja uzywanych plikow naglowkowych */
  #include <stdio.h>
  #include <stdarg.h>
  #include <stdlib.h>
  #include <sys/types.h>
  #include <sys/file.h>
  #include <sys/stat.h>
  #include <sys/errno.h>
  #include <curses.h>
  #include <readline/readline.h>
  #include <readline/history.h>

/* Definicje */
  // Maxymalna dlugosc komendy
  #define MAX_COMMAND_LEN 4096
	// Maxymalna dlugosc prompt'a
  #define MAX_PROMPT_LEN 1024
	// Maxymalna ilosc obslugiwanych okien
  #define MAX_WINDOWS 9
	// Ilosc linii przewidziana na ekran, 50 powinno wystarczyc na kazdy
  // framebuffer, tak mi sie wydaje... wicej chyba nie ma sensu.
  #define MAX_LINES_PER_SCREEN 50
	// Nasza mapa klawiatury
  #define MY_KEYMAP emacs_meta_keymap
  // Komunikat wyswietlany w przypadku niepowodzenia malloc()'a
  #define ERR_NO_MEMORY "Ooops! Nie moglem zaalokowac pamieci na bufor :(\n"

/* Definiowanie struktur */

  // Struktura t_bufor_ekranu
  typedef struct {
     char *linia[MAX_LINES_PER_SCREEN];
		 int ostatni;
	   } t_bufor_ekranu;
		 
  // Struktura t_okno
  typedef struct {
     char query[MAX_PROMPT_LEN];
     char prompt[MAX_PROMPT_LEN];
		 t_bufor_ekranu bufor;
	   } t_okno;

  // Struktura t_komenda
  typedef struct {
     char *nazwa;
     Function *funkcja;
     char *pomoc;
   } t_komenda;

/* Deklaracje uzytych funkcji */
  // inicjalizacja readline
  int initialize_readline(); 
  // wykonuje zadana komende
  int wykonaj_komende();
  // Zmienia prompt
  int podmien_prompt();
  // Usuwa spacje z komendy
  char *usun_spacje();
	// Tworzy nowe okienko
  int c_window_new();
	// Usuwa aktualne okno
  int c_window_delete();
	// Przechodzi do nastepnego okna
  int c_window_next();
	// Przechodzi do poprzedniego okna
  int c_window_prev();
  // Przelacza okna
  int c_window_switch();
  // Uruchamia tryb rozmowy
  int c_query();
	// Pomoc
  int c_help();
	// Wyjscie z programu
  int c_quit();
  // Funkcja zmienia okna
	int c_window_sw();
  // Do tych funkcji zostaja zabindowane klawisze alt-1, alt-2, alt-2,
	// wszystkie te funkcje wywoluja c_window_sw().
	int c_window_01(); int c_window_02(); int c_window_03();
	int c_window_04(); int c_window_05(); int c_window_06();
	int c_window_07(); int c_window_08(); int c_window_09();
  // wyszukuje zadana komende i zwraca wskaznik do przypisanej do niej funkcji
  t_komenda *szukaj_komendy();
	
/* Globalne zmienne */

	// Kazda wartosc zmiennej rozna od zera spowoduje, ze program zakonczy sie
	// zaraz po najblizszym przejsciu przez glowna petle programu.
  int done;
  // Aktualnie aktywne okno
  int aktywne_okno;
	// Ilosc aktywnych okien
  int ilosc_okien;
  // Nasz aktualny prompt. Z niego zawsze bedzie korzystal readline
  char prompt[MAX_PROMPT_LEN];
  // Definicja dostepnych okien
  t_okno okna[MAX_WINDOWS+1];
  // Definicja dostepnych komend
  t_komenda komendy[] =
    {
     { "/wadd",  c_window_new,    "Dodaje nowe okno"},
     { "/wkill", c_window_delete, "Usuwa bierzace okno"},
     { "/wnext", c_window_next,   "Przeskakuje do nastepnego okna"},
     { "/wprev", c_window_prev,   "Przeskakuje do poprzedniego okna"},
     { "/query", c_query,         "Uruchamia tryb rozmowy z uzytkownikiem"},
     { "/help",  c_help,          "Pokazuje pomoc"},
     { "/q",     c_quit,          "Konczy dzialanie programu"},
     {(char *)NULL,(Function *)NULL,(char *)NULL}
    };

/* Czysci bufor i ekran dla bierzacego okna */
int oczysc_ekran()
   {
	 int j;
	 // Czyscimy ekran
   printf("\033[H\033[J");
	 // A nastepnie zerujemy wszystkie linie w buforze aktualnego ekranu.
   for (j=0;j<MAX_LINES_PER_SCREEN;j++)
	   if (okna[aktywne_okno].bufor.linia[j])
	      okna[aktywne_okno].bufor.linia[j][0]=0;
	 // Musimy jeszcze narysowac prompt, wraz z tym co user juz wpisal
	 // do linii komend
	 printf("%s%s", prompt, rl_line_buffer);
	 return(0);
	 }

/* Dodaje linie do bufora ekranu dla danego okna */
int dodaj_do_bufora(int numer_okna, char *zawartosc)
   {
	 int j=1;
	 // Jezeli nie ma nic do dodania to zwracamy sterowanie
	 if (!zawartosc) return(-1);
   // Pobieramy numer ostatniego elementu (pierwszego dodanego, czyli
	 // pierwszego w kolejce do usuniecia)
	 j=okna[numer_okna].bufor.ostatni;
   // jezeli byl juz zaalokowany to go zwalniamy
	 if (okna[numer_okna].bufor.linia[j])
	    {
			free(okna[numer_okna].bufor.linia[j]);
			}
   // no i alokujemy miejsce na nowy text.
   if((okna[numer_okna].bufor.linia[j]=(char*)malloc(strlen(zawartosc)+2))!=NULL)
      {
      // jezeli alokacja sie udala to zastepujemy najstarszy element nowym
      snprintf(okna[numer_okna].bufor.linia[j],strlen(zawartosc)+2,zawartosc);
      }
   else
	    {
			// a jezeli nie, to dajemy stosowny komunikat
      printf(ERR_NO_MEMORY);
			// i wracamy
      return(-1);
		  }
   // jezeli doszlismy do tego momentu, to znaczy, ze zamienilismy najstarszy
	 // element, nowym i nalezy zmienic "wskaznik" ostatniego elementu, a wiec
	 // przesuwamy go... na nastepny (czyli ten ktory byl do tej pory przedostatni)
	 okna[numer_okna].bufor.ostatni++;
	 // Musimy sprawdzic, czy przypadkiem nowy "wskaznik" nie pokazuje ostatniej
	 // dostepnej dla nas linii bufora, bo jezeli tak, to musimy go przestawic
	 // na poczatek tablicy bufora ekranu
	 if(okna[numer_okna].bufor.ostatni==MAX_LINES_PER_SCREEN)
	    {
			okna[numer_okna].bufor.ostatni=1;
			}
   return(0);
	 }

/*
 * UWAGA!!! Ta funkcja nie powinna byc uzywana!!!
 * Jest ona umieszczona tylko dla celow tego engine'u, a konkretnie
 * po to, aby zcentralizowac wypisywanie textow na ekranie tak, aby
 * wszystko co zostaje wypisane zostalo dodane do bufora ekranu.
 *
 * Funkcja uzywa zmiennych tablicowych o stalej wielkosci, jest napisana
 * w sposob nieelegancki i powinna zostac zastapiona czyms lepszym.
 *
 */
int my_printf(char *fmt, ...)
{
va_list ap;
int d;
char c, *s, buforek[MAX_COMMAND_LEN], buf[MAX_COMMAND_LEN];
va_start(ap, fmt);
buf[0]=0;
buforek[0]=0;

while (*fmt)
  {
	  if (*(fmt)!='%')
		  {
			sprintf(buf,"%c",*fmt++);
			}
		else
			{
      switch(*++fmt)
			    {
          case 's':
                s = va_arg(ap, char *);
                sprintf(buf,"%s", s);
                break;
          case 'd':
                d = va_arg(ap, int);
                sprintf(buf,"%d", d);
                break;
          case 'c':
                c = va_arg(ap, char);
                sprintf(buf,"%c", c);
                break;
          }	
			 }
	  strcat(buforek,buf);
	}
va_end(ap);
dodaj_do_bufora(aktywne_okno,buforek);				 
return printf(buforek);
}

/* Przywraca ekran dla okreslonego okna */
int przywroc_ekran(int numer_okna)
   { 
	 int j=0;
	 // Czyscimy ekran
	 printf("\033[H\033[J");
	 // I rysujemy linijka po linijce wszystko co mamy w buforze
	 // Zaczynamy od ostatniego (najstarszego) i rysujemy az do
	 // konca bufora..
	 for (j=okna[numer_okna].bufor.ostatni;j<MAX_LINES_PER_SCREEN;j++)
	   {
      if (okna[numer_okna].bufor.linia[j])
		    {
		     printf("%s",okna[numer_okna].bufor.linia[j]);
			  }
		 }
	 // musimy jednak przewidziec, ze wskaznik nie pokazywal na poczatku
	 // na pierwszy element tablicy (bufora ekranu) (to chyba normalne ? :P)
	 // wtedy musimy "dorysowac" jeszcze liniie ktore zostaly PRZED wskaznikiem
	 // w tablicy.
	 for (j=0;j<okna[numer_okna].bufor.ostatni;j++)
	   {
      if (okna[numer_okna].bufor.linia[j])
		    {
		     printf("%s",okna[numer_okna].bufor.linia[j]);
			  }
		 }
   // przykladowa sytuacja, mamy taki bufor skladajacy sie z 4 linii:
	 // 1:  ta linia zostala dodana jako trzecia
	 // 2:  ta linia zostala dodana jako ostatnia
	 // 3>  linia na ktora jest najstarsza (dodana najwszczesniej)
	 // 4:  ta linia zostala dodana jako druga
	 //
	 // musimy wiec napisac linie ze wskaznikiem (zaznaczona ">") a nastepnie
	 // az do konca czyli linie 4, potem musimy wrocic i dopisac jeszcze linie
	 // przed wskaznikiem... czyli 1,2
	 return(0);
	 }

/* Glowna funkcja */
int main(int argc, char **argv)
 {
   // Przydadza nam sie zmienne
   char *line, *s, *tmp;
	 // Inicjalizujemy readline. A w nim bindujemy klawiesze, etc.
   initialize_readline(); 
	 // Inicjalizujemy zmienne globalne:
   // Ilosc aktywnych okien na poczatku to 1
	 ilosc_okien=1;
	 // Ilosc aktywnych okien na poczatku to 1
	 aktywne_okno=1;
   // ustawiamy prompt dla tego okna i inicjalizujemy go.
	 podmien_prompt(1, "", 1);
	 // Wskaznik ostatno dodanej linii ustawiamy na 1 
	 okna[aktywne_okno].bufor.ostatni=1;
	 // Wyczyscimy ekran i przedstawimy sie..
	 printf("\033[H\033[J");
	 my_printf("\n\033[1;33m" \
			"Witaj! Jestem nowym enginem dla Eksperymentalnego Klienta Gadu-gadu.\n" \
	    "by nils <nils@kki.net.pl>\n\n" \
	    "Napisz /help aby uzyskac pomoc. Baw sie dobrze..." \
			"\033[0;0m\n\n");
	 // No i zaczynamy glowna petle.. Bedzie ona wykonywana do momenntu
	 // az done nie przyjmie wartosci innej niz 0.
   for(;done==0;)
     {
       // wczytujemy linie
       line = readline(prompt);
       // Jezeli linia nie wczytana to konczymy petle.
			 if(!line) break;
		   // zapisujemy linie
			 // i dodajemy do bufora aktualnego ekranu (jesli sie da:))
       if ((tmp=(char *)malloc(strlen(prompt)+strlen(line)+2))!=NULL)
			    {
           snprintf(tmp, strlen(prompt)+strlen(line)+2, "%s%s\n", prompt, line);
			     dodaj_do_bufora(aktywne_okno,tmp);
			     free(tmp);
					}
			 else printf(ERR_NO_MEMORY);
			 // Wsuwany zbedne spacje z wczytanej linii
       s = usun_spacje(line);
			 // Jezeli po usunieciu spacji jest co wykonac
       if(*s)
         {
				   // to dodajemy to do historii
           add_history(s);
					 // i sprawdzamy czy jestesmy w trybie rozmowy..
					 if ((s[0]!='/')&&(strlen(okna[aktywne_okno].query)>0))
					   {
					   // jezeli tak, to wysylamy wiadomosc..
						 my_printf("Wiadomosc wyslana do %s.\n", okna[aktywne_okno].query);
						 }
					 // w przeciwnym wypadku wykonujemy polecenie
					 else wykonaj_komende(s);
         }
			 // juz mozemy zwolnic zmienna zainicjalizowana przez readline()
			 // zostanie ona ponownie zaincjalizowana przy kolejnym wywolaniu
       free(line);
     }
	// koniec programu. Zwracamy do srodowiska 0.
	// Program zakonczyl sie bezblednie.
   exit(0);
 }

 /* Wykonaj komende */
int wykonaj_komende(char *line)
  {
   t_komenda *komenda;
   register int i = 0;
   char *word;

   // Oddzielamy komende od parametrow
   while(line[i] && whitespace(line[i])) i++;
   word = line + i;

   while(line[i] && !whitespace(line[i])) i++;

   // Dodajemy ascii 0 zaraz za komenda
   if(line[i]) line[i++] = '\0';

   // Staramy sie odnalesc podana komende
   komenda = szukaj_komendy(word);

   // Jezeli komenda nie zostala odnaleziona
   if(!komenda)
	   {
		   // to kwitujemy to odpowiednim komunikatem
       my_printf("Nie wiem co to znaczy: %s\n", word);
			 // i zwracamy sterowanie
       return(-1);
     }

   // Teraz pobierzemy przekazany do komendy parametr
   while(whitespace(line[i])) i++;
   word = line + i;

   // I wywolamy odpowienia funkcje
   return((*(komenda->funkcja))(word));
  }

/* Zwraca adres znalezionej komendy */
t_komenda *szukaj_komendy(char *nazwa)
  {
   register int i;
   // przeszukujemy tablice komend
   for(i = 0; komendy[i].nazwa; i++)
	   // i jezeli znajdziemy zadana komende
     if(strcmp(nazwa, komendy[i].nazwa) == 0)
		   // to zwracamy
       return(&komendy[i]);
   // jezeli nie odnajdziemy komendy w tabeli to zwracamy "NULL"
   return((t_komenda *)NULL);
  }

/* Usuwa zbedne spacje z komendy */
char *usun_spacje(char *string)
  {
   register char *s, *t;
   for(s = string; whitespace(*s); s++);
	 
   if(*s == 0) return(s);

   t = s + strlen(s) - 1;
   while(t > s && whitespace(*t)) t--;
   *++t = '\0';

   return s;
  }

/* Inicjalizujemy readline */
int initialize_readline()
  {
	 //int i;
	 //char a[5];
   // Dopuszczamy warunkowe przetworzenie pliku ~/.inputrc
   rl_readline_name = "ekg";
	 // Bedziemy czytac co najwyzej MAX_COMMAND_LEN bajtów
	 rl_num_chars_to_read = MAX_COMMAND_LEN;
   // Bindujemy klawisze od alt-1 do alt-0:
	 /*
	 for (i=0; i<=9; i++)
	   {
	    sprintf(a,"%d\0",i);
      rl_generic_bind(ISFUNC, a, (char *)c_window_sw, MY_KEYMAP);
		 }
	 */
	 // Bardzo malo eleganckie, ale nie mialem pomyslu jak przekazac
	 // parametr do funkcji (wogole da sie w ten sposob przekazac jakos
	 // parametr?), wtedy mozna by bylo bindowac wszystkie klawisze
	 // do jednej funkcji tak jak w petli powyzej.
	 // A dopuki tego nie ma to funkcja c_window_sw jest wywolywana z
	 // funkcji c_window_0? ;(
   rl_generic_bind(ISFUNC, "1", (char *)c_window_01, MY_KEYMAP);
   rl_generic_bind(ISFUNC, "2", (char *)c_window_02, MY_KEYMAP);
   rl_generic_bind(ISFUNC, "3", (char *)c_window_03, MY_KEYMAP);
   rl_generic_bind(ISFUNC, "4", (char *)c_window_04, MY_KEYMAP);
   rl_generic_bind(ISFUNC, "5", (char *)c_window_05, MY_KEYMAP);
   rl_generic_bind(ISFUNC, "6", (char *)c_window_06, MY_KEYMAP);
   rl_generic_bind(ISFUNC, "7", (char *)c_window_07, MY_KEYMAP);
   rl_generic_bind(ISFUNC, "8", (char *)c_window_08, MY_KEYMAP);
   rl_generic_bind(ISFUNC, "9", (char *)c_window_09, MY_KEYMAP);
	 // Bindujemy czysczenie ekranu do ctrl+l
	 // Normalnie to by bylo niepotrzebne bo ctrl+l samo w sobie czysci
	 // ale my musimy wyczyscic jeszcze bufory ekranu
	 rl_bind_key(12, oczysc_ekran);
	 return(0);
  }

/* Zmienimy prompt */
int podmien_prompt(int numer_okna, char *text_prompta, int zainicjowac)
  {
   // Jezeli zostal podany argument
	 if (text_prompta)
	   {
       // to wsadzamy otrzymany parametr do zmiennej prompt okreslonego okna
       snprintf(okna[numer_okna].prompt, MAX_PROMPT_LEN, "%d: %s $ ",
			                                          aktywne_okno, text_prompta);
       // i ustawiamy nowy prompt jezeli zaszla taka potrzeba
       if (zainicjowac!=0)
			   {
         snprintf(prompt, MAX_PROMPT_LEN, okna[numer_okna].prompt);
		     rl_expand_prompt(prompt);
				 }
     }
   // i tutaj zwracamy sterowanie
   return(1);
	}
	
/* Tworzymy nowe okno */
int c_window_new()
  {
	 // Jezeli ilosc aktywnych okien jest mniejsza od dopuszczalnej
	 if (ilosc_okien<MAX_WINDOWS)
	   {
	   // to dodajemy nowe okno
		 ilosc_okien++;
		 // jego zmienna query dla nowego okna (zmienne prompt zostanie
		 // wyzerowana przez podmien_prompt() w dalszej czesci.
		 okna[ilosc_okien].query[0]=0;
		 // i aktualizujemy prompt, nie zmieniajac jednoczesnie aktywnego prompta
     podmien_prompt(ilosc_okien, "", 0);
	   }
   return(1);
  }

/* Usuwamy biezace okno */
int c_window_delete()
 {
   int i,k;
	 // jesli mamy wiecej niz jedno okno
	 if (ilosc_okien>1)
	   {
		 // i okno ktore usuwamy nie jest oknem ostanim
     if (aktywne_okno!=ilosc_okien)
	     {
			 // to przesuwamy wszystkie okna liczac od bierzacego
			 // o jeden do tylu
	     for (i=aktywne_okno;i<ilosc_okien;i++)
		     {
				 // k to zmienna, ktora bedzie nam pokazywac okno
				 // ktore wstawiamy zamiast okna i
				 k=i+1;
				 // podmieniamy okno i oknem k
				 okna[i]=okna[k];
  			 }
       }
		 // i zmniejszamy ilosc aktywnych okien
	   ilosc_okien--;
     // a jezeli okno aktywne nie jest pierwszym
     if (aktywne_okno>1)
		    {
        // to oknem aktywnym staje sie okno poprzednie
				aktywne_okno--;
				}
		 // musimy jeszcze wsadzic na ekran poprawny prompt
     podmien_prompt(aktywne_okno, okna[aktywne_okno].query, 1);
		 // oraz zawartosc uaktywnianego okna
		 przywroc_ekran(aktywne_okno);
		 }
   // i wracamy
   return(1);
 }

/* Przechodzimy do nastepnego okna */
int c_window_next()
 {
   // zmieniamy aktywne okno na nastepne
   c_window_sw(aktywne_okno+1);
   // i oddajemy sterowanie
   return(1);
 }

/* Przechodzimy do poprzedniego okna */
int c_window_prev()
 {
   // zmieniamy aktywne okno na poprzednie
   c_window_sw(aktywne_okno-1);
   // i oddajemy sterowanie
   return(1);
 }

/* Uruchamiamy tryb rozmowy */
int c_query(char *uzytkownik)
 {
   // jezeli podano nazwe uzytkownika
   if (uzytkownik)
	   {
		 // to zapamietujemy ja do zmiennej query aktywnego okna
		 sprintf(okna[aktywne_okno].query,uzytkownik);
		 // a takze uaktualniamy prompt
		 podmien_prompt(aktywne_okno, uzytkownik, 1);
		 }
   // i wracamy
   return(1);
 }

/* Wyswietlamy pomoc */
int c_help(char *argument)
  {
   register int i;
   int printed = 0;
   // naglowek
   my_printf("\nKomenda\t\tCo robi\n\n");
	 // w petli sprawdzamy czy mamy w tablicy pomoc dla okreslonego wywolania
   for(i = 0; komendy[i].nazwa; i++)
     {
		   // jezeli nie podano parametru dla komendy /help, albo znalezlismy
			 // pomoc dla podanego parametru
       if(!*argument || (strcmp(argument, komendy[i].nazwa) == 0))
         {
			     // to wypisujemy aktualnie przetwarzana linijke help'u
           my_printf(" %s\t\t%s\n",
					      komendy[i].nazwa,komendy[i].pomoc);
					 // i zwiekszamy flage powodzenia operacji
           printed++;
         }
     }
   // jezeli flaga nadal wynosi 0 to wypisyjemu komunikat o braky pomocy dla
	 // wywolanego parametru.
   if(!printed) my_printf("Niestety, ale brak pomocy dla: %s", argument);
	 my_printf("\n");
	 // i wracamy
   return(0);
 }

int c_window_sw(int numer)
  {
	  // Jezeli mamy aktywne okno na ktore nastepuje proba zmiany
	  // oraz nie jest ono aktualnie aktywne
	  if ((numer>0)&&(numer<=ilosc_okien)&&(numer!=aktywne_okno))
	    {
		  // to zmieniamy aktywne okno
		  aktywne_okno=numer;
			// przywracamy to co bylo na ekranie
			przywroc_ekran(aktywne_okno);
		  // i uaktualniamy prompt dla tego okna
      podmien_prompt(aktywne_okno, okna[aktywne_okno].query, 1);
	    }
    // i oddajemy sterowanie
    return(1);
  }

/* Zmiana okna nastepuje przez wywolanie funckcji c_window_sw() */
int c_window_01() { c_window_sw( 1); return(0); }
int c_window_02() { c_window_sw( 2); return(0); }
int c_window_03() { c_window_sw( 3); return(0); }
int c_window_04() { c_window_sw( 4); return(0); }
int c_window_05() { c_window_sw( 5); return(0); }
int c_window_06() { c_window_sw( 6); return(0); }
int c_window_07() { c_window_sw( 7); return(0); }
int c_window_08() { c_window_sw( 8); return(0); }
int c_window_09() { c_window_sw( 9); return(0); }

/* Konczymy program */
int c_quit(char *arg)
  {
   printf("\033[1;33mWyglada na to, ze sie juz zegnamy.. " \
                                        "\033[1;34mpapa.\033[0;0m\n");
	 // ustawiamy flage zakonczenia programu
   done = 1;
	 // i zwracamy sterowanie
   return(0);
  }

/* koniec kodu programu */
