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
#include <sys/time.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <readline.h>
#include <history.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "config.h"
#include "libgadu.h"
#include "stuff.h"
#include "commands.h"
#include "events.h"
#include "themes.h"
#include "version.h"
#include "userlist.h"
#include "vars.h"

time_t last_action = 0;
int ioctl_daemon_pid = 0;
int ekg_pid = 0;

/*
 * struktura zawieraj±ca adresy funkcji obs³uguj±cych ró¿ne sesje
 */
static struct {
	int type;
	void (*handler)(void*);
} handlers[] = {
	{ GG_SESSION_GG, (void (*)(void*)) handle_event, },
	{ GG_SESSION_DCC, (void (*)(void*)) handle_dcc, },
	{ GG_SESSION_DCC_SOCKET, (void (*)(void*)) handle_dcc, },
	{ GG_SESSION_DCC_SEND, (void (*)(void*)) handle_dcc, },
	{ GG_SESSION_DCC_GET, (void (*)(void*)) handle_dcc, },
	{ GG_SESSION_DCC_VOICE, (void (*)(void*)) handle_dcc, },
	{ GG_SESSION_SEARCH, (void (*)(void*)) handle_search, },
	{ GG_SESSION_REGISTER, (void (*)(void*)) handle_pubdir, },
	{ GG_SESSION_PASSWD, (void (*)(void*)) handle_pubdir, },
	{ GG_SESSION_REMIND, (void (*)(void*)) handle_pubdir, },
	{ GG_SESSION_CHANGE, (void (*)(void*)) handle_pubdir, },
	{ GG_SESSION_USERLIST_GET, (void (*)(void*)) handle_userlist, },
	{ GG_SESSION_USERLIST_PUT, (void (*)(void*)) handle_userlist, },
	{ GG_SESSION_USER2, (void (*)(void*)) handle_voice, },
	{ -1, NULL, }, 
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
int get_char_from_pipe(struct gg_common *c)
{
	static char buf[PIPE_MSG_MAX_BUF_LEN + 1];
	char ch;
	int ret = 0;
  
	if (!c)
  		return 0;

	if (read(c->fd, &ch, 1) > 0) {
		if (ch != '\n') {
			if (strlen(buf) < PIPE_MSG_MAX_BUF_LEN)
				buf[strlen(buf)] = ch;
		}
		if (ch == '\n' || (strlen(buf) >= PIPE_MSG_MAX_BUF_LEN)) {
			ret = execute_line(buf);
			memset(buf, 0, PIPE_MSG_MAX_BUF_LEN + 1);
		}
	}
	return ret;
}

/*
 * my_getc()
 *
 * funkcja wywo³ywana przez readline() do odczytania znaku. my przy okazji
 * bêdziemy sprawdzaæ, czy co¶ siê nie zmieni³o na innych deskryptorach,
 * by móc obs³ugiwaæ ruch podczas wprowadzania danych z klawiatury.
 *
 *  - f - plik, z którego czytamy. readline() podaje stdin.
 *
 * zwraca wczytany znak, przy okazji wykonuj±c, co trzeba.
 */
int my_getc(FILE *f)
{
	static time_t last_ping = 0;
	struct timeval tv;
	struct list *l, *m;
	fd_set rd, wd;
	int ret, maxfd, pid, status;

	for (;;) {
		FD_ZERO(&rd);
		FD_ZERO(&wd);
		
		maxfd = 0;

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
						my_printf("conn_timeout");
						list_remove(&watches, s, 0);
						gg_free_session(s);
						sess = NULL;
						do_reconnect();
						break;

					case GG_SESSION_SEARCH:
						my_printf("search_timeout");
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

						my_printf(errmsg);
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
						my_printf("dcc_timeout");
						list_remove(&watches, d, 0);
						gg_free_dcc(d);
						break;
				}
				break;
			}
			
			/* timeout reconnectu */
			if (!sess && reconnect_timer && time(NULL) - reconnect_timer >= config_auto_reconnect && config_uin && config_password) {
				reconnect_timer = 0;
				my_printf("connecting");
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
				    	char *path = prepare_path("away.reasons");

					reason = get_random_reason(path);
					if (!reason && config_away_reason)
					    	reason = strdup(config_away_reason);
				}
				else if (config_away_reason)
				    	reason = strdup(config_away_reason);
				
				away = (reason) ? 3 : 1;
				reset_prompt();

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
				
				my_printf((reason) ? "auto_away_descr" : "auto_away", tmp, reason);
				free(busy_reason);
				busy_reason = reason;
			}

			/* auto save */
			if (config_changed && config_auto_save && time(NULL) - last_save > config_auto_save) {
				last_save = time(NULL);
				gg_debug(GG_DEBUG_MISC, "-- autosaving userlist and config after %d seconds.\n", config_auto_save);

				if (!userlist_write(NULL) && !config_write(NULL)) {
					config_changed = 0;
					my_printf("autosaved");
				} else
					my_printf("error_saving");
			}

			/* przegl±danie zdech³ych dzieciaków */
			while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
				for (l = children; l; l = m) {
					struct process *p = l->data;

					m = l->next;

					if (pid != p->pid)
						continue;

					if (p->name[0] == '\001') {
						my_printf((!(WEXITSTATUS(status))) ? "sms_sent" : "sms_failed", p->name + 1);
					} else if (p->name[0] == '\002') {
						// do nothing
					} else {	
						my_printf("process_exit", itoa(p->pid), p->name, itoa(WEXITSTATUS(status)));
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
					return rl_getc(f);

				if (c->type == GG_SESSION_USER1) {
					if (get_char_from_pipe(c)) {
						immediately_quit = 1;
						return '\n';
					}
					break;
				}

				if (c->type == GG_SESSION_USER2) {
					handle_voice();
					break;
				}

				for (i = 0; handlers[i].type != -1; i++)
					if (c->type == handlers[i].type) {
						(handlers[i].handler)(c);
						break;
					}

				if (handlers[i].type == -1) {
					printf("\n*** FATAL ERROR: Unknown session type. Application may fail.\n*** Mail this to the authors:\n*** ekg-" VERSION "/type=%d/state=%d/id=%d\n", c->type, c->state, c->id);
					list_remove(&watches, c, 1);
					break;
				}
				
				break;
			}
		}
	}
	
	return -1;
}

void sigusr1_handler()
{
	check_event(EVENT_SIGUSR1, 1, "SIGUSR1");
	signal(SIGUSR1, sigusr1_handler);
}

void sigusr2_handler()
{
	check_event(EVENT_SIGUSR2, 1, "SIGUSR2");
	signal(SIGUSR1, sigusr2_handler);
}

void sigcont_handler()
{
	rl_forced_update_display();
	signal(SIGCONT, sigcont_handler);
}

void sighup_handler()
{
	if (sess && sess->state != GG_STATE_IDLE) {
		my_printf("disconected");
		ekg_logoff(sess, NULL);
		list_remove(&watches, sess, 0);
		gg_free_session(sess);
		sess = NULL;
	}
	
	signal(SIGHUP, sighup_handler);
}

void kill_ioctl_daemon()
{
        if (ioctl_daemon_pid > 0 && ekg_pid == getpid())
                kill(ioctl_daemon_pid, SIGINT);
}

void sigsegv_handler()
{
	signal(SIGSEGV, SIG_DFL);
	
	kill_ioctl_daemon();
	
	fprintf(stderr, "\n\
*** Naruszenie ochrony pamiêci ***\n\
\n\
Próbujê zapisaæ ustawienia do pliku %s/config.%d i listê kontaktów\n\
do pliku %s/userlist.%d, ale nie obiecujê, ¿e cokolwiek z tego\n\
wyjdzie.\n\
\n\
Je¶li zostanie utworzony plik %s/core, spróbuj uruchomiæ program ,,gdb''\n\
zgodnie z instrukcjami zawartymi w pliku README. Dziêki temu autorzy\n\
dowiedz± siê, w którym miejscu wyst±pi³ b³±d i najprawdopodobniej pozwoli\n\
to unikn±æ tego typu b³êdów w przysz³o¶ci.\n\
\n", config_dir, getpid(), config_dir, getpid(), config_dir);

	config_write_crash();
	userlist_write_crash();

	raise(SIGSEGV);			/* niech zrzuci core */
}

void sigwinch_handler()
{
#ifdef HAVE_RL_GET_SCREEN_SIZE
	rl_get_screen_size(&screen_lines, &screen_columns);
#endif
}

void sigint_handler()
{
	rl_delete_text(0, rl_end);
	rl_point = rl_end = 0;
	putchar('\n');
	rl_forced_update_display();
	signal(SIGINT, sigint_handler);
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

	bl = malloc(m);
	if (!bl)
		return NULL;

	for (i = n; i < argc; i++) {
		strcat(bl, argv[i]);
		if (i < argc - 1)
			strcat(bl, " ");
	}

	return bl;
}

int main(int argc, char **argv)
{
	int auto_connect = 1, force_debug = 0, i, new_status = 0 ;
	char *load_theme = NULL;
	char *pipe_file = NULL;
#ifdef WITH_IOCTLD
    	char *sock_path = NULL, *ioctl_daemon_path = IOCTLD_PATH;
#	define IOCTLD_HELP "  -I, --ioctld-path [¦CIE¯KA]    ustawia ¶cie¿kê do ioctl_daemon-a\n"
#else
#	define IOCTLD_HELP ""
#endif
	struct list *l;
	struct passwd *pw; 
	struct gg_common si;
	
	variable_init();

	if (!(home_dir = getenv("HOME")))
		if ((pw = getpwuid(getuid())))
			home_dir = pw->pw_dir;

	if (home_dir)
		home_dir = strdup(home_dir);

	if (!home_dir) {
		fprintf(stderr, "Nie mogê znale¼æ katalogu domowego. Popro¶ administratora, ¿eby to naprawi³.\n");
		return 1;
	}

	if (getenv("CONFIG_DIR"))
	    config_dir = saprintf("%s/%s/gg", home_dir, getenv("CONFIG_DIR"));
	else
	    config_dir = saprintf("%s/.gg", home_dir);

	signal(SIGSEGV, sigsegv_handler);
	signal(SIGCONT, sigcont_handler);
	signal(SIGHUP, sighup_handler);
	signal(SIGINT, sigint_handler);
	signal(SIGUSR1, sigusr1_handler);
	signal(SIGUSR2, sigusr2_handler);
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	if (screen_lines < 1)
		screen_lines = 24;

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
IOCTLD_HELP
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
		if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--theme"))
			load_theme = argv[++i];
		if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
		    	printf("EKG - Eksperymentalny Klient Gadu-Gadu (%s)\n", VERSION);
			return 0;
		}
#ifdef WITH_IOCTLD
                if (!strcmp(argv[i], "-I") || !strcmp(argv[i], "--ioctld-path")) {
                        if (argv[i+1]) {
                                ioctl_daemon_path = argv[i+1];
                                i++;
                        } else {
                                fprintf(stderr, "Nie podano ¶cie¿ki do ioctl_daemon-a.\n");
                                return 1;
                        }
                }
#endif
	}
	if (i < argc && *argv[i] != '-') {
		batch_line = prepare_batch_line(argc, argv, i);
		if (!batch_line) {
			fprintf(stderr, "Nie mogê zaalokowac pamiêci dla polecenia wsadowego");
			return 1;
		}
		batch_mode = 1;
	}
	
        ekg_pid = getpid();

	init_theme();

	in_autoexec = 1;
        userlist_read(NULL);
	config_read(NULL);
	read_sysmsg(NULL);
	emoticon_read();
	in_autoexec = 0;

#ifdef WITH_IOCTLD
	if (!batch_mode) {
	        sock_path = prepare_path(".socket");
	
	        if (!(ioctl_daemon_pid = fork())) {
			execl(ioctl_daemon_path, "ioctl_daemon", sock_path, NULL);
			exit(0);
		}
	
		init_socket(sock_path);
	
	        atexit(kill_ioctl_daemon);
	}
#endif // WITH_IOCTLD

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
		read_theme(load_theme, 1);
	else
		if (config_theme)
			read_theme(config_theme, 1);
	
	reset_theme_cache();
		
	time(&last_action);

	/* dodajemy stdin do ogl±danych deskryptorów */
	if (!batch_mode) {
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
		si.fd = pipe_fd;
		si.check = GG_CHECK_READ;
		si.state = GG_STATE_READING_DATA;
		si.type = GG_SESSION_USER1;
		si.id = 0;
		si.timeout = -1;
		list_add(&watches, &si, sizeof(si));
	}

	/* inicjujemy readline */
	rl_initialize();
	rl_getc_function = my_getc;
	rl_readline_name = "gg";
	rl_attempted_completion_function = (CPPFunction *) my_completion;
	rl_completion_entry_function = (void*) empty_generator;
#ifdef HAVE_RL_SET_KEY
	rl_set_key("\033[[A", binding_help, rl_get_keymap());
	rl_set_key("\033OP", binding_help, rl_get_keymap());
	rl_set_key("\033[11~", binding_help, rl_get_keymap());
	rl_set_key("\033[M", binding_help, rl_get_keymap());
	rl_set_key("\033[[B", binding_quick_list, rl_get_keymap());
	rl_set_key("\033OQ", binding_quick_list, rl_get_keymap());
	rl_set_key("\033[12~", binding_quick_list, rl_get_keymap());
	rl_set_key("\033[N", binding_quick_list, rl_get_keymap());
	
	rl_set_key("\033[24~", binding_toggle_debug, rl_get_keymap());
#endif
	
#ifdef HAVE_RL_GET_SCREEN_SIZE
#  ifdef SIGWINCH
	signal(SIGWINCH, sigwinch_handler);
#  endif
	rl_get_screen_size(&screen_lines, &screen_columns);
#endif
	
	if (!batch_mode)
		my_printf("welcome", VERSION);
	
	if (!config_uin || !config_password)
		my_printf("no_config");

	if (!config_log_path) {
		if (config_user != "")
			config_log_path = saprintf("%s/%s/history", config_dir, config_user);
		else
			config_log_path = saprintf("%s/history", config_dir);
	}
	
	changed_dcc("dcc");

	if (config_uin && config_password && auto_connect) {
		my_printf("connecting");
		connecting = 1;
		do_connect();
	}

	if (config_auto_save)
		last_save = time(NULL);
	
	for (;;) {
		char *line;
		
		if (!(line = my_readline())) {
			if (query_nick) {
				my_printf("query_finished", query_nick);
				free(query_nick);
				query_nick = NULL;
				continue;
			} else
				break;
		}

		if (strcmp(line, ""))
			add_history(line);
		else {
			if (batch_mode && !batch_line) {
				quit_message_send = 1;
				break;
			}
			free(line);
			continue;
		}
		my_printf_lines = 0;

		if (!query_nick && (l = alias_check(line))) {
			char *p = line;
			int quit = 0;

			while (*p != ' ' && *p)
				p++;
			
			for (; l && !quit; l = l->next) {
				char *tmp = saprintf("%s%s", (char*) l->data, p);
				if (tmp)
					if (execute_line(tmp)) 
						quit = 1;
				
				free(tmp);
			}
			if (quit) {
				free(line);
				break;
			}
		} else {
			if (execute_line(line)) {
				free(line);
				break;
			}
		}
		
		my_printf_lines = -1;
		
		free(line);
	}

	if (!batch_mode && !quit_message_send) {
		putchar('\n');
		my_printf("quit");
		putchar('\n');
		quit_message_send = 1;
	}

	ekg_logoff(sess, NULL);
	list_remove(&watches, sess, 0);
	gg_free_session(sess);
	sess = NULL;

	if (config_changed) {
		char *line, *prompt = find_format("config_changed");

		if ((line = readline(prompt))) {
			if (!strcasecmp(line, "tak") || !strcasecmp(line, "yes") || !strcasecmp(line, "t") || !strcasecmp(line, "y")) {
				if (!userlist_write(NULL) && !config_write(NULL))
					my_printf("saved");
				else
					my_printf("error_saving");

			}
			free(line);
		} else
			printf("\n");
	}

	for (l = children; l; l = l->next) {
		struct process *p = l->data;

		kill(p->pid, SIGTERM);
	}

	if (pipe_fd > 0)
		close(pipe_fd);
	if (pipe_file)
		unlink(pipe_file);
	
	free(config_log_path);	
	return 0;
}

