/* $Id$ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <limits.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <readline/readline.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include "config.h"
#include "libgg.h"
#include "stuff.h"
#include "dynstuff.h"
#include "themes.h"
#include "commands.h"
#include "vars.h"

struct gg_session *sess = NULL;
struct list *userlist = NULL;
struct list *ignored = NULL;
struct list *children = NULL;
struct list *aliases = NULL;
struct list *watches = NULL;
struct list *transfers = NULL;
int in_readline = 0;
int no_prompt = 0;
int away = 0;
int in_autoexec = 0;
int auto_reconnect = 10;
int reconnect_timer = 0;
int auto_away = 600;
int log = 0;
int log_ignored = 0;
char *log_path = NULL;
int display_color = 1;
int enable_beep = 1, enable_beep_msg = 1, enable_beep_chat = 1, enable_beep_notify = 1;
char *sound_msg_file = NULL;
char *sound_chat_file = NULL;
char *sound_sysmsg_file = NULL;
char *sound_app = NULL;
int config_uin = 0;
int last_sysmsg = 0;
char *config_password = NULL;
int sms_away = 0;
char *sms_number = NULL;
char *sms_send_app = NULL;
int sms_max_length = 100;
int search_type = 0;
int config_changed = 0;
int display_ack = 1;
int completion_notify = 1;
char *bold_font = NULL;		/* dla kompatybilno¶ci z gnu gadu */
int private_mode = 0;
int connecting = 0;
int display_notify = 1;
char *default_theme = NULL;
int default_status = GG_STATUS_AVAIL;
char *reg_password = NULL;
int use_dcc = 0;
char *dcc_ip = 0;
char *query_nick = NULL;
uin_t query_uin = 0;

/*
 * my_puts()
 *
 * wy¶wietla dany tekst na ekranie, uwa¿aj±c na trwaj±ce w danych chwili
 * readline().
 */
void my_puts(char *format, ...)
{
        int old_end = rl_end, i;
	char *old_prompt = rl_prompt;
        va_list ap;

        if (in_readline) {
                rl_end = 0;
                rl_prompt = "";
                rl_redisplay();
                printf("\r");
                for (i = 0; i < strlen(old_prompt); i++)
                        printf(" ");
                printf("\r");
        }

        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);

        if (in_readline) {
                rl_end = old_end;
                rl_prompt = old_prompt;
		rl_forced_update_display();
        }
}

/*
 * current_prompt() // funkcja wewnêtrzna
 *
 * zwraca wska¼nik aktualnego prompta. statyczny bufor, nie zwalniaæ.
 */
static char *current_prompt()
{
	static char buf[80];	/* g³upio strdup()owaæ wszystko */
	char *prompt;
	
	if (query_nick) {
		if ((prompt = format_string(find_format("readline_prompt_query"), query_nick, NULL))) {
			strncpy(buf, prompt, sizeof(buf)-1);
			prompt = buf;
		}
	} else {
		if (!readline_prompt)
			readline_prompt = find_format("readline_prompt");
		if (!readline_prompt_away)
			readline_prompt_away = find_format("readline_prompt_away");

		prompt = (!away) ? readline_prompt : readline_prompt_away;
	}

	if (no_prompt || !prompt)
		prompt = "";

	return prompt;
}

/*
 * my_readline()
 *
 * malutki wrapper na readline(), który przygotowuje odpowiedniego prompta
 * w zale¿no¶ci od tego, czy jeste¶my zajêci czy nie i informuje resztê
 * programu, ¿e jeste¶my w trakcie czytania linii i je¶li chc± wy¶wietlaæ,
 * powinny najpierw sprz±tn±æ.
 */
char *my_readline()
{
        char *res, *prompt = current_prompt();

        in_readline = 1;
#ifdef HAS_RL_SET_PROMPT
	rl_set_prompt(prompt);
#endif
        res = readline(prompt);
        in_readline = 0;

        return res;
}

/*
 * reset_prompt()
 *
 * dostosowuje prompt aktualnego my_readline() do awaya.
 */
void reset_prompt()
{
#ifdef HAS_RL_SET_PROMPT
	rl_set_prompt(current_prompt());
	rl_redisplay();
#endif
}

/*
 * prepare_path()
 *
 * zwraca pe³n± ¶cie¿kê do podanego pliku katalogu ~/.gg/
 *
 *  - filename - nazwa pliku.
 */
char *prepare_path(char *filename)
{
	static char path[PATH_MAX];
	char *home = getenv("HOME");
	struct passwd *pw;
	
	if (!home) {
		if (!(pw = getpwuid(getuid())))
			return NULL;
		home = pw->pw_dir;
	}
	
	if (config_user != "") {
	  snprintf(path, sizeof(path), "%s/.gg/%s/%s", home, config_user, filename);
	} else {
	  snprintf(path, sizeof(path), "%s/.gg/%s", home, filename);
	}
	
	return path;
}

/*
 * put_log()
 *
 * wrzuca do logów informacjê od/do danego numerka. podaje siê go z tego
 * wzglêdu, ¿e gdy `log = 2', informacje lec± do $log_path/$uin.
 *
 * - uin,
 * - format...
 */
void put_log(uin_t uin, char *format, ...)
{
 	char *home = getenv("HOME"), *lp = log_path;
	char path[PATH_MAX];
	struct passwd *pw;
	va_list ap;
	FILE *f;

	if (!log)
		return;
	
	if (!lp) {
		if (log == 2)
			lp = ".";
		else
			lp = "gg.log";
	}

	if (*lp == '~') {
		if (!home) {
			if (!(pw = getpwuid(getuid())))
				return;
			home = pw->pw_dir;
		}
		snprintf(path, sizeof(path), "%s%s", home, lp + 1);
	} else
		strncpy(path, lp, sizeof(path));

	if (log == 2) {
		mkdir(path, 0700);
		snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%lu", uin);
	}

	if (!(f = fopen(path, "a")))
		return;

	fchmod(fileno(f), 0600);

	va_start(ap, format);
	vfprintf(f, format, ap);
	va_end(ap);

	fclose(f);
}

/*
 * full_timestamp()
 *
 * zwraca statyczny bufor z pe³n± reprezentacj± czasu. pewnie bêdzie ona
 * zgodna z aktualnym ustawieniem locali. przydaje siê do logów.
 */
char *full_timestamp()
{
	time_t t = time(NULL);
	char *foo = ctime(&t);

	foo[strlen(foo) - 1] = 0;

	return foo;
}

/*
 * read_config()
 *
 * czyta z pliku ~/.gg/config lub podanego konfiguracjê i listê ignorowanych
 * u¿yszkodników.
 *
 *  - filename.
 */
int read_config(char *filename)
{
	char *buf, *foo;
	FILE *f;

	if (!filename) {
		if (!(filename = prepare_path("config")))
			return -1;
	}
	
	if (!(f = fopen(filename, "r")))
		return -1;

	while ((buf = read_file(f))) {
		if (buf[0] == '#') {
			free(buf);
			continue;
		}

		if (!(foo = strchr(buf, ' '))) {
			free(buf);
			continue;
		}

		*foo++ = 0;

		if (!strcasecmp(buf, "ignore")) {
			if (atoi(foo))
				add_ignored(atoi(foo));
		} else if (!strcasecmp(buf, "alias")) {
			add_alias(foo, 1);
		} else
			set_variable(buf, foo);

		free(buf);
	}
	
	fclose(f);
	
	return 0;
}

/*
 * read_sysmsg()
 *
 *  - filename.
 */
int read_sysmsg(char *filename)
{
	char *buf, *foo;
	FILE *f;

	if (!filename) {
		if (!(filename = prepare_path("sysmsg")))
			return -1;
	}
	
	if (!(f = fopen(filename, "r")))
		return -1;

	while ((buf = read_file(f))) {
		if (buf[0] == '#') {
			free(buf);
			continue;
		}

		if (!(foo = strchr(buf, ' '))) {
			free(buf);
			continue;
		}

		*foo++ = 0;
		
		if (!strcasecmp(buf, "last_sysmsg")) {
			if (atoi(foo))
				last_sysmsg = atoi(foo);
		}
		free(buf);
	}
	
	fclose(f);
	
	return 0;
}


/*
 * write_config()
 *
 * zapisuje aktualn± konfiguracjê -- zmienne i listê ignorowanych do pliku
 * ~/.gg/config lub podanego.
 *
 *  - filename.
 */
int write_config(char *filename)
{
	struct variable *v = variables;
	struct list *l;
	char *tmp;
	FILE *f;

	if (!(tmp = prepare_path("")))
		return -1;
	mkdir(tmp, 0700);

	if (!filename) {
		if (!(filename = prepare_path("config")))
			return -1;
	}
	
	if (!(f = fopen(filename, "w")))
		return -1;
	
	fchmod(fileno(f), 0600);

	while (v->name) {
		if (v->type == VAR_STR) {
			if (*(char**)(v->ptr))
				fprintf(f, "%s %s\n", v->name, *(char**)(v->ptr));
		} else
			fprintf(f, "%s %d\n", v->name, *(int*)(v->ptr));
		v++;
	}	

	for (l = ignored; l; l = l->next) {
		struct ignored *i = l->data;

		fprintf(f, "ignore %lu\n", i->uin);
	}

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;

		fprintf(f, "alias %s %s\n", a->alias, a->cmd);
	}

	fclose(f);
	
	return 0;
}

/*
 * write_sysmsg()
 *
 *  - filename.
 */
int write_sysmsg(char *filename)
{
	char *tmp;
	FILE *f;

	if (!(tmp = prepare_path("")))
		return -1;
	mkdir(tmp, 0700);

	if (!filename) {
		if (!(filename = prepare_path("sysmsg")))
			return -1;
	}
	
	if (!(f = fopen(filename, "w")))
		return -1;
	
	fchmod(fileno(f), 0600);
	
	fprintf(f, "last_sysmsg %i\n", last_sysmsg);
	
	fclose(f);
	
	return 0;
}

/*
 * get_token()
 *
 * zwraca kolejny token oddzielany podanym znakiem. niszczy wej¶ciowy
 * ci±g znaków. bo po co on komu?
 *
 *  - ptr - gdzie ma zapisywaæ aktualn± pozycjê w ci±gu,
 *  - sep - znak oddzielaj±cy tokeny.
 */
char *get_token(char **ptr, char sep)
{
	char *foo, *res;

	if (!ptr || !sep || !*ptr)
		return NULL;

	res = *ptr;

	if (!(foo = strchr(*ptr, sep)))
		*ptr += strlen(*ptr);
	else {
		*ptr = foo + 1;
		*foo = 0;
	}

	return res;
}

/*
 * userlist_compare()
 *
 * funkcja pomocna przy list_add_sorted().
 *
 *  - data1, data2 - dwa wpisy userlisty do porównania.
 *
 * zwraca wynik strcasecmp() na nazwach userów.
 */
int userlist_compare(void *data1, void *data2)
{
	struct userlist *a = data1, *b = data2;
	
	if (!a || !a->comment || !b || !b->comment)
		return 0;

	return strcasecmp(a->comment, b->comment);
}

/*
 * read_userlist()
 *
 * wczytuje listê kontaktów z pliku ~/.gg/userlist. mo¿e ona byæ w postaci
 * linii ,,<numerek> <opis>'' lub w postaci eksportu tekstowego listy
 * kontaktów windzianego klienta.
 *
 *  - filename.
 */
int read_userlist(char *filename)
{
	FILE *f;
	char *buf;

	if (!filename) {
		if (!(filename = prepare_path("userlist")))
			return -1;
	}
	
	if (!(f = fopen(filename, "r")))
		return -1;

	while ((buf = read_file(f))) {
		struct userlist u;
		char *comment;
		
		if (buf[0] == '#') {
			free(buf);
			continue;
		}

		if (!strchr(buf, ';')) {
			if (!(comment = strchr(buf, ' '))) {
				free(buf);
				continue;
			}

			u.uin = strtol(buf, NULL, 0);
		
			if (!u.uin) {
				free(buf);
				continue;
			}

			u.first_name = NULL;
			u.last_name = NULL;
			u.nickname = NULL;
			u.comment = strdup(++comment);
			u.mobile = NULL;
			u.group = NULL;

		} else {
			char *first_name, *last_name, *nickname, *comment, *mobile, *group, *uin, *foo = buf;

			first_name = get_token(&foo, ';');
			last_name = get_token(&foo, ';');
			nickname = get_token(&foo, ';');
			comment = get_token(&foo, ';');
			mobile = get_token(&foo, ';');
			group = get_token(&foo, ';');
			uin = get_token(&foo, ';');

			if (!uin || !(u.uin = strtol(uin, NULL, 0))) {
				free(buf);
				continue;
			}

			u.first_name = strdup(first_name);
			u.last_name = strdup(last_name);
			u.nickname = strdup(nickname);
			u.comment = strdup(comment);
			u.mobile = strdup(mobile);
			u.group = strdup(group);
		}

		free(buf);

		u.status = GG_STATUS_NOT_AVAIL;

		list_add_sorted(&userlist, &u, sizeof(u), userlist_compare);
	}
	
	fclose(f);

	return 0;
}

/*
 * write_userlist()
 *
 * zapisuje listê kontaktów w pliku ~/.gg/userlist
 *
 *  - filename.
 */
int write_userlist(char *filename)
{
	struct list *l;
	char *tmp;
	FILE *f;

	if (!(tmp = prepare_path("")))
		return -1;
	mkdir(tmp, 0700);

	if (!filename) {
		if (!(filename = prepare_path("userlist")))
			return -1;
	}
	
	if (!(f = fopen(filename, "w")))
		return -2;

	fchmod(fileno(f), 0600);
	
	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		fprintf(f, "%s;%s;%s;%s;%s;%s;%lu\r\n", (u->first_name) ?
			u->first_name : u->comment, (u->last_name) ?
			u->last_name : "", (u->nickname) ? u->nickname :
			u->comment, u->comment, (u->mobile) ? u->mobile :
			"", (u->group) ? u->group : "", u->uin);
	}	

	fclose(f);
	
	return 0;
}

/*
 *
 * clear_userlist()
 *
 * czysci tablice userlist
 *
 */

void clear_userlist(void) {
        struct list *l;

        for (l = userlist; l; l = l->next) {
                struct userlist *u = l->data;
                u->status = GG_STATUS_NOT_AVAIL;
        };
};

/*
 * add_user()
 *
 * dodaje u¿ytkownika do listy.
 *
 *  - uin,
 *  - comment.
 */
int add_user(uin_t uin, char *comment)
{
	struct userlist u;

	u.uin = uin;
	u.status = GG_STATUS_NOT_AVAIL;
	u.first_name = NULL;
	u.last_name = NULL;
	u.nickname = NULL;
	u.mobile = NULL;
	u.group = NULL;
	u.comment = strdup(comment);

	list_add_sorted(&userlist, &u, sizeof(u), userlist_compare);
	
	return 0;
}

/*
 * del_user()
 *
 * usuwa danego u¿ytkownika z listy kontaktów.
 *
 *  - uin.
 */
int del_user(uin_t uin)
{
	struct list *l;
	
	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (u->uin == uin) {
			free(u->first_name);
			free(u->last_name);
			free(u->nickname);
			free(u->comment);
			free(u->mobile);
			free(u->group);

			list_remove(&userlist, u, 1);

			return 0;
		}
	}

	return -1;
}

/*
 * replace_user()
 *
 * usuwa i dodaje na nowo u¿ytkownika, ¿eby zosta³ umieszczony na odpowiednim
 * (pod wzglêdem kolejno¶ci alfabetycznej) miejscu. g³upie to trochê, ale
 * przy listach jednokierunkowych nie za bardzo chce mi siê mieszaæ z
 * przesuwaniem elementów listy.
 * 
 *  - u.
 *
 * zwraca zero je¶li jest ok, -1 je¶li b³±d.
 */
int replace_user(struct userlist *u)
{
	if (list_remove(&userlist, u, 0))
		return -1;
	if (list_add_sorted(&userlist, u, 0, userlist_compare))
		return -1;

	return 0;
}

/*
 * find_user()
 *
 * znajduje odpowiedni± strukturê `userlist' odpowiadaj±c± danemu numerkowi
 * lub jego opisowi.
 *
 *  - uin,
 *  - comment.
 */
struct userlist *find_user(uin_t uin, char *comment)
{
	struct list *l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

                if (uin && u->uin == uin)
			return u;
                if (comment && !strcasecmp(u->comment, comment))
                        return u;
        }

        return NULL;
}

/*
 * get_uin()
 *
 * je¶li podany tekst jest liczb±, zwraca jej warto¶æ. je¶li jest nazw±
 * u¿ytkownika w naszej li¶cie kontaktów, zwraca jego numerek. inaczej
 * zwraca zero.
 *
 *  - text.
 */
uin_t get_uin(char *text)
{
	uin_t uin = atoi(text);
	struct userlist *u;

	if (!uin) {
		if (!(u = find_user(0, text)))
			return 0;
		uin = u->uin;
	}

	return uin;
}

/*
 * format_user()
 *
 * zwraca ³adny (ew. kolorowy) tekst opisuj±cy dany numerek. je¶li jest
 * w naszej li¶cie kontaktów, formatuje u¿ywaj±c `known_user', w przeciwnym
 * wypadku u¿ywa `unknown_user'. wynik jest w statycznym buforze.
 *
 *  - uin - numerek danej osoby.
 */
char *format_user(uin_t uin)
{
	struct userlist *u = find_user(uin, NULL);
	static char buf[100], *tmp;
	
	snprintf(buf, sizeof(buf), "%lu", uin);
	
	if (!u)
		tmp = format_string(find_format("unknown_user"), buf);
	else
		tmp = format_string(find_format("known_user"), u->comment, buf);
	
	strncpy(buf, tmp, sizeof(buf) - 1);
	
	free(tmp);

	return buf;
}

/*
 * del_ignored()
 *
 * usuwa z listy ignorowanych numerków.
 *
 *  - uin.
 */
int del_ignored(uin_t uin)
{
	struct list *l;

	for (l = ignored; l; l = l->next) {
		struct ignored *i = l->data;

		if (i->uin == uin) {
			list_remove(&ignored, i, 1);
			return 0;
		}
	}

	return -1;
}

/*
 * add_ignored()
 *
 * dopisuje do listy ignorowanych numerków.
 *
 *  - uin.
 */
int add_ignored(uin_t uin)
{
	struct list *l;
	struct ignored i;

	for (l = ignored; l; l = l->next) {
		struct ignored *j = l->data;

		if (j->uin == uin)
			return -1;
	}

	i.uin = uin;
	list_add(&ignored, &i, sizeof(i));
	
	return 0;
}

/*
 * is_ignored()
 *
 * czy dany numerek znajduje siê na li¶cie ignorowanych.
 *
 *  - uin.
 */
int is_ignored(uin_t uin)
{
	struct list *l;

	for (l = ignored; l; l = l->next) {
		struct ignored *i = l->data;

		if (i->uin == uin)
			return 1;
	}

	return 0;
}

/*
 * cp_to_iso()
 *
 * zamienia krzaczki pisane w cp1250 na iso-8859-2, przy okazji maskuj±c
 * znaki, których nie da siê wy¶wietliæ, za wyj±tkiem \r i \n.
 *
 *  - buf.
 */
void cp_to_iso(unsigned char *buf)
{
	while (*buf) {
		if (*buf == (unsigned char)'¥') *buf = '¡';
		if (*buf == (unsigned char)'¹') *buf = '±';
		if (*buf == 140) *buf = '¦';
		if (*buf == 156) *buf = '¶';
		if (*buf == 143) *buf = '¬';
		if (*buf == 159) *buf = '¼';

                if (*buf != 13 && *buf != 10 && (*buf < 32 || (*buf > 127 && *buf < 160)))
                        *buf = '?';

		buf++;
	}
}

/*
 * iso_to_cp()
 *
 * zamienia sensowny format kodowania polskich znaczków na bezsensowny.
 *
 *  - buf.
 */
void iso_to_cp(unsigned char *buf)
{
	while (*buf) {
		if (*buf == (unsigned char)'¡') *buf = '¥';
		if (*buf == (unsigned char)'±') *buf = '¹';
		if (*buf == (unsigned char)'¦') *buf = 'Œ';
		if (*buf == (unsigned char)'¶') *buf = 'œ';
		if (*buf == (unsigned char)'¬') *buf = '';
		if (*buf == (unsigned char)'¼') *buf = 'Ÿ';
		buf++;
	}
}

/*
 * unidle()
 *
 * uaktualnia licznik czasu ostatniej akcji, ¿eby przypadkiem nie zrobi³o
 * autoawaya, kiedy piszemy.
 */
void unidle()
{
	time(&last_action);
}

/*
 * timestamp()
 *
 * zwraca statyczny buforek z ³adnie sformatowanym czasem.
 *
 *  - format.
 */
char *timestamp(char *format)
{
	static char buf[100];
	time_t t;
	struct tm *tm;

	time(&t);
	tm = localtime(&t);
	strftime(buf, sizeof(buf), format, tm);

	return buf;
}

/*
 * parse_autoexec()
 *
 * wykonuje po kolei wszystkie komendy z pliku ~/.gg/autoexec.
 *
 *  - filename.
 */
void parse_autoexec(char *filename)
{
	char *buf;
	FILE *f;

	if (!filename) {
		if (!(filename = prepare_path("autoexec")))
			return;
	}
	
	if (!(f = fopen(filename, "r")))
		return;
	
	in_autoexec = 1;
	
	while ((buf = read_file(f))) {
		if (buf[0] == '#') {
			free(buf);
			continue;
		}

		if (execute_line(buf)) {
			fclose(f);
			free(buf);
			exit(0);
		}

		free(buf);
	}

	in_autoexec = 0;
	
	fclose(f);
}

/*
 * send_userlist()
 *
 * wysy³a do serwera userlistê, wywo³uj±c gg_notify()
 */
void send_userlist()
{
        struct list *l;
        uin_t *uins;
        int i, count;

	count = list_count(userlist);

        uins = (void*) malloc(count * sizeof(uin_t));

	for (i = 0, l = userlist; l; i++, l = l->next) {
		struct userlist *u = l->data;

                uins[i] = u->uin;
	}

        gg_notify(sess, uins, count);

        free(uins);
}

/*
 * do_reconnect()
 *
 * je¶li jest w³±czony autoreconnect, wywo³uje timer, który za podan±
 * ilo¶æ czasu spróbuje siê po³±czyæ jeszcze raz.
 */
void do_reconnect()
{
	if (auto_reconnect && connecting)
		reconnect_timer = time(NULL);
}

/*
 * send_sms()
 *
 * wysy³a sms o podanej tre¶ci do podanej osoby.
 */
int send_sms(char *recipient, char *message, int show_result)
{
	int pid;
	char buf[50];

	if (!sms_send_app) {
		errno = EINVAL;
		return -1;
	}

	if (!recipient || !message) {
		errno = EINVAL;
		return -1;
	}

	if ((pid = fork()) == -1)
		return -1;

	if (!pid) {
		int i;

		for (i = 0; i < 255; i++)
			close(i);
			
		execlp(sms_send_app, sms_send_app, recipient, message, NULL);
		exit(1);
	}

	if (show_result)
		snprintf(buf, sizeof(buf), "\001%s", recipient);
	else
		strcpy(buf, "\002");

	add_process(pid, buf);

	return 0;
}

/*
 * play_sound()
 *
 * odtwarza dzwiêk o podanej nazwie.
 */
int play_sound(char *sound_path)
{
	int pid;

	if (!sound_app || !sound_path) {
		errno = EINVAL;
		return -1;
	}

	if ((pid = fork()) == -1)
		return -1;

	if (!pid) {
		int i;

		for (i = 0; i < 255; i++)
			close(i);
			
		execlp(sound_app, sound_app, sound_path, NULL);
		exit(1);
	}

	add_process(pid, "\002");

	return 0;
}

/*
 * read_file()
 *
 * czyta linijkê tekstu z pliku alokuj±c przy tym odpowiedni buforek.
 */
char *read_file(FILE *f)
{
	char buf[1024], *new, *res = NULL;

	while (fgets(buf, sizeof(buf) - 1, f)) {
		int new_size = ((res) ? strlen(res) : 0) + strlen(buf) + 1;

		if (!(new = realloc(res, new_size))) {
			/* je¶li braknie pamiêci, pomijamy resztê linii */
			if (strchr(buf, '\n'))
				break;
			else
				continue;
		}
		if (!res)
			*new = 0;
		res = new;
		strcpy(res + strlen(res), buf);
		
		if (strchr(buf, '\n'))
			break;
	}

	if (res && strlen(res) > 0 && res[strlen(res) - 1] == '\n')
		res[strlen(res) - 1] = 0;
	if (res && strlen(res) > 0 && res[strlen(res) - 1] == '\r')
		res[strlen(res) - 1] = 0;

	return res;
}

/*
 * add_ignored()
 *
 * dopisuje do listy uruchomionych dzieci procesów.
 *
 *  - pid.
 */
int add_process(int pid, char *name)
{
	struct process p;

	p.pid = pid;
	p.name = strdup(name);
	list_add(&children, &p, sizeof(p));
	
	return 0;
}

/*
 * del_process()
 *
 * usuwa proces z listy dzieciaków.
 *
 *  - pid.
 */
int del_process(int pid)
{
	struct list *l;

	for (l = children; l; l = l->next) {
		struct process *p = l->data;

		if (p->pid == pid) {
			list_remove(&children, p, 1);
			return 0;
		}
	}

	return -1;
}

/*
 * on_off()
 *
 * zwraca 1 je¶li tekst znaczy w³±czyæ, 0 je¶li wy³±czyæ, -1 je¶li co innego.
 *
 *  - value.
 */
int on_off(char *value)
{
	if (!value)
		return -1;

	if (!strcasecmp(value, "on") || !strcasecmp(value, "true") || !strcasecmp(value, "yes") || !strcasecmp(value, "tak") || !strcmp(value, "1"))
		return 1;

	if (!strcasecmp(value, "off") || !strcasecmp(value, "false") || !strcasecmp(value, "no") || !strcasecmp(value, "nie") || !strcmp(value, "0"))
		return 0;

	return -1;
}

/*
 * add_alias()
 *
 * dopisuje alias do listy aliasów.
 *
 * - string - linia w formacie 'alias cmd'
 * - quiet - czy wypluwaæ mesgi na stdout
 */
int add_alias(char *string, int quiet)
{
	char *cmd;
	struct list *l;
	struct alias a;

	if (!string || !(cmd = strchr(string, ' '))) {
		if (!quiet)
			my_printf("not_enough_params");
		return -1;
	}

	*cmd++ = 0;

	for (l = aliases; l; l = l->next) {
		struct alias *j = l->data;

		if (!strcmp(string, j->alias)) {
			if (!quiet)
				my_printf("aliases_exist", string);
			return -1;
		}
	}

	a.alias = strdup(string);
	a.cmd = strdup(cmd);
	list_add(&aliases, &a, sizeof(a));

	if (!quiet)
		my_printf("aliases_add", a.alias, a.cmd);

	return 0;
}

/*
 * del_alias()
 *
 * usuwa alias z listy aliasów.
 *
 * - name - alias.
 */
int del_alias(char *name)
{
	struct list *l;

	if (!name) {
		my_printf("not_enough_params");
		return -1;
	}

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;

		if (!strcmp(a->alias, name)) {
			my_printf("aliases_del", name);
			list_remove(&aliases, a, 1);
			return 0;
		}
	}

	my_printf("aliases_noexist", name);

	return -1;
}

/*
 * is_alias()
 *
 * sprawdza czy komenda w foo jest aliasem, je¶li tak - zwraca cmd,
 * w przeciwnym razie NULL.
 *
 * - foo
 */
char *is_alias(char *foo)
{
	struct list *l;
	char *param = NULL, *buf, *line = strdup(foo);

	if ((param = strchr(line, ' ')))
		*param++ = 0;

	for (l = aliases; l; l = l->next) {
		struct alias *j = l->data;

		if (!strcmp(line, j->alias)) {
			buf = malloc(strlen(j->cmd) + ((param) ? strlen(param) : 0) + 4);
			strcpy(buf, j->cmd);
			if (param) {
				strcat(buf, " ");
				strcat(buf, param);
			}
			free(line);
			return buf;
		}
	}

	free(line);

	return NULL;
}

static char base64_set[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * encode_base64()
 *
 * zapisuje ci±g znaków w base64. alokuje pamiêæ. 
 */
char *encode_base64(char *buf)
{
	return strdup(buf);	/* fix me, baby. */
}

/*
 * decode_base64()
 *
 * wczytuje ci±g znaków base64, zwraca zaalokowany buforek.
 */
char *decode_base64(char *buf)
{
	char *res, *save, *end, *foo, val;
	int index = 0;

	if (!(save = res = strdup(buf)))  /* ¿re wiêcej pamiêci, whatever. */
		return NULL;

	end = buf + strlen(buf);

	while (*buf && buf < end) {
		if (*buf == '\r' || *buf == '\n') {
			buf++;
			continue;
		}
		if (!(foo = strchr(base64_set, *buf)))
			foo = base64_set;
		val = (int)foo - (int)base64_set;
		*buf = 0;
		buf++;
		switch (index) {
			case 0:
				*res |= val << 2;
				break;
			case 1:
				*res++ |= val >> 4;
				*res |= val << 4;
				break;
			case 2:
				*res++ |= val >> 2;
				*res |= val << 6;
				break;
			case 3:
				*res++ |= val;
				break;
		}
		/* gcc 3.x */
		index++;
		index = index % 4;
	}
	*res = 0;
	return res;
}

/*
 * changed_debug()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,debug''.
 */
void changed_debug(char *var)
{
	if (display_debug)
		gg_debug_level = 255;
	else
		gg_debug_level = 0;
}

/*
 * changed_dcc()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,dcc''.
 */
void changed_dcc(char *var)
{
	struct gg_dcc *dcc = NULL;
	struct list *l;
	
	for (l = watches; l; l = l->next) {
		struct gg_common *c = l->data;

		if (c->type == GG_SESSION_DCC_SOCKET)
			dcc = l->data;
	}

	if (!use_dcc && dcc) {
		list_remove(&watches, dcc, 0);
		gg_free_dcc(dcc);
	}

	if (use_dcc && !dcc) {
		if (!(dcc = gg_dcc_create_socket(config_uin, 0))) {
			my_printf("dcc_create_error", strerror(errno));
		} else
			list_add(&watches, dcc, 0);
	}
}

/*
 * changed_theme()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,theme''.
 */
void changed_theme(char *var)
{
    init_theme();
    reset_theme_cache();
    if(default_theme) {
	if(!read_theme(default_theme, 1)) {
	    reset_theme_cache();
    	    my_printf("theme_loaded", default_theme);
	} else
	    my_printf("error_loading_theme", strerror(errno));
    }
}

/*
 * prepare_connect()
 *
 * przygotowuje wszystko pod po³±czenie gg_login.
 */
void prepare_connect()
{
	struct list *l;

	for (l = watches; l; l = l->next) {
		struct gg_dcc *d = l->data;
		
		if (d->type == GG_SESSION_DCC_SOCKET)
			gg_dcc_port = d->port;
	}

	if (dcc_ip)
		gg_dcc_ip = dcc_ip;
}

/*
 * transfer_id()
 *
 * zwraca pierwszy wolny identyfikator transferu dcc.
 */
int transfer_id()
{
	struct list *l;
	int id = 1;

	for (l = transfers; l; l = l->next) {
		struct transfer *t = l->data;

		if (t->id >= id)
			id = t->id + 1;
	}

	return id;
}
