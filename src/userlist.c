/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Piotr Domagalski <szalik@szalik.net>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "dynstuff.h"
#include "libgadu.h"
#ifndef HAVE_STRLCAT
#  include "../compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "../compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "userlist.h"
#include "vars.h"
#include "xmalloc.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

list_t userlist = NULL;

struct ignore_label ignore_labels[IGNORE_LABELS_COUNT + 1] = {
	{ IGNORE_STATUS, "status" },
	{ IGNORE_STATUS_DESCR, "descr" },
	{ IGNORE_NOTIFY, "notify" },
	{ IGNORE_MSG, "msg" },
	{ IGNORE_DCC, "dcc" },
	{ IGNORE_EVENTS, "events" },
	{ 0, NULL }
};

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
		return 1;

	return strcasecmp(a->display, b->display);
}

/*
 * userlist_read()
 *
 * wczytuje listê kontaktów z pliku ~/.gg/userlist w postaci eksportu
 * tekstowego listy kontaktów windzianego klienta.
 *
 * 0/-1
 */
int userlist_read()
{
	const char *filename;
	char *buf;
	FILE *f;

	if (!(filename = prepare_path("userlist", 0)))
		return -1;
	
	if (!(f = fopen(filename, "r")))
		return -1;

	while ((buf = read_file(f))) {
		struct userlist u;
		char **entry, *uin;
		int i, count;
		
		memset(&u, 0, sizeof(u));
			
		if (buf[0] == '#' || (buf[0] == '/' && buf[1] == '/')) {
			xfree(buf);
			continue;
		}

		entry = array_make(buf, ";", 8, 0, 0);

		uin = entry[6];
		if (!strncasecmp(uin, "gg:", 3))
			uin += 3;

		if ((count = array_count(entry)) < 7 || !(u.uin = atoi(uin))) {
			array_free(entry);
			xfree(buf);
			continue;
		}

		for (i = 0; i < 6; i++) {
			if (!strcmp(entry[i], "(null)") || !strcmp(entry[i], "")) {
				xfree(entry[i]);
				entry[i] = NULL;
			}
		}
			
		u.first_name = xstrdup(entry[0]);
		u.last_name = xstrdup(entry[1]);
		u.nickname = xstrdup(entry[2]);
		if (entry[3] && !valid_nick(entry[3]))
			u.display = saprintf("_%s", entry[3]);
		else
			u.display = xstrdup(entry[3]);
		u.mobile = xstrdup(entry[4]);
		u.groups = group_init(entry[5]);
		u.status = GG_STATUS_NOT_AVAIL;
		if (entry[7])
			u.foreign = saprintf(";%s", entry[7]);
		else
			u.foreign = xstrdup("");

		for (i = 0; i < count; i++)
			xfree(entry[i]);

		xfree(entry);
		xfree(buf);

		list_add_sorted(&userlist, &u, sizeof(u), userlist_compare);
	}
	
	fclose(f);

	return 0;
}

/*
 * userlist_set()
 *
 * ustawia listê kontaktów na podan±.
 *
 * 0/-1
 */
int userlist_set(const char *contacts, int config)
{
	string_t vars = NULL;
	char *buf, *cont, *contsave;

	if (!contacts)
		return -1;

	userlist_clear();

	if (config)
		vars = string_init(NULL);
	
	contsave = cont = xstrdup(contacts);
	
	while ((buf = gg_get_line(&cont))) {
		struct userlist u;
		char **entry, *uin;
		int i;
		
		memset(&u, 0, sizeof(u));
			
		if (buf[0] == '#' || (buf[0] == '/' && buf[1] == '/'))
			continue;

		if (!strncmp(buf, "__config", 8)) {
			char **entry;

			if (!config)
				continue;
			
			entry = array_make(buf, ";", 7, 0, 0);
			
			for (i = 1; i < 6; i++)
				string_append(vars, entry[i]);

			array_free(entry);

			continue;
		}

		entry = array_make(buf, ";", 8, 0, 0);
		
		uin = entry[6];
		if (!strncasecmp(uin, "gg:", 3))
			uin += 3;
		
		if (array_count(entry) < 7 || !(u.uin = atoi(uin))) {
			array_free(entry);
			continue;
		}

		for (i = 0; i < 6; i++) {
			if (!strcmp(entry[i], "(null)") || !strcmp(entry[i], "")) {
				xfree(entry[i]);
				entry[i] = NULL;
			}
		}

		u.first_name = xstrdup(entry[0]);
		u.last_name = xstrdup(entry[1]);
		u.nickname = xstrdup(entry[2]);
		if (entry[3] && !valid_nick(entry[3]))
			u.display = saprintf("_%s", entry[3]);
		else
			u.display = xstrdup(entry[3]);
		u.mobile = xstrdup(entry[4]);
		u.groups = group_init(entry[5]);
		u.status = GG_STATUS_NOT_AVAIL;
		if (entry[7])
			u.foreign = saprintf(";%s", entry[7]);
		else
			u.foreign = xstrdup("");

		array_free(entry);

		list_add_sorted(&userlist, &u, sizeof(u), userlist_compare);
	}

	xfree(contsave);

	if (config) {
		char *tmp = string_free(vars, 0);
		
		gg_debug(GG_DEBUG_MISC, "// received ekg variables digest: %s\n", tmp);

		if (variable_undigest(tmp)) {
			xfree(tmp);
			return -1;
		}

		if (sess && sess->state == GG_STATE_CONNECTED) {
			if (config_reason) {
				iso_to_cp(config_reason);
				gg_change_status_descr(sess, config_status, config_reason);
				cp_to_iso(config_reason);
			} else
				gg_change_status(sess, config_status);
		}

		xfree(tmp);
	}

	return 0;
}

/*
 * userlist_dump()
 *
 * zapisuje listê kontaktów w postaci tekstowej.
 *
 * zwraca zaalokowany bufor, który nale¿y zwolniæ.
 */
char *userlist_dump()
{
	string_t s = string_init(NULL);
	list_t l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		char *groups, *line;

		groups = group_to_string(u->groups, 1, 0);
		
		line = saprintf("%s;%s;%s;%s;%s;%s;%d%s\r\n",
			(u->first_name) ? u->first_name : "",
			(u->last_name) ? u->last_name : "",
			(u->nickname) ? u->nickname : ((u->display) ? u->display: ""),
			(u->display) ? u->display : "",
			(u->mobile) ? u->mobile : "",
			groups,
			u->uin,
			(u->foreign) ? u->foreign : "");
		
		string_append(s, line);

		xfree(line);
		xfree(groups);
	}	

	return string_free(s, 0);
}

/*
 * userlist_write()
 *
 * zapisuje listê kontaktów w pliku ~/.gg/userlist
 */
int userlist_write()
{
	const char *filename;
	char *contacts;
	FILE *f;

	if (!(contacts = userlist_dump()))
		return -1;
	
	if (!(filename = prepare_path("userlist", 1))) {
		xfree(contacts);
		return -1;
	}
	
	if (!(f = fopen(filename, "w"))) {
		xfree(contacts);
		return -2;
	}
	fchmod(fileno(f), 0600);
	fputs(contacts, f);
	fclose(f);
	
	xfree(contacts);

	return 0;
}

#ifdef WITH_WAP
/*
 * userlist_write_wap()
 *
 * zapisuje listê kontaktów w pliku ~/.gg/wapstatus
 */
int userlist_write_wap()
{
	const char *filename;
	list_t l;
	FILE *f;

	if (!(filename = prepare_path("wapstatus", 1)))
		return -1;

	if (!(f = fopen(filename, "w")))
		return -1;

	fchmod(fileno(f), 0600);
	fprintf(f, "%s\n", (sess) ? "C" : "D");

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		
		fprintf(f, "%s:%d%s%s\n", u->display, u->status, (u->descr) ? ":" : "", (u->descr) ? u->descr : "");
	}

	fclose(f);

	return 0;
}
#endif

/*
 * userlist_write_crash()
 *
 * zapisuje listê kontaktów w sytuacji kryzysowej jak najmniejszym
 * nak³adem pamiêci i pracy.
 */
void userlist_write_crash()
{
	list_t l;
	char name[32];
	FILE *f;

	chdir(config_dir);
	
	snprintf(name, sizeof(name), "userlist.%d", (int) getpid());
	if (!(f = fopen(name, "w")))
		return;

	chmod(name, 0400);
		
	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		list_t m;
		
		fprintf(f, "%s;%s;%s;%s;%s;", 
			(u->first_name) ? u->first_name : "",
			(u->last_name) ? u->last_name : "",
			(u->nickname) ? u->nickname : ((u->display) ? u->display: ""),
			(u->display) ? u->display: "",
			(u->mobile) ? u->mobile : "");
		
		for (m = u->groups; m; m = m->next) {
			struct group *g = m->data;

			if (m != u->groups)
				fprintf(f, ",");

			fprintf(f, "%s", g->name);
		}
		
		fprintf(f, ";%u%s\r\n", u->uin, u->foreign);
	}	

	fclose(f);
}

/*
 * userlist_clear_status()
 *
 * czy¶ci stan u¿ytkowników na li¶cie. je¶li uin != 0 to
 * to czy¶ci danego u¿ytkownika.
 *
 *  - uin.
 */
void userlist_clear_status(uin_t uin)
{
        list_t l;

        for (l = userlist; l; l = l->next) {
                struct userlist *u = l->data;

		if (!uin || uin == u->uin) {
			u->status = GG_STATUS_NOT_AVAIL;
			memset(&u->ip, 0, sizeof(struct in_addr));
			memset(&u->last_ip, 0, sizeof(struct in_addr));
			u->port = 0;
			u->last_port = 0;
			xfree(u->descr);
			xfree(u->last_descr);
			u->descr = NULL;
			u->last_descr = NULL;
		}
        }
}

/*
 * userlist_clear()
 *
 * czy¶ci listê u¿ytkowników.
 */
void userlist_clear()
{
	while (userlist)
		userlist_remove(userlist->data);
}

/*
 * userlist_add()
 *
 * dodaje u¿ytkownika do listy.
 *
 *  - uin,
 *  - display.
 */
struct userlist *userlist_add(uin_t uin, const char *display)
{
	struct userlist u;

	memset(&u, 0, sizeof(u));

	u.uin = uin;
	u.status = GG_STATUS_NOT_AVAIL;
	u.display = xstrdup(display);

	return list_add_sorted(&userlist, &u, sizeof(u), userlist_compare);
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
	list_t l;

	if (!u)
		return -1;
	
	xfree(u->first_name);
	xfree(u->last_name);
	xfree(u->nickname);
	xfree(u->display);
	xfree(u->mobile);
	xfree(u->descr);
	xfree(u->foreign);
	xfree(u->last_descr);

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		xfree(g->name);
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
	if (!u)
		return -1;
	if (list_remove(&userlist, u, 0))
		return -1;
	if (!list_add_sorted(&userlist, u, 0, userlist_compare))
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
struct userlist *userlist_find(uin_t uin, const char *display)
{
	list_t l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

                if (uin && u->uin == uin)
			return u;
                if (display && u->display && !strcasecmp(u->display, display))
                        return u;
        }

        return NULL;
}

/*
 * userlist_find_mobile()
 *
 * znajduje u¿ytkownika, do którego nale¿y podany numer telefonu.
 *
 * - mobile.
 */
struct userlist *userlist_find_mobile(const char *mobile)
{
	list_t l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (mobile && u->mobile && !strcasecmp(u->mobile, mobile))
			return u;
	}

	return NULL;
}

/*
 * userlist_type()
 *
 * zwraca rodzaj u¿ytkownika dla funkcji gg_*_notify_ex().
 *
 *  - u - wpis u¿ytkownika
 *
 * GG_USER_*
 */
char userlist_type(struct userlist *u)
{
	char res = GG_USER_NORMAL;

	if (!u)
		return res;
	
	if (group_member(u, "__offline"))
		res = GG_USER_OFFLINE;
		
	if (group_member(u, "__blocked"))
		res = GG_USER_BLOCKED;

	return res;
}

/*
 * str_to_uin()
 *
 * funkcja, która zajmuje siê zamian± stringa na 
 * liczbê i sprawdzeniem, czy to prawid³owy uin.
 *
 * zwraca uin lub 0 w przypadku b³êdu.
 */
uin_t str_to_uin(const char *text)
{
	char *tmp;
	long num;

	if (!text)
		return 0;

	errno = 0;
	num = strtol(text, &tmp, 0);

	if (*text == '\0' || *tmp != '\0')
		return 0;

	if ((errno == ERANGE || (num == LONG_MAX || num == LONG_MIN)) || num > UINT_MAX || num < 0)
		return 0;

	return (uin_t) num;
}

/*
 * valid_nick()
 *
 * sprawdza, czy nick nie zawiera znaków specjalnych,
 * które mog³yby powodowaæ problemy.
 *
 * zwraca 1 je¶li nick jest w porz±dku, w przeciwnym razie 0.
 */
int valid_nick(const char *nick)
{
	int i;
	const char *wrong[] = { "(null)", "__debug", "__status",
				 "__current", "__contacts", "*", "$", NULL };

	if (!nick)
		return 0;

	for (i = 0; wrong[i]; i++) {
		if (!strcasecmp(nick, wrong[i]))
			return 0;
	}

	if (nick[0] == '@' || nick[0] == '#' || strchr(nick, ','))
		return 0;

	return 1;
}

/*
 * get_uin()
 *
 * je¶li podany tekst jest liczb± (ale nie jednocze¶nie nazw± u¿ytkownika),
 * zwraca jej warto¶æ. je¶li jest nazw± u¿ytkownika w naszej li¶cie kontaktów,
 * zwraca jego numerek. je¶li tekstem jestem znak ,,$'', pyta ui o aktualnego
 * rozmówcê i zwraca jego uin. inaczej zwraca zero.
 *
 *  - text.
 */
uin_t get_uin(const char *text)
{
	uin_t uin = str_to_uin(text);
	struct userlist *u = userlist_find(uin, text);

	if (u)
		return u->uin;

	if (text && !strcmp(text, "$"))
		ui_event("command", 0, "query-current", &uin, NULL);

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
const char *format_user(uin_t uin)
{
	struct userlist *u = userlist_find(uin, NULL);
	static char buf[100], *tmp;
	
	if (!u || !u->display)
		tmp = format_string(format_find("unknown_user"), itoa(uin));
	else
		tmp = format_string(format_find("known_user"), u->display, itoa(uin));
	
	strlcpy(buf, tmp, sizeof(buf));
	
	xfree(tmp);

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
	struct userlist *u = userlist_find(uin, NULL);
	list_t l;
	int level;

	if (!u)
		return -1;

	if (!(level = ignored_check(uin)))
		return -1;

	for (l = u->groups; l; ) {
		struct group *g = l->data;

		l = l->next;

		if (strncasecmp(g->name, "__ignored", 9))
			continue;

		xfree(g->name);
		list_remove(&u->groups, g, 1);
	}

	if (!u->display && !u->groups) {
		userlist_remove(u);
		return 0;
	}

	if (sess && (level & IGNORE_STATUS || level & IGNORE_STATUS_DESCR)) {
		gg_remove_notify_ex(sess, u->uin, userlist_type(u));
		gg_add_notify_ex(sess, u->uin, userlist_type(u));
	}

	return 0;
}

/*
 * ignored_add()
 *
 * dopisuje do listy ignorowanych numerków.
 *
 *  - uin.
 *  - level.
 */
int ignored_add(uin_t uin, int level)
{
	struct userlist *u;
	char *tmp;

	if (ignored_check(uin))
		return -1;
	
	if (!(u = userlist_find(uin, NULL)))
		u = userlist_add(uin, NULL);

	tmp = saprintf("__ignored_%d", level);
	group_add(u, tmp);
	xfree(tmp);

	if (level & IGNORE_STATUS)
		u->status = GG_STATUS_NOT_AVAIL;

	if (level & IGNORE_STATUS_DESCR)
		u->status = ekg_hide_descr_status(u->status);
	
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
	struct userlist *u = userlist_find(uin, NULL);
	list_t l;

	if (!u)
		return 0;

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!strcasecmp(g->name, "__ignored"))
			return IGNORE_ALL;

		if (!strncasecmp(g->name, "__ignored_", 10))
			return atoi(g->name + 10);
	}

	return 0;
}

/*
 * ignore_flags()
 *
 * zamienia ³añcuch znaków na odpowiedni
 * poziom ignorowania w postaci liczby.
 */
int ignore_flags(const char *str)
{
	int x, y, ret = 0;
	char **arr;

	if (!str)
		return ret;

	arr = array_make(str, "|,:", 0, 1, 0);

	for (x = 0; arr[x]; x++) {
		if (!strcmp(arr[x], "*")) {
			ret = IGNORE_ALL;
			break;
		}

		for (y = 0; ignore_labels[y].name; y++)
			if (!strcasecmp(arr[x], ignore_labels[y].name))
				ret |= ignore_labels[y].level;
	}

	array_free(arr);

	return ret;
}

/*
 * ignore_format()
 *
 * zwraca statyczny ³añcuch znaków reprezentuj±cy
 * dany poziom ignorowania.
 */
const char *ignore_format(int level)
{
	static char buf[200];
	int i, comma = 0;

	buf[0] = 0;

	if (level == IGNORE_ALL)
		return "*";

	for (i = 0; ignore_labels[i].name; i++) {
		if (level & ignore_labels[i].level) {
			if (comma++)
				strlcat(buf, ",", sizeof(buf));

			strlcat(buf, ignore_labels[i].name, sizeof(buf));
		}
	}

	return buf;
}

/*
 * blocked_remove()
 *
 * usuwa z listy blokowanych numerków.
 *
 *  - uin.
 */
int blocked_remove(uin_t uin)
{
	struct userlist *u = userlist_find(uin, NULL);
	list_t l;

	if (!u)
		return -1;

	if (!group_member(u, "__blocked"))
		return -1;

	gg_remove_notify_ex(sess, u->uin, userlist_type(u));

	for (l = u->groups; l; ) {
		struct group *g = l->data;

		l = l->next;

		if (strcasecmp(g->name, "__blocked"))
			continue;

		xfree(g->name);
		list_remove(&u->groups, g, 1);
	}

	if (!u->display && !u->groups)
		userlist_remove(u);
	else
		gg_add_notify_ex(sess, u->uin, userlist_type(u));

	return 0;
}

/*
 * blocked_add()
 *
 * dopisuje do listy blokowanych numerków.
 *
 *  - uin.
 */
int blocked_add(uin_t uin)
{
	struct userlist *u = userlist_find(uin, NULL);

	if (u && group_member(u, "__blocked"))
		return -1;
	
	if (!u)
		u = userlist_add(uin, NULL);
	else
		gg_remove_notify_ex(sess, uin, userlist_type(u));

	group_add(u, "__blocked");

	gg_add_notify_ex(sess, uin, userlist_type(u));
	
	return 0;
}

/*
 * userlist_send()
 *
 * wysy³a do serwera userlistê, wywo³uj±c gg_notify_ex()
 */
void userlist_send()
{
        list_t l;
        uin_t *uins;
	char *types;
        int i, count;

	count = list_count(userlist);

        uins = xmalloc(count * sizeof(uin_t));
	types = xmalloc(count * sizeof(char));

	for (i = 0, l = userlist; l; i++, l = l->next) {
		struct userlist *u = l->data;

                uins[i] = u->uin;
		types[i] = userlist_type(u);
	}

        gg_notify_ex(sess, uins, types, count);

        xfree(uins);
	xfree(types);
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
 */
int group_add(struct userlist *u, const char *group)
{
	struct group g;
	list_t l;

	if (!u || !group)
		return -1;

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!strcasecmp(g->name, group))
			return -1;
	}
	
	g.name = xstrdup(group);

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
int group_remove(struct userlist *u, const char *group)
{
	list_t l;

	if (!u || !group)
		return -1;
	
	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!strcasecmp(g->name, group)) {
			xfree(g->name);
			list_remove(&u->groups, g, 1);
			
			return 0;
		}
	}
	
	return -1;
}

/*
 * group_member()
 *
 * sprawdza czy u¿ytkownik jest cz³onkiem danej grupy.
 *
 * zwraca 1 je¶li tak, 0 je¶li nie.
 */
int group_member(struct userlist *u, const char *group)
{
	list_t l;

	if (!u || !group)
		return 0;

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!strcasecmp(g->name, group))
			return 1;
	}

	return 0;
}

/*
 * group_init()
 *
 * inicjuje listê grup u¿ytkownika na podstawie danego ci±gu znaków,
 * w którym kolejne nazwy grup s± rozdzielone przecinkiem.
 * 
 *  - names - nazwy grup.
 *
 * zwraca listê `struct group' je¶li siê uda³o, inaczej NULL.
 */
list_t group_init(const char *names)
{
	list_t l = NULL;
	char **groups;
	int i;

	if (!names)
		return NULL;

	groups = array_make(names, ",", 0, 1, 0);

	for (i = 0; groups[i]; i++) {
		struct group g;

		g.name = xstrdup(groups[i]);
		list_add_sorted(&l, &g, sizeof(g), group_compare);
	}
	
	array_free(groups);
	
	return l;
}

/*
 * group_to_string()
 *
 * zmienia listê grup na ci±g znaków rodzielony przecinkami.
 *
 *  - groups - lista grup.
 *  - meta - czy do³±czyæ ,,meta-grupy''?
 *  - sep - czy oddzielaæ przecinkiem _i_ spacj±?
 *
 * zwraca zaalokowany ci±g znaków lub NULL w przypadku b³êdu.
 */
char *group_to_string(list_t groups, int meta, int sep)
{
	string_t foo = string_init(NULL);
	list_t l;
	int comma = 0;

	for (l = groups; l; l = l->next) {
		struct group *g = l->data;

		if (!meta && !strncmp(g->name, "__", 2)) {
			comma = 0;
			continue;
		}

		if (comma)
			string_append(foo, (sep) ? ", " : ",");

		comma = 1;

		string_append(foo, g->name);
	}

	return string_free(foo, 0);
}
