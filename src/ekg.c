/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *                          Adam Osuchowski <adwol@polsl.gliwice.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Wojciech Bojdo³ <wojboj@htcon.pl>
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
#include <fcntl.h>
#include <getopt.h>
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
#include "msgqueue.h"
#ifdef HAVE_OPENSSL
#  include "sim.h"
#endif

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

static int ekg_pid = 0;
static char argv0[PATH_MAX];
static void (*ui_init)();
static int ioctld_pid = 0;
static int mesg_startup;

time_t last_action = 0;
char *pipe_file = NULL;

/*
 * usuwanie sesji GG_SESSION_USERx.
 */
static void reaper_user(void *foo)
{
	xfree(foo);
}

/*
 * usuwanie sesji GG_SESSION_USER3.
 */
static void reaper_user3(struct gg_exec *e)
{
	if (e->buf)
		string_free(e->buf, 1);

	xfree(e);
}

/*
 * usuwanie sesji wyszukiwania.
 */
static void reaper_search(struct gg_http *s)
{
	gg_search_request_free((struct gg_search_request*) s->user_data);
	gg_search_free(s);
}

static void get_line_from_pipe(struct gg_exec *c);

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
	{ GG_SESSION_SEARCH, (VV) handle_search, (VV) reaper_search },
	{ GG_SESSION_REGISTER, (VV) handle_pubdir, (VV) gg_register_free },
	{ GG_SESSION_UNREGISTER, (VV) handle_pubdir, (VV) gg_pubdir_free },
	{ GG_SESSION_PASSWD, (VV) handle_pubdir, (VV) gg_change_passwd_free },
	{ GG_SESSION_REMIND, (VV) handle_pubdir, (VV) gg_remind_passwd_free },
	{ GG_SESSION_CHANGE, (VV) handle_pubdir, (VV) gg_change_pubdir_free },
	{ GG_SESSION_USERLIST_GET, (VV) handle_userlist, (VV) gg_userlist_get_free },
	{ GG_SESSION_USERLIST_PUT, (VV) handle_userlist, (VV) gg_userlist_put_free },
	{ GG_SESSION_USER0, NULL, (VV) reaper_user },
	{ GG_SESSION_USER1, NULL, (VV) reaper_user },
	{ GG_SESSION_USER2, (VV) handle_voice, (VV) reaper_user },
	{ GG_SESSION_USER3, (VV) get_line_from_pipe, (VV) reaper_user3 },
	{ GG_SESSION_USER4, (VV) get_line_from_pipe, (VV) reaper_user3 },
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
static void get_char_from_pipe(struct gg_common *c)
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
 * get_line_from_pipe()
 *
 * funkcja pobiera z potoku steruj±cego znak do bufora, a gdy dojdzie
 * do konca linii puszcza na ekran.
 *
 * - c - struktura steruj±ca przechowuj±ca m.in. deskryptor potoku.
 */
static void get_line_from_pipe(struct gg_exec *c)
{
	char buf[1024];
	int ret;
  
	if (!c)
  		return;

	if ((ret = read(c->fd, buf, sizeof(buf) - 1)) > 0) {
		char *tmp;

		buf[ret] = 0;
		string_append(c->buf, buf);

		while ((tmp = strchr(c->buf->str, '\n'))) {
			int index = tmp - c->buf->str;
			char *line = xstrmid(c->buf->str, 0, index);
			string_t new;
			
			if (line[strlen(line) - 1] == '\r')
				line[strlen(line) - 1] = 0;

			if (c->id) {
				if (c->type == GG_SESSION_USER3)
					print("exec", line, itoa(c->id));
			} else
				print_window("debug", 0, "debug", line);

			new = string_init(c->buf->str + index + 1);
			string_free(c->buf, 1);
			c->buf = new;
			xfree(line);
		}
	}

	if ((ret == -1 && errno != EAGAIN) || ret == 0) {
		if (c->buf->len) {
			if (c->id) {
				switch (c->type) {
					case GG_SESSION_USER3:
						print("exec", c->buf->str, itoa(c->id));
						break;
					case GG_SESSION_USER4:
						check_mail_update(atoi(c->buf->str));
						break;
				}
			} else
				print_window("debug", 0, "debug", c->buf->str);
		}
		string_free(c->buf, 1);
		list_remove(&watches, c, 1);
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
					ui_event(t->command, NULL);
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
						xfree(h->user_data);
						list_remove(&watches, h, 0);
						gg_free_search(h);
						break;

					case GG_SESSION_REGISTER:
						if (!errmsg)
							errmsg = "register_timeout";
					case GG_SESSION_UNREGISTER:
						if (!errmsg)
							errmsg = "unregister_timeout";
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
						if (h->type == GG_SESSION_REGISTER || h->type == GG_SESSION_PASSWD) {
							xfree(reg_password);
							reg_password = NULL;
							xfree(reg_email);
							reg_email = NULL;
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
			if (sess && config_auto_away && (away == 0 || away == 4) && time(NULL) - last_action > config_auto_away && sess->state == GG_STATE_CONNECTED)
				change_status(GG_STATUS_BUSY, NULL, config_auto_away);

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

					xfree(p->name);
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

				if (c->type == GG_SESSION_USER0) {
					if (config_auto_back == 2 && (away == 1 || away == 3)) 
						change_status(GG_STATUS_AVAIL, NULL, 1);

					if (config_auto_back == 2)
						unidle();

					return;
				}

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

static void sigusr1_handler()
{
	event_check(EVENT_SIGUSR1, 1, "SIGUSR1");
	signal(SIGUSR1, sigusr1_handler);
}

static void sigusr2_handler()
{
	event_check(EVENT_SIGUSR2, 1, "SIGUSR2");
	signal(SIGUSR1, sigusr2_handler);
}

static void sighup_handler()
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

static void kill_ioctld()
{
        if (ioctld_pid > 0 && ekg_pid == getpid())
                kill(ioctld_pid, SIGINT);
}

static void sigsegv_handler()
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
static char *prepare_batch_line(int argc, char *argv[], int n)
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

extern FILE *gg_debug_file;

static void setup_debug()
{
	struct gg_exec se;
	int fd[2];

	if (pipe(fd) == -1)
		return;

	memset(&se, 0, sizeof(se));

	se.fd = fd[0];
	se.check = GG_CHECK_READ;
	se.state = GG_STATE_READING_DATA;
	se.type = GG_SESSION_USER3;
	se.id = 0;
	se.timeout = -1;
	se.buf = string_init(NULL);

	fcntl(fd[0], F_SETFL, O_NONBLOCK);
	fcntl(fd[1], F_SETFL, O_NONBLOCK);
	
	gg_debug_file = fdopen(fd[1], "w");
	setbuf(gg_debug_file, NULL);		/* XXX leak */

	list_add(&watches, &se, sizeof(se));
}

/*
 * ekg_ui_set()
 *
 * w³±cza interfejs o podanej nazwie.
 */
static int ekg_ui_set(const char *name)
{
	if (!name)
		return 0;

	if (!strcasecmp(optarg, "none"))
		ui_init = ui_none_init;
	else if (!strcasecmp(optarg, "batch"))
		ui_init = ui_batch_init;
	else if (!strcasecmp(optarg, "automaton"))
		ui_init = ui_automaton_init;
#ifdef WITH_UI_READLINE
	else if (!strcasecmp(optarg, "readline"))
		ui_init = ui_readline_init;
#endif
#ifdef WITH_UI_NCURSES
	else if (!strcasecmp(optarg, "ncurses"))
		ui_init = ui_ncurses_init;
#endif
	else
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	int auto_connect = 1, force_debug = 0, new_status = 0, ui_set = 0;
	int c = 0, set_private = 0;
	char *load_theme = NULL, *new_reason = NULL;
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
		{ "debug", no_argument, 0, 'd' },
		{ "no-auto", no_argument, 0, 'n' },
		{ "control-pipe", no_argument, 0, 'c' },
		{ "frontend", required_argument, 0, 'f' },
		{ "help", no_argument, 0, 'h' },
		{ "ioctld-path", required_argument, 0, 'I' },
		{ "no-pipe", no_argument, 0, 'o' },
		{ "theme", required_argument, 0, 't' },
		{ "user", required_argument, 0, 'u' },
		{ "version", no_argument, 0, 'v' },
		{ 0, 0, 0, 0 }
	};

#ifdef WITH_UI_READLINE
	ui_init = ui_readline_init;
#else
	ui_init = ui_ncurses_init;
#endif

#ifdef WITH_FORCE_NCURSES
	ui_init = ui_ncurses_init;
#endif 

	ekg_ui_set(getenv("EKG_UI"));
	ekg_ui_set(getenv("EKG_FRONTEND"));

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

	while ((c = getopt_long(argc, argv, "b::a::i::pdnc:f:hI:ot:u:v", ekg_options, NULL)) != -1) {
		switch (c) {
			case 'b':
				new_status = (optarg) ? GG_STATUS_AVAIL_DESCR : GG_STATUS_AVAIL;
				xfree(new_reason);
				new_reason = xstrdup(optarg);
			        break;
			case 'a':
				new_status = (optarg) ? GG_STATUS_BUSY_DESCR : GG_STATUS_BUSY;
				xfree(new_reason);
				new_reason = xstrdup(optarg);
			        break;
			case 'i':
				new_status = (optarg) ? GG_STATUS_INVISIBLE_DESCR : GG_STATUS_INVISIBLE;
				xfree(new_reason);
				new_reason = xstrdup(optarg);
			        break;
			case 'p':
				set_private = 1;
			        break;
			case 'd':
				force_debug =1;
				break;
			case 'n':
				auto_connect = 0;
				break;
			case 'h':
				printf(""
"u¿ycie: %s [OPCJE] [KOMENDY]\n"
"  -u, --user=NAZWA           korzysta z profilu u¿ytkownika o podanej nazwie\n"
"  -t, --theme=PLIK           ³aduje opis wygl±du z podanego pliku\n"
"  -c, --control-pipe=PLIK    potok nazwany sterowania\n"
"  -n, --no-auto              nie ³±czy siê automatycznie z serwerem\n"
"  -a, --away[=OPIS]          domy¶lnie zmienia stan na ,,zajêty''\n"
"  -b, --back[=OPIS]          domy¶lnie zmienia stan na ,,dostêpny''\n"
"  -i, --invisible[=OPIS]     domy¶lnie zmienia stan na ,,niewidoczny''\n"
"  -p, --private              domy¶lnie ustawia tryb ,,tylko dla przyjació³''\n"
"  -d, --debug                w³±cza wy¶wietlanie dodatkowych informacji\n"
"  -v, --version              wy¶wietla wersje programu i wychodzi\n"
#ifdef WITH_IOCTLD
"  -I, --ioctld-path=¦CIE¯KA  ustawia ¶cie¿kê do ioctld\n"
#endif
"  -f, --frontend=NAZWA       wybiera jeden z dostêpnych interfejsów\n"
"                             (none, batch, automaton"
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
				config_user = optarg;
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
			    	printf("ekg-%s\nlibgadu-%s (headers %s, protocol 0x%.2x, client \"%s\")\n", VERSION, gg_libgadu_version(), GG_LIBGADU_VERSION, GG_DEFAULT_PROTOCOL_VERSION, GG_DEFAULT_CLIENT_VERSION);
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
				/* obs³ugiwane przez getopt */
				fprintf(stdout, "Aby uzyskaæ wiêcej informacji uruchom program z opcj± --help.\n");
				return 1;
			default:
				break;
		}
	}

	if (set_private)
		new_status |= GG_STATUS_FRIENDS_MASK;

	if (optind < argc) {
		batch_line = prepare_batch_line(argc, argv, optind);
		batch_mode = 1;
		
		if (!ui_set)
			ui_init = ui_batch_init;
	}

        ekg_pid = getpid();

	mesg_startup = mesg_set(2);

#ifdef WITH_PYTHON
	python_initialize();
#endif

	theme_init();

	ui_screen_width = getenv("COLUMNS") ? atoi(getenv("COLUMNS")) : 80;
	ui_screen_height = getenv("LINES") ? atoi(getenv("LINES")) : 24;
	ui_init();
	ui_event("theme_init");

	config_timestamp = xstrdup("%H:%M ");

	in_autoexec = 1;
        userlist_read();
	update_status();
	config_read();
	sysmsg_read();
	emoticon_read();
	msg_queue_read();
	in_autoexec = 0;

	ui_event("xterm_update");
	
#ifdef WITH_IOCTLD
	if (!batch_mode) {
		sock_path = prepare_path(".socket", 1);
	
		if (!(ioctld_pid = fork())) {
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

	if (new_reason) {
		xfree(config_reason);
		config_reason = new_reason;
	}

	switch (config_status & ~GG_STATUS_FRIENDS_MASK) {
		case GG_STATUS_AVAIL:
			away = 0;
			break;
		case GG_STATUS_AVAIL_DESCR:
			away = 4;
			break;
		case GG_STATUS_BUSY:
			away = 1;
			break;
		case GG_STATUS_BUSY_DESCR:
			away = 3;
			break;
		case GG_STATUS_INVISIBLE:
			away = 2;
			break;
		case GG_STATUS_INVISIBLE_DESCR:
			away = 5;
			break;
	}
	
	if ((config_status & GG_STATUS_FRIENDS_MASK))
		private_mode = 1;
	
	/* czy w³±czyæ debugowanie? */	
	setup_debug();
	
	if (force_debug || gg_debug_level || config_debug) {
		gg_debug_level = 255;
		config_debug = 1;
	}
	
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

	if (!batch_mode && config_display_welcome)
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

#ifdef HAVE_OPENSSL
	SIM_KC_Init();
	strncpy(SIM_Key_Path, prepare_path("keys/", 0), sizeof(SIM_Key_Path));
#endif

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
	char **vars = NULL;
	list_t l;
	int i;

	ekg_logoff(sess, NULL);
	list_remove(&watches, sess, 0);
	gg_free_session(sess);
	sess = NULL;

	ui_deinit();

	msg_queue_write();

	if (config_keep_reason) {
		array_add(&vars, xstrdup("status"));
		array_add(&vars, xstrdup("reason"));
	}

	if (config_server_save)
		array_add(&vars, xstrdup("server"));

	if (vars) {
		config_write_partly(vars);
		array_free(vars);
	}

	if (config_changed && !config_speech_app) {
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
	
	msg_queue_free();
	alias_free();
	conference_free();
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
	xfree(gg_proxy_username);
	xfree(gg_proxy_password);
	xfree(config_dir);

#ifdef WITH_PYTHON
	python_finalize();
#endif

#ifdef HAVE_OPENSSL
	SIM_KC_Finish();
#endif

	/* kapitan schodzi ostatni */
	if (gg_debug_file) {
		fclose(gg_debug_file);
		gg_debug_file = NULL;
	}

	if (config_mesg_allow != mesg_startup)
		mesg_set(mesg_startup);

	exit(0);
}

