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
#ifdef WITH_PYTHON
#  include "python.h"
#endif

static void ui_ncurses_loop();
static void ui_ncurses_print(const char *target, int separate, const char *line);
static void ui_ncurses_beep();
static int ui_ncurses_event(const char *event, ...);
static void ui_ncurses_postinit();
static void ui_ncurses_deinit();

static void window_switch(int id);
static void update_statusbar(int commit);

static void binding_add(const char *key, const char *action, int internal, int quiet);
static void binding_delete(const char *key, int quiet);
static void binding_default();

struct screen_line {
	int len;		/* d³ugo¶æ linii */
	
	char *str;		/* tre¶æ */
	char *attr;		/* atrybuty */
	
	char *prompt_str;	/* tre¶æ promptu */
	char *prompt_attr;	/* atrybuty promptu */
	int prompt_len;		/* d³ugo¶æ promptu */
	
	char *ts;		/* timestamp */
	int ts_len;		/* d³ugo¶æ timestampu */

	int backlog;		/* z której linii backlogu pochodzi? */
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

struct format_data {
	char *name;			/* %{nazwa} */
	char *text;			/* tre¶æ */
};

WINDOW *status = NULL;		/* okno stanu */
WINDOW *header = NULL;		/* okno nag³ówka */
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
int config_display_transparent = 1;	/* czy chcemy przezroczyste t³o? */
struct binding *binding_map[KEY_MAX + 1];	/* mapa bindowanych klawiszy */
struct binding *binding_map_meta[KEY_MAX + 1];	/* j.w. z altem */

static void window_kill(struct window *w, int quiet);
static int window_backlog_split(struct window *w, int full, int removed);
static void window_redraw(struct window *w);
static void window_clear(struct window *w, int full);
static void window_refresh();

#define COLOR_DEFAULT (-1)

#define CONTACTS_SIZE ((config_contacts) ? (config_contacts_size + 2) : 0)

/* rozmiar okna wy¶wietlaj±cego tekst */
#define OUTPUT_SIZE (stdscr->_maxy + 1 - input_size - config_statusbar_size - config_header_size)

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
 * window_commit()
 *
 * zatwierdza wszystkie zmiany w buforach ncurses i wy¶wietla je na ekranie.
 */
void window_commit()
{
	window_refresh();

	if (contacts)
		wnoutrefresh(contacts);
	
	if (header)
		wnoutrefresh(header);

	wnoutrefresh(status);

	wnoutrefresh(input);

	doupdate();
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
	int i, removed = 0;
	
	if (!w)
		return 0;

	if (w->backlog_size == config_backlog_size) {
		fstring_t fs = w->backlog[w->backlog_size - 1];
		int i;

		for (i = 0; i < w->lines_count; i++) {
			if (w->lines[i].backlog == w->backlog_size - 1)
				removed++;
		}

		xfree(fs->str);
		xfree(fs->attr);
		xfree(fs);

		w->backlog_size--;
	} else 
		w->backlog = xrealloc(w->backlog, (w->backlog_size + 1) * sizeof(fstring_t));

	memmove(&w->backlog[1], &w->backlog[0], w->backlog_size * sizeof(fstring_t));
	w->backlog[0] = str;
	w->backlog_size++;

	for (i = 0; i < w->lines_count; i++)
		w->lines[i].backlog++;

	return window_backlog_split(w, 0, removed);
}

/*
 * window_backlog_split()
 *
 * dzieli linie tekstu w buforze na linie ekranowe.
 *
 *  - w - okno do podzielenia
 *  - full - czy robimy pe³ne uaktualnienie?
 *  - removed - ile linii ekranowych z góry usuniêto?
 *
 * zwraca rozmiar w liniach ekranowych ostatnio dodanej linii.
 */
int window_backlog_split(struct window *w, int full, int removed)
{
	int i, res = 0, bottom = 0;

	if (!w)
		return 0;

	/* przy pe³nym przebudowaniu ilo¶ci linii nie musz± siê koniecznie
	 * zgadzaæ, wiêc nie bêdziemy w stanie pó¼niej stwierdziæ czy jeste¶my
	 * na koñcu na podstawie ilo¶ci linii mieszcz±cych siê na ekranie. */
	if (full && w->start == w->lines_count - w->height)
		bottom = 1;
	
	/* mamy usun±æ co¶ z góry, bo wywalono liniê z backloga. */
	if (removed) {
		for (i = 0; i < removed && i < w->lines_count; i++)
			xfree(w->lines[i].ts);
		memmove(&w->lines[0], &w->lines[removed], sizeof(struct screen_line) * (w->lines_count - removed));
		w->lines_count -= removed;
	}

	/* je¶li robimy pe³ne przebudowanie backloga, czy¶cimy wszystko */
	if (full) {
		for (i = 0; i < w->lines_count; i++)
			xfree(w->lines[i].ts);
		w->lines_count = 0;
		xfree(w->lines);
		w->lines = NULL;
	}

	/* je¶li upgrade... je¶li pe³ne przebudowanie... */
	for (i = (!full) ? 0 : (w->backlog_size - 1); i >= 0; i--) {
		struct screen_line *l;
		char *str, *attr;
		int j;
		time_t ts;

		str = w->backlog[i]->str + w->backlog[i]->prompt_len;
		attr = w->backlog[i]->attr + w->backlog[i]->prompt_len;
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
			l->backlog = i;

			l->prompt_len = w->backlog[i]->prompt_len;
			l->prompt_str = w->backlog[i]->str;
			l->prompt_attr = w->backlog[i]->attr;

			if (!w->floating && config_timestamp) {
				struct tm *tm = localtime(&ts);
				char buf[100];
				strftime(buf, sizeof(buf), config_timestamp, tm);
				l->ts = xstrdup(buf);
				l->ts_len = strlen(l->ts);
			}

			width = w->width - l->ts_len - l->prompt_len;
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
 * window_resize()
 *
 * dostosowuje rozmiar okien do rozmiaru ekranu, przesuwaj±c odpowiednio
 * wy¶wietlan± zawarto¶æ.
 */
void window_resize()
{
	list_t l;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;
		int delta;

		delta = OUTPUT_SIZE - w->height;

		if (w->lines_count - w->start == w->height) {
			w->start -= delta;

			if (delta < 0) {
				if (w->start > w->lines_count)
					w->start = w->lines_count;
			} else {
				if (w->start < 0)
					w->start = 0;
			}
		}

		w->height = OUTPUT_SIZE;
		wresize(w->window, w->height, w->width);

		w->top = config_header_size;
		mvwin(w->window, w->top, w->left);

		w->redraw = 1;
	}
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
	wattrset(w->window, (config_display_color) ? COLOR_PAIR(4) : A_NORMAL);

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

		/* XXX po³±czyæ wy¶wietlanie prompta i linii w jeden kod */

		for (x = 0; x < l->prompt_len; x++) {
			int attr = A_NORMAL;
			unsigned char ch = l->prompt_str[x];

			if ((l->prompt_attr[x] & 64))
				attr |= A_BOLD;

			if (!(l->prompt_attr[x] & 128)) {
				int tmp = l->prompt_attr[x] & 7;

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

			mvwaddch(w->window, top + y, left + x + l->ts_len + l->prompt_len, ch);
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
	int current = ((target) ? !strcasecmp(target, "__current") : 0);
	int debug = ((target) ? !strcasecmp(target, "__debug") : 0);
	int status = ((target) ? !strcasecmp(target, "__status") : 0);

	/* nie traktujemy debug jako aktualne - piszemy do statusowego */
	if (!target || current) {
		if (window_current->id)
			return window_current;
		else
			status = 1;
	}

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (!w->id && debug)
			return w;

		if (w->id == 1 && status)
			return w;

		if (w->target && target && !strcasecmp(target, w->target))
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
	
	mvwin(status, config_header_size + OUTPUT_SIZE, 0);
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

		update_statusbar(0);

		window_commit();

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

	/* okno z debug'iem */
	if (id == -1)
		id = 0;
	
	memset(&w, 0, sizeof(w));

	w.id = id;
	w.top = config_header_size;

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
				goto crap;

			if (!separate)
				w = window_find("__status");

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (separate && !w->target && w->id != 1) {
					w->target = xstrdup(target);
					xfree(w->prompt);
					w->prompt = format_string(format_find("ncurses_prompt_query"), target);
					w->prompt_len = strlen(w->prompt);
					print("window_id_query_started", itoa(w->id), target);
					print_window(target, 1, "query_started", target);
					print_window(target, 1, "query_started_window", target);
					event_check(EVENT_QUERY, get_uin(target), target);
					break;
				}
			}

		case 2:
			if (!(w = window_find(target))) {
				if (!separate)
					w = window_find("__status");
				else {
					w = window_new(target, 0);
					print("window_id_query_started", itoa(w->id), target);
					print_window(target, 1, "query_started", target);
					print_window(target, 1, "query_started_window", target);
					event_check(EVENT_QUERY, get_uin(target), target);
				}
			}

crap:
			if (!config_display_crap && target && !strcmp(target, "__current"))
				w = window_find("__status");
			
			break;
			
		default:
			/* je¶li nie ma okna, rzuæ do statusowego. */
			if (!(w = window_find(target)))
				w = window_find("__status");
	}

	if (w != window_current && !w->floating) {
		w->act = 1;
		update_statusbar(0);
	}

	if (config_speech_app)
		speech = string_init(NULL);
	
	if (w->start == w->lines_count - w->height || (w->start == 0 && w->lines_count <= w->height))
		bottom = 1;
	
	prev_count = w->lines_count;
	
	/* XXX wyrzuciæ dzielenie na linie z ui do ekg */
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
		window_commit();
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
 *
 *  - commit - czy rzuciæ od razu na ekran?
 */
static void update_contacts(int commit)
{
	int y = 0;
	list_t l;
		
	if (!config_contacts || !contacts)
		return;
	
	werase(contacts);

	wattrset(contacts, (config_display_color) ? COLOR_PAIR(4) : A_NORMAL);

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

	if (commit)
		window_commit();
}

/*
 * contacts_rebuild()
 *
 * wywo³ywane przy zmianach rozmiaru.
 */
void contacts_rebuild()
{
	static int last_header_size = -1, last_statusbar_size = -1;
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
			window_backlog_split(w, 1, 0);
		}

		window_commit();

		return;
	}

	if (config_contacts_size == last_contacts_size && config_header_size == last_header_size && config_statusbar_size == last_statusbar_size)
		return;
		
	last_contacts_size = config_contacts_size;
	last_header_size = config_header_size;
	
	if (contacts)
		delwin(contacts);
	
	contacts = newwin(OUTPUT_SIZE, config_contacts_size + 2, config_header_size, stdscr->_maxx - config_contacts_size - 1);

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (w->floating)
			continue;

		w->width = stdscr->_maxx + 1 - CONTACTS_SIZE;
		wresize(w->window, w->height, w->width);
		window_backlog_split(w, 1, 0);
	}

	window_commit();
}

/*
 * update_header()
 *
 * uaktualnia nag³ówek okna i wy¶wietla go ponownie.
 *
 *  - commit - czy wy¶wietliæ od razu?
 */
static void update_header(int commit)
{
	int y;

	if (!header)
		return;

	if (config_display_color)
		wattrset(header, COLOR_PAIR(15));
	else
		wattrset(header, A_REVERSE);

	for (y = 0; y < config_header_size; y++) {
		int x;
		
		wmove(header, y, 0);

		for (x = 0; x <= status->_maxx; x++)
			waddch(header, ' ');
	}

	if (commit)
		window_commit();
}
		
/*
 * print_statusbar()
 *
 * wy¶wietla pasek stanu zgodnie z podany formatem i danymi.
 *
 *  - w - okno ncurses, do którego piszemy
 *  - x, y - wspó³rzêdne, od których zaczynamy
 *  - format - co mamy wy¶wietliæ
 *  - data - dane do podstawienia w formatach
 *
 * zwraca ilo¶æ dopisanych znaków.
 */
int print_statusbar(WINDOW *w, int x, int y, const char *format, void *data_)
{
	const char *p = format;
	int orig_x = x, bgcolor, fgcolor, bold;
	struct format_data *data = data_;

	wmove(w, y, x);
	bgcolor = 1;
	fgcolor = 7;
	bold = 0;
			
	while (*p && *p != '}' && x <= w->_maxx) {
		int i, nest;

		if (*p != '%') {
			waddch(w, (unsigned char) *p);
			p++;
			x++;
			continue;
		}

		p++;
		if (!*p)
			break;

#define __color(x,y,z) \
		case x: fgcolor = z; bold = 0; break; \
		case y: fgcolor = z; bold = 1; break;

		if (*p != '{') {
			switch (*p) {
				__color('k', 'K', 0);
				__color('r', 'R', 1);
				__color('g', 'G', 2);
				__color('y', 'Y', 3);
				__color('b', 'B', 4);
				__color('m', 'M', 5);
				__color('c', 'C', 6);
				__color('w', 'W', 7);
				case 'l':
					bgcolor = 1;
					break;
				case 'e':
					bgcolor = 0;
					break;
				case 'n':
					bgcolor = 1;
					fgcolor = 7;
					bold = 0;
					break;
			}
			p++;

			if (config_display_color)
				wattrset(w, COLOR_PAIR(bgcolor * 8 + fgcolor) | ((bold) ? A_BOLD : 0));
			else
				wattrset(w, (bgcolor == 1) ? A_REVERSE : A_NORMAL | (bold) ? A_BOLD : 0);
			
			continue;
		}
#undef __color
		if (*p != '{' && !config_display_color)
			continue;

		p++;
		if (!*p)
			break;

		for (i = 0; data && data[i].name; i++) {
			int len;

			if (!data[i].text)
				continue;

			len = strlen(data[i].name);

			if (!strncmp(p, data[i].name, len) && p[len] == '}') {
				waddstr(w, data[i].text);
				p += len;
				x += strlen(data[i].text);
				goto next;
			}
		}

		if (*p == '?') {
			int neg = 0;

			p++;
			if (!*p)
				break;

			if (*p == '!') {
				neg = 1;
				p++;
			}

			for (i = 0; data && data[i].name; i++) {
				int len, matched = (int) data[i].text;

				if (neg)
					matched = !matched;

				len = strlen(data[i].name);

				if (!strncmp(p, data[i].name, len) && p[len] == ' ') {
					p += len + 1;

					if (matched)
						x += print_statusbar(w, x, y, p, data);
					goto next;
				}
			}

			goto next;
		}

next:
		/* uciekamy z naszego poziomu zagnie¿d¿enia */

		nest = 1;

		while (*p && nest) {
			if (*p == '}')
				nest--;
			if (*p == '{')
				nest++;
			p++;
		}
	}

	return x - orig_x;
}

/*
 * update_statusbar()
 *
 * uaktualnia pasek stanu i wy¶wietla go ponownie.
 *
 *  - commit - czy wy¶wietliæ od razu?
 */
static void update_statusbar(int commit)
{
	struct userlist *u = userlist_find(config_uin, NULL);
	struct userlist *q = userlist_find(0, window_current->target);
	struct format_data *formats = NULL;
	int formats_count = 0, i, y;

	if (config_display_color) {
		wattrset(status, COLOR_PAIR(15));
		if (header)
			wattrset(header, COLOR_PAIR(15));
	} else {
		wattrset(status, A_REVERSE);
		if (header)
			wattrset(header, A_REVERSE);
	}

	/* inicjalizujemy wszystkie opisowe bzdurki */

	/* XXX mo¿naby zrobiæ sta³± tablicê, bez realokowania. by³oby
	 * nieco szybciej, bo do¶æ czêsto siê wywo³uje t± funkcjê. */

	memset(&formats, 0, sizeof(formats));

#define __add_format(x, y, z) \
	{ \
		formats = xrealloc(formats, (formats_count + 2) * sizeof(struct format_data)); \
		formats[formats_count].name = x; \
		formats[formats_count].text = (y) ? xstrdup(z) : NULL; \
		formats_count++; \
		formats[formats_count].name = NULL; \
		formats[formats_count].text = NULL; \
	} 

	{
		time_t t = time(NULL);
		struct tm *tm;
		char tmp[16];

		tm = localtime(&t);

		strftime(tmp, sizeof(tmp), "%H:%M", tm);
		
		__add_format("time", 1, tmp);
	}

	__add_format("window", window_current->id, itoa(window_current->id));
	__add_format("uin", config_uin, itoa(config_uin));
	__add_format("nick", (u && u->display), u->display);
	__add_format("query", window_current->id, window_current->target);
	__add_format("descr", 1, config_reason);
	__add_format("mail", (config_check_mail && mail_count), itoa(mail_count));
	{
		string_t s = string_init("");
		int first = 1, act = 0;
		list_t l;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (!w->act || !w->id) 
				continue;

			if (!first)
				string_append_c(s, ',');
			
			string_append(s, itoa(w->id));
			first = 0;
			act = 1;
		}
		
		__add_format("activity", (act), s->str);

		string_free(s, 1);
	}
	
	__add_format("debug", (!window_current->id), "");
	__add_format("away", GG_S_B(config_status), "");
	__add_format("busy", GG_S_B(config_status), "");
	__add_format("avail", GG_S_A(config_status), "");
	__add_format("invisible", GG_S_I(config_status), "");
	__add_format("notavail", (!sess || sess->state != GG_STATE_CONNECTED), "");
	__add_format("more", (window_current->more), "");

	__add_format("query_descr", (q), q->descr);
	__add_format("query_away", (q && GG_S_B(q->status)), "");
	__add_format("query_busy", (q && GG_S_B(q->status)), "");
	__add_format("query_avail", (q && GG_S_A(q->status)), "");
	__add_format("query_invisible", (q && GG_S_I(q->status)), "");
	__add_format("query_notavail", (q && GG_S_NA(q->status)), "");
	__add_format("query_ip", (q && q->ip.s_addr), inet_ntoa(q->ip));


	__add_format("url", 1, "http://dev.null.pl/ekg/");
	__add_format("version", 1, VERSION);

#undef __add_format

	for (y = 0; y < config_header_size; y++) {
		const char *p;
		int x;

		wmove(header, y, 0);

		for (x = 0; x <= header->_maxx; x++)
			waddch(header, ' ');
		
		wmove(header, y, 0);

		if (!y) {
			p = format_find("header1");

			if (!strcmp(p, ""))
				p = format_find("header");
		} else {
			char *tmp = saprintf("header%d", y + 1);
			p = format_find(tmp);
			xfree(tmp);
		}

		print_statusbar(header, 0, y, p, formats);
	}

	for (y = 0; y < config_statusbar_size; y++) {
		const char *p;
		int x;

		wmove(status, y, 0);

		for (x = 0; x <= status->_maxx; x++)
			waddch(status, ' ');
		
		wmove(status, y, 0);

		if (!y) {
			p = format_find("statusbar1");

			if (!strcmp(p, ""))
				p = format_find("statusbar");
		} else {
			char *tmp = saprintf("statusbar%d", y + 1);
			p = format_find(tmp);
			xfree(tmp);
		}

		print_statusbar(status, 0, y, p, formats);
	}

	for (i = 0; formats[i].name; i++)
		xfree(formats[i].text);

	xfree(formats);

#ifdef WITH_PYTHON
	PYTHON_HANDLE_HEADER(redraw_header, "")
	;
	PYTHON_HANDLE_FOOTER()
	
	PYTHON_HANDLE_HEADER(redraw_statusbar, "")
	;
	PYTHON_HANDLE_FOOTER()
#endif
	
	if (commit)
		window_commit();
}

/*
 * winch_handler()
 *
 * robi, co trzeba po zmianie rozmiaru terminala.
 */
static void winch_handler()
{
	endwin();
	refresh();

	window_resize();
	window_refresh();
	contacts_rebuild();
	window_commit();
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
	int background = COLOR_BLACK;

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

	if (config_display_transparent) {
		background = COLOR_DEFAULT;
		use_default_colors();
	}

	ui_screen_width = stdscr->_maxx + 1;
	ui_screen_height = stdscr->_maxy + 1;
	
	window_new(NULL, -1);
	window_current = window_new(NULL, 0);

	status = newwin(1, stdscr->_maxx + 1, stdscr->_maxy - 1, 0);
	input = newwin(1, stdscr->_maxx + 1, stdscr->_maxy, 0);
	keypad(input, TRUE);

	start_color();

	init_pair(16, COLOR_BLACK, background);
	init_pair(1, COLOR_RED, background);
	init_pair(2, COLOR_GREEN, background);
	init_pair(3, COLOR_YELLOW, background);
	init_pair(4, COLOR_BLUE, background);
	init_pair(5, COLOR_MAGENTA, background);
	init_pair(6, COLOR_CYAN, background);
	init_pair(7, COLOR_WHITE, background);

	init_pair(8, COLOR_BLACK, COLOR_BLUE);
	init_pair(9, COLOR_RED, COLOR_BLUE);
	init_pair(10, COLOR_GREEN, COLOR_BLUE);
	init_pair(11, COLOR_YELLOW, COLOR_BLUE);
	init_pair(12, COLOR_BLUE, COLOR_BLUE);
	init_pair(13, COLOR_MAGENTA, COLOR_BLUE);
	init_pair(14, COLOR_CYAN, COLOR_BLUE);
	init_pair(15, COLOR_WHITE, COLOR_BLUE);

	window_commit();

	signal(SIGINT, SIG_IGN);
#ifdef SIGWINCH
	signal(SIGWINCH, winch_handler);
#endif
	
	memset(history, 0, sizeof(history));

	timer_add(1, 0, TIMER_UI, 0, "ui-ncurses-time", "refresh_time");

	memset(binding_map, 0, sizeof(binding_map));
	memset(binding_map_meta, 0, sizeof(binding_map_meta));

	contacts_rebuild();

	binding_default();
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
					if (w->floating) {
						char *tmp = saprintf("*%d,%d,%d,%d,%d,", w->left, w->top, w->width, w->height, w->frames);
						string_append(s, tmp);
						xfree(tmp);
					}
				
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
	const char *words[] = { "close", "get", "send", "list", "voice", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

void command_generator(const char *text, int len)
{
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

void events_generator(const char *text, int len)
{
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
}

void unknown_uin_generator(const char *text, int len)
{
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		if (send_nicks[i] && isdigit(send_nicks[i][0]) && !strncasecmp(text, send_nicks[i], len))
			array_add(&completions, xstrdup(send_nicks[i]));
	}
}

/*
 * XXX nie obs³uguje eskejpowania znaków, dope³nia tylko pierwszy wyraz.
 */
void known_uin_generator(const char *text, int len)
{
	int escaped = 0;
	list_t l;

	if (text[0] == '"') {
		escaped = 1;
		text++;
		len--;

		if (len > 0 && text[len - 1] == '"')
			len--;
	}
       	       
	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (u->display && !strncasecmp(text, u->display, len))
			array_add(&completions, (escaped || strchr(u->display, ' ')) ? saprintf("\"%s\"", u->display) : xstrdup(u->display));
	}

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;

		if (!strncasecmp(text, c->name, len))
			array_add(&completions, xstrdup(c->name));
	}

	unknown_uin_generator(text, len);
}

void variable_generator(const char *text, int len)
{
	list_t l;

	for (l = variables; l; l = l->next) {
		struct variable *v = l->data;

		if (v->type == VAR_FOREIGN || v->display == 2)
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

void blocked_uin_generator(const char *text, int len)
{
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

void python_generator(const char *text, int len)
{
	const char *words[] = { "load", "unload", "run", "exec", "list", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

void window_generator(const char *text, int len)
{
	const char *words[] = { "new", "kill", "move", "next", "prev", "resize", "switch", "clear", "refresh", "list", NULL };
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
	{ 'b', blocked_uin_generator },
	{ 'v', variable_generator },
	{ 'd', dcc_generator },
	{ 'p', python_generator },
	{ 'w', window_generator },
	{ 'f', file_generator },
	{ 'e', events_generator },
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

		cols = (window_current->width - 6) / maxlen;
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
		if (send_nicks_index >= send_nicks_count)
			send_nicks_index = 0;

		if (send_nicks_count)
			snprintf(line, LINE_MAXLEN, (window_current->target && line[0] != '/') ? "/%s%s " : "%s%s ", cmd, send_nicks[send_nicks_index++]);
		else
			snprintf(line, LINE_MAXLEN, (window_current->target && line[0] != '/') ? "/%s" : "%s", cmd);
		*line_start = 0;
		*line_index = strlen(line);

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
	
	window_resize();

	if (contacts) {
		wresize(contacts, OUTPUT_SIZE, contacts->_maxx + 1);
		update_contacts(0);
	}

	window_redraw(window_current);
	touchwin(window_current->window);

	window_commit();
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
 * ekg_getch()
 *
 * czeka na wci¶niêcie klawisza i je¶li wkompilowano obs³ugê pythona,
 * przekazuje informacjê o zdarzeniu do skryptu.
 *
 *  - meta - przedrostek klawisza.
 *
 * zwraca kod klawisza lub -2, je¶li nale¿y go pomin±æ.
 */
int ekg_getch(int meta)
{
	int ch;

	ch = wgetch(input);

#ifdef WITH_PYTHON
	PYTHON_HANDLE_HEADER(keypress, "(ii)", meta, ch)
	{
		int dummy;

		PYTHON_HANDLE_RESULT("ii", &dummy, &ch)
		{
		}
	}

	PYTHON_HANDLE_FOOTER()

	if (python_handle_result == 0 || python_handle_result == 2)
		ch = -2;
#endif

	return ch;
}

static void binding_backward_word(const char *arg)
{
	while (line_index > 0 && line[line_index - 1] == ' ')
		line_index--;
	while (line_index > 0 && line[line_index - 1] != ' ')
		line_index--;
}

static void binding_forward_word(const char *arg)
{
	while (line_index < strlen(line) && line[line_index] == ' ')
		line_index++;
	while (line_index < strlen(line) && line[line_index] != ' ')
		line_index++;
}

static void binding_kill_word(const char *arg)
{
	char *p = line + line_index;
	int eaten = 0;

	while (*p && *p == ' ') {
		p++;
		eaten++;
	}

	while (*p && *p != ' ') {
		p++;
		eaten++;
	}

	memmove(line + line_index, line + line_index + eaten, strlen(line) - line_index - eaten + 1);
}

static void binding_toggle_input(const char *arg)
{
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

static void binding_cancel_input(const char *arg)
{
	if (input_size == 5) {
		input_size = 1;
		update_input();
	}
}

static void binding_backward_delete_char(const char *arg)
{
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
	}

	if (strlen(line) > 0 && line_index > 0) {
		memmove(line + line_index - 1, line + line_index, LINE_MAXLEN - line_index);
		line[LINE_MAXLEN - 1] = 0;
		line_index--;
	}
}

static void binding_kill_line(const char *arg)
{
	line[line_index] = 0;
}

static void binding_yank(const char *arg)
{
	if (yanked && strlen(yanked) + strlen(line) + 1 < LINE_MAXLEN) {
		memmove(line + line_index + strlen(yanked), line + line_index, LINE_MAXLEN - line_index - strlen(yanked));
		memcpy(line + line_index, yanked, strlen(yanked));
		line_index += strlen(yanked);
	}
}

static void binding_delete_char(const char *arg)
{
	if (line_index == strlen(line) && lines_index < array_count(lines) - 1 && strlen(line) + strlen(lines[lines_index + 1]) < LINE_MAXLEN) {
		int i;

		strcat(line, lines[lines_index + 1]);

		xfree(lines[lines_index + 1]);

		for (i = lines_index + 1; i < array_count(lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (array_count(lines) + 1) * sizeof(char*));

		lines_adjust();
	
		return;
	}
				
	if (line_index < strlen(line)) {
		memmove(line + line_index, line + line_index + 1, LINE_MAXLEN - line_index - 1);
		line[LINE_MAXLEN - 1] = 0;
	}
}
				
static void binding_accept_line(const char *arg)
{
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
	
		return;
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
}

static void binding_line_discard(const char *arg)
{
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
}

static void binding_quoted_insert(const char *arg)
{
	int ch;

	ekg_wait_for_key();

	ch = ekg_getch('V' - 64);	/* XXX */

	if (ch == -1)
		return;

	if (strlen(line) >= LINE_MAXLEN - 1)
		return;

	memmove(line + line_index + 1, line + line_index, LINE_MAXLEN - line_index - 1);

	line[line_index++] = ch;
}

static void binding_word_rubout(const char *arg)
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
}

static void binding_complete(const char *arg)
{
	if (!lines)
		complete(&line_start, &line_index);
	else {
		int i, count = 8 - (line_index % 8);

		if (strlen(line) + count >= LINE_MAXLEN - 1)
			return;

		memmove(line + line_index + count, line + line_index, LINE_MAXLEN - line_index - count);

		for (i = line_index; i < line_index + count; i++)
			line[i] = ' ';

		line_index += count;
	}
}

static void binding_backward_char(const char *arg)
{
	if (lines) {
		if (line_index > 0)
			line_index--;
		else {
			if (lines_index > 0)
				lines_index--;
			lines_adjust();
		}

		return;
	}

	if (line_index > 0)
		line_index--;
}

static void binding_forward_char(const char *arg)
{
	if (lines) {
		if (line_index < strlen(line))
			line_index++;
		else {
			if (lines_index < array_count(lines) - 1)
				lines_index++;
			lines_adjust();
		}

		return;
	}

	if (line_index < strlen(line))
		line_index++;
}

static void binding_end_of_line(const char *arg)
{
	line_adjust();
}

static void binding_beginning_of_line(const char *arg)
{
	line_index = 0;
	line_start = 0;
}

static void binding_previous_history(const char *arg)
{
	if (lines) {
		if (lines_index - lines_start == 0)
			if (lines_start)
				lines_start--;

		if (lines_index)
			lines_index--;

		lines_adjust();

		return;
	}
				
	if (history[history_index + 1]) {
		if (history_index == 0)
			history[0] = xstrdup(line);
		history_index++;
		strcpy(line, history[history_index]);
		line_adjust();
	}
}

static void binding_next_history(const char *arg)
{
	if (lines) {
		if (lines_index - line_start == 4)
			if (lines_index < array_count(lines) - 1)
				lines_start++;

		if (lines_index < array_count(lines) - 1)
			lines_index++;

		lines_adjust();

		return;
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
}

static void binding_backward_page(const char *arg)
{
	window_current->start -= window_current->height;
	if (window_current->start < 0)
		window_current->start = 0;
	window_redraw(window_current);
}

static void binding_forward_page(const char *arg)
{
	window_current->start += window_current->height;

	if (window_current->start > window_current->lines_count - window_current->height + window_current->overflow)
		window_current->start = window_current->lines_count - window_current->height + window_current->overflow;

	if (window_current->start < 0)
		window_current->start = 0;

	if (window_current->start == window_current->lines_count - window_current->height + window_current->overflow) {
		window_current->more = 0;
		update_statusbar(0);
	}
	window_redraw(window_current);
}

static void binding_ignore_query(const char *arg)
{
	char *tmp;
	
	if (!window_current->target)
		return;
	
	tmp = saprintf("/ignore %s", window_current->target);
	command_exec(NULL, tmp);
	xfree(tmp);
}

static void binding_quick_list_wrapper(const char *arg)
{
	binding_quick_list(0, 0);
}

static void binding_toggle_contacts_wrapper(const char *arg)
{
	binding_toggle_contacts(0, 0);
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
		struct binding *b = NULL;
		int ch;
		
		ekg_wait_for_key();
		ch = ekg_getch(0);

		if (ch == -1) {		/* stracony terminal */
			ekg_exit();
			continue;
		}

		if (ch == -2)		/* python ka¿e ignorowaæ */
			continue;

		if (ch == 27) {
			if ((ch = ekg_getch(27)) == -2)
				continue;

			b = binding_map_meta[ch];

			if (ch == 27)
				b = binding_map[27];

			if (b && b->action) {
				if (b->function)
					b->function(b->arg);
				else
					command_exec(NULL, b->action);
			} else {
				/* obs³uga Ctrl-F1 - Ctrl-F12 na FreeBSD */
				if (ch == '[') {
					ch = wgetch(input);
					if (ch >= 107 && ch <= 118)
						window_switch(ch - 106);
				}
			}
		} else {
			if ((b = binding_map[ch]) && b->action) {
				if (b->function)
					b->function(b->arg);
				else
					command_exec(NULL, b->action);
			} else if (ch < 255) {
				if (strlen(line) >= LINE_MAXLEN - 1)
					break;
				memmove(line + line_index + 1, line + line_index, LINE_MAXLEN - line_index - 1);

				line[line_index++] = ch;
			}
		}

		/* je¶li siê co¶ zmieni³o, wygeneruj dope³nienia na nowo */
		if (!b || (b && b->function != binding_complete)) {
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
		
		window_commit();
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
		next = window_find("__status");

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

	if (!prev->id && window_current->id == 1)
		for (l = windows; l; l = l->next)
			prev = l->data;

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
		printq("query_finished", window_current->target);
		xfree(window_current->target);
		xfree(window_current->prompt);
		window_current->target = NULL;
		window_current->prompt = NULL;
		window_current->prompt_len = 0;
		return;
	}
	
	if (id == 1) {
		printq("window_kill_status");
		return;
	}

	if (id == 0)
		return;
	
	if (w == window_current)
		window_prev();

	if (config_sort_windows) {
		list_t l;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (w->id > 1 && w->id > id)
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
 * binding_parse()
 *
 * analizuje dan± akcjê i wype³nia pola struct binding odpowiedzialne
 * za dzia³anie.
 *
 *  - b - wska¼nik wype³nianej skruktury,
 *  - action - akcja,
 */
static void binding_parse(struct binding *b, const char *action)
{
	char **args;

	if (!b || !action)
		return;

	b->action = xstrdup(action);

	args = array_make(action, " \t", 1, 1, 1);

	if (!args[0]) {
		array_free(args);
		return;
	}
	
#define __action(x,y) \
	if (!strcmp(args[0], x)) { \
		b->function = y; \
		b->arg = xstrdup(args[1]); \
	} 

	__action("backward-word", binding_backward_word);
	__action("forward-word", binding_forward_word);
	__action("kill-word", binding_kill_word);
	__action("toggle-input", binding_toggle_input);
	__action("cancel-input", binding_cancel_input);
	__action("backward-delete-char", binding_backward_delete_char);
	__action("beginning-of-line", binding_beginning_of_line);
	__action("end-of-line", binding_end_of_line);
	__action("delete-char", binding_delete_char);
	__action("backward-page", binding_backward_page);
	__action("forward-page", binding_forward_page);
	__action("kill-line", binding_kill_line);
	__action("yank", binding_yank);
	__action("accept-line", binding_accept_line);
	__action("line-discard", binding_line_discard);
	__action("quoted-insert", binding_quoted_insert);
	__action("word-rubout", binding_word_rubout);
	__action("backward-char", binding_backward_char);
	__action("forward-char", binding_forward_char);
	__action("previous-history", binding_previous_history);
	__action("next-history", binding_next_history);
	__action("complete", binding_complete);
	__action("quick-list", binding_quick_list_wrapper);
	__action("toggle-contacts", binding_toggle_contacts_wrapper);
	__action("ignore-query", binding_ignore_query);

#undef __action
}

/*
 * binding_key()
 *
 * analizuje nazwê klawisza i wpisuje akcjê do odpowiedniej mapy.
 *
 * 0/-1.
 */
int binding_key(struct binding *b, const char *key, int add)
{
	if (!strncasecmp(key, "Alt-", 4)) {
		unsigned char ch;

		if (!strcasecmp(key + 4, "Enter")) {
			b->key = xstrdup("Alt-Enter");
			if (add)
				binding_map_meta[13] = list_add(&bindings, b, sizeof(struct binding));
			return 0;
		}

		if (strlen(key) != 5)
			return -1;
	
		ch = toupper(key[4]);

		b->key = saprintf("Alt-%c", ch);

		if (add) {
			binding_map_meta[ch] = list_add(&bindings, b, sizeof(struct binding));
			if (isalpha(ch))
				binding_map_meta[tolower(ch)] = binding_map_meta[ch];
		}

		return 0;
	}

	if (!strncasecmp(key, "Ctrl-", 5)) {
		unsigned char ch;
		
		if (strlen(key) != 6)
			return -1;

		ch = toupper(key[5]);
		b->key = saprintf("Ctrl-%c", ch);

		if (add)
			binding_map[ch - 64] = list_add(&bindings, b, sizeof(struct binding));
		
		return 0;
	}

	if (toupper(key[0]) == 'F' && atoi(key + 1)) {
		int f = atoi(key + 1);

		if (f < 1 || f > 24)
			return -1;

		b->key = saprintf("F%d", f);
		
		if (add)
			binding_map[KEY_F(f)] = list_add(&bindings, b, sizeof(struct binding));
		
		return 0;
	}

#define __key(x, y, z) \
	if (!strcasecmp(key, x)) { \
		b->key = xstrdup(x); \
		if (add) { \
			binding_map[y] = list_add(&bindings, b, sizeof(struct binding)); \
			if (z) \
				binding_map[z] = binding_map[y]; \
		} \
		return 0; \
	}

	__key("Enter", 13, 0);
	__key("Escape", 27, 0);
	__key("Home", KEY_HOME, KEY_FIND);
	__key("End", KEY_END, KEY_SELECT);
	__key("Delete", KEY_DC, 0);
	__key("Backspace", KEY_BACKSPACE, 127);
	__key("Tab", 9, 0);
	__key("Left", KEY_LEFT, 0);
	__key("Right", KEY_RIGHT, 0);
	__key("Up", KEY_UP, 0);
	__key("Down", KEY_DOWN, 0);
	__key("PageUp", KEY_PPAGE, 0);
	__key("PageDown", KEY_NPAGE, 0);

#undef __key

	return -1;
}

/*
 * binding_add()
 *
 * przypisuje danemu klawiszowi akcjê.
 *
 *  - key - opis klawisza,
 *  - action - akcja,
 *  - internal - czy to wewnêtrzna akcja interfejsu?
 *  - quiet - czy byæ cicho i nie wy¶wietlaæ niczego?
 */
static void binding_add(const char *key, const char *action, int internal, int quiet)
{
	struct binding b, *c = NULL;
	list_t l;
	
	if (!key || !action)
		return;
	
	memset(&b, 0, sizeof(b));

	b.internal = internal;
	
	for (l = bindings; l; l = l->next) {
		struct binding *d = l->data;

		if (!strcasecmp(key, d->key)) {
			if (d->internal) {
				c = d;
				break;
			}
			printq("bind_seq_exist", d->key);
			return;
		}
	}

	binding_parse(&b, action);

	if (internal) {
		b.default_action = xstrdup(b.action);
		b.default_function = b.function;
		b.default_arg = xstrdup(b.arg);
	}

	if (binding_key(&b, key, (c) ? 0 : 1)) {
		printq("bind_seq_incorrect", key);
		xfree(b.action);
		xfree(b.arg);
		xfree(b.default_action);
		xfree(b.default_arg);
		xfree(b.key);
	} else {
		printq("bind_seq_add", b.key);

		if (c) {
			xfree(c->action);
			c->action = b.action;
			xfree(c->arg);
			c->arg = b.arg;
			c->function = b.function;
			xfree(b.default_action);
			xfree(b.default_arg);
			xfree(b.key);
			c->internal = 0;
		}

		if (!in_autoexec)
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

		if (b->internal) {
			printq("bind_seq_incorrect", key);
			return;
		}

		xfree(b->action);
		xfree(b->arg);
		
		if (b->default_action) {
			b->action = xstrdup(b->default_action);
			b->arg = xstrdup(b->default_arg);
			b->function = b->default_function;
			b->internal = 1;
		} else {
			xfree(b->key);
			for (i = 0; i < KEY_MAX + 1; i++) {
				if (binding_map[i] == b)
					binding_map[i] = NULL;
				if (binding_map_meta[i] == b)
					binding_map_meta[i] = NULL;
			}

			list_remove(&bindings, b, 1);
		}

		config_changed = 1;

		printq("bind_seq_remove", key);
		
		return;
	}

	printq("bind_seq_incorrect", key);
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

	if (!strcmp(event, "refresh_time"))
		timer_add(1, 0, TIMER_UI, 0, "ui-ncurses-time", "refresh_time");

        if (!strcasecmp(event, "check_mail"))
		check_mail();

	if (!strcmp(event, "variable_changed")) {
		char *name = va_arg(ap, char*);

		if (!strcasecmp(name, "sort_windows") && config_sort_windows) {
			list_t l;
			int id = 2;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;
				
				if (w->id > 1)
					w->id = id++;
			}
		}

		if (!strcasecmp(name, "timestamp")) {
			list_t l;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;
				
				window_backlog_split(w, 1, 0);
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

				window_backlog_split(w, 1, 0);
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
		int quiet = va_arg(ap, int);
		char *command = va_arg(ap, char*);

		if (!strcasecmp(command, "bind")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*), *p3 = va_arg(ap, char*);

			if (match_arg(p1, 'a', "add", 2)) {
				if (!p2 || !p3) {
					printq("not_enough_params", "bind");
				} else
					binding_add(p2, p3, 0, quiet);
			} else if (match_arg(p1, 'd', "delete", 2)) {
				if (!p2) {
					printq("not_enough_params", "bind");
				} else
					binding_delete(p2, quiet);
			} else if (match_arg(p1, 'L', "list-default", 5)) {
				binding_list(quiet, 1);
			} else
				binding_list(quiet, 0);

			goto cleanup;
		}

		if (!strcasecmp(command, "find")) {
			char *tmp = NULL;
			
			if (window_current->target) {
				struct userlist *u = userlist_find(0, window_current->target);
				struct conference *c = conference_find(window_current->target);
				int uin;

				if (u && u->uin)
					tmp = saprintf("find %d", u->uin);

				if (c && c->name)
					tmp = saprintf("conference --find %s", c->name);

				if (!u && !c && (uin = atoi(window_current->target)))
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

				if (!quiet) {
					print_window(param, 0, "query_started", param);
					print_window(param, 0, "query_started_window", param);
				}

				xfree(window_current->target);
				xfree(window_current->prompt);
				window_current->target = xstrdup(param);
				window_current->prompt = format_string(format_find("ncurses_prompt_query"), param);
				window_current->prompt_len = strlen(window_current->prompt);
			} else {
				const char *f = format_find("ncurses_prompt_none");

				printq("query_finished", window_current->target);

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

			goto cleanup;
		}

		if (!strcasecmp(command, "window")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*);

			if (!p1 || !strcasecmp(p1, "list")) {
				list_t l;

				for (l = windows; l; l = l->next) {
					struct window *w = l->data;

					if (!quiet && w->id) {
						if (w->target) {
							if (!w->floating)	
								print("window_list_query", itoa(w->id), w->target);
							else
								print("window_list_floating", itoa(w->id), itoa(w->left), itoa(w->top), itoa(w->width), itoa(w->height), w->target);
						} else
							print("window_list_nothing", itoa(w->id));
					}
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
					printq("not_enough_params", "window");
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
						printq("window_noexist");
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
					printq("not_enough_params", "window");
					goto cleanup;
				}

				argv = array_make(p2, " ,", 3, 0, 0);

				if (array_count(argv) < 3) {
					printq("not_enough_params", "window");
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
					printq("window_noexist");
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
					printq("not_enough_params", "window");
					goto cleanup;
				}

				argv = array_make(p2, " ,", 3, 0, 0);

				if (array_count(argv) < 3) {
					printq("not_enough_params", "window");
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
					printq("window_noexist");
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
					window_backlog_split(w, 1, 0);
					window_redraw(w);
					touchwin(window_current->window);
					window_commit();
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
				
				window_commit();

				goto cleanup;
			}
			
			printq("invalid_params", "window");
		}
	}

cleanup:
	va_end(ap);

	update_contacts(0);
	update_statusbar(1);
	
	return 0;
}

/*
 * header_statusbar_resize()
 *
 * zmienia rozmiar paska stanu i/lub nag³ówka okna.
 */
void header_statusbar_resize()
{
	if (!status)
		return;
	
	if (config_header_size < 0)
		config_header_size = 0;

	if (config_header_size > 5)
		config_header_size = 5;

	if (config_statusbar_size < 1)
		config_statusbar_size = 1;

	if (config_statusbar_size > 5)
		config_statusbar_size = 5;

	window_resize();

	if (config_header_size) {
		if (!header)
			header = newwin(config_header_size, stdscr->_maxx + 1, 0, 0);
		else
			wresize(header, config_header_size, stdscr->_maxx + 1);

		update_header(0);
	}

	if (!config_header_size && header) {
		delwin(header);
		header = NULL;
	}

	wresize(status, config_statusbar_size, stdscr->_maxx + 1);
	mvwin(status, config_header_size + OUTPUT_SIZE, 0);

	update_statusbar(0);

	contacts_rebuild();	/* commitnie */
}

/*
 * binding_default()
 *
 * ustawia lub przywraca domy¶lne ustawienia przypisanych klawiszy.
 */
static void binding_default()
{
	binding_add("Alt-`", "/window switch 0", 1, 1);
	binding_add("Alt-1", "/window switch 1", 1, 1);
	binding_add("Alt-2", "/window switch 2", 1, 1);
	binding_add("Alt-3", "/window switch 3", 1, 1);
	binding_add("Alt-4", "/window switch 4", 1, 1);
	binding_add("Alt-5", "/window switch 5", 1, 1);
	binding_add("Alt-6", "/window switch 6", 1, 1);
	binding_add("Alt-7", "/window switch 7", 1, 1);
	binding_add("Alt-8", "/window switch 8", 1, 1);
	binding_add("Alt-9", "/window switch 0", 1, 1);
	binding_add("Alt-0", "/window switch 10", 1, 1);
	binding_add("Alt-Q", "/window switch 11", 1, 1);
	binding_add("Alt-W", "/window switch 12", 1, 1);
	binding_add("Alt-E", "/window switch 13", 1, 1);
	binding_add("Alt-R", "/window switch 14", 1, 1);
	binding_add("Alt-T", "/window switch 15", 1, 1);
	binding_add("Alt-Y", "/window switch 16", 1, 1);
	binding_add("Alt-U", "/window switch 17", 1, 1);
	binding_add("Alt-I", "/window switch 18", 1, 1);
	binding_add("Alt-O", "/window switch 19", 1, 1);
	binding_add("Alt-P", "/window switch 20", 1, 1);
	binding_add("Alt-K", "/window kill", 1, 1);
	binding_add("Alt-N", "/window new", 1, 1);
	binding_add("Alt-G", "ignore-query", 1, 1);
	binding_add("Alt-B", "backward-word", 1, 1);
	binding_add("Alt-F", "forward-word", 1, 1);
	binding_add("Alt-D", "kill-word", 1, 1);
	binding_add("Alt-Enter", "toggle-input", 1, 1);
	binding_add("Escape", "cancel-input", 1, 1);
	binding_add("Ctrl-N", "/window next", 1, 1);
	binding_add("Ctrl-P", "/window prev", 1, 1);
	binding_add("Backspace", "backward-delete-char", 1, 1);
	binding_add("Ctrl-H", "backward-delete-char", 1, 1);
	binding_add("Ctrl-A", "beginning-of-line", 1, 1);
	binding_add("Home", "beginning-of-line", 1, 1);
	binding_add("Ctrl-D", "delete-char", 1, 1);
	binding_add("Delete", "delete-char", 1, 1);
	binding_add("Ctrl-E", "end-of-line", 1, 1);
	binding_add("Ctrl-K", "kill-line", 1, 1);
	binding_add("Ctrl-Y", "yank", 1, 1);
	binding_add("Enter", "accept-line", 1, 1);
	binding_add("Ctrl-M", "accept-line", 1, 1);
	binding_add("Ctrl-U", "line-discard", 1, 1);
	binding_add("Ctrl-V", "quoted-insert", 1, 1);
	binding_add("Ctrl-W", "word-rubout", 1, 1);
	binding_add("Ctrl-L", "/window refresh", 1, 1);
	binding_add("Tab", "complete", 1, 1);
	binding_add("Right", "forward-char", 1, 1);
	binding_add("Left", "backward-char", 1, 1);
	binding_add("Up", "previous-history", 1, 1);
	binding_add("Down", "next-history", 1, 1);
	binding_add("PageUp", "backward-page", 1, 1);
	binding_add("Ctrl-F", "backward-page", 1, 1);
	binding_add("PageDown", "forward-page", 1, 1);
	binding_add("Ctrl-G", "forward-page", 1, 1);
	binding_add("F1", "/help", 1, 1);
	binding_add("F2", "quick-list", 1, 1);
	binding_add("F3", "toggle-contacts", 1, 1);
	binding_add("F12", "/window switch 0", 1, 1);
}
