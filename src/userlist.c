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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <limits.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"
#include "libgadu.h"
#include "stuff.h"
#include "dynstuff.h"
#include "themes.h"
#include "commands.h"
#include "vars.h"
#include "userlist.h"

struct list *userlist = NULL;
struct list *ignored = NULL;

/*
 * userlist_compare()
 *
 * funkcja pomocna przy list_add_sorted().
 *
 *  - data1, data2 - dwa wpisy userlisty do porównania.
 *
 * zwraca wynik strcasecmp() na nazwach userów.
 */
static int userlist_compare(void *data1, void *data2)
{
	struct userlist *a = data1, *b = data2;
	
	if (!a || !a->display || !b || !b->display)
		return 0;

	return strcasecmp(a->display, b->display);
}

/*
 * userlist_read()
 *
 * wczytuje listê kontaktów z pliku ~/.gg/userlist. mo¿e ona byæ w postaci
 * linii ,,<numerek> <opis>'' lub w postaci eksportu tekstowego listy
 * kontaktów windzianego klienta.
 *
 *  - filename.
 */
int userlist_read(char *filename)
{
	char *buf;
	FILE *f;

	if (!filename) {
		if (!(filename = prepare_path("userlist")))
			return -1;
	}
	
	if (!(f = fopen(filename, "r")))
		return -1;

	while ((buf = read_file(f))) {
		struct userlist u;
		char *display;
		
		if (buf[0] == '#') {
			free(buf);
			continue;
		}

		if (!strchr(buf, ';')) {
			if (!(display = strchr(buf, ' '))) {
				free(buf);
				continue;
			}

			u.uin = strtol(buf, NULL, 0);
		
			if (!u.uin) {
				free(buf);
				continue;
			}

			u.first_name = NULL;
			u.last_name = NULL;
			u.nickname = NULL;
			u.display = strdup(++display);
			u.mobile = NULL;
			u.groups = NULL;

		} else {
			char **entry = array_make(buf, ";", 7, 0, 0);
			
			if (!entry[0] || !entry[1] || !entry[2] || !entry[3] || !entry[4] || !entry[5] || !entry[6] || !(u.uin = strtol(entry[6], NULL, 0))) {
				array_free(entry);
				free(buf);
				continue;
			}
			
			u.first_name = strdup_null(entry[0]);
			u.last_name = strdup_null(entry[1]);
			u.nickname = strdup_null(entry[2]);
			u.display = strdup_null(entry[3]);
			u.mobile = strdup_null(entry[4]);
			u.groups = group_init(entry[5]);

			array_free(entry);
		}

		free(buf);

		u.status = GG_STATUS_NOT_AVAIL;

		list_add_sorted(&userlist, &u, sizeof(u), userlist_compare);
	}
	
	fclose(f);

	return 0;
}

/*
 * userlist_write()
 *
 * zapisuje listê kontaktów w pliku ~/.gg/userlist
 *
 *  - filename.
 */
int userlist_write(char *filename)
{
	struct list *l;
	char *tmp;
	FILE *f;

	if (!(tmp = prepare_path("")))
		return -1;
	mkdir(tmp, 0700);

	if (!filename) {
		if (!(filename = prepare_path("userlist")))
			return -1;
	}
	
	if (!(f = fopen(filename, "w")))
		return -2;

	fchmod(fileno(f), 0600);
	
	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		char *groups = group_to_string(u->groups);

		fprintf(f, "%s;%s;%s;%s;%s;%s;%lu\r\n", (u->first_name) ?
			u->first_name : u->display, (u->last_name) ?
			u->last_name : "", (u->nickname) ? u->nickname :
			u->display, u->display, (u->mobile) ? u->mobile :
			"", groups, u->uin);
		free(groups);
	}	

	fclose(f);
	
	return 0;
}

/*
 * userlist_clear_status()
 *
 * czy¶ci stan u¿ytkowników na li¶cie.
 */
void userlist_clear_status(void) {
        struct list *l;

        for (l = userlist; l; l = l->next) {
                struct userlist *u = l->data;

                u->status = GG_STATUS_NOT_AVAIL;
        };
};

/*
 * userlist_add()
 *
 * dodaje u¿ytkownika do listy.
 *
 *  - uin,
 *  - display.
 */
int userlist_add(uin_t uin, char *display)
{
	struct userlist u;

	u.uin = uin;
	u.status = GG_STATUS_NOT_AVAIL;
	u.first_name = NULL;
	u.last_name = NULL;
	u.nickname = NULL;
	u.mobile = NULL;
	u.groups = NULL;
	u.display = strdup(display);

	list_add_sorted(&userlist, &u, sizeof(u), userlist_compare);
	
	return 0;
}

/*
 * userlist_remove()
 *
 * usuwa danego u¿ytkownika z listy kontaktów.
 *
 *  - u.
 */
int userlist_remove(struct userlist *u)
{
	struct list *l;

	if (!u)
		return -1;
	
	free(u->first_name);
	free(u->last_name);
	free(u->nickname);
	free(u->display);
	free(u->mobile);

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		free(g->name);
	}
	list_destroy(u->groups, 1);

	list_remove(&userlist, u, 1);

	return 0;
}

/*
 * userlist_replace()
 *
 * usuwa i dodaje na nowo u¿ytkownika, ¿eby zosta³ umieszczony na odpowiednim
 * (pod wzglêdem kolejno¶ci alfabetycznej) miejscu. g³upie to trochê, ale
 * przy listach jednokierunkowych nie za bardzo chce mi siê mieszaæ z
 * przesuwaniem elementów listy.
 * 
 *  - u.
 *
 * zwraca zero je¶li jest ok, -1 je¶li b³±d.
 */
int userlist_replace(struct userlist *u)
{
	if (list_remove(&userlist, u, 0))
		return -1;
	if (list_add_sorted(&userlist, u, 0, userlist_compare))
		return -1;

	return 0;
}

/*
 * userlist_find()
 *
 * znajduje odpowiedni± strukturê `userlist' odpowiadaj±c± danemu numerkowi
 * lub jego opisowi.
 *
 *  - uin,
 *  - display.
 */
struct userlist *userlist_find(uin_t uin, char *display)
{
	struct list *l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

                if (uin && u->uin == uin)
			return u;
                if (display && !strcasecmp(u->display, display))
                        return u;
        }

        return NULL;
}

/*
 * get_uin()
 *
 * je¶li podany tekst jest liczb±, zwraca jej warto¶æ. je¶li jest nazw±
 * u¿ytkownika w naszej li¶cie kontaktów, zwraca jego numerek. inaczej
 * zwraca zero.
 *
 *  - text.
 */
uin_t get_uin(char *text)
{
	uin_t uin = atoi(text);
	struct userlist *u;

	if (!uin) {
		if (!(u = userlist_find(0, text)))
			return 0;
		uin = u->uin;
	}

	return uin;
}

/*
 * format_user()
 *
 * zwraca ³adny (ew. kolorowy) tekst opisuj±cy dany numerek. je¶li jest
 * w naszej li¶cie kontaktów, formatuje u¿ywaj±c `known_user', w przeciwnym
 * wypadku u¿ywa `unknown_user'. wynik jest w statycznym buforze.
 *
 *  - uin - numerek danej osoby.
 */
char *format_user(uin_t uin)
{
	struct userlist *u = userlist_find(uin, NULL);
	static char buf[100], *tmp;
	
	if (!u)
		tmp = format_string(find_format("unknown_user"), itoa(uin));
	else
		tmp = format_string(find_format("known_user"), u->display, itoa(uin));
	
	strncpy(buf, tmp, sizeof(buf) - 1);
	
	free(tmp);

	return buf;
}

/*
 * ignored_remove()
 *
 * usuwa z listy ignorowanych numerków.
 *
 *  - uin.
 */
int ignored_remove(uin_t uin)
{
	struct list *l;

	for (l = ignored; l; l = l->next) {
		struct ignored *i = l->data;

		if (i->uin == uin) {
			list_remove(&ignored, i, 1);
			return 0;
		}
	}

	return -1;
}

/*
 * ignored_add()
 *
 * dopisuje do listy ignorowanych numerków.
 *
 *  - uin.
 */
int ignored_add(uin_t uin)
{
	struct list *l;
	struct ignored i;

	for (l = ignored; l; l = l->next) {
		struct ignored *j = l->data;

		if (j->uin == uin)
			return -1;
	}

	i.uin = uin;
	list_add(&ignored, &i, sizeof(i));
	
	return 0;
}

/*
 * ignored_check()
 *
 * czy dany numerek znajduje siê na li¶cie ignorowanych.
 *
 *  - uin.
 */
int ignored_check(uin_t uin)
{
	struct list *l;

	for (l = ignored; l; l = l->next) {
		struct ignored *i = l->data;

		if (i->uin == uin)
			return 1;
	}

	return 0;
}

/*
 * userlist_send()
 *
 * wysy³a do serwera userlistê, wywo³uj±c gg_notify()
 */
void userlist_send()
{
        struct list *l;
        uin_t *uins;
        int i, count;

	count = list_count(userlist);

        uins = (void*) malloc(count * sizeof(uin_t));

	for (i = 0, l = userlist; l; i++, l = l->next) {
		struct userlist *u = l->data;

                uins[i] = u->uin;
	}

        gg_notify(sess, uins, count);

        free(uins);
}

/*
 * group_compare()
 *
 * funkcja pomocna przy list_add_sorted().
 *
 *  - data1, data2 - dwa wpisy grup do porównania.
 *
 * zwraca wynik strcasecmp() na nazwach grup.
 */
static int group_compare(void *data1, void *data2)
{
	struct group *a = data1, *b = data2;
	
	if (!a || !a->name || !b || !b->name)
		return 0;

	return strcasecmp(a->name, b->name);
}

/*
 * group_add()
 *
 * dodaje u¿ytkownika do podanej grupy.
 *
 *  - u - wpis usera,
 *  - group - nazwa grupy.
 *
 * zwraca 0 je¶li siê uda³o, inaczej -1.
 */
int group_add(struct userlist *u, char *group)
{
	struct group g;
	
	g.name = strdup(group);

	list_add_sorted(&u->groups, &g, sizeof(g), group_compare);
	
	return 0;
}

/*
 * group_remove()
 *
 * usuwa u¿ytkownika z podanej grupy.
 *
 *  - u - wpis usera,
 *  - group - nazwa grupy.
 *
 * zwraca 0 je¶li siê uda³o, inaczej -1.
 */
int group_remove(struct userlist *u, char *group)
{
	struct list *l;

	if (!u || !group)
		return -1;
	
	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!g || !g->name)
			continue;

		if (!strcasecmp(g->name, group)) {
			free(g->name);
			list_remove(&u->groups, g, 1);
			
			return 0;
		}
	}
	
	return -1;
}

/*
 * group_init()
 *
 * inicjuje listê grup u¿ytkownika na podstawie danego ci±gu znaków,
 * w którym kolejne nazwy grup s± rozdzielone przecinkiem. NISZCZY
 * DANE WEJ¦CIOWE!
 * 
 *  - named - nazwy grup.
 *
 * zwraca listê `struct group' je¶li siê uda³o, inaczej NULL.
 */
struct list *group_init(char *names)
{
	struct list *l = NULL;
	char *token;

	while ((token = get_token(&names, ','))) {
		struct group g;

		if (!(g.name = strdup(token))) 
			continue;

		list_add_sorted(&l, &g, sizeof(g), group_compare);
	}
	
	return l;
}

/*
 * group_to_string()
 *
 * zmienia listê grup na ci±g znaków rodzielony przecinkami.
 *
 *  - l - lista grup.
 *
 * zwraca zaalokowany ci±g znaków lub NULL w przypadku b³êdu.
 */
char *group_to_string(struct list *groups)
{
	struct string *foo;
	struct list *l;

	if (!(foo = string_init(NULL)))
		return NULL;

	for (l = groups; l; l = l->next) {
		struct group *g = l->data;

		if (!g || !g->name)
			continue;
		
		if (l != groups)
			string_append_c(foo, ',');
		
		string_append(foo, g->name);
	}

	return string_free(foo, 0);
}

