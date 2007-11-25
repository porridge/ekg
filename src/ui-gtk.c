#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "commands.h"
#include "stuff.h"
#include "userlist.h"
#include "xmalloc.h"
#include "themes.h"
#include "ui.h"


/*
#include "xtext.h"
#include "gtkutil.h"
#include "menu.h"
#include "bindings.h"
#include "userlistgui.h"
*/

#include "ui-gtk.h"
#include "ui-gtk-chanview.h"
#include "ui-gtk-xtext.h"
#include "ui-gtk-palette.h"

static int ui_gtk_inited = 0;	/* czy zainicjowano? */
struct window *window_current;	/* wskaźnik na aktualne okno */
struct window *window_status;
list_t windows = NULL;		/* lista okien */

char *history[HISTORY_MAX];	/* zapamiętane linie */
int history_index = 0;		/* offset w historii */

/* two different types of tabs */
#define TAG_IRC 0		/* server, channel, dialog */
#define TAG_UTIL 1		/* dcc, notify, chanlist */

#define GUI_SPACING (3)
#define GUI_BORDER (0)
#define SCROLLBAR_SPACING (2)

/* vars */
const char font_normal_config[] = "Monospace 9";
int mainwindow_width_config	= 640;
int mainwindow_height_config	= 400;
int gui_tweaks_config		= 0;
int tab_small_config		= 0;
int tab_layout_config		= 2;
int tint_red_config		= 195;
int tint_green_config		= 195;
int tint_blue_config		= 195;
int wordwrap_config		= 1;
int indent_nicks_config		= 1;
int show_separator_config	= 1;
int max_auto_indent_config	= 255;
int gui_ulist_pos_config	= 3;
int tab_pos_config		= 6;
int show_marker_config		= 1;
int thin_separator_config	= 1;

int gui_pane_left_size_config	= 100;
int gui_pane_right_size_config	= 100;
#define hidemenu_config 0
#define topicbar_config 1
#define newtabstofront_config 2

/* ekg2-core var */
#define config_timestamp_show 1

/* forward */
void fe_set_tab_color(window_t *sess, int col);
void mg_switch_page(int relative, int num);


/* BUGS/ FEATURES NOT IMPLEMENTED,
 * 	ncurses window_printat() 
 * 	ncurses /window dump
 */

/************************************************************************************
 *                    FUNCTIONS COPIED FROM UI-NCURSES
 ************************************************************************************/

/*
 * window_switch()
 *
 * przełącza do podanego okna.
 */
static void window_switch(int id)
{
	static int lock = 0;

	list_t l;

	if (lock) return;

	lock = 1;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (id != w->id)
			continue;

		window_current = w;

		w->act = 0;
		
		mg_switch_page(0, w->id);
		fe_set_tab_color(w, 0 /* w->act */);
		break;
	}

	lock = 0;
}

/*
 * window_find()
 *
 * szuka okna o podanym celu. zwraca strukturę opisującą je.
 */
static struct window *window_find(const char *target)
{
	list_t l;
	int current = ((target) ? !strcasecmp(target, "$") || !strcasecmp(target, "__current") : 0);
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

			next = w;
			break;
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

		if (w == window_current && l != windows)
			break;

		prev = l->data;
	}

	if (!prev->id) {
		for (l = windows; l; l = l->next, prev = l->data);
	}

	window_switch(prev->id);
}

void gtk_window_clear(struct window *w)
{
	gtk_xtext_clear(gtk_private(window_current)->buffer);
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
	w.last_act_time = time(NULL);

	if (target) {
		if (*target == '+' && !u) {
			w.doodle = 1;
			w.target = xstrdup(target + 1);
		}
		
		if (*target == '*' && !u) {
			const char *tmp = strchr(target, '/');
			char **argv, **arg;
			
			if (!tmp)
				tmp = "";
				
			w.target = xstrdup(tmp);

			argv = arg = array_make(target + 1, ",", 5, 0, 0);

			if (*arg && *arg[0] != '/')
				w.frames = atoi(*arg);

			array_free(argv);

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

	result = list_add_sorted(&windows, &w, sizeof(w), window_new_compare);

	/* w.window */
	{
		int tab = TRUE;

		/* tab == new_window_in_tab */

		mg_changui_new(result, tab, 0);
	}

	return result;
}

/*
 * window_kill()
 *
 * usuwa podane okno.
 */
static void window_kill(struct window *w, int quiet)
{
	int id = w->id;

	if (quiet) 
		goto cleanup;

	if (id == 1 && w->target) {
		printq("query_finished", window_current->target);
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
	if (w == window_current)
		window_current = NULL;

	xfree(w->target);
	fe_close_window(w);

	list_remove(&windows, w, 1);
}

/**************************************************************************************************/

/*
 * ui_gtk_beep()
 *
 * ostentacyjnie wzywa użytkownika do reakcji.
 */
static void ui_gtk_beep()
{
#warning "XXX"
/* XXX,
 * 	config_beep_title? 
 * 		- zrobic miganie w trayu?
 * 		- miganie okienka?
 */
	gdk_beep();
}

/*
 * ui_gtk_loop()
 *
 * główna pętla interfejsu.
 */
static void ui_gtk_loop()
{
#define ui_quit 0
	do {
		ekg_wait_for_key();

		while (gtk_events_pending()) {
			gtk_main_iteration();
		}
	} while (ui_quit == 0);
}

static int ui_gtk_event_command(const char *command, int quiet, va_list ap) {
	if (!strcasecmp(command, "window")) {
		char *p1 = va_arg(ap, char*);
		char *p2 = va_arg(ap, char*);

		if (!p1 || !strcasecmp(p1, "list")) {
			int num = p2 ? atoi(p2) : 0;
			list_t l;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (w->id && (!p2 || w->id == num)) {
					if (w->target) {
#warning "Tutaj, wyswietlic gdy okno jest toplevel, lub odlaczone"
						printq("window_list_query", itoa(w->id), w->target);
					} else
						printq("window_list_nothing", itoa(w->id));
				}
			}
			return 0;
		}

		if (!strcasecmp(p1, "active")) {
			list_t l;
			int id = 0;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (w->act && w->id) {
					id = w->id;
					break;
				}
			}

			if (id)
				window_switch(id);
			return 0;
		}

		if (!strcasecmp(p1, "new")) {
			struct window *w = window_new(p2, 0);
			window_switch(w->id);

			return 0;
		}

		if (!strcasecmp(p1, "switch")) {
			if (!p2) {
				printq("not_enough_params", "window");
				return -1;
			}
			window_switch(atoi(p2));

			return 0;
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
					return -1;
				}
			}

			window_kill(w, 0);
			return 0;
		}

		if (!strcasecmp(p1, "next")) {
			window_next();
			return 0;
		}

		if (!strcasecmp(p1, "prev")) {
			window_prev();
			return 0;
		}
			
		if (!strcasecmp(p1, "clear")) {
			gtk_window_clear(window_current);
			return 0;
		}
			

		printq("invalid_params", "window");
		return -1;
	}

	if (!strcasecmp(command, "query")) {
		char *param = va_arg(ap, char*);

		if (!param && !window_current->target)
			return -1;

		if (param) {
			struct window *w;

			if ((w = window_find(param))) {
				window_switch(w->id);
				return -1;
			}

			if ((config_make_window & 3) == 1) {
				list_t l;

				for (l = windows; l; l = l->next) {
					struct window *v = l->data;

					if (v->id < 2 || v->target)
						continue;

					w = v;
					break;
				}

				if (!w)
					w = window_new(param, 0);

				window_switch(w->id);
			}

			if ((config_make_window & 3) == 2) {
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

		return -1;
	}

	return 0;
}

static int ui_gtk_event_variable_changed(const char *name, va_list ap) {
#if 0
	if (!strncasecmp(name, "contacts", 8))
		contacts_changed();

#endif
	if (!strcmp(name, "uin")) {
		/* XXX, for each window */
		gtk_label_set_label(GTK_LABEL(gtk_private_ui(window_status)->nick_label), itoa(config_uin));
	}

	return 0;
}

extern gboolean mg_populate_userlist(window_t *sess);
extern gboolean fe_userlist_rehash(struct window *sess, void *data);

static int ui_gtk_event(const char *event, ...)
{
	va_list ap;

	if (!event)
		return 0;

	va_start(ap, event);

	if (!strcmp(event, "command")) {
		int quiet = va_arg(ap, int);
		char *command = va_arg(ap, char*);

		ui_gtk_event_command(command, quiet, ap);

	} else if (!strcmp(event, "variable_changed")) {
		char *name = va_arg(ap, char*);
		
		ui_gtk_event_variable_changed(name, ap);

	} else if (!strcmp(event, "status")) {		/* uin, display, status, descr */
		uin_t uin = va_arg(ap, uin_t);

		struct userlist *u = userlist_find(uin, NULL);

		if (u && u->display)
			ui_gtk_foreach_window_data(fe_userlist_rehash, u);

	} else if (!strcmp(event, "userlist_changed")) {	/* nick, uin, NULL */
		/* XXX,
		 * 	w handle_userlist()::GG_USERLIST_GET_REPLY
		 * 	nie informujemy o tym ze userow kasujemy.
		 * 	Tylko dodajemy nowych. W zwiazku z tym trzeba zrobic przebudowanie calej userlist
		 *
		 * 	W sumie nic strasznego, bo nawet taki /del * jest (przynajmniej u mnie) atomowy.
		 */
		ui_gtk_foreach_window(mg_populate_userlist);
	}

	va_end(ap);

	return 0;
}

/*
 * ui_gtk_postinit()
 *
 * uruchamiana po wczytaniu konfiguracji.
 */

static void ui_gtk_postinit()
{
#if 0
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
#endif
	mg_apply_setup();

	mg_switch_page(FALSE, window_current->id);

	ui_gtk_foreach_window(mg_populate_userlist);
}

void ui_gtk_foreach_window_data(int (*func)(struct window *, void *data), void *data) {
	list_t l;
	int once = 0;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (w->gui->is_tab) {
			if (!once) once = 1;
			else continue;
		}

		if (func(w, data))
			return;
	}
}

void ui_gtk_foreach_window(int (*func)(struct window *)) {
	list_t l;
	int once = 0;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (w->gui->is_tab) {
			if (!once) once = 1;
			else continue;
		}

		if (func(w))
			return;
	}
}

/* function copied from ui-ncurses */
static void ui_gtk_print(const char *target, int separate, const char *line)
{
	struct window *w;
	fstring_t fs;
	list_t l;
	char *lines, *lines_save, *line2;
	string_t speech = NULL;
	time_t cur_time;

	switch (config_make_window & 3) {
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
					if (get_uin(target) && !(ignored_check(get_uin(target)) & IGNORE_EVENTS))
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
					if (get_uin(target) && !(ignored_check(get_uin(target)) & IGNORE_EVENTS))
						event_check(EVENT_QUERY, get_uin(target), target);
				}
			}

crap:
			if (!config_display_crap && target && !strcmp(target, "__current"))
				w = window_find("__status");
			
			break;
			
		default:
			/* jeśli nie ma okna, rzuć do statusowego. */
			if (!(w = window_find(target)))
				w = window_find("__status");
	}

	/* albo zaczynamy, albo kończymy i nie ma okienka żadnego */
	if (!w) 
		return;

	cur_time = time(NULL);

	if (config_speech_app)
		speech = string_init(NULL);
	
	if (config_display_daychanges) {
		int day_win, day_cur;

		day_win = localtime(&w->last_act_time)->tm_yday;
		day_cur = localtime(&cur_time)->tm_yday;

		if (cur_time > w->last_act_time && day_win != day_cur) {
			struct tm *tm;
			char *tmp, *fmt, *tmp2;
			char str_win[12], str_cur[12];

			fmt = (config_datestamp) ? config_datestamp : "%Y-%m-%d";

			tm = localtime(&w->last_act_time);
			strftime (str_win, sizeof(str_win), fmt, tm);

			tm = localtime(&cur_time);
			strftime (str_cur, sizeof(str_cur), fmt, tm);

			tmp = format_string(format_find("window_day_changed"), 
			    str_win, str_cur);

			if ((tmp2 = strchr(tmp, '\n')))
				*tmp2 = 0;

			fs = reformat_string(tmp);
			fs->ts = cur_time;

			if (config_speech_app) {
				string_append(speech, fs->str);
				string_append_c(speech, '\n');
			}

			gtk_xtext_append_fstring(w->buffer, fs);
			xfree(tmp);
		}
	}

	w->last_act_time = cur_time;
	if (w != window_current) {
		if (!w->act) {
			w->act = 1;
			w->first_act_time = w->last_act_time;
		}
	}

	/* XXX wyrzucić dzielenie na linie z ui do ekg */
	lines = lines_save = xstrdup(line);
	while ((line2 = gg_get_line(&lines))) {
		fs = reformat_string(line2);
		fs->ts = cur_time;
		if (config_speech_app) {
			string_append(speech, fs->str);
			string_append_c(speech, '\n');
		}
		gtk_xtext_append_fstring(w->buffer, fs);
	}
	xfree(lines_save);

	if (config_speech_app && w->id)
		say_it(speech->str);

	string_free(speech, 1);

}

static void ui_gtk_deinit()
{
	list_t l;
	int i;

	if (!ui_gtk_inited)
		return;

	ui_gtk_inited = 0;

	for (l = windows; l; ) {
		struct window *w = l->data;

		l = l->next;

		window_kill(w, 1);
	}

	list_destroy(windows, 1);

	for (i = 0; i < HISTORY_MAX; i++) {
		xfree(history[i]);
		history[i] = NULL;
	}
}

void gtk_window_switch(int id)				{ window_switch(id); }
void gtk_window_kill(struct window *w, int quiet)	{ window_kill(w, quiet); }

void ui_gtk_init()
{
	const char no_display[] = "Zmienna $DISPLAY nie jest ustawiona\nInicjalizacja gtk napewno niemozliwa...\n";

	if (!getenv("DISPLAY")) {
		fprintf(stderr, no_display);
		/* fallback na inne ui? */
		return;
	}

	if (!(gtk_init_check(0, NULL))) {
		/* fallback na inne ui? */
		return;
	}

#ifndef GG_DEBUG_DISABLE
	window_new(NULL, -1);
#endif
	window_current = window_status = window_new(NULL, 0);
	ui_gtk_inited = 1;

	ui_loop = ui_gtk_loop;
	ui_beep = ui_gtk_beep;

	ui_postinit = ui_gtk_postinit;
	ui_event    = ui_gtk_event;
	ui_print    = ui_gtk_print;
	ui_deinit   = ui_gtk_deinit;

	pixmaps_init();
}
