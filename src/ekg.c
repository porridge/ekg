/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *                          Adam Osuchowski <adwol@polsl.gliwice.pl>
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
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <sys/time.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <libgadu.h>
#include "config.h"
#include "libgadu.h"
#include "stuff.h"
#include "commands.h"
#include "events.h"
#include "themes.h"
#include "version.h"
#include "userlist.h"
#include "vars.h"
#include "xmalloc.h"
#include "ui.h"
#include "python.h"

time_t last_action = 0;
int ioctld_pid = 0;
int ekg_pid = 0;
char argv0[PATH_MAX];
char *pipe_file = NULL;

/*
 * usuwanie sesji GG_SESSION_USERx.
 */
void reaper_user(void *foo)
{
	xfree(foo);
}

#define VV void(*)(void*)

/*
 * struktura zawieraj±ca adresy funkcji obs³uguj±cych ró¿ne sesje
 * i zwalniaj±cych pamiêæ po nich.
 */
static struct {
	int type;
	void (*handler)(void*);
	void (*reaper)(void*);
} handlers[] = {
	{ GG_SESSION_GG, (VV) handle_event, (VV) gg_free_session },
	{ GG_SESSION_DCC, (VV) handle_dcc, (VV) gg_dcc_free },
	{ GG_SESSION_DCC_SOCKET, (VV) handle_dcc, (VV) gg_dcc_free },
	{ GG_SESSION_DCC_SEND, (VV) handle_dcc, (VV) gg_dcc_free },
	{ GG_SESSION_DCC_GET, (VV) handle_dcc, (VV) gg_dcc_free },
	{ GG_SESSION_DCC_VOICE, (VV) handle_dcc, (VV) gg_dcc_free },
	{ GG_SESSION_SEARCH, (VV) handle_search, (VV) gg_search_free },
	{ GG_SESSION_REGISTER, (VV) handle_pubdir, (VV) gg_register_free },
	{ GG_SESSION_PASSWD, (VV) handle_pubdir, (VV) gg_change_passwd_free },
	{ GG_SESSION_REMIND, (VV) handle_pubdir, (VV) gg_remind_passwd_free },
	{ GG_SESSION_CHANGE, (VV) handle_pubdir, (VV) gg_change_pubdir_free },
	{ GG_SESSION_USERLIST_GET, (VV) handle_userlist, (VV) gg_userlist_get_free },
	{ GG_SESSION_USERLIST_PUT, (VV) handle_userlist, (VV) gg_userlist_put_free },
	{ GG_SESSION_USER0, NULL, (VV) reaper_user },
	{ GG_SESSION_USER1, NULL, (VV) reaper_user },
	{ GG_SESSION_USER2, (VV) handle_voice, (VV) reaper_user },
	{ -1, NULL, NULL }, 
};

#define PIPE_MSG_MAX_BUF_LEN 1024

/*
 * get_char_from_pipe()
 *
 * funkcja pobiera z potoku steruj±cego znak do bufora, a gdy siê zape³ni
 * bufor wykonuje go tak jakby tekst w buforze wpisany by³ z terminala.
 *
 * - c - struktura steruj±ca przechowuj±ca m.in. deskryptor potoku.
 */
void get_char_from_pipe(struct gg_common *c)
{
	static char buf[PIPE_MSG_MAX_BUF_LEN + 1];
	char ch;
  
	if (!c)
  		return;

	if (read(c->fd, &ch, 1) > 0) {
		if (ch != '\n' && ch != '\r') {
			if (strlen(buf) < PIPE_MSG_MAX_BUF_LEN)
				buf[strlen(buf)] = ch;
		}
		if (ch == '\n' || (strlen(buf) >= PIPE_MSG_MAX_BUF_LEN)) {
			command_exec(NULL, buf);
			memset(buf, 0, PIPE_MSG_MAX_BUF_LEN + 1);
		}
	}
}

/*
 * ekg_wait_for_key()
 *
 * funkcja wywo³ywana przez interfejsy u¿ytkownika do przetwarzania danych
 * z sieci, gdy czeka siê na reakcjê u¿ytkownika.
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
		/* przejrzyj timery */
		for (l = timers; l; ) {
			struct timer *t = l->data;

			l = l->next;

			if (time(NULL) - t->started > t->period) {
#ifdef WITH_PYTHON
				if (t->script)
					python_function(t->command);
				else
#endif
				if (t->ui)
					ui_event(t->command);
				else
					command_exec(NULL, t->command);

				xfree(t->name);
				xfree(t->command);

				list_remove(&timers, t, 1);
			}
		}

		FD_ZERO(&rd);
		FD_ZERO(&wd);
		
		maxfd = 0;

#ifdef WITH_WAP
		/* co jaki¶ czas zrzuæ userlistê dla frontendu wap */
		if (!wap_userlist_timer)
			wap_userlist_timer = time(NULL);

		if (wap_userlist_timer + 60 > time(NULL)) {
			userlist_write_wap();
			wap_userlist_timer = time(NULL);
		}
#endif

		/* zerknij na wszystkie niezbêdne deskryptory */
		for (l = watches; l; l = l->next) {
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

		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		ret = select(maxfd + 1, &rd, &wd, NULL, &tv);
	
		if (ret == -1) {
			if (errno != EINTR)
				perror("select()");
			continue;
		}

		if (!ret) {
			/* timeouty danych sesji */
			for (l = watches; l; l = l->next) {
				struct gg_session *s = l->data;
				struct gg_common *c = l->data;
				struct gg_http *h = l->data;
				struct gg_dcc *d = l->data;
				char *errmsg = "";

				if (!c || c->timeout == -1 || --c->timeout)
					continue;
				
				switch (c->type) {
					case GG_SESSION_GG:
						print("conn_timeout");
						list_remove(&watches, s, 0);
						gg_free_session(s);
						sess = NULL;
						do_reconnect();
						break;

					case GG_SESSION_SEARCH:
						print("search_timeout");
						free(h->user_data);
						list_remove(&watches, h, 0);
						gg_free_search(h);
						break;

					case GG_SESSION_REGISTER:
						if (!errmsg)
							errmsg = "register_timeout";
					case GG_SESSION_PASSWD:
						if (!errmsg)
							errmsg = "passwd_timeout";
					case GG_SESSION_REMIND:
						if (!errmsg)
							errmsg = "remind_timeout";
					case GG_SESSION_CHANGE:
						if (!errmsg)
							errmsg = "change_timeout";

						print(errmsg);
						if (h->type == GG_SESSION_REGISTER) {
							free(reg_password);
							reg_password = NULL;
						}
						list_remove(&watches, h, 0);
						gg_free_pubdir(h);
						break;
						
					case GG_SESSION_DCC:
					case GG_SESSION_DCC_GET:
					case GG_SESSION_DCC_SEND:
						/* XXX informowaæ który */
						print("dcc_timeout");
						list_remove(&watches, d, 0);
						gg_free_dcc(d);
						break;
				}
				break;
			}
			
			/* timeout reconnectu */
			if (!sess && reconnect_timer && time(NULL) - reconnect_timer >= config_auto_reconnect && config_uin && config_password) {
				reconnect_timer = 0;
				print("connecting");
				connecting = 1;
				do_connect();
			}

			/* timeout pinga */
			if (sess && sess->state == GG_STATE_CONNECTED && time(NULL) - last_ping > 60) {
				if (last_ping)
					gg_ping(sess);
				last_ping = time(NULL);
			}

			/* timeout autoawaya */
			if (sess && config_auto_away && (away == 0 || away == 4) && time(NULL) - last_action > config_auto_away && sess->state == GG_STATE_CONNECTED) {
				char tmp[16], *reason = NULL;

				if (config_random_reason & 1) {
					reason = random_line(prepare_path("away.reasons", 0));
					if (!reason && config_away_reason)
					    	reason = xstrdup(config_away_reason);
				}
				else if (config_away_reason)
				    	reason = xstrdup(config_away_reason);
				
				away = (reason) ? 3 : 1;

				if (reason) {
				    	iso_to_cp(reason);
					gg_change_status_descr(sess, GG_STATUS_BUSY_DESCR | (private_mode ? GG_STATUS_FRIENDS_MASK : 0), reason);
					cp_to_iso(reason);
				} else
				    	gg_change_status(sess, GG_STATUS_BUSY | (private_mode ? GG_STATUS_FRIENDS_MASK : 0));
				
				if (!(config_auto_away % 60))
					snprintf(tmp, sizeof(tmp), "%dm", config_auto_away / 60);
				else
					snprintf(tmp, sizeof(tmp), "%ds", config_auto_away);
				
				print((reason) ? "auto_away_descr" : "auto_away", tmp, reason);
				ui_event("my_status", "away", reason);
				ui_event("my_status_raw", GG_STATUS_BUSY, reason);	/* XXX */
				free(config_reason);
				config_reason = reason;
			}

			/* auto save */
			if (config_changed && config_auto_save && time(NULL) - last_save > config_auto_save) {
				last_save = time(NULL);
				gg_debug(GG_DEBUG_MISC, "-- autosaving userlist and config after %d seconds.\n", config_auto_save);

				if (!userlist_write(NULL) && !config_write(NULL)) {
					config_changed = 0;
					print("autosaved");
				} else
					print("error_saving");
			}

			/* przegl±danie zdech³ych dzieciaków */
			while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
				for (l = children; l; l = m) {
					struct process *p = l->data;

					m = l->next;

					if (pid != p->pid)
						continue;

					if (p->name[0] == '\001') {
						print((!(WEXITSTATUS(status))) ? "sms_sent" : "sms_failed", p->name + 1);
					} else if (p->name[0] == '\002') {
						// do nothing
					} else {	
						print("process_exit", itoa(p->pid), p->name, itoa(WEXITSTATUS(status)));
					}

					free(p->name);
					list_remove(&children, p, 1);
				}
			}

			if (batch_mode && !batch_line)
				break;
		} else {
			for (l = watches; l; l = l->next) {
				struct gg_common *c = l->data;
				int i;

				if (!c || (!FD_ISSET(c->fd, &rd) && !FD_ISSET(c->fd, &wd)))
					continue;

				if (c->type == GG_SESSION_USER0) 
					return;

				if (c->type == GG_SESSION_USER1) {
					get_char_from_pipe(c);
					break;
				}

				if (c->type == GG_SESSION_USER2) {
					handle_voice();
					break;
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
		}
	}
	
	return;
}

void sigusr1_handler()
{
	event_check(EVENT_SIGUSR1, 1, "SIGUSR1");
	signal(SIGUSR1, sigusr1_handler);
}

void sigusr2_handler()
{
	event_check(EVENT_SIGUSR2, 1, "SIGUSR2");
	signal(SIGUSR1, sigusr2_handler);
}

void sighup_handler()
{
	if (sess && sess->state != GG_STATE_IDLE) {
		print("disconected");
		ekg_logoff(sess, NULL);
		list_remove(&watches, sess, 0);
		gg_free_session(sess);
		sess = NULL;
	}
	
	signal(SIGHUP, sighup_handler);
}

void kill_ioctld()
{
        if (ioctld_pid > 0 && ekg_pid == getpid())
                kill(ioctld_pid, SIGINT);
}

void sigsegv_handler()
{
	signal(SIGSEGV, SIG_DFL);

	ekg_segv_handler = 1;

	ui_deinit();
	
	kill_ioctld();
	
	fprintf(stderr, "\n\
*** Naruszenie ochrony pamiêci ***\n\
\n\
Spróbujê zapisaæ ustawienia, ale nie obiecujê, ¿e cokolwiek z tego\n\
wyjdzie. Trafi± one do plików %s/config.%d\n\
oraz %s/userlist.%d\n\
\n\
Je¶li zostanie utworzony plik %s/core, spróbuj uruchomiæ\n\
polecenie:\n\
\n\
    gdb %s %s/core\n\
\n\
zanotowaæ kilka ostatnich linii, a nastêpnie zanotowaæ wynik polecenia\n\
,,bt''. Dziêki temu autorzy dowiedz± siê, w którym miejscu wyst±pi³ b³±d\n\
i najprawdopodobniej pozwoli to unikn±æ tego typu sytuacji w przysz³o¶ci.\n\
Wiêcej szczegó³ów w dokumentacji, w pliku ,,gdb.txt''.\n\
\n",
config_dir, getpid(), config_dir, getpid(), config_dir, argv0, config_dir);

	config_write_crash();
	userlist_write_crash();

	raise(SIGSEGV);			/* niech zrzuci core */
}

/*
 * prepare_batch_line()
 *
 * funkcja bierze podane w linii poleceñ argumenty i robi z nich pojedyñcz±
 * liniê poleceñ.
 *
 * - argc - wiadomo co ;)
 * - argv - wiadomo co ;)
 * - n - numer argumentu od którego zaczyna siê polecenie.
 *
 * zwraca stworzon± linie w zaalokowanym buforze lub NULL przy b³êdzie.
 */
char *prepare_batch_line(int argc, char *argv[], int n)
{
	int i;
	size_t m = 0;
	char *bl;

	for (i = n; i < argc; i++)
		m += strlen(argv[i]) + 1;

	bl = xmalloc(m);

	for (i = n; i < argc; i++) {
		strcat(bl, argv[i]);
		if (i < argc - 1)
			strcat(bl, " ");
	}

	return bl;
}

int main(int argc, char **argv)
{
	int auto_connect = 1, force_debug = 0, i, new_status = 0, ui_set = 0;
	char *load_theme = NULL;
#ifdef WITH_IOCTLD
	const char *sock_path = NULL, *ioctld_path = IOCTLD_PATH;
#endif
	struct passwd *pw; 
	struct gg_common si;
	void (*ui_init)();

#ifdef WITH_UI_NCURSES
	ui_init = ui_ncurses_init;
#else
	ui_init = ui_readline_init;
#endif

	srand(time(NULL));

	strncpy(argv0, argv[0], sizeof(argv0) - 1);
	argv0[sizeof(argv0) - 1] = 0;

	variable_init();
	command_init();

	if (!(home_dir = getenv("HOME")))
		if ((pw = getpwuid(getuid())))
			home_dir = pw->pw_dir;

	if (home_dir)
		home_dir = xstrdup(home_dir);

	if (!home_dir) {
		fprintf(stderr, "Nie mogê znale¼æ katalogu domowego. Popro¶ administratora, ¿eby to naprawi³.\n");
		return 1;
	}

	if (getenv("CONFIG_DIR"))
		config_dir = saprintf("%s/%s/gg", home_dir, getenv("CONFIG_DIR"));
	else
		config_dir = saprintf("%s/.gg", home_dir);

	signal(SIGSEGV, sigsegv_handler);
	signal(SIGHUP, sighup_handler);
	signal(SIGUSR1, sigusr1_handler);
	signal(SIGUSR2, sigusr2_handler);
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	config_user = "";

	for (i = 1; i < argc && *argv[i] == '-'; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			printf(""
"u¿ycie: %s [OPCJE] [KOMENDY]\n"
"  -u, --user [NAZWA]   korzysta z profilu u¿ytkownika o podanej nazwie\n"
"  -t, --theme [PLIK]   ³aduje opis wygl±du z podanego pliku\n"
"  -c, --control-pipe [PLIK]	potok nazwany sterowania\n"
"  -n, --no-auto        nie ³±czy siê automatycznie z serwerem\n"
"  -a, --away           po po³±czeniu zmienia stan na ,,zajêty''\n"
"  -b, --back           po po³±czeniu zmienia stan na ,,dostêpny''\n"
"  -i, --invisible      po po³±czeniu zmienia stan na ,,niewidoczny''\n"
"  -p, --private        po po³±czeniu zmienia stan na ,,tylko dla przyjació³''\n"
"  -d, --debug          w³±cza wy¶wietlanie dodatkowych informacji\n"
"  -v, --version        wy¶wietla wersje programu i wychodzi\n"
#ifdef WITH_IOCTLD
"  -I, --ioctld-path [¦CIE¯KA]    ustawia ¶cie¿kê do ioctld\n"
#endif
"  -f, --frontend [NAZWA]         wybiera jeden z dostêpnych interfejsów\n"
"                                 (none, batch"
#ifdef WITH_UI_READLINE
", readline"
#endif
#ifdef WITH_UI_NCURSES
", ncurses"
#endif
")\n"
"\n", argv[0]);
			return 0;	
		}
		if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--back"))
			new_status = GG_STATUS_AVAIL;
		if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--away"))
			new_status = GG_STATUS_BUSY;
		if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--invisible"))
			new_status = GG_STATUS_INVISIBLE;
		if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--private"))
			new_status |= GG_STATUS_FRIENDS_MASK;
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug"))
			force_debug = 1;
		if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--no-auto"))
			auto_connect = 0;
		if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--user")){ 
			if (argv[i+1]) { 
				config_user = argv[i+1];
				i++;
			} else {
				fprintf(stderr, "Nie podano nazwy u¿ytkownika.\n");
		   		return 1;
			}
		}
		if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--control-pipe")) {
			if (argv[i + 1]) {
				pipe_file = argv[i + 1];
				i++;
			} else {
				fprintf(stderr, "Nie podano nazwy potoku kontrolnego");
				return 1;
			}
		}
		if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--no-pipe"))
			pipe_file = NULL;
		if ((!strcmp(argv[i], "-t") || !strcmp(argv[i], "--theme")) && argv[i + 1])
			load_theme = argv[++i];
		if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
		    	printf("ekg-%s\nlibgadu-%s (headers %s, protocol 0x%.2x, client \"%s\")\n", VERSION, gg_libgadu_version(), GG_LIBGADU_VERSION, GG_DEFAULT_PROTOCOL_VERSION, GG_DEFAULT_CLIENT_VERSION);
			return 0;
		}
#ifdef WITH_IOCTLD
                if (!strcmp(argv[i], "-I") || !strcmp(argv[i], "--ioctld-path")) {
                        if (argv[i + 1]) {
                                ioctld_path = argv[i + 1];
                                i++;
                        } else {
                                fprintf(stderr, "Nie podano ¶cie¿ki do ioctld.\n");
                                return 1;
                        }
                }
#endif
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--frontend")) {
			ui_set = 1;
			if (!argv[i + 1]) {
				fprintf(stderr, "Nie podano nazwy interfejsu.\n");
				return 1;
			}
			if (!strcasecmp(argv[i + 1], "none"))
				ui_init = ui_none_init;
			else if (!strcasecmp(argv[i + 1], "batch"))
				ui_init = ui_batch_init;
#ifdef WITH_UI_READLINE
			else if (!strcasecmp(argv[i + 1], "readline"))
				ui_init = ui_readline_init;
#endif
#ifdef WITH_UI_NCURSES
			else if (!strcasecmp(argv[i + 1], "ncurses"))
				ui_init = ui_ncurses_init;
#endif
			else {
				fprintf(stderr, "Nieznany interfejs %s.\n", argv[i + 1]);
				return 1;
			}
			i++;
		}
	}

	if (i < argc && *argv[i] != '-') {
		batch_line = prepare_batch_line(argc, argv, i);
		batch_mode = 1;
		if (!ui_set)
			ui_init = ui_batch_init;
	}
	
        ekg_pid = getpid();

#ifdef WITH_PYTHON
	python_initialize();
#endif

	theme_init();
	ui_init();

	in_autoexec = 1;
        userlist_read();
	config_read();
	sysmsg_read();
	emoticon_read();
	in_autoexec = 0;

#ifdef WITH_IOCTLD
	if (!batch_mode) {
		sock_path = prepare_path(".socket", 1);
	
		if (!(ioctld_pid = fork())) {
			close(1);
			close(2);
			execl(ioctld_path, "ioctld", sock_path, NULL);
			exit(0);
		}
	
		ioctld_socket(sock_path);
	
		atexit(kill_ioctld);
	}
#endif /* WITH_IOCTLD */

	if (!batch_mode && pipe_file)
		pipe_fd = init_control_pipe(pipe_file);

	/* okre¶lanie stanu klienta po w³±czeniu */
	if (new_status)
		config_status = new_status;

	switch (config_status & ~GG_STATUS_FRIENDS_MASK) {
		case GG_STATUS_AVAIL:
			away = 0;
			break;
		case GG_STATUS_AVAIL_DESCR:
			away = 0;
			config_status = (config_status & GG_STATUS_FRIENDS_MASK) | GG_STATUS_AVAIL;
			break;
		case GG_STATUS_BUSY:
			away = 1;
			break;
		case GG_STATUS_BUSY_DESCR:
			away = 1;
			config_status = (config_status & GG_STATUS_FRIENDS_MASK) | GG_STATUS_BUSY;
			break;
		case GG_STATUS_INVISIBLE:
			away = 2;
			break;
		case GG_STATUS_INVISIBLE_DESCR:
			away = 2;
			config_status = (config_status & GG_STATUS_FRIENDS_MASK) | GG_STATUS_INVISIBLE;
			break;
	}
	
	if ((config_status & GG_STATUS_FRIENDS_MASK))
		private_mode = 1;
	
	/* czy w³±czyæ debugowanie? */	
	if (gg_debug_level)
		config_debug = 1;

	if (force_debug)
		gg_debug_level = 255;
	
	if (config_debug)
		gg_debug_level = 255;
	
	if (load_theme)
		theme_read(load_theme, 1);
	else
		if (config_theme)
			theme_read(config_theme, 1);
	
	theme_cache_reset();
		
	time(&last_action);

	/* dodajemy stdin do ogl±danych deskryptorów */
	if (!batch_mode) {
		memset(&si, 0, sizeof(si));
		si.fd = 0;
		si.check = GG_CHECK_READ;
		si.state = GG_STATE_READING_DATA;
		si.type = GG_SESSION_USER0;
		si.id = 0;
		si.timeout = -1;
		list_add(&watches, &si, sizeof(si));
	}

	/* dodajemy otwarty potok sterujacy do ogl±danych deskryptorów */
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

	if (!batch_mode)
		print("welcome", VERSION);

	if (!config_uin || !config_password)
		print("no_config");

	ui_event("config_changed");

	if (!config_log_path) {
		if (config_user != "")
			config_log_path = saprintf("%s/%s/history", config_dir, config_user);
		else
			config_log_path = saprintf("%s/history", config_dir);
	}
	
	changed_dcc("dcc");

	if (config_uin && config_password && auto_connect) {
		print("connecting");
		connecting = 1;
		do_connect();
	}

	if (config_auto_save)
		last_save = time(NULL);
	
	ui_loop();

	ekg_exit();

	return 0;
}

void ekg_exit()
{
	list_t l;
	int i;

	ekg_logoff(sess, NULL);
	list_remove(&watches, sess, 0);
	gg_free_session(sess);
	sess = NULL;

	ui_deinit();

	if (config_changed) {
		char line[80];

		printf("%s", format_find("config_changed"));
		fflush(stdout);
		if (fgets(line, sizeof(line), stdin)) {
			if (line[strlen(line) - 1] == '\n')
				line[strlen(line) - 1] = 0;
			if (!strcasecmp(line, "tak") || !strcasecmp(line, "yes") || !strcasecmp(line, "t") || !strcasecmp(line, "y")) {
				if (userlist_write(NULL) || config_write(NULL))
					printf("Wyst±pi³ b³±d podczas zapisu.\n");
			}
		} else
			printf("\n");
	}

	for (i = 0; i < SEND_NICKS_MAX; i++) {
		xfree(send_nicks[i]);
		send_nicks[i] = NULL;
	}
	send_nicks_count = 0;

	for (l = children; l; l = l->next) {
		struct process *p = l->data;

		kill(p->pid, SIGTERM);
	}

	if (pipe_fd > 0)
		close(pipe_fd);
	if (pipe_file)
		unlink(pipe_file);
	
	alias_free();
	userlist_free();
	theme_free();
	variable_free();
	event_free();
	emoticon_free();
	command_free();
	timer_free();

	xfree(home_dir);

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

	xfree(gg_proxy_host);
	xfree(config_dir);

#ifdef WITH_PYTHON
	python_finalize();
#endif

	exit(0);
}

