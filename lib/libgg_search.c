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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netdb.h>
#include <errno.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdarg.h>
#include <ctype.h>
#include "libgg.h"

/*
 * gg_urlencode() // funkcja wewnêtrzna
 *
 * zamienia podany tekst na ci±g znaków do formularza http. przydaje siê
 * przy szukaniu userów z dziwnymi znaczkami.
 *
 *  - str - ci±g znaków do poprawki.
 *
 * zwraca zaalokowany bufor, który wypada³oby kiedy¶ zwolniæ albo NULL
 * w przypadku b³êdu.
 */
char *gg_urlencode(char *str)
{
	char *p, *q, *buf, hex[] = "0123456789abcdef";
	int size = 0;

	for (p = str; *p; p++, size++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9'))
			size += 2;
	}

	if (!(buf = malloc(size + 1)))
		return NULL;

	for (p = str, q = buf; *p; p++, q++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9'))
			*q = *p;
		else {
			*q++ = '%';
			*q++ = hex[*p >> 4 & 15];
			*q = hex[*p & 15];
		}
	}

	*q = 0;

	return buf;
}

/*
 * gg_search()
 *
 * rozpoczyna szukanie u¿ytkowników. informacje o tym, czego dok³adnie szukamy
 * s± zawarte w strukturze `gg_search_request'. ze wzglêdu na specyfikê ich
 * przeszukiwarki, niektórych pól nie mo¿na mieszaæ. s± oznaczone w libgg.h
 * jako osobne mode'y.
 *
 *  - r - informacja o tym, czego szukamy,
 *  - async - ma byæ asynchronicznie?
 *
 * zwraca zaalokowan± strukturê `gg_search', któr± po¼niej nale¿y zwolniæ
 * funkcj± gg_free_search(), albo NULL je¶li wyst±pi³ b³±d.
 */
struct gg_search *gg_search(struct gg_search_request *r, int async)
{
	struct gg_search *f;
	int mode = -1;

	if (!r) {
		errno = EFAULT;
		return NULL;
	}

	if (r->nickname || r->first_name || r->last_name || r->city || r->gender || r->min_birth || r->max_birth)
		mode = 0;

	if (r->email) {
		if (mode != -1) {
			errno = EINVAL;
			return NULL;
		}
		mode = 1;
	}

	if (r->phone) {
		if (mode != -1) {
			errno = EINVAL;
			return NULL;
		}
		mode = 2;
	}

	if (r->uin) {
		if (mode != -1) {
			errno = EINVAL;
			return NULL;
		}
		mode = 3;
	}

	if (mode == -1) {
		errno = EINVAL;
		return NULL;
	}

	if (!(f = malloc(sizeof(*f))))
		return NULL;

	memset(f, 0, sizeof(*f));

	if (r->first_name)
		f->request.first_name = strdup(r->first_name);
	if (r->last_name)
		f->request.last_name = strdup(r->last_name);
	if (r->nickname)
		f->request.nickname = strdup(r->nickname);
	if (r->city)
		f->request.city = strdup(r->city);
	if (r->email)
		f->request.email = strdup(r->email);
	if (r->phone)
		f->request.phone = strdup(r->phone);
	f->request.active = r->active;
	f->request.gender = r->gender;
	if (f->request.gender == GG_GENDER_NONE)
		f->request.gender = -1;
	f->request.min_birth = r->min_birth;
	f->request.max_birth = r->max_birth;
	f->request.uin = r->uin;

	f->async = async;
	f->mode = mode;

	if (async) {
		if (gg_resolve(&f->fd, &f->pid, GG_PUBDIR_HOST)) {
			gg_free_search(f);
			return NULL;
		}

		f->state = GG_STATE_RESOLVING;
		f->check = GG_CHECK_READ;
	} else {
		struct hostent *he;
		struct in_addr a;

		if (!(he = gethostbyname(GG_PUBDIR_HOST))) {
			gg_free_search(f);
			return NULL;
		} else
			memcpy((char*) &a, he->h_addr, sizeof(a));

		if (!(f->fd = gg_connect(&a, GG_PUBDIR_PORT, 0)) == -1) {
			gg_free_search(f);
			return NULL;
		}

		f->state = GG_STATE_CONNECTING_HTTP;

		while (f->state != GG_STATE_IDLE && f->state != GG_STATE_FINISHED) {
			if (gg_search_watch_fd(f) == -1)
				break;
		}

		if (f->state != GG_STATE_FINISHED) {
			gg_free_search(f);
			return NULL;
		}
	}

	return f;
}

#define GET_LOST(x) \
	close(f->fd); \
	f->state = GG_STATE_IDLE; \
	f->error = x; \
	f->fd = 0; \
	return -1;

/*
 * gg_search_watch_fd()
 *
 * przy asynchronicznym szukaniu userów wypada³oby wywo³aæ t± funkcjê przy
 * jaki¶ zmianach na gg_search->fd.
 *
 *  - f - to co¶, co zwróci³o gg_search()
 *
 * je¶li wszystko posz³o dobrze to 0, inaczej -1. przeszukiwanie bêdzie
 * zakoñczone, je¶li f->state == GG_STATE_FINISHED. je¶li wyst±pi jaki¶
 * b³±d, to bêdzie tam GG_STATE_IDLE i odpowiedni kod b³êdu w f->error.
 */
int gg_search_watch_fd(struct gg_search *f)
{
	if (!f) {
		errno = EINVAL;
		return -1;
	}

	if (f->state == GG_STATE_RESOLVING) {
		struct in_addr a;

		gg_debug(GG_DEBUG_MISC, "=> resolved\n");

		if (read(f->fd, &a, sizeof(a)) < sizeof(a) || a.s_addr == INADDR_NONE) {
			gg_debug(GG_DEBUG_MISC, "=> resolver thread failed\n");
			GET_LOST(GG_FAILURE_RESOLVING);
		}

		close(f->fd);

		waitpid(f->pid, NULL, 0);

		gg_debug(GG_DEBUG_MISC, "=> connecting\n");

		if ((f->fd = gg_connect(&a, GG_PUBDIR_PORT, f->async)) == -1) {
			gg_debug(GG_DEBUG_MISC, "=> connection failed\n");
			GET_LOST(GG_FAILURE_CONNECTING);
		}

		f->state = GG_STATE_CONNECTING_HTTP;
		f->check = GG_CHECK_WRITE;

		return 0;
	}

	if (f->state == GG_STATE_CONNECTING_HTTP) {
		int res, res_size = sizeof(res);
		char *form, *query;

		if (f->async && (getsockopt(f->fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res)) {
			gg_debug(GG_DEBUG_MISC, "=> async connection failed\n");
			GET_LOST(GG_FAILURE_CONNECTING);
		}

		if (f->mode == 0) {
			char *__first_name, *__last_name, *__nickname, *__city;

			__first_name = gg_urlencode((f->request.first_name) ? f->request.first_name : "");
			__last_name = gg_urlencode((f->request.last_name) ? f->request.last_name : "");
			__nickname = gg_urlencode((f->request.nickname) ? f->request.nickname : "");
			__city = gg_urlencode((f->request.city) ? f->request.city : "");

			if (!__first_name || !__last_name || !__nickname || !__city) {
				free(__first_name);
				free(__last_name);
				free(__nickname);
				free(__city);
				gg_debug(GG_DEBUG_MISC, "=> not enough memory for form fields\n");
				GET_LOST(GG_FAILURE_WRITING);
			}

			form = gg_alloc_sprintf("Mode=0&FirstName=%s&LastName=%s&Gender=%d&NickName=%s&City=%s&MinBirth=%d&MaxBirth=%d%s", __first_name, __last_name, f->request.gender, __nickname, __city, f->request.min_birth, f->request.max_birth, (f->request.active) ? "&ActiveOnly=" : "");

			free(__first_name);
			free(__last_name);
			free(__nickname);
			free(__city);

		} else if (f->mode == 1) {
			char *__email = gg_urlencode((f->request.email) ? f->request.email : "");

			if (!__email) {
				gg_debug(GG_DEBUG_MISC, "=> not enough memory for form fields\n");
				GET_LOST(GG_FAILURE_WRITING);
			}

			form = gg_alloc_sprintf("Mode=1&Email=%s%s", __email, (f->request.active) ? "&ActiveOnly=" : "");

			free(__email);

		} else if (f->mode == 2) {
			char *__phone = gg_urlencode((f->request.phone) ? f->request.phone : "");

			if (!__phone) {
				gg_debug(GG_DEBUG_MISC, "=> not enough memory for form fields\n");
				GET_LOST(GG_FAILURE_WRITING);
			}

			form = gg_alloc_sprintf("Mode=2&MobilePhone=%s%s", __phone, (f->request.active) ? "&ActiveOnly=" : "");

			free(__phone);

		} else
			form = gg_alloc_sprintf("Mode=3&UserId=%u%s", f->request.uin, (f->request.active) ? "&ActiveOnly=" : "");

		if (!form) {
			gg_debug(GG_DEBUG_MISC, "=> not enough memory for form query\n");
			GET_LOST(GG_FAILURE_WRITING);
		}

		gg_debug(GG_DEBUG_MISC, "=> %s\n", form);

		query = gg_alloc_sprintf(
			"POST /appsvc/fmpubquery2.asp HTTP/1.0\r\n"
			"Host: " GG_PUBDIR_HOST "\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"User-Agent: Mozilla/4.7 [en] (Win98; I)\r\n"
			"Content-Length: %d\r\n"
			"Pragma: no-cache\r\n"
			"\r\n"
			"%s",
			strlen(form), form);

		free(form);

		if (!query) {
			gg_debug(GG_DEBUG_MISC, "=> not enough memory for query string\n");
			GET_LOST(GG_FAILURE_WRITING);
		}

		if ((res = write(f->fd, query, strlen(query))) < strlen(query)) {
			gg_debug(GG_DEBUG_MISC, "=> http request failed (len=%d, res=%d, errno=%d)\n", strlen(query), res, errno);
			free(query);
			GET_LOST(GG_FAILURE_WRITING);
		}

		free(query);

		gg_debug(GG_DEBUG_MISC, "=> http request sent (len=%d)\n", strlen(query));

		f->state = GG_STATE_READING_HEADER;
		f->check = GG_CHECK_READ;

		return 0;
	
	}

	if (f->state == GG_STATE_READING_HEADER) {
		char buf[1024], *tmp;
		int res;

		if ((res = read(f->fd, buf, sizeof(buf))) == -1) {
			gg_debug(GG_DEBUG_MISC, "=> reading http header failed (errno=%d)\n", errno);
			if (f->header_buf) {
				free(f->header_buf);
				f->header_buf = NULL;
			}
			GET_LOST(GG_FAILURE_READING);
		}

		gg_debug(GG_DEBUG_MISC, "=> read %d bytes\n", res);

#if 0
		if (!f->header_buf) {
			if (!(f->header_buf = malloc(res + 1))) {
				gg_debug(GG_DEBUG_MISC, "=> not enough memory for header\n");
				GET_LOST(GG_FAILURE_READING);
			}
			memcpy(f->header_buf, buf, res);
			f->header_size = res;
		} else {
			if (!(f->header_buf = realloc(f->header_buf, f->header_size + res + 1))) {
				gg_debug(GG_DEBUG_MISC, "=> not enough memory for header\n");
				GET_LOST(GG_FAILURE_READING);
			}
			memcpy(f->header_buf + f->header_size, buf, res);
			f->header_size += res;
		}
#endif

		if (!(f->header_buf = realloc(f->header_buf, f->header_size + res + 1))) {
			gg_debug(GG_DEBUG_MISC, "=> not enough memory for header\n");
			GET_LOST(GG_FAILURE_READING);
		}
		memcpy(f->header_buf + f->header_size, buf, res);
		f->header_size += res;

		gg_debug(GG_DEBUG_MISC, "=> header_buf=%p, header_size=%d\n", f->header_buf, f->header_size);

		f->header_buf[f->header_size] = 0;

		if ((tmp = strstr(f->header_buf, "\r\n\r\n")) || (tmp = strstr(f->header_buf, "\n\n"))) {
			int sep_len = (*tmp == '\r') ? 4 : 2, left;
			char *line;

			left = f->header_size - ((long)(tmp) - (long)(f->header_buf) + sep_len);

			gg_debug(GG_DEBUG_MISC, "=> got all header (%d bytes, %d left)\n", f->header_size - left, left);

			/* HTTP/1.1 200 OK */
			if (strlen(f->header_buf) < 16 || strncmp(f->header_buf + 9, "200", 3)) {
				gg_debug(GG_DEBUG_MISC, f->header_buf);
				gg_debug(GG_DEBUG_MISC, "=> didn't get 200 OK -- no results\n");
				free(f->header_buf);
				f->header_buf = NULL;
				close(f->fd);
				f->state = GG_STATE_FINISHED;
				f->fd = 0;
				f->count = 0;
				f->results = NULL;

				return 0;
			}

			f->data_size = 0;
			line = f->header_buf;
			*tmp = 0;

			while (line) {
				if (!strncasecmp(line, "Content-length: ", 16)) {
					f->data_size = atoi(line + 16);
				}
				line = strchr(line, '\n');
				if (line)
					line++;
			}

			if (!f->data_size) {
				gg_debug(GG_DEBUG_MISC, "=> content-length not found\n");
				free(f->header_buf);
				f->header_buf = NULL;
				GET_LOST(GG_FAILURE_READING);
			}

			gg_debug(GG_DEBUG_MISC, "=> data_size=%d\n", f->data_size);

			if (!(f->data_buf = malloc(f->data_size + 1))) {
				gg_debug(GG_DEBUG_MISC, "=> not enough memory (%d bytes for data_buf)\n", f->data_size + 1);
				free(f->header_buf);
				f->header_buf = NULL;
				GET_LOST(GG_FAILURE_READING);
			}

			if (left) {
				if (left > f->data_size) {
					gg_debug(GG_DEBUG_MISC, "=> too much data (%d bytes left, %d needed)\n", left, f->data_size);
					free(f->header_buf);
					free(f->data_buf);
					f->header_buf = NULL;
					f->data_buf = NULL;
					GET_LOST(GG_FAILURE_READING);
				}

				memcpy(f->data_buf, tmp + sep_len, left);
				f->data_buf[left] = 0;
			}

			free(f->header_buf);
			f->header_buf = NULL;
			f->header_size = 0;

			if (left && left == f->data_size) {
				gg_debug(GG_DEBUG_MISC, "=> wow, we already have all data\n");
				f->state = GG_STATE_PARSING;
			} else {
				f->state = GG_STATE_READING_DATA;
				f->check = GG_CHECK_READ;
				return 0;
			}
		} else
			return 0;
	}

	if (f->state == GG_STATE_READING_DATA) {
		char buf[1024];
		int res;

		if ((res = read(f->fd, buf, sizeof(buf))) == -1) {
			gg_debug(GG_DEBUG_MISC, "=> reading http data failed (errno=%d)\n", errno);
			if (f->data_buf) {
				free(f->data_buf);
				f->data_buf = NULL;
			}
			GET_LOST(GG_FAILURE_READING);
		}

		gg_debug(GG_DEBUG_MISC, "=> read %d bytes of data\n", res);

		if (strlen(f->data_buf) + res > f->data_size) {
			gg_debug(GG_DEBUG_MISC, "=> too much data (%d bytes, %d needed), truncating\n", strlen(f->data_buf) + res, f->data_size);
			res = f->data_size - strlen(f->data_buf);
		}

		f->data_buf[strlen(f->data_buf) + res] = 0;
		memcpy(f->data_buf + strlen(f->data_buf), buf, res);

		gg_debug(GG_DEBUG_MISC, "=> strlen(data_buf)=%d, data_size=%d\n", strlen(f->data_buf), f->data_size);

		if (strlen(f->data_buf) >= f->data_size) {
			gg_debug(GG_DEBUG_MISC, "=> okay, we've got all the data, closing socket\n");
			f->state = GG_STATE_PARSING;
			close(f->fd);
			f->fd = 0;
		} else
			return 0;
	}

	if (f->state == GG_STATE_PARSING) {
		char *foo = f->data_buf;

		gg_debug(GG_DEBUG_MISC, "=> ladies and gentlemen, parsing begins\n");
		gg_debug(GG_DEBUG_MISC, "%s\n", foo);

		f->count = 0;
		f->results = NULL;
		f->state = GG_STATE_FINISHED;

		if (!gg_get_line(&foo)) {
			gg_debug(GG_DEBUG_MISC, "=> aiye, can't read the first line\n");
			return 0;
		}
		
		while (1) {
			char *tmp[8];
			int i;

			for (i = 0; i < 8; i++) {
				if (!(tmp[i] = gg_get_line(&foo))) {
					gg_debug(GG_DEBUG_MISC, "=> aiye, can't read line %d of this user\n", i + 1);

					free(f->data_buf);
					f->data_buf = NULL;
					return 0;
				}
				gg_debug(GG_DEBUG_MISC, "=> [%s]\n", tmp[i]);
			}

			if (!(f->results = realloc(f->results, (f->count + 1) * sizeof(struct gg_search_result)))) {
				gg_debug(GG_DEBUG_MISC, "=> not enough memory for results (non critical)\n");
				free(f->data_buf);
				f->data_buf = NULL;
				return 0;
			}

			f->results[f->count].active = (atoi(tmp[0]) == 2);
			f->results[f->count].uin = (strtol(tmp[1], NULL, 0));
			f->results[f->count].first_name = strdup(tmp[2]);
			f->results[f->count].last_name = strdup(tmp[3]);
			f->results[f->count].nickname = strdup(tmp[4]);
			f->results[f->count].born = atoi(tmp[5]);
			f->results[f->count].gender = atoi(tmp[6]);
			f->results[f->count].city = strdup(tmp[7]);

			f->count++;
		}

		free(f->data_buf);
		f->data_buf = NULL;

		f->state = GG_STATE_FINISHED;

		gg_debug(GG_DEBUG_MISC, "=> done (%d entries)\n", f->count);

		return 0;
	}

	if (f->fd)
		close(f->fd);

	f->state = GG_STATE_IDLE;
	f->error = 0;

	return -1;
}

#undef GET_LOST

/*
 * gg_search_cancel()
 *
 * je¶li szukanie jest w trakcie, przerywa.
 *
 *  - f - to co¶, co zwróci³o gg_search().
 *
 * UWAGA! funkcja potencjalnie niebezpieczna, bo mo¿e pozwalniaæ bufory
 * i pozamykaæ sockety, kiedy co¶ siê dzieje. ale to ju¿ nie mój problem ;)
 */
void gg_search_cancel(struct gg_search *f)
{
	if (!f)
		return;

	if (f->state == GG_STATE_IDLE || f->state == GG_STATE_FINISHED)
		return;

	if (f->fd)
		close(f->fd);

	if (f->header_buf)
		free(f->header_buf);
	if (f->data_buf)
		free(f->data_buf);	
}

/*
 * gg_free_search()
 *
 * zwalnia pamiêæ po efektach szukania userów.
 *
 *  - f - to co¶, co nie jest ju¿ nam potrzebne.
 *
 * nie zwraca niczego. najwy¿ej segfaultnie ;)
 */
void gg_free_search(struct gg_search *f)
{
	int i;

	if (!f)
		return;

	for (i = 0; i < f->count; i++) {
		free(f->results[i].first_name);
		free(f->results[i].last_name);
		free(f->results[i].nickname);
		free(f->results[i].city);
	}

	free(f->results);
	free(f->request.first_name);
	free(f->request.last_name);
	free(f->request.nickname);
	free(f->request.city);
	free(f->request.email);
	free(f->request.phone);
	free(f);
}

