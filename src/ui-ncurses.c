/* $Id$ */

/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Wojtek Bojdo³ <wojboj@htcon.pl>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *
 *  Aspell support added by Piotr 'Deletek' Kupisiewicz <deli@rzepaknet.us>
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

#include "config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#else
#  include "../compat/dirname.h"
#endif
#ifndef HAVE_SCANDIR
#  include "../compat/scandir.h"
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "commands.h"
#include "libgadu.h"
#include "mail.h"
#ifdef WITH_PYTHON
#  include "python.h"
#endif
#ifndef HAVE_STRLCAT
#  include "../compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "../compat/strlcpy.h"
#endif
#ifdef WITH_ASPELL
#	include <aspell.h>
#endif
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "userlist.h"
#include "vars.h"
#include "version.h"
#include "xmalloc.h"

/* nadpisujemy funkcjê strncasecmp() odpowiednikiem z obs³ug± polskich znaków */
#define strncasecmp(x...) strncasecmp_pl(x)

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
	int doodle;		/* czy do gryzmolenia? */
	int frames;		/* informacje o ramkach */
	int edge;		/* okienko brzegowe */
	int last_update;	/* czas ostatniego uaktualnienia */
	int nowrap;		/* nie zawijamy linii */
	int hide;		/* ukrywamy, bo jest zbyt du¿e */
	
	char *target;		/* nick query albo inna nazwa albo NULL */
	
	int id;			/* numer okna */
	int act;		/* czy co¶ siê zmieni³o? */
	int more;		/* pojawi³o siê co¶ poza ekranem */

	char *prompt;		/* sformatowany prompt lub NULL */
	int prompt_len;		/* d³ugo¶æ prompta lub 0 */

	int left, top;		/* pozycja (x, y) wzglêdem pocz±tku ekranu */
	int width, height;	/* wymiary okna */

	int margin_left, margin_right, margin_top, margin_bottom;
				/* marginesy */

	fstring_t *backlog;	/* bufor z liniami */
	int backlog_size;	/* rozmiar backloga */

	int redraw;		/* trzeba przerysowaæ przed wy¶wietleniem */

	int start;		/* od której linii zaczyna siê wy¶wietlanie */
	int lines_count;	/* ilo¶æ linii ekranowych w backlogu */
	struct screen_line *lines;	
				/* linie ekranowe */

	int overflow;		/* ilo¶æ nadmiarowych linii w okienku */

	int (*handle_redraw)(struct window *w);
				/* obs³uga przerysowania zawarto¶ci okna */
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
static int window_last_id = -1;		/* numer ostatnio wybranego okna */
static int input_size = 1;		/* rozmiar okna wpisywania tekstu */
static int ui_ncurses_debug = 0;	/* debugowanie */
static int ui_ncurses_inited = 0;	/* czy zainicjowano? */

static struct termios old_tio;

int config_backlog_size = 1000;		/* maksymalny rozmiar backloga */
int config_display_transparent = 1;	/* czy chcemy przezroczyste t³o? */
int config_contacts_size = 9;		/* szeroko¶æ okna kontaktów */
int config_contacts = 2;		/* czy ma byæ okno kontaktów */
char *config_contacts_options = NULL;	/* opcje listy kontaktów */
char *config_contacts_groups = NULL;	/* grupy listy kontaktów */

static int contacts_margin = 1;
static int contacts_edge = WF_RIGHT;
static int contacts_frame = WF_LEFT;
static int contacts_descr = 0;
static int contacts_wrap = 0;
static int contacts_order[5] = { 0, 1, 2, 3, -1 };
static int contacts_framecolor = 4;
static int contacts_group_index = 0;

struct binding *binding_map[KEY_MAX + 1];	/* mapa bindowanych klawiszy */
struct binding *binding_map_meta[KEY_MAX + 1];	/* j.w. z altem */

static void window_kill(struct window *w, int quiet);
static int window_backlog_split(struct window *w, int full, int removed);
static void window_redraw(struct window *w);
static void window_clear(struct window *w, int full);
static void window_refresh();

static int contacts_update(struct window *w);

#ifndef COLOR_DEFAULT
#  define COLOR_DEFAULT (-1)
#endif

#ifdef WITH_ASPELL
#  define ASPELLCHAR 5
// #  define ASPELL_ALLOWED_CHARS "()[]',.`\"<>!?"
AspellConfig * spell_config;
AspellSpeller * spell_checker = 0;
static char *aspell_line;
#endif


/*
 * zamienia podany znak na ma³y je¶li to mo¿liwe
 * obs³uguje polskie znaki
 */
static int tolower_pl(const unsigned char c) {
        switch(c) {
                case 161: // ¡
                        return 177;
                case 198: // Æ
                        return 230;
                case 202: // Ê
                        return 234;
                case 163: // £
                        return 179;
                case 209: // Ñ
                        return 241;
                case 211: // Ó
                        return 243;
                case 166: // ¦
                        return 182;
                case 175: // ¯
                        return 191;
                case 172: // ¬
                        return 188;
                default: //reszta
                        return tolower(c);
        }
}

/*
 * porównuje dwa ci±gi o okre¶lonej przez n d³ugo¶ci
 * dzia³a analogicznie do strncasecmp()
 * obs³uguje polskie znaki
 */

int strncasecmp_pl(const char * cs,const char * ct,size_t count)
{
        register signed char __res = 0;

        while (count) {
                if ((__res = tolower_pl(*cs) - tolower_pl(*ct++)) != 0 || !*cs++)
                        break;
                count--;
        }
        return __res;
}

/*
 * zamienia wszystkie znaki ci±gu na ma³e
 * zwraca ci±g po zmianach
 */
static char *str_tolower(const char *text) {
        int i;
        char *tmp;

        tmp = xmalloc(strlen(text) + 1);

        for(i=0; i < strlen(text); i++)
                tmp[i] = tolower_pl(text[i]);
        tmp[i] = '\0';
        return tmp;
}

/*
 * contacts_size()
 *
 * liczy szeroko¶æ okna z list± kontaktów.
 */
int contacts_size()
{
	if (!config_contacts)
		return 0;

	if (config_contacts_size + 2 > (stdscr->_maxx + 1) / 2)
		return 0;

	return config_contacts_size + (contacts_frame) ? 1 : 0;
}

/*
 * ui_debug()
 *
 * wy¶wietla szybko sformatowany tekst w aktualnym oknie. do debugowania.
 */
#define ui_debug(x...) { \
	char *ui_debug_tmp = saprintf("UI " x); \
	ui_ncurses_print("__debug", 0, ui_debug_tmp); \
	xfree(ui_debug_tmp); \
}

/*
 * sprawdza czy podany znak jest znakiem alphanumerycznym (uwzlglednia polskie znaki)
 */
int isalpha_pl(unsigned char c)
{
/*  gg_debug(GG_DEBUG_MISC, "c: %d\n", c); */
    if(isalpha(c)) // normalne znaki
        return 1;
    else if(c == 177 || c == 230 || c == 234 || c == 179 || c == 241 || c == 243 || c == 182 || c == 191 || c == 188) /* polskie literki */
	return 1;
    else if(c == 161 || c == 198 || c == 202 || c == 209 || c == 163 || c == 211 || c == 166 || c == 175 || c == 172) /* wielka litery polskie */
	return 1;
    else 
	return 0;
}

/*
 * color_pair()
 *
 * zwraca numer COLOR_PAIR odpowiadaj±cej danej parze atrybutów: kolorze
 * tekstu (plus pogrubienie) i kolorze t³a.
 */
static int color_pair(int fg, int bold, int bg)
{
	if (fg >= 8) {
		bold = 1;
		fg &= 7;
	}

	if (fg == COLOR_BLACK && bg == COLOR_BLACK) {
		fg = 7;
	} else if (fg == COLOR_WHITE && bg == COLOR_BLACK) {
		fg = 0;
	}

	if (!config_display_color) {
		if (bg != COLOR_BLACK)
			return A_REVERSE;
		else
			return A_NORMAL | ((bold) ? A_BOLD : 0);
	}
		
	return COLOR_PAIR(fg + 8 * bg) | ((bold) ? A_BOLD : 0);
}

/*
 * window_commit()
 *
 * zatwierdza wszystkie zmiany w buforach ncurses i wy¶wietla je na ekranie.
 */
void window_commit()
{
	window_refresh();

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
			if (!w->backlog[i]->prompt_empty) {
				l->prompt_str = w->backlog[i]->str;
				l->prompt_attr = w->backlog[i]->attr;
			} else {
				l->prompt_str = NULL;
				l->prompt_attr = NULL;
			}

			if (!w->floating && config_timestamp) {
				struct tm *tm = localtime(&ts);
				char buf[100];
				strftime(buf, sizeof(buf), config_timestamp, tm);
				l->ts = xstrdup(buf);
				l->ts_len = strlen(l->ts);
			}

			width = w->width - l->ts_len - l->prompt_len - w->margin_left - w->margin_right;
			if ((w->frames & WF_LEFT))
				width -= 1;
			if ((w->frames & WF_RIGHT))
				width -= 1;

			if (l->len < width)
				break;
			
			for (j = 0, word = 0; j < l->len; j++) {

				if (str[j] == ' ' && !w->nowrap)
					word = j + 1;

				if (j == width) {
					l->len = (word) ? word : width;
					if (str[j] == ' ') {
						l->len--;
						str++;
						attr++;
					}
					break;
				}
			}

			if (w->nowrap) {
				while (*str) {
					str++;
					attr++;
				}

				break;
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
		if (window_current && window_current->id == w->id) 
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
	int left, right, top, bottom, width, height;
	list_t l;

	left = 0;
	right = stdscr->_maxx + 1;
	top = config_header_size;
	bottom = stdscr->_maxy + 1 - input_size - config_statusbar_size;
	width = right - left;
	height = bottom - top;

	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (!w->edge)
			continue;

		w->hide = 0;

		if ((w->edge & WF_LEFT)) {
			if (w->width * 2 > width)
				w->hide = 1;
			else {
				w->left = left;
				w->top = top;
				w->height = height;
				w->hide = 0;
				width -= w->width;
				left += w->width;
			}
		}

		if ((w->edge & WF_RIGHT)) {
			if (w->width * 2 > width)
				w->hide = 1;
			else {
				w->left = right - w->width;
				w->top = top;
				w->height = height;
				width -= w->width;
				right -= w->width;
			}
		}

		if ((w->edge & WF_TOP)) {
			if (w->height * 2 > height)
				w->hide = 1;
			else {
				w->left = left;
				w->top = top;
				w->width = width;
				height -= w->height;
				top += w->height;
			}
		}

		if ((w->edge & WF_BOTTOM)) {
			if (w->height * 2 > height)
				w->hide = 1;
			else {
				w->left = left;
				w->top = bottom - w->height;
				w->width = width;
				height -= w->height;
				bottom -= w->height;
			}
		}

		wresize(w->window, w->height, w->width);
		mvwin(w->window, w->top, w->left);

/*		ui_debug("edge window id=%d resized to (%d,%d,%d,%d)\n", w->id, w->left, w->top, w->width, w->height); */
		
		w->redraw = 1;
	}

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;
		int delta;

		if (w->floating)
			continue;

		delta = height - w->height;

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

		if (w->overflow > height)
			w->overflow = height;

		w->height = height;

		if (w->height < 1)
			w->height = 1;

		if (w->width != width && !w->doodle) {
			w->width = width;
			window_backlog_split(w, 1, 0);
		}

		w->width = width;
		
		wresize(w->window, w->height, w->width);

		w->top = top;
		w->left = left;

		if (w->left < 0)
			w->left = 0;
		if (w->left > stdscr->_maxx)
			w->left = stdscr->_maxx;

		if (w->top < 0)
			w->top = 0;
		if (w->top > stdscr->_maxy)
			w->top = stdscr->_maxy;

		mvwin(w->window, w->top, w->left);

/*		ui_debug("normal window id=%d resized to (%d,%d,%d,%d)\n", w->id, w->left, w->top, w->width, w->height); */

		if (w->overflow)
			w->start = w->lines_count - w->height + w->overflow;

		w->redraw = 1;
	}

	ui_screen_width = width;
	ui_screen_height = height;
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
	int x, y, left = w->margin_left, top = w->margin_top, height = w->height - w->margin_top - w->margin_bottom;
	
	if (w->doodle) {
		w->redraw = 0;
		return;
	}

	if (w->handle_redraw) {
		/* handler mo¿e sam narysowaæ wszystko, wtedy zwraca -1.
		 * mo¿e te¿ tylko uaktualniæ zawarto¶æ okna, wtedy zwraca
		 * 0 i rysowaniem zajmuje siê ta funkcja. */
		if (w->handle_redraw(w) == -1)
			return;
	}
	
	werase(w->window);
	wattrset(w->window, color_pair(contacts_framecolor, 0, COLOR_BLACK));

	if (w->floating) {
		if ((w->frames & WF_LEFT)) {
			left++;

			for (y = 0; y < w->height; y++)
				mvwaddch(w->window, y, w->margin_left, ACS_VLINE);
		}

		if ((w->frames & WF_RIGHT)) {
			for (y = 0; y < w->height; y++)
				mvwaddch(w->window, y, w->width - 1 - w->margin_right, ACS_VLINE);
		}
			
		if ((w->frames & WF_TOP)) {
			top++;
			height--;

			for (x = 0; x < w->width; x++)
				mvwaddch(w->window, w->margin_top, x, ACS_HLINE);
		}

		if ((w->frames & WF_BOTTOM)) {
			height--;

			for (x = 0; x < w->width; x++)
				mvwaddch(w->window, w->height - 1 - w->margin_bottom, x, ACS_HLINE);
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

	if (w->start < 0)
		w->start = 0;

	for (y = 0; y < height && w->start + y < w->lines_count; y++) {
		struct screen_line *l = &w->lines[w->start + y];

		wattrset(w->window, A_NORMAL);

		for (x = 0; l->ts && x < l->ts_len; x++)
			mvwaddch(w->window, top + y, left + x, (unsigned char) l->ts[x]);

		for (x = 0; x < l->prompt_len + l->len; x++) {
			int attr = A_NORMAL;
			unsigned char ch, chattr;
			
			if (x < l->prompt_len) {
				if (!l->prompt_str)
					continue;
				
				ch = l->prompt_str[x];
				chattr = l->prompt_attr[x];
			} else {
				ch = l->str[x - l->prompt_len];
				chattr = l->attr[x - l->prompt_len];
			}

			if ((chattr & 64))
				attr |= A_BOLD;

			if (!(chattr & 128))
				attr |= color_pair(chattr & 7, 0, COLOR_BLACK);

			if (ch == 10) {
				ch = '|';
				attr |= color_pair(COLOR_BLACK, 1, COLOR_BLACK);
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
	w->more = 0;
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
	struct userlist *u = NULL;

	if (!target || current) {
		if (window_current->id)
			return window_current;
		else
			status = 1;
	}

	if (target)
		u = userlist_find(get_uin(target), NULL);

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (!w->id && debug)
			return w;

		if (w->id == 1 && status)
			return w;

		if (w->target && target) {
			if (!strcasecmp(target, w->target))
				return w;

			if (u && u->display && !strcasecmp(u->display, w->target))
				return w;

			if (u && !strcasecmp(itoa(u->uin), w->target))
				return w;
		}
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

		/* je¶li ma w³asn± obs³ugê od¶wie¿ania, nie ruszamy */
		if (w->handle_redraw)
			continue;
		
		if (w->last_update == time(NULL))
			continue;

		w->last_update = time(NULL);

		window_clear(w, 1);
		tmp = window_current;
		window_current = w;
		command_exec(w->target, w->target, 0);
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

		if (!w->hide)
			wnoutrefresh(w->window);
	}

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (!w->floating || w->hide)
			continue;

		if (w->handle_redraw)
			window_redraw(w);
		else
			window_floating_update(w->id);

		touchwin(w->window);
		wnoutrefresh(w->window);
	}
	
	mvwin(status, stdscr->_maxy + 1 - input_size - config_statusbar_size, 0);
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

		if (id != window_current->id)
			window_last_id = window_current->id;

		window_current = w;

		w->act = 0;
		
		if (w->redraw)
			window_redraw(w);

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
	struct window w, *result = NULL;
	int id = 1, done = 0;
	list_t l;
	struct userlist *u = NULL;

	if (target) {
		struct window *w = window_find(target);
		if (w)
			return w;

		u = userlist_find(0, target);

		if (!strcmp(target, "$"))
			return window_current;
	}

	if (target && *target == '*' && !u)
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

	/* domy¶lne rozmiary zostan± dostosowane przez window_resize() */
	w.top = 0;
	w.left = 0;
	w.width = 1;
	w.height = 1;

	if (target) {
		if (!strcmp(target, "__contacts")) {
			int size = config_contacts_size + contacts_margin + ((contacts_frame) ? 1 : 0);
			switch (contacts_edge) {
				case WF_LEFT:
					w.width = size;
					w.margin_right = contacts_margin;
					break;
				case WF_RIGHT:
					w.width = size;
					w.margin_left = contacts_margin;
					break;
				case WF_TOP:
					w.height = size;
					w.margin_bottom = contacts_margin;
					break;
				case WF_BOTTOM:
					w.height = size;
					w.margin_top = contacts_margin;
					break;
			}

			w.floating = 1;
			w.edge = contacts_edge;
			w.frames = contacts_frame;
			w.handle_redraw = contacts_update;
			w.target = xstrdup(target);
			w.nowrap = !contacts_wrap;
		}

		if (*target == '+' && !u) {
			w.doodle = 1;
			w.target = xstrdup(target + 1);
		}
		
		if (*target == '*' && !u) {
			const char *tmp = strchr(target, '/');
			char **argv, **arg;
			
			w.floating = 1;
			
			if (!tmp)
				tmp = "";
				
			w.target = xstrdup(tmp);

			argv = arg = array_make(target + 1, ",", 5, 0, 0);

			w.width = 10;	/* domy¶lne wymiary */
			w.height = 5;

			if (*arg)
				w.left = atoi(*arg++);
			if (*arg)
				w.top = atoi(*arg++);
			if (*arg)
				w.width = atoi(*arg++);
			if (*arg)
				w.height = atoi(*arg++);
			if (*arg && *arg[0] != '/')
				w.frames = atoi(*arg);

			array_free(argv);

			if (w.left > stdscr->_maxx)
				w.left = stdscr->_maxx;
			if (w.top > stdscr->_maxy)
				w.top = stdscr->_maxy;
			if (w.left + w.width > stdscr->_maxx)
				w.width = stdscr->_maxx + 1 - w.left;
			if (w.top + w.height > stdscr->_maxy)
				w.height = stdscr->_maxy + 1 - w.top;
		}
		
		if (!w.target) {
			w.target = xstrdup(target);
			w.prompt = format_string(format_find("ncurses_prompt_query"), target);
			w.prompt_len = strlen(w.prompt);
		}
	}
	
	if (!target) {
		const char *f = format_find("ncurses_prompt_none");

		if (strcmp(f, "")) {
			w.prompt = xstrdup(f);
			w.prompt_len = strlen(w.prompt);
		}
	}

 	w.window = newwin(w.height, w.width, w.top, w.left);
	result = list_add_sorted(&windows, &w, sizeof(w), window_new_compare);

	window_resize();

	return result;
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

				if (separate && !w->target && w->id > 1) {
					w->target = xstrdup(target);
					xfree(w->prompt);
					w->prompt = format_string(format_find("ncurses_prompt_query"), target);
					w->prompt_len = strlen(w->prompt);
					print("window_id_query_started", itoa(w->id), target);
					print_window(target, 1, "query_started", target);
					print_window(target, 1, "query_started_window", target);
					if (!(ignored_check(get_uin(target)) & IGNORE_EVENTS))
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
					if (!(ignored_check(get_uin(target)) & IGNORE_EVENTS))
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

	/* albo zaczynamy, albo koñczymy i nie ma okienka ¿adnego */
	if (!w) 
		return;
 
	if (w != window_current && !w->floating) {
		w->act = 1;
		if (!command_processing)
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

	if (w->overflow) {
		w->overflow -= count;

		if (w->overflow < 0) {
			bottom = 1;
			w->overflow = 0;
		}
	}

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
		if (!command_processing)
			window_commit();
	}

	if (config_speech_app && !w->floating && w->id)
		say_it(speech->str);

	string_free(speech, 1);
}

/*
 * contacts_update()
 *
 * uaktualnia okno listy kontaktów.
 */
static int contacts_update(struct window *w)
{
	struct {
		int status1, status2;
		char *format, *format_descr, *format_descr_full, *format_header, *format_footer;
	} table[5] = {
		{ GG_STATUS_AVAIL, GG_STATUS_AVAIL_DESCR, "contacts_avail", "contacts_avail_descr", "contacts_avail_descr_full", "contacts_avail_header", "contacts_avail_footer" },
		{ GG_STATUS_BUSY, GG_STATUS_BUSY_DESCR, "contacts_busy", "contacts_busy_descr", "contacts_busy_descr_full", "contacts_busy_header", "contacts_busy_footer" },
		{ GG_STATUS_INVISIBLE, GG_STATUS_INVISIBLE_DESCR, "contacts_invisible", "contacts_invisible_descr", "contacts_invisible_descr_full", "contacts_invisible_header", "contacts_invisible_footer" },
		{ GG_STATUS_BLOCKED, -1, "contacts_blocking", "contacts_blocking", "contacts_blocking", "contacts_blocking_header", "contacts_blocking_footer" },
		{ GG_STATUS_NOT_AVAIL, GG_STATUS_NOT_AVAIL_DESCR, "contacts_not_avail", "contacts_not_avail_descr", "contacts_not_avail_descr_full", "contacts_not_avail_header", "contacts_not_avail_footer" },
	};
	const char *header = NULL, *footer = NULL;
	char *group = NULL;
	int j;
		
	if (!w) {
		list_t l;

		for (l = windows; l; l = l->next) {
			struct window *v = l->data;
			
			if (v->target && !strcmp(v->target, "__contacts")) {
				w = v;
				break;
			}
		}

		if (!w)
			return -1;
	}
	
	window_clear(w, 1);

	if (config_contacts_groups) {
		char **groups = array_make(config_contacts_groups, ", ", 0, 1, 0);
		if (contacts_group_index > array_count(groups))
			contacts_group_index = 0;

		if (contacts_group_index > 0) {
			group = groups[contacts_group_index - 1];
			if (*group == '@')
				group++;
			group = xstrdup(group);
			header = format_find("contacts_header_group");
			footer = format_find("contacts_footer_group");
		}

		array_free(groups);
	}

	if (!header || !footer) {
		header = format_find("contacts_header");
		footer = format_find("contacts_footer");
	}
	
	if (strcmp(header, "")) 
		window_backlog_add(w, reformat_string(format_string(header, group)));

	for (j = 0; j < 5; j++) {
		const char *header, *footer;
		int i = contacts_order[j], count;
		list_t l;

		if (i < 0 || i > 4)
			continue;

		header = format_find(table[i].format_header);
		footer = format_find(table[i].format_footer);
		count = 0;

		for (l = userlist; l; l = l->next) {
			struct userlist *u = l->data;
			const char *format;
			char *line;

			if ((u->status != table[i].status1 && u->status != table[i].status2) || !u->display || !u->uin)
				continue;

			if (group && !group_member(u, group))
				continue;

			if (!count && strcmp(header, ""))
				window_backlog_add(w, reformat_string(format_string(header)));

			if (GG_S_D(u->status) && contacts_descr)
				format = table[i].format_descr_full;
			else if (GG_S_D(u->status) && !contacts_descr)
				format = table[i].format_descr;
			else
				format = table[i].format;

			line = format_string(format_find(format), u->display, u->descr);
			window_backlog_add(w, reformat_string(line));
			xfree(line);

			count++;
		}

		if (count && strcmp(footer, ""))
			window_backlog_add(w, reformat_string(format_string(footer)));
	}

	if (strcmp(footer, "")) 
		window_backlog_add(w, reformat_string(format_string(footer, group)));

	xfree(group);

	w->redraw = 1;

	return 0;
}

/*
 * contacts_changed()
 *
 * wywo³ywane przy zmianach rozmiaru i w³±czeniu klienta.
 */
void contacts_changed()
{
	struct window *w = NULL;
	list_t l;

	if (config_contacts_size < 0)
		config_contacts_size = 0;

	if (config_contacts_size > 1000)
		config_contacts_size = 1000;
	
	contacts_margin = 1;
	contacts_edge = WF_RIGHT;
	contacts_frame = WF_LEFT;
	contacts_order[0] = 0;
	contacts_order[1] = 1;
	contacts_order[2] = 2;
	contacts_order[3] = 3;
	contacts_order[4] = -1;
	contacts_wrap = 0;
	contacts_descr = 0;

	if (config_contacts_options) {
		char **args = array_make(config_contacts_options, " \t,", 0, 1, 1);
		int i;

		for (i = 0; args[i]; i++) {
			if (!strcasecmp(args[i], "left")) {
				contacts_edge = WF_LEFT;
				if (contacts_frame)
					contacts_frame = WF_RIGHT;
			}

			if (!strcasecmp(args[i], "right")) {
				contacts_edge = WF_RIGHT;
				if (contacts_frame)
					contacts_frame = WF_LEFT;
			}

			if (!strcasecmp(args[i], "top")) {
				contacts_edge = WF_TOP;
				if (contacts_frame)
					contacts_frame = WF_BOTTOM;
			}

			if (!strcasecmp(args[i], "bottom")) {
				contacts_edge = WF_BOTTOM;
				if (contacts_frame)
					contacts_frame = WF_TOP;
			}

			if (!strcasecmp(args[i], "noframe"))
				contacts_frame = 0;

			if (!strcasecmp(args[i], "frame")) {
				switch (contacts_edge) {
					case WF_TOP:
						contacts_frame = WF_BOTTOM;
						break;
					case WF_BOTTOM:
						contacts_frame = WF_TOP;
						break;
					case WF_LEFT:
						contacts_frame = WF_RIGHT;
						break;
					case WF_RIGHT:
						contacts_frame = WF_LEFT;
						break;
				}
			}

			if (!strncasecmp(args[i], "margin=", 7)) {
				contacts_margin = atoi(args[i] + 7);
				if (contacts_margin > 10)
					contacts_margin = 10;
				if (contacts_margin < 0)
					contacts_margin = 0;
			}

			if (!strcasecmp(args[i], "nomargin"))
				contacts_margin = 0;

			if (!strcasecmp(args[i], "wrap"))
				contacts_wrap = 1;

			if (!strcasecmp(args[i], "nowrap"))
				contacts_wrap = 0;

			if (!strcasecmp(args[i], "descr"))
				contacts_descr = 1;

			if (!strcasecmp(args[i], "nodescr"))
				contacts_descr = 0;

			if (!strncasecmp(args[i], "framecolor=", 11))
				if (args[i][11])
					sscanf(args[i] + 11, "%d", &contacts_framecolor);

			if (!strncasecmp(args[i], "order=", 6)) {
				int j;
				
				contacts_order[0] = -1;
				contacts_order[1] = -1;
				contacts_order[2] = -1;
				contacts_order[3] = -1;
				contacts_order[4] = -1;
				
				for (j = 0; args[i][j + 6] && j < 5; j++)
					if (args[i][j + 6] >= '0' && args[i][j + 6] <= '4')
						contacts_order[j] = args[i][j + 6] - '0';	
			}
		}

		if (contacts_margin < 0)
			contacts_margin = 0;

		array_free(args);
	}
	
	for (l = windows; l; l = l->next) {
		struct window *v = l->data;

		if (v->target && !strcmp(v->target, "__contacts")) {
			w = v;
			break;
		}
	}

	if (w) {
		window_kill(w, 1);
		w = NULL;
	}

	if (config_contacts && !w)
		window_new("__contacts", 1000);
	
	contacts_update(NULL);
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

	wattrset(header, color_pair(COLOR_WHITE, 0, COLOR_BLUE));

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
 * window_printat()
 *
 * wy¶wietla dany tekst w danym miejscu okna.
 *
 *  - w - okno ncurses, do którego piszemy
 *  - x, y - wspó³rzêdne, od których zaczynamy
 *  - format - co mamy wy¶wietliæ
 *  - data - dane do podstawienia w formatach
 *  - fgcolor - domy¶lny kolor tekstu
 *  - bold - domy¶lne pogrubienie
 *  - bgcolor - domy¶lny kolor t³a
 *  - status - czy to pasek stanu albo nag³ówek okna?
 *
 * zwraca ilo¶æ dopisanych znaków.
 */
int window_printat(WINDOW *w, int x, int y, const char *format_, void *data_, int fgcolor, int bold, int bgcolor, int status)
{
	int orig_x = x;
	int backup_display_color = config_display_color;
	char *format = (char*) format_;
	const char *p;
	struct format_data *data = data_;

	if (!config_display_pl_chars) {
		format = xstrdup(format);
		iso_to_ascii(format);
	}

	p = format;

	if (status && config_display_color == 2)
		config_display_color = 0;
	
	if (status && x == 0) {
		int i;

		wattrset(w, color_pair(fgcolor, 0, bgcolor));

		wmove(w, y, 0);

		for (i = 0; i <= w->_maxx; i++)
			waddch(w, ' ');
	}

	wmove(w, y, x);
			
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

#define __fgcolor(x,y,z) \
		case x: fgcolor = z; bold = 0; break; \
		case y: fgcolor = z; bold = 1; break;
#define __bgcolor(x,y) \
		case x: bgcolor = y; break;

		if (*p != '{') {
			switch (*p) {
				__fgcolor('k', 'K', COLOR_BLACK);
				__fgcolor('r', 'R', COLOR_RED);
				__fgcolor('g', 'G', COLOR_GREEN);
				__fgcolor('y', 'Y', COLOR_YELLOW);
				__fgcolor('b', 'B', COLOR_BLUE);
				__fgcolor('m', 'M', COLOR_MAGENTA);
				__fgcolor('c', 'C', COLOR_CYAN);
				__fgcolor('w', 'W', COLOR_WHITE);
				__bgcolor('l', COLOR_BLACK);
				__bgcolor('s', COLOR_RED);
				__bgcolor('h', COLOR_GREEN);
				__bgcolor('z', COLOR_YELLOW);
				__bgcolor('e', COLOR_BLUE);
				__bgcolor('q', COLOR_MAGENTA);
				__bgcolor('d', COLOR_CYAN);
				__bgcolor('x', COLOR_WHITE);
				case 'n':
					bgcolor = COLOR_BLUE;
					fgcolor = COLOR_WHITE;
					bold = 0;
					break;
				case '|':
					while (x <= w->_maxx) {
						waddch(w, ' ');
						x++;
					}
					break;
				case '}':
					waddch(w, '}');
					p++;
					x++;
					break;

			}

			p++;

			wattrset(w, color_pair(fgcolor, bold, bgcolor));
			
			continue;
		}
#undef __fgcolor
#undef __bgcolor

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
				char *text = data[i].text;
				int j;

				if (!config_display_pl_chars) {
					text = xstrdup(text);
					iso_to_ascii(text);
				}

				for (j = 0; text && j < strlen(text); j++) {
					if (text[j] != 10) {
						waddch(w, (unsigned char) text[j]);
						continue;
					}

					wattrset(w, color_pair(COLOR_BLACK, 1, bgcolor));
					waddch(w, '|');
					wattrset(w, color_pair(fgcolor, bold, bgcolor));
				}

				p += len;
				x += strlen(data[i].text);
				
				if (!config_display_pl_chars)
					xfree(text);
				
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
				int len, matched = ((data[i].text) ? 1 : 0);

				if (neg)
					matched = !matched;

				len = strlen(data[i].name);

				if (!strncmp(p, data[i].name, len) && p[len] == ' ') {
					p += len + 1;

					if (matched)
						x += window_printat(w, x, y, p, data, fgcolor, bold, bgcolor, status);
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

	config_display_color = backup_display_color;

	if (!config_display_pl_chars)
		xfree(format);

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
	struct userlist *q = userlist_find(str_to_uin(window_current->target), window_current->target);
	struct format_data *formats = NULL;
	int formats_count = 0, i, y;

	wattrset(status, color_pair(config_statusbar_fgcolor, 0, config_statusbar_bgcolor));
	if (header)
		wattrset(header, color_pair(config_statusbar_fgcolor, 0, config_statusbar_bgcolor));

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
		char tmp[80];

		tm = localtime(&t);

		strftime(tmp, sizeof(tmp), format_find("ncurses_timestamp"), tm);
		
		__add_format("time", 1, tmp);
	}

	__add_format("window", window_current->id, itoa(window_current->id));
	__add_format("uin", config_uin, itoa(config_uin));
	__add_format("nick", (u && u->display), u->display);
	__add_format("query", window_current->target, window_current->target);
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
	__add_format("away", (sess && sess->state == GG_STATE_CONNECTED && GG_S_B(config_status)), "");
	__add_format("busy", (sess && sess->state == GG_STATE_CONNECTED && GG_S_B(config_status)), "");
	__add_format("avail", (sess && sess->state == GG_STATE_CONNECTED && GG_S_A(config_status)), "");
	__add_format("invisible", (sess && sess->state == GG_STATE_CONNECTED && GG_S_I(config_status)), "");
	__add_format("notavail", (!sess || sess->state != GG_STATE_CONNECTED), "");
	__add_format("more", (window_current->more), "");

	__add_format("query_descr", (q && GG_S_D(q->status)), q->descr);
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

		if (!y) {
			p = format_find("header1");

			if (!strcmp(p, ""))
				p = format_find("header");
		} else {
			char *tmp = saprintf("header%d", y + 1);
			p = format_find(tmp);
			xfree(tmp);
		}

		window_printat(header, 0, y, p, formats, config_statusbar_fgcolor, 0, config_statusbar_bgcolor, 1);
	}

	for (y = 0; y < config_statusbar_size; y++) {
		const char *p;

		if (!y) {
			p = format_find("statusbar1");

			if (!strcmp(p, ""))
				p = format_find("statusbar");
		} else {
			char *tmp = saprintf("statusbar%d", y + 1);
			p = format_find(tmp);
			xfree(tmp);
		}

		switch (ui_ncurses_debug) {
			case 0:
				window_printat(status, 0, y, p, formats, config_statusbar_fgcolor, 0, config_statusbar_bgcolor, 1);
				break;
				
			case 1:
			{
				char *tmp = saprintf(" debug: lines_count=%d start=%d height=%d overflow=%d screen_width=%d", window_current->lines_count, window_current->start, window_current->height, window_current->overflow, ui_screen_width);
				window_printat(status, 0, y, tmp, formats, config_statusbar_fgcolor, 0, config_statusbar_bgcolor, 1);
				xfree(tmp);
				break;
			}

			case 2:
			{
				char *tmp = saprintf(" debug: lines(count=%d,start=%d,index=%d), line(start=%d,index=%d)", array_count(lines), lines_start, lines_index, line_start, line_index);
				window_printat(status, 0, y, tmp, formats, config_statusbar_fgcolor, 0, config_statusbar_bgcolor, 1);
				xfree(tmp);
				break;
			}
		}
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

#ifdef SIGWINCH
static void sigwinch_handler()
{
	ui_resize_term = 1;
	signal(SIGWINCH, sigwinch_handler);
}
#endif

/*
 * ui_ncurses_beep()
 *
 * ostentacyjnie wzywa u¿ytkownika do reakcji.
 */
static void ui_ncurses_beep()
{
	beep();
}

#ifdef WITH_ASPELL
/*
 * inicjuje slownik, ustawia kodowanie na takie jakie mamy w konfigu.
 */
void spellcheck_init(void)
{
        AspellCanHaveError * possible_err;
	if(config_aspell != 1)
	    return;
        spell_config = new_aspell_config();
        aspell_config_replace(spell_config, "encoding", config_aspell_encoding);
        aspell_config_replace(spell_config, "lang", config_aspell_lang);
	possible_err = new_aspell_speller(spell_config);

        if (aspell_error_number(possible_err) != 0)
        {
	    gg_debug(GG_DEBUG_MISC, "Aspell error: %s\n", aspell_error_message(possible_err));
            config_aspell = 0;
        }
        else
            spell_checker = to_aspell_speller(possible_err);
}
#endif
			    
			    

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
	ui_resize_term = 0;
	
#ifndef GG_DEBUG_DISABLE
	window_new(NULL, -1);
#endif
	window_current = window_new(NULL, 0);

	status = newwin(1, stdscr->_maxx + 1, stdscr->_maxy - 1, 0);
	input = newwin(1, stdscr->_maxx + 1, stdscr->_maxy, 0);
	keypad(input, TRUE);
	nodelay(input, TRUE);

	start_color();

	init_pair(7, COLOR_BLACK, background);	/* ma³e obej¶cie domy¶lnego koloru */
	init_pair(1, COLOR_RED, background);
	init_pair(2, COLOR_GREEN, background);
	init_pair(3, COLOR_YELLOW, background);
	init_pair(4, COLOR_BLUE, background);
	init_pair(5, COLOR_MAGENTA, background);
	init_pair(6, COLOR_CYAN, background);

#define __init_bg(x, y) \
	init_pair(x, COLOR_BLACK, y); \
	init_pair(x + 1, COLOR_RED, y); \
	init_pair(x + 2, COLOR_GREEN, y); \
	init_pair(x + 3, COLOR_YELLOW, y); \
	init_pair(x + 4, COLOR_BLUE, y); \
	init_pair(x + 5, COLOR_MAGENTA, y); \
	init_pair(x + 6, COLOR_CYAN, y); \
	init_pair(x + 7, COLOR_WHITE, y);

	__init_bg(8, COLOR_RED);
	__init_bg(16, COLOR_GREEN);
	__init_bg(24, COLOR_YELLOW);
	__init_bg(32, COLOR_BLUE);
	__init_bg(40, COLOR_MAGENTA);
	__init_bg(48, COLOR_CYAN);
	__init_bg(56, COLOR_WHITE);

#undef __init_bg

	contacts_changed();
	window_commit();

	/* deaktywujemy klawisze INTR, QUIT, SUSP i DSUSP */
	if (!tcgetattr(0, &old_tio)) {
		struct termios tio;

		memcpy(&tio, &old_tio, sizeof(tio));
		tio.c_cc[VINTR] = _POSIX_VDISABLE;
		tio.c_cc[VQUIT] = _POSIX_VDISABLE;
#ifdef VDSUSP
		tio.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
#ifdef VSUSP
		tio.c_cc[VSUSP] = _POSIX_VDISABLE;
#endif

		tcsetattr(0, TCSADRAIN, &tio);
	}

#ifdef SIGWINCH
	signal(SIGWINCH, sigwinch_handler);
#endif

	memset(history, 0, sizeof(history));

	timer_add(1, 1, TIMER_UI, 0, "ui-ncurses-time", "refresh_time");

	memset(binding_map, 0, sizeof(binding_map));
	memset(binding_map_meta, 0, sizeof(binding_map_meta));

	binding_default();

	ui_ncurses_inited = 1;
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

	if (!ui_ncurses_inited)
		return;

	if (done)
		return;

	done = 1;

	if (config_windows_save) {
		string_t s = string_init(NULL);
		int maxid = 0, i;
		
		xfree(config_windows_layout);

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (!w->floating && w->id > maxid)
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

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (w->floating && (!w->target || strncmp(w->target, "__", 2))) {
				char *tmp = saprintf("|*%d,%d,%d,%d,%d,%s", w->left, w->top, w->width, w->height, w->frames, w->target);
				string_append(s, tmp);
				xfree(tmp);
			}
		}

		config_windows_layout = string_free(s, 0);
	}

	for (l = windows; l; ) {
		struct window *w = l->data;

		l = l->next;

		window_kill(w, 1);
	}

	list_destroy(windows, 1);

	tcsetattr(0, TCSADRAIN, &old_tio);

	keypad(input, FALSE);

	werase(input);
	wnoutrefresh(input);
	doupdate();

	delwin(input);
	delwin(status);
	if (contacts)
		delwin(contacts);
	if (header)
		delwin(header);
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
#ifdef WITH_ASPELL
	xfree(aspell_line);
#endif
	xfree(yanked);

	if (getenv("TERM") && !strncmp(getenv("TERM"), "xterm", 5) && !getenv("EKG_NO_TITLE"))
		write(1, "\033]2;\007", 5);
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
	const char *words[] = { "close", "get", "send", "list", "resume", "rsend", "rvoice", "voice", NULL };
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

void ignorelevels_generator(const char *text, int len)
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

	for (i = 0; ignore_labels[i].name; i++)
		if (!strncasecmp(tmp, ignore_labels[i].name, len))
			array_add(&completions, ((tmp == text) ? xstrdup(ignore_labels[i].name) : saprintf("%s%s", pre, ignore_labels[i].name)));
}

void unknown_uin_generator(const char *text, int len)
{
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		if (send_nicks[i] && xisdigit(send_nicks[i][0]) && !strncasecmp(text, send_nicks[i], len))
			if (!array_contains(completions, send_nicks[i], 0))
				array_add(&completions, xstrdup(send_nicks[i]));
	}
}

void known_uin_generator(const char *text, int len)
{
	list_t l;
	int done = 0;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (u->display && u->uin && !strncasecmp(text, u->display, len)) {
			array_add(&completions, xstrdup(u->display));
			done = 1;
		}
	}

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (!done && u->uin && !strncasecmp(text, itoa(u->uin), len))
			array_add(&completions, xstrdup(itoa(u->uin)));
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
	char *dname, *tmp;
	const char *fname;
	int count, i;

	/* `dname' zawiera nazwê katalogu z koñcz±cym znakiem `/', albo
	 * NULL, je¶li w dope³nianym tek¶cie nie ma ¶cie¿ki. */

	dname = xstrdup(text);

	if ((tmp = strrchr(dname, '/'))) {
		tmp++;
		*tmp = 0;
	} else
		dname = NULL;

	/* `fname' zawiera nazwê szukanego pliku */

	fname = strrchr(text, '/');

	if (fname)
		fname++;
	else
		fname = text;

again:
	/* zbierzmy listê plików w ¿±danym katalogu */
	
	count = scandir((dname) ? dname : ".", &namelist, NULL, alphasort);

	ui_debug("dname=\"%s\", fname=\"%s\", count=%d\n", (dname) ? dname : "(null)", (fname) ? fname : "(null)", count);

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

		/* je¶li mamy `..', sprawd¼ czy katalog sk³ada siê z
		 * `../../../' lub czego¶ takiego. */
		
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

	/* je¶li w dope³nieniach wyl±dowa³ tylko jeden wpis i jest katalogiem
	 * to wejd¼ do niego i szukaj jeszcze raz */

	if (array_count(completions) == 1 && strlen(completions[0]) > 0 && completions[0][strlen(completions[0]) - 1] == '/') {
		xfree(dname);
		dname = xstrdup(completions[0]);
		fname = "";
		array_free(completions);
		completions = NULL;

		goto again;
	}

	xfree(dname);
	xfree(namelist);
}

void python_generator(const char *text, int len)
{
	const char *words[] = { "exec", "list", "load", "restart", "run", "unload", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

void window_generator(const char *text, int len)
{
	const char *words[] = { "new", "kill", "move", "next", "resize", "prev", "switch", "clear", "refresh", "list", "active", "last", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

void reason_generator(const char *text, int len)
{
	if (config_reason && !strncasecmp(text, config_reason, len)) {
		char *reason;
		/* brzydkie rozwi±zanie, ¿eby nie ruszaæ opisu przy dope³nianiu */
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
 * funkcja obs³uguj±ca dope³nianie klawiszem tab.
 */
static void complete(int *line_start, int *line_index)
{
	char *start, *cmd, **words, *separators;
	int i, count, word, j, words_count, word_current;
	
	start = xmalloc(strlen(line) + 1);
	
	/* 
	 * je¶li uzbierano ju¿ co¶ to próbujemy wy¶wietliæ wszystkie mo¿liwo¶ci 
	 */
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
				print("none", tmp);
			}
		}

		xfree(tmp);
		xfree(start);
		return;
	}
	
	/* zerujemy co mamy */
	words = NULL;

	/* podziel (uwzglêdniaj±c cudzys³owia)*/
	for (i = 0; i < strlen(line); i++) {
		if(line[i] == '"') 
			for(j = 0,  i++; i < strlen(line) && line[i] != '"'; i++, j++)
				start[j] = line[i];
		else
			for(j = 0; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; j++, i++) 
				start[j] = line[i];
		start[j] = '\0';
		/* "przewijamy" wiêksz± ilo¶æ spacji */
		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
		array_add(&words, saprintf("%s", start));
	}

	/* je¿eli ostatnie znaki to spacja, albo przecinek to trzeba dodaæ jeszcze pusty wyraz do words */
	if (strlen(line) > 1 && (line[strlen(line) - 1] == ' ' || line[strlen(line) - 1] == ','))
		array_add(&words, xstrdup(""));

/*	 for(i = 0; i < array_count(words); i++)
		gg_debug(GG_DEBUG_MISC, "words[i = %d] = \"%s\"\n", i, words[i]);     */

	/* inicjujemy pamiêc dla separators */
	separators = xmalloc(array_count(words));
		
	/* sprawd¼, gdzie jeste¶my (uwzgêdniaj±c cudzys³owia) i dodaj separatory*/
	for (word = 0, i = 0; i < strlen(line); i++, word++) {
		if(line[i] == '"')  {
			for(j = 0, i++; i < strlen(line) && line[i] != '"'; j++, i++)
				start[j] = line[i];
		}
		else {
			for(j = 0; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; j++, i++) 
				start[j] = line[i];
		}
		/* "przewijamy */
		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		/* ustawiamy znak koñca */
		start[j] = '\0';
		/* je¿eli to koniec linii, to koñczymy t± zabawê */
		if(i >= strlen(line))
	    		break;
		/* obni¿amy licznik o 1, ¿eby wszystko by³o okey, po "przewijaniu" */
		i--;
		/* hmm, jeste¶my ju¿ na wyrazie wskazywany przez kursor ? */
                if(i >= *line_index)
            		break;
	}
	
	/* dodajmy separatory - pewne rzeczy podobne do pêtli powy¿ej */
	for (i = 0, j = 0; i < strlen(line); i++, j++) {
		if(line[i] == '"')  {
			for(i++; i < strlen(line) && line[i] != '"'; i++);
			if(i < strlen(line)) 
				separators[j] = line[i + 1];
		}
		else {
			for(; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; i++);
			separators[j] = line[i];
		}

		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
	}

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
	}

	/* trzeba pododawaæ trochê do liczników w spefycicznych (patrz warunki) sytuacjach */
	if((xisspace(line[strlen(line) - 1]) || line[strlen(line) - 1] == ',') && word + 1== array_count(words) -1 ) 
		word++;
	if(xisspace(line[strlen(line) - 1]) && words_count == word_current) 
		word_current++;
	if(xisspace(line[strlen(line) - 1])) 
		words_count++;
		
/*	gg_debug(GG_DEBUG_MISC, "word = %d\n", word);
	gg_debug(GG_DEBUG_MISC, "start = \"%s\"\n", start);   
	gg_debug(GG_DEBUG_MISC, "words_count = %d\n", words_count);	 
	
	 for(i = 0; i < strlen(separators); i++)
		gg_debug(GG_DEBUG_MISC, "separators[i = %d] = \"%c\"\n", i, separators[i]);   */
	
	cmd = saprintf("/%s ", (config_tab_command) ? config_tab_command : "chat");
	
	/* nietypowe dope³nienie nicków przy rozmowach */
	if (!strcmp(line, "") || (!strncasecmp(line, cmd, strlen(cmd)) && word == 2 && send_nicks_count > 0) || (!strcasecmp(line, cmd) && send_nicks_count > 0)) {
		if (send_nicks_index >= send_nicks_count)
			send_nicks_index = 0;

		if (send_nicks_count) {
			char *nick = send_nicks[send_nicks_index++];

			snprintf(line, LINE_MAXLEN, (strchr(nick, ' ')) ? "%s\"%s\" " : "%s%s ", cmd, nick);
		} else
			snprintf(line, LINE_MAXLEN, "%s", cmd);
		*line_start = 0;
		*line_index = strlen(line);

                array_free(completions);
                array_free(words);
		xfree(start);
		xfree(separators);
		xfree(cmd);
		return;
	}
	xfree(cmd);

	/* pocz±tek komendy? */
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
			if(strchr(params, 'u') && word_current != strlen(strchr(params, 'u'))) 
				goto problem;
					
			for (i = 0; generators[i].ch; i++) {
				if (generators[i].ch == params[word - 1] || strchr(params, 'u')) {
					int j;

					generators[i].generate(words[word], strlen(words[word]));

					for (j = 0; completions && completions[j]; j++) {
						string_t s;
						const char *p;

						if (!strchr(completions[j], '"') && !strchr(completions[j], '\\') && !strchr(completions[j], ' '))
							continue;
						
						s = string_init("\"");

						for (p = completions[j]; *p; p++) {
							if (!strchr("\"\\", *p))
								string_append_c(s, *p);
							else {
								string_append_c(s, '\\');
								string_append_c(s, *p);
							}
						}
						string_append_c(s, '\"');

						xfree(completions[j]);
						completions[j] = string_free(s, 0);
					}
					break;
				}
			} 

		}
	}
problem:
	
	count = array_count(completions);

	/* 
	 * je¶li jest tylko jedna mo¿lwio¶æ na dope³nienie to drukujemy co mamy, 
	 * ewentualnie bierzemy czê¶æ wyrazów w cudzys³owia ... 
	 * i uwa¿amy oczywi¶cie na \001 (patrz funkcje wy¿ej 
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
				}
				else
			    		strcat(line, completions[0]);
				*line_index = strlen(line) + 1;
			}
			else {
				if(strchr(words[i], ' '))
					strcat(line, saprintf("\"%s\"", words[i]));
				else
					strcat(line, words[i]);
			}
			if((i == array_count(words) - 1 && line[strlen(line) - 1] != ' ' ))
				strcat(line, " ");
			else if (line[strlen(line) - 1] != ' ') 
                                strcat(line, saprintf("%c", separators[i]));
		}
		array_free(completions);
		completions = NULL;
	}

	/* 
	 * gdy jest wiêcej mo¿liwo¶ci to robimy podobnie jak wy¿ej tyle, ¿e czasem
	 * trzeba u¿yæ cudzys³owia tylko z jednej storny, no i trzeba dope³niæ do pewnego miejsca
	 * w sumie proste rzeczy, ale jak widaæ jest trochê opcji ... 
	 */
	if (count > 1) {
		int common = 0;
		int tmp = 0;
		int quotes = 0;

		/* 
		 * mo¿e nie za ³adne programowanie, ale skuteczne i w sumie jedyne w 100% spe³niaj±ce	
	 	 * wymagania dope³niania (uwzglêdnianie cudzyws³owiów itp...)
		 */
		for(i=1, j = 0; ; i++, common++) { 
			for(j=1; j < count; j++) {
				if(completions[j][0] == '"') 
					quotes = 1;
				if(completions[j][0] == '"' && completions[0][0] != '"')
					tmp = strncasecmp(completions[0], completions[j] + 1, i); 
				else if(completions[0][0] == '"' && completions[j][0] != '"')
					tmp = strncasecmp(completions[0] + 1, completions[j], i); 
				else
					tmp = strncasecmp(completions[0], completions[j], i); 
				/* gg_debug(GG_DEBUG_MISC,"strncasecmp(\"%s\", \"%s\", %d) = %d\n", completions[0], completions[j], i, strncasecmp(completions[0], completions[j], i));  */
				if( tmp < 0 || ( tmp > 0 && tmp < i))
					break;
			}
			if( tmp < 0 || ( tmp > 0 && tmp < i))
				break;
		} 
		
		/* gg_debug(GG_DEBUG_MISC,"common :%d\n", common); */

		if (strlen(line) + common < LINE_MAXLEN) {
		
			line[0] = '\0';		
			for(i = 0; i < array_count(words); i++) {
				if(i == word) {
					if(quotes == 1 && completions[0][0] != '"') 
						strcat(line, "\"");
					if(completions[0][0] == '"')
						common++;
					if(completions[0][common - 1] == '"')
						common--;
					strncat(line, str_tolower(completions[0]), common);
					*line_index = strlen(line);
				}
				else {
					if(strrchr(words[i], ' '))
						strcat(line, saprintf("\"%s\"", words[i]));
					else
						strcat(line, words[i]);
				}
				if(separators[i]) {
					strcat(line, saprintf("%c", separators[i]));
				}
			}
		}
	}

	array_free(words);
	xfree(start);
	xfree(separators);
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
		strlcpy(line, "", LINE_MAXLEN);

		history[0] = line;

		line_start = 0;
		line_index = 0; 
		lines_start = 0;
		lines_index = 0;
	} else {
		lines = xmalloc(2 * sizeof(char*));
		lines[0] = xmalloc(LINE_MAXLEN);
		lines[1] = NULL;
		strlcpy(lines[0], line, LINE_MAXLEN);
		xfree(line);
		line = lines[0];
		history[0] = NULL;
		lines_start = 0;
		lines_index = 0;
	}
	
	window_resize();

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
 * print_char_underlined()
 *
 * wy¶wietla w danym okienku podkreslony znak, bior±c pod uwagê znaki ,,niewy¶wietlalne''.
 */
void print_char_underlined(WINDOW *w, int y, int x, unsigned char ch)
{
        wattrset(w, A_UNDERLINE);

        if (ch < 32) {
                wattrset(w, A_REVERSE | A_UNDERLINE);
                ch += 64;
        }

        if (ch >= 128 && ch < 160) {
                ch = '?';
                wattrset(w, A_REVERSE | A_UNDERLINE);
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

		command_exec(window_current->target, tmp, 0);
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
		strlcat(lines[lines_index - 1], lines[lines_index], LINE_MAXLEN);
		
		xfree(lines[lines_index]);

		for (i = lines_index; i < array_count(lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (array_count(lines) + 1) * sizeof(char*));

		lines_index--;
		lines_adjust();

		return;
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

		strlcat(line, lines[lines_index + 1], LINE_MAXLEN);

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
		strlcpy(lines[lines_index + 1], line + line_index, LINE_MAXLEN);
		line[line_index] = 0;
		
		line_index = 0;
		line_start = 0;
		lines_index++;

		lines_adjust();
	
		return;
	}
				
	command_exec(window_current->target, line, 0);

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

	xfree(yanked);

	p = line + line_index;
	while (p > line && xisspace(*(p-1))) {
		p--;
		eaten++;
	}
	if (p > line) {
		while (p > line && !xisspace(*(p-1))) {
			p--;
			eaten++;
		}
	}

	yanked = xmalloc(eaten + 1);
	strlcpy(yanked, p, eaten + 1);

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
			if (lines_index > 0) {
				lines_index--;
				lines_adjust();
				line_adjust();
			}
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
			if (lines_index < array_count(lines) - 1) {
				lines_index++;
				line_index = 0;
				line_start = 0;
			}
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
		strlcpy(line, history[history_index], LINE_MAXLEN);
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
		strlcpy(line, history[history_index], LINE_MAXLEN);
		line_adjust();
		if (history_index == 0) {
			if (history[0] != line) {
				xfree(history[0]);
				history[0] = line;
			}
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
	command_exec(window_current->target, tmp, 0);
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

static void binding_next_contacts_group(const char *arg)
{
	contacts_group_index++;
	contacts_update(NULL);
	window_commit();
}

static void binding_ui_ncurses_debug_toggle(const char *arg)
{
	if (ui_ncurses_debug++ > 2)
		ui_ncurses_debug = 0;

	update_statusbar(1);
}


#ifdef WITH_ASPELL

/* 
 * Funkcja sprawdzajaca pisownie
 */

static void spellcheck(char *what, char *where)
{
        char *word;             /* aktualny wyraz */
        register int i = 0;     /* licznik */
	register int j = 0;     /* licznik */
	int size;	/* zmienna tymczasowa */
	
        /* Sprawdzamy czy nie mamy doczynienia z 47 (wtedy nie sprawdzamy reszty ) */
        if(what[0] == 47 || what == NULL)
            return;       /* konczymy funkcje */
	    
	for(i = 0; what[i] != '\0' && what[i] != '\n' && what[i] != '\r'; i++)
	{
	    if((!isalpha_pl(what[i]) || i == 0 ) && what[i+1] != '\0' ) // separator/koniec lini/koniec stringu
	    {
		size = strlen(what) + 1;
        	word = xmalloc(size);
        	memset(word, 0, size); /* czyscimy pamiec */
		
		for(; what[i] != '\0' && what[i] != '\n' && what[i] != '\r'; i++)
		{
		    if(isalpha_pl(what[i])) /* szukamy jakiejs pierwszej literki */
			break; 
		}
		
		/* trochê poprawiona wydajno¶æ */
		if(what[i] == '\0' || what[i] == '\n' || what[i] == '\r')
		{
			i--;
			goto aspell_loop_end; /* 
					       * nie powinno siê u¿ywaæ goto, aczkolwiek s± du¿o szybsze
					       * ni¿ instrukcje warunkowe i w tym przypadku nie psuj± bardzo
					       * czytelno¶ci kodu
					       */
		}
		/* sprawdzanie czy nastêpny wyraz nie rozpoczyna adresu www */ 
		else if (what[i] == 'h' && what[i + 1] && what[i + 1] == 't' && what[i + 2] && what[i + 2] == 't' && what[i + 3] && what[i + 3] == 'p' && what[i + 4] && what[i + 4] == ':' && what[i + 5] && what[i + 5] == '/' && what[i + 6] && what[i + 6] == '/')
		{
			for(; what[i] != ' ' && what[i] != '\n' && what[i] != '\r' && what[i] != '\0'; i++);
			i--;
			goto aspell_loop_end;
		}
		
		/* sprawdzanie czy nastêpny wyraz nie rozpoczyna adresu ftp */ 
		else if (what[i] == 'f' && what[i + 1] && what[i + 1] == 't' && what[i + 2] && what[i + 2] == 'p' && what[i + 3] && what[i + 3] == ':' && what[i + 4] && what[i + 4] == '/' && what[i + 5] && what[i + 5] == '/')
		{
			for(; what[i] != ' ' && what[i] != '\n' && what[i] != '\r' && what[i] != '\0'; i++);
			i--;
			goto aspell_loop_end;
		}
		
		

		    
				
		/* wrzucamy aktualny wyraz do zmiennej word */		    
		for(j=0; what[i] != '\n' && what[i] != '\0' && isalpha_pl(what[i]); i++)
		{
			if(isalpha_pl(what[i]))
		 	{
		    		word[j]= what[i];
				j++;
		    	}
		    	else 
				break;
		}
		word[j] = '\0';
		if(i > 0)
		    i--;

/*		gg_debug(GG_DEBUG_MISC, "Word: %s\n", word);  */

		/* sprawdzamy pisownie tego wyrazu */
        	if(aspell_speller_check(spell_checker, word, strlen(word) ) == 0) /* jesli wyraz jest napisany blednie */
        	{
			for(j=strlen(word) - 1; j >= 0; j--)
				where[i - j] = ASPELLCHAR;
        	}
        	else /* jesli wyraz jest napisany poprawnie */
        	{
			for(j=strlen(word) - 1; j >= 0; j--)
				where[i - j] = ' ';
        	}
aspell_loop_end:
		xfree(word);
	    }	
	}
}

#endif

/*
 * ui_ncurses_loop()
 *
 * g³ówna pêtla interfejsu.
 */
static void ui_ncurses_loop()
{
#ifdef WITH_ASPELL
	int mispelling = 0; /* zmienna pomocnicza */
#endif	
	line = xmalloc(LINE_MAXLEN);
#ifdef WITH_ASPELL
	aspell_line = xmalloc(LINE_MAXLEN);
#endif
	strlcpy(line, "", LINE_MAXLEN);
	history[0] = line;

	for (;;) {
		struct binding *b = NULL;
		int ch;

		ekg_wait_for_key();

		if (ui_resize_term) {
			ui_resize_term = 0;
			endwin();
			refresh();
			keypad(input, TRUE);
			/* wywo³a wszystko, co potrzebne */
			header_statusbar_resize();
			changed_backlog_size("backlog_size");

			continue;
		}

		ch = ekg_getch(0);

		if (ch == -1)		/* dziwny b³±d? */
			continue;

		if (ch == -2)		/* python ka¿e ignorowaæ */
			continue;

		if (ch == 0)		/* Ctrl-Space, g³upie to */
			continue;

		if (ch == 27) {
			if ((ch = ekg_getch(27)) == -2)
				continue;

			b = binding_map_meta[ch];

			if (ch == 27)
				b = binding_map[27];

			/* je¶li dostali¶my \033O to albo mamy Alt-O, albo
			 * pokaleczone klawisze funkcyjne (\033OP do \033OS).
			 * ogólnie rzecz bior±c, nieciekawa sytuacja ;) */

			if (ch == 'O') {
				int tmp = ekg_getch(ch);

				if (tmp >= 'P' && tmp <= 'S')
					b = binding_map[KEY_F(tmp - 'P' + 1)];
				else if (tmp == 'H')
					b = binding_map[KEY_HOME];
				else if (tmp == 'F')
					b = binding_map[KEY_END];
				else if (tmp == 'M')
					continue;
				else
					ungetch(tmp);
			}

			if (b && b->action) {
				if (b->function)
					b->function(b->arg);
				else {
					char *tmp = saprintf("%s%s", ((b->action[0] == '/') ? "" : "/"), b->action);
					command_exec(window_current->target, tmp, 0);
					xfree(tmp);
				}
			} else {
				/* obs³uga Ctrl-F1 - Ctrl-F12 na FreeBSD */
				if (ch == '[') {
					ch = wgetch(input);

					if (ch == '4' && wgetch(input) == '~' && binding_map[KEY_END])
						binding_map[KEY_END]->function(NULL);

					if (ch == '1' && wgetch(input) == '~' && binding_map[KEY_HOME])
						binding_map[KEY_HOME]->function(NULL);

					if (ch >= 107 && ch <= 118)
						window_switch(ch - 106);
				}
			}
		} else {
			if ((b = binding_map[ch]) && b->action) {
				if (b->function)
					b->function(b->arg);
				else {
					char *tmp = saprintf("%s%s", ((b->action[0] == '/') ? "" : "/"), b->action);
					command_exec(window_current->target, tmp, 0);
					xfree(tmp);
				}
			} else if (ch < 255 && strlen(line) < LINE_MAXLEN - 1) {
					
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
		wattrset(input, color_pair(COLOR_WHITE, 0, COLOR_BLACK));

		if (lines) {
			int i;
			
			for (i = 0; i < 5; i++) {
				unsigned char *p;
				int j;

				if (!lines[lines_start + i])
					break;

				p = lines[lines_start + i];
				
#ifdef WITH_ASPELL
				/* maly cleanup */
				memset(aspell_line, 32, LINE_MAXLEN);
				if(line_start == 0) 
					mispelling = 0;
				    
				/* sprawdzamy pisownie */
				if(config_aspell == 1)
					spellcheck(p, aspell_line);

                                for (j = 0; j + line_start < strlen(p) && j < input->_maxx + 1; j++)
                                {
                                    
				    if(aspell_line[line_start + j] == ASPELLCHAR) /* jesli b³êdny to wy¶wietlamy podkre¶lony */
                                        print_char_underlined(input, i, j, p[line_start + j]);
                                    else /* jesli jest wszystko okey to wyswietlamy normalny */
				        print_char(input, i, j, p[j + line_start]);
				}
#else
                                for (j = 0; j + line_start < strlen(p) && j < input->_maxx + 1; j++)
                                        print_char(input, i, j, p[j + line_start]);
#endif
			}
			wmove(input, lines_index - lines_start, line_index - line_start);
		} else {
			int i;

			if (window_current->prompt)
				mvwaddstr(input, 0, 0, window_current->prompt);

#ifdef WITH_ASPELL			
			/* maly cleanup */
			memset(aspell_line, 32, LINE_MAXLEN);
			if(line_start == 0) 
				mispelling = 0;

			/* sprawdzamy pisownie */
			if(config_aspell == 1)
		    		spellcheck(line, aspell_line);

                        for (i = 0; i < input->_maxx + 1 - window_current->prompt_len && i < strlen(line) - line_start; i++)
                        {
				if(aspell_line[line_start + i] == ASPELLCHAR) /* jesli b³êdny to wy¶wietlamy podkre¶lony */
                                    print_char_underlined(input, 0, i + window_current->prompt_len, line[line_start + i]);
                                else /* jesli jest wszystko okey to wyswietlamy normalny */
                                    print_char(input, 0, i + window_current->prompt_len, line[line_start + i]);
			}
#else
                        for (i = 0; i < input->_maxx + 1 - window_current->prompt_len && i < strlen(line) - line_start; i++)
                                print_char(input, 0, i + window_current->prompt_len, line[line_start + i]);
#endif

			wattrset(input, color_pair(COLOR_BLACK, 1, COLOR_BLACK));
			if (line_start > 0)
				mvwaddch(input, 0, window_current->prompt_len, '<');
			if (strlen(line) - line_start > input->_maxx + 1 - window_current->prompt_len)
				mvwaddch(input, 0, input->_maxx, '>');
			wattrset(input, color_pair(COLOR_WHITE, 0, COLOR_BLACK));
			wmove(input, 0, line_index - line_start + window_current->prompt_len);
		}
		
		window_commit();
	}
}

static void window_next()
{
	struct window *next = NULL;
	int passed = 0;
	list_t l;

	for (l = windows; l; l = l->next) {
		if (l->data == window_current)
			passed = 1;

		if (passed && l->next) {
			struct window *w = l->next->data;

			if (!w->floating) {
				next = w;
				break;
			}
		}
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
		struct window *w = l->data;

		if (w->floating)
			continue;

		if (w == window_current && l != windows)
			break;

		prev = l->data;
	}

	if (!prev->id)
		for (l = windows; l; l = l->next) {
			struct window *w = l->data;

			if (!w->floating)
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

			if (w->floating)
				continue;

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

	window_resize();
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
	__action("next-contacts-group", binding_next_contacts_group);
	__action("ignore-query", binding_ignore_query);
	__action("ui-ncurses-debug-toggle", binding_ui_ncurses_debug_toggle);

#undef __action

	array_free(args);
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

		if (!strcasecmp(key + 4, "Backspace")) {
			b->key = xstrdup("Alt-Backspace");
			if (add) {
				binding_map_meta[KEY_BACKSPACE] = list_add(&bindings, b, sizeof(struct binding));
				binding_map_meta[127] = binding_map_meta[KEY_BACKSPACE];
			}
			return 0;
		}

		if (strlen(key) != 5)
			return -1;
	
		ch = xtoupper(key[4]);

		b->key = saprintf("Alt-%c", ch);

		if (add) {
			binding_map_meta[ch] = list_add(&bindings, b, sizeof(struct binding));
			if (xisalpha(ch))
				binding_map_meta[xtolower(ch)] = binding_map_meta[ch];
		}

		return 0;
	}

	if (!strncasecmp(key, "Ctrl-", 5)) {
		unsigned char ch;
		
		if (strlen(key) != 6)
			return -1;

		ch = xtoupper(key[5]);
		b->key = saprintf("Ctrl-%c", ch);

		if (add)
			binding_map[ch - 64] = list_add(&bindings, b, sizeof(struct binding));
		
		return 0;
	}

	if (xtoupper(key[0]) == 'F' && atoi(key + 1)) {
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
	binding_map[KEY_LL] = binding_map[KEY_END];
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

	if (!strcmp(event, "xterm_update") && getenv("TERM") && !strncmp(getenv("TERM"), "xterm", 5) && !getenv("EKG_NO_TITLE")) {
		char *tmp = saprintf("\033]2;ekg (%d)\007", config_uin);
		write(1, tmp, strlen(tmp));
		xfree(tmp);
	}

	if (!strcasecmp(event, "refresh_time"))
		goto cleanup;

        if (!strcasecmp(event, "check_mail"))
		check_mail();

	if (!strcasecmp(event, "commit"))
		window_commit();
		
	if (!strcasecmp(event, "printat")) {
		char *target = va_arg(ap, char*);
		int id = va_arg(ap, int), x = va_arg(ap, int), y = va_arg(ap, int);
		char *text = va_arg(ap, char*);
		struct window *w = NULL;

		if (target)
			w = window_find(target);

		if (id) {
			list_t l;

			for (l = windows; l; l = l->next) {
				struct window *v = l->data;

				if (v->id == id) {
					w = v;
					break;
				}
			}
		}

		if (w && text)
			window_printat(w->window, x, y, text, NULL, COLOR_WHITE, 0, COLOR_BLACK, 0);
	}

	if (!strcmp(event, "variable_changed")) {
		char *name = va_arg(ap, char*);

		if (!strcasecmp(name, "sort_windows") && config_sort_windows) {
			list_t l;
			int id = 2;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (w->floating)
					continue;
				
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
			
			window_resize();
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

		if (!strncasecmp(name, "contacts", 8))
			contacts_changed();
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

			if (!w->target || !p1 || strcasecmp(w->target, p1))
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
				if (!p2 || !p3)
					printq("not_enough_params", "bind");
				else
					binding_add(p2, p3, 0, quiet);
			} else if (match_arg(p1, 'd', "delete", 2)) {
				if (!p2)
					printq("not_enough_params", "bind");
				else
					binding_delete(p2, quiet);
			} else if (match_arg(p1, 'L', "list-default", 5)) {
				binding_list(quiet, p2, 1);
			} else {
				if (match_arg(p1, 'l', "list", 2))
					binding_list(quiet, p2, 0);
				else
					binding_list(quiet, p1, 0);
			}

			goto cleanup;
		}

		if (!strcasecmp(command, "find")) {
			char *tmp = NULL;
			
			if (window_current->target) {
				struct userlist *u = userlist_find(0, window_current->target);
				struct conference *c = conference_find(window_current->target);
				int uin;

				if (u && u->uin)
					tmp = saprintf("/find %d", u->uin);

				if (c && c->name)
					tmp = saprintf("/conference --find %s", c->name);

				if (!u && !c && (uin = atoi(window_current->target)))
					tmp = saprintf("/find %d", uin);
			}

			if (!tmp)
				tmp = saprintf("/find %d", config_uin);

			command_exec(window_current->target, tmp, 0);

			xfree(tmp);

			goto cleanup;
		}

		if (!strcasecmp(command, "query-current")) {
			int *param = va_arg(ap, uin_t*);

			if (window_current->target)
				*param = get_uin(window_current->target);
			else
				*param = 0;

			goto cleanup;
		}

		if (!strcasecmp(command, "query-nicks")) {
			char ***param = va_arg(ap, char***);
			list_t l;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (w->floating || !w->target)
					continue;

				array_add(param, xstrdup(w->target));
			}

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

				if (config_make_window == 1) {
					list_t l;

					for (l = windows; l; l = l->next) {
						struct window *v = l->data;
	
						if (v->id < 2 || v->floating || v->target)
							continue;

						w = v;
						break;
					}

					if (!w)
						w = window_new(param, 0);

					window_switch(w->id);
				}

				if (config_make_window == 2) {
					w = window_new(param, 0);
					window_switch(w->id);
				}

				xfree(window_current->target);
				xfree(window_current->prompt);
				window_current->target = xstrdup(param);
				window_current->prompt = format_string(format_find("ncurses_prompt_query"), param);
				window_current->prompt_len = strlen(window_current->prompt);

				if (!quiet) {
					print_window(param, 0, "query_started", param);
					print_window(param, 0, "query_started_window", param);
				}
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

					if (w->id) {
						if (w->target) {
							if (!w->floating)	
								printq("window_list_query", itoa(w->id), w->target);
							else
								printq("window_list_floating", itoa(w->id), itoa(w->left), itoa(w->top), itoa(w->width), itoa(w->height), w->target);
						} else
							printq("window_list_nothing", itoa(w->id));
					}
				}

				goto cleanup;
			}

			if (!strcasecmp(p1, "active")) {
				list_t l;
				int id = 0;
		
				for (l = windows; l; l = l->next) {
					struct window *w = l->data;

					if (w->act && !w->floating && w->id) {
						id = w->id;
						break;
					}
				}

				if (id)
					window_switch(id);
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

			if (!strcasecmp(p1, "last")) {
				window_switch(window_last_id);
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
					w->top = stdscr->_maxy + 1 - w->height;
				
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
				window_current->more = 0;

				goto cleanup;
			}
			
			printq("invalid_params", "window");
		}
	}

cleanup:
	va_end(ap);

	contacts_update(NULL);
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

	window_resize();

	wresize(status, config_statusbar_size, stdscr->_maxx + 1);
	mvwin(status, stdscr->_maxy + 1 - input_size - config_statusbar_size, 0);

	update_statusbar(0);

	window_commit();
}

/*
 * binding_default()
 *
 * ustawia lub przywraca domy¶lne ustawienia przypisanych klawiszy.
 */
static void binding_default()
{
#ifndef GG_DEBUG_DISABLE
	binding_add("Alt-`", "/window switch 0", 1, 1);
#endif
	binding_add("Alt-1", "/window switch 1", 1, 1);
	binding_add("Alt-2", "/window switch 2", 1, 1);
	binding_add("Alt-3", "/window switch 3", 1, 1);
	binding_add("Alt-4", "/window switch 4", 1, 1);
	binding_add("Alt-5", "/window switch 5", 1, 1);
	binding_add("Alt-6", "/window switch 6", 1, 1);
	binding_add("Alt-7", "/window switch 7", 1, 1);
	binding_add("Alt-8", "/window switch 8", 1, 1);
	binding_add("Alt-9", "/window switch 9", 1, 1);
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
	binding_add("Alt-A", "/window active", 1, 1);
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
	binding_add("F4", "next-contacts-group", 1, 1);
#ifndef GG_DEBUG_DISABLE
	binding_add("F12", "/window switch 0", 1, 1);
#endif
	binding_add("F11", "ui-ncurses-debug-toggle", 1, 1);
}
