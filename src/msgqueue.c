/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Piotr Domagalski <szalik@szalik.net>
 *                          Wojtek Kaniewski <wojtekka@dev.null.pl>
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
#include <dirent.h>
#include <fcntl.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <errno.h>
#include "compat.h"
#include "stuff.h"
#include "dynstuff.h"
#include "xmalloc.h"
#include "msgqueue.h"

list_t msg_queue = NULL;

/*
 * msg_queue_add()
 *
 * dodaje wiadomo¶æ do kolejki wiadomo¶ci.
 * 
 *  - msg_class - typ wiadomo¶ci,
 *  - msg_seq - numer sekwencyjny,
 *  - uin_count - ilo¶æ adresatów,
 *  - uins - adresaci wiadomo¶ci,
 *  - msg - wiadomo¶æ,
 *  - secure - czy ma byæ zaszyfrowana,
 *  - format - formatowanie wiadomo¶ci,
 *  - formatlen - d³ugo¶æ informacji o formatowaniu.
 *
 * 0 je¶li siê uda³o, -1 je¶li b³±d.
 */
int msg_queue_add(int msg_class, int msg_seq, int uin_count, uin_t *uins, const unsigned char *msg, int secure, const unsigned char *format, int formatlen)
{
	struct msg_queue m;

	if (uin_count == 1 && uins[0] == config_uin)	/* nie dostaniemy potwierdzenia, je¶li wy¶lemy wiadomo¶æ do siebie */
		return -1;

	m.msg_class = msg_class;
	m.msg_seq = msg_seq;
	m.uin_count = uin_count;
	m.uins = xmalloc(uin_count * sizeof(uin_t));
	memmove(m.uins, uins, uin_count * sizeof(uin_t));
	m.msg = xstrdup(msg);
	m.secure = secure;
	m.time = time(NULL);
	m.format = xmalloc(formatlen * sizeof(unsigned char));
	memmove(m.format, format, formatlen * sizeof(unsigned char));
	m.formatlen = formatlen;

	return (list_add(&msg_queue, &m, sizeof(m)) != NULL) ? 0 : -1;
}

/*
 * msg_queue_remove()
 *
 * usuwa wiadomo¶æ z kolejki wiadomo¶ci.
 *
 *  - msg_seq - numer sekwencyjny wiadomo¶ci.
 *
 * 0 je¶li usuniêto, 1 je¶li nie ma takiej wiadomo¶ci.
 */
int msg_queue_remove(int msg_seq)
{
	list_t l;

	for (l = msg_queue; l; l = l->next) {
		struct msg_queue *m = l->data;

		if (m->msg_seq == msg_seq) {
			xfree(m->uins);
			xfree(m->msg);
			xfree(m->format);

			list_remove(&msg_queue, m, 1);

			return 0;
		}
	}

	return 1; 
}

/*
 * msg_queue_remove_uin()
 *
 * usuwa wiadomo¶æ z kolejki wiadomo¶ci dla danego
 * u¿ytkownika.
 *
 *  - uin.
 *
 * 0 je¶li usuniêto, 1 je¶li nie ma takiej wiadomo¶ci.
 */
int msg_queue_remove_uin(uin_t uin)
{
	list_t l;
	int x = 0;

	for (l = msg_queue; l; l = l->next) {
		struct msg_queue *m = l->data;

		if (find_in_uins(m->uin_count, m->uins, uin)) {
			xfree(m->uins);
			xfree(m->msg);
			xfree(m->format);

			list_remove(&msg_queue, m, 1);
			x = 1;
		}
	}

	return (x) ? 0 : 1;
}

/*
 * msg_queue_free()
 *
 * pozbywa siê kolejki wiadomo¶ci.
 */
void msg_queue_free()
{
	list_t l;

	for (l = msg_queue; l; l = l->next) {
		struct msg_queue *m = l->data;

		xfree(m->uins);
		xfree(m->msg);
		xfree(m->format);
	}

	list_destroy(msg_queue, 1);
	msg_queue = NULL;
}

/*
 * msg_queue_flush()
 *
 * wysy³a wiadomo¶ci z kolejki.
 *
 * 0 je¶li wys³ano, 1 je¶li nast±pi³ b³±d przy wysy³aniu, 2 je¶li
 * kolejka pusta.
 */
int msg_queue_flush()
{
	list_t l = msg_queue;

	if (!l)
		return 2;

	for (; l; l = l->next) {
		struct msg_queue *m = l->data;
		int new_seq;
		unsigned char *tmp = xstrdup(m->msg);

		iso_to_cp(tmp);

		if (m->uin_count == 1) {
			if (m->secure)
				msg_encrypt(*(m->uins), &tmp);
			new_seq = gg_send_message_richtext(sess, m->msg_class, *(m->uins), tmp, m->format, m->formatlen);
		} else
			new_seq = gg_send_message_confer_richtext(sess, m->msg_class, m->uin_count, m->uins, tmp, m->format, m->formatlen);

		xfree(tmp);

		if (new_seq != -1)
			m->msg_seq = new_seq;
		else
			return 1;
	}

	return 0;
}

/*
 * msg_queue_count()
 *
 * zwraca liczbê wiadomo¶ci w kolejce.
 */
int msg_queue_count()
{
	return list_count(msg_queue);
}

/*
 * msg_queue_count_uin()
 *
 * zwraca liczbê wiadomo¶ci w kolejce dla danego
 * u¿ytkownika.
 *
 * - uin.
 */
int msg_queue_count_uin(uin_t uin)
{
	list_t l;
	int count = 0;

	for (l = msg_queue; l; l = l->next) {
		struct msg_queue *m = l->data;

		if (find_in_uins(m->uin_count, m->uins, uin))
			count++;
	}

	return count;
}

/*
 * msg_queue_write()
 *
 * zapisuje niedostarczone wiadomo¶ci na dysku.
 */
int msg_queue_write()
{
	const char *path;
	list_t l;
	int num = 0;

	path = prepare_path("queue", 1);

	if (mkdir(path, 0700) && errno != EEXIST)
		return -1;

	for (l = msg_queue; l; l = l->next) {
		struct msg_queue *m = l->data;
		char *fn;
		FILE *f;
		int i;

		/* nie zapisujemy wiadomo¶ci, które za³apa³y siê do wysy³ki */
		if (m->msg_seq != -1)
			continue;

		fn = saprintf("%s/%ld.%d", path, (long) m->time, num++);

		if (!(f = fopen(fn, "w"))) {
			xfree(fn);
			continue;
		}

		fprintf(f, "%d\n%d\n%d\n", m->msg_class, m->msg_seq, m->uin_count);

		for (i = 0; i < m->uin_count; i++)
			fprintf(f, "%d\n", m->uins[i]);

		fprintf(f, "%d\n%ld\n%s", m->secure, (long) m->time, m->msg);

		fclose(f);

		chmod(fn, 0600);

		xfree(fn);
	}

	return 0;
}

/*
 * msg_queue_read()
 *
 * wczytuje kolejkê niewys³anych wiadomo¶ci z dysku.
 */
int msg_queue_read()
{
	const char *path;
	struct dirent *d;
	DIR *dir;

	path = prepare_path("queue", 0);

	if (!(dir = opendir(path)))
		return 0;

	while ((d = readdir(dir))) {
		struct msg_queue m;
		struct stat st;
		string_t msg;
		char *fn, *buf;
		FILE *f;
		int i;

		fn = saprintf("%s/%s", path, d->d_name);
		
		if (stat(fn, &st) || !S_ISREG(st.st_mode)) {
			xfree(fn);
			continue;
		}

		if (!(f = fopen(fn, "r"))) {
			xfree(fn);
			continue;
		}

		memset(&m, 0, sizeof(m));

		fscanf(f, "%d\n", &m.msg_class);
		fscanf(f, "%d\n", &m.msg_seq);
		fscanf(f, "%d\n", &m.uin_count);

		if (m.uin_count < 1 || m.uin_count > 100) {	/* XXX jaki¶ zdrowy limit */
			fclose(f);
			xfree(fn);
			continue;
		}
		
		m.uins = xcalloc(m.uin_count, sizeof(uin_t));
		
		for (i = 0; i < m.uin_count; i++)
			fscanf(f, "%d\n", &m.uins[i]);

		fscanf(f, "%d\n", &m.secure);
		fscanf(f, "%ld\n", (long *) &m.time);

		/* dziwny plik? */
		if (!m.time || !m.msg_seq || !m.msg_class) {
			fclose(f);
			xfree(fn);
			xfree(m.uins);
			continue;
		}

		msg = string_init(NULL);

		buf = read_file(f);

		while (buf) {
			string_append(msg, buf);
			xfree(buf);
			buf = read_file(f);
			if (buf)
				string_append(msg, "\r\n");
		}

		m.msg = msg->str;

		string_free(msg, 0);

		fclose(f);
	
		list_add(&msg_queue, &m, sizeof(m));

		unlink(fn);
		xfree(fn);
	}

	closedir(dir);

	return 0;
}
