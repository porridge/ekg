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
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef WITH_IOCTLD
#  include <sys/un.h>
#endif
#include <ctype.h>
#include "config.h"
#include "compat.h"
#include "libgadu.h"
#include "stuff.h"
#include "dynstuff.h"
#include "themes.h"
#include "commands.h"
#include "vars.h"
#include "userlist.h"
#include "xmalloc.h"
#include "ui.h"

struct gg_session *sess = NULL;
list_t children = NULL;
list_t aliases = NULL;
list_t watches = NULL;
list_t transfers = NULL;
list_t events = NULL;
list_t emoticons = NULL;
list_t sequences = NULL;

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
char *config_dcc_dir = NULL;
int sock = 0;
int length = 0;
#ifdef WITH_IOCTLD
struct sockaddr_un addr;
#endif 
char *config_reason = NULL;
char *home_dir = NULL;
char *config_quit_reason = NULL;
char *config_away_reason = NULL;
char *config_back_reason = NULL;
int config_random_reason = 0;
int config_query_commands = 0;
char *config_proxy = NULL;
char *config_server = NULL;
int quit_message_send = 0;
int registered_today = 0;
int config_protocol = 0;
int pipe_fd = -1;
int batch_mode = 0;
char *batch_line = NULL;
int immediately_quit = 0;
int config_emoticons = 1;
int config_make_window = 0;
int ekg_segv_handler = 0;
char *config_tab_command = NULL;

static struct {
	int event;
	char *name;
} event_labels[] = {
	{ EVENT_MSG, "msg" },
	{ EVENT_CHAT, "chat" },
	{ EVENT_AVAIL, "avail" },
	{ EVENT_NOT_AVAIL, "disconnect" },
	{ EVENT_AWAY, "away" },
	{ EVENT_DCC, "dcc" },
	{ EVENT_INVISIBLE, "invisible" },
	{ EVENT_EXEC, "exec" },
	{ EVENT_SIGUSR1, "sigusr1" },
	{ EVENT_SIGUSR2, "sigusr2" },
	{ 0, NULL },
};

/*
 * emoticon_expand()
 *
 * rozwija definicje makr (najczesciej to beda emoticony)
 *
 * - s - string z makrami
 *
 * zwraca zaalokowany, rozwiniety string.
 */
char *emoticon_expand(const char *s)
{
	list_t l = NULL;
	const char *ss;
	char *ms;
	size_t n = 0;

	for (ss = s; *ss; ss++) {
		struct emoticon *e = NULL;
		size_t ns = strlen(ss);
		int ret = 1;

		for (l = emoticons; l && ret; l = (ret ? l->next : l)) {
			size_t nn;

			e = l->data;
			nn = strlen(e->name);
			if (ns < nn)
				nn = ns;
			ret = strncmp(ss, e->name, nn);
		}

		if (l) {
			e = l->data;
			n += strlen(e->value);
			ss += strlen(e->name) - 1;
		} else
			n++;
	}

	ms = xcalloc(1, n + 1);

	for (ss = s; *ss; ss++) {
		struct emoticon *e = NULL;
		size_t ns = strlen(ss);
		int ret = 1;

		for (l = emoticons; l && ret; l = (ret ? l->next : l)) {
			size_t n;

			e = l->data;
			n = strlen(e->name);
			if (ns < n)
				n = ns;
			ret = strncmp(ss, e->name, n);
		}

		if (l) {
			e = l->data;
			strcat(ms, e->value);
			ss += strlen(e->name) - 1;
		} else
			ms[strlen(ms)] = *ss;
	}

	return ms;
}

/*
 * prepare_path()
 *
 * zwraca pe�n� �cie�k� do podanego pliku katalogu ~/.gg/
 *
 *  - filename - nazwa pliku.
 *  - do_mkdir - czy tworzy� katalog ~/.gg ?
 */
const char *prepare_path(const char *filename, int do_mkdir)
{
	static char path[PATH_MAX];
	
	if (do_mkdir) {
		if (mkdir(config_dir, 0700) && errno != EEXIST)
			return NULL;
		if (config_user && *config_user) {
			snprintf(path, sizeof(path), "%s/%s", config_dir, config_user);
			if (mkdir(path, 0700) && errno != EEXIST)
				return NULL;
		}
	}
	
	if (!filename || !*filename) {
		if (config_user && *config_user)
			snprintf(path, sizeof(path), "%s/%s", config_dir, config_user);
		else
			snprintf(path, sizeof(path), "%s", config_dir);
	} else {
		if (config_user && *config_user)
			snprintf(path, sizeof(path), "%s/%s/%s", config_dir, config_user, filename);
		else
			snprintf(path, sizeof(path), "%s/%s", config_dir, filename);
	}
	
	return path;
}

/*
 * log()
 *
 * wrzuca do log�w informacj� od/do danego numerka. podaje si� go z tego
 * wzgl�du, �e gdy `log = 2', informacje lec� do $config_log_path/$uin.
 *
 * - uin,
 * - format...
 */
void log(uin_t uin, const char *format, ...)
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
		snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%u", uin);
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
 * config_read()
 *
 * czyta z pliku ~/.gg/config lub podanego konfiguracj� i list� ignorowanych
 * u�yszkodnik�w.
 *
 *  - filename.
 */
int config_read()
{
	const char *filename;
	char *buf, *foo;
	FILE *f;

	if (!(filename = prepare_path("config", 0)))
		return -1;
	
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
			alias_add(foo, 1, 1);
		} else if (!strcasecmp(buf, "on")) {
                        int flags;
                        uin_t uin;
                        char **pms = array_make(foo, " \t", 3, 1, 0);
                        if (pms && pms[0] && pms[1] && pms[2] && (flags = event_flags(pms[0])) && (uin = atoi(pms[1])) && !event_correct(pms[2]))
                                event_add(event_flags(pms[0]), atoi(pms[1]), pms[2], 1);
			array_free(pms);
		} else if (!strcasecmp(buf, "bind")) {
			char **pms = array_make(foo, " \t", 2, 1, 0);
			if (pms && pms[0] && pms[1]) 
				ui_event("command", "bind", "--add-quiet", pms[0], pms[1]);
                } else 
			variable_set(buf, foo, 1);

		free(buf);
	}
	
	fclose(f);
	
	return 0;
}

/*
 * sysmsg_read()
 *
 *  - filename.
 */
int sysmsg_read()
{
	const char *filename;
	char *buf, *foo;
	FILE *f;

	if (!(filename = prepare_path("sysmsg", 0)))
		return -1;
	
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
		
		xfree(buf);
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
	list_t l;

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

		fprintf(f, "ignore %u\n", i->uin);
	}

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;
		list_t m;

		for (m = a->commands; m; m = m->next)
			fprintf(f, "alias %s %s\n", a->name, (char*) m->data);
	}

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                fprintf(f, "on %s %u %s\n", event_format(e->flags), e->uin, e->action);
        }

	for (l = sequences; l; l = l->next) {
		struct sequence *s = l->data;

		fprintf(f, "bind %s %s\n", s->seq, s->command);
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

	chdir(config_dir);

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
int config_write()
{
	const char *filename;
	FILE *f;

	if (!(filename = prepare_path("config", 1)))
		return -1;
	
	if (!(f = fopen(filename, "w")))
		return -1;
	
	fchmod(fileno(f), 0600);

	config_write_main(f, 1);

	fclose(f);
	
	return 0;
}

/*
 * sysmsg_write()
 *
 *  - filename.
 */
int sysmsg_write()
{
	const char *filename;
	FILE *f;

	if (!(filename = prepare_path("sysmsg", 1)))
		return -1;
	
	if (!(f = fopen(filename, "w")))
		return -1;
	
	fchmod(fileno(f), 0600);
	
	fprintf(f, "last_sysmsg %i\n", last_sysmsg);
	
	fclose(f);
	
	return 0;
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
const char *timestamp(const char *format)
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
int send_sms(const char *recipient, const char *message, int show_result)
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

	process_add(pid, buf);

	return 0;
}

/*
 * print_history()
 * 
 * wy�wietla ostatnie n wypowiedzi w rozmowie z podan� osob�.
 */
int print_history(uin_t uin, int no)
{
#if 0
	char *buf, *filename=config_log_path;
	char path[PATH_MAX];
	int f_lines_count=0;
	int i=0;
	char *token=NULL;
	FILE *f;
	
	if (!config_log)
		return 0;
	
	if (!filename) {
		if (config_log == 2)
			filename = ".";
		else
			filename = "gg.log";
	}

	if (*filename == '~')
		snprintf(path, sizeof(path), "%s%s", home_dir, filename + 1);
        else
		strncpy(path, filename, sizeof(path));

	if (config_log == 2) {
                if (mkdir(path, 0700) && errno != EEXIST)
                        return 0;
                snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%u", uin);
	}

	if (!(f = fopen(path, "r")))
		return 0;
	
	if (config_log == 1)	// jesli wszystko jest w jednym pliku
		token = saprintf(",%u,", itoa(uin));
	
	while ((buf = read_file(f))) {
		if (config_log == 2 || strstr(buf, token))
			f_lines_count++;
		free(buf);
	}

	if (!f_lines_count) {
		print("history_error", "Brak historii dla wybranego u�ytkownika");
		return 0;
	}

	if (f_lines_count < no)	// mamy mniej logow, niz user sobie zyczy
		f_lines_count = 0;
	else
		f_lines_count -= no;
	
	fseek(f, 0, SEEK_SET);
	
	while ((buf = read_file(f))) {
		if (config_log ==2 || strstr(buf, token))
			i++;
		
		if (i > f_lines_count && (config_log == 2 || strstr(buf, token))) {
			char *msgtype=NULL, *uin=NULL, *nick=NULL, *notsure=NULL, *ip=NULL, *msg=NULL;
			char time1[256], time2[256];
			time_t val=0;

			msgtype = strtok(buf, ",");
			uin = strtok(NULL, ",");
			nick = strtok(NULL, ",");
			notsure = strtok(NULL, ",");
			
			if (!strtol(notsure, NULL, 0)) {
				ip = malloc(strlen(notsure));
				strcpy(ip, notsure);
				notsure = strtok(NULL, ",");
			} else {
				ip = malloc(strlen("0.0.0.0"));
				ip = "0.0.0.0";
			}
			
			val = strtol(notsure, NULL, 0);
			strftime(time1, sizeof(time1), "%Y-%m-%d %T", localtime(&val));
			notsure = strtok(NULL, ",");
			if (strtol(notsure, NULL, 0) > 0) {
				val = strtol(notsure, NULL, 0);
				strftime(time2, sizeof(time2), "%Y-%m-%d %T", localtime(&val));
				notsure = strtok(NULL, ",");
			} else
				strcpy(time2, "");
			msg = xstrdup(notsure);
			notsure = strtok(NULL, ",");
			
			while (notsure != NULL) {
				strcat(msg, notsure);
				notsure = strtok(NULL, ",");
			}
			
			if (strstr(msgtype, "send"))
				print("history_send", uin, nick, time1, msg, ip, time2);
			else
				print("history_recv", uin, nick, time1, msg, ip, time2);
		}
		free(buf);
	}
	
	free(token);
	fclose(f);	
#endif
	return 0;
}

/*
 * play_sound()
 *
 * odtwarza dzwi�k o podanej nazwie.
 */
int play_sound(const char *sound_path)
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

	process_add(pid, "\002");

	return 0;
}

/*
 * read_file()
 *
 * czyta linijk� tekstu z pliku alokuj�c przy tym odpowiedni buforek.
 */
char *read_file(FILE *f)
{
	char buf[1024], *res = NULL;

	while (fgets(buf, sizeof(buf) - 1, f)) {
		int first = (res) ? 0 : 1;
		int new_size = ((res) ? strlen(res) : 0) + strlen(buf) + 1;

		res = xrealloc(res, new_size);
		if (first)
			*res = 0;
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
 * process_add()
 *
 * dopisuje do listy uruchomionych dzieci proces�w.
 *
 *  - pid.
 *  - name.
 */
int process_add(int pid, const char *name)
{
	struct process p;

	p.pid = pid;
	p.name = xstrdup(name);
	list_add(&children, &p, sizeof(p));
	
	return 0;
}

/*
 * process_remove()
 *
 * usuwa proces z listy dzieciak�w.
 *
 *  - pid.
 */
int process_remove(int pid)
{
	list_t l;

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
int on_off(const char *value)
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
 * alias_add()
 *
 * dopisuje alias do listy alias�w.
 *
 * - string - linia w formacie 'alias cmd'
 * - quiet - czy wypluwa� mesgi na stdout.
 */
int alias_add(const char *string, int quiet, int append)
{
	char *cmd;
	list_t l;
	struct alias a;

	if (!string || !(cmd = strchr(string, ' '))) {
		if (!quiet)
			print("not_enough_params");
		return -1;
	}

	*cmd++ = 0;

	for (l = aliases; l; l = l->next) {
		struct alias *j = l->data;

		if (!strcasecmp(string, j->name)) {
			if (!append) {
				if (!quiet)
					print("aliases_exist", string);
				return -1;
			} else {
				list_add(&j->commands, cmd, strlen(cmd) + 1);
				if (!quiet)
					print("aliases_append", string);
				return 0;
			}
		}
	}

	a.name = xstrdup(string);
	a.commands = NULL;
	list_add(&a.commands, cmd, strlen(cmd) + 1);
	list_add(&aliases, &a, sizeof(a));
	command_add(a.name, "?", cmd_alias_exec, 1, "", "", "");

	if (!quiet)
		print("aliases_add", a.name, "");

	return 0;
}

/*
 * alias_remove()
 *
 * usuwa alias z listy alias�w.
 *
 * - name - alias.
 */
int alias_remove(const char *name)
{
	list_t l;

	if (!name) {
		print("not_enough_params");
		return -1;
	}

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;

		if (!strcasecmp(a->name, name)) {
			print("aliases_del", name);
			command_remove(a->name);
			xfree(a->name);
			list_destroy(a->commands, 1);
			list_remove(&aliases, a, 1);
			return 0;
		}
	}

	print("aliases_noexist", name);

	return -1;
}

/*
 * alias_check()
 *
 * sprawdza czy komenda w foo jest aliasem, je�li tak - zwraca list�
 * komend, innaczej NULL.
 *
 *  - line - linia z komend�.
 */
list_t alias_check(const char *line)
{
	list_t l;
	int i = 0;

	while (*line == ' ')
		line++;

	while (line[i] != ' ' && line[i])
		i++;

	if (!i)
		return NULL;

	for (l = aliases; l; l = l->next) {
		struct alias *j = l->data;

		if (strlen(j->name) >= i && !strncmp(line, j->name, i))
			return j->commands;
	}

	return NULL;
}

/*
 * alias_free()
 *
 * usuwa pami�� zaj�t� przez aliasy.
 */
void alias_free()
{
	list_t l;

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;
		
		xfree(a->name);
		list_destroy(a->commands, 1);
	}

	list_destroy(aliases, 1);
}
/*
 * event_format()
 *
 * zwraca �a�cuch zdarze� w oparciu o flagi. statyczny bufor.
 *
 *  - flags.
 */
const char *event_format(int flags)
{
        static char buf[100];
	int i, first = 1;

	buf[0] = 0;

	if (flags == EVENT_ALL)
		return "*";

	for (i = 0; event_labels[i].event; i++) {
		if ((flags & event_labels[i].event)) {
			if (!first)
				strcat(buf, ",");
			strcat(buf, event_labels[i].name);
			first = 0;
		}
	}

	return buf;
}

/*
 * event_flags()
 *
 * zwraca flagi na podstawie �a�cucha.
 *
 * - events
 */
int event_flags(const char *events)
{
	int i, j, flags = 0;
	char **a;

	if (!(a = array_make(events, "|,:", 0, 1, 0)))
		return 0;

	for (j = 0; a[j]; j++) {
		if (!strcmp(a[j], "*"))
			flags |= EVENT_ALL;
		for (i = 0; event_labels[i].event; i++)
			if (!strcasecmp(a[j], event_labels[i].name))
				flags |= event_labels[i].event;
	}

	array_free(a);

        return flags;
}

/*
 * event_add()
 *
 * Dodaje zdarzenie do listy zdarze�.
 *
 * - flags
 * - uin
 * - action
 * - quiet  
 */
int event_add(int flags, uin_t uin, const char *action, int quiet)
{
        int f;
        list_t l;
        struct event e;

        for (l = events; l; l = l->next) {
                struct event *ev = l->data;

                if (ev->uin == uin && (f = ev->flags & flags) != 0) {
		    	if (!quiet)
			    	print("events_exist", event_format(f), (uin == 1) ? "*" : format_user(uin));
                        return -1;
                }
        }

        e.uin = uin;
        e.flags = flags;
        e.action = xstrdup(action);

        list_add(&events, &e, sizeof(e));

	if (!quiet)
	    	print("events_add", event_format(flags), (uin == 1) ? "*"  : format_user(uin), action);

        return 0;
}

/*
 * event_remove()
 *
 * usuwa zdarzenie z listy zdarze�.
 *
 * - flags
 * - uin
 */
int event_remove(int flags, uin_t uin)
{
        list_t l;

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                if (e && e->uin == uin && e->flags & flags) {
                        if ((e->flags &= ~flags) == 0) {
                                print("events_del", event_format(flags), (uin == 1) ? "*" : format_user(uin), e->action);
				free(e->action);
                                list_remove(&events, e, 1);
                                return 0;
                        } else {
                                print("events_del_flags", event_format(flags));
                                list_remove(&events, e, 0);
                                list_add_sorted(&events, e, 0, NULL);
                                return 0;
                        }
                }
        }

        print("events_del_noexist", event_format(flags), (uin == 1) ? "3"  : format_user(uin));

        return 1;
}

/*
 * event_check()
 *
 * sprawdza i ewentualnie uruchamia akcj� na podane zdarzenie.
 *
 * - event
 * - uin
 */
int event_check(int event, uin_t uin, const char *data)
{
	const char *uin_number = NULL, *uin_display = NULL;
        char *action = NULL, **actions, *edata = NULL;
	struct userlist *u;
        list_t l;
	int i;

	uin_number = itoa(uin);
	if ((u = userlist_find(uin, NULL)))
		uin_display = u->display;
	else
		uin_display = uin_number;

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                if ((e->flags & event) && (e->uin == uin || e->uin == 1)) {
			action = e->action;
			break;
                }
        }

        if (!action)
                return 1;

	if (data) {
		int size = 1;
		const char *p;
		char *q;

		for (p = data; *p; p++) {
			if (strchr("`!#$&*?|\\\'\"{}[]<>", *p))
				size += 2;
			else
				size++;
		}
		
		edata = xmalloc(size);

		for (p = data, q = edata; *p; p++, q++) {
			if (strchr("`!#$&*?|\\\'\"{}[]<>", *p))
				*q++ = '\\';
			*q = *p;
		}
		*q = 0;
	}

	actions = array_make(action, ";", 0, 0, 0);

	for (i = 0; actions && actions[i]; i++) {	
		char *tmp = format_string(actions[i], uin_number, uin_display, (data) ? data : "", (edata) ? edata : "");
		event_run(tmp);
		free(tmp);
	}

	array_free(actions);

	xfree(edata);

        return 0;
}

/*
 * event_run()
 *
 * wykonuje dan� akcj�.
 *
 * - act.
 */
int event_run(const char *act)
{
        uin_t uin;
        char *action, *ptr, **acts;
#ifdef WITH_IOCTLD
	int res;
#endif /* WITH_IOCTLD */

	gg_debug(GG_DEBUG_MISC, "// event_run(\"%s\");\n", act);

	action = xstrdup(act);
	
	ptr = action;
	
	while (isspace(*ptr)) 
	    ptr++;

        if (strchr(ptr, ' ')) 
	    	acts = array_make(ptr, " ", 2, 0, 0);
        else 
	    	acts = array_make(ptr, " ", 1, 0, 0);

#ifdef WITH_IOCTLD
        if (!strncasecmp(acts[0], "blink", 5)) {
		gg_debug(GG_DEBUG_MISC, "//   blinking leds\n");
		res = event_send(acts[1], ACT_BLINK_LEDS);
		free(action);
		array_free(acts);
                return res;
	}

        if (!strncasecmp(acts[0], "beeps", 5)) {
		gg_debug(GG_DEBUG_MISC, "//   beeping speaker\n");
		res = event_send(acts[1], ACT_BEEPS_SPK);
		free(action);
		array_free(acts);
		return res;
	}
#endif /* WITH_IOCTLD */
 	
	if (!strcasecmp(acts[0], "play")) {
		gg_debug(GG_DEBUG_MISC, "//   playing sound\n");
		play_sound(acts[1]);
		goto cleanup;
	} 

	if (!strcasecmp(acts[0], "exec")) {
		char *tmp = saprintf("exec %s", action + 5);
		
		if (tmp) {
			gg_debug(GG_DEBUG_MISC, "//   executing program\n");
			command_exec(NULL, tmp);
			xfree(tmp);
		} else
			gg_debug(GG_DEBUG_MISC, "//   not enough memory\n");
		
		goto cleanup;
	} 

	if (!strcasecmp(acts[0], "command")) {
		gg_debug(GG_DEBUG_MISC, "//   executing command\n");
		command_exec(NULL, action + 8);
		goto cleanup;
	} 

	if (!strcasecmp(acts[0], "chat") || !strcasecmp(acts[0], "msg")) {
                struct userlist *u;
		char *data;
		int i = 0, chat = (!strcasecmp(acts[0], "chat"));

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

                log(uin, "%s,%ld,%s,%ld,%s\n", (chat) ? "chatsend" : "msgsend", uin, (u) ? u->display : "", time(NULL), data);

                iso_to_cp(data);
                gg_send_message(sess, (chat) ? GG_CLASS_CHAT : GG_CLASS_MSG, uin, data);
		goto cleanup;
        }

        if (!strcasecmp(acts[0], "beep")) {
                gg_debug(GG_DEBUG_MISC, "//   BEEP\n");
		ui_beep();
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

#ifdef WITH_IOCTLD
/*
 * event_send()
 *
 * wysy�a do ioctld polecenie uruchomienia akcji z ioctl.
 *
 * - seq
 * - act
*/
int event_send(const char *seq, int act)
{
	const char *s;
	struct action_data data;

	if (*seq == '$') {
		seq++;
		s = format_find(seq);
		if (!strcmp(s, "")) {
			print("events_seq_not_found", seq);
			return 1;
		}
	} else
		s = seq;

	data.act = act;

	if (event_parse_seq(s, &data))
		return 1;

	sendto(sock, &data, sizeof(data), 0,(struct sockaddr *)&addr, length);

	return 0;
}
#endif /* WITH_IOCTLD */

/*
 * event_correct()
 *
 * sprawdza czy akcja na zdarzenie jest poprawna.
 *
 * - act
 */
int event_correct(const char *action)
{
        char *event, **events = NULL, **acts = NULL;
	int i = 0;

#ifdef WITH_IOCTLD
        struct action_data test;
#endif /* WITH_IOCTLD */

        if (!strncasecmp(action, "clear", 5))
                return 1;
	
	events = array_make(action, ";", 0, 0, 0);

	while ((event = events[i++])) {
                while (*event == ' ')
			event++;

                if (strchr(event, ' ')) 
		    	acts = array_make(event, " \t", 2, 0, 0);
                else 
                        acts = array_make(event, " \t", 1, 0, 0);
		
#ifdef WITH_IOCTLD
                if (!strncasecmp(acts[0], "blink", 5) || !strncasecmp(acts[0], "beeps", 5)) {
                        if (!acts[1]) {
                                print("events_act_no_params", acts[0]);
				goto fail;
			}

                        if (*acts[1] == '$') {
                                if (!strcmp(format_find(acts[1] + 1), "")) {
                                        print("events_seq_not_found", acts[1] + 1);
					goto fail;
				}
                        } else if (event_parse_seq(acts[1], &test)) {
                                print("events_seq_incorrect", acts[1]);
				goto fail;
                        }

			goto check;
                }
#endif /* WITH_IOCTLD */

		if (!strncasecmp(acts[0], "play", 4)) {
			if (!acts[1]) {
				print("events_act_no_params", acts[0]);
				goto fail;
			}
			goto check;
		} 
		
		if (!strcasecmp(acts[0], "exec") || !strcasecmp(acts[0], "command")) {
			if (!acts[1]) {
				print("events_act_no_params", acts[0]);
				goto fail;
			}
			goto check;
		} 

		if (!strcasecmp(acts[0], "chat") || !strcasecmp(acts[0], "msg")) {
                        if (!acts[1]) {
                                print("events_act_no_params", acts[0]);
				goto fail;
                        }
                        if (!strchr(acts[1], ' ')) {
                                print("events_act_no_params", acts[0]);
				goto fail;
                        }
			goto check;
                }

		if (!strcasecmp(acts[0], "beep")) {
		    	if (acts[1]) {
			    	print("events_act_toomany_params", acts[0]);
				goto fail;
			}
			goto check;
		}
				
		print("events_noexist");
		goto fail;

check:
		array_free(acts);
        }

	array_free(events);
	return 0;

fail:
	array_free(events);
	array_free(acts);
        return 1;
}

/*
 * event_parse_seq()
 *
 * zamie� string na odpowiedni� struktur�.
 *
 *  - seq.
 *  - data.
 *
 * je�li w porz�dku 0, je�li nie w porz�dku > 0.
 */
int event_parse_seq(const char *seq, struct action_data *data)
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
                } else if (seq[a] == ' ')
                        continue;
                else if (seq[a] == '\0') {
                        data->value[l] = atoi(tmp_buff);
                        data->delay[l] = default_delay;
                        data->value[++l] = data->delay[l] = -1;
                } else
			return 3;
        }

	return 0;
}

/*
 * event_free()
 *
 * zwalnia pami�� zwi�zan� ze zdarzeniami.
 */
void event_free()
{
	list_t l;

	for (l = events; l; l = l->next) {
		struct event *e = l->data;

		xfree(e->action);
	}

	list_destroy(events, 1);
}

/*
 * init_control_pipe()
 *
 * inicjuje potok nazwany do zewn�trznej kontroli ekg
 *
 * - pipe_file
 *
 * zwraca deskryptor otwartego potoku lub warto�� b��du
 */
int init_control_pipe(const char *pipe_file)
{
	int fd;

	if (!pipe_file)
		return 0;
	if (mkfifo(pipe_file, 0600) < 0 && errno != EEXIST) {
		fprintf(stderr, "Nie mog� stworzy� potoku %s: %s. Ignoruj�.\n", pipe_file, strerror(errno));
		return -1;
	}
	if ((fd = open(pipe_file, O_RDWR | O_NDELAY)) < 0) {
		fprintf(stderr, "Nie mog� otworzy� potoku %s: %s. Ignoruj�.\n", pipe_file, strerror(errno));
		return -1;
	}
	return fd;
}

#ifdef WITH_IOCTLD

/*
 * init_socket()
 *
 * inicjuje gniazdo oraz struktur� addr dla ioctld.
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

#endif /* WITH_IOCTLD */

static char base64_charset[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * base64_encode()
 *
 * zapisuje ci�g znak�w w base64. alokuje pami��. 
 */
char *base64_encode(const char *buf)
{
	char *out, *res;
	int i = 0, j = 0, k = 0, len = strlen(buf);
	
	res = out = xmalloc((len / 3 + 1) * 4 + 2);
	
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
char *base64_decode(const char *buf)
{
	char *res, *save, *foo, val;
	const char *end;
	int index = 0;
	
	save = res = xcalloc(1, (strlen(buf) / 4 + 1) * 3 + 2);

	end = buf + strlen(buf);

	while (*buf && buf < end) {
		if (*buf == '\r' || *buf == '\n') {
			buf++;
			continue;
		}
		if (!(foo = strchr(base64_charset, *buf)))
			foo = base64_charset;
		val = (int)foo - (int)base64_charset;
/*		*buf = 0;	XXX kto mi powie po co to by�o dostaje piwo */
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
void changed_debug(const char *var)
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
void changed_dcc(const char *var)
{
	struct gg_dcc *dcc = NULL;
	list_t l;
	
	if (!config_uin)
		return;
	
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
				print("dcc_create_error", strerror(errno));
			} else {
				list_add(&watches, dcc, 0);
			}
		}
	}

	if (!strcmp(var, "dcc_ip")) {
		if (config_dcc_ip) {
			if (inet_addr(config_dcc_ip) != INADDR_NONE)
				gg_dcc_ip = inet_addr(config_dcc_ip);
			else {
				print("dcc_invalid_ip");
				config_dcc_ip = NULL;
				gg_dcc_ip = 0;
			}
		}
		else
			gg_dcc_ip = 0;
	}
}
	
/*
 * changed_theme()
 *
 * funkcja wywo�ywana przy zmianie warto�ci zmiennej ,,theme''.
 */
void changed_theme(const char *var)
{
	if (!config_theme) {
		theme_init();
	} else {
		if (!theme_read(config_theme, 1)) {
			theme_cache_reset();
			if (!in_autoexec)
				print("theme_loaded", config_theme);
		} else
			if (!in_autoexec)
				print("error_loading_theme", strerror(errno));
	}
}

/*
 * changed_proxy()
 *
 * funkcja wywo�ywana przy zmianie warto�ci zmiennej ,,proxy''.
 */
void changed_proxy(const char *var)
{
	char *tmp;
	
	if (!config_proxy) {
		gg_proxy_enabled = 0;
		xfree(gg_proxy_host);
		gg_proxy_host = NULL;
		gg_proxy_port = 0;
		return;
	}

	gg_proxy_enabled = 1;
	xfree(gg_proxy_host);

	if ((tmp = strchr(config_proxy, ':'))) {
		int len = (int) tmp - (int) config_proxy;
		
		gg_proxy_port = atoi(tmp + 1);
		gg_proxy_host = xmalloc(len + 1);
		strncpy(gg_proxy_host, config_proxy, len);
		gg_proxy_host[len] = 0;
	} else {
		gg_proxy_host = xstrdup(config_proxy);
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
	list_t l;
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
#ifdef HAVE_VOIP
	p.has_audio = 1;
#endif
	p.protocol_version = config_protocol;
	p.last_sysmsg = last_sysmsg;

	if (config_server) {
		char *tmp = strchr(config_server, ':'), *foo = xstrdup(config_server);
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
		print("conn_failed", strerror(errno));
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
	list_t l;
	int id = 1;

	for (l = transfers; l; l = l->next) {
		struct transfer *t = l->data;

		if (t->id >= id)
			id = t->id + 1;
	}

	return id;
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
void ekg_logoff(struct gg_session *sess, const char *reason)
{
	if (!sess)
		return;

	if (sess->state != GG_STATE_CONNECTED || sess->status == GG_STATUS_NOT_AVAIL || sess->status == GG_STATUS_NOT_AVAIL_DESCR)
		return;

	if (reason) {
		char *tmp = xstrdup(reason);
		iso_to_cp(tmp);
		gg_change_status_descr(sess, GG_STATUS_NOT_AVAIL_DESCR, tmp);
		free(tmp);
	} else
		gg_change_status(sess, GG_STATUS_NOT_AVAIL);

	gg_logoff(sess);
}

char *random_line(const char *path)
{
        int max = 0, item, tmp = 0;
	char *line;
        FILE *f;

	if (!path)
		return NULL;

        if ((f = fopen(path, "r")) == NULL)
                return NULL;

        while ((line = read_file(f))) {
		xfree(line);
                max++;
	}

        rewind(f);

        item = rand() % max;

        while ((line = read_file(f))) {
                if (tmp == item) {
			fclose(f);
			return line;
		}
		xfree(line);
		tmp++;
        }

        fclose(f);
        return NULL;
}

/*
 * emoticon_add()
 *
 * dodaje dany emoticon do listy.
 *
 *  - name - nazwa,
 *  - value - warto��,
 */
int emoticon_add(char *name, char *value)
{
	struct emoticon e;
	list_t l;

	for (l = emoticons; l; l = l->next) {
		struct emoticon *g = l->data;

		if (!strcasecmp(name, g->name)) {
			free(g->value);
			g->value = xstrdup(value);
			return 0;
		}
	}

	e.name = xstrdup(name);
	e.value = xstrdup(value);
	list_add(&emoticons, &e, sizeof(e));

	return 0;
}

/*
 * emoticon_remove()
 *
 * usuwa emoticon o danej nazwie.
 *
 *  - name.
 */
int emoticon_remove(char *name)
{
	list_t l;

	for (l = emoticons; l; l = l->next) {
		struct emoticon *f = l->data;

		if (!strcasecmp(f->name, name)) {
			free(f->value);
			free(f->name);
			list_remove(&emoticons, f, 1);

			return 0;
		}
	}
	
	return -1;
}

/*
 * emoticon_read()
 *
 * �aduje do listy wszystkie makra z pliku ~/.gg/emoticons
 * format tego pliku w dokumentacji
 */
int emoticon_read()
{
	const char *filename;
	char *buf, **emot;
	FILE *f;

	if (!(filename = prepare_path("emoticons", 0)))
		return -1;
	
	if (!(f = fopen(filename, "r")))
		return -1;

	while ((buf = read_file(f))) {
	
		if (buf[0] == '#') {
			free(buf);
			continue;
		}

		emot = array_make(buf, "\t", 2, 1, 1);
	
		if (emot) {
			if (emot[1])
				emoticon_add(emot[0], emot[1]);
			else
				emoticon_remove(emot[0]);
			free(emot[0]);
			free(emot[1]);
			free(emot);
		}

		xfree(buf);
	}
	
	fclose(f);
	
	return 0;
}

/*
 * emoticon_free()
 *
 * usuwa pami�� zaj�t� przez emoticony.
 */
void emoticon_free()
{
	list_t l;

	for (l = emoticons; l; l = l->next) {
		struct emoticon *e = l->data;

		xfree(e->name);
		xfree(e->value);
	}

	list_destroy(emoticons, 1);
}

/*
 * ekg_hash()
 *
 * liczy prosty hash z nazwy, wykorzystywany przy przeszukiwaniu list
 * zmiennych, format�w itp.
 *
 *  - name - nazwa,
 */
int ekg_hash(const char *name)
{
	int hash = 0;

	for (; *name; name++) {
		hash ^= *name;
		hash <<= 1;
	}

	return hash;
}
