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
#include <unistd.h>
#include <pwd.h>
#include <sys/time.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "libgg.h"
#include "stuff.h"
#include "commands.h"
#include "events.h"
#include "themes.h"
#include "version.h"

time_t last_action = 0;

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

		for (l = watches; l; l = l->next) {
			struct gg_common *w = l->data;

			if (w->state == GG_STATE_ERROR || w->state == GG_STATE_IDLE || w->state == GG_STATE_DONE)
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
		
		if (display_debug) {
			gg_debug_level = 255;
		} else 
			gg_debug_level = 0;

		ret = select(maxfd + 1, &rd, &wd, NULL, &tv);
	
		if (!ret) {
			/* timeout reconnectu */
			if (!sess && reconnect_timer && time(NULL) - reconnect_timer >= auto_reconnect && config_uin && config_password) {
				reconnect_timer = 0;
				my_printf("connecting");
				connecting = 1;
				if (!(sess = gg_login(config_uin, config_password, 1))) {
					my_printf("conn_failed", strerror(errno));
					do_reconnect();
				} else {
					sess->initial_status = default_status;
				}
				list_add(&watches, sess, 0);
			}

			/* timeout pinga */
			if (sess && sess->state == GG_STATE_CONNECTED && time(NULL) - last_ping > 60) {
				if (last_ping)
					gg_ping(sess);
				last_ping = time(NULL);
			}

			/* timeout autoawaya */
			if (sess && auto_away && !away && time(NULL) - last_action > auto_away && sess->state == GG_STATE_CONNECTED) {
				char tmp[16];
				
				away = 1;
				reset_prompt();
				gg_change_status(sess, GG_STATUS_BUSY | (private_mode ? GG_STATUS_FRIENDS_MASK : 0));
				
				if (!(auto_away % 60))
					snprintf(tmp, sizeof(tmp), "%dm", auto_away / 60);
				else
					snprintf(tmp, sizeof(tmp), "%ds", auto_away);
				
				my_printf("auto_away", tmp);
			}

			/* przegl±danie zdech³ych dzieciaków */
			while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
				for (l = children; l; l = m) {
					struct process *p = l->data;
					char buf1[10], buf2[10];

					m = l->next;

					if (pid != p->pid)
						continue;

					if (p->name[0] == '\001') {
						my_printf((!(WEXITSTATUS(status))) ? "sms_sent" : "sms_failed", p->name + 1);
					} else if (p->name[0] == '\002') {
						// do nothing
					} else {	
						snprintf(buf1, sizeof(buf1), "%d", p->pid);
						snprintf(buf2, sizeof(buf2), "%d", WEXITSTATUS(status));
						my_printf("process_exit", buf1, p->name, buf2);
					}

					free(p->name);
					list_remove(&children, p, 1);
				}
			}

		} else {
			for (l = watches; l; l = l->next) {
				struct gg_session *s = l->data;
				struct gg_common *c = l->data;
				struct gg_http *h = l->data;
				char *errmsg;

				if (!FD_ISSET(c->fd, &rd) && !FD_ISSET(c->fd, &wd))
					continue;

				errmsg = NULL;
				
				switch (c->type) {
					case GG_SESSION_USER0:
						return rl_getc(stdin);
					
					case GG_SESSION_GG:
						handle_event(s);
						break;

					case GG_SESSION_SEARCH:
						if (gg_search_watch_fd(h) || h->state == GG_STATE_ERROR) {
							my_printf("search_failed", strerror(errno));
							free(h->user_data);
							list_remove(&watches, h, 0);
							gg_free_search(h);
							break;
						}
						if (h->state == GG_STATE_DONE) {	
							gg_debug(GG_DEBUG_MISC, "++ gg_search()... done\n");
							handle_search(h);
							free(h->user_data);
							list_remove(&watches, h, 0);
							gg_free_search(h);
							break;
						}
						break;

					case GG_SESSION_REGISTER:
						if (!errmsg)
							errmsg = "register_failed";
					case GG_SESSION_PASSWD:
						if (!errmsg)
							errmsg = "passwd_failed";
					case GG_SESSION_REMIND:
						if (!errmsg)
							errmsg = "remind_failed";
					case GG_SESSION_CHANGE:
						if (!errmsg)
							errmsg = "change_failed";

						if (gg_pubdir_watch_fd(h) || s->state == GG_STATE_ERROR) {
							my_printf(errmsg, strerror(errno));
							list_remove(&watches, h, 0);
							gg_free_pubdir(h);
							if (h->type == GG_SESSION_REGISTER) {
								free(reg_password);
								reg_password = NULL;
							}
							break;
						}
						
						if (s->state == GG_STATE_DONE) {	
							handle_pubdir(h);
							list_remove(&watches, h, 0);
							gg_free_pubdir(h);
							if (h->type == GG_SESSION_REGISTER) {
								free(reg_password);
								reg_password = NULL;
							}
							break;
						}
					}
				break;
			}
		}
	}
	
	return -1;
}

void sigcont()
{
	rl_forced_update_display();
	signal(SIGCONT, sigcont);
}

void sighup()
{
	if (sess && sess->state != GG_STATE_IDLE) {
		my_printf("disconected");
		gg_logoff(sess);
		list_remove(&watches, sess, 0);
		gg_free_session(sess);
		sess = NULL;
	}
	
	signal(SIGHUP, sighup);
}

int main(int argc, char **argv)
{
	int auto_connect = 1, i, new_status = 0;
	char *home = getenv("HOME"), *load_theme = NULL;
	struct passwd *pw; 
	struct gg_common si;
	
	config_user = "";

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			printf("\
u¿ycie: %s [OPCJE]
  -u, --user [NAZWA]   korzysta z profilu u¿ytkownika o podanej nazwie
  -t, --theme [PLIK]   ³aduje opis wygl±du z podanego pliku
  -n, --no-auto        nie ³±czy siê automatycznie z serwerem
  -a, --away           po po³±czeniu zmienia stan na ,,zajêty''
  -b, --back           po po³±czeniu zmienia stan na ,,dostêpny''
  -i, --invisible      po po³±czeniu zmienia stan na ,,niewidoczny''
  -p, --private        po po³±czeniu zmienia stan na ,,tylko dla przyjació³''
  -d, --debug          w³±cza wy¶wietlanie dodatkowych informacji

", argv[0]);
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
			gg_debug_level = 255;
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
		if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--theme"))
			load_theme = argv[++i];
	}
	
	read_config(NULL);

	if (new_status)
		default_status = new_status;

	switch (default_status & ~GG_STATUS_FRIENDS_MASK) {
		case GG_STATUS_AVAIL:
			away = 0;
			break;
		case GG_STATUS_BUSY:
			away = 1;
			break;
		case GG_STATUS_INVISIBLE:
			away = 2;
			break;
	}
	
	if ((default_status & GG_STATUS_FRIENDS_MASK))
		private_mode = 1;
	
	if (gg_debug_level)
		display_debug = 1;
	if (display_debug)
		gg_debug_level = 255;
	
	init_theme();
	if (load_theme)
		read_theme(load_theme, 1);
	else
		if (default_theme)
			read_theme(default_theme, 1);
	
	signal(SIGCONT, sigcont);
	signal(SIGHUP, sighup);
	signal(SIGALRM, SIG_IGN);

	time(&last_action);

	/* dodajemy stdin do ogl±danych deskryptorów */
	si.fd = 0;
	si.check = GG_CHECK_READ;
	si.state = GG_STATE_READING_DATA;
	si.type = GG_SESSION_USER0;
	si.id = 0;
	list_add(&watches, &si, sizeof(si));

	rl_initialize();
	rl_getc_function = my_getc;
	rl_readline_name = "gg";
	rl_attempted_completion_function = (CPPFunction *) my_completion;
	rl_completion_entry_function = (void*) empty_generator;

	if (load_theme)
		read_theme(load_theme, 1);
	
	my_printf("welcome", VERSION);
	
	if (!config_uin || !config_password)
		my_printf("no_config");

	read_userlist(NULL);
		
	if (!log_path) {
	    if (!home) { pw = getpwuid(getuid()); home = pw->pw_dir; }
	    if (config_user != "") {
		log_path = gg_alloc_sprintf("%s/.gg/%s/history", home, config_user);
	    } else {
		log_path = gg_alloc_sprintf("%s/.gg/history", home);
	    }
	}

	if (config_uin && config_password && auto_connect) {
		my_printf("connecting");
		connecting = 1;
		if (!(sess = gg_login(config_uin, config_password, 1))) {
			my_printf("conn_failed", strerror(errno));
			do_reconnect();
		} else {
			sess->initial_status = default_status;
		}
		list_add(&watches, sess, 0);
	}

	for (;;) {
		char *line, *tmp = NULL;
		
		if (!(line = my_readline()))
			break;

		if (strcmp(line, ""))
			add_history(line);

		if ((tmp = is_alias(line))) {
			free(line);
			line = tmp;
		}

		if (execute_line(line)) {
			free(line);
			break;
		}
		
		free(line);
	}

	printf("\n");
	gg_logoff(sess);
	list_remove(&watches, sess, 0);
	gg_free_session(sess);
	sess = NULL;

	if (config_changed) {
		char *line;

		my_printf("config_changed");
		no_prompt = 1;
		if ((line = my_readline())) {
			if (!strcasecmp(line, "tak") || !strcasecmp(line, "yes") || !strcasecmp(line, "t") || !strcasecmp(line, "y")) {
				if (!write_userlist(NULL) && !write_config(NULL))
					my_printf("saved");
				else
					my_printf("error_saving");

			}
		} else
			printf("\n");
	}

	free(log_path);	
	return 0;
}

