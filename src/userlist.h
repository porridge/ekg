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

#define EKG_GENDER(u) (((u)->first_name) ? (u)->first_name : (u)->display)

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

struct group {
	char *name;
};

enum ignore_t {
	IGNORE_STATUS = 1,
	IGNORE_STATUS_DESCR = 2,
	IGNORE_MSG = 4,
	IGNORE_DCC = 8,
	IGNORE_EVENTS = 16,
	IGNORE_NOTIFY = 32,
	
	IGNORE_ALL = 255
};

#define	IGNORE_LABELS_MAX 7
struct ignore_label ignore_labels[IGNORE_LABELS_MAX];

struct ignore_label {
	int level;
	char *name;
};

list_t userlist;

int userlist_read();
int userlist_write();
#ifdef WITH_WAP
int userlist_write_wap();
#endif
void userlist_write_crash();
void userlist_clear_status(uin_t uin);
struct userlist *userlist_add(uin_t uin, const char *display);
int userlist_remove(struct userlist *u);
int userlist_replace(struct userlist *u);
void userlist_send();
struct userlist *userlist_find(uin_t uin, const char *display);
char *userlist_dump();
void userlist_clear();
#define userlist_free userlist_clear
int userlist_set(const char *contacts, int config);
char userlist_type(struct userlist *u);

int ignored_add(uin_t uin, int level);
int ignored_remove(uin_t uin);
int ignored_check(uin_t uin);

int ignore_flags(const char *str);
const char *ignore_format(int level);

int blocked_add(uin_t uin);
int blocked_remove(uin_t uin);

int group_add(struct userlist *u, const char *group);
int group_remove(struct userlist *u, const char *group);
int group_member(struct userlist *u, const char *group);
char *group_to_string(list_t l, int meta, int sep);
list_t group_init(const char *groups);

uin_t str_to_uin(const char *text);
int valid_nick(const char *nick);
uin_t get_uin(const char *text);
const char *format_user(uin_t uin);

#endif /* __USERLIST_H */
