#ifndef UI_GTK_H
#define UI_GTK_H

#include <gtk/gtkwidget.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtksignal.h>

typedef struct {
	GtkWidget
	 *xtext, *vscrollbar, *window,	/* toplevel */
	 *topic_entry, *note_book, *main_table, *user_tree,	/* GtkTreeView */
	 *user_box,		/* userlist box */
	 *dialogbutton_box, *topicbutton_box, 
	 *topic_bar, *hpane_left, *hpane_right, *vpane_left, *vpane_right, *menu, *bar,	/* connecting progress bar */
	 *nick_box,		/* contains label to the left of input_box */
	 *nick_label, *op_xpm,	/* icon to the left of nickname */
	 *namelistinfo,		/* label above userlist */
	 *input_box;

#define MENU_ID_NUM 12
	GtkWidget *menu_item[MENU_ID_NUM + 1];	/* some items we may change state of */

	void *chanview;		/* chanview.h */

	int pane_left_size;	/*last position of the pane */
	int pane_right_size;

	guint16 is_tab;		/* is tab or toplevel? */
	guint16 ul_hidden;	/* userlist hidden? */
} gtk_window_ui_t;

typedef struct window {
	int doodle;		/* czy do gryzmolenia? */
	int frames;		/* informacje o ramkach */
	int edge;		/* okienko brzegowe */
	int last_update;	/* czas ostatniego uaktualnienia */
	int nowrap;		/* nie zawijamy linii */
	int hide;		/* ukrywamy, bo jest zbyt duże */
	
	char *target;		/* nick query albo inna nazwa albo NULL */
	
	int id;			/* numer okna */
	int act;		/* czy coś się zmieniło? */
	time_t first_act_time;	/* timestamp zmiany act */
	time_t last_act_time;	/* timestamp ostatniej aktywności */

	char *prompt;		/* sformatowany prompt lub NULL */
	int prompt_len;		/* długość prompta lub 0 */

	int margin_left, margin_right, margin_top, margin_bottom;
				/* marginesy */

/* gtk window */
	gtk_window_ui_t *gui;

	void *tab;                      /* (chan *) */

	/* information stored when this tab isn't front-most */
	void *user_model;       /* for filling the GtkTreeView */
	void *buffer;           /* xtext_Buffer */
	gfloat old_ul_value;    /* old userlist value (for adj) */
} window_t;

enum {
	POS_INVALID = 0,
	POS_TOPLEFT = 1,
	POS_BOTTOMLEFT = 2,
	POS_TOPRIGHT = 3,
	POS_BOTTOMRIGHT = 4,
	POS_TOP = 5,		/* for tabs only */
	POS_BOTTOM = 6,
	POS_HIDDEN = 7
};

#define gtk_private(w) (w)
#define gtk_private_ui(w) (w->gui)

extern struct window *window_current;	/* wskaźnik na aktualne okno */
extern struct window *window_status;
extern list_t windows;			/* lista okien */

#define HISTORY_MAX 1000		/* maksymalna ilość wpisów historii */
extern char *history[HISTORY_MAX];	/* zapamiętane linie */
extern int history_index;		/* offset w historii */

extern const char font_normal_config[];
extern int mainwindow_width_config;
extern int mainwindow_height_config;
extern int gui_tweaks_config;
extern int tab_small_config;
extern int gui_pane_right_size_config;
extern int tab_layout_config;
extern int show_marker_config;
extern int tint_red_config;
extern int tint_green_config;
extern int tint_blue_config;
extern int wordwrap_config;
extern int indent_nicks_config;
extern int show_separator_config;
extern int max_auto_indent_config;

extern int gui_ulist_pos_config;
extern int tab_pos_config;

extern int gui_pane_left_size_config;
extern int gui_pane_right_size_config;
extern int thin_separator_config;
extern int new_window_in_tab_config;

#define mainwindow_left_config 0
#define mainwindow_top_config 0
#define chanmodebuttons_config -1

#define gui_win_state_config 0
#define hidemenu_config 0
#define topicbar_config 1
#define newtabstofront_config 2
#define gui_quit_dialog_config -1
#define truncchans_config 20
#define tab_sort_config 1
#define tab_icons_config 0
#define style_namelistgad_config 0
#define paned_userlist_config 0		/* XXX xchat def: 1 */
#define style_inputbox_config 0		/* XXX xchat commented def: 1 */

/* ekg2-core var */
#define config_timestamp_show 1

void ui_gtk_init(void);

void ui_gtk_window_switch(int id);
void ui_gtk_window_new(const char *target, int new_id);
void ui_gtk_window_kill(struct window *w, int quiet);
void ui_gtk_window_clear(struct window *w);
void ui_gtk_binding_init();

void ui_gtk_foreach_window(int (*func)(struct window *));
void ui_gtk_foreach_window_data(int (*func)(struct window *, void *data), void *data);

int key_handle_key_press(GtkWidget *wid, GdkEventKey *evt, window_t *sess);

#endif
