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

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <time.h>

#include "libgadu.h"
#include "dynstuff.h"
#include "stuff.h"

struct userlist {
	char *first_name;	/* imiê */
	char *last_name;	/* nazwisko */
	char *nickname;		/* pseudonim */
	char *display;		/* wy¶wietlania nazwa */
	char *mobile;		/* komórka */
	char *email;		/* adres e-mail */
	list_t groups;		/* grupy, do których nale¿y */
	uin_t uin;		/* numer */
	int status;		/* aktualny stan */
	char *descr;		/* opis/powód stanu */
	struct in_addr ip;	/* adres ip */
	unsigned short port;	/* port */
	int protocol;		/* wersja protoko³u */
	char *foreign;		/* dla kompatybilno¶ci */
	time_t last_seen;	/* jesli jest niedostepny/ukryty, to od kiedy */
	char *last_descr;	/* j.w. ostatni opis */
	struct in_addr last_ip; /* j.w. ostatni adres ip */
	unsigned short last_port; /* j.w. ostatni port */
	int image_size;		/* maksymalny rozmiar obrazków */
};

struct group {
	char *name;
};

enum ignore_t {
	IGNORE_STATUS = TOGGLE_BIT(1),
	IGNORE_STATUS_DESCR = TOGGLE_BIT(2),
	IGNORE_MSG = TOGGLE_BIT(3),
	IGNORE_DCC = TOGGLE_BIT(4),
	IGNORE_EVENTS = TOGGLE_BIT(5),
	IGNORE_NOTIFY = TOGGLE_BIT(6),
	IGNORE_SMSAWAY = TOGGLE_BIT(7),
	IGNORE_DISPLAY = TOGGLE_BIT(8),
	
	IGNORE_ALL = 255
};

struct ignore_label {
	int level;
	char *name;
};

#define	IGNORE_LABELS_COUNT 8
struct ignore_label ignore_labels[IGNORE_LABELS_COUNT + 1];

list_t userlist;

int userlist_read(void);
int userlist_write(int pid);
#ifdef WITH_WAP
int userlist_write_wap(void);
#endif
void userlist_write_crash(void);
void userlist_clear_status(uin_t uin);
struct userlist *userlist_add(uin_t uin, const char *display);
int userlist_remove(struct userlist *u, int full);
int userlist_replace(struct userlist *u);
void userlist_send(void);
struct userlist *userlist_find(uin_t uin, const char *display);
struct userlist *userlist_find_mobile(const char *mobile);
char *userlist_dump(void);
void userlist_clear(void);
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
