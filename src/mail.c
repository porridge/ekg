/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Piotr Domagalski <szalik@szalik.net>
 *                          Pawe³ Maziarz <drg@infomex.pl>
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pwd.h>
#include <dirent.h>
#include <fcntl.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <errno.h>
#include "compat.h"
#include "dynstuff.h"
#include "mail.h"
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "xmalloc.h"

int config_check_mail = 0;
int config_check_mail_frequency = 15;
char *config_check_mail_folders = NULL;

int mail_count = 0;
int last_mail_count = 0;

list_t mail_folders = NULL;

/*
 * check_mail()
 *
 * wywo³uje odpowiednie sprawdzanie poczty.
 */
int check_mail()
{
	if (!config_check_mail)
		return -1;

	if (config_check_mail & 1)
		check_mail_mbox();
	else
		if (config_check_mail & 2)
			check_mail_maildir();

	return 0;
}

/*
 * check_mail_update()
 *
 * modyfikuje liczbê nowych emaili i daje o tym znaæ.
 */
int check_mail_update(const char *s, int more)
{
	int h = 0, c = 0, new_count = 0;
	char **buf = NULL;
	list_t l;

	if (!s)
		return 1;

	buf = array_make(s, ",", 0, 0, 0);

	if (!(buf[0] && buf[1]))
		return 1;

	h = atoi(buf[0]);
	c = atoi(buf[1]);

	array_free(buf);

	for (l = mail_folders; l; l = l->next) {
		struct mail_folder *m = l->data;

		if (m->fhash == h)
			m->count = c;

		new_count += m->count;
	}

	if (new_count == mail_count)
		return 1;

	last_mail_count = mail_count;
	mail_count = new_count;

	if (!more && mail_count && mail_count > last_mail_count) {
		if (config_check_mail & 4) {
			if (mail_count == 1)
				print("new_mail_one");
			else {
				if (mail_count >= 2 && mail_count <= 4)
					print("new_mail_two_four", itoa(mail_count));
				else
					print("new_mail_more", itoa(mail_count));
			}
		}

		if (config_beep && config_beep_mail)
			ui_beep();

		event_check(EVENT_NEW_MAIL, 1, NULL);
	}

	return 0;
}

/*
 * check_mail_mbox()
 *
 * tworzy dzieciaka, który sprawdza wszystkie pliki typu
 * mbox i liczy ile jest nowych wiadomo¶ci, potem zwraca
 * wynik rurk±. sprawdza tylko te pliki, które by³y
 * modyfikowane od czasu ostatniego sprawdzania.
 */
int check_mail_mbox()
{
	int fd[2], pid, to_check = 0;
	struct gg_exec x;
	list_t l;

	for (l = mail_folders; l; l = l->next) {
		struct mail_folder *m = l->data;
		struct stat st;

		/* plik móg³ zostaæ usuniêty, uaktualnijmy */
		if (stat(m->fname, &st) == -1) {
			if (m->count != 0) {
				char *buf = saprintf("%d,%d", m->fhash, 0);
				check_mail_update(buf, 0);
				xfree(buf);
			}	

			m->mtime = 0;
			m->size = 0;
			m->check = 0;
			m->count = 0;

			continue;
		}

		if ((st.st_mtime != m->mtime) || (st.st_size != m->size)) {
			m->mtime = st.st_mtime;
			m->size = st.st_size;
			m->check = 1;
			to_check++;
		} else
			m->check = 0;
	}

	if (!to_check || pipe(fd))
		return 1;

 	if ((pid = fork()) < 0) {
		close(fd[0]);
		close(fd[1]);
		return 1;
	}

	if (!pid) {	/* born to be wild */
		char *s = NULL, *line = NULL;
		int f_new = 0, new = 0, in_header = 0, i = 0;
		FILE *f;
		struct stat st;

		close(fd[0]);

		for (l = mail_folders; l; l = l->next) {
			struct mail_folder *m = l->data;

			if (!m->check)
				continue;

			i++;

			if ((stat(m->fname, &st) == -1) || !(f = fopen(m->fname, "r")))
				continue;

			while ((line = read_file(f))) {
				if (!strncmp(line, "From ", 5)) {
					in_header = 1;
					f_new++;
				}

				if (in_header && (!strncmp(line, "Status: RO", 10) || !strncmp(line, "Status: O", 9))) 
					f_new--;	

				strip_spaces(line);

				if (strlen(line) == 0)
					in_header = 0;

				xfree(line);
			}

			fclose(f);

#ifdef HAVE_UTIMES
			{
				struct timeval foo[1];

				foo[0].tv_sec = st.st_atime;
				foo[1].tv_sec = st.st_mtime;

				/* przecie¿ my nic nie ruszali¶my ;> */			
				utimes(m->fname, (const struct timeval *) &foo);
			}
#endif

			if (i == to_check)
				s = saprintf("%d,%d", m->fhash, f_new);
			else
				s = saprintf("%d,%d\n", m->fhash, f_new);

			{
				int sent = 0, left = strlen(s);
				char *ptr = s;

				while (left > 0) {
					sent = write(fd[1], ptr, sizeof(ptr));

					left -= sent;
					ptr += sent;
				}
			}

			xfree(s);

			new += f_new;
			f_new = 0;
		}

		close(fd[1]);
		exit(0);
	}

	x.fd = fd[0];
	x.check = GG_CHECK_READ;
	x.state = GG_STATE_READING_DATA;
	x.type = GG_SESSION_USER4;
	x.id = pid;
	x.timeout = 60;
	x.buf = string_init(NULL);

	fcntl(x.fd, F_SETFL, O_NONBLOCK);

	list_add(&watches, &x, sizeof(x));
	process_add(pid, "\002");

	close(fd[1]);

	return 0;
}

/*
 * check_mail_maildir()
 *
 * tworzy dzieciaka, który sprawdza wszystkie 
 * katalogi typu Maildir i liczy, ile jest w nich
 * nowych wiadomo¶ci. zwraca wynik rurk±.
 */
int check_mail_maildir()
{
	int fd[2], pid;
	struct gg_exec x;

	if (pipe(fd))
		return 1;

	if ((pid = fork()) < 0) {
		close(fd[0]);
		close(fd[1]);
		return 1;
	}

	if (!pid) {	/* born to be wild */
		int d_new = 0, new = 0;
		char *s = NULL;
		struct dirent *d;
		DIR *dir;
		list_t l;

		close(fd[0]);

		for (l = mail_folders; l; l = l->next) {
			struct mail_folder *m = l->data;
			char *tmp = saprintf("%s/%s", m->fname, "new");

			if (!(dir = opendir(tmp))) {
				xfree(tmp);
				continue;
			}

			while ((d = readdir(dir))) {
				char *fname = saprintf("%s/%s", tmp, d->d_name);
				struct stat st;

				if (d->d_name[0] != '.' && !stat(fname, &st) && S_ISREG(st.st_mode))
					d_new++;

				xfree(fname);
			}
	
			xfree(tmp);
			closedir(dir);

			if (!l->next)
				s = saprintf("%d,%d", m->fhash, d_new);
			else
				s = saprintf("%d,%d\n", m->fhash, d_new);

			{
				int sent = 0, left = strlen(s);
				char *ptr = s;

				while (left > 0) {
					sent = write(fd[1], ptr, sizeof(ptr));

					left -= sent;
					ptr += sent;
				}
			}

			xfree(s);

			new += d_new;
			d_new = 0;
		}

		close(fd[1]);
		exit(0);
	}

	x.fd = fd[0];
	x.check = GG_CHECK_READ;
	x.state = GG_STATE_READING_DATA;
	x.type = GG_SESSION_USER4;
	x.id = pid;
	x.timeout = 60;
	x.buf = string_init(NULL);

	fcntl(x.fd, F_SETFL, O_NONBLOCK);

	list_add(&watches, &x, sizeof(x));
	process_add(pid, "\002");

	close(fd[1]);

	return 0;
}

/*
 * changed_check_mail()
 *
 * wywo³ywane przy zmianie zmiennej check_mail.
 */
void changed_check_mail(const char *var)
{
	if (config_check_mail) {
		list_t l;
		struct timer *t;

		/* konieczne, je¶li by³a zmiana typu skrzynek */
		changed_check_mail_folders("check_mail_folders");

		for (l = timers; l; l = l->next) {
			t = l->data;

			if (!strcmp(t->name, "check-mail-time")) {
				t->period = config_check_mail_frequency;
				return;
			}
		}
		 
		t = timer_add(config_check_mail_frequency, "check-mail-time", "check_mail");
		t->ui = 1;
	} else
		timer_remove("check-mail-time", NULL);
}

/*
 * changed_check_mail_folders()
 *
 * wywo³ywane przy zmianie check_mail_folders.
 */
void changed_check_mail_folders(const char *var)
{
	struct mail_folder foo;

	check_mail_free();

	if (config_check_mail_folders) {
		char **f = NULL;
		int i;
		
		f = array_make(config_check_mail_folders, ", ", 0, 1, 0);

		for (i = 0; f[i]; i++) {
			if (f[i][0] != '/') {
				char *buf = saprintf("%s/%s", home_dir, f[i]);
				
				xfree(f[i]);
				f[i] = buf;
			}

			foo.fhash = ekg_hash(f[i]);
			foo.fname = f[i];
			foo.mtime = 0;
			foo.size = 0;
			foo.count = 0;
			foo.check = 1;

			list_add(&mail_folders, &foo, sizeof(foo));
		}

		xfree(f);
	}

	if (config_check_mail & 1) {
		char *inbox = xstrdup(getenv("MAIL"));

		if (!inbox) {
			struct passwd *pw = getpwuid(getuid());

			if (!pw)
				return;

			inbox = saprintf("%s/%s", "/var/mail", pw->pw_name);
		}

		foo.fhash = ekg_hash(inbox);
		foo.fname = inbox;
		foo.mtime = 0;
		foo.size = 0;
		foo.count = 0;
		foo.check = 1;

		list_add(&mail_folders, &foo, sizeof(foo));
	} else
		if (config_check_mail & 2) {
			char *inbox = saprintf("%s/Maildir", home_dir);
			
			foo.fhash = ekg_hash(inbox);
			foo.fname = inbox;
			foo.mtime = 0;
			foo.size = 0;
			foo.count = 0;
			foo.check = 1;

			list_add(&mail_folders, &foo, sizeof(foo));
		}
}

/*
 * check_mail_free()
 * 
 * czy¶ci pamiêæ po li¶cie folderów z poczt±.
 */
void check_mail_free()
{
	if (mail_folders) {
		list_t l;

		for (l = mail_folders; l; l = l->next) {
			struct mail_folder *m = l->data;

			xfree(m->fname);
		}

		list_destroy(mail_folders, 1);
		mail_folders = NULL;
	}
}
