/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "config.h"
#include "libgadu.h"
#include "stuff.h"
#include "dynstuff.h"
#include "themes.h"
#include "commands.h"
#include "vars.h"
#include "userlist.h"

struct gg_session *sess = NULL;
struct list *children = NULL;
struct list *aliases = NULL;
struct list *watches = NULL;
struct list *transfers = NULL;
struct list *events = NULL;

int in_readline = 0;
int no_prompt = 0;
int away = 0;
int in_autoexec = 0;
int config_auto_reconnect = 10;
int reconnect_timer = 0;
int config_auto_away = 600;
int config_log = 0;
int config_log_ignored = 0;
char *config_log_path = NULL;
int config_display_color = 1;
int config_beep = 1, config_beep_msg = 1, config_beep_chat = 1, config_beep_notify = 1;
char *config_sound_msg_file = NULL;
char *config_sound_chat_file = NULL;
char *config_sound_sysmsg_file = NULL;
char *config_sound_app = NULL;
int config_uin = 0;
int last_sysmsg = 0;
char *config_password = NULL;
int config_sms_away = 0;
char *config_sms_number = NULL;
char *config_sms_app = NULL;
int config_sms_max_length = 100;
int search_type = 0;
int config_changed = 0;
int config_display_ack = 1;
int config_completion_notify = 1;
int private_mode = 0;
int connecting = 0;
int config_display_notify = 1;
char *config_theme = NULL;
int config_status = GG_STATUS_AVAIL;
char *reg_password = NULL;
int config_dcc = 0;
char *query_nick = NULL;
uin_t query_uin = 0;
int sock = 0;
int length = 0;
struct sockaddr_un addr;

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
 * wzglêdu, ¿e gdy `log = 2', informacje lec± do $config_log_path/$uin.
 *
 * - uin,
 * - format...
 */
void put_log(uin_t uin, char *format, ...)
{
 	char *home = getenv("HOME"), *lp = config_log_path;
	char path[PATH_MAX];
	struct passwd *pw;
	va_list ap;
	FILE *f;

	if (!config_log)
		return;
	
	if (!lp) {
		if (config_log == 2)
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

	if (config_log == 2) {
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
 * config_read()
 *
 * czyta z pliku ~/.gg/config lub podanego konfiguracjê i listê ignorowanych
 * u¿yszkodników.
 *
 *  - filename.
 */
int config_read(char *filename)
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
		if (buf[0] == '#' || buf[0] == ';' || (buf[0] == '/' || buf[1] == '/')) {
			free(buf);
			continue;
		}

		if (!(foo = strchr(buf, ' '))) {
			free(buf);
			continue;
		}

		*foo++ = 0;

		if (!strcasecmp(buf, "set")) {
			char *bar;

			if (!(bar = strchr(foo, ' ')))
				variable_set(foo, NULL, 1);
			else {
				*bar++ = 0;
				variable_set(foo, bar, 1);
			}
		} else if (!strcasecmp(buf, "ignore")) {
			if (atoi(foo))
				ignored_add(atoi(foo));
		} else if (!strcasecmp(buf, "alias")) {
			add_alias(foo, 1);
		} else if (!strcasecmp(buf, "on")) {
                        int flags;
                        uin_t uin;
                        char **pms = split_params(foo, 3);
                        if ((flags = get_flags(pms[0])) && (uin = atoi(pms[1]))
&& !correct_event(pms[2]))
                                add_event(get_flags(pms[0]), atoi(pms[1]), strdup(pms[2]));
                        else
                            my_printf("config_line_incorrect");
                } else
			variable_set(buf, foo, 1);

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
 * config_write()
 *
 * zapisuje aktualn± konfiguracjê -- zmienne i listê ignorowanych do pliku
 * ~/.gg/config lub podanego.
 *
 *  - filename.
 */
int config_write(char *filename)
{
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

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;
		
		if (v->type == VAR_STR) {
			if (*(char**)(v->ptr)) {
				if (!v->display) {
					char *tmp = base64_encode(*(char**)(v->ptr));
					fprintf(f, "%s \001%s\n", v->name, tmp);
					free(tmp);
				} else 	
					fprintf(f, "%s %s\n", v->name, *(char**)(v->ptr));
			}
		} else if (v->type == VAR_FOREIGN)
			fprintf(f, "%s %s\n", v->name, (char*) v->ptr);
		else
			fprintf(f, "%s %d\n", v->name, *(int*)(v->ptr));
	}	

	for (l = ignored; l; l = l->next) {
		struct ignored *i = l->data;

		fprintf(f, "ignore %lu\n", i->uin);
	}

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;

		fprintf(f, "alias %s %s\n", a->alias, a->cmd);
	}

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                fprintf(f, "on %s %lu %s\n", format_events(e->flags), e->uin, e->action);
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

	if (!ptr || !sep || !*ptr || !**ptr)
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
 * do_reconnect()
 *
 * je¶li jest w³±czony autoreconnect, wywo³uje timer, który za podan±
 * ilo¶æ czasu spróbuje siê po³±czyæ jeszcze raz.
 */
void do_reconnect()
{
	if (config_auto_reconnect && connecting)
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

	if (!config_sms_app) {
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
			
		execlp(config_sms_app, config_sms_app, recipient, message, NULL);
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

	if (!config_sound_app || !sound_path) {
		errno = EINVAL;
		return -1;
	}

	if ((pid = fork()) == -1)
		return -1;

	if (!pid) {
		int i;

		for (i = 0; i < 255; i++)
			close(i);
			
		execlp(config_sound_app, config_sound_app, sound_path, NULL);
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
 * add_process()
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

/*
 * format_events()
 *
 * zwraca ³añcuch zdarzeñ w oparciu o flagi.
 *
 * - flags
 */
char *format_events(int flags)
{
        char buff[64] = "";

        if (flags & EVENT_MSG) strcat(buff, *buff ? "|msg" : "msg");
        if (flags & EVENT_CHAT) strcat(buff, *buff ? "|chat" : "chat");
        if (flags & EVENT_AVAIL) strcat(buff, *buff ? "|avail" : "avail");
        if (flags & EVENT_NOT_AVAIL) strcat(buff, *buff ? "|disconnect" : "disconnect");
        if (flags & EVENT_AWAY) strcat(buff, *buff ? "|away" : "away");
        if (flags & EVENT_DCC) strcat(buff, *buff ? "|dcc" : "dcc");
        if (strlen(buff) > 33) strcpy(buff, "*");

        return strdup(buff);
}

/*
 * get_flags()
 *
 * zwraca flagi na podstawie ³añcucha.
 *
 * - events
 */
int get_flags(char *events)
{
        int flags = 0;

        if(!strncasecmp(events, "*", 1)) return EVENT_MSG|EVENT_CHAT|EVENT_AVAIL|EVENT_NOT_AVAIL|EVENT_AWAY|EVENT_DCC;
        if (strstr(events, "msg") || strstr(events, "MSG")) flags |= EVENT_MSG;
        if (strstr(events, "chat") || strstr(events, "CHAT")) flags |= EVENT_CHAT;
        if (strstr(events, "avail") || strstr(events, "AVAIL")) flags |= EVENT_AVAIL;
        if (strstr(events, "disconnect") || strstr(events, "disconnect")) flags
|= EVENT_NOT_AVAIL;
        if (strstr(events, "away") || strstr(events, "AWAY")) flags |= EVENT_AWAY;
        if (strstr(events, "dcc") || strstr(events, "DCC")) flags |= EVENT_DCC;

        return flags;
}

/*
 * add_event()
 *
 * Dodaje zdarzenie do listy zdarzeñ.
 *
 * - flags
 * - uin
 * - action
 */
int add_event(int flags, uin_t uin, char *action)
{
        int f;
        struct list *l;
        struct event e;

        for (l = events; l; l = l->next) {
                struct event *ev = l->data;

                if (ev->uin == uin && (f = ev->flags & flags) != 0) {
                        my_printf("events_exist", format_events(f), (uin ==1) ?
"*"  : format_user(uin));
                        return -1;
                }
        }

        e.uin = uin;
        e.flags = flags;
        e.action = action;

        list_add(&events, &e, sizeof(e));

        my_printf("events_add", format_events(flags), (uin ==1) ? "*"  : format_user(uin), action);

        return 0;
}

/*
 * del_event()
 *
 * usuwa zdarzenie z listy zdarzeñ.
 *
 * - flags
 * - uin
 */
int del_event(int flags, uin_t uin)
{
        struct list *l;

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                if (e->uin == uin && e->flags & flags) {
                        if ((e->flags &=~ flags) == 0) {
                                my_printf("events_del", format_events(flags), (uin ==1) ? "*"  : format_user(uin), e->action);
                                list_remove(&events, e, 1);
                                return 0;
                        }
                        else {
                                my_printf("events_del_flags", format_events(flags));
                                list_remove(&events, e, 0);
                                list_add_sorted(&events, e, 0, NULL);
                                return 0;
                        }
                }
        }

        my_printf("events_del_noexist", format_events(flags), (uin ==1) ? "3"  : format_user(uin));

        return 1;
}

/*
 * check_event()
 *
 * sprawdza i ewentualnie uruchamia akcjê na podane zdarzenie.
 *
 * - event
 * - uin
 */
int check_event(int event, uin_t uin)
{
        char *evt_ptr = NULL, *evs[10];
        struct list *l;
        int y = 0, i;

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                if (e->flags & event) {
                        if (e->uin == uin) {
                                evt_ptr = strdup(e->action);
                                break;
                        }
                        else if (e->uin == 1)
                                evt_ptr = strdup(e->action);
                }
        }

        if (evt_ptr == NULL)
                return 1;

        if (strchr(evt_ptr, ';')) {
                evs[y] = strtok(evt_ptr, ";");
                while ((evs[++y] = strtok(NULL, ";"))) {
                        if (y >= 10) break;
                }
        }
        else
                return run_event(evt_ptr);

        for (i = 0; i < y; i++) {
                if (!evs[i]) continue;
                while (*evs[i] == ' ') evs[i]++;
                run_event(evs[i]);
        }

        return 0;
}

/*
 * run_event()
 *
 * wykonuje dan± akcjê.
 *
 * - action
 */
int run_event(char *act)
{
        uin_t uin;
        char *cmd = NULL, *arg = NULL, *data = NULL, *action = strdup(act);

        if(strstr(action, " ")) {
                cmd = strtok(action, " ");
                arg = strtok(NULL, "");
        }
        else
                cmd = action;

#ifdef IOCTL
        if (!strncasecmp(cmd, "blink_leds", 4))
                return send_event(strdup(arg), ACT_BLINK_LEDS);

        else if(!strncasecmp(cmd, "beeps_spk", 4))
                return send_event(strdup(arg), ACT_BEEPS_SPK);

        else 
#endif	//IOCTL
 	
	if(!strncasecmp(cmd, "play", 4)) {
		play_sound(strdup(arg));
	} 
	else if(!strncasecmp(cmd, "chat", 4) || !strncasecmp(cmd, "msg", 3)) {
                struct userlist *u;
                char sender[50];

                if (!strstr(arg, " "))
                        return 1;

                strtok(arg, " ");
                data = strtok(NULL, "");

                if (!(uin = get_uin(arg)))
                        return 1;

                if ((u = userlist_find(uin, NULL)))
                        snprintf(sender, sizeof(sender), "%s/%lu", u->display, u->uin);
                else
                        snprintf(sender, strlen(sender), "%s", arg);

                put_log(uin, "<<* %s %s (%s)\n%s\n", (!strcasecmp(cmd, "chat"))
? "Rozmowa do" : "Wiadomo¶æ do", sender, full_timestamp(), data);

                iso_to_cp(data);
                gg_send_message(sess, (!strcasecmp(cmd, "msg")) ? GG_CLASS_MSG : GG_CLASS_CHAT, uin, data);

                return 0;
        }

        else
                my_printf("temporary_run_event", action);

        return 0;
}

/*
 * send_event()
 *
 * wysy³a do ioctl_daemon'a polecenie uruchomienia akcji z ioctl.
 *
 * - seq
 * - act
*/
int send_event(char *seq, int act)
{
        char *s;
        struct action_data data;

        if (*seq == '$') {
                seq++;
                s = find_format(seq);
                if (!strcmp(s, "")) {
                        my_printf("events_seq_not_found", seq);
                        return 1;
                }
        } else
                s = seq;

        data.act = act;

        if (events_parse_seq(s, &data))
                return 1;

        sendto(sock, &data, sizeof(data), 0,(struct sockaddr *)&addr, length);

        return 0;
}

/* correct_event()
 *
 * sprawdza czy akcja na zdarzenie jest poprawna.
 *
 * - act
*/
int correct_event(char *act)
{
        int y = 0, i;
        char *cmd = NULL, *arg = NULL, *action = strdup(act), *evs[10];

#ifdef IOCTL
        struct action_data test;
#endif // IOCTL

        if (!strncasecmp(action, "clear",  5))
                return 0;

        evs[y] = strtok(action, ";");
        while ((evs[++y] = strtok(NULL, ";")))
                if (y >= 10) break;

        for (i = 0; i < y; i++) {

                if (!evs[i]) {
                        y--;
                        continue;
                }

                while (*evs[i] == ' ') evs[i]++;

                if (strstr(evs[i], " ")) {
                        cmd = strtok(evs[i], " ");
                        arg = strtok(NULL, "");
                } else
                        cmd = evs[i];

#ifdef IOCTL
                if (!strncasecmp(cmd, "blink_leds", 10) || !strncasecmp(cmd, "beeps_spk", 9)) {
                        if (arg == NULL) {
                                my_printf("events_act_no_params", cmd);
                                return 1;
                        }

                        if (*arg == '$') {
                                arg++;
                                if (!strcmp(find_format(arg), "")) {
                                        my_printf("events_seq_not_found", arg);
                                        return 1;
                                }
                                continue;
                        }

                        if (events_parse_seq(arg, &test)) {
                                my_printf("events_seq_incorrect", arg);
                                return 1;
                        }

                        continue;
                }

                else 
#endif // IOCTL		

		if (!strncasecmp(cmd, "play", 4)) {
			if (arg == NULL) {
				my_printf("events_act_no_params", cmd);
				return 1; 
			}
		} 
		else if (!strncasecmp(cmd, "chat", 4) || !strncasecmp(cmd, "msg", 3)) {
                        if (arg == NULL) {
                                my_printf("events_act_no_params", cmd);
                                return 1;
                        }

                        if (!strstr(arg, " ")) {
                                my_printf("events_act_no_params", cmd);
                                return 1;
                        }

                        strtok(arg, " ");

                        if (!get_uin(arg)) {
                                my_printf("user_not_found", arg);
                                return 1;
                        }

                        continue;
                }

                else {
                        my_printf("events_noexist");
                        return 1;
                }
        }

        if (y == 0) {
                my_printf("events_noexist");
                return 1;
        }

        return 0;
}

/*
 * events_parse_seq()
 *
 * zamieñ string na odpowiedni± strukturê, zwraca >0 w przypadku b³êdu.
 *
 * seq
 * data
 */
int events_parse_seq(char *seq, struct action_data *data)
{
        char tmp_buff[16] = "";
        int i = 0, a, l = 0, default_delay = 10000;

        if (!seq || !isdigit(seq[0]))
                return 1;

        for (a = 0; a <= strlen(seq) && a < MAX_ITEMS; a++) {
                if (i > 15 ) return 2;
                if (isdigit(seq[a]))
                        tmp_buff[i++] = seq[a];
                else if (seq[a] == '/') {
                        data->value[l] = atoi(tmp_buff);
                        bzero(tmp_buff, 16);
                        for (i = 0; isdigit(seq[++a]); i++)
                                tmp_buff[i] = seq[a];
                        data->delay[l] = default_delay = atoi(tmp_buff);
                        bzero(tmp_buff, 16);
                        i = 0;
                        l++;
                }
                else if (seq[a] == ',') {
                        data->value[l] = atoi(tmp_buff);
                        data->delay[l] = default_delay;
                        bzero(tmp_buff, 16);
                        i = 0;
                        l++;
                }
                else if (seq[a] == ' ')
                        continue;
                else if (seq[a] == '\0') {
                        data->value[l] = atoi(tmp_buff);
                        data->delay[l] = default_delay;
                        data->value[++l] = data->delay[l] = -1;
                }
                else return 3;
        }
        return 0;
}

/*
 * init_socket()
 *
 * inicjuje gniazdo oraz strukturê addr dla ioctl_daemon'a.
 *
 * - sock_path
*/
int init_socket(char *sock_path)
{
        sock = socket(AF_UNIX, SOCK_DGRAM, 0);

        if (sock < 0) perror("socket");

        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, sock_path);
        length = sizeof(addr);

        return 0;
}

static char base64_charset[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * base64_encode()
 *
 * zapisuje ci±g znaków w base64. alokuje pamiêæ. 
 */
char *base64_encode(char *buf)
{
	char *out, *res;
	int i = 0, j = 0, k = 0, len = strlen(buf);
	
	if (!(res = out = malloc((len / 3 + 1) * 4 + 2))) {
		gg_debug(GG_DEBUG_MISC, "// base64_encode() not enough memory\n");
		return NULL;
	}
	
	while (j <= len) {
		switch (i % 4) {
			case 0:
				k = (buf[j] & 252) >> 2;
				break;
			case 1:
				k = ((buf[j] & 3) << 4) | ((buf[++j] & 240) >> 4);
				break;
			case 2:
				k = ((buf[j] & 15) << 2) | ((buf[++j] & 192) >> 6);
				break;
			case 3:
				k = buf[j++] & 63;
				break;
		}
		*out++ = base64_charset[k];
		i++;
	}

	if (i % 4)
		for (j = 0; j < 4 - (i % 4); j++, out++)
			*out = '=';
	
	*out = 0;
	
	return res;
}

/*
 * base64_decode()
 *
 * wczytuje ci±g znaków base64, zwraca zaalokowany buforek.
 */
char *base64_decode(char *buf)
{
	char *res, *save, *end, *foo, val;
	int index = 0;
	
	if (!(save = res = calloc(1, (strlen(buf) / 4 + 1) * 3 + 2))) {
		gg_debug(GG_DEBUG_MISC, "// base64_decode() not enough memory\n");
		return NULL;
	}

	end = buf + strlen(buf);

	while (*buf && buf < end) {
		if (*buf == '\r' || *buf == '\n') {
			buf++;
			continue;
		}
		if (!(foo = strchr(base64_charset, *buf)))
			foo = base64_charset;
		val = (int)foo - (int)base64_charset;
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
		index++;
		index %= 4;
	}
	*res = 0;
	
	return save;
}

	
/*
 * changed_debug()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,debug''.
 */
void changed_debug(char *var)
{
	if (config_debug)
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

	if (!config_dcc && dcc) {
		list_remove(&watches, dcc, 0);
		gg_free_dcc(dcc);
	}

	if (config_dcc && !dcc) {
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
    if(config_theme) {
	if(!read_theme(config_theme, 1)) {
	    reset_theme_cache();
    	    my_printf("theme_loaded", config_theme);
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
		
		if (d->type == GG_SESSION_DCC_SOCKET) {
			gg_dcc_port = d->port;
			
		}
	}
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

/*
 * strdup_null()
 *
 * dzia³a tak samo jak strdup(), tyle ¿e przy argumencie równym NULL
 * zwraca NULL zamiast segfaultowaæ.
 *
 *  - ptr - bufor do skopiowania.
 *
 * zwraca zaalokowany bufor lub NULL.
 */
char *strdup_null(char *ptr)
{
	return (ptr) ? strdup(ptr) : NULL;
}

