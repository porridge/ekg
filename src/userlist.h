/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
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

#ifndef __USERLIST_H
#define __USERLIST_H

#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "config.h"
#include "libgadu.h"
#include "dynstuff.h"

struct userlist {
	char *first_name;	/* imiê */
	char *last_name;	/* nazwisko */
	char *nickname;		/* pseudonim */
	char *display;		/* wy¶wietlania nazwa */
	char *mobile;		/* komórka */
	list_t groups;		/* grupy, do których nale¿y */
	uin_t uin;		/* numer */
	int status;		/* aktualny stan */
	char *descr;		/* opis/powód stanu */
	struct in_addr ip;	/* adres ip */
	unsigned short port;	/* port */
};

struct ignored {
	uin_t uin;		/* numer */
	int level;		/* XXX poziom ignorowania, bêdzie kiedy¶ */
};

struct group {
	char *name;
};

list_t userlist;
list_t ignored;

int userlist_read();
int userlist_write();
#ifdef WITH_WAP
int userlist_write_wap();
#endif
void userlist_write_crash();
void userlist_clear_status(void);
int userlist_add(uin_t uin, const char *display);
int userlist_remove(struct userlist *u);
int userlist_replace(struct userlist *u);
void userlist_send();
struct userlist *userlist_find(uin_t uin, const char *display);
char *userlist_dump();
void userlist_clear();
#define userlist_free userlist_clear
int userlist_set(char *contacts);

int ignored_add(uin_t uin);
int ignored_remove(uin_t uin);
int ignored_check(uin_t uin);

int group_add(struct userlist *u, const char *group);
int group_remove(struct userlist *u, const char *group);
int group_member(struct userlist *u, const char *group);
char *group_to_string(list_t l);
list_t group_init(const char *groups);

uin_t get_uin(const char *text);
const char *format_user(uin_t uin);

#endif /* __USERLIST_H */
