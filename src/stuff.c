/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
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

/*
 * XXX TODO:
 * - escapowanie w put_log().
 */

#include "config.h"
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
#ifdef HAVE_ZLIB_H
#  include <zlib.h>
#endif
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
list_t lasts = NULL;
list_t lasts_count = NULL;
list_t conferences = NULL;

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
int config_beep = 1;
int config_beep_msg = 1;
int config_beep_chat = 1;
int config_beep_notify = 1;
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
int ioctld_sock = -1;
int config_ctrld_quits = 1;
int config_save_password = 1;
char *config_timestamp = NULL;
int config_display_sent = 0;
int config_sort_windows = 0;
int config_last_size = 10;
int config_last = 0;
int config_keep_reason = 0;
int config_enter_scrolls = 0;
int server_index = 0;

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
 * rozwija definicje makr (najczê¶ciej to bêd± emoticony)
 *
 * - s - string z makrami
 *
 * zwraca zaalokowany, rozwiniêty string.
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
			if (ns >= nn)
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
			if (ns >= n)
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
 * zwraca pe³n± ¶cie¿kê do podanego pliku katalogu ~/.gg/
 *
 *  - filename - nazwa pliku.
 *  - do_mkdir - czy tworzyæ katalog ~/.gg ?
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
 * put_log()
 *
 * wrzuca do logów informacjê od/do danego numerka. podaje siê go z tego
 * wzglêdu, ¿e gdy `log = 2', informacje lec± do $config_log_path/$uin.
 *
 *  - uin - numer delikwenta,
 *  - format... - akceptuje tylko %s, %d i %ld.
 */
void put_log(uin_t uin, const char *format, ...)
{
 	char *lp = config_log_path;
	char path[PATH_MAX], *buf;
	const char *p;
	int size = 0;
	va_list ap;
	FILE *f;

	if (!config_log)
		return;

	/* oblicz d³ugo¶æ tekstu */
	va_start(ap, format);
	for (p = format; *p; p++) {
		if (*p == '%') {
			p++;
			if (!*p)
				break;
			
			if (*p == 'l') {
				p++;
				if (!*p)
					break;
			}
			
			if (*p == 's') {
				char *tmp = va_arg(ap, char*);

				size += strlen(tmp);
			}
			
			if (*p == 'd') {
				int tmp = va_arg(ap, int);

				size += strlen(itoa(tmp));
			}
		} else
			size++;
	}
	va_end(ap);

	/* zaalokuj bufor */
	buf = xmalloc(size + 1);
	*buf = 0;

	/* utwórz tekst z logiem */
	va_start(ap, format);
	for (p = format; *p; p++) {
		if (*p == '%') {
			p++;
			if (!*p)
				break;
			if (*p == 'l') {
				p++;
				if (!*p)
					break;
			}

			if (*p == 's') {
				char *tmp = va_arg(ap, char*);

				strcat(buf, tmp);
			}

			if (*p == 'd') {
				int tmp = va_arg(ap, int);

				strcat(buf, itoa(tmp));
			}
		} else {
			buf[strlen(buf) + 1] = 0;
			buf[strlen(buf)] = *p;
		}
	}

	/* teraz skonstruuj ¶cie¿kê logów */

	if (!lp)
		lp = (config_log & 2) ? "." : "gg.log";

	if (*lp == '~')
		snprintf(path, sizeof(path), "%s%s", home_dir, lp + 1);
	else
		strncpy(path, lp, sizeof(path));

	if ((config_log & 2)) {
		if (mkdir(path, 0700) && errno != EEXIST)
			goto cleanup;
		snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%u", uin);
	}

#ifdef HAVE_ZLIB
	/* nawet je¶li chcemy gzipowane logi, a istnieje nieskompresowany log,
	 * olewamy kompresjê. je¶li loga nieskompresowanego nie ma, dodajemy
	 * rozszerzenie .gz i balujemy. */
	if (config_log & 4) {
		struct stat st;
		
		if (stat(path, &st) == -1) {
			gzFile f;

			snprintf(path + strlen(path), sizeof(path) - strlen(path), ".gz");

			if (!(f = gzopen(path, "a")))
				goto cleanup;

			gzputs(f, buf);
			gzclose(f);
			chmod(path, 0600);

			goto cleanup;
		}
	}
#endif

	if (!(f = fopen(path, "a")))
		goto cleanup;
	fputs(buf, f);
	fclose(f);
	chmod(path, 0600);

cleanup:
	xfree(buf);
}

/*
 * config_read()
 *
 * czyta z pliku ~/.gg/config lub podanego konfiguracjê i listê ignorowanych
 * u¿yszkodników.
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
 * w³a¶ciwa funkcja zapisuj±ca konfiguracjê do podanego pliku.
 *
 *  - f - plik, do którego piszemy,
 *  - base64 - czy kodowaæ ukryte pola?
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
					if (config_save_password)
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
 * funkcja zapisuj±ca awaryjnie konfiguracjê. nie powinna alokowaæ ¿adnej
 * pamiêci.
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
 * zapisuje aktualn± konfiguracjê -- zmienne i listê ignorowanych do pliku
 * ~/.gg/config lub podanego.
 *
 *  - filename.
 *  - base64 - czy kodowaæ zmienne?
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
 * zamienia krzaczki pisane w cp1250 na iso-8859-2, przy okazji maskuj±c
 * znaki, których nie da siê wy¶wietliæ, za wyj±tkiem \r i \n.
 *
 *  - buf.
 */
void cp_to_iso(unsigned char *buf)
{
	if (!buf)
		return;

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
	if (!buf)
		return;

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
 * wy¶wietla ostatnie n wypowiedzi w rozmowie z podan± osob±.
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
		print("history_error", "Brak historii dla wybranego u¿ytkownika");
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
 * odtwarza dzwiêk o podanej nazwie.
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
 * czyta linijkê tekstu z pliku alokuj±c przy tym odpowiedni buforek.
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
 * dopisuje do listy uruchomionych dzieci procesów.
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
 * usuwa proces z listy dzieciaków.
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
 * zwraca 1 je¶li tekst znaczy w³±czyæ, 0 je¶li wy³±czyæ, -1 je¶li co innego.
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
 * conference_set_ignore()
 *
 * Ustawia stan konferencji na ignorowany lub nie.
 *
 * - name - nazwa konferencji
 * - flag - 1 ignorowac, 0 nie ignorowac
 */
int conference_set_ignore(const char *name, int flag)
{
	struct conference *c = NULL;

	if (name[0] != '#') {
		print("conferences_name_error");
		return -1;
	}

	c = conference_find(name);

	if (!c) {
		print("conferences_noexist", name);
		return -1;
	}

	c->ignore = flag ? 1 : 0;
	print(flag ? "conferences_ignore" : "conferences_unignore", name);

	return 0;
}

/*
 * conference_rename()
 *
 * zmienia nazwê instniej±cej konferencji.
 * 
 *  - oldname - stara nazwa
 *  - newname - nowa nazwa
 */
int conference_rename(const char *oldname, const char *newname)
{
	struct conference *c;
	
	if (oldname[0] != '#' || newname[0] != '#') {
		print("conferences_name_error");
		return -1;
	}
	
	if (conference_find(newname)) {
		print("conferences_exist", newname);
		return -1;
	}

	if (!(c = conference_find(oldname))) {
		print("conference_noexist", oldname);
		return -1;
	}

	xfree(c->name);
	c->name = xstrdup(newname);
	remove_send_nick(oldname);
	add_send_nick(newname);

	print("conferences_rename", oldname, newname);

	ui_event("conference_rename", oldname, newname);
	
	return 0;
}

/*
 * conference_add()
 *
 * dopisuje konferencje do listy konferencji.
 *
 *  - name - nazwa konferencji,
 *  - nicklist - lista nicków, grup, czegokolwiek.
 *  - quiet - czy wypluwaæ mesgi na stdout.
 *
 * zaalokowan± struct conference lub NULL w przypadku b³êdu.
 */
struct conference *conference_add(const char *name, const char *nicklist, int quiet)
{
	struct conference c;
	char **nicks, **p;
	list_t l;
	int i, count;

	memset(&c, 0, sizeof(c));

	if (!name || !nicklist) {
		if (!quiet)
			print("not_enough_params", "conference");
		return NULL;
	}

	if (name[0] != '#') {
		if (!quiet) 
			print("conferences_name_error");
		return NULL;
	}

	nicks = array_make(nicklist, " ,", 0, 1, 0);

	/* grupy zamieniamy na niki */
	for (i = 0; nicks[i]; i++) {
		if (nicks[i][0] == '@') {
			char *gname = xstrdup(nicks[i] + 1);
			int first = 0;
			int nig = 0; /* nicks in group */

		        for (l = userlist; l; l = l->next) {
				struct userlist *u = l->data;
				list_t m;

				for (m = u->groups; m; m = m->next) {
					struct group *g = m->data;

					if (!strcasecmp(gname, g->name)) {
						if (first++)
							array_add(&nicks, xstrdup(u->display));
						else {
							xfree(nicks[i]);
							nicks[i] = xstrdup(u->display);
						}

						nig++;

						break;
					}
				}
			}

			if (!nig) {
				if (!quiet) {
					print("group_empty", gname);
					print("conferences_not_added", name);
				}
				xfree(gname);

				return NULL;
			}

			xfree(gname);
		}
	}

	count = array_count(nicks);

	for (l = conferences; l; l = l->next) {
		struct conference *cf = l->data;
		
		if (!strcasecmp(name, cf->name)) {
			if (!quiet)
				print("conferences_exist", name);

			array_free(nicks);

			return NULL;
		}
	}

	for (p = nicks, i = 0; *p; p++) {
		uin_t uin;

		if (!strcmp(*p, ""))
		        continue;

		if (!(uin = get_uin(*p))) {
			if (!quiet)
			        print("user_not_found", *p);
			continue;
		}


		list_add(&(c.recipients), &uin, sizeof(uin));
		i++;
	}

	array_free(nicks);

	if (i != count) {
		if (!quiet)
			print("conferences_not_added", name);

		return NULL;
	}

	if (!quiet)
		print("conferences_add", name);

	c.name = xstrdup(name);

	add_send_nick(name);

	return list_add(&conferences, &c, sizeof(c));
}

/*
 * conference_free()
 *
 * usuwa pamiêæ zajêt± przez konferencje.
 */
void conference_free()
{
	list_t l;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;
		
		xfree(c->name);
		list_destroy(c->recipients, 1);
	}

	list_destroy(conferences, 1);
	conferences = NULL;
}

/*
 * conference_remove()
 *
 * usuwa konferencje z listy konferencji.
 *
 * - name - konferencja.
 */
int conference_remove(const char *name)
{
	list_t l;

	if (!name) {
		print("not_enough_params", "conference");
		return -1;
	}

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;

		if (!strcasecmp(c->name, name)) {
			print("conferences_del", name);
			xfree(c->name);
			list_destroy(c->recipients, 1);
			list_remove(&conferences, c, 1);
			remove_send_nick(name);

			return 0;
		}
	}

	print("conferences_noexist", name);

	return -1;
}

/*
 * conference_create()
 *
 * Tworzy nowa konferencje z wygenerowana nazwa.
 *
 * - nicksstr - lista nikow tak jak dla polecenia conference.
 */
struct conference *conference_create(const char *nicks)
{
	struct conference *c;
	static int count = 1;
	char *name = saprintf("#conf%d", count++);

	c = conference_add(name, nicks, 0);

	xfree(name);

	return c;
}

/*
 * conference_find()
 *
 * znajduje i zwraca wska¼nik do konferencji lub NULL.
 *
 *  - name - nazwa konferencji.
 */
struct conference *conference_find(const char *name) 
{
	list_t l;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;

		if (!strcmp(c->name, name))
			return c;
	}
	
	return NULL;
}

/*
 * conference_find_by_uins()
 *
 * znajduje konferencjê, do której nale¿± podane uiny. je¿eli nie znaleziono,
 * zwracany jest NULL.
 * 
 * - from - kto jest nadawc± wiadomo¶ci,
 * - recipients - tablica numerów nale¿±cych do konferencji,
 * - count - ilo¶æ numerów.
 */
struct conference *conference_find_by_uins(uin_t from, uin_t *recipients, int count) 
{
	int i;
	list_t l, r;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;
		int matched = 0;

		for (r = c->recipients; r; r = r->next) {
			for (i = 0; i <= count; i++) {
				uin_t uin = (i == count) ? from : recipients[i];
				
				if (uin == *((uin_t *) (r->data))) {
					matched++;
					break;
				}
			}
		}

		if (matched == list_count(c->recipients)) 
			return l->data;
	}

	return NULL;
}

/*
 * alias_add()
 *
 * dopisuje alias do listy aliasów.
 *
 * - string - linia w formacie 'alias cmd'
 * - quiet - czy wypluwaæ mesgi na stdout.
 */
int alias_add(const char *string, int quiet, int append)
{
	char *cmd;
	list_t l;
	struct alias a;

	if (!string || !(cmd = strchr(string, ' '))) {
		if (!quiet)
			print("not_enough_params", "alias");
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

	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (!strcasecmp(string, c->name) && !c->alias) {
			print("aliases_command", string);
			return -1;
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
 * usuwa alias z listy aliasów.
 *
 * - name - alias.
 */
int alias_remove(const char *name)
{
	list_t l;

	if (!name) {
		print("not_enough_params", "alias");
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
 * sprawdza czy komenda w foo jest aliasem, je¶li tak - zwraca listê
 * komend, innaczej NULL.
 *
 *  - line - linia z komend±.
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
 * usuwa pamiêæ zajêt± przez aliasy.
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

#ifdef WITH_IOCTLD

/*
 * ioctld_parse_seq()
 *
 * zamieñ string na odpowiedni± strukturê dla ioctld.
 *
 *  - seq.
 *  - data.
 *
 * 0/-1.
 */
int ioctld_parse_seq(const char *seq, struct action_data *data)
{
        char tmp_buff[16] = "";
        int i = 0, a, l = 0, default_delay = 10000;

        if (!data || !seq || !isdigit(seq[0]))
                return -1;

        for (a = 0; a <= strlen(seq) && a < MAX_ITEMS; a++) {
                if (i > 15)
			return -1;
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
			return -1;
        }

	return 0;
}

/*
 * ioctld_socket()
 *
 * inicjuje gniazdo dla ioctld.
 *
 * - path - ¶cie¿ka do gniazda.
 *
 * 0/-1.
 */
int ioctld_socket(char *path)
{
	struct sockaddr_un sun;

	if (ioctld_sock != -1)
		close(ioctld_sock);

	if ((ioctld_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
		return -1;

	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, path);

	if (connect(ioctld_sock, (struct sockaddr*) &sun, sizeof(sun)) == -1)
		return -1;

        return 0;
}

/*
 * ioctld_send()
 *
 * wysy³a do ioctld polecenie uruchomienia danej akcji.
 *
 * - seq - sekwencja danych
 * - act - rodzaj akcji
 *
 * 0/-1.
 */
int ioctld_send(const char *seq, int act)
{
	const char *s;
	struct action_data data;

	if (*seq == '$') {
		seq++;
		s = format_find(seq);
		if (!strcmp(s, "")) {
			print("events_seq_not_found", seq);
			return -1;
		}
	} else
		s = seq;

	data.act = act;

	if (ioctld_parse_seq(s, &data))
		return -1;

	return send(ioctld_sock, &data, sizeof(data), 0);
}

#endif /* WITH_IOCTLD */

/*
 * event_format()
 *
 * zwraca ³añcuch zdarzeñ w oparciu o flagi. statyczny bufor.
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
 * zwraca flagi na podstawie ³añcucha.
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
 * Dodaje zdarzenie do listy zdarzeñ.
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
 * usuwa zdarzenie z listy zdarzeñ.
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
 * event_run()
 *
 * wykonuje dan± akcjê.
 *
 * - act - tre¶æ akcji.
 */
static int event_run(const char *act)
{
        char *action, *ptr, **acts;

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
		if (ioctld_send(acts[1], ACT_BLINK_LEDS) == -1)
			goto fail;
		goto cleanup;
	}

        if (!strncasecmp(acts[0], "beeps", 5) || !strncasecmp(acts[0], "sound", 5)) {
		gg_debug(GG_DEBUG_MISC, "//   making sounds\n");
		if (ioctld_send(acts[1], ACT_BEEPS_SPK) == -1)
			goto fail;
		goto cleanup;
	}
#endif
 	
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
		char *tmp;

		gg_debug(GG_DEBUG_MISC, "//   chatting/mesging\n");

		tmp = xstrdup(action);
		command_exec(NULL, tmp);
		xfree(tmp);
		
		goto cleanup;
        }

        if (!strcasecmp(acts[0], "beep")) {
                gg_debug(GG_DEBUG_MISC, "//   beeping\n");
		ui_beep();
		goto cleanup;
        }

	gg_debug(GG_DEBUG_MISC, "//   unknown action\n");

cleanup:
	free(action);
	array_free(acts);
        return 0;

	goto fail;	/* ¿eby gcc nie wyrzuca³ warningów */

fail:
	free(action);
	array_free(acts);
        return 1;
}

/*
 * event_check()
 *
 * sprawdza i ewentualnie uruchamia akcjê na podane zdarzenie.
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
                if (!strncasecmp(acts[0], "blink", 5) || !strncasecmp(acts[0], "beeps", 5) || !strncasecmp(acts[0], "sound", 5)) {
        		struct action_data test;

                        if (!acts[1]) {
                                print("events_act_no_params", acts[0]);
				goto fail;
			}

                        if (*acts[1] == '$') {
                                if (!strcmp(format_find(acts[1] + 1), "")) {
                                        print("events_seq_not_found", acts[1] + 1);
					goto fail;
				}
                        } else if (ioctld_parse_seq(acts[1], &test)) {
                                print("events_seq_incorrect", acts[1]);
				goto fail;
                        }

			goto check;
                }
#endif /* WITH_IOCTLD */

		if (!strncasecmp(acts[0], "play", 4) || !strcasecmp(acts[0], "exec") || !strcasecmp(acts[0], "command")) {
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

	goto fail;	/* ¿eby gcc nie wyrzuca³ ostrze¿eñ */

fail:
	array_free(events);
	array_free(acts);
        return 1;
}

/*
 * event_free()
 *
 * zwalnia pamiêæ zwi±zan± ze zdarzeniami.
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
 * inicjuje potok nazwany do zewnêtrznej kontroli ekg
 *
 * - pipe_file
 *
 * zwraca deskryptor otwartego potoku lub warto¶æ b³êdu
 */
int init_control_pipe(const char *pipe_file)
{
	int fd;

	if (!pipe_file)
		return 0;
	if (mkfifo(pipe_file, 0600) < 0 && errno != EEXIST) {
		fprintf(stderr, "Nie mogê stworzyæ potoku %s: %s. Ignorujê.\n", pipe_file, strerror(errno));
		return -1;
	}
	if ((fd = open(pipe_file, O_RDWR | O_NDELAY)) < 0) {
		fprintf(stderr, "Nie mogê otworzyæ potoku %s: %s. Ignorujê.\n", pipe_file, strerror(errno));
		return -1;
	}
	return fd;
}

static char base64_charset[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * base64_encode()
 *
 * zapisuje ci±g znaków w base64. alokuje pamiêæ. 
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
 * wczytuje ci±g znaków base64, zwraca zaalokowany buforek.
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
/*		*buf = 0;	XXX kto mi powie po co to by³o dostaje piwo */
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
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,dcc''.
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
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,theme''.
 */
void changed_theme(const char *var)
{
	if (!config_theme) {
		theme_init();
		ui_event("theme_init");
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
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,proxy''.
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
 * changed_uin()
 *
 * funkcja wywo³ywana przy zmianie zmiennej uin.
 */
void changed_uin(const char *var)
{
	ui_event("xterm_update");
}

/*
 * changed_xxx_reason()
 *
 * funkcja wywo³ywana przy zmianie domy¶lnych powodów.
 */
void changed_xxx_reason(const char *var)
{
	char *tmp = NULL;

	if (!strcmp(var, "away_reason"))
		tmp = config_away_reason;
	if (!strcmp(var, "back_reason"))
		tmp = config_back_reason;
	if (!strcmp(var, "quit_reason"))
		tmp = config_quit_reason;

	if (!tmp)
		return;

	if (strlen(tmp) > GG_STATUS_DESCR_MAXSIZE)
		print("descr_too_long", itoa(strlen(tmp) - GG_STATUS_DESCR_MAXSIZE));
}

/*
 * do_connect()
 *
 * przygotowuje wszystko pod po³±czenie gg_login i ³±czy siê.
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
	p.status_descr = config_reason;
	p.async = 1;
#ifdef HAVE_VOIP
	p.has_audio = 1;
#endif
	p.protocol_version = config_protocol;
	p.last_sysmsg = last_sysmsg;

	if (config_server) {
		char *server, **servers = array_make(config_server, ",; ", 0, 1, 0);

		if (server_index >= array_count(servers))
			server_index = 0;

		if ((server = xstrdup(servers[server_index++]))) {
			char *tmp = strchr(server, ':');
			
			if (tmp) {
				p.server_port = atoi(tmp + 1);
				*tmp = 0;
				p.server_addr = inet_addr(server);
				gg_debug(GG_DEBUG_MISC, "-- server_addr=%s, server_port=%d\n", server, p.server_port);
			} else {
				p.server_port = GG_DEFAULT_PORT;
				p.server_addr = inet_addr(server);
				gg_debug(GG_DEBUG_MISC, "-- server_addr=%s, server_port=%d\n", server, p.server_port);
			}

			xfree(server);
		}

		array_free(servers);
	}

	if (!(sess = gg_login(&p))) {
		print("conn_failed", format_find((errno == ENOMEM) ? "conn_failed_memory" : "conn_failed_connecting"));
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
 * roz³±cza siê, zmieniaj±c uprzednio stan na niedostêpny z opisem.
 *
 *  - sess - opis sesji,
 *  - reason - powód, mo¿e byæ NULL.
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

	update_status();
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
 *  - value - warto¶æ,
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
 * ³aduje do listy wszystkie makra z pliku ~/.gg/emoticons
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
 * usuwa pamiêæ zajêt± przez emoticony.
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
 * zmiennych, formatów itp.
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

/*
 * timer_add()
 *
 * dodaje timera.
 *
 *  - period - za jaki czas w sekundach ma byæ uruchomiony,
 *  - name - nazwa timera w celach identyfikacji. je¶li jest równa NULL,
 *           zostanie przyznany pierwszy numerek z brzegu.
 *  - command - komenda wywo³ywana po up³yniêciu czasu.
 *
 * zwraca zaalokowan± struct timer.
 */
struct timer *timer_add(int period, const char *name, const char *command)
{
	struct timer t;

	if (!name) {
		int i;

		for (i = 1; ; i++) {
			int gotit = 0;
			list_t l;

			for (l = timers; l; l = l->next) {
				struct timer *tt = l->data;

				if (!strcmp(tt->name, itoa(i))) {
					gotit = 1;
					break;
				}
			}

			if (!gotit)
				break;
		}

		name = itoa(i);
	}

	memset(&t, 0, sizeof(t));
	t.started = time(NULL);
	t.period = period;
	t.name = xstrdup(name);
	t.command = xstrdup(command);

	return list_add(&timers, &t, sizeof(t));
}

/*
 * timer_remove()
 *
 * usuwa timer.
 *
 *  - name - nazwa timera, mo¿e byæ NULL.
 *  - command - komenda timera, mo¿e byæ NULL.
 *
 * 0/-1.
 */
int timer_remove(const char *name, const char *command)
{
	list_t l;
	int removed = 0;

	for (l = timers; l; ) {
		struct timer *t = l->data;

		l = l->next;

		if ((name && !strcmp(name, t->name)) || (command && !strcmp(command, t->command))) {
			xfree(t->name);
			xfree(t->command);
			list_remove(&timers, t, 1);
			removed = 1;
		}
	}

	return (removed) ? 0 : -1;
}

/*
 * timer_free()
 *
 * zwalnia pamiêæ po timerach.
 */
void timer_free()
{
	list_t l;

	for (l = timers; l; l = l->next) {
		struct timer *t = l->data;
		
		xfree(t->name);
		xfree(t->command);
	}

	list_destroy(timers, 1);
}

/*
 * log_escape()
 *
 * je¶li trzeba, eskejpuje tekst do logów.
 * 
 *  - str - tekst.
 *
 * zaalokowany bufor.
 */
char *log_escape(const char *str)
{
	const char *p;
	char *res, *q;
	int size, needto = 0;

	if (!str)
		return NULL;
	
	for (p = str; *p; p++) {
		if (*p == '"' || *p == '\'' || *p == '\r' || *p == '\n' || *p == ',')
			needto = 1;
	}

	if (!needto)
		return xstrdup(str);

	for (p = str, size = 0; *p; p++) {
		if (*p == '"' || *p == '\'' || *p == '\r' || *p == '\n' || *p == '\\')
			size += 2;
		else
			size++;
	}

	q = res = xmalloc(size + 3);
	
	*q++ = '"';
	
	for (p = str; *p; p++, q++) {
		if (*p == '\\' || *p == '"' || *p == '\'') {
			*q++ = '\\';
			*q = *p;
		} else if (*p == '\n') {
			*q++ = '\\';
			*q = 'n';
		} else if (*p == '\r') {
			*q++ = '\\';
			*q = 'r';
		} else
			*q = *p;
	}
	*q++ = '"';
	*q = 0;

	return res;
}

/*
 * last_add()
 *
 * dodaje wiadomo¶æ do listy ostatnio otrzymanych.
 * 
 *  - type - rodzaj wiadomo¶ci,
 *  - uin - nadawca,
 *  - t - czas,
 *  - msg - tre¶æ wiadomo¶ci,
 */
void last_add(unsigned int type, uin_t uin, time_t t, const char *msg)
{
	list_t l;
	struct last ll;
	int last_count = 0;

	/* nic nie zapisujemy, je¿eli user sam nie wie czego chce. */
	if (config_last_size <= 0)
		return;
	
	if (config_last & 2) 
		last_count = last_count_get(uin);
	else
		last_count = list_count(lasts);
				
	/* usuwamy ostatni± wiadomo¶æ, w razie potrzeby... */
	if (last_count >= config_last_size) {
		time_t tmp_time = 0;
		
		/* najpierw j± znajdziemy... */
		for (l = lasts; l; l = l->next) {
			struct last *lll = l->data;

			if (config_last & 2 && lll->uin != uin)
				continue;

			if (!tmp_time)
				tmp_time = lll->time;
			
			if (lll->time <= tmp_time)
				tmp_time = lll->time;
		}
		
		/* ...by teraz usun±æ */
		for (l = lasts; l; l = l->next) {
			struct last *lll = l->data;

			if (lll->time == tmp_time && lll->uin == uin) {
				xfree(lll->message);
				list_remove(&lasts, lll, 1);
				last_count_del(uin);
				break;
			}
		}

	}

	ll.type = type;
	ll.uin = uin;
	ll.time = t;
	ll.message = xstrdup(msg);
	
	list_add(&lasts, &ll, sizeof(ll));
	last_count_add(uin);

	return;
}

/*
 * last_count_add()
 *
 * XXX
 */
void last_count_add(uin_t uin)
{
	list_t l;
	int add = 0;

	for (l = lasts_count; l; l = l->next) {
		struct last_count *lc = l->data;

		if (lc->uin == uin) {
			lc->count++;
			add++;
		}
	}

	if (!add) {
		struct last_count lc;

		lc.uin = uin;
		lc.count = 1;

		list_add(&lasts_count, &lc, sizeof(lc));
	}
}

/*
 * last_count_del()
 *
 * XXX
 */
void last_count_del(uin_t uin) 
{
	list_t l;

	for (l = lasts_count; l; l = l->next) {
		struct last_count *lc = l->data;

		if (lc->uin == uin) { 
			lc->count--;
			
			if (lc->count <= 0)
				list_remove(&lasts_count, lc, 1);
		}
	}
}

/*
 * last_count_get()
 *
 * XXX
 */
int last_count_get(uin_t uin) 
{
	int last_count = 0;
	list_t l;

	for (l = lasts_count; l; l = l->next) {
		struct last_count *lc = l->data;
		
		if (lc->uin == uin) {
			last_count = lc->count;
			break;
		}
	}

	return last_count;
}

/* 
 * xstrmid()
 *
 * wycina fragment tekstu alokuj±c dla niego pamiêæ.
 *
 *  - str - tekst ¼ród³owy,
 *  - start - pierwszy znak,
 *  - length - d³ugo¶æ wycinanego tekstu, je¶li -1 
 */
char *xstrmid(const char *str, int start, int length)
{
	char *res, *q;
	const char *p;

	if (!str)
		return xstrdup("");

	if (length == -1)
		length = strlen(str) - start;

	if (length < 1)
		return xstrdup("");

	if (length > strlen(str) - start)
		length = strlen(str) - start;
	
	res = xmalloc(length + 1);
	
	for (p = str + start, q = res; length; p++, q++, length--)
		*q = *p;

	*q = 0;

	return res;
}

/*
 * http_error_string()
 *
 * zwraca tekst opisuj±cy powód b³êdu us³ug po http.
 *
 *  - h - warto¶æ gg_http->error lub 0, gdy funkcja zwróci³a NULL.
 */
const char *http_error_string(int h)
{
	switch (h) {
		case 0:
			return format_find((errno == ENOMEM) ? "http_failed_memory" : "http_failed_connecting");
		case GG_ERROR_RESOLVING:
			return format_find("http_failed_resolving");
		case GG_ERROR_CONNECTING:
			return format_find("http_failed_connecting");
		case GG_ERROR_READING:
			return format_find("http_failed_reading");
		case GG_ERROR_WRITING:
			return format_find("http_failed_writing");
	}

	return "?";
}

/*
 * update_status()
 *
 * uaktualnia w³asny stan w li¶cie kontaktów, je¶li dopisali¶my siebie.
 */
void update_status()
{
	int st[6] = { GG_STATUS_AVAIL, GG_STATUS_BUSY, GG_STATUS_INVISIBLE, GG_STATUS_BUSY_DESCR, GG_STATUS_AVAIL_DESCR, GG_STATUS_INVISIBLE_DESCR };
	struct userlist *u = userlist_find(config_uin, NULL);

	if (!u)
		return;

	xfree(u->descr);
	u->descr = xstrdup(config_reason);
	
	if (!sess || sess->state != GG_STATE_CONNECTED)
		u->status = (u->descr) ? GG_STATUS_NOT_AVAIL_DESCR : GG_STATUS_NOT_AVAIL;
	else
		u->status = st[away];
}

