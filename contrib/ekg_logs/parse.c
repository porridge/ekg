/*
	Pobiera logi ekg z stdin i wypluwa w ³adnej formie na stdout
	Zalety: 
		+ szybki
		+ napisany w C
	Wady:
		- trochê nieprawid³owo wy¶wietla linijki zawieraj±ce tekst `\n' lub `\r'
			(literalnie, je¶li kto¶ wpisa³by backslash i [rn] w tre¶ci wiadomo¶ci)
		- wymaga glib'a
		- trzeba go skompilowaæ
		- autor jest debilem.

	Autor: Robert Goliasz <rgoliasz@poczta.onet.pl>
*/

#define FORMAT_TIME   "%02d%s%02d"
#define FORMAT_SEC    ":%02d"
#define FORMAT_FROM   "<%s< "
#define FORMAT_TO     ">%s> "
#define FORMAT_STATUS "|%s| "

#define COLOR_NORM    "\033[0;37m"
#define COLOR_ERR     "\033[0;37;41;1m"
#define COLOR_NOTICE  "\033[0;37;1m"
#define COLOR_TIME    "\033[0;36m"
#define COLOR_RECTIME "\033[0;34m"
#define COLOR_FROM    "\033[0;32;1m"
#define COLOR_TO      "\033[0;34;1m"
#define COLOR_STATUS  "\033[0;37m"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <time.h>

// zmienne okre¶laj±ce parametry:
int nodate=0,notime=0,shownick=-1,showrecd=0,justify=0,showsec=0;
int showrecsec=0,nobad=0,color=0,nostatus=0,showcolon=0;
char *uin=NULL, *nick=NULL;

long lastdate=0;

void showhelp()
{
	fprintf(stderr,
		"Pobiera logi ekg z stdin i wypluwa w ³adnej formie na stdout\n"
		"U¿ycie:\n\n"
		"t      -- nie wy¶wietlaj w ogóle czasu\n"
		"d      -- nie wy¶wietlaj zmiany dni\n"
		"x      -- zawsze wy¶wietlaj ksywkê\n"
		"v      -- nigdy nie wy¶wietlaj ksywki \n"
		"            (domy¶lnie: wy¶wietlaj je¶li nie podano opcji `u' ani `n')\n"
		"r      -- wy¶wietlaj czas odebrania w wiadomo¶ciach przychodz±cych\n"
		"R      -- j/w i wyrównaj wiadomo¶ci wychodz±ce\n"
		"s      -- wy¶wietlaj sekundy\n"
		"S      -- wy¶wietlaj sekundy przy czasie odebrania\n"
		"C      -- wy¶wietlaj dwukropek miêdzy godzin± a minut±\n"
		"b      -- nie wy¶wietlaj b³êdnych linijek (zaczynaj± siê od !!!)\n"
		"a      -- nie wy¶wietlaj zmian statusu\n"
		"c      -- wy¶wietlaj kolorki\n"
		"h      -- this help screen\n"
		"u UIN  -- pokazuj tylko wiadomo¶ci od/do UIN\n"
		"n NICK -- pokazuj tylko wiadomo¶ci od/do NICKa\n\n"
		"mo¿na poprzedziæ `-', mo¿na ³±czyæ lub nie, `u' lub `n' powinny byæ\n"
		"na koñcu i nie powinny wystêpowaæ razem\n");
			
	exit(1);
}

void fatal(char *s)
{
	perror(s);
	fprintf(stderr,"Aby uzyskaæ pomoc u¿yj opcji -h\n");
	exit(2);
}

void set_params(int argc, char **argv) 
	// moje w³asne, mo¿e nie dzia³aæ w 100% przypadków
{
	int i;
	
	for(i=1;(i<argc) && (!uin) && (!nick);i++)
	{
		int j;
		for(j=0;j<strlen(argv[i]);j++)
		{
			switch(argv[i][j])
			{
				case 't':
					notime=1;
					break;
				case 'd':
					nodate=1;
					break;
				case 'x':
					shownick=1;
					break;
				case 'v':
					shownick=0;
					break;
				case 'r':
					showrecd=1;
					break;
				case 'R':
					showrecd=1;
					justify=1;
					break;
				case 's':
					showsec=1;
					break;
				case 'S':
					showrecsec=1;
					break;
				case 'C':
					showcolon=1;
					break;
				case 'b':
					nobad=1;
					break;
				case 'c':
					color=1;
					break;
				case 'a':
					nostatus=1;
					break;
				case 'h':
					showhelp();
					break;

				case '-':
					break;
				case 'u':
					if(i+1>=argc)
						fatal("Za ma³o/z³e parametry");
					uin=argv[i+1];
					break;
				case 'n':
	 				if(i+1>=argc)
						fatal("Za ma³o/z³e parametry");
					nick=argv[i+1];
					break;
			}
		}
	}
	if(shownick==-1)
	{
		if(uin || nick)
			shownick=0;
		else
			shownick=1;
	}
}

// notime, showrecd, justify, showsec, showrecsec

void print_time(time_t sent, time_t recd)
{
	struct tm *tim;
	long date;

	tim=localtime(&sent);

	if(!nodate)
	{
		date=(tim->tm_year)*1000+tim->tm_yday;
		if(date!=lastdate)
		{
			if(color)
				printf(COLOR_NOTICE);
			printf("-!- Dzieñ %02d-%02d-%04d\n",
					tim->tm_mday,tim->tm_mon+1,tim->tm_year+1900);
			lastdate=date;
		}
	}
	
	if(!notime)
	{
		// czas wys³ania
		if(color)
			printf(COLOR_TIME);
		printf(FORMAT_TIME,
				tim->tm_hour,
				showcolon ? ":" : "",
				tim->tm_min);
		if(showsec)
			printf(FORMAT_SEC,tim->tm_sec);
		printf(" ");

		// czas odbioru
		if(recd && showrecd)
		{
			if(color)
				printf(COLOR_RECTIME);
			tim=localtime(&recd);
			printf("("FORMAT_TIME,
					tim->tm_hour,
					showcolon ? ":" : "",
					tim->tm_min);
			if(showrecsec)
				printf(FORMAT_SEC,tim->tm_sec);
			printf(") ");
		}
		else if(justify)
		{
			printf("       ");
			if(showrecsec)
				printf("   ");
			if(showcolon)
				printf(" ");
		}
	}
}

int check_line(char *u, char *n)
{
	if(uin && strcmp(uin,u))
		return 0;

	if(nick && strcmp(nick,n))
		return 0;

	return 1;
}

// przerabia linijkê ¿eby nie zawiera³a `\', kod jest do bani, ale chyba dzia³a
char *reformat(char *a, int part, char chr)
{
	static char buf[300];
	int i,len;
	
	// usuñ pocz±tkowe "
	if(!part && a[0]=='"')
		strcpy(buf,a+1);
	else
		strcpy(buf,a);
	
	// zamieñ \" na "
	// zamieñ \r\n na <CR> << 
	// zamieñ \\ na \ . 
	len=strlen(buf);
	for(i=len-2;i>=0;i--)
	{
		if(buf[i]=='\\')
		{
			switch(buf[i+1])
			{
				case '"':
					buf[i]='"';
					break;
				case '\'':
					buf[i]='\'';
					break;
				case 'n':
					buf[i]=chr;
					buf[i+1]=' ';
					continue;
					break;
				case 'r':
					buf[i]='\n';
					buf[i+1]=chr;
					continue;
					break;
			}
			memmove(buf+i+1,buf+i+2,strlen(buf+i+2)+1);
		}
	}

	// usuñ koñcowe "
	if(
			buf[strlen(buf)-1]=='\n' && 
			buf[strlen(buf)-2]=='"')
	{
		buf[strlen(buf)-2]='\n';
		buf[strlen(buf)-1]=0;
	}
	
	return buf;
}

void process_line()
{
	char linebuf[255]; // zak³adam ¿e nag³ówki nie przekraczaj± 249 znaków...
	int printed=1,reformatted=0;
	char chr;
	
	if(!fgets(linebuf,250,stdin))
		return;
	
	if(
			!strncmp(linebuf,"msgrecv", 7) ||
			!strncmp(linebuf,"chatrecv",8) )
	{
		char **a;
		char *l;
		
		chr='<';
		a=g_strsplit(linebuf,",",5);
		l=a[5];
		if(a[5][0]=='"')
		{
			reformatted=1;
			l=reformat(a[5],0,chr);
		}
		if(check_line(a[1],a[2]))
		{
			print_time(atol(a[3]), atol(a[4]));
			if(color)
				printf(COLOR_FROM);
			printf(FORMAT_FROM"%s",shownick?(a[2][0]?a[2]:a[1]):"",l);
		}
		else printed=0;

		g_strfreev(a);
	}
	else if(
			!strncmp(linebuf,"msgsend", 7) ||
			!strncmp(linebuf,"chatsend",8) )
	{
		char **a;
		char *l;
		
		chr='>';
		a=g_strsplit(linebuf,",",4);
		l=a[4];
		if(a[4][0]=='"')
		{
			reformatted=1;
			l=reformat(a[4],0,chr);
		}
		if(check_line(a[1],a[2]))		
		{
			print_time(atol(a[3]),0);
			if(color)
				printf(COLOR_TO);
			printf(FORMAT_TO"%s",shownick?(a[2][0]?a[2]:a[1]):"",l);
		}
		else printed=0;
		
		g_strfreev(a);
	}
	else if(!strncmp(linebuf,"status",6))
	{
		char **a;
		char *l;

		chr='|';
		a=g_strsplit(linebuf,",",6);
		l=a[6];
		if(l && l[0]=='"')
		{
			reformatted=1;
			l=reformat(l,0,chr);
		}
		if(check_line(a[1],a[2]) && !nostatus)
		{
			print_time(atol(a[4]),0);
			if(color)
				printf(COLOR_STATUS);
			printf(FORMAT_STATUS"%s",shownick?(a[2][0]?a[2]:a[1]):"",a[5]);
			if(l)
				printf(": %s",l);
		}
		else printed=0;

		g_strfreev(a);
	}
	else
	{
		if(!nobad)
		{
			if(color)
				printf(COLOR_ERR);
			printf("!!! %s",linebuf); // nieznana linijka
		}
		else
			printed=0;
	}
	
	// linijki d³u¿sze ni¿ 250 znaków (je¶li printed)
	while(strlen(linebuf)==249 && linebuf[248]!='\n')  // [249] bêdzie zerem
	{
		fgets(linebuf,250,stdin);
		if(printed)
		{
			printf("%s",reformatted?reformat(linebuf,1,chr):linebuf);
		}
	}
}

int main(int argc, char **argv)
{
	set_params(argc,argv);

	if(color)
		printf(COLOR_NOTICE);
	if(uin)
		printf("-!- Rozmowa z UIN %s\n",uin);
	if(nick)
		printf("-!- Rozmowa z %s\n",nick);
	
	while(!feof(stdin))
		process_line();
	if(color)
		printf(COLOR_NORM);
	return 0;
}
