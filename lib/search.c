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
#include <errno.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdarg.h>
#include <ctype.h>
#include "libgg.h"
#include "http.h"

#define GG_SEARCH_COPY \
{ \
	f->fd = f->http->fd; \
	f->check = f->http->check; \
	f->state = f->http->state; \
	f->error = f->http->error; \
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
	char *form, *query;
	int mode = -1, gender;

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

	gender = (r->gender == GG_GENDER_NONE) ? -1 : r->gender;

	if (mode == 0) {
		char *__first_name, *__last_name, *__nickname, *__city;

		__first_name = gg_urlencode(r->first_name);
		__last_name = gg_urlencode(r->last_name);
		__nickname = gg_urlencode(r->nickname);
		__city = gg_urlencode(r->city);

		if (!__first_name || !__last_name || !__nickname || !__city) {
			free(__first_name);
			free(__last_name);
			free(__nickname);
			free(__city);
			gg_debug(GG_DEBUG_MISC, "=> search, not enough memory for form fields\n");
			return NULL;
		}

		form = gg_alloc_sprintf("Mode=0&FirstName=%s&LastName=%s&Gender=%d&NickName=%s&City=%s&MinBirth=%d&MaxBirth=%d%s", __first_name, __last_name, gender, __nickname, __city, r->min_birth, r->max_birth, (r->active) ? "&ActiveOnly=" : "");

		free(__first_name);
		free(__last_name);
		free(__nickname);
		free(__city);

	} else if (mode == 1) {
		char *__email = gg_urlencode(r->email);

		if (!__email) {
			gg_debug(GG_DEBUG_MISC, "=> search, not enough memory for form fields\n");
			return NULL;
		}

		form = gg_alloc_sprintf("Mode=1&Email=%s%s", __email, (r->active) ? "&ActiveOnly=" : "");

		free(__email);

	} else if (mode == 2) {
		char *__phone = gg_urlencode(r->phone);

		if (!__phone) {
			gg_debug(GG_DEBUG_MISC, "=> search, not enough memory for form fields\n");
			return NULL;
		}

		form = gg_alloc_sprintf("Mode=2&MobilePhone=%s%s", __phone, (r->active) ? "&ActiveOnly=" : "");

		free(__phone);

	} else
		form = gg_alloc_sprintf("Mode=3&UserId=%u%s", r->uin, (r->active) ? "&ActiveOnly=" : "");

	if (!form) {
		gg_debug(GG_DEBUG_MISC, "=> search, not enough memory for form query\n");
		return NULL;
	}

	gg_debug(GG_DEBUG_MISC, "=> search, %s\n", form);

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

	if (!(f = malloc(sizeof(*f))))
		return NULL;

	memset(f, 0, sizeof(*f));

	f->count = 0;
	f->results = NULL;
	f->done = 0;

	if (!(f->http = gg_http_connect(GG_PUBDIR_HOST, GG_PUBDIR_PORT, async, query))) {
		gg_debug(GG_DEBUG_MISC, "=> search, gg_http_connect() failed mysteriously\n");
		free(query);
		free(f);
		return NULL;
	}

	free(query);

	GG_SEARCH_COPY;
	
	if (!async)
		gg_search_watch_fd(f);
	
	return f;
}

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
	int res;

	if (!f || !f->http) {
		errno = EINVAL;
		return -1;
	}
	
	if (f->http->state != GG_STATE_FINISHED) {
		if ((res = gg_http_watch_fd(f->http)) == -1) {
			gg_debug(GG_DEBUG_MISC, "=> search, http failure\n");
			return -1;
		}
		GG_SEARCH_COPY;
	}
	
	if (f->state == GG_STATE_FINISHED) {
		char *foo = f->http->data;

		f->done = 1;
		gg_debug(GG_DEBUG_MISC, "=> search, let's parse\n");

		if (!gg_get_line(&foo)) {
			gg_debug(GG_DEBUG_MISC, "=> search, can't read the first line\n");
			return 0;
		}
		
		while (1) {
			char *tmp[8];
			int i;

			for (i = 0; i < 8; i++) {
				if (!(tmp[i] = gg_get_line(&foo))) {
					gg_debug(GG_DEBUG_MISC, "=> search, can't read line %d of this user\n", i + 1);
					return 0;
				}
				gg_debug(GG_DEBUG_MISC, "=> search, line %i \"%s\"\n", i, tmp[i]);
			}

			if (!(f->results = realloc(f->results, (f->count + 1) * sizeof(struct gg_search_result)))) {
				gg_debug(GG_DEBUG_MISC, "=> search, not enough memory for results (non critical)\n");
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

		gg_debug(GG_DEBUG_MISC, "=> search, done (%d entries)\n", f->count);

		return 0;
	}

	return 0;
}

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
	if (!f || !f->http)
		return;

	gg_http_stop(f->http);
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

	gg_free_http(f->http);

	for (i = 0; i < f->count; i++) {
		free(f->results[i].first_name);
		free(f->results[i].last_name);
		free(f->results[i].nickname);
		free(f->results[i].city);
	}

	free(f->results);
	free(f);
}

