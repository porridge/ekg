/* $Id$ */

/*
 *  (C) Copyright 2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                     Wojtek Bojdo³ <wojboj@htcon.pl>
 *                     Pawe³ Maziarz <drg@infomex.pl>
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

/*
 * 21:56 <@Kooba> MASZ OKIENKA ALA IRSSI?
 * 21:56 <@Kooba> no to chlopie.
 * 21:56 <@Kooba> wywalam gaima.
 * 21:56 <@Kooba> bo taki ekg obsluguje dzwiek.
 * 21:56 <@Kooba> dcc
 * 21:56 <@bkU> JA CIE!
 * 21:56 <@Kooba> i lepiej wyszukuje.
 * 21:56 <@Kooba> i lepiej sie podrywa LASKI!
 *
 * 21:59 <@bkU> elluin, mo¿esz napisaæ, tak jak w ³añcuszkach szczê¶cia
 * 21:59 <@bkU> Hosse Pedro Alvaros z po³udniowej Gwatemali
 * 21:59 <@bkU> przesta³ u¿ywaæ ekg
 * 21:59 <@bkU> i w ci±gu 7 dni straci³ wszystko
 *
 * 22:02 <@bkU> ekg mo¿e wszystko!
 * 22:02 <@bkU> ostatnio s³ysza³em przecieki, ¿e w NASA u¿ywaj± w³a¶nie ekg.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "config.h"
#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#else
#  include "../compat/dirname.h"
#endif
#ifndef HAVE_SCANDIR
#  include "../compat/scandir.h"
#endif
#include <ncurses.h>
#include <signal.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <ctype.h>
#include "stuff.h"
#include "commands.h"
#include "userlist.h"
#include "xmalloc.h"
#include "themes.h"
#include "vars.h"
#include "ui.h"
#include "version.h"
#include "mail.h"

static void ui_ncurses_loop();
static void ui_ncurses_print(const char *target, int separate, const char *line);
static void ui_ncurses_beep();
static int ui_ncurses_event(const char *event, ...);
static void ui_ncurses_postinit();
static void ui_ncurses_deinit();

static void window_switch(int id);
static void update_statusbar();

static void binding_add(const char *key, const char *action, int quiet);
static void binding_delete(const char *key, int quiet);

struct screen_line {
	int len;
	char *str;
	char *attr;
	char *ts;
	int ts_len;
};

enum window_frame_t {
	WF_LEFT = 1,
	WF_TOP = 2,
	WF_RIGHT = 4,
	WF_BOTTOM = 8,
	WF_ALL = 15
};

struct window {
	WINDOW *window;		/* okno okna */

	int floating;		/* czy p³ywaj±ce? */
	int frames;		/* informacje o ramkach */
	int last_update;	/* czas ostatniego uaktualnienia */
	
	char *target;		/* nick query albo inna nazwa albo NULL */
	
	int id;			/* numer okna */
	int act;		/* czy co¶ siê zmieni³o? */
	int more;		/* pojawi³o siê co¶ poza ekranem */

	char *prompt;		/* sformatowany prompt lub NULL */
	int prompt_len;		/* d³ugo¶æ prompta lub 0 */

	int left, top;		/* pozycja (x, y) wzglêdem pocz±tku ekranu */
	int width, height;	/* wymiary okna */

	fstring_t *backlog;	/* bufor z liniami */
	int backlog_size;	/* rozmiar backloga */

	int redraw;		/* trzeba przerysowaæ przed wy¶wietleniem */

	int start;		/* od której linii zaczyna siê wy¶wietlanie */
	int lines_count;	/* ilo¶æ linii ekranowych w backlogu */
	struct screen_line *lines;	/* linie ekranowe */

	int overflow;		/* ilo¶æ nadmiarowych linii w okienku */
};

static WINDOW *status = NULL;		/* okno stanu */
static WINDOW *input = NULL;		/* okno wpisywania tekstu */
static WINDOW *contacts = NULL;		/* okno kontaktów */

#define HISTORY_MAX 1000		/* maksymalna ilo¶æ wpisów historii */
static char *history[HISTORY_MAX];	/* zapamiêtane linie */
static int history_index = 0;		/* offset w historii */

#define LINE_MAXLEN 1000		/* rozmiar linii */
static char *line = NULL;		/* wska¼nik aktualnej linii */
static char *yanked = NULL;		/* bufor z ostatnio wyciêtym tekstem */
static char **lines = NULL;		/* linie wpisywania wielolinijkowego */
static int line_start = 0;		/* od którego znaku wy¶wietlamy? */
static int line_index = 0;		/* na którym znaku jest kursor? */
static int lines_start = 0;		/* od której linii wy¶wietlamy? */
static int lines_index = 0;		/* w której linii jeste¶my? */
static char **completions = NULL;	/* lista dope³nieñ */
static list_t windows = NULL;		/* lista okien */
static struct window *window_current;	/* wska¼nik na aktualne okno */
static int input_size = 1;		/* rozmiar okna wpisywania tekstu */

int config_backlog_size = 1000;		/* maksymalny rozmiar backloga */
int config_contacts_size = 8;		/* szeroko¶æ okna kontaktów */
static int last_contacts_size = 0;	/* poprzedni rozmiar przed zmian± */
int config_contacts = 0;		/* czy ma byæ okno kontaktów */
int config_contacts_descr = 0;		/* i czy maj± byæ wy¶wietlane opisy */
struct binding *binding_map[KEY_MAX + 1];	/* mapa bindowanych klawiszy */
struct binding *binding_map_meta[KEY_MAX + 1];	/* j.w. z altem */

static void window_kill(struct window *w, int quiet);
static int window_backlog_split(struct window *w, int full);
static void window_redraw(struct window *w);
static void window_clear(struct window *w, int full);
static void window_refresh();

#define COLOR_DEFAULT (-1)

#define CONTACTS_SIZE ((config_contacts) ? (config_contacts_size + 2) : 0)

/* rozmiar okna wy¶wietlaj±cego tekst */
#define OUTPUT_SIZE (stdscr->_maxy - input_size)

/*
 * ui_debug()
 *
 * wy¶wietla szybko sformatowany tekst w aktualnym oknie. do debugowania.
 */
#define ui_debug(x...) { \
	char *ui_debug_tmp = saprintf(x); \
	ui_ncurses_print(NULL, 0, ui_debug_tmp); \
	xfree(ui_debug_tmp); \
}

/*
 * window_backlog_add()
 *
 * dodaje do bufora okna. zak³adamy dodawanie linii ju¿ podzielonych.
 * je¶li doda siê do backloga liniê zawieraj±c± '\n', bêdzie ¼le.
 *
 *  - w - wska¼nik na okno ekg
 *  - str - linijka do dodania
 *
 * zwraca rozmiar dodanej linii w liniach ekranowych.
 */
int window_backlog_add(struct window *w, fstring_t str)
{
	if (!w)
		return 0;

	if (w->backlog_size == config_backlog_size) {
		fstring_t fs = w->backlog[w->backlog_size - 1];

		xfree(fs->str);
		xfree(fs->attr);
		xfree(fs);

		w->backlog_size--;
	} else 
		w->backlog = xrealloc(w->backlog, (w->backlog_size + 1) * sizeof(fstring_t));

	memmove(&w->backlog[1], &w->backlog[0], w->backlog_size * sizeof(fstring_t));
	w->backlog[0] = str;
	w->backlog_size++;

	return window_backlog_split(w, 0);
}

/*
 * window_backlog_split()
 *
 * dzieli linie tekstu w buforze na linie ekranowe.
 * TODO: optymalizacja, dzielenie tylko tego, co zosta³o ostatnio dodane.
 *
 *  - w - okno do podzielenia
 *  - full - czy robimy pe³ne uaktualnienie?
 *
 * zwraca rozmiar w liniach ekranowych ostatnio dodanej linii.
 */
int window_backlog_split(struct window *w, int full)
{
	int i, res = 0, bottom = 0;

	if (!w)
		return 0;

	if (full && w->start == w->lines_count - w->height)
		bottom = 1;
	
	for (i = 0; i < w->lines_count; i++)
		xfree(w->lines[i].ts);
	w->lines_count = 0;
	xfree(w->lines);
	w->lines = NULL;

	for (i = w->backlog_size - 1; i >= 0; i--) {
		struct screen_line *l;
		char *str, *attr;
		time_t ts;
		int j;

		str = w->backlog[i]->str;
		attr = w->backlog[i]->attr;
		ts = w->backlog[i]->ts;

		for (;;) {
			int word = 0, width;

			if (!i)
				res++;

			w->lines_count++;
			w->lines = xrealloc(w->lines, w->lines_count * sizeof(struct screen_line));
			l = &w->lines[w->lines_count - 1];

			l->str = str;
			l->attr = attr;
			l->len = strlen(str);
			l->ts = NULL;
			l->ts_len = 0;

			if (!w->floating && config_timestamp) {
				struct tm *tm = localtime(&ts);
				char buf[100];
				strftime(buf, sizeof(buf), config_timestamp, tm);
				l->ts = xstrdup(buf);
				l->ts_len = strlen(l->ts);
			}

			width = w->width - l->ts_len;
			if ((w->frames & WF_LEFT))
				width -= 2;
			if ((w->frames & WF_RIGHT))
				width -= 2;

			if (l->len < width)
				break;
			
			for (j = 0, word = 0; j < l->len; j++) {

				if (str[j] == ' ')
					word = j + 1;

				if (j == width - 1) {
					l->len = (word) ? word : (width - 1);
					break;
				}
			}

			str += l->len;
			attr += l->len;

			if (!str[0])
				break;
		}
	}

	if (bottom) {
		w->start = w->lines_count - w->height;
		if (w->start < 0)
			w->start = 0;
	}

	if (full) {
		if (window_current->id == w->id) 
			window_redraw(w);
		else
			w->redraw = 1;
	}

	return res;
}

/*
 * window_redraw()
 *
 * przerysowuje zawarto¶æ okienka.
 *
 *  - w - okno
 */
void window_redraw(struct window *w)
{
	int x, y, left = 0, top = 0, height = w->height;
	
	werase(w->window);
	wattrset(w->window, COLOR_PAIR(4));

	if (w->floating) {
		if ((w->frames & WF_LEFT)) {
			left += 2;

			for (y = 0; y < w->height; y++)
				mvwaddch(w->window, y, 0, ACS_VLINE);
		}

		if ((w->frames & WF_RIGHT)) {
			for (y = 0; y < w->height; y++)
				mvwaddch(w->window, y, w->width - 1, ACS_VLINE);
		}
			
		if ((w->frames & WF_TOP)) {
			top++;
			height--;

			for (x = 0; x < w->width; x++)
				mvwaddch(w->window, 0, x, ACS_HLINE);
		}

		if ((w->frames & WF_BOTTOM)) {
			height--;

			for (x = 0; x < w->width; x++)
				mvwaddch(w->window, w->height - 1, x, ACS_HLINE);
		}

		if ((w->frames & WF_LEFT) && (w->frames & WF_TOP))
			mvwaddch(w->window, 0, 0, ACS_ULCORNER);

		if ((w->frames & WF_RIGHT) && (w->frames & WF_TOP))
			mvwaddch(w->window, 0, w->width - 1, ACS_URCORNER);

		if ((w->frames & WF_LEFT) && (w->frames & WF_BOTTOM))
			mvwaddch(w->window, w->height - 1, 0, ACS_LLCORNER);

		if ((w->frames & WF_RIGHT) && (w->frames & WF_BOTTOM))
			mvwaddch(w->window, w->height - 1, w->width - 1, ACS_LRCORNER);
	}

	for (y = 0; y < height && w->start + y < w->lines_count; y++) {
		struct screen_line *l = &w->lines[w->start + y];

		wattrset(w->window, A_NORMAL);

		for (x = 0; l->ts && x < l->ts_len; x++)
			mvwaddch(w->window, top + y, left + x, (unsigned char) l->ts[x]);

		for (x = 0; x < l->len; x++) {
			int attr = A_NORMAL;
			unsigned char ch = l->str[x];

			if ((l->attr[x] & 64))
				attr |= A_BOLD;

			if (!(l->attr[x] & 128)) {
				int tmp = l->attr[x] & 7;

				attr |= COLOR_PAIR((tmp) ? tmp : 16);
			}

			if (ch < 32) {
				ch += 64;
				attr |= A_REVERSE;
			}

			if (ch > 127 && ch < 160) {
				ch = '?';
				attr |= A_REVERSE;
			}

			wattrset(w->window, attr);

			mvwaddch(w->window, top + y, left + x + l->ts_len, ch);
		}
	}

	w->redraw = 0;
}

/*
 * window_clear()
 *
 * czy¶ci zawarto¶æ okna.
 */
static void window_clear(struct window *w, int full)
{
	if (!full) {
		w->start = w->lines_count;
		w->redraw = 1;
		w->overflow = w->height;
		return;
	}

	if (w->backlog) {
		int i;

		for (i = 0; i < w->backlog_size; i++) {
			xfree(w->backlog[i]->str);
			xfree(w->backlog[i]->attr);
			xfree(w->backlog[i]);
		}

		xfree(w->backlog);

		w->backlog = NULL;
		w->backlog_size = 0;
	}

	if (w->lines) {
		int i;

		for (i = 0; i < w->lines_count; i++)
			xfree(w->lines[i].ts);
		
		xfree(w->lines);

		w->lines = NULL;
		w->lines_count = 0;
	}

	w->start = 0;
	w->redraw = 1;
}

/*
 * window_find()
 *
 * szuka okna o podanym celu. zwraca strukturê opisuj±c± je.
 */
static struct window *window_find(const char *target)
{
	list_t l;

	if (!target || !strcasecmp(target, "__current"))
		return window_current;

	if (!strcasecmp(target, "__status"))
		return windows->data;
		
	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (w->target && !strcasecmp(target, w->target))
			return w;
	}

	return NULL;
}

/*
 * window_floating_update()
 *
 * uaktualnia zawarto¶æ p³ywaj±cego okna o id == n
 * lub wszystkich okienek, gdy n == 0.
 */
static void window_floating_update(int n)
{
	list_t l;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data, *tmp;

		if (n && (w->id != n))
			continue;

		if (!w->floating)
			continue;

		if (w->last_update == time(NULL))
			continue;

		w->last_update = time(NULL);

		window_clear(w, 1);
		tmp = window_current;
		window_current = w;
		command_exec(w->target, w->target);
		window_current = tmp;

		window_redraw(w);
	}
}

/*
 * window_refresh()
 *
 * wnoutrefresh()uje aktualnie wy¶wietlane okienko.
 */
static void window_refresh()
{
	list_t l;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (w->floating || window_current->id != w->id)
			continue;

		if (w->redraw)
			window_redraw(w);

		wnoutrefresh(w->window);
	}

	window_floating_update(0);	/* XXX chwilowo, ¿eby dzia³a³o */
	
	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (!w->floating)
			continue;

		touchwin(w->window);
		wnoutrefresh(w->window);
	}
	
	mvwin(status, stdscr->_maxy - input_size, 0);
	wresize(input, input_size, input->_maxx + 1);
	mvwin(input, stdscr->_maxy - input_size + 1, 0);
}

/*
 * window_switch()
 *
 * prze³±cza do podanego okna.
 */
static void window_switch(int id)
{
	list_t l;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (id != w->id || w->floating)
			continue;

		window_current = w;

		w->act = 0;
		
		if (w->redraw)
			window_redraw(w);

		if (w->floating)
			window_floating_update(id);

		touchwin(w->window);
		window_refresh();

		if (contacts)
			wnoutrefresh(contacts);
		wnoutrefresh(status);
		wnoutrefresh(input);
		doupdate();
		update_statusbar();

		break;
	}
}

/*
 * window_new_compare()
 *
 * do sortowania okienek.
 */
static int window_new_compare(void *data1, void *data2)
{
	struct window *a = data1, *b = data2;

	if (!a || !b)
		return 0;

	return a->id - b->id;
}

/*
 * window_new()
 *
 * tworzy nowe okno o podanej nazwie i id.
 */
static struct window *window_new(const char *target, int new_id)
{
	struct window w;
	list_t l;
	int id = 1, done = 0;

	if (target && *target == '*')
		id = 100;

	while (!done) {
		done = 1;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (w->id == id) {
				done = 0;
				id++;
				break;
			}
		}
	}

	if (new_id != 0)
		id = new_id;
	
	memset(&w, 0, sizeof(w));

	w.id = id;

	if (target) {
		if (*target == '*') {
			const char *tmp = index(target, '/');
			char **argv;
			
			w.floating = 1;
			
			if (!tmp)
				tmp = target + 1;
				
			w.target = xstrdup(tmp);

			argv = array_make(target + 1, ",", 5, 0, 0);

			if (argv[0])
				w.left = atoi(argv[0]);
			if (argv[0] && argv[1])
				w.top = atoi(argv[1]);
			if (argv[0] && argv[1] && argv[2])
				w.width = atoi(argv[2]);
			if (argv[0] && argv[1] && argv[2] && argv[3])
				w.height = atoi(argv[3]);
			if (argv[0] && argv[1] && argv[2] && argv[3] && argv[4] && argv[4][0] != '/')
				w.frames = atoi(argv[4]);

			array_free(argv);
		} else {
			w.target = xstrdup(target);
			w.prompt = format_string(format_find("ncurses_prompt_query"), target);
			w.prompt_len = strlen(w.prompt);
		}
	} else {
		const char *f = format_find("ncurses_prompt_none");

		if (strcmp(f, "")) {
			w.prompt = xstrdup(f);
			w.prompt_len = strlen(w.prompt);
		}
	}

	/* sprawdzamy wspó³rzêdne */
	if (w.left > stdscr->_maxx)
		w.left = 0;
	if (w.top > stdscr->_maxy)
		w.top = 0;
	if (!w.height)
		w.height = OUTPUT_SIZE;
 	if (!w.width)
		w.width = stdscr->_maxx + 1 - CONTACTS_SIZE;
	if (w.left + w.width > stdscr->_maxx + 1)
		w.width = stdscr->_maxx + 1 - w.left;
	if (w.top + w.height > stdscr->_maxy + 1)
		w.height = stdscr->_maxy + 1 - w.top;

#if 0
	if (windows)
		ui_debug("(new %d,%d,%d,%d,%d,%d)\n", w.floating, w.frames, w.left, w.top, w.width, w.height);
#endif

 	w.window = newwin(w.height, w.width, w.top, w.left);
 
	return list_add_sorted(&windows, &w, sizeof(w), window_new_compare);
}

/*
 * ui_ncurses_print()
 *
 * wy¶wietla w podanym okienku, co trzeba.
 */
static void ui_ncurses_print(const char *target, int separate, const char *line)
{
	struct window *w;
	fstring_t fs;
	list_t l;
	int count = 0, bottom = 0, prev_count;
	char *lines, *lines_save, *line2;
	string_t speech = NULL;

	switch (config_make_window) {
		case 1:
			if ((w = window_find(target)))
				break;

			if (!separate)
				w = windows->data;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (!w->target && w->id != 1) {
					w->target = xstrdup(target);
					xfree(w->prompt);
					w->prompt = format_string(format_find("ncurses_prompt_query"), target);
					w->prompt_len = strlen(w->prompt);
					print("window_id_query_started", itoa(w->id), target);
					print_window(target, 1, "query_started", target);
					print_window(target, 1, "query_started_window", target);
					break;
				}
			}

		case 2:
			if (!(w = window_find(target))) {
				if (!separate)
					w = windows->data;
				else {
					w = window_new(target, 0);
					print("window_id_query_started", itoa(w->id), target);
					print_window(target, 1, "query_started", target);
					print_window(target, 1, "query_started_window", target);
				}
			}

			if (!config_display_crap && target && !strcmp(target, "__current"))
				w = windows->data;
			
			break;
			
		default:
			/* je¶li nie ma okna, rzuæ do statusowego. */
			if (!(w = window_find(target)))
				w = windows->data;
	}

	if (w != window_current && !w->floating) {
		w->act = 1;
		update_statusbar();
	}

	if (config_speech_app)
		speech = string_init(NULL);
	
	if (w->start == w->lines_count - w->height || (w->start == 0 && w->lines_count <= w->height))
		bottom = 1;
	
	prev_count = w->lines_count;
	
	/* XXX wyrzuciæ dzielenie na linie z ekg */
	lines = lines_save = xstrdup(line);
	while ((line2 = gg_get_line(&lines))) {
		fs = reformat_string(line2);
		fs->ts = time(NULL);
		if (config_speech_app) {
			string_append(speech, fs->str);
			string_append_c(speech, '\n');
		}
		count += window_backlog_add(w, fs);
	}
	xfree(lines_save);

	w->overflow -= count;

	if (w->overflow < 0)
		w->overflow = 0;

	if (bottom)
		w->start = w->lines_count - w->height;
	else {
		if (w->backlog_size == config_backlog_size)
			w->start -= count - (w->lines_count - prev_count);
	}

	if (w->start < 0)
		w->start = 0;

	if (w->start < w->lines_count - w->height)
		w->more = 1;

	if (!w->floating) {
		window_redraw(w);
		window_refresh();
		if (contacts)
			wnoutrefresh(contacts);
		update_statusbar();
		wnoutrefresh(input);
		doupdate();
	}

	if (config_speech_app) {
		char *tmp = saprintf("%s 2> /dev/null", config_speech_app);
		FILE *f = popen(tmp, "w");

		xfree(tmp);

		if (f) {
			fprintf(f, "%s.", speech->str);
			fclose(f);
		}

		string_free(speech, 1);
	}
}


/*
 * update_contacts()
 *
 * uaktualnia listê kontaktów po prawej.
 */
static void update_contacts()
{
	int y = 0;
	list_t l;
		
	if (!config_contacts || !contacts)
		return;
	
	werase(contacts);

	wattrset(contacts, COLOR_PAIR(4));

	for (y = 0; y <= contacts->_maxy; y++)
		mvwaddch(contacts, y, 0, ACS_VLINE);

	wattrset(contacts, COLOR_PAIR(0));
	
	for (l = userlist, y = 0; l && y <= contacts->_maxy; l = l->next) {
		struct userlist *u = l->data;
		int x, z;

		if (!GG_S_A(u->status))
			continue;

		wattrset(contacts, COLOR_PAIR(3) | A_BOLD);
		
		for (x = 0; *(u->display + x) && x < config_contacts_size; x++)
			mvwaddch(contacts, y, x + 2, (unsigned char) u->display[x]);

		wattrset(contacts, COLOR_PAIR(7));

		if (config_contacts_descr && u->descr)
			for (z = 0, x++; *(u->descr + z) && x < config_contacts_size; x++, z++)
				mvwaddch(contacts, y, x + 2, (unsigned char) u->descr[z]);

		if (GG_S_D(u->status)) {
			wattrset(contacts, COLOR_PAIR(16) | A_BOLD);
			mvwaddch(contacts, y, 1, 'i');
		}
		
		y++;
	}

	for (l = userlist; l && y <= contacts->_maxy; l = l->next) {
		struct userlist *u = l->data;
		int x, z;

		if (!GG_S_B(u->status))
			continue;
		
		wattrset(contacts, COLOR_PAIR(2));

		for (x = 0; *(u->display + x) && x < config_contacts_size; x++)
			mvwaddch(contacts, y, x + 2, (unsigned char) u->display[x]);

		wattrset(contacts, COLOR_PAIR(7));

		if (config_contacts_descr && u->descr)
			for (z = 0, x++; *(u->descr + z) && x < config_contacts_size; x++, z++)
				mvwaddch(contacts, y, x + 2, (unsigned char) u->descr[z]);


		if (GG_S_D(u->status)) {
			wattrset(contacts, COLOR_PAIR(16) | A_BOLD);
			mvwaddch(contacts, y, 1, 'i');
		}
	
		y++;
	}

	for (l = userlist; l && y <= contacts->_maxy; l = l->next) {
		struct userlist *u = l->data;
		int x, z;

		if (!GG_S_I(u->status))
			continue;
		
		wattrset(contacts, COLOR_PAIR(0));

		for (x = 0; *(u->display + x) && x < config_contacts_size; x++)
			mvwaddch(contacts, y, x + 2, (unsigned char) u->display[x]);

		wattrset(contacts, COLOR_PAIR(7));

		if (config_contacts_descr && u->descr)
			for (z = 0, x++; *(u->descr + z) && x < config_contacts_size; x++, z++)
				mvwaddch(contacts, y, x + 2, (unsigned char) u->descr[z]);

		if (GG_S_D(u->status)) {
			wattrset(contacts, COLOR_PAIR(16) | A_BOLD);
			mvwaddch(contacts, y, 1, 'i');
		}
	
		y++;
	}

	wnoutrefresh(contacts);
}

/*
 * contacts_rebuild()
 *
 * wywo³ywane przy zmianach rozmiaru.
 */
void contacts_rebuild()
{
	list_t l;

	/* nie jeste¶my w ncurses */
	if (!windows)
		return;
	
	ui_screen_width = stdscr->_maxx - CONTACTS_SIZE;

	if (!config_contacts) {
		if (contacts)
			delwin(contacts);

		contacts = NULL;

		last_contacts_size = 0;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (w->floating)
				continue;

			w->width = stdscr->_maxx + 1;
			wresize(w->window, w->height, w->width);
			window_backlog_split(w, 1);
		}

		window_refresh();
		doupdate();

		return;
	}

	if (config_contacts_size == last_contacts_size)
		return;
		
	last_contacts_size = config_contacts_size;
	
	if (contacts)
		delwin(contacts);
	
	contacts = newwin(OUTPUT_SIZE, config_contacts_size + 2, 0, stdscr->_maxx - config_contacts_size - 1);

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (w->floating)
			continue;

		w->width = stdscr->_maxx + 1 - CONTACTS_SIZE;
		wresize(w->window, w->height, w->width);
		window_backlog_split(w, 1);
	}

	window_refresh();
	update_contacts();
	doupdate();
}

/*
 * update_statusbar()
 *
 * uaktualnia pasek stanu i wy¶wietla go ponownie.
 */
static void update_statusbar()
{
	const char *p;
	int i, nested = 0;

	wmove(status, 0, 0);
	if (config_display_color)
		wattrset(status, COLOR_PAIR(15));
	else
		wattrset(status, A_REVERSE);

	for (i = 0; i <= status->_maxx; i++)
		waddch(status, ' ');
	
	wmove(status, 0, 0);

	for (p = format_find("statusbar"); *p; p++) {
		if (*p == '}' && nested) {
			nested--;
			continue;
		}

		if (*p != '%') {
			waddch(status, (unsigned char) *p);
			continue;
		}
	
		p++;

		if (!*p)
			break;

#define __color(x,y,z) \
	case x: wattrset(status, COLOR_PAIR(8+z)); break; \
	case y: wattrset(status, COLOR_PAIR(8+z) | A_BOLD); break;

		if (*p != '{' && config_display_color) {
			switch (*p) {
				__color('k', 'K', 0);
				__color('r', 'R', 1);
				__color('g', 'G', 2);
				__color('y', 'Y', 3);
				__color('b', 'B', 4);
				__color('m', 'M', 5);
				__color('c', 'C', 6);
				__color('w', 'W', 7);
				case 'n':
					wattrset(status, (config_display_color) ? COLOR_PAIR(15) : A_NORMAL);
					break;
			}
			continue;
		}
#undef __color
		if (*p != '{' && !config_display_color)
			continue;

		p++;
		if (!*p)
			break;

		if (!strncmp(p, "time}", 5)) {
			struct tm *tm;
			time_t t = time(NULL);
			char tmp[16];

			tm = localtime(&t);

			strftime(tmp, sizeof(tmp), "%H:%M", tm);
			waddstr(status, tmp);
			p += 4;
		} else if (!strncmp(p, "time ", 5)) {	/* XXX naprawiæ */
			struct tm *tm;
			time_t t = time(NULL);
			char tmp[100], *fmt;
			const char *q;

			tm = localtime(&t);

			p += 5;

			for (q = p; *q && *q != '}'; q++);

			fmt = xmalloc(q - p + 1);
			strncpy(fmt, p, q - p);
			fmt[q - p] = 0;

			strftime(tmp, sizeof(tmp), fmt, tm);

			xfree(fmt);
			p = q - 1;
		} else if (!strncmp(p, "window}", 7)) {
			waddstr(status, itoa(window_current->id));
			p += 6;
		} else if (!strncmp(p, "uin}", 4)) {
			waddstr(status, itoa(config_uin));
			p += 3;
		} else if (!strncmp(p, "nick}", 5)) {
			struct userlist *u = userlist_find(config_uin, NULL);
			if (u && u->display)
				waddstr(status, u->display);
			p += 4;
		} else if (!strncmp(p, "query}", 6)) {
			if (window_current->target)
				waddstr(status, window_current->target);
			p += 5;
		} else if (!strncmp(p, "descr}", 6)) {
			if (config_reason)
				waddstr(status, config_reason);
			p += 5;
		} else if (!strncmp(p, "mail}", 5)) {
			if (config_check_mail && mail_count)
				waddstr(status, itoa(mail_count));
			p += 4;
		} else if (!strncmp(p, "activity}", 9)) {
			string_t s = string_init("");
			int first = 1;
			list_t l;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (w->act) {
					if (!first) 
						string_append_c(s, ',');
					string_append(s, itoa(w->id));
					first = 0;
				}
			}
			
			waddstr(status, s->str);

			string_free(s, 1);

			p += 8;
		} else if (*p == '?') {
			int matched = 0, neg = 0;

			p++;
			if (!*p)
				break;

			if (*p == '!') {
				neg = 1;
				p++;
				if (!*p)
					break;
			}
			
			if (!strncmp(p, "debug ", 6)) {
				matched = (config_debug);
				p += 5;
			} else if (!strncmp(p, "mail ", 5)) {
				matched = (config_check_mail && mail_count);
				p += 4;
			} else if (!strncmp(p, "descr ", 6)) {
				matched = (config_reason != NULL);
				p += 5;
			} else if (!strncmp(p, "away ", 5)) {
				matched = (away == 1 || away == 3);
				p += 4;
			} else if (!strncmp(p, "avail ", 6)) {
				matched = (away == 0 || away == 4);
				p += 5;
			} else if (!strncmp(p, "invisible ", 10)) {
				matched = (away == 2 || away == 5);
				p += 9;
			} else if (!strncmp(p, "notavail ", 9)) {
				matched = (!sess || sess->state != GG_STATE_CONNECTED);
				p += 8;
			} else if (!strncmp(p, "query ", 6)) {
				matched = (window_current->target != NULL);
				p += 5;
			} else if (!strncmp(p, "activity ", 9)) {
				list_t l;

				for (l = windows; l; l = l->next) {
					struct window *w = l->data;

					if (w->act)
						matched = 1;
				}

				p += 8;
			} else if (!strncmp(p, "more ", 5)) {
				matched = (window_current->more);
				p += 4;
			} else if (!strncmp(p, "nick ", 5)) {
				struct userlist *u = userlist_find(config_uin, NULL);
				
				matched = (u && u->display);
				p += 4;
			}

			if (neg)
				matched = !matched;

			if (!matched) {
				while (*p && *p != '}')
					p++;
				if (!*p)
					break;
			} else 
				nested++;
		} else {
			while (*p && *p != '}')
				p++;
			if (!*p)
				break;
		}
	}

	wnoutrefresh(status);
	if (config_contacts)
		wnoutrefresh(contacts);
	wnoutrefresh(input);
	doupdate();
}

/*
 * ui_ncurses_beep()
 *
 * ostentacyjnie wzywa u¿ytkownika do reakcji.
 */
static void ui_ncurses_beep()
{
	beep();
}

/*
 * ui_ncurses_init()
 *
 * inicjalizuje ca³± zabawê z ncurses.
 */
void ui_ncurses_init()
{
	struct timer *t;

	ui_postinit = ui_ncurses_postinit;
	ui_print = ui_ncurses_print;
	ui_loop = ui_ncurses_loop;
	ui_beep = ui_ncurses_beep;
	ui_event = ui_ncurses_event;
	ui_deinit = ui_ncurses_deinit;
		
	initscr();
	cbreak();
	noecho();
	nonl();
	use_default_colors();

	ui_screen_width = stdscr->_maxx + 1;
	ui_screen_height = stdscr->_maxy + 1;
	
	window_current = window_new(NULL, 0);

	status = newwin(1, stdscr->_maxx + 1, stdscr->_maxy - 1, 0);
	input = newwin(1, stdscr->_maxx + 1, stdscr->_maxy, 0);
	keypad(input, TRUE);

	start_color();
	init_pair(16, COLOR_BLACK, COLOR_DEFAULT);
	init_pair(1, COLOR_RED, COLOR_DEFAULT);
	init_pair(2, COLOR_GREEN, COLOR_DEFAULT);
	init_pair(3, COLOR_YELLOW, COLOR_DEFAULT);
	init_pair(4, COLOR_BLUE, COLOR_DEFAULT);
	init_pair(5, COLOR_MAGENTA, COLOR_DEFAULT);
	init_pair(6, COLOR_CYAN, COLOR_DEFAULT);
	init_pair(7, COLOR_WHITE, COLOR_DEFAULT);

	init_pair(8, COLOR_BLACK, COLOR_BLUE);
	init_pair(9, COLOR_RED, COLOR_BLUE);
	init_pair(10, COLOR_GREEN, COLOR_BLUE);
	init_pair(11, COLOR_YELLOW, COLOR_BLUE);
	init_pair(12, COLOR_BLUE, COLOR_BLUE);
	init_pair(13, COLOR_MAGENTA, COLOR_BLUE);
	init_pair(14, COLOR_CYAN, COLOR_BLUE);
	init_pair(15, COLOR_WHITE, COLOR_BLUE);

	wnoutrefresh(status);
	if (contacts)
		wnoutrefresh(contacts);
	wnoutrefresh(input);
	doupdate();

	signal(SIGINT, SIG_IGN);
	
	memset(history, 0, sizeof(history));

	t = timer_add(1, "ui-ncurses-time", "refresh_time");
	t->ui = 1;

	memset(binding_map, 0, sizeof(binding_map));
	memset(binding_map_meta, 0, sizeof(binding_map_meta));

	contacts_rebuild();
}

/*
 * ui_ncurses_postinit()
 *
 * uruchamiana po wczytaniu konfiguracji.
 */
static void ui_ncurses_postinit()
{
	if (config_windows_save && config_windows_layout) {
		char **targets = array_make(config_windows_layout, "|", 0, 0, 0);
		int i;

		if (targets[0] && strcmp(targets[0], "")) {
			xfree(window_current->target);
			xfree(window_current->prompt);
			window_current->target = xstrdup(targets[0]);
			window_current->prompt = format_string(format_find("ncurses_prompt_query"), targets[0]);
			window_current->prompt_len = strlen(window_current->prompt);
		}

		for (i = 1; targets[i]; i++) {
			if (!strcmp(targets[i], "-"))
				continue;
			window_new((strcmp(targets[i], "")) ? targets[i] : NULL, i + 1);
		}

		array_free(targets);
	}
}

/*
 * ui_ncurses_deinit()
 *
 * zamyka, robi porz±dki.
 */
static void ui_ncurses_deinit()
{
	static int done = 0;
	list_t l;
	int i;

	if (done)
		return;

	if (config_windows_save) {
		string_t s = string_init(NULL);
		int maxid = 0, i;
		
		xfree(config_windows_layout);

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (w->id > maxid)
				maxid = w->id;
		}

		for (i = 1; i <= maxid; i++) {
			const char *target = "-";
			
			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (w->id == i) {
					target = w->target;
					break;
				}
			}

			if (target)
				string_append(s, target);

			if (i < maxid)
				string_append_c(s, '|');
		}

		config_windows_layout = string_free(s, 0);
	}

	for (l = windows; l; ) {
		struct window *w = l->data;

		l = l->next;

		window_kill(w, 1);
	}

	list_destroy(windows, 1);

	keypad(input, FALSE);

	werase(input);
	wnoutrefresh(input);
	doupdate();
	delwin(input);
	delwin(status);
	delwin(contacts);
	endwin();

	for (i = 0; i < HISTORY_MAX; i++)
		if (history[i] != line) {
			xfree(history[i]);
			history[i] = NULL;
		}

	if (lines) {
		for (i = 0; lines[i]; i++) {
			if (lines[i] != line)
				xfree(lines[i]);
			lines[i] = NULL;
		}

		xfree(lines);
		lines = NULL;
	}

	xfree(line);
	xfree(yanked);

	done = 1;
}

/*
 * line_adjust()
 *
 * ustawia kursor w odpowiednim miejscu ekranu po zmianie tekstu w poziomie.
 */
static void line_adjust()
{
	int prompt_len = (lines) ? 0 : window_current->prompt_len;

	line_index = strlen(line);
	if (strlen(line) < input->_maxx - 9 - prompt_len)
		line_start = 0;
	else
		line_start = strlen(line) - strlen(line) % (input->_maxx - 9 - prompt_len);
}

/*
 * lines_adjust()
 *
 * poprawia kursor po przesuwaniu go w pionie.
 */
static void lines_adjust()
{
	if (lines_index < lines_start)
		lines_start = lines_index;

	if (lines_index - 4 > lines_start)
		lines_start = lines_index - 4;

	line = lines[lines_index];

	if (line_index > strlen(line))
		line_index = strlen(line);
}

void dcc_generator(const char *text, int len)
{
	char *words[] = { "close", "get", "send", "show", "voice", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

void command_generator(const char *text, int len)
{
	char *slash = "";
	list_t l;

	if (*text == '/') {
		slash = "/";
		text++;
		len--;
	}

	if (window_current->target)
		slash = "/";
			
	for (l = commands; l; l = l->next) {
		struct command *c = l->data;

		if (!strncasecmp(text, c->name, len))
			array_add(&completions, saprintf("%s%s", slash, c->name));
	}
}

void known_uin_generator(const char *text, int len)
{
	list_t l;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (u->display && !strncasecmp(text, u->display, len))
			array_add(&completions, xstrdup(u->display));
	}

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;

		if (!strncasecmp(text, c->name, len))
			array_add(&completions, xstrdup(c->name));
	}
}

void unknown_uin_generator(const char *text, int len)
{
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		if (isdigit(send_nicks[i][0]) && !strncasecmp(text, send_nicks[i], len))
			array_add(&completions, xstrdup(send_nicks[i]));
	}
}

void variable_generator(const char *text, int len)
{
	list_t l;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		if (v->type == VAR_FOREIGN)
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

void ignored_uin_generator(const char *text, int len)
{
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

void empty_generator(const char *text, int len)
{

}

void file_generator(const char *text, int len)
{
	struct dirent **namelist = NULL;
	const char *dname, *bname;
	char *dirc;
	int count, i;

	dirc = xstrdup(text);

	bname = strrchr(text, '/');

	if (bname)
		bname++;
	else
		bname = text;
				
	dname = dirname(dirc);

	if (text[len - 1] == '/') {
		dname = text;
		bname = text + len - 1;
	}

	count = scandir(dname, &namelist, NULL, alphasort);

	for (i = 0; i < count; i++) {
		char *file = namelist[i]->d_name;

		if (!strcmp(file, ".") || !strcmp(file, "..")) {
			xfree(namelist[i]);
			continue;
		}
		
		if (!strncmp(bname, file, strlen(bname)) || *bname == '/') {
			if (strcmp(dname, "."))
				file = saprintf(dname[strlen(dname) - 1] == '/' ? "%s%s" : "%s/%s", dname, file);
			else
				file = saprintf("%s", file);

			array_add(&completions, file);
		}

		xfree(namelist[i]);
        }

	xfree(dirc);
	xfree(namelist);
}

void window_generator(const char *text, int len)
{
	char *words[] = { "new", "kill", "move", "next", "prev", "resize", "switch", "clear", "refresh", "list", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
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
	{ 'v', variable_generator },
	{ 'd', dcc_generator },
	{ 'f', file_generator },
	{ 'w', window_generator },
	{ 0, NULL }
};

/*
 * complete()
 *
 * funkcja obs³uguj±ca dope³nianie klawiszem tab.
 */
static void complete(int *line_start, int *line_index)
{
	int i, blanks = 0, count;
	char *p, *start = line, *cmd;

	/* nie obs³ugujemy dope³niania w ¶rodku tekstu */
	if (*line_index != strlen(line))
		return;

	/* je¶li uzbierano ju¿ co¶ */
	if (completions) {
		int maxlen = 0, cols, rows;
		char *tmp;

		for (i = 0; completions[i]; i++)
			if (strlen(completions[i]) + 2 > maxlen)
				maxlen = strlen(completions[i]) + 2;

		cols = (stdscr->_maxx - 5) / maxlen;
		if (cols == 0)
			cols = 1;

		rows = array_count(completions) / cols + 1;

		tmp = xmalloc(cols * maxlen + 2);

		for (i = 0; i < rows; i++) {
			int j;

			strcpy(tmp, "");

			for (j = 0; j < cols; j++) {
				int cell = j * rows + i;

				if (cell < array_count(completions)) {
					int k;

					strcat(tmp, completions[cell]); 

					for (k = 0; k < maxlen - strlen(completions[cell]); k++)
						strcat(tmp, " ");
				}
			}

			if (strcmp(tmp, "")) {
				strcat(tmp, "\n");
				ui_ncurses_print(NULL, 0, tmp);
			}
		}

		xfree(tmp);

		return;
	}
	
	/* policz spacje */
	for (p = line; *p; p++) {
		if (*p == ' ')
			blanks++;
	}

	/* nietypowe dope³nienie nicków przy rozmowach */
	cmd = saprintf("%s%s ", (line[0] == '/') ? "/" : "", (config_tab_command) ? config_tab_command : "chat");

	if (!strcmp(line, "") || (!strncasecmp(line, cmd, strlen(cmd)) && blanks == 2 && send_nicks_count > 0) || !strcasecmp(line, cmd)) {
		if (send_nicks_count)
			snprintf(line, LINE_MAXLEN, (window_current->target && line[0] != '/') ? "/%s%s " : "%s%s ", cmd, send_nicks[send_nicks_index++]);
		else
			snprintf(line, LINE_MAXLEN, (window_current->target && line[0] != '/') ? "/%s" : "%s", cmd);
		*line_start = 0;
		*line_index = strlen(line);
		if (send_nicks_index >= send_nicks_count)
			send_nicks_index = 0;

		xfree(cmd);
		array_free(completions);
		completions = NULL;

		return;
	}

	xfree(cmd);

	/* pocz±tek komendy? */
	if (!blanks)
		command_generator(line, strlen(line));
	else {
		char *params = NULL;
		int abbrs = 0, word = 0;
		list_t l;

		start = line + strlen(line);

		while (start > line && *(start - 1) != ' ')
			start--;

		for (p = line + 1; *p; p++)
			if (isspace(*p) && !isspace(*(p - 1)))
				word++;
		word--;

		for (l = commands; l; l = l->next) {
			struct command *c = l->data;
			int len = strlen(c->name);
			char *cmd = (line[0] == '/') ? line + 1 : line;

			if (!strncasecmp(cmd, c->name, len) && isspace(cmd[len])) {
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

		if (params && abbrs == 1 && word < strlen(params)) {
			for (i = 0; generators[i].ch; i++)
				if (generators[i].ch == params[word]) {
					generators[i].generate(start, strlen(start));
					break;
				}
		}
	}

	count = array_count(completions);

	if (count == 1) {
		snprintf(start, LINE_MAXLEN - (start - line), "%s ", completions[0]);
		*line_index = strlen(line);
		array_free(completions);
		completions = NULL;
	}

	if (count > 1) {
		int common = 0, minlen = strlen(completions[0]);

		for (i = 1; i < count; i++) {
			if (strlen(completions[i]) < minlen)
				minlen = strlen(completions[i]);
		}

		for (i = 0; i < minlen; i++, common++) {
			char c = completions[0][i];
			int j, out = 0;

			for (j = 1; j < count; j++) {
				if (completions[j][i] != c) {
					out = 1;
					break;
				}
			}
			
			if (out)
				break;
		}

		if (common > strlen(start) && start - line + common < LINE_MAXLEN) {
			snprintf(start, common + 1, "%s", completions[0]);
			*line_index = strlen(line);
		}
	}

	return;
}

/*
 * update_input()
 *
 * uaktualnia zmianê rozmiaru pola wpisywania tekstu -- przesuwa okienka
 * itd. je¶li zmieniono na pojedyncze, czy¶ci dane wej¶ciowe.
 */
static void update_input()
{
	list_t l;

	if (input_size == 1) {
		int i;
		
		for (i = 0; lines[i]; i++)
			xfree(lines[i]);
		xfree(lines);
		lines = NULL;

		line = xmalloc(LINE_MAXLEN);
		strcpy(line, "");

		history[0] = line;

		line_start = 0;
		line_index = 0; 
		lines_start = 0;
		lines_index = 0;
	} else {
		lines = xmalloc(2 * sizeof(char*));
		lines[0] = xmalloc(LINE_MAXLEN);
		lines[1] = NULL;
		strcpy(lines[0], line);
		xfree(line);
		line = lines[0];
		history[0] = NULL;
		lines_start = 0;
		lines_index = 0;
	}
	
	/* przesuñ/ods³oñ okienka */
	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (input_size == 5) {
			if (w->lines_count - w->start == w->height) {
				w->start += 4;
				if (w->start > w->lines_count)
					w->start = w->lines_count;
			}
			w->height -= 4;
		}

		if (input_size == 1) {
			if (w->lines_count - w->start == w->height) {
				w->start -= 4;
				if (w->start < 0)
					w->start = 0;
			}
			w->height += 4;
		}

		w->redraw = 1;
	}

	window_redraw(window_current);
	touchwin(window_current->window);
	window_refresh();
	if (contacts) {
		wresize(contacts, OUTPUT_SIZE, contacts->_maxx + 1);
		update_contacts();
	}
	wnoutrefresh(status);
	wnoutrefresh(input);
	doupdate();
}

/*
 * print_char()
 *
 * wy¶wietla w danym okienku znak, bior±c pod uwagê znaki ,,niewy¶wietlalne''.
 */
void print_char(WINDOW *w, int y, int x, unsigned char ch)
{
	wattrset(w, A_NORMAL);

	if (ch < 32) {
		wattrset(w, A_REVERSE);
		ch += 64;
	}

	if (ch >= 128 && ch < 160) {
		ch = '?';
		wattrset(w, A_REVERSE);
	}

	mvwaddch(w, y, x, ch);
	wattrset(w, A_NORMAL);
}

/*
 * ui_ncurses_loop()
 *
 * g³ówna pêtla interfejsu.
 */
static void ui_ncurses_loop()
{
	line = xmalloc(LINE_MAXLEN);
	strcpy(line, "");

	history[0] = line;

	for (;;) {
		int ch;

		ekg_wait_for_key();

		ch = wgetch(input);

		if (ch != 27 && binding_map[ch] && binding_map[ch]->action) {
			command_exec(NULL, binding_map[ch]->action);
			continue;
		}
		
		switch (ch) {
			case -1:	/* stracony terminal */
				ekg_exit();
				break;

			case KEY_RESIZE:  /* zmiana rozmiaru terminala */
				beep();
				window_refresh();
				if (contacts)
					wnoutrefresh(contacts);
				wnoutrefresh(status);
				wnoutrefresh(input);
				doupdate();
				wrefresh(curscr);
				break;

			case 27:
				ch = wgetch(input);

				if (binding_map_meta[ch] && binding_map_meta[ch]->action) {
					command_exec(NULL, binding_map_meta[ch]->action);
					break;
				}
				
				/* obs³uga Ctrl-F1 - Ctrl-F12 na FreeBSD */
				if (ch == '[') {
					ch = wgetch(input);
					if (ch >= 107 && ch <= 118)
						window_switch(ch - 106);
					break;
				}
				
				if (ch >= '1' && ch <= '9')	/* Alt-cyfra */
					window_switch(ch - '1' + 1);
				else if (ch == '0')
					window_switch(10);

				if (ch == 'k' || ch == 'K')	/* Alt-K */
					ui_event("command", "window", "kill", NULL);

				if (ch == 'n' || ch == 'N')	/* Alt-N */
					ui_event("command", "window", "new", NULL);

				if (ch == 'i' || ch == 'I') {	/* Alt-I */
					char *tmp = saprintf("/ignore %s", window_current->target);
					command_exec(NULL, tmp);
					xfree(tmp);
				}

				if (ch == 13) {		/* Ctrl-Enter */
					if (input_size == 1) {
						input_size = 5;
						update_input();
					} else {
						string_t s = string_init("");
						char *tmp;
						int i;
						
						for (i = 0; lines[i]; i++) {
							if (!strcmp(lines[i], "") && !lines[i + 1])
								break;

							string_append(s, lines[i]);
							string_append(s, "\r\n");
						}

						line = string_free(s, 0);
						tmp = xstrdup(line);
						history[0] = line;

						input_size = 1;
						update_input();

						command_exec(window_current->target, tmp);
						xfree(tmp);
					}
				}

				if (ch == 27 && input_size == 5) {  /* Esc */
					input_size = 1;

					update_input();
				}

				break;
			
			case 'N' - 64:	/* Ctrl-N */
				ui_event("command", "window", "next", NULL);
				break;

			case 'P' - 64:	/* Ctrl-P */
				ui_event("command", "window", "prev", NULL);
				break;
				
			case KEY_BACKSPACE:
			case 8:
			case 127:
				if (lines && line_index == 0 && lines_index > 0 && strlen(lines[lines_index]) + strlen(lines[lines_index - 1]) < LINE_MAXLEN) {
					int i;

					line_index = strlen(lines[lines_index - 1]);
					strcat(lines[lines_index - 1], lines[lines_index]);
					
					xfree(lines[lines_index]);

					for (i = lines_index; i < array_count(lines); i++)
						lines[i] = lines[i + 1];

					lines = xrealloc(lines, (array_count(lines) + 1) * sizeof(char*));

					lines_index--;
					lines_adjust();

					break;
				}

				if (strlen(line) > 0 && line_index > 0) {
					memmove(line + line_index - 1, line + line_index, LINE_MAXLEN - line_index);
					line[LINE_MAXLEN - 1] = 0;
					line_index--;
				}
				break;

			case 'Y' - 64:
				if (yanked && strlen(yanked) + strlen(line) + 1 < LINE_MAXLEN) {
					memmove(line + line_index + strlen(yanked), line + line_index, LINE_MAXLEN - line_index - strlen(yanked));
					memcpy(line + line_index, yanked, strlen(yanked));
					line_index += strlen(yanked);
				}
				break;

			case KEY_DC:	/* Ctrl-D, Delete */
			case 'D' - 64:
				if (line_index == strlen(line) && lines_index < array_count(lines) - 1 && strlen(line) + strlen(lines[lines_index + 1]) < LINE_MAXLEN) {
					int i;

					strcat(line, lines[lines_index + 1]);

					xfree(lines[lines_index + 1]);

					for (i = lines_index + 1; i < array_count(lines); i++)
						lines[i] = lines[i + 1];

					lines = xrealloc(lines, (array_count(lines) + 1) * sizeof(char*));

					lines_adjust();
					
					break;
				}
				
				if (line_index < strlen(line)) {
					memmove(line + line_index, line + line_index + 1, LINE_MAXLEN - line_index - 1);
					line[LINE_MAXLEN - 1] = 0;
				}
				break;	
				
			case KEY_ENTER:	/* Enter */
			case 13:
				if (lines) {
					int i;

					lines = xrealloc(lines, (array_count(lines) + 2) * sizeof(char*));

					for (i = array_count(lines); i > lines_index; i--)
						lines[i + 1] = lines[i];

					lines[lines_index + 1] = xmalloc(LINE_MAXLEN);
					strcpy(lines[lines_index + 1], line + line_index);
					line[line_index] = 0;
					
					line_index = 0;
					lines_index++;

					lines_adjust();
					
					break;
				}
				
				command_exec(window_current->target, line);

				if (strcmp(line, "")) {
					if (history[0] != line)
						xfree(history[0]);
					history[0] = xstrdup(line);
					xfree(history[HISTORY_MAX - 1]);
					memmove(&history[1], &history[0], sizeof(history) - sizeof(history[0]));
				} else {
					if (config_enter_scrolls)
						print("none", "");
				}

				history[0] = line;
				history_index = 0;
				line[0] = 0;
				line_adjust();
				break;	

			case 'U' - 64:	/* Ctrl-U */
				xfree(yanked);
				yanked = strdup(line);
				line[0] = 0;
				line_adjust();

				if (lines && lines_index < array_count(lines) - 1) {
					int i;

					xfree(lines[lines_index]);

					for (i = lines_index; i < array_count(lines); i++)
						lines[i] = lines[i + 1];

					lines = xrealloc(lines, (array_count(lines) + 1) * sizeof(char*));

					lines_adjust();
				}

				break;

			case 'W' - 64:	/* Ctrl-W */
			{
				char *p;
				int eaten = 0;

				p = line + line_index;
				while (isspace(*(p-1)) && p > line) {
					p--;
					eaten++;
				}
				if (p > line) {
					while (!isspace(*(p-1)) && p > line) {
						p--;
						eaten++;
					}
				}
				memmove(p, line + line_index, strlen(line) - line_index + 1);
				line_index -= eaten;

				break;
			}

			case 'L' - 64:	/* Ctrl-L */
				ui_event("command", "window", "refresh", NULL);
				break;
				
			case 9:		/* Tab */
				if (!lines)
					complete(&line_start, &line_index);
				else {
					int i, count = 8 - (line_index % 8);

					if (strlen(line) + count >= LINE_MAXLEN - 1)
						break;

					memmove(line + line_index + count, line + line_index, LINE_MAXLEN - line_index - count);

					for (i = line_index; i < line_index + count; i++)
						line[i] = ' ';

					line_index += count;
				}

				break;
				
			case KEY_LEFT:	/* <-- */
				if (lines) {
					if (line_index > 0)
						line_index--;
					else {
						if (lines_index > 0)
							lines_index--;
						lines_adjust();
					}

					break;
				}

				if (line_index > 0)
					line_index--;
				break;
				
			case KEY_RIGHT:	/* --> */
				if (lines) {
					if (line_index < strlen(line))
						line_index++;
					else {
						if (lines_index < array_count(lines) - 1)
							lines_index++;
						lines_adjust();
					}

					break;
				}

				if (line_index < strlen(line))
					line_index++;
				break;
				
			case 'E' - 64:	/* Ctrl-E, End */
			case KEY_END:
			case KEY_SELECT:
				line_adjust();
				break;
				
			case 'A' - 64:	/* Ctrl-A, Home */
			case KEY_HOME:
			case KEY_FIND:
				line_index = 0;
				line_start = 0;
				break;
				
			case KEY_UP:	/* /\ */
				if (lines) {
					if (lines_index - lines_start == 0)
						if (lines_start)
							lines_start--;

					if (lines_index)
						lines_index--;

					lines_adjust();

					break;
				}
				
				if (history[history_index + 1]) {
					if (history_index == 0)
						history[0] = xstrdup(line);
					history_index++;
					strcpy(line, history[history_index]);
					line_adjust();
				}
				break;
				
			case KEY_DOWN:	/* \/ */
				if (lines) {
					if (lines_index - line_start == 4)
						if (lines_index < array_count(lines) - 1)
							lines_start++;

					if (lines_index < array_count(lines) - 1)
						lines_index++;

					lines_adjust();

					break;
				}

				if (history_index > 0) {
					history_index--;
					strcpy(line, history[history_index]);
					line_adjust();
					if (history_index == 0) {
						xfree(history[0]);
						history[0] = line;
					}
				}
				break;
				
			case KEY_PPAGE:	/* Page Up */
			case 'F' - 64:	/* Ctrl-F */
				window_current->start -= window_current->height;
				if (window_current->start < 0)
					window_current->start = 0;
				window_redraw(window_current);

				break;

			case KEY_NPAGE:	/* Page Down */
			case 'G' - 64:	/* Ctrl-G */
				window_current->start += window_current->height;

				if (window_current->start > window_current->lines_count - window_current->height + window_current->overflow)
					window_current->start = window_current->lines_count - window_current->height + window_current->overflow;

				if (window_current->start < 0)
					window_current->start = 0;

				if (window_current->start == window_current->lines_count - window_current->height + window_current->overflow) {
					window_current->more = 0;
					update_statusbar();
				}
				window_redraw(window_current);

				break;
				
			case KEY_F(1):	/* F1 */
				binding_help(0, 0);
				break;

			case KEY_F(2):	/* F2 */
				binding_quick_list(0, 0);
				break;
	
			case KEY_F(3):  /* F3 */
				binding_toggle_contacts(0, 0);
				break;
				
			case KEY_F(12):	/* F12 */
				binding_toggle_debug(0, 0);
				break;
				
			default:
				if (strlen(line) >= LINE_MAXLEN - 1)
					break;
				memmove(line + line_index + 1, line + line_index, LINE_MAXLEN - line_index - 1);

				line[line_index++] = ch;
		}

		/* je¶li siê co¶ zmieni³o, wygeneruj dope³nienia na nowo */
		if (ch != 9) {
			array_free(completions);
			completions = NULL;
		}

		if (line_index - line_start > input->_maxx - 9 - window_current->prompt_len)
			line_start += input->_maxx - 19 - window_current->prompt_len;
		if (line_index - line_start < 10) {
			line_start -= input->_maxx - 19 - window_current->prompt_len;
			if (line_start < 0)
				line_start = 0;
		}
		
		werase(input);
		wattrset(input, COLOR_PAIR(7));

		if (lines) {
			int i;
			
			for (i = 0; i < 5; i++) {
				unsigned char *p;
				int j;

				if (!lines[lines_start + i])
					break;

				p = lines[lines_start + i];

				for (j = 0; j + line_start < strlen(p) && j < input->_maxx + 1; j++)
					print_char(input, i, j, p[j + line_start]);
			}

			wmove(input, lines_index - lines_start, line_index - line_start);
		} else {
			int i;

			if (window_current->prompt)
				mvwaddstr(input, 0, 0, window_current->prompt);

			for (i = 0; i < input->_maxx + 1 - window_current->prompt_len && i < strlen(line) - line_start; i++)
				print_char(input, 0, i + window_current->prompt_len, line[line_start + i]);

			wattrset(input, COLOR_PAIR(16) | A_BOLD);
			if (line_start > 0)
				mvwaddch(input, 0, window_current->prompt_len, '<');
			if (strlen(line) - line_start > input->_maxx + 1 - window_current->prompt_len)
				mvwaddch(input, 0, input->_maxx, '>');
			wattrset(input, COLOR_PAIR(7));
			wmove(input, 0, line_index - line_start + window_current->prompt_len);
		}
		window_refresh();
		if (contacts)
			wnoutrefresh(contacts);
		wnoutrefresh(status);
		wnoutrefresh(input);
		doupdate();
	}
}

static void window_next()
{
	struct window *next = NULL;
	list_t l;

	for (l = windows; l; l = l->next) {
		if (l->data == window_current && l->next)
			next = l->next->data;
	}

	if (!next)
		next = windows->data;

	window_switch(next->id);
}

static void window_prev()
{
	struct window *prev = NULL;
	list_t l;

	for (l = windows; l; l = l->next) {
		if (l->data == window_current && l != windows)
			break;
		prev = l->data;
	}

	window_switch(prev->id);
}

/*
 * window_kill()
 *
 * usuwa podane okno.
 */
void window_kill(struct window *w, int quiet)
{
	int id = w->id;

	if (quiet) 
		goto cleanup;

	if (id == 1 && w->target) {
		print("query_finished", window_current->target);
		xfree(window_current->target);
		xfree(window_current->prompt);
		window_current->target = NULL;
		window_current->prompt = NULL;
		window_current->prompt_len = 0;
	}
	
	if (id == 1) {
		print("window_kill_status");
		return;
	}
	
	if (w == window_current)
		window_prev();

	if (config_sort_windows) {
		list_t l;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (w->id > id)
				w->id--;
		}
	}

cleanup:
	if (w->backlog) {
		int i;

		for (i = 0; i < w->backlog_size; i++) {
			xfree(w->backlog[i]->str);
			xfree(w->backlog[i]->attr);
			xfree(w->backlog[i]);
		}

		xfree(w->backlog);
	}

	if (w->lines) {
		int i;

		for (i = 0; i < w->lines_count; i++)
			xfree(w->lines[i].ts);
		
		xfree(w->lines);
	}
		
	xfree(w->target);
	xfree(w->prompt);
	delwin(w->window);
	list_remove(&windows, w, 1);
}

/*
 * binding_add()
 *
 * przypisuje danemu klawiszowi akcjê.
 */
static void binding_add(const char *key, const char *action, int quiet)
{
	int correct = 0;
	struct binding b;
	
	if (!key || !action)
		return;
	
	if (!strncasecmp(key, "ctrl-", 5) && strlen(key) == 6 && isalpha(key[5])) {
		char ch = toupper(key[5]);
		list_t l;

		b.key = saprintf("Ctrl-%c", ch);
		b.action = xstrdup(action);

		for (l = bindings; l; l = l->next) { 
			struct binding *foo = l->data;

			if (!strcmp(b.key, foo->key)) {
				print("bind_seq_exist", b.key); 
				xfree(b.key);
				xfree(b.action);
				return;
			}
		}

		binding_map[ch - 64] = list_add(&bindings, &b, sizeof(b));

		correct = 1;
	}

	if (!strncasecmp(key, "alt-", 4) && strlen(key) == 5) {
		char ch = isalpha(key[4]) ? toupper(key[4]) : key[4];
		list_t l;

		b.key = saprintf("Alt-%c", ch);
		b.action = xstrdup(action);

		for (l = bindings; l; l = l->next) {
			struct binding *foo = l->data;
	
			if (!strcmp(b.key, foo->key)) {
				print("bind_seq_exist", b.key);
				xfree(b.key);
				xfree(b.action);
				return;
			}
		}

		binding_map_meta[(unsigned char) ch] = list_add(&bindings, &b, sizeof(b));
		if (isalpha(ch))
			binding_map_meta[tolower(ch)] = binding_map_meta[(unsigned char) ch];

		correct = 1;
	}

	if (!quiet) {
		print((correct) ? "bind_seq_add" : "bind_seq_incorrect", b.key);
		if (correct)
			config_changed = 1;
	}
}

/*
 * binding_delete()
 *
 * usuwa akcjê z danego klawisza.
 */
static void binding_delete(const char *key, int quiet)
{
	list_t l;

	if (!key)
		return;

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;
		int i;

		if (!b->key || strcasecmp(key, b->key))
			continue;

		xfree(b->key);
		xfree(b->action);
		
		for (i = 0; i < KEY_MAX + 1; i++) {
			if (binding_map[i] == b)
				binding_map[i] = NULL;
			if (binding_map_meta[i] == b)
				binding_map_meta[i] = NULL;
		}

		list_remove(&bindings, b, 1);

		config_changed = 1;

		if (!quiet)
			print("bind_seq_remove", key);
		
		return;
	}

	if (!quiet)
		print("bind_seq_incorrect", key);
}

/*
 * ui_ncurses_event()
 *
 * obs³uga zdarzeñ.
 */
static int ui_ncurses_event(const char *event, ...)
{
	va_list ap;

	va_start(ap, event);

	if (!event)
		return 0;

#if 0
	if (!strcmp(event, "xterm_update") && getenv("TERM") && !strncmp(getenv("TERM"), "xterm", 5)) {
		char *tmp = saprintf("\033]0;EKG (%d)\007", config_uin);
		tputs(tmp, 1, putchar);
		xfree(tmp);
	}
#endif

	if (!strcmp(event, "refresh_time")) {
		struct timer *t = timer_add(1, "ui-ncurses-time", "refresh_time");
		t->ui = 1;
	}

        if (!strcasecmp(event, "check_mail")) {
		struct timer *t = timer_add(config_check_mail_frequency, "check-mail-time", "check_mail");

		t->ui = 1;
		check_mail();
	}

	if (!strcmp(event, "variable_changed")) {
		char *name = va_arg(ap, char*);

		if (!strcasecmp(name, "sort_windows") && config_sort_windows) {
			list_t l;
			int id = 1;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;
				
				w->id = id++;
			}
		}

		if (!strcasecmp(name, "timestamp")) {
			list_t l;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;
				
				window_backlog_split(w, 1);
			}
		}

		if (!strcasecmp(name, "backlog_size")) {
			list_t l;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;
				int i;
				
				if (w->backlog_size <= config_backlog_size)
					continue;
				
				for (i = config_backlog_size; i < w->backlog_size; i++) {
					xfree(w->backlog[i]->str);
					xfree(w->backlog[i]->attr);
					xfree(w->backlog[i]);
				}

				w->backlog_size = config_backlog_size;

				w->backlog = xrealloc(w->backlog, w->backlog_size * sizeof(fstring_t));

				window_backlog_split(w, 1);
			}
		}
	}

	if (!strcmp(event, "conference_rename")) {
		char *oldname = va_arg(ap, char*), *newname = va_arg(ap, char*);
		list_t l;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (w->target && !strcasecmp(w->target, oldname)) {
				xfree(w->target);
				xfree(w->prompt);
				w->target = xstrdup(newname);
				w->prompt = format_string(format_find("ncurses_prompt_query"), newname);
				w->prompt_len = strlen(w->prompt);
			}
		}
	}

	if (!strcmp(event, "userlist_changed")) {
		const char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*);
		list_t l;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (!w->target || strcasecmp(w->target, p1))
				continue;

			xfree(w->target);
			w->target = xstrdup(p2);

			xfree(w->prompt);
			w->prompt = format_string(format_find("ncurses_prompt_query"), w->target);
			w->prompt_len = strlen(w->prompt);
		}

		goto cleanup;
	}

	if (!strcmp(event, "command")) {
		char *command = va_arg(ap, char*);

		if (!strcasecmp(command, "bind")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*), *p3 = va_arg(ap, char*);

			if (match_arg(p1, 'a', "add", 2) || match_arg(p1, 'a', "add-quiet", 5)) {
				if (!p2 || !p3)
					print("not_enough_params", "bind");
				else
					binding_add(p2, p3, (!strcasecmp(p1, "--add-quiet")) ? 1 : 0);
			} else if (match_arg(p1, 'd', "delete", 2)) {
				if (!p2)
					print("not_enough_params", "bind");
				else
					binding_delete(p2, 0);
			} else
				binding_list();

			goto cleanup;
		}

		if (!strcasecmp(command, "find")) {
			char *tmp = NULL;
			
			if (window_current->target) {
				struct userlist *u = userlist_find(0, window_current->target);
				int uin;

				if (u && u->uin)
					tmp = saprintf("find %d", u->uin);

				if (!u && (uin = atoi(window_current->target)))
					tmp = saprintf("find %d", uin);
			}

			if (!tmp)
				tmp = saprintf("find %d", config_uin);

			command_exec(NULL, tmp);

			xfree(tmp);

			goto cleanup;
		}

		if (!strcasecmp(command, "query")) {
			char *param = va_arg(ap, char*);

			if (!param && !window_current->target)
				goto cleanup;

			if (param) {
				struct window *w;

				if ((w = window_find(param))) {
					window_switch(w->id);
					goto cleanup;
				}

				if (config_make_window == 2) {
					w = window_new(param, 0);
					window_switch(w->id);
				}

				print_window(param, 0, "query_started", param);
				print_window(param, 0, "query_started_window", param);
				xfree(window_current->target);
				xfree(window_current->prompt);
				window_current->target = xstrdup(param);
				window_current->prompt = format_string(format_find("ncurses_prompt_query"), param);
				window_current->prompt_len = strlen(window_current->prompt);
			} else {
				const char *f = format_find("ncurses_prompt_none");

				print("query_finished", window_current->target);
				xfree(window_current->target);
				xfree(window_current->prompt);
				window_current->target = NULL;
				window_current->prompt = NULL;
				window_current->prompt_len = 0;
				
				if (strcmp(f, "")) {
					window_current->prompt = xstrdup(f);
					window_current->prompt_len = strlen(f);
				}
			}
			update_statusbar();
		}

		if (!strcasecmp(command, "window")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*);

			if (!p1 || !strcasecmp(p1, "list")) {
				list_t l;

				for (l = windows; l; l = l->next) {
					struct window *w = l->data;

					if (w->target) {
						if (!w->floating)	
							print("window_list_query", itoa(w->id), w->target);
						else
							print("window_list_floating", itoa(w->id), itoa(w->left), itoa(w->top), itoa(w->width), itoa(w->height), w->target);
					} else
						print("window_list_nothing", itoa(w->id));
				}

				goto cleanup;
			}

			if (!strcasecmp(p1, "new")) {
				struct window *w = window_new(p2, 0);
				if (!w->floating)
					window_switch(w->id);
				goto cleanup;
			}

			if (!strcasecmp(p1, "switch")) {
				if (!p2) {
					print("not_enough_params", "window");
					goto cleanup;
				}
				window_switch(atoi(p2));
				goto cleanup;
			}			
			
			if (!strcasecmp(p1, "kill")) {
				struct window *w = window_current;

				if (p2) {
					list_t l;

					for (w = NULL, l = windows; l; l = l->next) {
						struct window *ww = l->data;

						if (ww->id == atoi(p2)) {
							w = ww;
							break;
						}
					}

					if (!w) {
						print("window_noexist");
						goto cleanup;
					}
				}

				window_kill(w, 0);
				window_switch(window_current->id);
				goto cleanup;
			}

			if (!strcasecmp(p1, "next")) {
				window_next();
				goto cleanup;
			}
			
			if (!strcasecmp(p1, "prev")) {
				window_prev();
				goto cleanup;
			}
			
			if (!strcasecmp(p1, "move")) {
				struct window *w = NULL;
				char **argv;
				list_t l;

				if (!p2) {
					print("not_enough_params", "window");
					goto cleanup;
				}

				argv = array_make(p2, " ,", 3, 0, 0);

				if (array_count(argv) < 3) {
					print("not_enough_params", "window");
					array_free(argv);
					goto cleanup;
				}

				for (l = windows; l; l = l->next) {
					struct window *v = l->data;

					if (v->id == atoi(argv[0])) {
						w = v;
						break;
					}
				}

				if (!w) {
					print("window_noexist");
					array_free(argv);
					goto cleanup;
				}

				switch (argv[1][0]) {
					case '-':
						w->left += atoi(argv[1]);
						break;
					case '+':
						w->left += atoi(argv[1] + 1);
						break;
					default:
						w->left = atoi(argv[1]);
				}

				switch (argv[2][0]) {
					case '-':
						w->top += atoi(argv[2]);
						break;
					case '+':
						w->top += atoi(argv[2] + 1);
						break;
					default:
						w->top = atoi(argv[2]);
				}

				array_free(argv);
					
				if (w->left + w->width > stdscr->_maxx + 1)
					w->left = stdscr->_maxx + 1 - w->width;
				
				if (w->top + w->height > stdscr->_maxy + 1)
					w->top = OUTPUT_SIZE - w->height;
				
				if (w->floating)
					mvwin(w->window, w->top, w->left);

				touchwin(window_current->window);
				window_refresh();
				doupdate();

				goto cleanup;
			}

			if (!strcasecmp(p1, "resize")) {
				struct window *w = NULL;
				char **argv;
				list_t l;

				if (!p2) {
					print("not_enough_params", "window");
					goto cleanup;
				}

				argv = array_make(p2, " ,", 3, 0, 0);

				if (array_count(argv) < 3) {
					print("not_enough_params", "window");
					array_free(argv);
					goto cleanup;
				}

				for (l = windows; l; l = l->next) {
					struct window *v = l->data;

					if (v->id == atoi(argv[0])) {
						w = v;
						break;
					}
				}

				if (!w) {
					print("window_noexist");
					array_free(argv);
					goto cleanup;
				}

				switch (argv[1][0]) {
					case '-':
						w->width += atoi(argv[1]);
						break;
					case '+':
						w->width += atoi(argv[1] + 1);
						break;
					default:
						w->width = atoi(argv[1]);
				}

				switch (argv[2][0]) {
					case '-':
						w->height += atoi(argv[2]);
						break;
					case '+':
						w->height += atoi(argv[2] + 1);
						break;
					default:
						w->height = atoi(argv[2]);
				}

				array_free(argv);
					
				if (w->floating) {
					wresize(w->window, w->height, w->width);
					window_backlog_split(w, 1);
					window_redraw(w);
					touchwin(window_current->window);
					window_refresh();
					doupdate();
				}

				goto cleanup;
			}
			
			if (!strcasecmp(p1, "refresh")) {
				window_floating_update(0);
				wrefresh(curscr);
				goto cleanup;
			}

			if (!strcasecmp(p1, "clear")) {
				window_clear(window_current, 0);
				
				window_refresh();
				if (contacts)
					wnoutrefresh(contacts);
				wnoutrefresh(input);
				doupdate();

				goto cleanup;
			}
			
			print("window_invalid");
		}
	}

cleanup:
	va_end(ap);

	update_contacts();
	update_statusbar();
	
	return 0;
}

