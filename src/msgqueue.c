/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Piotr Domagalski <szalik@szalik.net>
 *                          Wojtek Kaniewski <wojtekka@irc.pl>
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
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dynstuff.h"
#include "libgadu.h"
#include "msgqueue.h"
#include "stuff.h"
#include "xmalloc.h"

list_t msg_queue = NULL;

/*
 * find_in_uins()
 *
 * sprawdza, czy w ci�gu uin'�w znajduje si� dany uin.
 *
 * 1 je�li znaleziono, 0 je�li nie.
 */
int find_in_uins(int uin_count, uin_t *uins, uin_t uin)
{
	int i;

	for (i = 0; i < uin_count; i++)
		if (uins[i] == uin)
			return 1;

	return 0;
}

/*
 * msg_queue_add()
 *
 * dodaje wiadomo�� do kolejki wiadomo�ci.
 * 
 *  - msg_class - typ wiadomo�ci,
 *  - msg_seq - numer sekwencyjny,
 *  - uin_count - ilo�� adresat�w,
 *  - uins - adresaci wiadomo�ci,
 *  - msg - wiadomo��,
 *  - secure - czy ma by� zaszyfrowana,
 *  - format - formatowanie wiadomo�ci,
 *  - formatlen - d�ugo�� informacji o formatowaniu.
 *
 * 0/-1
 */
int msg_queue_add(int msg_class, int msg_seq, int uin_count, uin_t *uins, const unsigned char *msg, int secure, const unsigned char *format, int formatlen)
{
	struct msg_queue m;

	if (uin_count == 1 && uins[0] == config_uin)	/* nie dostaniemy potwierdzenia, je�li wy�lemy wiadomo�� do siebie */
		return -1;

	m.msg_class = msg_class;
	m.msg_seq = msg_seq;
	m.uin_count = uin_count;
	m.uins = xmalloc(uin_count * sizeof(uin_t));
	memmove(m.uins, uins, uin_count * sizeof(uin_t));
	m.msg = (unsigned char *) xstrdup((const char *) msg);
	m.secure = secure;
	m.time = time(NULL);
	m.formatlen = formatlen;
	if (formatlen > 0) {
		m.format = xmalloc(formatlen * sizeof(unsigned char));
		memmove(m.format, format, formatlen * sizeof(unsigned char));
	} else
		m.format = NULL;

	return (list_add(&msg_queue, &m, sizeof(m)) ? 0 : -1);
}

/*
 * msg_queue_remove()
 *
 * usuwa wiadomo�� z kolejki wiadomo�ci.
 *
 *  - msg_seq - numer sekwencyjny wiadomo�ci.
 *
 * 0 je�li usuni�to, -1 je�li nie ma takiej wiadomo�ci.
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

	return -1; 
}

/*
 * msg_queue_remove_uin()
 *
 * usuwa wiadomo�� z kolejki wiadomo�ci dla danego
 * u�ytkownika.
 *
 *  - uin.
 *
 * 0 je�li usuni�to, -1 je�li nie ma takiej wiadomo�ci.
 */
int msg_queue_remove_uin(uin_t uin)
{
	list_t l;
	int x = -1;

	for (l = msg_queue; l; ) {
		struct msg_queue *m = l->data;

		l = l->next;

		if (find_in_uins(m->uin_count, m->uins, uin)) {
			xfree(m->uins);
			xfree(m->msg);
			xfree(m->format);

			list_remove(&msg_queue, m, 1);
			x = 0;
		}
	}

	return x;
}

/*
 * msg_queue_free()
 *
 * zwalnia pami�� po kolejce wiadomo�ci.
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
 * wysy�a wiadomo�ci z kolejki.
 *
 * 0 je�li wys�ano, -1 je�li nast�pi� b��d przy wysy�aniu, -2 je�li
 * kolejka pusta.
 */
int msg_queue_flush()
{
	list_t l = msg_queue;

	if (!l)
		return -2;

	for (; l; l = l->next) {
		struct msg_queue *m = l->data;
		int new_seq;
		unsigned char *tmp = (unsigned char *) xstrdup((const char *) m->msg);

		iso_to_cp(tmp);

		if (m->uin_count == 1) {
			if (m->secure)
				msg_encrypt(m->uins[0], &tmp);
			new_seq = gg_send_message_richtext(sess, m->msg_class, m->uins[0], tmp, m->format, m->formatlen);
		} else
			new_seq = gg_send_message_confer_richtext(sess, m->msg_class, m->uin_count, m->uins, tmp, m->format, m->formatlen);

		xfree(tmp);

		if (new_seq != -1)
			m->msg_seq = new_seq;
		else
			return -1;
	}

	return 0;
}

/*
 * msg_queue_count()
 *
 * zwraca liczb� wiadomo�ci w kolejce.
 */
int msg_queue_count()
{
	return list_count(msg_queue);
}

/*
 * msg_queue_count_uin()
 *
 * zwraca liczb� wiadomo�ci w kolejce dla danego
 * u�ytkownika.
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
 * zapisuje niedostarczone wiadomo�ci na dysku.
 *
 * 0/-1
 */
int msg_queue_write()
{
	const char *path;
	list_t l;
	int num = 0;

	if (!msg_queue)
		return -1;

	path = prepare_path("queue", 1);

	if (mkdir(path, 0700) && errno != EEXIST)
		return -1;

	for (l = msg_queue; l; l = l->next) {
		struct msg_queue *m = l->data;
		char *fn;
		FILE *f;
		int i;

		/* nie zapisujemy wiadomo�ci, kt�re za�apa�y si� do wysy�ki */
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

		fprintf(f, "%d\n%ld\n%d\n", m->secure, (long) m->time, m->formatlen);

		if (m->formatlen) {
			for (i = 0; i < m->formatlen; i++)
				fprintf(f, "%c", m->format[i]);

			fprintf(f, "\n");
		}

		fprintf(f, "%s", m->msg);

		fclose(f);
		chmod(fn, 0600);
		xfree(fn);
	}

	return 0;
}

/*
 * msg_queue_read()
 *
 * wczytuje kolejk� niewys�anych wiadomo�ci z dysku.
 *
 * 0/-1
 */
int msg_queue_read()
{
	const char *path;
	struct dirent *d;
	DIR *dir;

	path = prepare_path("queue", 0);

	if (!(dir = opendir(path)))
		return -1;

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

		/* jaki� zdrowy limit */
		if (m.uin_count < 1 || m.uin_count > 100) {
			fclose(f);
			xfree(fn);
			continue;
		}
		
		m.uins = xcalloc(m.uin_count, sizeof(uin_t));
		
		for (i = 0; i < m.uin_count; i++)
			fscanf(f, "%d\n", &m.uins[i]);

		fscanf(f, "%d\n", &m.secure);
		fscanf(f, "%ld\n", (long *) &m.time);
		fscanf(f, "%d\n", &m.formatlen);

		/* dziwny plik? */
		if (!m.time || !m.msg_seq || !m.msg_class) {
			fclose(f);
			xfree(fn);
			xfree(m.uins);
			continue;
		}

		if (m.formatlen) {
			m.format = xcalloc(m.formatlen, sizeof(unsigned char));

			for (i = 0; i < m.formatlen; i++)
				fscanf(f, "%c", &m.format[i]);

			fscanf(f, "%*c");
		} else
			m.format = NULL;

		msg = string_init(NULL);

		buf = read_file(f);

		while (buf) {
			string_append(msg, buf);
			xfree(buf);
			buf = read_file(f);
			if (buf)
				string_append(msg, "\r\n");
		}

		m.msg = (unsigned char *) msg->str;

		string_free(msg, 0);

		fclose(f);
	
		list_add(&msg_queue, &m, sizeof(m));

		unlink(fn);
		xfree(fn);
	}

	closedir(dir);

	return 0;
}
