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

#ifndef __GG_HTTP_H
#define __GG_HTTP_H

struct gg_http {
	int fd, async, state, check, error, pid, port;
	char *header, *data, *query;
	int header_size, data_size;
};

struct gg_http *gg_http_connect(char *hostname, int port, int async, char *method, char *path, char *header);
int gg_http_watch_fd(struct gg_http *h);
void gg_http_stop(struct gg_http *h);
void gg_free_http(struct gg_http *h);

#define gg_http_copy_vars(x) \
{ \
	x->fd = x->http->fd; \
	x->check = x->http->check; \
	x->state = x->http->state; \
	x->error = x->http->error; \
}
				
#endif /* __GG_HTTP_H */

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: notnil
 * End:
 *
 * vim: expandtab shiftwidth=8:
 */
