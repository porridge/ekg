/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
 *  port to ekg2:
 *  Copyright (C) 2007 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkclist.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkvscrollbar.h>
#include <gdk/gdkkeysyms.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include <dirent.h>

#include "commands.h"
#include "userlist.h"
#include "vars.h"
#include "themes.h"
#include "xmalloc.h"

#include "ui-gtk.h"
// #include "completion.h"
#define COMPLETION_MAXLEN 2048		/* rozmiar linii */

#define GTK_BINDING_FUNCTION(x) int x(GtkWidget *wid, GdkEventKey *evt, char *d1, window_t *sess)

/* These are cp'ed from history.c --AGL */
#define STATE_SHIFT     GDK_SHIFT_MASK
#define	STATE_ALT	GDK_MOD1_MASK
#define STATE_CTRL	GDK_CONTROL_MASK

/*****************************************************************************************************/
static char **completions = NULL;	/* lista dopełnień */

static void dcc_generator(const char *text, int len) {
	const char *words[] = { "close", "get", "send", "list", "resume", "rsend", "rvoice", "voice", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

static void command_generator(const char *text, int len) {
	const char *slash = "", *dash = "";
	list_t l;

	if (*text == '/') {
		slash = "/";
		text++;
		len--;
	}

	if (*text == '^') {
		dash = "^";
		text++;
		len--;
	}

	if (window_current->target)
		slash = "/";
			
	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (!strncasecmp(text, c->name, len))
			array_add(&completions, saprintf("%s%s%s", slash, dash, c->name));
	}
}

static void events_generator(const char *text, int len) {
	int i;
	const char *tmp = NULL;
	char *pre = NULL;

	if ((tmp = strrchr(text, '|')) || (tmp = strrchr(text, ','))) {
		char *foo;

		pre = xstrdup(text);
		foo = strrchr(pre, *tmp);
		*(foo + 1) = 0;

		len -= tmp - text + 1;
		tmp = tmp + 1;
	} else
		tmp = text;

	for (i = 0; event_labels[i].name; i++)
		if (!strncasecmp(tmp, event_labels[i].name, len))
			array_add(&completions, ((tmp == text) ? xstrdup(event_labels[i].name) : saprintf("%s%s", pre, event_labels[i].name)));
	xfree(pre);
}

static void ignorelevels_generator(const char *text, int len) {
	int i;
	const char *tmp = NULL;
	char *pre = NULL;

	if ((tmp = strrchr(text, '|')) || (tmp = strrchr(text, ','))) {
		char *foo;

		pre = xstrdup(text);
		foo = strrchr(pre, *tmp);
		*(foo + 1) = 0;

		len -= tmp - text + 1;
		tmp = tmp + 1;
	} else
		tmp = text;

	for (i = 0; ignore_labels[i].name; i++)
		if (!strncasecmp(tmp, ignore_labels[i].name, len))
			array_add(&completions, ((tmp == text) ? xstrdup(ignore_labels[i].name) : saprintf("%s%s", pre, ignore_labels[i].name)));
	xfree(pre);
}

static void unknown_uin_generator(const char *text, int len) {
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		if (send_nicks[i] && xisdigit(send_nicks[i][0]) && !strncasecmp(text, send_nicks[i], len))
			if (!array_contains(completions, send_nicks[i], 0))
				array_add(&completions, xstrdup(send_nicks[i]));
	}
}

static void known_uin_generator(const char *text, int len) {
	list_t l;
	int done = 0;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (u->display && u->uin && !strncasecmp(text, u->display, len)) {
			array_add_check(&completions, xstrdup(u->display), 1);
			done = 1;
		}
	}

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (!done && u->uin && !strncasecmp(text, itoa(u->uin), len))
			array_add_check(&completions, xstrdup(itoa(u->uin)), 1);
	}

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;

		if (!strncasecmp(text, c->name, len))
			array_add_check(&completions, xstrdup(c->name), 1);
	}

	unknown_uin_generator(text, len);
}

static void variable_generator(const char *text, int len) {
	list_t l;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		if (v->type == VAR_FOREIGN || !v->ptr)
			continue;

		if (*text == '-') {
			if (!strncasecmp(text + 1, v->name, len - 1))
				array_add(&completions, saprintf("-%s", v->name));
		} else {
			if (!strncasecmp(text, v->name, len))
				array_add(&completions, xstrdup(v->name));
		}
	}
}

static void ignored_uin_generator(const char *text, int len) {
	list_t l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (!ignored_check(u->uin))
			continue;

		if (!u->display) {
			if (!strncasecmp(text, itoa(u->uin), len))
				array_add(&completions, xstrdup(itoa(u->uin)));
		} else {
			if (u->display && !strncasecmp(text, u->display, len))
				array_add(&completions, xstrdup(u->display));
		}
	}
}

static void blocked_uin_generator(const char *text, int len) {
	list_t l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (!group_member(u, "__blocked"))
			continue;

		if (!u->display) {
			if (!strncasecmp(text, itoa(u->uin), len))
				array_add(&completions, xstrdup(itoa(u->uin)));
		} else {
			if (u->display && !strncasecmp(text, u->display, len))
				array_add(&completions, xstrdup(u->display));
		}
	}

}

static void empty_generator(const char *text, int len) {

}

static void file_generator(const char *text, int len) {
	struct dirent **namelist = NULL;
	char *dname, *tmp;
	const char *fname;
	int count, i;

	/* `dname' zawiera nazwę katalogu z kończącym znakiem `/', albo
	 * NULL, jeśli w dopełnianym tekście nie ma ścieżki. */

	dname = xstrdup(text);

	if ((tmp = strrchr(dname, '/'))) {
		tmp++;
		*tmp = 0;
	} else {
		xfree(dname);
		dname = NULL;
	}

	/* `fname' zawiera nazwę szukanego pliku */

	fname = strrchr(text, '/');

	if (fname)
		fname++;
	else
		fname = text;

again:
	/* zbierzmy listę plików w żądanym katalogu */
	
	count = scandir((dname) ? dname : ".", &namelist, NULL, alphasort);

/* 	ui_debug("dname=\"%s\", fname=\"%s\", count=%d\n", (dname) ? dname : "(null)", (fname) ? fname : "(null)", count); */

	for (i = 0; i < count; i++) {
		char *name = namelist[i]->d_name, *tmp = saprintf("%s%s", (dname) ? dname : "", name);
		struct stat st;
		int isdir = 0;

		if (!stat(tmp, &st))
			isdir = S_ISDIR(st.st_mode);

		xfree(tmp);

		if (!strcmp(name, ".")) {
			xfree(namelist[i]);
			continue;
		}

		/* jeśli mamy `..', sprawdź czy katalog składa się z
		 * `../../../' lub czegoś takiego. */
		
		if (!strcmp(name, "..")) {
			const char *p;
			int omit = 0;

			for (p = dname; p && *p; p++) {
				if (*p != '.' && *p != '/') {
					omit = 1;
					break;
				}
			}

			if (omit) {
				xfree(namelist[i]);
				continue;
			}
		}
		
		if (!strncmp(name, fname, strlen(fname))) {
			name = saprintf("%s%s%s", (dname) ? dname : "", name, (isdir) ? "/" : "");
			array_add(&completions, name);
		}

		xfree(namelist[i]);
        }

	/* jeśli w dopełnieniach wylądował tylko jeden wpis i jest katalogiem
	 * to wejdź do niego i szukaj jeszcze raz */

	if (array_count(completions) == 1 && strlen(completions[0]) > 0 && completions[0][strlen(completions[0]) - 1] == '/') {
		xfree(dname);
		dname = xstrdup(completions[0]);
		fname = "";
		xfree(namelist);
		namelist = NULL;
		array_free(completions);
		completions = NULL;

		goto again;
	}

	xfree(dname);
	xfree(namelist);
}

static void python_generator(const char *text, int len) {
	const char *words[] = { "exec", "list", "load", "restart", "run", "unload", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

static void window_generator(const char *text, int len) {
	const char *words[] = { "new", "kill", "move", "next", "resize", "prev", "switch", "clear", "refresh", "list", "active", "last", "dump", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

static void reason_generator(const char *text, int len) {
	if (config_reason && !strncasecmp(text, config_reason, len)) {
		char *reason;
		/* brzydkie rozwiązanie, żeby nie ruszać opisu przy dopełnianiu */
		if (xisspace(*config_reason))
			reason = saprintf("\001\\%s", config_reason);
		else
			reason = saprintf("\001%s", config_reason);
		array_add(&completions, reason);
	}
}

static struct {
	char ch;
	void (*generate)(const char *text, int len);
} generators[] = {
	{ 'u', known_uin_generator },
	{ 'U', unknown_uin_generator },
	{ 'c', command_generator },
	{ 's', empty_generator },
	{ 'i', ignored_uin_generator },
	{ 'b', blocked_uin_generator },
	{ 'v', variable_generator },
	{ 'd', dcc_generator },
	{ 'p', python_generator },
	{ 'w', window_generator },
	{ 'f', file_generator },
	{ 'e', events_generator },
	{ 'I', ignorelevels_generator },
	{ 'r', reason_generator },
	{ 0, NULL }
};

/*
 * complete()
 *
 * funkcja obsługująca dopełnianie klawiszem tab.
 * 
 * Działanie:
 * - Wprowadzona linia dzielona jest na wyrazy (uwzględniając przecinki i znaki cudzyslowia)
 * - następnie znaki separacji znajdujące się między tymi wyrazami wrzucane są do tablicy separators
 * - dalej sprawdzane jest za pomocą zmiennej word_current (określającej aktualny wyraz bez uwzględnienia
 *   przecinków - po to, aby wiedzieć czy w przypadku np funkcji /query ma być szukane dopełnienie 
 * - zmienna word odpowiada za aktualny wyraz (*z* uwzględnieniem przecinków)
 * - words - tablica zawierają wszystkie wyrazy
 * - gdy jest to możliwe szukane jest dopełnienie 
 * - gdy dopełnień jest więcej niż jedno (count > 1) wyświetlamy tylko "wspólną" część wszystkich dopełnień
 *   np ,,que'' w przypadku funkcji /query i /queue 
 * - gdy dopełnienie jest tylko jedno wyświetlamy owo dopełnienie 
 * - przy wyświetlaniu dopełnienia cała linijka konstruowana jest od nowa, ponieważ nie wiadomo w którym miejscu
 *   podany wyraz ma zostań "wsadzony", stąd konieczna jest tablica separatorów, tablica wszystkich wyrazów itd ...
 */
static void complete(int *line_index, char *line) {
	char *start, *cmd, **words, *separators;
	int i, count, word, j, words_count, word_current, open_quote;
	
	/* 
	 * jeśli uzbierano już coś to próbujemy wyświetlić wszystkie możliwości 
	 */
	if (completions) {
		int maxlen = 0;
		int count = 0;
		char *tmp;

		for (i = 0; completions[i]; i++) {
			if (strlen(completions[i]) + 2 > maxlen)
				maxlen = strlen(completions[i]) + 2;
			count++;
		}

		tmp = xmalloc(count * maxlen + 2);
		tmp[0] = '\0';

		for (i = 0; completions[i]; i++) {
			int k;

			strcat(tmp, completions[i]); 
			for (k = 0; k < maxlen - strlen(completions[i]); k++)
				strcat(tmp, " ");
		}

		print("none", tmp);

		xfree(tmp);
		return;
	}

	start = xmalloc(strlen(line) + 1);
	
	/* zerujemy co mamy */
	words = NULL;

	/* podziel (uwzględniając cudzysłowia)*/
	for (i = 0, j = 0, open_quote = 0; i < strlen(line); i++) {
		if(line[i] == '"') {
			for(j = 0,  i++; i < strlen(line) && line[i] != '"'; i++, j++)
				start[j] = line[i];
			if (i == strlen(line))
				open_quote = 1;
		} else
			for(j = 0; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; j++, i++) 
				start[j] = line[i];
		start[j] = '\0';
		/* "przewijamy" większą ilość spacji */
		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
		array_add(&words, saprintf("%s", start));
	}

	/* jeżeli ostatnie znaki to spacja, albo przecinek to trzeba dodać jeszcze pusty wyraz do words */
	if (strlen(line) > 1 && (line[strlen(line) - 1] == ' ' || line[strlen(line) - 1] == ',') && !open_quote)
		array_add(&words, xstrdup(""));

/*	 for(i = 0; i < array_count(words); i++)
		gg_debug(GG_DEBUG_MISC, "words[i = %d] = \"%s\"\n", i, words[i]);     */

	/* inicjujemy pamięc dla separators */
	if (words != NULL)
		separators = xmalloc(array_count(words) + 1);
	else
		separators = NULL;
		
	/* sprawdź, gdzie jesteśmy (uwzgędniając cudzysłowia) i dodaj separatory*/
	for (word = 0, i = 0; i < strlen(line); i++, word++) {
		if(line[i] == '"')  {
			for(j = 0, i++; i < strlen(line) && line[i] != '"'; j++, i++)
				start[j] = line[i];
		} else {
			for(j = 0; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; j++, i++) 
				start[j] = line[i];
		}
		/* "przewijamy */
		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		/* ustawiamy znak końca */
		start[j] = '\0';
		/* jeżeli to koniec linii, to kończymy tą zabawę */
		if(i >= strlen(line))
	    		break;
		/* obniżamy licznik o 1, żeby wszystko było okey, po "przewijaniu" */
		i--;
		/* hmm, jesteśmy już na wyrazie wskazywany przez kursor ? */
                if(i >= *line_index)
            		break;
	}
	
	/* dodajmy separatory - pewne rzeczy podobne do pętli powyżej */
	for (i = 0, j = 0; i < strlen(line); i++, j++) {
		if(line[i] == '"')  {
			for(i++; i < strlen(line) && line[i] != '"'; i++);
			if(i < strlen(line)) 
				separators[j] = line[i + 1];
		} else {
			for(; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; i++);
			separators[j] = line[i];
		}

		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
	}

	if (separators)
		separators[j] = '\0'; // koniec ciagu 	
	
	/* aktualny wyraz bez uwzgledniania przecinkow */
	for (i = 0, words_count = 0, word_current = 0; i < strlen(line); i++, words_count++) {
		for(; i < strlen(line) && !xisspace(line[i]); i++)
			if(line[i] == '"') 
				for(i++; i < strlen(line) && line[i] != '"'; i++);
		for(i++; i < strlen(line) && xisspace(line[i]); i++);
		if(i >= strlen(line))
			word_current = words_count + 1;
		i--;
              	/* hmm, jesteśmy już na wyrazie wskazywany przez kursor ? */
                if(i >= *line_index)
                        break;

	}

	/* trzeba pododawać trochę do liczników w spefycicznych (patrz warunki) sytuacjach */
	if (strlen(line) > 1) {
		if((xisspace(line[strlen(line) - 1]) || line[strlen(line) - 1] == ',') && word + 1== array_count(words) -1 ) 
			word++;
		if(xisspace(line[strlen(line) - 1]) && words_count == word_current) 
			word_current++;
		if(xisspace(line[strlen(line) - 1])) 
			words_count++;
	}
		
/*	gg_debug(GG_DEBUG_MISC, "word = %d\n", word);
	gg_debug(GG_DEBUG_MISC, "start = \"%s\"\n", start);   
	gg_debug(GG_DEBUG_MISC, "words_count = %d\n", words_count);	
	gg_debug(GG_DEBUG_MISC, "word_current = %d\n", word_current); */
	
/*	 for(i = 0; i < strlen(separators); i++)
		gg_debug(GG_DEBUG_MISC, "separators[i = %d] = \"%c\"\n", i, separators[i]);  */ 
	
	cmd = saprintf("/%s ", (config_tab_command) ? config_tab_command : "chat");
	
	/* nietypowe dopełnienie nicków przy rozmowach */
	if (!strcmp(line, "") || (!strncasecmp(line, cmd, strlen(cmd)) && word == 2 && send_nicks_count > 0) || (!strcasecmp(line, cmd) && send_nicks_count > 0)) {
		if (send_nicks_index >= send_nicks_count)
			send_nicks_index = 0;

		if (send_nicks_count) {
			char *nick = send_nicks[send_nicks_index++];

			snprintf(line, COMPLETION_MAXLEN, (strchr(nick, ' ')) ? "%s\"%s\" " : "%s%s ", cmd, nick);
		} else
			snprintf(line, COMPLETION_MAXLEN, "%s", cmd);
		*line_index = strlen(line);

                array_free(completions);
                array_free(words);
		xfree(start);
		xfree(separators);
		xfree(cmd);
		return;
	}
	xfree(cmd);

	/* początek komendy? */
	if (word == 0)
		command_generator(start, strlen(start));
	else {
		char *params = NULL;
		int abbrs = 0, i;
		list_t l;

		for (l = commands; l; l = l->next) {
			struct command *c = l->data;
			int len = strlen(c->name);
			char *cmd = (line[0] == '/') ? line + 1 : line;

			if (!strncasecmp(cmd, c->name, len) && xisspace(cmd[len])) {
				params = c->params;
				abbrs = 1;
				break;
			}

			for (len = 0; cmd[len] && cmd[len] != ' '; len++);

			if (!strncasecmp(cmd, c->name, len)) {
				params = c->params;
				abbrs++;
			} else
				if (params && abbrs == 1)
					break;
		}
		
		if (params && abbrs == 1) {
			for (i = 0; generators[i].ch; i++) {
				if (generators[i].ch == params[word_current - 2]) {
					int j;

					generators[i].generate(words[word], strlen(words[word]));

					for (j = 0; completions && completions[j]; j++) {
						string_t s;

						if (!strchr(completions[j], '"') && !strchr(completions[j], '\\') && !strchr(completions[j], ' '))
							continue;
						
						s = string_init("\"");
						string_append(s, completions[j]);
						string_append_c(s, '\"');

						xfree(completions[j]);
						completions[j] = string_free(s, 0);
					}
					break;
				}
			} 

		}
	}
	
	count = array_count(completions);

	/* 
	 * jeśli jest tylko jedna możlwiość na dopełnienie to drukujemy co mamy, 
	 * ewentualnie bierzemy część wyrazów w cudzysłowia ... 
	 * i uważamy oczywiście na \001 (patrz funkcje wyżej 
	 */
	if (count == 1) {
		line[0] = '\0';		
		for(i = 0; i < array_count(words); i++) {
			if(i == word) {
				if(strchr(completions[0],  '\001')) {
					if(completions[0][0] == '"')
						strncat(line, completions[0] + 2, strlen(completions[0]) - 2 - 1 );
					else
						strncat(line, completions[0] + 1, strlen(completions[0]) - 1);
				} else
			    		strcat(line, completions[0]);
				*line_index = strlen(line) + 1;
			} else {
				if(strchr(words[i], ' ')) {
					char *buf =  saprintf("\"%s\"", words[i]);
					strcat(line, buf);
					xfree(buf);
				} else
					strcat(line, words[i]);
			}
			if((i == array_count(words) - 1 && line[strlen(line) - 1] != ' ' ))
				strcat(line, " ");
			else if (line[strlen(line) - 1] != ' ') {
				size_t slen = strlen(line);
				line[slen] = separators[i];
				line[slen + 1] = '\0';
			}
		}
		array_free(completions);
		completions = NULL;
	}

	/* 
	 * gdy jest więcej możliwości to robimy podobnie jak wyżej tyle, że czasem
	 * trzeba użyć cudzysłowia tylko z jednej storny, no i trzeba dopełnić do pewnego miejsca
	 * w sumie proste rzeczy, ale jak widać jest trochę opcji ... 
	 */
	if (count > 1) {
		int common = 0;
		int tmp = 0;
		int quotes = 0;
		char *s1 = completions[0];

                if (*s1 =='"')
                      s1++;
		/* 
		 * może nie za ładne programowanie, ale skuteczne i w sumie jedyne w 100% spełniające	
	 	 * wymagania dopełniania (uwzględnianie cudzywsłowiów itp...)
		 */
		for(i=1, j = 0; ; i++, common++) { 
			for(j=0; j < count; j++) {
		                char *s2;

	                        s2 = completions[j];
        	                if (*s2 == '"') {
					s2++;
					quotes = 1;
				}
				tmp = strncasecmp(s1, s2, i);
			/* gg_debug(GG_DEBUG_MISC,"strncasecmp(\"%s\", \"%s\", %d) = %d\n", s1, s2, i, strncasecmp(s1, s2, i));  */
                                if (tmp)
                                        break;
                        }
                        if (tmp)
                                break;
                }

		
		/* gg_debug(GG_DEBUG_MISC,"common :%d\n", common); */

		if (strlen(line) + common < COMPLETION_MAXLEN) {
		
			line[0] = '\0';		
			for(i = 0; i < array_count(words); i++) {
				if(i == word) {
					if(quotes == 1 && completions[0][0] != '"') 
						strcat(line, "\"");
						
					if(completions[0][0] == '"')
						common++;
						
					if(common > 0 && completions[0][common - 1] == '"')
						common--;
						
					strncat(line, completions[0], common);
					*line_index = strlen(line);
				} else {
					if(strrchr(words[i], ' ')) {
						char *buf;
						buf = saprintf("\"%s\"", words[i]);
						strcat(line, buf);
						xfree(buf);
					} else
						strcat(line, words[i]);
				}
				
				if (separators[i]) {
					size_t slen = strlen(line);
					line[slen] = separators[i];
					line[slen + 1] = '\0';
				}
			}
		}
	}

	array_free(words);
	xfree(start);
	xfree(separators);
	return;
}

static GTK_BINDING_FUNCTION(key_action_scroll_page) {
	int value, end;
	GtkAdjustment *adj;
	enum scroll_type { PAGE_UP, PAGE_DOWN, LINE_UP, LINE_DOWN };
	int type = PAGE_DOWN;

	if (d1) {
		if (!strcasecmp(d1, "up"))
			type = PAGE_UP;
		else if (!strcasecmp(d1, "+1"))
			type = LINE_DOWN;
		else if (!strcasecmp(d1, "-1"))
			type = LINE_UP;
	}

	if (!sess)
		return 0;

	adj = GTK_RANGE(gtk_private_ui(sess)->vscrollbar)->adjustment;
	end = adj->upper - adj->lower - adj->page_size;

	switch (type) {
	case LINE_UP:
		value = adj->value - 1.0;
		break;

	case LINE_DOWN:
		value = adj->value + 1.0;
		break;

	case PAGE_UP:
		value = adj->value - (adj->page_size - 1);
		break;

	default:		/* PAGE_DOWN */
		value = adj->value + (adj->page_size - 1);
		break;
	}

	if (value < 0)
		value = 0;
	if (value > end)
		value = end;

	gtk_adjustment_set_value(adj, value);

	return 0;
}

static GTK_BINDING_FUNCTION(key_action_history_up) {
	if (history_index < HISTORY_MAX && history[history_index + 1]) {
		/* for each line? */
		if (history_index == 0) {
			xfree(history[0]);
			history[0] = xstrdup((GTK_ENTRY(wid)->text));
		}

		history_index++;

		gtk_entry_set_text(GTK_ENTRY(wid), history[history_index]);
		gtk_editable_set_position(GTK_EDITABLE(wid), -1);
	}
	return 2;
}

static GTK_BINDING_FUNCTION(key_action_history_down) {
	if (history_index > 0) {
		history_index--;

		gtk_entry_set_text(GTK_ENTRY(wid), history[history_index]);
		gtk_editable_set_position(GTK_EDITABLE(wid), -1);
	}
	return 2;
}

static GTK_BINDING_FUNCTION(key_action_tab_comp) {
	char buf[COMPLETION_MAXLEN];

	const char *text;
	int cursor_pos;

/* in fjuczer, use g_completion_new() ? */

	text = ((GTK_ENTRY(wid)->text));
	if (text[0] == '\0')
		return 1;

	cursor_pos = gtk_editable_get_position(GTK_EDITABLE(wid));

	if (strlcpy(buf, text, sizeof(buf)) >= sizeof(buf))
		printf("key_action_tab_comp(), strlcpy() UUUUUUUCH!\n");

	complete(&cursor_pos, buf);

	gtk_entry_set_text(GTK_ENTRY(wid), buf);
	gtk_editable_set_position(GTK_EDITABLE(wid), cursor_pos);

	return 2;
}

gboolean key_handle_key_press(GtkWidget *wid, GdkEventKey * evt, window_t *sess) {
	int keyval = evt->keyval;
	int mod, n;
	int was_complete = 0;
	list_t l;

	{
		sess = NULL;

		/* where did this event come from? */
		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (gtk_private_ui(w)->input_box == wid) {
				sess = w;
				if (gtk_private_ui(w)->is_tab)
					sess = window_current;
				break;
			}
		}
	}

	if (!sess) {
		printf("key_handle_key_press() FAILED (sess == NULL)\n");
		return FALSE;
	}

/*	printf("key_handle_key_press() %p [%d %d %d %s]\n", sess, evt->state, evt->keyval, evt->length, evt->string); */
	/* XXX, EMIT: KEY_PRESSED */

	mod = evt->state & (STATE_CTRL | STATE_ALT | STATE_SHIFT);

	n = -1;


/* yeah, i know it's awful. */
	if (keyval == GDK_Page_Up)		 	n = key_action_scroll_page(wid, evt, "up", sess);
	else if (keyval == GDK_Page_Down)		n = key_action_scroll_page(wid, evt, "down", sess);

	else if (keyval == GDK_Up)			n = key_action_history_up(wid, evt, NULL, sess);
	else if (keyval == GDK_Down)			n = key_action_history_down(wid, evt, NULL, sess);
	else if (keyval == GDK_Tab) {			n = key_action_tab_comp(wid, evt, NULL, sess); was_complete = 1; }

#ifndef GG_DEBUG_DISABLE
	else if (keyval == GDK_F12)			command_exec(sess->target, "/window switch 0", 0);
#endif
	else if (keyval == GDK_F1)			command_exec(sess->target, "/help", 0);

	else if (keyval == GDK_0 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 10", 0);
	else if (keyval == GDK_9 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 9", 0);
	else if (keyval == GDK_8 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 8", 0);
	else if (keyval == GDK_7 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 7", 0);
	else if (keyval == GDK_6 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 6", 0);
	else if (keyval == GDK_5 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 5", 0);
	else if (keyval == GDK_4 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 4", 0);
	else if (keyval == GDK_3 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 3", 0);
	else if (keyval == GDK_2 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 2", 0);
	else if (keyval == GDK_1 && mod == STATE_ALT)	command_exec(sess->target, "/window switch 1", 0);
#ifndef GG_DEBUG_DISABLE
	else if (keyval == '`' && mod == STATE_ALT)	command_exec(sess->target, "/window switch 0", 0);
#endif
	else if (((keyval == GDK_Q) || (keyval == GDK_q)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 11", 0);
	else if (((keyval == GDK_W) || (keyval == GDK_w)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 12", 0);
	else if (((keyval == GDK_E) || (keyval == GDK_e)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 13", 0);
	else if (((keyval == GDK_R) || (keyval == GDK_r)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 14", 0);
	else if (((keyval == GDK_T) || (keyval == GDK_t)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 15", 0);
	else if (((keyval == GDK_Y) || (keyval == GDK_y)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 16", 0);
	else if (((keyval == GDK_U) || (keyval == GDK_u)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 17", 0);
	else if (((keyval == GDK_I) || (keyval == GDK_i)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 18", 0);
	else if (((keyval == GDK_O) || (keyval == GDK_o)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 19", 0);
	else if (((keyval == GDK_P) || (keyval == GDK_p)) && mod == STATE_ALT)	command_exec(sess->target, "/window switch 20", 0);

	else if (((keyval == GDK_N) || (keyval == GDK_n)) && mod == STATE_ALT)	command_exec(sess->target, "/window new", 0);
	else if (((keyval == GDK_K) || (keyval == GDK_k)) && mod == STATE_ALT)	command_exec(sess->target, "/window kill", 0);
	else if (((keyval == GDK_A) || (keyval == GDK_a)) && mod == STATE_ALT)	command_exec(sess->target, "/window active", 0);

	else if (((keyval == GDK_N) || (keyval == GDK_n)) && mod == STATE_CTRL)	command_exec(sess->target, "/window next", 0);
	else if (((keyval == GDK_P) || (keyval == GDK_p)) && mod == STATE_CTRL)	command_exec(sess->target, "/window prev", 0);

	else if (((keyval == GDK_F) || (keyval == GDK_f)) && mod == STATE_CTRL)	n = key_action_scroll_page(wid, evt, "up", sess);
	else if (((keyval == GDK_G) || (keyval == GDK_g)) && mod == STATE_CTRL)	n = key_action_scroll_page(wid, evt, "down", sess);

	/* BINDINGI XCHATOWE */
	/* Najwazniejszy jest: F9 + kolorki. */
#if 0
	"C\no\nInsert in Buffer\nD1:\nD2!\n\n"\
	"C\nb\nInsert in Buffer\nD1:\nD2!\n\n"\
	"C\nk\nInsert in Buffer\nD1:\nD2!\n\n"\
	"S\nNext\nChange Selected Nick\nD1!\nD2!\n\n"\
	"S\nPrior\nChange Selected Nick\nD1:Up\nD2!\n\n"\
	"None\nNext\nScroll Page\nD1:Down\nD2!\n\n"\
	"None\nPrior\nScroll Page\nD1:Up\nD2!\n\n"\
	"None\nspace\nCheck For Replace\nD1!\nD2!\n\n"\
	"None\nReturn\nCheck For Replace\nD1!\nD2!\n\n"\
	"None\nKP_Enter\nCheck For Replace\nD1!\nD2!\n\n"\
	"A\nLeft\nMove front tab left\nD1!\nD2!\n\n"\
	"A\nRight\nMove front tab right\nD1!\nD2!\n\n"\
	"CS\nPrior\nMove tab family left\nD1!\nD2!\n\n"\
	"CS\nNext\nMove tab family right\nD1!\nD2!\n\n"\
	"None\nF9\nRun Command\nD1:/GUI MENU TOGGLE\nD2!\n\n"
#endif

#if 0
	binding_add("Alt-S", "/window oldest", 1, 1);
	binding_add("Alt-G", "ignore-query", 1, 1);
	binding_add("Alt-B", "backward-word", 1, 1);
	binding_add("Alt-F", "forward-word", 1, 1);
	binding_add("Alt-D", "kill-word", 1, 1);
	binding_add("Alt-Enter", "toggle-input", 1, 1);
	binding_add("Escape", "cancel-input", 1, 1);
	binding_add("Backspace", "backward-delete-char", 1, 1);
	binding_add("Ctrl-H", "backward-delete-char", 1, 1);
	binding_add("Ctrl-A", "beginning-of-line", 1, 1);
	binding_add("Home", "beginning-of-line", 1, 1);
	binding_add("Ctrl-D", "delete-char", 1, 1);
	binding_add("Delete", "delete-char", 1, 1);
	binding_add("Ctrl-E", "end-of-line", 1, 1);
	binding_add("End", "end-of-line", 1, 1);
	binding_add("Ctrl-K", "kill-line", 1, 1);
	binding_add("Ctrl-Y", "yank", 1, 1);
	binding_add("Enter", "accept-line", 1, 1);
	binding_add("Ctrl-M", "accept-line", 1, 1);
	binding_add("Ctrl-U", "line-discard", 1, 1);
	binding_add("Ctrl-V", "quoted-insert", 1, 1);
	binding_add("Ctrl-W", "word-rubout", 1, 1);
	binding_add("Alt-Backspace", "word-rubout", 1, 1);
	binding_add("Ctrl-L", "/window refresh", 1, 1);
	binding_add("Right", "forward-char", 1, 1);
	binding_add("Left", "backward-char", 1, 1);
	binding_add("Up", "previous-history", 1, 1);
	binding_add("Down", "next-history", 1, 1);
	binding_add("Ctrl-F", "backward-page", 1, 1);
	binding_add("Ctrl-G", "forward-page", 1, 1);
	binding_add("F2", "quick-list", 1, 1);
	binding_add("F3", "toggle-contacts", 1, 1);
	binding_add("F4", "next-contacts-group", 1, 1);
	binding_add("F11", "ui-ncurses-debug-toggle", 1, 1);
	binding_add("Alt-Z", "contacts-scroll-up", 1, 1);
	binding_add("Alt-X", "contacts-scroll-down", 1, 1);
#endif

#if 0
	for (l = bindings; l; l = l->next) {
		if (kb->keyval == keyval && kb->mod == mod) {

			/* Run the function */
			n = key_actions[kb->action].handler(wid, evt, kb->data1, kb->data2, sess);
			switch (n) {
				case 0:
					return 1;
				case 2:
					g_signal_stop_emission_by_name(G_OBJECT(wid), "key_press_event");
					return 1;
			}
		}
	}
#endif
	if (!was_complete) {
		/* jeśli się coś zmieniło, wygeneruj dopełnienia na nowo */
		array_free(completions);
		completions = NULL;

		/* w xchacie bylo tylko na GDK_space */
	}

	if (n == 2) {
		g_signal_stop_emission_by_name(G_OBJECT(wid), "key_press_event");
		return 1;
	}

	return (n == 0);
}

void gtk_binding_init() {

}

static void gtk_binding_destroy() {

}
