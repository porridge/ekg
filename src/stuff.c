/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo�ny <speedy@ziew.org>
 *                          Pawe� Maziarz <drg@o2.pl>
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
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <limits.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <readline.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef IOCTL
#include <sys/un.h>
#endif
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
int config_auto_save = 0;
time_t last_save = 0;
int config_log = 0;
int config_log_ignored = 0;
int config_log_status = 0;
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
char *config_dcc_ip = NULL;
char *query_nick = NULL;
uin_t query_uin = 0;
int sock = 0;
int length = 0;
#ifdef IOCTL
struct sockaddr_un addr;
#endif
char *busy_reason = NULL;
char *home_dir = NULL;
int screen_lines = 24;
int screen_columns = 80;
char *config_quit_reason = NULL;
char *config_away_reason = NULL;
char *config_back_reason = NULL;
int config_random_reason = 0;
int config_query_commands = 0;
char *config_proxy = NULL;
char *config_server = NULL;
int my_printf_lines = -1;
int quit_message_send = 0;
int registered_today = 0;
int config_protocol = 0;

/*
 * my_puts()
 *
 * wy�wietla dany tekst na ekranie, uwa�aj�c na trwaj�ce w danych chwili
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
 * current_prompt() // funkcja wewn�trzna
 *
 * zwraca wska�nik aktualnego prompta. statyczny bufor, nie zwalnia�.
 */
static char *current_prompt()
{
	static char buf[80];	/* g�upio strdup()owa� wszystko */
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
		if (!readline_prompt_invisible)
			readline_prompt_invisible = find_format("readline_prompt_invisible");

		switch (away) {
			case 1:
			case 3:
				prompt = readline_prompt_away;
				break;
			case 2:
			case 5:
				prompt = readline_prompt_invisible;
				break;
			default:
				prompt = readline_prompt;
		}
	}

	if (no_prompt || !prompt)
		prompt = "";

	return prompt;
}

/*
 * my_readline()
 *
 * malutki wrapper na readline(), kt�ry przygotowuje odpowiedniego prompta
 * w zale�no�ci od tego, czy jeste�my zaj�ci czy nie i informuje reszt�
 * programu, �e jeste�my w trakcie czytania linii i je�li chc� wy�wietla�,
 * powinny najpierw sprz�tn��.
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
 * zwraca pe�n� �cie�k� do podanego pliku katalogu ~/.gg/
 *
 *  - filename - nazwa pliku.
 */
char *prepare_path(char *filename)
{
	static char path[PATH_MAX];
	
	if (!filename || !*filename) {
		if (config_user && *config_user)
			snprintf(path, sizeof(path), "%s/.gg/%s", home_dir, config_user);
		else
			snprintf(path, sizeof(path), "%s/.gg", home_dir);
	} else {
		if (config_user && *config_user)
			snprintf(path, sizeof(path), "%s/.gg/%s/%s", home_dir, config_user, filename);
		else
			snprintf(path, sizeof(path), "%s/.gg/%s", home_dir, filename);
	}
	
	return path;
}

/*
 * put_log()
 *
 * wrzuca do log�w informacj� od/do danego numerka. podaje si� go z tego
 * wzgl�du, �e gdy `log = 2', informacje lec� do $config_log_path/$uin.
 *
 * - uin,
 * - format...
 */
void put_log(uin_t uin, char *format, ...)
{
 	char *lp = config_log_path;
	char path[PATH_MAX];
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

	if (*lp == '~')
		snprintf(path, sizeof(path), "%s%s", home_dir, lp + 1);
	else
		strncpy(path, lp, sizeof(path));

	if (config_log == 2) {
		if (mkdir(path, 0700) && errno != EEXIST)
			return;
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
 * zwraca statyczny bufor z pe�n� reprezentacj� czasu. pewnie b�dzie ona
 * zgodna z aktualnym ustawieniem locali. przydaje si� do log�w.
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
 * czyta z pliku ~/.gg/config lub podanego konfiguracj� i list� ignorowanych
 * u�yszkodnik�w.
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
		if (buf[0] == '#' || buf[0] == ';' || (buf[0] == '/' && buf[1] == '/')) {
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
                        char **pms = array_make(foo, " \t", 3, 1, 0);
                        if (pms && pms[0] && pms[1] && pms[2] && (flags = get_flags(pms[0])) && (uin = atoi(pms[1])) && !correct_event(pms[2]))
                                add_event(get_flags(pms[0]), atoi(pms[1]), strdup(pms[2]), 1);
			array_free(pms);
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
 * config_write_main()
 *
 * w�a�ciwa funkcja zapisuj�ca konfiguracj� do podanego pliku.
 *
 *  - f - plik, do kt�rego piszemy,
 *  - base64 - czy kodowa� ukryte pola?
 */
void config_write_main(FILE *f, int base64)
{
	struct list *l;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;
		
		if (v->type == VAR_STR) {
			if (*(char**)(v->ptr)) {
				if (!v->display && base64) {
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
}

/*
 * config_write_crash()
 *
 * funkcja zapisuj�ca awaryjnie konfiguracj�. nie powinna alokowa� �adnej
 * pami�ci.
 */
void config_write_crash()
{
	char name[32];
	FILE *f;

	chdir(home_dir);
	chdir(".gg");

	snprintf(name, sizeof(name), "config.%d", getpid());
	if (!(f = fopen(name, "w")))
		return;
	
	config_write_main(f, 0);
	
	fclose(f);
}

/*
 * config_write()
 *
 * zapisuje aktualn� konfiguracj� -- zmienne i list� ignorowanych do pliku
 * ~/.gg/config lub podanego.
 *
 *  - filename.
 *  - base64 - czy kodowa� zmienne?
 */
int config_write(char *filename)
{
	char *tmp;
	FILE *f;

	if (!(tmp = prepare_path("")))
		return -1;
    	
	if (mkdir(tmp, 0700) && errno != EEXIST)
		return -1;

	if (!filename) {
		if (!(filename = prepare_path("config")))
			return -1;
	}
	
	if (!(f = fopen(filename, "w")))
		return -1;
	
	fchmod(fileno(f), 0600);

	config_write_main(f, 1);

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

	if (mkdir(tmp, 0700) && errno != EEXIST)
		return -1;

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
 * zwraca kolejny token oddzielany podanym znakiem. niszczy wej�ciowy
 * ci�g znak�w. bo po co on komu?
 *
 *  - ptr - gdzie ma zapisywa� aktualn� pozycj� w ci�gu,
 *  - sep - znak oddzielaj�cy tokeny.
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
 * zamienia krzaczki pisane w cp1250 na iso-8859-2, przy okazji maskuj�c
 * znaki, kt�rych nie da si� wy�wietli�, za wyj�tkiem \r i \n.
 *
 *  - buf.
 */
void cp_to_iso(unsigned char *buf)
{
	if (!buf)
		return;

	while (*buf) {
		if (*buf == (unsigned char)'�') *buf = '�';
		if (*buf == (unsigned char)'�') *buf = '�';
		if (*buf == 140) *buf = '�';
		if (*buf == 156) *buf = '�';
		if (*buf == 143) *buf = '�';
		if (*buf == 159) *buf = '�';

                if (*buf != 13 && *buf != 10 && (*buf < 32 || (*buf > 127 && *buf < 160)))
                        *buf = '?';

		buf++;
	}
}

/*
 * iso_to_cp()
 *
 * zamienia sensowny format kodowania polskich znaczk�w na bezsensowny.
 *
 *  - buf.
 */
void iso_to_cp(unsigned char *buf)
{
	if (!buf)
		return;

	while (*buf) {
		if (*buf == (unsigned char)'�') *buf = '�';
		if (*buf == (unsigned char)'�') *buf = '�';
		if (*buf == (unsigned char)'�') *buf = '�';
		if (*buf == (unsigned char)'�') *buf = '�';
		if (*buf == (unsigned char)'�') *buf = '�';
		if (*buf == (unsigned char)'�') *buf = '�';
		buf++;
	}
}

/*
 * unidle()
 *
 * uaktualnia licznik czasu ostatniej akcji, �eby przypadkiem nie zrobi�o
 * autoawaya, kiedy piszemy.
 */
void unidle()
{
	time(&last_action);
}

/*
 * timestamp()
 *
 * zwraca statyczny buforek z �adnie sformatowanym czasem.
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
 * je�li jest w��czony autoreconnect, wywo�uje timer, kt�ry za podan�
 * ilo�� czasu spr�buje si� po��czy� jeszcze raz.
 */
void do_reconnect()
{
	if (config_auto_reconnect && connecting)
		reconnect_timer = time(NULL);
}

/*
 * send_sms()
 *
 * wysy�a sms o podanej tre�ci do podanej osoby.
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
 * odtwarza dzwi�k o podanej nazwie.
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
 * czyta linijk� tekstu z pliku alokuj�c przy tym odpowiedni buforek.
 */
char *read_file(FILE *f)
{
	char buf[1024], *new, *res = NULL;

	while (fgets(buf, sizeof(buf) - 1, f)) {
		int new_size = ((res) ? strlen(res) : 0) + strlen(buf) + 1;

		if (!(new = realloc(res, new_size))) {
			/* je�li braknie pami�ci, pomijamy reszt� linii */
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
 * dopisuje do listy uruchomionych dzieci proces�w.
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
 * usuwa proces z listy dzieciak�w.
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
 * zwraca 1 je�li tekst znaczy w��czy�, 0 je�li wy��czy�, -1 je�li co innego.
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
 * dopisuje alias do listy alias�w.
 *
 * - string - linia w formacie 'alias cmd'
 * - quiet - czy wypluwa� mesgi na stdout
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
 * usuwa alias z listy alias�w.
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
 * sprawdza czy komenda w foo jest aliasem, je�li tak - zwraca cmd,
 * w przeciwnym razie NULL.
 *
 * - foo
 */
char *is_alias(char *foo)
{
	struct list *l;
	char *param = NULL, *line = strdup(foo);

	if ((param = strchr(line, ' ')))
		*param++ = 0;

	for (l = aliases; l; l = l->next) {
		struct alias *j = l->data;

		if (!strcmp(line, j->alias)) {
			char *tmp = saprintf("%s %s", j->cmd, (param) ? param : "");
			free(line);
			return tmp;
		}
	}

	free(line);

	return NULL;
}

/*
 * format_events()
 *
 * zwraca �a�cuch zdarze� w oparciu o flagi. statyczny bufor.
 *
 *  - flags
 * 
 */
char *format_events(int flags)
{
        static char buff[64];

	buff[0] = 0;

        if (flags & EVENT_MSG)
		strcat(buff, *buff ? "|msg" : "msg");
        if (flags & EVENT_CHAT)
		strcat(buff, *buff ? "|chat" : "chat");
        if (flags & EVENT_AVAIL)
		strcat(buff, *buff ? "|avail" : "avail");
	if (flags & EVENT_INVISIBLE)
	    	strcat(buff, *buff ? "|invisible" : "invisible");
        if (flags & EVENT_NOT_AVAIL)
		strcat(buff, *buff ? "|disconnect" : "disconnect");
        if (flags & EVENT_AWAY)
		strcat(buff, *buff ? "|away" : "away");
        if (flags & EVENT_DCC)
		strcat(buff, *buff ? "|dcc" : "dcc");
        if (flags & EVENT_EXEC)
		strcat(buff, *buff ? "|exec" : "exec");
        if (flags & EVENT_SIGUSR1)
		strcat(buff, *buff ? "|sigusr1" : "sigusr1");
        if (flags & EVENT_SIGUSR2)
		strcat(buff, *buff ? "|sigusr2" : "sigusr2");
        if (strlen(buff) > 37)
		strcpy(buff, "*");

        return buff;
}

/*
 * get_flags()
 *
 * zwraca flagi na podstawie �a�cucha.
 *
 * - events
 */
int get_flags(char *events)
{
        int flags = 0;

        if (!strncasecmp(events, "*", 1))
		return EVENT_MSG|EVENT_CHAT|EVENT_AVAIL|EVENT_NOT_AVAIL|EVENT_AWAY|EVENT_DCC|EVENT_INVISIBLE;
        if (strstr(events, "msg") || strstr(events, "MSG"))
		flags |= EVENT_MSG;
        if (strstr(events, "chat") || strstr(events, "CHAT"))
		flags |= EVENT_CHAT;
        if (strstr(events, "avail") || strstr(events, "AVAIL"))
		flags |= EVENT_AVAIL;
        if (strstr(events, "disconnect") || strstr(events, "disconnect"))
		flags |= EVENT_NOT_AVAIL;
        if (strstr(events, "away") || strstr(events, "AWAY"))
		flags |= EVENT_AWAY;
        if (strstr(events, "dcc") || strstr(events, "DCC"))
		flags |= EVENT_DCC;
        if (strstr(events, "exec") || strstr(events, "EXEC"))
		flags |= EVENT_DCC;
	if (strstr(events, "invisible") || strstr(events, "INVISIBLE"))
	    	flags |= EVENT_INVISIBLE;
	if (strstr(events, "sigusr1") || strstr(events, "SIGUSR1"))
	    	flags |= EVENT_SIGUSR1;
	if (strstr(events, "sigusr2") || strstr(events, "SIGUSR2"))
	    	flags |= EVENT_SIGUSR2;

        return flags;
}

/*
 * add_event()
 *
 * Dodaje zdarzenie do listy zdarze�.
 *
 * - flags
 * - uin
 * - action
 * - quiet  
 */
int add_event(int flags, uin_t uin, char *action, int quiet)
{
        int f;
        struct list *l;
        struct event e;

        for (l = events; l; l = l->next) {
                struct event *ev = l->data;

                if (ev->uin == uin && (f = ev->flags & flags) != 0) {
		    	if (!quiet)
			    	my_printf("events_exist", format_events(f), (uin == 1) ? "*"  : format_user(uin));
                        return -1;
                }
        }

        e.uin = uin;
        e.flags = flags;
        e.action = strdup(action);

        list_add(&events, &e, sizeof(e));

	if (!quiet)
	    	my_printf("events_add", format_events(flags), (uin == 1) ? "*"  : format_user(uin), action);

        return 0;
}

/*
 * del_event()
 *
 * usuwa zdarzenie z listy zdarze�.
 *
 * - flags
 * - uin
 */
int del_event(int flags, uin_t uin)
{
        struct list *l;

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                if (e && e->uin == uin && e->flags & flags) {
                        if ((e->flags &= ~flags) == 0) {
                                my_printf("events_del", format_events(flags), (uin == 1) ? "*" : format_user(uin), e->action);
				free(e->action);
                                list_remove(&events, e, 1);
                                return 0;
                        } else {
                                my_printf("events_del_flags", format_events(flags));
                                list_remove(&events, e, 0);
                                list_add_sorted(&events, e, 0, NULL);
                                return 0;
                        }
                }
        }

        my_printf("events_del_noexist", format_events(flags), (uin == 1) ? "3"  : format_user(uin));

        return 1;
}

/*
 * check_event()
 *
 * sprawdza i ewentualnie uruchamia akcj� na podane zdarzenie.
 *
 * - event
 * - uin
 */
int check_event(int event, uin_t uin, const char *data)
{
        char *evt_ptr = NULL, *uin_number = NULL, *uin_display = NULL;
	struct userlist *u;
        struct list *l;

	uin_number = itoa(uin);
	if ((u = userlist_find(uin, NULL)))
		uin_display = u->display;
	else
		uin_display = uin_number;

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                if ((e->flags & event) && (e->uin == uin || e->uin == 1)) {
			evt_ptr = strdup(e->action);
			break;
                }
        }

        if (!evt_ptr)
                return 1;

        if (strchr(evt_ptr, ';')) {
		char **events = array_make(evt_ptr, ";", 0, 0, 0);
		int i = 0;
		
                while (events[i]) {
			char *tmp = format_string(events[i], uin_number, uin_display, (data) ? data : "");
			run_event(tmp);
			free(tmp);
			i++;
		}
		
		array_free(events);
        } else {
		char *tmp = format_string(evt_ptr, uin_number, uin_display, (data) ? data : "");
		run_event(tmp);
		free(tmp);
	}

	free(evt_ptr);

        return 0;
}

/*
 * run_event()
 *
 * wykonuje dan� akcj�.
 *
 * - action
 */
int run_event(char *act)
{
        uin_t uin;
        char *action, *ptr, **acts;
#ifdef IOCTL
	int res;
#endif

	gg_debug(GG_DEBUG_MISC, "// run_event(\"%s\");\n", act);

	if (!(action = strdup(act)))
		return 1;
	
	ptr = action;
	
	while (isspace(*ptr)) 
	    ptr++;

        if (strchr(ptr, ' ')) 
	    	acts = array_make(ptr, " ", 2, 0, 0);
        else 
	    	acts = array_make(ptr, " ", 1, 0, 0);

#ifdef IOCTL
        if (!strncasecmp(acts[0], "blink_leds", 10)) {
		gg_debug(GG_DEBUG_MISC, "//   blinking leds\n");
		res = send_event(acts[1], ACT_BLINK_LEDS);
		free(action);
		array_free(acts);
                return res;
	}

        if (!strncasecmp(acts[0], "beeps_spk", 9)) {
		gg_debug(GG_DEBUG_MISC, "//   beeping speaker\n");
		res = send_event(acts[1], ACT_BEEPS_SPK);
		free(action);
		array_free(acts);
		return res;
	}
#endif /* IOCTL */
 	
	if (!strncasecmp(acts[0], "play", 4)) {
		gg_debug(GG_DEBUG_MISC, "//   playing sound\n");
		play_sound(acts[1]);
		goto cleanup;
	} 

	if (!strncasecmp(acts[0], "exec", 4)) {
		gg_debug(GG_DEBUG_MISC, "//   *bzzzt*, be back later\n");

#if 0
		gg_debug(GG_DEBUG_MISC, "//   executing program\n");
                if (!(pid = fork())) {
                        execl("/bin/sh", "sh", "-c", acts[1], NULL);
                        exit(1);
                }
                add_process(pid, "\002");
#endif
		goto cleanup;
	} 

	if (!strncasecmp(acts[0], "command", 7)) {
		gg_debug(GG_DEBUG_MISC, "//   executing program\n");
		execute_line(action + 8);
		goto cleanup;
	} 

	if (!strncasecmp(acts[0], "chat", 4) || !strncasecmp(acts[0], "msg", 3)) {
                struct userlist *u;
		char *data;
		int i = 0, chat = (!strncasecmp(acts[0], "chat", 4));

		gg_debug(GG_DEBUG_MISC, "//   chatting/mesging\n");
		
                if (!strchr(acts[1], ' '))
			goto fail;

		while (isalnum(acts[1][i]))
		    	i++;
		acts[1][i++] = '\0';
		
                if (!(uin = get_uin(acts[1])))
			goto fail;

		data = acts[1] + i;	

		u = userlist_find(uin, NULL);

                put_log(uin, "%s,%ld,%s,%ld,%s\n", (chat) ? "chatsend" : "msgsend", uin, (u) ? u->display : "", time(NULL), data);

                iso_to_cp(data);
                gg_send_message(sess, (chat) ? GG_CLASS_CHAT : GG_CLASS_MSG, uin, data);
		goto cleanup;
        }

        if (!strncasecmp(acts[0], "beep", 4)) {
                gg_debug(GG_DEBUG_MISC, "//   BEEP\n");
		my_puts("\007");
		goto cleanup;
        }

	gg_debug(GG_DEBUG_MISC, "//   unknown action\n");

cleanup:
	free(action);
	array_free(acts);
        return 0;

fail:
	free(action);
	array_free(acts);
        return 1;
}

#ifdef IOCTL
/*
 * send_event()
 *
 * wysy�a do ioctl_daemon'a polecenie uruchomienia akcji z ioctl.
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
#endif

/*
 * correct_event()
 *
 * sprawdza czy akcja na zdarzenie jest poprawna.
 *
 * - act
 */
int correct_event(char *act)
{
        char *action, *ev, **events;
	int a = 0;

#ifdef IOCTL
        struct action_data test;
#endif // IOCTL

	if (!(action = strdup(act)))
		return 1;

        if (!strncasecmp(action, "clear",  5)) {
		free(action);
                return 1;
	}
	
	events = array_make(action, ";", 0, 0, 0);

	while ((ev = events[a++])) {
	    	char **acts; int i = 0;
                while (*ev == ' ') ev++;

                if (strchr(ev, ' ')) 
		    	acts = array_make(ev, " \t", 2, 0, 0);
                else 
                        acts = array_make(ev, " \t", 1, 0, 0);
		

#ifdef IOCTL
                if (!strncasecmp(acts[0], "blink_leds", 10) || !strncasecmp(acts[0], "beeps_spk", 9)) {
                        if (acts[1] == NULL) {
                                my_printf("events_act_no_params", acts[0]);
				free(action);
				array_free(acts);
				array_free(events);
                                return 1;
                        }

                        if (*acts[1] == '$') {
			    	char *blah = strdup(acts[1]+1);
				
                                if (!strcmp(find_format(blah), "")) {
                                        my_printf("events_seq_not_found", blah);
					free(blah);
					free(action);
					array_free(acts);
					array_free(events);
                                        return 1;
                                }
				free(blah);
				array_free(acts);
                                continue;
                        }

                        if (events_parse_seq(acts[1], &test)) {
                                my_printf("events_seq_incorrect", acts[1]);
				free(action);
				array_free(acts);
				array_free(events);
                                return 1;
                        }

			array_free(acts);
                        continue;
                }

                else 
#endif // IOCTL		

		if (!strncasecmp(acts[0], "play", 4)) {
			if (!acts[1]) {
				my_printf("events_act_no_params", acts[0]);
				free(action);
				array_free(acts);
				array_free(events);
				return 1; 
			}
		} 
		else if (!strncasecmp(acts[0], "exec", 4)) {
			if (!acts[1]) {
				my_printf("events_act_no_params", acts[0]);
				free(action);
				array_free(acts);
				array_free(events);
				return 1; 
			}
		} 
		else if (!strncasecmp(acts[0], "command", 7)) {
			if (!acts[1]) {
				my_printf("events_act_no_params", acts[0]);
				free(action);
				array_free(acts);
				array_free(events);
				return 1; 
			}
		} 
		else if (!strncasecmp(acts[0], "chat", 4) || !strncasecmp(acts[0], "msg", 3)) {
                        if (!acts[1]) {
                                my_printf("events_act_no_params", acts[0]);
				free(action);
				array_free(acts);
				array_free(events);
                                return 1;
                        }

                        if (!strchr(acts[1], ' ')) {
                                my_printf("events_act_no_params", acts[0]);
				free(action);
				array_free(acts);
				array_free(events);
                                return 1;
                        }

			while (isalnum(acts[1][i]))
			    	i++;
			acts[1][i] = '\0';

#if 0

// wywalone z tego wzgl�du, �e usera mo�na dopisa� dopiero po zdefiniowaniu
// zdarzenia i mo�e to by� te� %1 lub %2.

                        if (!get_uin(acts[1])) {
                                my_printf("user_not_found", acts[1]);
				free(action);
				array_free(acts);
				array_free(events);
                                return 1;
                        }
#endif

			array_free(acts);
                        continue;
                }

		else if (!strncasecmp(acts[0], "beep", 4)) {
		    	if (acts[1]) {
			    	my_printf("events_act_toomany_params", acts[0]);
				free(action);
				array_free(acts);
				array_free(events);
				return 1;
			}
		}
				
                else {
                        my_printf("events_noexist");
			free(action);
			array_free(acts);
			array_free(events);
                        return 1;
                }

		array_free(acts);
        }

	free(action);
	array_free(events);

        return 0;
}

/*
 * events_parse_seq()
 *
 * zamie� string na odpowiedni� struktur�, zwraca >0 w przypadku b��du.
 *
 * seq
 * data
 */
int events_parse_seq(char *seq, struct action_data *data)
{
        char tmp_buff[16] = "";
        int i = 0, a, l = 0, default_delay = 10000;

        if (!data || !seq || !isdigit(seq[0]))
                return 1;

        for (a = 0; a <= strlen(seq) && a < MAX_ITEMS; a++) {
                if (i > 15)
			return 2;
                if (isdigit(seq[a]))
                        tmp_buff[i++] = seq[a];
                else if (seq[a] == '/') {
                        data->value[l] = atoi(tmp_buff);
                        memset(tmp_buff, 0, 16);
                        for (i = 0; isdigit(seq[++a]); i++)
                                tmp_buff[i] = seq[a];
                        data->delay[l] = default_delay = atoi(tmp_buff);
                        memset(tmp_buff, 0, 16);
                        i = 0;
                        l++;
                }
                else if (seq[a] == ',') {
                        data->value[l] = atoi(tmp_buff);
                        data->delay[l] = default_delay;
                        memset(tmp_buff, 0, 16);
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

#ifdef IOCTL

/*
 * init_socket()
 *
 * inicjuje gniazdo oraz struktur� addr dla ioctl_daemon'a.
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

#endif

static char base64_charset[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * base64_encode()
 *
 * zapisuje ci�g znak�w w base64. alokuje pami��. 
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
 * wczytuje ci�g znak�w base64, zwraca zaalokowany buforek.
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
 * funkcja wywo�ywana przy zmianie warto�ci zmiennej ,,debug''.
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
 * funkcja wywo�ywana przy zmianie warto�ci zmiennej ,,dcc''.
 */
void changed_dcc(char *var)
{
	struct gg_dcc *dcc = NULL;
	struct list *l;
	
	if (!strcmp(var, "dcc")) {
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
			if (!(dcc = gg_dcc_socket_create(config_uin, 0))) {
				my_printf("dcc_create_error", strerror(errno));
			} else
				list_add(&watches, dcc, 0);
		}
	}

	if (!strcmp(var, "dcc_ip"))
		gg_dcc_ip = inet_addr(config_dcc_ip);
}
	
/*
 * changed_theme()
 *
 * funkcja wywo�ywana przy zmianie warto�ci zmiennej ,,theme''.
 */
void changed_theme(char *var)
{
	if (!config_theme) {
		init_theme();
		reset_theme_cache();
	} else {
		if (!read_theme(config_theme, 1)) {
			reset_theme_cache();
			if (!in_autoexec)
				my_printf("theme_loaded", config_theme);
		} else
			if (!in_autoexec)
				my_printf("error_loading_theme", strerror(errno));
	}
}

/*
 * changed_proxy()
 *
 * funkcja wywo�ywana przy zmianie warto�ci zmiennej ,,proxy''.
 */
void changed_proxy(char *var)
{
	char *tmp;
	
	if (!config_proxy) {
		gg_proxy_enabled = 0;
		free(gg_proxy_host);
		gg_proxy_host = NULL;
		gg_proxy_port = 0;
		return;
	}

	gg_proxy_enabled = 1;
	free(gg_proxy_host);

	if ((tmp = strchr(config_proxy, ':'))) {
		int len = (int) tmp - (int) config_proxy;
		
		gg_proxy_port = atoi(tmp + 1);
		gg_proxy_host = malloc(len + 1);
		if (gg_proxy_host) {
			strncpy(gg_proxy_host, config_proxy, len);
			gg_proxy_host[len] = 0;
		}
	} else {
		gg_proxy_host = strdup(config_proxy);
		gg_proxy_port = 8080;
	}
}

/*
 * do_connect()
 *
 * przygotowuje wszystko pod po��czenie gg_login i ��czy si�.
 */
void do_connect()
{
	struct list *l;
	struct gg_login_params p;

	for (l = watches; l; l = l->next) {
		struct gg_dcc *d = l->data;
		
		if (d->type == GG_SESSION_DCC_SOCKET) {
			gg_dcc_port = d->port;
			
		}
	}

	memset(&p, 0, sizeof(p));

	p.uin = config_uin;
	p.password = config_password;
	p.status = config_status;
	p.async = 1;
	p.client_version = config_protocol;

	if (config_server) {
		char *tmp = strchr(config_server, ':'), *foo = strdup(config_server);
		int len = (int) tmp - (int) config_server;
			
		if (foo) {
			if (tmp) {
				p.server_port = atoi(tmp + 1);
				foo[len] = 0;
				p.server_addr = inet_addr(foo);
				gg_debug(GG_DEBUG_MISC, "-- server_addr=%s, server_port=%d\n", foo, p.server_port);
			} else {
				p.server_port = GG_DEFAULT_PORT;
				p.server_addr = inet_addr(config_server);
				gg_debug(GG_DEBUG_MISC, "-- server_addr=%s, server_port=%d\n", config_server, p.server_port);
			}
			free(foo);
		}
	}

	if (!(sess = gg_login(&p))) {
		my_printf("conn_failed", strerror(errno));
		do_reconnect();
	} else
		list_add(&watches, sess, 0);
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
 * dzia�a tak samo jak strdup(), tyle �e przy argumencie r�wnym NULL
 * zwraca NULL zamiast segfaultowa�.
 *
 *  - ptr - bufor do skopiowania.
 *
 * zwraca zaalokowany bufor lub NULL.
 */
char *strdup_null(char *ptr)
{
	return (ptr) ? strdup(ptr) : NULL;
}

/*
 * ekg_logoff()
 *
 * roz��cza si�, zmieniaj�c uprzednio stan na niedost�pny z opisem.
 *
 *  - sess - opis sesji,
 *  - reason - pow�d, mo�e by� NULL.
 *
 * niczego nie zwraca.
 */
void ekg_logoff(struct gg_session *sess, char *reason)
{
	char *tmp = NULL;

	if (!sess)
		return;

	if (reason)
		tmp = strdup(reason);
	
	if (tmp) {
		iso_to_cp(tmp);
		gg_change_status_descr(sess, GG_STATUS_NOT_AVAIL_DESCR, tmp);
		free(tmp);
	} else
		gg_change_status(sess, GG_STATUS_NOT_AVAIL);

	gg_logoff(sess);
}

char *get_random_reason(char *path)
{
        int max = 0, embryo, item, fd, tmp = 0;
        char buf[256];
        FILE *f;

        if ((f = fopen(path, "r")) == NULL)
                return NULL;

        while (fgets(buf, sizeof(buf) - 1, f))
                max++;

        rewind(f);

        if((fd = open("/dev/urandom", O_RDONLY)) > 0) {
                read(fd, &embryo, sizeof(embryo));
                close(fd);
        }
        else
                embryo=(int)time(NULL);

        srand(embryo);

        item = (rand()%max) + 1;

        while (fgets(buf, sizeof(buf) - 1, f)) {
                tmp++;
                if (tmp == item) {
                        fclose(f);
                        if (buf[strlen(buf) - 1] == '\n')
                                buf[strlen(buf) - 1] = '\0';
			return strdup(buf);
                }
        }

        fclose(f);
        return NULL;
}

