/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <errno.h>
#ifndef _AIX
#  include <string.h>
#endif
#include "compat.h"
#include "libgadu.h"

/*
 * gg_search50_new()
 *
 * tworzy now± zmienn± typu gg_search50_t.
 *
 * zaalokowana zmienna lub NULL w przypadku braku pamiêci.
 */
gg_search50_t gg_search50_new()
{
	gg_search50_t res = malloc(sizeof(struct gg_search50_s));

	if (!res) {
		gg_debug(GG_DEBUG_MISC, "// gg_search50_new() out of memory\n");
		return NULL;
	}

	memset(res, 0, sizeof(struct gg_search50_s));

	return res;
}

/*
 * gg_search50_add_n()  // funkcja wewnêtrzna
 *
 * funkcja dodaje pole do zapytania lub odpowiedzi.
 *
 *  - req - wska¼nik opisu zapytania,
 *  - num - numer wyniku (0 dla zapytania),
 *  - field - nazwa pola,
 *  - value - warto¶æ pola,
 *
 * 0/-1
 */
int gg_search50_add_n(gg_search50_t req, int num, const char *field, const char *value)
{
	struct gg_search50_entry *tmp = NULL, *entry;
	char *dupfield, *dupvalue;

	if (!(dupfield = strdup(field))) {
		gg_debug(GG_DEBUG_MISC, "// gg_search50_add_n() out of memory\n");
		return -1;
	}

	if (!(dupvalue = strdup(value))) {
		gg_debug(GG_DEBUG_MISC, "// gg_search50_add_n() out of memory\n");
		free(dupfield);
		return -1;
	}

	if (!(tmp = realloc(req->entries, sizeof(struct gg_search50_entry) * (req->entries_count + 1)))) {
		gg_debug(GG_DEBUG_MISC, "// gg_search50_add_n() out of memory\n");
		free(dupfield);
		free(dupvalue);
		return -1;
	}

	req->entries = tmp;

	entry = &req->entries[req->entries_count];
	entry->num = num;
	entry->field = dupfield;
	entry->value = dupvalue;

	req->entries_count++;

	return 0;
}

/*
 * gg_search50_add()
 *
 * funkcja dodaje pole do zapytania.
 *
 *  - req - wska¼nik opisu zapytania,
 *  - field - nazwa pola,
 *  - value - warto¶æ pola,
 *
 * 0/-1
 */
int gg_search50_add(gg_search50_t req, const char *field, const char *value)
{
	return gg_search50_add_n(req, 0, field, value);
}

/*
 * gg_search50_free()
 *
 * zwalnia pamiêæ po zapytaniu lub rezultacie szukania u¿ytkownika.
 *
 *  - s - zwalniana zmienna,
 */
void gg_search50_free(gg_search50_t s)
{
	int i;

	if (!s)
		return;
	
	for (i = 0; i < s->entries_count; i++) {
		free(s->entries[i].field);
		free(s->entries[i].value);
	}

	free(s->entries);
}

/*
 * gg_search50()
 *
 * wysy³a zapytanie katalogu publicznego do serwera.
 *
 *  - sess - sesja,
 *  - req - zapytanie.
 *
 * 0/-1
 */
int gg_search50(struct gg_session *sess, gg_search50_t req)
{
	int i, size = 5, res;
	char *buf, *p;
	struct gg_search50_request *r;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_search50(%p, %p);\n", sess, req);
	
	if (!sess || !req) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	for (i = 0; i < req->entries_count; i++) {
		/* wyszukiwanie bierze tylko pierwszy wpis */
		if (req->entries[i].num)
			continue;
		
		size += strlen(req->entries[i].field) + 1;
		size += strlen(req->entries[i].value) + 1;
	}

	buf = malloc(size);

	r = (struct gg_search50_request*) buf;
	r->dunno1 = 0x1f000003;
	r->dunno2 = '>';

	for (i = 0, p = buf + 5; i < req->entries_count; i++) {
		if (req->entries[i].num)
			continue;

		strcpy(p, req->entries[i].field);
		p += strlen(p) + 1;

		strcpy(p, req->entries[i].value);
		p += strlen(p) + 1;
	}

	res = gg_send_packet(sess->fd, GG_SEARCH50_REQUEST, buf, size, NULL, 0);

	free(buf);

	return res;
}

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: notnil
 * End:
 *
 * vim: shiftwidth=8:
 */
