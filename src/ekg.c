/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo�ny <speedy@ziew.org>
 *                          Pawe� Maziarz <drg@infomex.pl>
 *                          Adam Osuchowski <adwol@polsl.gliwice.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Wojciech Bojdo� <wojboj@htcon.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#include "config.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "commands.h"
#include "configfile.h"
#include "emoticons.h"
#include "events.h"
#include "libgadu.h"
#include "log.h"
#include "mail.h"
#include "msgqueue.h"
#ifdef WITH_PYTHON
#  include "python.h"
#endif
#ifdef HAVE_OPENSSL
#  include "simlite.h"
#endif
#ifndef HAVE_STRLCAT
#  include "../compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "../compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "userlist.h"
#include "vars.h"
#include "version.h"
#include "xmalloc.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

static pid_t ekg_pid = 0;
static char argv0[PATH_MAX];
static int ioctld_pid = 0;

time_t last_action = 0;
char *pipe_file = NULL;

pid_t speech_pid = 0;

static void get_line_from_pipe(struct gg_exec *c);
static int get_char_from_pipe(struct gg_common *c);

int old_stderr = 0;

/*
 * usuwanie sesji GG_SESSION_USERx. wystarczy zwolni�.
 */
static void reaper_user(void *foo)
{
	xfree(foo);
}

/*
 * usuwanie sesji GG_SESSION_USER3. trzeba wcze�niej zwolni� pola sesji.
 */
static void reaper_user3(struct gg_exec *e)
{
	if (e->buf)
		string_free(e->buf, 1);

	if (e->target)
		xfree(e->target);

	xfree(e);
}

/*
 * struktura zawieraj�ca adresy funkcji obs�uguj�cych r�ne sesje
 * i zwalniaj�cych pami�� po nich.
 */
static struct {
	int type;
	void (*handler)(void*);
	void (*reaper)(void*);
} handlers[] = {

#define EKG_HANDLER(x, y, z) { x, (void(*)(void*)) y, (void(*)(void*)) z },

	EKG_HANDLER(GG_SESSION_GG, handle_event, gg_free_session)
	EKG_HANDLER(GG_SESSION_DCC, handle_dcc, gg_dcc_free)
	EKG_HANDLER(GG_SESSION_DCC_SOCKET, handle_dcc, gg_dcc_free)
	EKG_HANDLER(GG_SESSION_DCC_SEND, handle_dcc, gg_dcc_free)
	EKG_HANDLER(GG_SESSION_DCC_GET, handle_dcc, gg_dcc_free)
	EKG_HANDLER(GG_SESSION_DCC_VOICE, handle_dcc, gg_dcc_free)
	EKG_HANDLER(GG_SESSION_REGISTER, handle_pubdir, gg_register_free)
	EKG_HANDLER(GG_SESSION_UNREGISTER, handle_pubdir, gg_pubdir_free)
	EKG_HANDLER(GG_SESSION_PASSWD, handle_pubdir, gg_change_passwd_free)
	EKG_HANDLER(GG_SESSION_REMIND, handle_pubdir, gg_remind_passwd_free)
	EKG_HANDLER(GG_SESSION_TOKEN, handle_token, gg_token_free)
	EKG_HANDLER(GG_SESSION_USER0, NULL, reaper_user)		/* stdin */
	EKG_HANDLER(GG_SESSION_USER1, get_char_from_pipe, reaper_user)	/* control pipe */
	EKG_HANDLER(GG_SESSION_USER2, handle_voice, reaper_user)	/* voice */
	EKG_HANDLER(GG_SESSION_USER3, get_line_from_pipe, reaper_user3)	/* exec, stderr */
	EKG_HANDLER(GG_SESSION_USER4, get_line_from_pipe, reaper_user3)	/* mail */

#undef EKG_HANDLER

	{ -1, NULL, NULL }
};

/*
 * get_char_from_pipe()
 *
 * funkcja pobiera z potoku steruj�cego znak do bufora, a gdy si� zape�ni
 * bufor wykonuje go tak jakby tekst w buforze wpisany by� z terminala.
 *
 * - c - struktura steruj�ca przechowuj�ca m.in. deskryptor potoku.
 *
 * 0/-1
 */
static int get_char_from_pipe(struct gg_common *c)
{
	static char buf[2048];
	static int escaped;
	char ch;
  
	if (!c)
  		return -1;

	if (read(c->fd, &ch, 1) == -1)
		return -1;
	
	if (ch != '\n' && ch != '\r') {
		if (strlen(buf) < sizeof(buf) - 1)
			buf[strlen(buf)] = ch;
	}

	if (ch == '\n' && escaped) {	/* zamazuje \\ */
		buf[strlen(buf) - 1] = '\r';
		buf[strlen(buf) - 1] = '\n';
	}

	if ((ch == '\n' && !escaped) || (strlen(buf) >= sizeof(buf) - 1)) {
		command_exec(NULL, buf, 0);
		memset(buf, 0, sizeof(buf));
	}

	if (ch == '\\') {
		escaped = 1;
	} else if (ch != '\r' && ch != '\n') {
		escaped = 0;
	}

	return 0;
}

/*
 * get_line_from_pipe()
 *
 * funkcja pobiera z potoku steruj�cego znak do bufora, a gdy dojdzie
 * do konca linii puszcza na ekran.
 *
 * - c - struktura steruj�ca przechowuj�ca m.in. deskryptor potoku.
 */
static void get_line_from_pipe(struct gg_exec *c)
{
	char buf[8192];
	int ret;

	if (!c)
		return;

	if ((ret = read(c->fd, buf, sizeof(buf) - 1)) != 0 && ret != -1) {
		char *tmp, *tab;

		buf[ret] = 0;
		string_append(c->buf, buf);

		while ((tab = strchr(c->buf->str, '\t'))) {
			int count;
			char *last_n = tab;
			
			*tab = ' ';

			while (*last_n) {
				if (*last_n == '\n')
					break;
				else
					last_n--;
			}

			count = 8 - ((int) (tab - last_n)) % 8;

			if (count > 1)
				string_insert_n(c->buf, (tab - c->buf->str), "        ", count - 1);
		}

		while ((tmp = strchr(c->buf->str, '\n'))) {
			int index = tmp - c->buf->str;
			char *line = xstrmid(c->buf->str, 0, index);
			string_t new;
			
			if (strlen(line) > 1 && line[strlen(line) - 1] == '\r')
				line[strlen(line) - 1] = 0;

			if (c->type == GG_SESSION_USER4)
				check_mail_update(line, 1);
			else if (!c->quiet) {
				switch (c->msg) {
					case 0:
						print_window(c->target, 0, "exec", line, itoa(c->id));
						break;
					case 1:
					{
						char *tmp = saprintf("/chat \"%s\" %s", c->target, line);
						command_exec(NULL, tmp, 0);
						xfree(tmp);
						break;
					}
					case 2:
						buffer_add(BUFFER_EXEC, c->target, line, 0);
						break;
				}
			}

			new = string_init(c->buf->str + index + 1);
			string_free(c->buf, 1);
			c->buf = new;
			xfree(line);
		}
	}

	if ((ret == -1 && errno != EAGAIN) || ret == 0) {
		if (c->buf->len) {
			if (c->type == GG_SESSION_USER4)
				check_mail_update(c->buf->str, 0);
			else if (!c->quiet) {
				switch (c->msg) {
					case 0:
						print_window(c->target, 0, "exec", c->buf->str, itoa(c->id));
						break;
					case 1:
					{
						char *tmp = saprintf("/chat \"%s\" %s", c->target, c->buf->str);
						command_exec(NULL, tmp, 0);
						xfree(tmp);
						break;
					}
					case 2:
						buffer_add(BUFFER_EXEC, c->target, c->buf->str, 0);
						break;
				}
			}
		}

		if (!c->quiet && c->msg == 2) {
			char *out = buffer_flush(BUFFER_EXEC, c->target);
			char *tmp = saprintf("/chat \"%s\" %s", c->target, out);

			command_exec(NULL, tmp, 0);
			
			xfree(out);
			xfree(tmp);
		}
		
		close(c->fd);
		xfree(c->target);
		string_free(c->buf, 1);
		list_remove(&watches, c, 1);
	}
}

/*
 * ekg_wait_for_key()
 *
 * funkcja wywo�ywana przez interfejsy u�ytkownika do przetwarzania danych
 * z sieci, gdy czeka si� na reakcj� u�ytkownika. obs�uguje timery,
 * timeouty i wszystko, co ma si� dzia� w tle.
 */
void ekg_wait_for_key()
{
	static time_t last_ping = 0;
	struct timeval tv;
	list_t l, m;
	fd_set rd, wd;
	int ret, maxfd, pid, status;
#ifdef WITH_WAP
	static int wap_userlist_timer = 0;
#endif

	for (;;) {
		/* przejrzyj timery u�ytkownika, ui, skrypt�w */
		for (l = timers; l; ) {
			struct timer *t = l->data;
			struct timeval tv;
			struct timezone tz;

			l = l->next;

			gettimeofday(&tv, &tz);

			if (tv.tv_sec > t->ends.tv_sec || (tv.tv_sec == t->ends.tv_sec && tv.tv_usec >= t->ends.tv_usec)) {
				char *command = xstrdup(t->command), *id = xstrdup(t->id);
				int type = t->type;

				if (!t->persistent) {
					xfree(t->name);
					xfree(t->command);
					xfree(t->id);

					list_remove(&timers, t, 1);
				} else {
					struct timeval tv;
					struct timezone tz;

					gettimeofday(&tv, &tz);
					tv.tv_sec += t->period;
					memcpy(&t->ends, &tv, sizeof(tv));
				}

				switch (type) {
					case TIMER_SCRIPT:
#ifdef WITH_PYTHON
						python_function(command, id);
#endif
						break;
					case TIMER_UI:
						ui_event(command, NULL);
						break;
					default:
						command_exec(NULL, command, 0);
				}

				xfree(command);
				xfree(id);
			}
		}

		/* sprawd� timeouty r�nych sesji */
		for (l = watches; l; l = l->next) {
			struct gg_session *s = l->data;
			struct gg_common *c = l->data;
			struct gg_http *h = l->data;
			struct gg_dcc *d = l->data;
			static time_t last_check = 0;

			if (!c || c->timeout == -1 || time(NULL) == last_check)
				continue;

			last_check = time(NULL);

			c->timeout--;

			if (c->timeout > 0)
				continue;
			
			switch (c->type) {
				case GG_SESSION_GG:
					if (c->state == GG_STATE_CONNECTING_GG) {
						/* w przypadku timeoutu nie
						 * wyrzucamy po��czenia z listy
						 * tylko ka�emy mu stwierdzi�
						 * b��d i po��czy� si� z
						 * kolejnym kandydatem. */
						handle_event((struct gg_session*) c);
					} else {
						print("conn_timeout");
						list_remove(&watches, s, 0);
						gg_logoff(s);
						gg_free_session(s);
						userlist_clear_status(0);
						sess = NULL;
						ekg_reconnect();
					}
					break;

				case GG_SESSION_REGISTER:
					print("register_timeout");
					list_remove(&watches, h, 0);
					gg_free_pubdir(h);

					xfree(reg_password);
					reg_password = NULL;
					xfree(reg_email);
					reg_email = NULL;

					break;

				case GG_SESSION_UNREGISTER:
					print("unregister_timeout");
					list_remove(&watches, h, 0);
					gg_free_pubdir(h);
					break;

				case GG_SESSION_PASSWD:
					print("passwd_timeout");
					list_remove(&watches, h, 0);
					gg_free_pubdir(h);

					xfree(reg_password);
					reg_password = NULL;
					xfree(reg_email);
					reg_email = NULL;

					break;

				case GG_SESSION_REMIND:
					print("remind_timeout");
					list_remove(&watches, h, 0);
					gg_free_pubdir(h);
					break;

				case GG_SESSION_DCC:
				case GG_SESSION_DCC_GET:
				case GG_SESSION_DCC_SEND:
				{
					struct in_addr addr;
					unsigned short port = d->remote_port;
					char *tmp;
			
					addr.s_addr = d->remote_addr;

					if (d->peer_uin) {
						struct userlist *u = userlist_find(d->peer_uin, NULL);
						if (!addr.s_addr && u) {
							addr.s_addr = u->ip.s_addr;
							port = u->port;
						}
						tmp = saprintf("%s (%s:%d)", xstrdup(format_user(d->peer_uin)), inet_ntoa(addr), port);
					} else 
						tmp = saprintf("%s:%d", inet_ntoa(addr), port);
					print("dcc_timeout", tmp);
					xfree(tmp);
					remove_transfer(d);
					list_remove(&watches, d, 0);
					gg_free_dcc(d);
					break;
				}
			}

			break;
		}
		
		/* timeout reconnectu */
		if (!sess && reconnect_timer && time(NULL) - reconnect_timer >= config_auto_reconnect && config_uin && config_password) {
			reconnect_timer = 0;
			print("connecting");
			connecting = 1;
			ekg_connect();
		}

		/* timeout pinga */
		if (sess && sess->state == GG_STATE_CONNECTED && time(NULL) - last_ping > 60) {
			if (last_ping)
				gg_ping(sess);
			last_ping = time(NULL);
		}

		/* timeout autoawaya */
		if (config_auto_away && GG_S_A(config_status) && time(NULL) - last_action > config_auto_away && sess && sess->state == GG_STATE_CONNECTED) {
			change_status(GG_STATUS_BUSY, NULL, config_auto_away);
			in_auto_away = 1;
		}

		/* auto save */
		if (config_auto_save && config_changed && time(NULL) - last_save > config_auto_save) {
			gg_debug(GG_DEBUG_MISC, "-- autosaving userlist and config after %d seconds.\n", time(NULL) - last_save);
			last_save = time(NULL);

			if (!userlist_write(NULL) && !config_write(NULL)) {
				config_changed = 0;
				print("autosaved");
			} else
				print("error_saving");
		}
#ifdef WITH_WAP
		/* co jaki� czas zrzu� userlist� dla frontendu wap */
		if (!wap_userlist_timer)
			wap_userlist_timer = time(NULL);

		if (wap_userlist_timer + 60 > time(NULL)) {
			userlist_write_wap();
			wap_userlist_timer = time(NULL);
		}
#endif

		/* dostali�my sygna�, wracamy do ui */
		if (ui_resize_term)
			break;

		/* zerknij na wszystkie niezb�dne deskryptory */
		
		FD_ZERO(&rd);
		FD_ZERO(&wd);

		for (maxfd = 0, l = watches; l; l = l->next) {
			struct gg_common *w = l->data;

			if (!w || w->state == GG_STATE_ERROR || w->state == GG_STATE_IDLE || w->state == GG_STATE_DONE)
				continue;
			
			if (w->fd > maxfd)
				maxfd = w->fd;
			if ((w->check & GG_CHECK_READ))
				FD_SET(w->fd, &rd);
			if ((w->check & GG_CHECK_WRITE))
				FD_SET(w->fd, &wd);
		}

		/* domy�lny timeout to 1s */
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/* ale je�li kt�ry� timer ma wyst�pi� wcze�niej ni� za sekund�
		 * to skr��my odpowiednio czas oczekiwania */
		
		for (l = timers; l; l = l->next) {
			struct timer *t = l->data;
			struct timeval tv2;
			struct timezone tz;
			int usec = 0;

			gettimeofday(&tv2, &tz);

			/* �eby unikn�� przekr�cenia licznika mikrosekund przy
			 * wi�kszych czasach, pomijamy d�ugie timery */

			if (t->ends.tv_sec - tv2.tv_sec > 5)
				continue;
			
			/* zobacz, ile zosta�o do wywo�ania timera */

			usec = (t->ends.tv_sec - tv2.tv_sec) * 1000000 + (t->ends.tv_usec - tv2.tv_usec);

			/* je�li wi�cej ni� sekunda, to nie ma znacznia */
			
			if (usec >= 1000000)
				continue;

			/* je�li mniej ni� aktualny timeout, zmniejsz */

			if (tv.tv_sec * 1000000 + tv.tv_usec > usec) {
				tv.tv_sec = 0;
				tv.tv_usec = usec;
			}
		}

		/* na wszelki wypadek sprawd� warto�ci */
		
		if (tv.tv_sec < 0)
			tv.tv_sec = 0;

		if (tv.tv_usec < 0)
			tv.tv_usec = 0;

		/* sprawd�, co si� dzieje */

		ret = select(maxfd + 1, &rd, &wd, NULL, &tv);
	
		/* je�li wyst�pi� b��d, daj zna� */

		if (ret == -1) {
			if (errno != EINTR)
				perror("select()");
			continue;
		}

		/* nic si� nie sta�o? je�li to tryb wsadowy i zrobili�my,
		 * co mieli�my zrobi�, wyjd�. */
		
		if (!ret) {
			if (batch_mode && !batch_line)
				break;

			continue;
		}

		/* przejrzyj deskryptory */

		for (l = watches; l; l = l->next) {
			struct gg_common *c = l->data;
			int i;

			if (!c || (!FD_ISSET(c->fd, &rd) && !FD_ISSET(c->fd, &wd)))
				continue;

			if (c->type == GG_SESSION_USER0) {
				if (config_auto_back == 2 && GG_S_B(config_status) && in_auto_away) {
					change_status(GG_STATUS_AVAIL, NULL, 1);
					in_auto_away = 0;
				}

				if (config_auto_back == 2)
					unidle();

				return;
			}

			/* obs�ugujemy poza list� handler�w, poniewa� mo�e
			 * zwr�ci� b��d w przypadku b��du. wtedy grzecznie
			 * usuwamy z listy deskryptor�w. */
			if (c->type == GG_SESSION_USER1) {
				if (get_char_from_pipe(c))
					list_remove(&watches, c, 1);
			}

			for (i = 0; handlers[i].type != -1; i++)
				if (c->type == handlers[i].type && handlers[i].handler) {
					(handlers[i].handler)(c);
					break;
				}

			if (handlers[i].type == -1) {
				list_remove(&watches, c, 1);
				break;
			}
			
			break;
		}

		/* przegl�danie zdech�ych dzieciak�w */
		while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			for (l = children; l; l = m) {
				struct process *p = l->data;

				m = l->next;

				if (pid != p->pid)
					continue;

				if (pid == speech_pid) {
					speech_pid = 0;

					if (!config_speech_app)
						xfree(buffer_flush(BUFFER_SPEECH, NULL));

					if (buffer_count(BUFFER_SPEECH) && !(WEXITSTATUS(status))) {
						char *str = buffer_tail(BUFFER_SPEECH);
						say_it(str);
						xfree(str);
					}
				}

				switch (p->name[0]) {
					case '\001':
						print((!(WEXITSTATUS(status))) ? "sms_sent" : "sms_failed", p->name + 1);
					
						xfree(p->name);
						list_remove(&children, p, 1);
						break;
					default:
						p->died = 1;
						break;
				}
			}
		}

		for (l = children; l; l = m) {
			struct process *p = l->data;

			m = l->next;

			if (p->died) {
				int left = 0;
#ifdef FIONREAD
				list_t l;
				int fd = -1;

				for (l = watches; l; l = l->next) {
					struct gg_common *c = l->data;

					if (c->type == GG_SESSION_USER3 && c->id == p->pid) {
						fd = c->fd;
						break;
					}
				}

				if (fd > 0)
					ioctl(fd, FIONREAD, &left);
#endif
			
				if (!left) {
					if (p->name[0] != '\002' && p->name[0] != '\003')
						print("process_exit", itoa(p->pid), p->name, itoa(WEXITSTATUS(status)));
					xfree(p->name);
					list_remove(&children, p, 1);
				}
			}
		}

	}
	
	return;
}

static void handle_sigusr1()
{
	event_check(EVENT_SIGUSR1, 1, "SIGUSR1");
	signal(SIGUSR1, handle_sigusr1);
}

static void handle_sigusr2()
{
	event_check(EVENT_SIGUSR2, 1, "SIGUSR2");
	signal(SIGUSR2, handle_sigusr2);
}

static void handle_sighup()
{
	ekg_exit();
}

/*
 * ioctld_kill()
 *
 * zajmuje si� usuni�ciem ioctld z pami�ci.
 */
static void ioctld_kill()
{
        if (ioctld_pid > 0 && ekg_pid == getpid())
                kill(ioctld_pid, SIGINT);
}

static void handle_sigsegv()
{
	static int killing_ui = 0;

	ioctld_kill();

	if (!killing_ui) {
		ui_deinit();
		killing_ui = 1;
	}
	
	signal(SIGSEGV, SIG_DFL);

	if (old_stderr)
		dup2(old_stderr, 2);

	fprintf(stderr, 
"\r\n"
"\r\n"
"*** Naruszenie ochrony pami�ci ***\r\n"
"\r\n"
"Spr�buj� zapisa� ustawienia, ale nie obiecuj�, �e cokolwiek z tego\r\n"
"wyjdzie. Trafi� one do plik�w %s/config.%d\r\n"
"oraz %s/userlist.%d\r\n"
"\r\n"
"Do pliku %s/debug.%d zapisz� ostatanie komunikaty\r\n"
"z okna debugowania.\r\n"
"\r\n"
"Je�li zostanie utworzony plik %s/core, spr�buj uruchomi�\r\n"
"polecenie:\r\n"
"\r\n"
"    gdb %s %s/core\r\n"
"\n"
"zanotowa� kilka ostatnich linii, a nast�pnie zanotowa� wynik polecenia\r\n"
",,bt''. Dzi�ki temu autorzy dowiedz� si�, w kt�rym miejscu wyst�pi� b��d\r\n"
"i najprawdopodobniej pozwoli to unikn�� tego typu sytuacji w przysz�o�ci.\r\n"
"Wi�cej szczeg��w w dokumentacji, w pliku ,,gdb.txt''.\r\n"
"\r\n",
config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, argv0, config_dir);

	config_write_crash();
	userlist_write_crash();
	debug_write_crash();

	raise(SIGSEGV);			/* niech zrzuci core */
}

/*
 * prepare_batch_line()
 *
 * funkcja bierze podane w linii polece� argumenty i robi z nich pojedy�cz�
 * lini� polece�.
 *
 * - argc - wiadomo co ;)
 * - argv - wiadomo co ;)
 * - n - numer argumentu od kt�rego zaczyna si� polecenie.
 *
 * zwraca stworzon� linie w zaalokowanym buforze lub NULL przy b��dzie.
 */
static char *prepare_batch_line(int argc, char *argv[], int n)
{
	size_t len = 0;
	char *buf;
	int i;

	for (i = n; i < argc; i++)
		len += strlen(argv[i]) + 1;

	buf = xmalloc(len);

	for (i = n; i < argc; i++) {
		strlcat(buf, argv[i], len);
		if (i < argc - 1)
			strlcat(buf, " ", len);
	}

	return buf;
}

#ifndef GG_DEBUG_DISABLE
/*
 * debug_handler()
 *
 * obs�uguje informacje debugowania libgadu i klienta.
 */
static void debug_handler(int level, const char *format, va_list ap)
{
	static string_t line = NULL;
	char *tmp;

	tmp = gg_vsaprintf(format, ap);

	if (line) {
		string_append(line, tmp);
		xfree(tmp);
		tmp = NULL;

		if (line->str[strlen(line->str) - 1] == '\n') {
			tmp = string_free(line, 0);
			line = NULL;
		}
	} else {
		if (tmp[strlen(tmp) - 1] != '\n') {
			line = string_init(tmp);
			xfree(tmp);
			tmp = NULL;
		}
	}
		
	if (!tmp)
		return;

	tmp[strlen(tmp) - 1] = 0;

	buffer_add(BUFFER_DEBUG, NULL, tmp, DEBUG_MAX_LINES);
	if (ui_print)
		print_window("__debug", 0, "debug", tmp);
	xfree(tmp);
}
#endif

/*
 * ekg_ui_set()
 *
 * w��cza interfejs o podanej nazwie.
 *
 * 0/-1
 */
static int ekg_ui_set(const char *name)
{
	if (!name)
		return 0;

	if (!strcasecmp(name, "none"))
		ui_init = ui_none_init;
	else if (!strcasecmp(name, "batch"))
		ui_init = ui_batch_init;
#ifdef WITH_UI_READLINE
	else if (!strcasecmp(name, "readline"))
		ui_init = ui_readline_init;
#endif
#ifdef WITH_UI_NCURSES
	else if (!strcasecmp(name, "ncurses"))
		ui_init = ui_ncurses_init;
#endif
	else
		return -1;

	xfree(config_interface);
	config_interface = xstrdup(name);

	return 0;
}

int main(int argc, char **argv)
{
	int auto_connect = 1, new_status = 0, ui_set = 0;
	int c = 0, set_private = 0, no_global_config = 0;
	char *tmp = NULL;
	char *load_theme = NULL, *new_reason = NULL, *new_profile = NULL;
#ifdef WITH_IOCTLD
	const char *sock_path = NULL, *ioctld_path = IOCTLD_PATH;
#endif
	struct passwd *pw; 
	struct gg_common si;
	struct option ekg_options[] = {
		{ "back", optional_argument, 0, 'b' },
		{ "away", optional_argument, 0, 'a' },
		{ "invisible", optional_argument, 0, 'i' },
		{ "private", no_argument, 0, 'p' },
		{ "no-auto", no_argument, 0, 'n' },
		{ "control-pipe", required_argument, 0, 'c' },
		{ "frontend", required_argument, 0, 'f' },
		{ "help", no_argument, 0, 'h' },
		{ "ioctld-path", required_argument, 0, 'I' },
		{ "no-pipe", no_argument, 0, 'o' },
		{ "theme", required_argument, 0, 't' },
		{ "user", required_argument, 0, 'u' },
		{ "version", no_argument, 0, 'v' },
		{ "no-global-config", no_argument, 0, 'N' },
		{ 0, 0, 0, 0 }
	};

	ekg_started = time(NULL);

#ifdef WITH_UI_READLINE
	ui_init = ui_readline_init;
#elif defined(WITH_UI_NCURSES)
	ui_init = ui_ncurses_init;
#else
	ui_init = ui_batch_init;
#endif

#ifdef WITH_FORCE_NCURSES
	ui_init = ui_ncurses_init;
#endif 

	ekg_ui_set(getenv("EKG_UI"));
	ekg_ui_set(getenv("EKG_FRONTEND"));

	srand(time(NULL));

	strlcpy(argv0, argv[0], sizeof(argv0));

	command_init();

	if (!(home_dir = getenv("HOME")))
		if ((pw = getpwuid(getuid())))
			home_dir = pw->pw_dir;

	if (home_dir)
		home_dir = xstrdup(home_dir);

	if (!home_dir) {
		fprintf(stderr, "Nie mog� znale�� katalogu domowego. Popro� administratora, �eby to naprawi�.\n");
		return 1;
	}

	signal(SIGSEGV, handle_sigsegv);
	signal(SIGHUP, handle_sighup);
	signal(SIGUSR1, handle_sigusr1);
	signal(SIGUSR2, handle_sigusr2);
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	while ((c = getopt_long(argc, argv, "b::a::i::pdnc:f:hI:ot:u:vN", ekg_options, NULL)) != -1) {
		switch (c) {
			case 'b':
				if (!optarg && argv[optind] && argv[optind][0] != '-')
					optarg = argv[optind++];

				new_status = (optarg) ? GG_STATUS_AVAIL_DESCR : GG_STATUS_AVAIL;
				xfree(new_reason);
				new_reason = xstrdup(optarg);
			        break;
			case 'a':
				if (!optarg && argv[optind] && argv[optind][0] != '-')
					optarg = argv[optind++];

				new_status = (optarg) ? GG_STATUS_BUSY_DESCR : GG_STATUS_BUSY;
				xfree(new_reason);
				new_reason = xstrdup(optarg);
			        break;
			case 'i':
				if (!optarg && argv[optind] && argv[optind][0] != '-')
					optarg = argv[optind++];

				new_status = (optarg) ? GG_STATUS_INVISIBLE_DESCR : GG_STATUS_INVISIBLE;
				xfree(new_reason);
				new_reason = xstrdup(optarg);
			        break;
			case 'p':
				set_private = 1;
			        break;
			case 'n':
				auto_connect = 0;
				break;
			case 'N':
				no_global_config = 1;
				break;
			case 'h':
				printf(""
"u�ycie: %s [OPCJE] [KOMENDY]\n"
"  -N, --no-global-config     ignoruje globalny plik konfiguracyjny\n"
"  -u, --user=NAZWA           korzysta z profilu u�ytkownika o podanej nazwie\n"
"  -t, --theme=PLIK           �aduje opis wygl�du z podanego pliku\n"
"  -c, --control-pipe=PLIK    potok nazwany sterowania\n"
"  -n, --no-auto              nie ��czy si� automatycznie z serwerem\n"
"  -a, --away[=OPIS]          domy�lnie zmienia stan na ,,zaj�ty''\n"
"  -b, --back[=OPIS]          domy�lnie zmienia stan na ,,dost�pny''\n"
"  -i, --invisible[=OPIS]     domy�lnie zmienia stan na ,,niewidoczny''\n"
"  -p, --private              domy�lnie ustawia tryb ,,tylko dla znajomych''\n"
"  -v, --version              wy�wietla wersje programu i wychodzi\n"
#ifdef WITH_IOCTLD
"  -I, --ioctld-path=�CIE�KA  ustawia �cie�k� do ioctld\n"
#endif
"  -f, --frontend=NAZWA       wybiera jeden z dost�pnych interfejs�w\n"
"                             (none, batch"
#ifdef WITH_UI_READLINE
", readline"
#endif
#ifdef WITH_UI_NCURSES
", ncurses"
#endif
")\n"
"\n", argv[0]);
				return 0;
				break;
			case 'u':
				new_profile = optarg;
				break;
			case 'c':
				pipe_file = optarg;
				break;
			case 'o':
				pipe_file = NULL;
				break;
			case 't':
				load_theme = optarg;
				break;
			case 'v':
			    	printf("ekg-%s\nlibgadu-%s (headers %s, protocol 0x%.2x, client \"%s\")\ncompile time: %s\n", VERSION, gg_libgadu_version(), GG_LIBGADU_VERSION, GG_DEFAULT_PROTOCOL_VERSION, GG_DEFAULT_CLIENT_VERSION, compile_time());
				return 0;
#ifdef WITH_IOCTLD
			case 'I':
				ioctld_path = optarg;
			break;
#endif
			case 'f':
				ui_set = 1;

				if (ekg_ui_set(optarg)) {
					fprintf(stderr, "Nieznany interfejs %s.\n", optarg);
					return 1;
				}

				break;
			case '?':
				/* obs�ugiwane przez getopt */
				fprintf(stdout, "Aby uzyska� wi�cej informacji, uruchom program z opcj� --help.\n");
				return 1;
			default:
				break;
		}
	}

	in_autoexec = 1;

	if (optind < argc) {
		batch_line = prepare_batch_line(argc, argv, optind);
		batch_mode = 1;
		
		if (!ui_set)
			ui_init = ui_batch_init;
	}

	if ((config_profile = new_profile))
		tmp = saprintf("/%s", config_profile);
	else
		tmp = xstrdup("");

	if (getenv("CONFIG_DIR"))
		config_dir = saprintf("%s/%s/gg%s", home_dir, getenv("CONFIG_DIR"), tmp);
	else
		config_dir = saprintf("%s/.gg%s", home_dir, tmp);

	xfree(tmp);
	tmp = NULL;

	if (!batch_mode && !ui_set && (tmp = config_read_variable("interface"))) {
		ekg_ui_set(tmp);
		xfree(tmp);
	}

	variable_init();
	variable_set_default();

	gg_debug_level = 0;

	if (getenv("EKG_DEBUG")) {
		if ((gg_debug_file = fopen(getenv("EKG_DEBUG"), "w"))) {
			setbuf(gg_debug_file, NULL);
			gg_debug_level = 255;
		}
	}

#ifdef WITH_UI_NCURSES
	if (ui_init == ui_ncurses_init) {
#ifndef GG_DEBUG_DISABLE
		if (!gg_debug_file)
			gg_debug_handler = debug_handler;
#endif

		gg_debug_level = 255;
	}
#endif

#ifdef WITH_UI_READLINE
	if (ui_init == ui_readline_init) {
		if (!gg_debug_file)
			gg_debug_handler = debug_handler;

		gg_debug_level = 255;
	}
#endif

        ekg_pid = getpid();
	mesg_startup = mesg_set(MESG_CHECK);

#ifdef WITH_PYTHON
	python_initialize();
#endif

	theme_init();

	ui_screen_width = getenv("COLUMNS") ? atoi(getenv("COLUMNS")) : 80;
	ui_screen_height = getenv("LINES") ? atoi(getenv("LINES")) : 24;

#ifdef WITH_UI_NCURSES
	if (ui_init == ui_ncurses_init) {
		if ((tmp = config_read_variable("display_transparent"))) {
			config_display_transparent = atoi(tmp);
			xfree(tmp);
		}

		if ((tmp = config_read_variable("contacts"))) {
			config_contacts = atoi(tmp);
			xfree(tmp);
		}
	}
#endif

	ui_init();
	ui_event("theme_init");

	if (ui_set && config_interface && strcmp(config_interface, "")) {
		char **arr = NULL;

		array_add(&arr, xstrdup("interface"));
		config_write_partly(arr);
		array_free(arr);
	}

	if (!no_global_config)
		config_read(SYSCONFDIR "/ekg.conf");

	config_read(NULL);

	if (!no_global_config)
		config_read(SYSCONFDIR "/ekg-override.conf");
	
        userlist_read();
	update_status();
	emoticon_read();
	msg_queue_read();

	in_autoexec = 0;

	ui_postinit();

	ui_event("xterm_update");
	
#ifdef WITH_IOCTLD
	if (!batch_mode) {
		sock_path = prepare_path(".socket", 1);
	
		if (!(ioctld_pid = fork())) {
			execl(ioctld_path, "ioctld", sock_path, (void *) NULL);
			exit(0);
		}
	
		ioctld_socket(sock_path);
	
		atexit(ioctld_kill);
	}
#endif /* WITH_IOCTLD */

	if (!batch_mode && pipe_file)
		pipe_fd = init_control_pipe(pipe_file);

	if (!config_keep_reason) {
		xfree(config_reason);
		config_reason = NULL;
		config_status = ekg_hide_descr_status(config_status);
	}

	/* okre�lanie stanu klienta po w��czeniu */
	if (new_status) {
		if (config_keep_reason && config_reason) {
			switch (new_status) {
				case GG_STATUS_AVAIL:
					new_status = GG_STATUS_AVAIL_DESCR;
					break;
				case GG_STATUS_BUSY:
					new_status = GG_STATUS_BUSY_DESCR;
					break;
				case GG_STATUS_INVISIBLE:
					new_status = GG_STATUS_INVISIBLE_DESCR;
					break;
			}
		}

		config_status = new_status | (GG_S_F(config_status) ? GG_STATUS_FRIENDS_MASK : 0);
	}

	if (new_reason) {
		xfree(config_reason);
		config_reason = new_reason;
	}

	if (set_private)
		config_status |= GG_STATUS_FRIENDS_MASK;

	/* uaktualnij wszystko, co trzeba */
	change_status(config_status, config_reason, 2);

	/* je�li ma by� theme, niech b�dzie theme */
	if (load_theme)
		theme_read(load_theme, 1);
	else
		if (config_theme)
			theme_read(config_theme, 1);
	
	theme_cache_reset();
		
	time(&last_action);

	/* dodajemy stdin do ogl�danych deskryptor�w */
	if (!batch_mode && ui_init != ui_none_init) {
		memset(&si, 0, sizeof(si));
		si.fd = 0;
		si.check = GG_CHECK_READ;
		si.state = GG_STATE_READING_DATA;
		si.type = GG_SESSION_USER0;
		si.id = 0;
		si.timeout = -1;
		list_add(&watches, &si, sizeof(si));
	}

	/* stderr */
	if (!batch_mode) {
		struct gg_exec se;
		int fd[2];
		
		if (!pipe(fd)) {
			memset(&se, 0, sizeof(se));
			se.fd = fd[0];
			se.check = GG_CHECK_READ;
			se.state = GG_STATE_READING_DATA;
			se.type = GG_SESSION_USER3;
			se.id = 2;
			se.timeout = -1;
			se.buf = string_init(NULL);
			list_add(&watches, &se, sizeof(se));

			fcntl(fd[0], F_SETFL, O_NONBLOCK);
			fcntl(fd[1], F_SETFL, O_NONBLOCK);

			old_stderr = fcntl(2, F_DUPFD, 0);
			dup2(fd[1], 2);
		}
	}

	/* dodajemy otwarty potok sterujacy do ogl�danych deskryptor�w */
	if (!batch_mode && pipe_fd > 0) {
		memset(&si, 0, sizeof(si));
		si.fd = pipe_fd;
		si.check = GG_CHECK_READ;
		si.state = GG_STATE_READING_DATA;
		si.type = GG_SESSION_USER1;
		si.id = 0;
		si.timeout = -1;
		list_add(&watches, &si, sizeof(si));
	}

	if (!batch_mode && config_display_welcome)
		print("welcome", VERSION);

	if (!config_uin || !config_password)
		print("no_config");

	ui_event("config_changed");

	if (!config_log_path) 
		config_log_path = xstrdup(prepare_path("history", 0));

#ifdef HAVE_OPENSSL
	sim_key_path = xstrdup(prepare_path("keys/", 0));
#endif

	changed_dcc("dcc");

#ifdef WITH_PYTHON
	python_autorun();
#endif

	if (config_uin && config_password && auto_connect) {
		print("connecting");
		connecting = 1;
		ekg_connect();
	}

	if (config_auto_save)
		last_save = time(NULL);
	
	ui_loop();

	ekg_exit();

	return 0;
}

/*
 * ekg_exit()
 *
 * wychodzi z klienta sprz�taj�c przy okazji wszystkie sesje, zwalniaj�c
 * pami�� i czyszcz�c pok�j.
 */
void ekg_exit()
{
	char **vars = NULL;
	list_t l;
	int i;

#ifdef WITH_PYTHON
	python_finalize();
#endif

	ekg_logoff(sess, NULL);
	list_remove(&watches, sess, 0);
	gg_free_session(sess);
	sess = NULL;

	ui_deinit();

	msg_queue_write();

	xfree(last_search_first_name);
	xfree(last_search_last_name);
	xfree(last_search_nickname);

	if (config_last_sysmsg_changed)
		array_add(&vars, xstrdup("last_sysmsg"));

	if (config_keep_reason) {
		if (config_keep_reason != 2)
			array_add(&vars, xstrdup("status"));
		array_add(&vars, xstrdup("reason"));
	}

	if (config_server_save)
		array_add(&vars, xstrdup("server"));

	if (config_windows_save)
		array_add(&vars, xstrdup("windows_layout"));

	if (vars) {
		config_write_partly(vars);
		array_free(vars);
	}

	if (config_changed && !config_speech_app && config_save_question) {
		char line[80];

		printf("%s", format_find("config_changed"));
		fflush(stdout);
		if (fgets(line, sizeof(line), stdin)) {
			if (line[strlen(line) - 1] == '\n')
				line[strlen(line) - 1] = 0;
			if (!strcasecmp(line, "tak") || !strcasecmp(line, "yes") || !strcasecmp(line, "t") || !strcasecmp(line, "y")) {
				if (userlist_write(NULL) || config_write(NULL))
					printf("Wyst�pi� b��d podczas zapisu.\n");
			}
		} else
			printf("\n");
	}

	for (i = 0; i < SEND_NICKS_MAX; i++) {
		xfree(send_nicks[i]);
		send_nicks[i] = NULL;
	}
	send_nicks_count = 0;

	for (l = searches; l; l = l->next)
		gg_pubdir50_free(l->data);

	for (l = children; l; l = l->next) {
		struct process *p = l->data;

		kill(p->pid, SIGTERM);
	}

	if (pipe_fd > 0)
		close(pipe_fd);
	if (pipe_file)
		unlink(pipe_file);
	
#ifdef HAVE_OPENSSL
	xfree(sim_key_path);
#endif

	for (l = watches; l; l = l->next) {
		struct gg_session *s = l->data;
		int i;

		for (i = 0; handlers[i].reaper; i++) {
			if (handlers[i].type == s->type) {
				handlers[i].reaper(s);
				break;
			}
		}
	}

	list_destroy(watches, 0);

	msg_queue_free();
	alias_free();
	conference_free();
	userlist_free();
	theme_free();
	variable_free();
	event_free();
	emoticon_free();
	sms_away_free();
	check_mail_free();
	command_free();
	timer_free();
	binding_free();
	last_free();
	buffer_free();
	list_destroy(autofinds, 1);

	xfree(home_dir);

	xfree(gg_proxy_host);
	xfree(gg_proxy_username);
	xfree(gg_proxy_password);
	xfree(config_dir);

	/* kapitan schodzi ostatni */
	if (gg_debug_file) {
		fclose(gg_debug_file);
		gg_debug_file = NULL;
	}

	mesg_set(mesg_startup);

	exit(0);
}
