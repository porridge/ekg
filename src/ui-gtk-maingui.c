/* X-Chat
 * Copyright (C) 1998-2007 Peter Zelezny.
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
 *  port to ekg2 && ekg:
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

// #define GTK_DISABLE_DEPRECATED

#define USE_XLIB

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkclipboard.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkversion.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtktreemodel.h>

#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkversion.h>
#include <gdk/gdkkeysyms.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtksignal.h>

#include "libgadu.h"		/* potrzebne dla stalych ze stanami */

#include "comptime.h"
#include "commands.h"
#include "userlist.h"
#include "xmalloc.h"

#include "ui.h"
#include "ui-gtk.h"
#include "ui-gtk-xtext.h"
#include "ui-gtk-palette.h"
// #include "menu.h"
#include "ui-gtk-chanview.h"
// #include "ui-gtk-bindings.h"
// #include "ui-gtk-userlistgui.h"

/* extern */

void fe_userlist_numbers(window_t *sess);
void fe_userlist_insert(window_t *sess, struct userlist *u);

/* forward */
void mg_changui_new(window_t *sess, int tab, int focus);
void mg_open_quit_dialog(gboolean minimize_button);
static void mg_detach(window_t *sess, int mode);

#if 0

#include "../common/xchat.h"
#include "../common/fe.h"
#include "../common/server.h"
#include "../common/xchatc.h"
#include "../common/outbound.h"
#include "../common/inbound.h"
#include "../common/plugin.h"
#include "../common/modes.h"
#include "../common/url.h"
#include "fe-gtk.h"
#include "banlist.h"
#include "joind.h"
#include "maingui.h"
#include "pixmaps.h"
#include "plugin-tray.h"

#endif

struct window; /* forward */

#define GUI_SPACING (3)
#define GUI_BORDER (0)
#define SCROLLBAR_SPACING (2)

/* two different types of tabs */
#define TAG_IRC 0		/* server, channel, dialog */
#define TAG_UTIL 1		/* dcc, notify, chanlist */

static void mg_link_irctab(struct window *sess, int focus);
static void mg_create_entry(struct window *sess, GtkWidget *box);

static gtk_window_ui_t static_mg_gui;
static gtk_window_ui_t *mg_gui = NULL;	/* the shared irc tab */

GtkWidget *parent_window = NULL;	/* the master window */
GtkStyle *input_style;

static chan *active_tab = NULL;	/* active tab */

static PangoAttrList *away_list;
static PangoAttrList *newdata_list;
static PangoAttrList *nickseen_list;
static PangoAttrList *newmsg_list;
static PangoAttrList *plain_list = NULL;

enum {
	USERLIST_STATUS = 0,
	USERLIST_UIN,
	USERLIST_NICKNAME,
	USERLIST_DESCRIPTION,
	USERLIST_USER,
	USERLIST_COLOR,

	USERLIST_COLS
};

static int contacts_order[5] = { 0, 1, 2, 3, 4 /* -1 */ };

#define show_descr_in_userlist_config 0		/* XXX!!! */

/* REMOVED:
 * 	userlist_select() ->>  select a row in the userlist by nick-name 
 *      fe_uselect()
 *      fe_userlist_set_selected()
 */


/************************************************* MENU    *****************************/
static GSList *submenu_list;

enum {
	M_MENUITEM,
	M_NEWMENU,
	M_END,
	M_SEP,
	M_MENUTOG,
	M_MENURADIO,
	M_MENUSTOCK,
	M_MENUPIX,
	M_MENUSUB
};

struct mymenu {
	char *text;
	void *callback;
	char *image;
	unsigned char type;	/* M_XXX */
	unsigned char id;	/* MENU_ID_XXX (menu.h) */
	unsigned char state;	/* ticked or not? */
	unsigned char sensitive;	/* shaded out? */
	guint key;		/* GDK_x */
};

#define XCMENU_DOLIST 1
#define XCMENU_SHADED 1
#define XCMENU_MARKUP 2
#define XCMENU_MNEMONIC 4

#if 0

/* execute a userlistbutton/popupmenu command */

static void nick_command(session *sess, char *cmd) {
	if (*cmd == '!')
		xchat_exec(cmd + 1);
	else
		handle_command(sess, cmd, TRUE);
}

/* fill in the %a %s %n etc and execute the command */

void nick_command_parse(session *sess, char *cmd, char *nick, char *allnick) {
	char *buf;
	char *host = _("Host unknown");
	struct User *user;
	int len;

/*	if (sess->type == SESS_DIALOG)
	{
		buf = (char *)(GTK_ENTRY (sess->gui->topic_entry)->text);
		buf = strrchr (buf, '@');
		if (buf)
			host = buf + 1;
	} else*/
	{
		user = userlist_find(sess, nick);
		if (user && user->hostname)
			host = strchr(user->hostname, '@') + 1;
	}

	/* this can't overflow, since popup->cmd is only 256 */
	len = strlen(cmd) + strlen(nick) + strlen(allnick) + 512;
	buf = malloc(len);

	auto_insert(buf, len, cmd, 0, 0, allnick, sess->channel, "",
		    server_get_network(sess->server, TRUE), host, sess->server->nick, nick);

	nick_command(sess, buf);

	free(buf);
}

/* userlist button has been clicked */

void userlist_button_cb(GtkWidget *button, char *cmd) {
	int i, num_sel, using_allnicks = FALSE;
	char **nicks, *allnicks;
	char *nick = NULL;
	session *sess;

	sess = current_sess;

	if (strstr(cmd, "%a"))
		using_allnicks = TRUE;

	if (sess->type == SESS_DIALOG) {
		/* fake a selection */
		nicks = malloc(sizeof(char *) * 2);
		nicks[0] = g_strdup(sess->channel);
		nicks[1] = NULL;
		num_sel = 1;
	} else {
		/* find number of selected rows */
		nicks = userlist_selection_list(sess->gui->user_tree, &num_sel);
		if (num_sel < 1) {
			nick_command_parse(sess, cmd, "", "");
			return;
		}
	}

	/* create "allnicks" string */
	allnicks = malloc(((NICKLEN + 1) * num_sel) + 1);
	*allnicks = 0;

	i = 0;
	while (nicks[i]) {
		if (i > 0)
			strcat(allnicks, " ");
		strcat(allnicks, nicks[i]);

		if (!nick)
			nick = nicks[0];

		/* if not using "%a", execute the command once for each nickname */
		if (!using_allnicks)
			nick_command_parse(sess, cmd, nicks[i], "");

		i++;
	}

	if (using_allnicks) {
		if (!nick)
			nick = "";
		nick_command_parse(sess, cmd, nick, allnicks);
	}

	while (num_sel) {
		num_sel--;
		g_free(nicks[num_sel]);
	}

	free(nicks);
	free(allnicks);
}
#endif

/* a popup-menu-item has been selected */

static void popup_menu_cb(GtkWidget *item, char *cmd) {
	char *nick;

	/* the userdata is set in menu_quick_item() */
	nick = g_object_get_data(G_OBJECT(item), "u");
#if 0
	if (!nick) {		/* userlist popup menu */
		/* treat it just like a userlist button */
		userlist_button_cb(NULL, cmd);
		return;
	}

	if (!current_sess)	/* for url grabber window */
		nick_command_parse(sess_list->data, cmd, nick, nick);
	else
		nick_command_parse(current_sess, cmd, nick, nick);
#endif
}

#if 0

GtkWidget *menu_toggle_item(char *label, GtkWidget *menu, void *callback, void *userdata, int state) {
	GtkWidget *item;

	item = gtk_check_menu_item_new_with_label(label);
	gtk_check_menu_item_set_active((GtkCheckMenuItem *) item, state);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), userdata);
	gtk_widget_show(item);

	return item;
}

#endif

static GtkWidget *
menu_quick_item(char *cmd, char *label, GtkWidget *menu, int flags, gpointer userdata, char *icon)
{
	GtkWidget *img, *item;

	if (!label)
		item = gtk_menu_item_new();
	else {
		if (icon) {
			/*if (flags & XCMENU_MARKUP)
			   item = gtk_image_menu_item_new_with_markup (label);
			   else */
			item = gtk_image_menu_item_new_with_mnemonic(label);
			img = gtk_image_new_from_file(icon);
			if (img)
				gtk_image_menu_item_set_image((GtkImageMenuItem *) item, img);
			else {
				img = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
				if (img)
					gtk_image_menu_item_set_image((GtkImageMenuItem *) item,
								      img);
			}
		} else {
			if (flags & XCMENU_MARKUP) {
				item = gtk_menu_item_new_with_label("");
				if (flags & XCMENU_MNEMONIC)
					gtk_label_set_markup_with_mnemonic(GTK_LABEL
									   (GTK_BIN(item)->child),
									   label);
				else
					gtk_label_set_markup(GTK_LABEL(GTK_BIN(item)->child),
							     label);
			} else {
				if (flags & XCMENU_MNEMONIC)
					item = gtk_menu_item_new_with_mnemonic(label);
				else
					item = gtk_menu_item_new_with_label(label);
			}
		}
	}
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_set_data(G_OBJECT(item), "u", userdata);
	if (cmd)
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(popup_menu_cb), cmd);
	if (flags & XCMENU_SHADED)
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
	gtk_widget_show_all(item);

	return item;
}

static void menu_quick_item_with_callback(void *callback, char *label, GtkWidget *menu, void *arg) {
	GtkWidget *item;

	item = gtk_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), arg);
	gtk_widget_show(item);
}

static GtkWidget *menu_quick_sub(char *name, GtkWidget *menu, GtkWidget **sub_item_ret, int flags, int pos) {
	GtkWidget *sub_menu;
	GtkWidget *sub_item;

	if (!name)
		return menu;

	/* Code to add a submenu */
	sub_menu = gtk_menu_new();
	if (flags & XCMENU_MARKUP) {
		sub_item = gtk_menu_item_new_with_label("");
		gtk_label_set_markup(GTK_LABEL(GTK_BIN(sub_item)->child), name);
	} else {
		if (flags & XCMENU_MNEMONIC)
			sub_item = gtk_menu_item_new_with_mnemonic(name);
		else
			sub_item = gtk_menu_item_new_with_label(name);
	}
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), sub_item, pos);
	gtk_widget_show(sub_item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(sub_item), sub_menu);

	if (sub_item_ret)
		*sub_item_ret = sub_item;

	if (flags & XCMENU_DOLIST)
		/* We create a new element in the list */
		submenu_list = g_slist_prepend(submenu_list, sub_menu);
	return sub_menu;
}

static GtkWidget *menu_quick_endsub() {
	/* Just delete the first element in the linked list pointed to by first */
	if (submenu_list)
		submenu_list = g_slist_remove(submenu_list, submenu_list->data);

	if (submenu_list)
		return (submenu_list->data);
	else
		return NULL;
}

#if 0

static void
toggle_cb(GtkWidget *item, char *pref_name)
{
	char buf[256];

	if (GTK_CHECK_MENU_ITEM(item)->active)
		snprintf(buf, sizeof(buf), "set %s 1", pref_name);
	else
		snprintf(buf, sizeof(buf), "set %s 0", pref_name);

	handle_command(current_sess, buf, FALSE);
}

static int
is_in_path(char *cmd)
{
	char *prog = strdup(cmd + 1);	/* 1st char is "!" */
	char *space, *path, *orig;

	orig = prog;		/* save for free()ing */
	/* special-case these default entries. */
	/*                  123456789012345678 */
	if (strncmp(prog, "gnome-terminal -x ", 18) == 0)
		/* don't check for gnome-terminal, but the thing it's executing! */
		prog += 18;

	space = strchr(prog, ' ');	/* this isn't 100% but good enuf */
	if (space)
		*space = 0;

	path = g_find_program_in_path(prog);
	if (path) {
		g_free(path);
		g_free(orig);
		return 1;
	}

	g_free(orig);
	return 0;
}

/* append items to "menu" using the (struct popup*) list provided */

void
menu_create(GtkWidget *menu, GSList * list, char *target, int check_path)
{
	struct popup *pop;
	GtkWidget *tempmenu = menu, *subitem = NULL;
	int childcount = 0;

	submenu_list = g_slist_prepend(0, menu);
	while (list) {
		pop = (struct popup *)list->data;

		if (!strncasecmp(pop->name, "SUB", 3)) {
			childcount = 0;
			tempmenu = menu_quick_sub(pop->cmd, tempmenu, &subitem, XCMENU_DOLIST, -1);

		} else if (!strncasecmp(pop->name, "TOGGLE", 6)) {
			childcount++;
			menu_toggle_item(pop->name + 7, tempmenu, toggle_cb, pop->cmd,
					 cfg_get_bool(pop->cmd));

		} else if (!strncasecmp(pop->name, "ENDSUB", 6)) {
			/* empty sub menu due to no programs in PATH? */
			if (check_path && childcount < 1)
				gtk_widget_destroy(subitem);
			subitem = NULL;

			if (tempmenu != menu)
				tempmenu = menu_quick_endsub();
			/* If we get here and tempmenu equals menu that means we havent got any submenus to exit from */

		} else if (!strncasecmp(pop->name, "SEP", 3)) {
			menu_quick_item(0, 0, tempmenu, XCMENU_SHADED, 0, 0);

		} else {
			if (!check_path || pop->cmd[0] != '!') {
				menu_quick_item(pop->cmd, pop->name, tempmenu, 0, target, 0);
				/* check if the program is in path, if not, leave it out! */
			} else if (is_in_path(pop->cmd)) {
				childcount++;
				menu_quick_item(pop->cmd, pop->name, tempmenu, 0, target, 0);
			}
		}

		list = list->next;
	}

	/* Let's clean up the linked list from mem */
	while (submenu_list)
		submenu_list = g_slist_remove(submenu_list, submenu_list->data);
}
#endif

static void
menu_destroy(GtkWidget *menu, gpointer objtounref)
{
	gtk_widget_destroy(menu);
	g_object_unref(menu);
	if (objtounref)
		g_object_unref(G_OBJECT(objtounref));
}

static void
menu_popup(GtkWidget *menu, GdkEventButton * event, gpointer objtounref)
{
#if (GTK_MAJOR_VERSION != 2) || (GTK_MINOR_VERSION != 0)
	if (event && event->window)
		gtk_menu_set_screen(GTK_MENU(menu), gdk_drawable_get_screen(event->window));
#endif

	g_object_ref(menu);
	g_object_ref_sink(menu);
	g_object_unref(menu);
	g_signal_connect(G_OBJECT(menu), "selection-done", G_CALLBACK(menu_destroy), objtounref);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, event ? event->time : 0);
}

static char *str_copy = NULL;	/* for all pop-up menus */

void menu_nickmenu(window_t *sess, GdkEventButton *event, char *nick, int num_sel)
{
	char buf[512];
	GtkWidget *menu = gtk_menu_new();
	struct userlist *user;

	if (str_copy)
		free(str_copy);
	str_copy = strdup(nick);

	submenu_list = NULL;	/* first time through, might not be 0 */		/* [XXX] khem? z czym to sie je? */

	/* more than 1 nick selected? */
	if (num_sel > 1) {
		snprintf(buf, sizeof(buf), "Zaznaczyles: %d uzytkownikow.", num_sel);
		menu_quick_item(0, buf, menu, 0, 0, 0);
		menu_quick_item(0, 0, menu, XCMENU_SHADED, 0, 0);
	} else {
		user = userlist_find(0, nick);

#warning "userlist_find() + czlonkowie konferencji"
		/* XXX,
		 * 	jesli bedziemy tworzyc support dla konferencji w userliscie.
		 * 	to tutaj zrob:
		 * 		if (!user)
		 * 			user = .........
		 */

		if (user) {
			const char fmt[]	= "<tt><b>%-11s</b></tt> %s";
			const char fmtd[]	= "<tt><b>%-11s</b></tt> %d";

			GtkWidget *submenu = menu_quick_sub(nick, menu, NULL, XCMENU_DOLIST, -1);

			snprintf(buf, sizeof(buf), fmtd, "Numerek:", user->uin);
			menu_quick_item(0, buf, submenu, XCMENU_MARKUP, 0, 0);

		/* XXX, <separator> ? */

		/* XXX, stworzyc funkcje ktora to bedzie automatyzowac? */
		/* XXX, imie i nazwisko razem? */
			if (user->first_name) {
				char *real = g_markup_escape_text(user->first_name, -1);
				snprintf(buf, sizeof(buf), fmt, "Imie:", real);
				g_free(real);

				menu_quick_item(0, buf, submenu, XCMENU_MARKUP, 0, 0);
			}

			if (user->last_name) {
				char *real = g_markup_escape_text(user->last_name, -1);
				snprintf(buf, sizeof(buf), fmt, "Nazwisko:", real);
				g_free(real);

				menu_quick_item(0, buf, submenu, XCMENU_MARKUP, 0, 0);
			}

			if (user->mobile) {
				char *real = g_markup_escape_text(user->mobile, -1);
				snprintf(buf, sizeof(buf), fmt, "Telefon:", real);
				g_free(real);

				menu_quick_item(0, buf, submenu, XCMENU_MARKUP, 0, 0);
			}

			if (user->email) {
				char *real = g_markup_escape_text(user->email, -1);
				snprintf(buf, sizeof(buf), fmt, "Email:", real);
				g_free(real);

				menu_quick_item(0, buf, submenu, XCMENU_MARKUP, 0, 0);
			}

			/* ... */

			/* XXX, wyswietl u->groups, u->status, u->descr, u->ip:u->port, u->protocol (?) */
			/* u->last_seen, u->last_descr, u->last_ip:u->last_port, u->image_size (?) */
#if 0
			if (user->lasttalk) {
				char min[96];

				snprintf(min, sizeof(min), _("%u minutes ago"),
					 (unsigned int)((time(0) - user->lasttalk) / 60));
				snprintf(buf, sizeof(buf), fmt, _("Last Msg:"), min);

				menu_quick_item(0, buf, submenu, XCMENU_MARKUP, 0, 0);
			}
#endif
			menu_quick_endsub();
			menu_quick_item(0, 0, menu, XCMENU_SHADED, 0, 0);
		}
	}

#if 0
	if (num_sel > 1)
		menu_create(menu, popup_list, NULL, FALSE);
	else
		menu_create(menu, popup_list, str_copy, FALSE);
#endif
#if 0
	if (num_sel == 0)	/* xtext click */
		menu_add_plugin_items(menu, "\x5$NICK", str_copy);
	else			/* userlist treeview click */
		menu_add_plugin_items(menu, "\x5$NICK", NULL);
#endif
	menu_popup(menu, event, NULL);
}

#if 0

/* stuff for the View menu */

static void
menu_showhide_cb(session *sess)
{
	if (prefs.hidemenu)
		gtk_widget_hide(sess->gui->menu);
	else
		gtk_widget_show(sess->gui->menu);
}

static void
menu_topic_showhide_cb(session *sess)
{
	if (prefs.topicbar)
		gtk_widget_show(sess->gui->topic_bar);
	else
		gtk_widget_hide(sess->gui->topic_bar);
}

static void
menu_userlist_showhide_cb(session *sess)
{
	mg_decide_userlist(sess, TRUE);
}

static void
menu_ulbuttons_showhide_cb(session *sess)
{
	if (prefs.userlistbuttons)
		gtk_widget_show(sess->gui->button_box);
	else
		gtk_widget_hide(sess->gui->button_box);
}

static void
menu_cmbuttons_showhide_cb(session *sess)
{
	switch (sess->type) {
	case SESS_CHANNEL:
		if (prefs.chanmodebuttons)
			gtk_widget_show(sess->gui->topicbutton_box);
		else
			gtk_widget_hide(sess->gui->topicbutton_box);
		break;
	default:
		gtk_widget_hide(sess->gui->topicbutton_box);
	}
}

static void
menu_setting_foreach(void (*callback) (session *), int id, guint state)
{
	session *sess;
	GSList *list;
	int maindone = FALSE;	/* do it only once for EVERY tab */

	list = sess_list;
	while (list) {
		sess = list->data;

		if (!sess->gui->is_tab || !maindone) {
			if (sess->gui->is_tab)
				maindone = TRUE;
			if (id != -1)
				GTK_CHECK_MENU_ITEM(sess->gui->menu_item[id])->active = state;
			if (callback)
				callback(sess);
		}

		list = list->next;
	}
}

void
menu_bar_toggle(void)
{
	prefs.hidemenu = !prefs.hidemenu;
	menu_setting_foreach(menu_showhide_cb, MENU_ID_MENUBAR, !prefs.hidemenu);
}

static void
menu_bar_toggle_cb(void)
{
	menu_bar_toggle();
	if (prefs.hidemenu)
		fe_message(_("The Menubar is now hidden. You can show it again"
			     " by pressing F9 or right-clicking in a blank part of"
			     " the main text area."), FE_MSG_INFO);
}

static void
menu_topicbar_toggle(GtkWidget *wid, gpointer ud)
{
	prefs.topicbar = !prefs.topicbar;
	menu_setting_foreach(menu_topic_showhide_cb, MENU_ID_TOPICBAR, prefs.topicbar);
}

static void
menu_userlist_toggle(GtkWidget *wid, gpointer ud)
{
	prefs.hideuserlist = !prefs.hideuserlist;
	menu_setting_foreach(menu_userlist_showhide_cb, MENU_ID_USERLIST, !prefs.hideuserlist);
}

static void
menu_ulbuttons_toggle(GtkWidget *wid, gpointer ud)
{
	prefs.userlistbuttons = !prefs.userlistbuttons;
	menu_setting_foreach(menu_ulbuttons_showhide_cb, MENU_ID_ULBUTTONS, prefs.userlistbuttons);
}

static void
menu_cmbuttons_toggle(GtkWidget *wid, gpointer ud)
{
	prefs.chanmodebuttons = !prefs.chanmodebuttons;
	menu_setting_foreach(menu_cmbuttons_showhide_cb, MENU_ID_MODEBUTTONS,
			     prefs.chanmodebuttons);
}

void
menu_middlemenu(session *sess, GdkEventButton * event)
{
	GtkWidget *menu;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new();
	menu = menu_create_main(accel_group, FALSE, sess->server->is_away, !sess->gui->is_tab,
				NULL);
	menu_popup(menu, event, accel_group);
}

static void
open_url_cb(GtkWidget *item, char *url)
{
	char buf[512];

	/* pass this to /URL so it can handle irc:// */
	snprintf(buf, sizeof(buf), "URL %s", url);
	handle_command(current_sess, buf, FALSE);
}

static void
copy_to_clipboard_cb(GtkWidget *item, char *url)
{
	gtkutil_copy_to_clipboard(item, NULL, url);
}

void
menu_urlmenu(GdkEventButton * event, char *url)
{
	GtkWidget *menu;
	char *tmp, *chop;

	if (str_copy)
		free(str_copy);
	str_copy = strdup(url);

	menu = gtk_menu_new();
	/* more than 51 chars? Chop it */
	if (g_utf8_strlen(str_copy, -1) >= 52) {
		tmp = strdup(str_copy);
		chop = g_utf8_offset_to_pointer(tmp, 48);
		chop[0] = chop[1] = chop[2] = '.';
		chop[3] = 0;
		menu_quick_item(0, tmp, menu, XCMENU_SHADED, 0, 0);
		free(tmp);
	} else {
		menu_quick_item(0, str_copy, menu, XCMENU_SHADED, 0, 0);
	}
	menu_quick_item(0, 0, menu, XCMENU_SHADED, 0, 0);

	/* Two hardcoded entries */
	if (strncmp(str_copy, "irc://", 6) == 0 || strncmp(str_copy, "ircs://", 7) == 0)
		menu_quick_item_with_callback(open_url_cb, _("Connect"), menu, str_copy);
	else
		menu_quick_item_with_callback(open_url_cb, _("Open Link in Browser"), menu,
					      str_copy);
	menu_quick_item_with_callback(copy_to_clipboard_cb, _("Copy Selected Link"), menu,
				      str_copy);
	/* custom ones from urlhandlers.conf */
	menu_create(menu, urlhandler_list, str_copy, TRUE);
	menu_add_plugin_items(menu, "\x4$URL", str_copy);
	menu_popup(menu, event, NULL);
}

static void
menu_chan_join(GtkWidget *menu, char *chan)
{
	char tbuf[256];

	if (current_sess) {
		snprintf(tbuf, sizeof tbuf, "join %s", chan);
		handle_command(current_sess, tbuf, FALSE);
	}
}

#endif

static void menu_open_server_list(GtkWidget *wid, gpointer none)
{
	printf("menu_open_server_list() stub!\n");
#if 0
	fe_serverlist_open(current_sess);
#endif
}

static void menu_settings(GtkWidget *wid, gpointer none) {
#if 0
	extern void setup_open(void);
	setup_open();
#endif
	printf("menu_settings() stub! :)\n");
}

static void menu_about(GtkWidget *wid, gpointer sess) {
	GtkWidget *vbox, *label, *hbox;
	static GtkWidget *about = NULL;
	char buf[512];

	if (about) {
		gtk_window_present(GTK_WINDOW(about));
		return;
	}

	about = gtk_dialog_new();
	gtk_window_set_position(GTK_WINDOW (about), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW (about), FALSE);
	gtk_window_set_title(GTK_WINDOW(about), "O programie...");

	vbox = GTK_DIALOG(about)->vbox;

	wid = gtk_image_new_from_pixbuf(pix_ekg);
	gtk_container_add(GTK_CONTAINER(vbox), wid);

	label = gtk_label_new(NULL);
	gtk_label_set_selectable(GTK_LABEL (label), TRUE);
	gtk_container_add(GTK_CONTAINER(vbox), label);
	snprintf(buf, sizeof (buf), 
		"<span size=\"x-large\"><b>ekg 1.7</b></span>\n\n"
			"Eksperymentalny Klient Gadu-Gadu wersja 1.7\n"
			"<b>Skompilowano</b>: %s\n\n"
			"<small>gtk frontend based on xchat: \302\251 1998-2007 Peter \305\275elezn\303\275 &lt;zed@xchat.org></small>",
			compile_time());

	gtk_label_set_markup(GTK_LABEL(label), buf);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);

	hbox = gtk_hbox_new(0, 2);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	wid = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(wid), GTK_CAN_DEFAULT);
	gtk_dialog_add_action_widget(GTK_DIALOG(about), wid, GTK_RESPONSE_OK);
	gtk_widget_grab_default(wid);

	gtk_widget_show_all(about);

	gtk_dialog_run(GTK_DIALOG(about));

	gtk_widget_destroy(about);
	about = NULL;
}

#if 0

static void
menu_usermenu(void)
{
	editlist_gui_open(NULL, NULL, usermenu_list, _("XChat: User menu"),
			  "usermenu", "usermenu.conf", 0);
}

static void
usermenu_create(GtkWidget *menu)
{
	menu_create(menu, usermenu_list, "", FALSE);
	menu_quick_item(0, 0, menu, XCMENU_SHADED, 0, 0);	/* sep */
	menu_quick_item_with_callback(menu_usermenu, _("Edit This Menu..."), menu, 0);
}

static void
usermenu_destroy(GtkWidget *menu)
{
	GList *items = ((GtkMenuShell *) menu)->children;
	GList *next;

	while (items) {
		next = items->next;
		gtk_widget_destroy(items->data);
		items = next;
	}
}

void
usermenu_update(void)
{
	int done_main = FALSE;
	GSList *list = sess_list;
	session *sess;
	GtkWidget *menu;

	while (list) {
		sess = list->data;
		menu = sess->gui->menu_item[MENU_ID_USERMENU];
		if (sess->gui->is_tab) {
			if (!done_main && menu) {
				usermenu_destroy(menu);
				usermenu_create(menu);
				done_main = TRUE;
			}
		} else if (menu) {
			usermenu_destroy(menu);
			usermenu_create(menu);
		}
		list = list->next;
	}
}

#endif

static void menu_newchannel_window(GtkWidget *wid, gpointer none) {
	int old = new_window_in_tab_config;

	new_window_in_tab_config = 0;
	ui_gtk_window_new(NULL, 0);
	new_window_in_tab_config = old;
}

static void menu_newchannel_tab(GtkWidget *wid, gpointer none) {
	int old = new_window_in_tab_config;

	new_window_in_tab_config = 1;
	ui_gtk_window_new(NULL, 0);
	new_window_in_tab_config = old;
}

static void menu_detach(GtkWidget *wid, gpointer none) {
	mg_detach(window_current, 0);
}

static void menu_close(GtkWidget *wid, gpointer none) {
#warning "XXX, czy takie behaviour jest ok."
	if (window_current->id == 1)
		mg_open_quit_dialog(FALSE);
	else
		ui_gtk_window_kill(window_current, 0);
}

static void menu_quit(GtkWidget *wid, gpointer none) {
	mg_open_quit_dialog(FALSE);
}

#if 0
static void menu_search() {
	search_open(current_sess);
}

static void menu_rawlog(GtkWidget *wid, gpointer none) {
	open_rawlog(current_sess->server);
}
#endif

static void menu_resetmarker(GtkWidget *wid, gpointer none) {
	gtk_xtext_reset_marker_pos(GTK_XTEXT(window_current->gui->xtext));
}

static void menu_flushbuffer(GtkWidget *wid, gpointer none) {
	ui_gtk_window_clear(window_current);
}

#if 0

static void
savebuffer_req_done(session *sess, char *file)
{
	int fh;

	if (!file)
		return;

	fh = open(file, O_TRUNC | O_WRONLY | O_CREAT, 0600);
	if (fh != -1) {
		gtk_xtext_save(GTK_XTEXT(sess->gui->xtext), fh);
		close(fh);
	}
}

static void
menu_savebuffer(GtkWidget *wid, gpointer none)
{
	gtkutil_file_req(_("Select an output filename"), savebuffer_req_done,
			 current_sess, NULL, FRF_WRITE);
}

static void
menu_disconnect(GtkWidget *wid, gpointer none)
{
	handle_command(current_sess, "DISCON", FALSE);
}

static void
menu_reconnect(GtkWidget *wid, gpointer none)
{
	if (current_sess->server->hostname[0])
		handle_command(current_sess, "RECONNECT", FALSE);
	else
		fe_serverlist_open(current_sess);
}

static void
menu_join_cb(GtkWidget *dialog, gint response, GtkEntry * entry)
{
	switch (response) {
	case GTK_RESPONSE_ACCEPT:
		menu_chan_join(NULL, entry->text);
		break;

	case GTK_RESPONSE_HELP:
		chanlist_opengui(current_sess->server, TRUE);
		break;
	}

	gtk_widget_destroy(dialog);
}

static void
menu_join_entry_cb(GtkWidget *entry, GtkDialog * dialog)
{
	gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}

static void
menu_join(GtkWidget *wid, gpointer none)
{
	GtkWidget *hbox, *dialog, *entry, *label;

	dialog = gtk_dialog_new_with_buttons(_("Join Channel"),
					     GTK_WINDOW(parent_window), 0,
					     _("Retrieve channel list..."), GTK_RESPONSE_HELP,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_box_set_homogeneous(GTK_BOX(GTK_DIALOG(dialog)->vbox), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	hbox = gtk_hbox_new(TRUE, 0);

	entry = gtk_entry_new();
	GTK_ENTRY(entry)->editable = 0;	/* avoid auto-selection */
	gtk_entry_set_text(GTK_ENTRY(entry), "#");
	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(menu_join_entry_cb), dialog);
	gtk_box_pack_end(GTK_BOX(hbox), entry, 0, 0, 0);

	label = gtk_label_new(_("Enter Channel to Join:"));
	gtk_box_pack_end(GTK_BOX(hbox), label, 0, 0, 0);

	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(menu_join_cb), entry);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	gtk_widget_show_all(dialog);

	gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
	gtk_editable_set_position(GTK_EDITABLE(entry), 1);
}

static void
menu_away(GtkCheckMenuItem * item, gpointer none)
{
	handle_command(current_sess, item->active ? "away" : "back", FALSE);
}

static void
menu_chanlist(GtkWidget *wid, gpointer none)
{
	chanlist_opengui(current_sess->server, FALSE);
}

static void
menu_blocklist(GtkWidget *wid, gpointer none)
{
	banlist_opengui(current_sess);
}

static void
menu_rpopup(void)
{
	editlist_gui_open(_("Text"), _("Replace with"), replace_list, _("XChat: Replace"),
			  "replace", "replace.conf", 0);
}

static void
menu_urlhandlers(void)
{
	editlist_gui_open(NULL, NULL, urlhandler_list, _("XChat: URL Handlers"), "urlhandlers",
			  "urlhandlers.conf", url_help);
}

static void
menu_evtpopup(void)
{
	pevent_dialog_show();
}

static void
menu_dcc_win(GtkWidget *wid, gpointer none)
{
	fe_dcc_open_recv_win(FALSE);
	fe_dcc_open_send_win(FALSE);
}

static void
menu_dcc_chat_win(GtkWidget *wid, gpointer none)
{
	fe_dcc_open_chat_win(FALSE);
}

void
menu_change_layout(void)
{
	if (prefs.tab_layout == 0) {
		menu_setting_foreach(NULL, MENU_ID_LAYOUT_TABS, 1);
		menu_setting_foreach(NULL, MENU_ID_LAYOUT_TREE, 0);
		mg_change_layout(0);
	} else {
		menu_setting_foreach(NULL, MENU_ID_LAYOUT_TABS, 0);
		menu_setting_foreach(NULL, MENU_ID_LAYOUT_TREE, 1);
		mg_change_layout(2);
	}
}

static void
menu_layout_cb(GtkWidget *item, gpointer none)
{
	prefs.tab_layout = 2;
	if (GTK_CHECK_MENU_ITEM(item)->active)
		prefs.tab_layout = 0;

	menu_change_layout();
}

#endif

static GdkPixbuf *pix_book = NULL;	/* XXX */

static struct mymenu mymenu[] = {
	{"ekg", 0, 0, M_NEWMENU, 0, 0, 1},
		{ "Wybierz serwer", menu_open_server_list, (char *)&pix_book, M_MENUPIX, 0, 0, 1, GDK_s},
		{0, 0, 0, M_SEP, 0, 0, 0},
		{ "Nowe okno", 0, GTK_STOCK_NEW, M_MENUSUB, 0, 0, 1},
			{"W zakladce", menu_newchannel_tab, 0, M_MENUITEM, 0, 0, 1},
			{"Osobno", menu_newchannel_window, 0, M_MENUITEM, 0, 0, 1},
			{0, 0, 0, M_END, 0, 0, 0},
		{0, 0, 0, M_SEP, 0, 0, 0},

#ifdef WITH_PYTHON
#define menu_loadplugin NULL
		{ "Zaladuj skrypt", menu_loadplugin, GTK_STOCK_REVERT_TO_SAVED, M_MENUSTOCK, 0, 0, 1},
#else
		{ "Zaladuj skrypt", NULL, GTK_STOCK_REVERT_TO_SAVED, M_MENUSTOCK, 0, 0, 0},			/* XXX, powinno byc bardziej szare.. */
#endif
		{0, 0, 0, M_SEP, 0, 0, 0},	/* 9 */

#define DETACH_OFFSET (10)
		{0, menu_detach, GTK_STOCK_REDO, M_MENUSTOCK, 0, 0, 1, GDK_I},		/* 10 */
#define CLOSE_OFFSET (11)
		{0, menu_close, GTK_STOCK_CLOSE, M_MENUSTOCK, 0, 0, 1, GDK_w},		/* 11 */
		{0, 0, 0, M_SEP, 0, 0, 0},
		{"Wyjdz", menu_quit, GTK_STOCK_QUIT, M_MENUSTOCK, 0, 0, 1, GDK_q},	/* 13 */
	{"Widok", 0, 0, M_NEWMENU, 0, 0, 1},


#if 0
#define MENUBAR_OFFSET (17)
		{N_("_Menu Bar"), menu_bar_toggle_cb, 0, M_MENUTOG, MENU_ID_MENUBAR, 0, 1, GDK_F9},
		{N_("_Topic Bar"), menu_topicbar_toggle, 0, M_MENUTOG, MENU_ID_TOPICBAR, 0, 1},
		{N_("_User List"), menu_userlist_toggle, 0, M_MENUTOG, MENU_ID_USERLIST, 0, 1, GDK_F7},
		{N_("U_serlist Buttons"), menu_ulbuttons_toggle, 0, M_MENUTOG, MENU_ID_ULBUTTONS, 0, 1},
		{N_("M_ode Buttons"), menu_cmbuttons_toggle, 0, M_MENUTOG, MENU_ID_MODEBUTTONS, 0, 1},
		{0, 0, 0, M_SEP, 0, 0, 0},
		{N_("_Channel Switcher"), 0, 0, M_MENUSUB, 0, 0, 1},	/* 23 */
#define TABS_OFFSET (24)
			{N_("_Tabs"), menu_layout_cb, 0, M_MENURADIO, MENU_ID_LAYOUT_TABS, 0, 1},
			{N_("T_ree"), 0, 0, M_MENURADIO, MENU_ID_LAYOUT_TREE, 0, 1},
			{0, 0, 0, M_END, 0, 0, 0},
	{N_("_Server"), 0, 0, M_NEWMENU, 0, 0, 1},
		{N_("_Disconnect"), menu_disconnect, GTK_STOCK_DISCONNECT, M_MENUSTOCK, MENU_ID_DISCONNECT, 0, 1},
		{N_("_Reconnect"), menu_reconnect, GTK_STOCK_CONNECT, M_MENUSTOCK, MENU_ID_RECONNECT, 0, 1},
		{N_("Join Channel..."), menu_join, GTK_STOCK_JUMP_TO, M_MENUSTOCK, MENU_ID_JOIN, 0, 1},
		{0, 0, 0, M_SEP, 0, 0, 0},
#define AWAY_OFFSET (38)
		{N_("Marked Away"), menu_away, 0, M_MENUTOG, MENU_ID_AWAY, 0, 1, GDK_a},

		{N_("_Usermenu"), 0, 0, M_NEWMENU, MENU_ID_USERMENU, 0, 1},	/* 39 */

#endif

	{"Ustawienia", 0, 0, M_NEWMENU, 0, 0, 1},							/* S_ettings */
		{"Preferencje", menu_settings, GTK_STOCK_PREFERENCES, M_MENUSTOCK, 0, 0, 1},		/* _Preferences */
		{"Zaawansowane", 0, GTK_STOCK_JUSTIFY_LEFT, M_MENUSUB, 0, 0, 1},			/* Advanced */
#if 0
			{N_("Auto Replace..."), menu_rpopup, 0, M_MENUITEM, 0, 0, 1},
			{N_("CTCP Replies..."), menu_ctcpguiopen, 0, M_MENUITEM, 0, 0, 1},
			{N_("Text Events..."), menu_evtpopup, 0, M_MENUITEM, 0, 0, 1},
			{N_("URL Handlers..."), menu_urlhandlers, 0, M_MENUITEM, 0, 0, 1},
			{N_("Userlist Buttons..."), menu_ulbuttons, 0, M_MENUITEM, 0, 0, 1},
#endif
			{0, 0, 0, M_END, 0, 0, 0},	/* 52 */
	{"Okna", 0, 0, M_NEWMENU, 0, 0, 1},
#if 0
		{"Zablokowani", menu_blocklist, 0, M_MENUITEM, 0, 0, 1},
		{"Konferencje", menu_chanlist, 0, M_MENUITEM, 0, 0, 1},
		{N_("Character Chart..."), ascii_open, 0, M_MENUITEM, 0, 0, 1},
		{N_("Direct Chat..."), menu_dcc_chat_win, 0, M_MENUITEM, 0, 0, 1},
		{N_("File Transfers..."), menu_dcc_win, 0, M_MENUITEM, 0, 0, 1},
		{N_("Ignore List..."), ignore_gui_open, 0, M_MENUITEM, 0, 0, 1},
		{N_("Notify List..."), notify_opengui, 0, M_MENUITEM, 0, 0, 1},
		{N_("Plugins and Scripts..."), menu_pluginlist, 0, M_MENUITEM, 0, 0, 1},
		{N_("Raw Log..."), menu_rawlog, 0, M_MENUITEM, 0, 0, 1},	/* 62 */
#endif
		{0, 0, 0, M_SEP, 0, 0, 0},
		{"Resetuj marker", menu_resetmarker, 0, M_MENUITEM, 0, 0, 1, GDK_m},			/* Reset Marker Line */
		{"Czysc okno", menu_flushbuffer, GTK_STOCK_CLEAR, M_MENUSTOCK, 0, 0, 1, GDK_l},		/* C_lear Text */
#if 0

#define SEARCH_OFFSET 67
		{N_("Search Text..."), menu_search, GTK_STOCK_FIND, M_MENUSTOCK, 0, 0, 1, GDK_f},
		{N_("Save Text..."), menu_savebuffer, GTK_STOCK_SAVE, M_MENUSTOCK, 0, 0, 1},
#endif
	{"Pomoc", 0, 0, M_NEWMENU, 0, 0, 1},	/* 69 */
#define menu_docs NULL
		{"Dokumentacja...", menu_docs, GTK_STOCK_HELP, M_MENUSTOCK, 0, 0, 1, GDK_F1},
		{"O ekg..", menu_about, GTK_STOCK_ABOUT, M_MENUSTOCK, 0, 0, 1},
	{0, 0, 0, M_END, 0, 0, 0},
};

GtkWidget *create_icon_menu(char *labeltext, void *stock_name, int is_stock) {
	GtkWidget *item, *img;

	if (is_stock)
		img = gtk_image_new_from_stock(stock_name, GTK_ICON_SIZE_MENU);
	else
		img = gtk_image_new_from_pixbuf(*((GdkPixbuf **)stock_name));
	item = gtk_image_menu_item_new_with_mnemonic(labeltext);
	gtk_image_menu_item_set_image((GtkImageMenuItem *) item, img);
	gtk_widget_show(img);

	return item;
}

#if 0


#if GTK_CHECK_VERSION(2,4,0)

/* Override the default GTK2.4 handler, which would make menu
   bindings not work when the menu-bar is hidden. */
static gboolean
menu_canacaccel(GtkWidget *widget, guint signal_id, gpointer user_data)
{
	/* GTK2.2 behaviour */
	return GTK_WIDGET_IS_SENSITIVE(widget);
}

#endif


/* === STUFF FOR /MENU === */

static GtkMenuItem *
menu_find_item(GtkWidget *menu, char *name)
{
	GList *items = ((GtkMenuShell *) menu)->children;
	GtkMenuItem *item;
	GtkWidget *child;
	const char *labeltext;

	while (items) {
		item = items->data;
		child = GTK_BIN(item)->child;
		if (child) {	/* separators arn't labels, skip them */
			labeltext = g_object_get_data(G_OBJECT(item), "name");
			if (!labeltext)
				labeltext = gtk_label_get_text(GTK_LABEL(child));
			if (!menu_streq(labeltext, name, 1))
				return item;
		} else if (name == NULL) {
			return item;
		}
		items = items->next;
	}

	return NULL;
}

static GtkWidget *
menu_find_path(GtkWidget *menu, char *path)
{
	GtkMenuItem *item;
	char *s;
	char name[128];
	int len;

	/* grab the next part of the path */
	s = strchr(path, '/');
	len = s - path;
	if (!s)
		len = strlen(path);
	len = MIN(len, sizeof(name) - 1);
	memcpy(name, path, len);
	name[len] = 0;

	item = menu_find_item(menu, name);
	if (!item)
		return NULL;

	menu = gtk_menu_item_get_submenu(item);
	if (!menu)
		return NULL;

	path += len;
	if (*path == 0)
		return menu;

	return menu_find_path(menu, path + 1);
}

static GtkWidget *
menu_find(GtkWidget *menu, char *path, char *label)
{
	GtkWidget *item = NULL;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu)
		item = (GtkWidget *)menu_find_item(menu, label);
	return item;
}

static void
menu_foreach_gui(menu_entry * me, void (*callback) (GtkWidget *, menu_entry *, char *))
{
	GSList *list = sess_list;
	int tabdone = FALSE;
	session *sess;

	if (!me->is_main)
		return;		/* not main menu */

	while (list) {
		sess = list->data;
		/* do it only once for tab sessions, since they share a GUI */
		if (!sess->gui->is_tab || !tabdone) {
			callback(sess->gui->menu, me, NULL);
			if (sess->gui->is_tab)
				tabdone = TRUE;
		}
		list = list->next;
	}
}

static void
menu_update_cb(GtkWidget *menu, menu_entry * me, char *target)
{
	GtkWidget *item;

	item = menu_find(menu, me->path, me->label);
	if (item) {
		gtk_widget_set_sensitive(item, me->enable);
		/* must do it without triggering the callback */
		if (GTK_IS_CHECK_MENU_ITEM(item))
			GTK_CHECK_MENU_ITEM(item)->active = me->state;
	}
}

/* radio state changed via mouse click */
static void
menu_radio_cb(GtkCheckMenuItem * item, menu_entry * me)
{
	me->state = 0;
	if (item->active)
		me->state = 1;

	/* update the state, incase this was changed via right-click. */
	/* This will update all other windows and menu bars */
	menu_foreach_gui(me, menu_update_cb);

	if (me->state && me->cmd)
		handle_command(current_sess, me->cmd, FALSE);
}

/* toggle state changed via mouse click */
static void
menu_toggle_cb(GtkCheckMenuItem * item, menu_entry * me)
{
	me->state = 0;
	if (item->active)
		me->state = 1;

	/* update the state, incase this was changed via right-click. */
	/* This will update all other windows and menu bars */
	menu_foreach_gui(me, menu_update_cb);

	if (me->state)
		handle_command(current_sess, me->cmd, FALSE);
	else
		handle_command(current_sess, me->ucmd, FALSE);
}

static GtkWidget *
menu_radio_item(char *label, GtkWidget *menu, void *callback, void *userdata,
		int state, char *groupname)
{
	GtkWidget *item;
	GtkMenuItem *parent;
	GSList *grouplist = NULL;

	parent = menu_find_item(menu, groupname);
	if (parent)
		grouplist = gtk_radio_menu_item_get_group((GtkRadioMenuItem *) parent);

	item = gtk_radio_menu_item_new_with_label(grouplist, label);
	gtk_check_menu_item_set_active((GtkCheckMenuItem *) item, state);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), userdata);
	gtk_widget_show(item);

	return item;
}

static void menu_reorder(GtkMenu * menu, GtkWidget *item, int pos) {
	if (pos == 0xffff)	/* outbound.c uses this default */
		return;

	if (pos < 0)		/* position offset from end/bottom */
		gtk_menu_reorder_child(menu, item,
				       (g_list_length(GTK_MENU_SHELL(menu)->children) + pos) - 1);
	else
		gtk_menu_reorder_child(menu, item, pos);
}

static GtkWidget *menu_add_radio(GtkWidget *menu, menu_entry * me) {
	GtkWidget *item = NULL;
	char *path = me->path + me->root_offset;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu) {
		item = menu_radio_item(me->label, menu, menu_radio_cb, me, me->state, me->group);
		menu_reorder(GTK_MENU(menu), item, me->pos);
	}
	return item;
}

static GtkWidget *menu_add_toggle(GtkWidget *menu, menu_entry *me) {
	GtkWidget *item = NULL;
	char *path = me->path + me->root_offset;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu) {
		item = menu_toggle_item(me->label, menu, menu_toggle_cb, me, me->state);
		menu_reorder(GTK_MENU(menu), item, me->pos);
	}
	return item;
}

static GtkWidget *menu_add_item(GtkWidget *menu, menu_entry * me, char *target) {
	GtkWidget *item = NULL;
	char *path = me->path + me->root_offset;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu) {
		item = menu_quick_item(me->cmd, me->label, menu,
				       me->
				       markup ? XCMENU_MARKUP | XCMENU_MNEMONIC : XCMENU_MNEMONIC,
				       target, me->icon);
		menu_reorder(GTK_MENU(menu), item, me->pos);
	}
	return item;
}

static GtkWidget *menu_add_sub(GtkWidget *menu, menu_entry * me) {
	GtkWidget *item = NULL;
	char *path = me->path + me->root_offset;
	int pos;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu) {
		pos = me->pos;
		if (pos < 0)	/* position offset from end/bottom */
			pos = g_list_length(GTK_MENU_SHELL(menu)->children) + pos;
		menu_quick_sub(me->label, menu, &item,
			       me->markup ? XCMENU_MARKUP | XCMENU_MNEMONIC : XCMENU_MNEMONIC,
			       pos);
	}
	return item;
}

static void menu_del_cb(GtkWidget *menu, menu_entry * me, char *target) {
	GtkWidget *item = menu_find(menu, me->path + me->root_offset, me->label);
	if (item)
		gtk_widget_destroy(item);
}

static void menu_add_cb(GtkWidget *menu, menu_entry * me, char *target) {
	GtkWidget *item;
	GtkAccelGroup *accel_group;

	if (me->group)		/* have a group name? Must be a radio item */
		item = menu_add_radio(menu, me);
	else if (me->ucmd)	/* have unselect-cmd? Must be a toggle item */
		item = menu_add_toggle(menu, me);
	else if (me->cmd || !me->label)	/* label=NULL for separators */
		item = menu_add_item(menu, me, target);
	else
		item = menu_add_sub(menu, me);

	if (item) {
		gtk_widget_set_sensitive(item, me->enable);
		if (me->key) {
			accel_group = g_object_get_data(G_OBJECT(menu), "accel");
			if (accel_group)	/* popup menus don't have them */
				gtk_widget_add_accelerator(item, "activate", accel_group, me->key,
							   me->modifier, GTK_ACCEL_VISIBLE);
		}
	}
}

char *fe_menu_add(menu_entry * me) {
	char *text;

	menu_foreach_gui(me, menu_add_cb);

	if (!me->markup)
		return NULL;

	if (!pango_parse_markup(me->label, -1, 0, NULL, &text, NULL, NULL))
		return NULL;

	/* return the label with markup stripped */
	return text;
}

void fe_menu_del(menu_entry * me) {
	menu_foreach_gui(me, menu_del_cb);
}

void fe_menu_update(menu_entry * me) {
	menu_foreach_gui(me, menu_update_cb);
}

#endif

/* used to add custom menus to the right-click menu */

static void menu_add_plugin_mainmenu_items(GtkWidget *menu) {
#if 0
	GSList *list;
	menu_entry *me;

	list = menu_list;	/* outbound.c */
	while (list) {
		me = list->data;
		if (me->is_main)
			menu_add_cb(menu, me, NULL);
		list = list->next;
	}
#endif
}

void menu_add_plugin_items(GtkWidget *menu, char *root, char *target) {
#if 0
	GSList *list;
	menu_entry *me;

	list = menu_list;	/* outbound.c */
	while (list) {
		me = list->data;
		if (!me->is_main && !strncmp(me->path, root + 1, root[0]))
			menu_add_cb(menu, me, target);
		list = list->next;
	}
#endif
}

/* === END STUFF FOR /MENU === */

GtkWidget *menu_create_main(void *accel_group, int bar, int away, int toplevel, GtkWidget **menu_widgets) {
	int i = 0;
	GtkWidget *item;
	GtkWidget *menu = 0;
	GtkWidget *menu_item = 0;
	GtkWidget *menu_bar;
	GtkWidget *usermenu = 0;
	GtkWidget *submenu = 0;
	int close_mask = GDK_CONTROL_MASK;
	int away_mask = GDK_MOD1_MASK;
	char *key_theme = NULL;
	GtkSettings *settings;
	GSList *group = NULL;

	if (bar)
		menu_bar = gtk_menu_bar_new();
	else
		menu_bar = gtk_menu_new();

	/* /MENU needs to know this later */
	g_object_set_data(G_OBJECT(menu_bar), "accel", accel_group);
#if DARK

#if GTK_CHECK_VERSION(2,4,0)
	g_signal_connect(G_OBJECT(menu_bar), "can-activate-accel", G_CALLBACK(menu_canacaccel), 0);
#endif
#endif

#if 0
	/* set the initial state of toggles */
	mymenu[MENUBAR_OFFSET].state = !prefs.hidemenu;
	mymenu[MENUBAR_OFFSET + 1].state = prefs.topicbar;
	mymenu[MENUBAR_OFFSET + 2].state = !prefs.hideuserlist;
	mymenu[MENUBAR_OFFSET + 3].state = prefs.userlistbuttons;
	mymenu[MENUBAR_OFFSET + 4].state = prefs.chanmodebuttons;

	mymenu[AWAY_OFFSET].state = away;
#endif

#if 0
	switch (prefs.tab_layout) {
	case 0:
		mymenu[TABS_OFFSET].state = 1;
		mymenu[TABS_OFFSET + 1].state = 0;
		break;
	default:
		mymenu[TABS_OFFSET].state = 0;
		mymenu[TABS_OFFSET + 1].state = 1;
	}


	/* change Close binding to ctrl-shift-w when using emacs keys */
	settings = gtk_widget_get_settings(menu_bar);
	if (settings) {
		g_object_get(settings, "gtk-key-theme-name", &key_theme, NULL);
		if (key_theme) {
			if (!strcasecmp(key_theme, "Emacs")) {
				close_mask = GDK_SHIFT_MASK | GDK_CONTROL_MASK;
				mymenu[SEARCH_OFFSET].key = 0;
			}
			g_free(key_theme);
		}
	}

	/* Away binding to ctrl-alt-a if the _Help menu conflicts (FR/PT/IT) */
	{
		char *help = _("_Help");
		char *under = strchr(help, '_');
		if (under && (under[1] == 'a' || under[1] == 'A'))
			away_mask = GDK_MOD1_MASK | GDK_CONTROL_MASK;
	}
#endif

	if (!toplevel) {
		mymenu[DETACH_OFFSET].text = "Odklej zakladke"; /* _Detach Tab" */
		mymenu[CLOSE_OFFSET].text = "Zamknij zakladke"; /* "_Close Tab" */
	} else {
		mymenu[DETACH_OFFSET].text = "Dolacz okno";  /* "_Attach Window" */
		mymenu[CLOSE_OFFSET].text = "Zamknij okno";  /* "_Close Window" */
	}

	while (1) {
		item = NULL;
#if 0
		if (mymenu[i].id == MENU_ID_USERMENU && !prefs.gui_usermenu) {
			i++;
			continue;
		}
#endif
		switch (mymenu[i].type) {
		case M_NEWMENU:
			if (menu)
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
			item = menu = gtk_menu_new();
#if 0
			if (mymenu[i].id == MENU_ID_USERMENU)
				usermenu = menu;
#endif
			menu_item = gtk_menu_item_new_with_mnemonic(mymenu[i].text);
			/* record the English name for /menu */
			g_object_set_data(G_OBJECT(menu_item), "name", mymenu[i].text);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item);
			gtk_widget_show(menu_item);
			break;

		case M_MENUPIX:
			item = create_icon_menu(mymenu[i].text, mymenu[i].image, FALSE);
			goto normalitem;

		case M_MENUSTOCK:
			item = create_icon_menu(mymenu[i].text, mymenu[i].image, TRUE);
			goto normalitem;

		case M_MENUITEM:
			item = gtk_menu_item_new_with_mnemonic(mymenu[i].text);
normalitem:
			if (mymenu[i].key != 0)
				gtk_widget_add_accelerator(item, "activate", accel_group,
							   mymenu[i].key,
							   mymenu[i].key == GDK_F1 ? 0 :
							   mymenu[i].key == GDK_w ? close_mask :
							   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
			if (mymenu[i].callback)
				g_signal_connect(G_OBJECT(item), "activate",
						 G_CALLBACK(mymenu[i].callback), 0);
			if (submenu)
				gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
			else
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			break;

		case M_MENUTOG:
			item = gtk_check_menu_item_new_with_mnemonic(mymenu[i].text);
togitem:
			/* must avoid callback for Radio buttons */
			GTK_CHECK_MENU_ITEM(item)->active = mymenu[i].state;
			/*gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
			   mymenu[i].state); */
#if 0
			if (mymenu[i].key != 0)
				gtk_widget_add_accelerator(item, "activate", accel_group,
							   mymenu[i].key,
							   mymenu[i].id ==
							   MENU_ID_AWAY ? away_mask :
							   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
#endif
			if (mymenu[i].callback)
				g_signal_connect(G_OBJECT(item), "toggled",
						 G_CALLBACK(mymenu[i].callback), 0);
			if (submenu)
				gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
			else
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			gtk_widget_set_sensitive(item, mymenu[i].sensitive);
			break;
		case M_MENURADIO:
			item = gtk_radio_menu_item_new_with_mnemonic(group, mymenu[i].text);
			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
			goto togitem;

		case M_SEP:
			item = gtk_menu_item_new();
			gtk_widget_set_sensitive(item, FALSE);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			break;

		case M_MENUSUB:
			group = NULL;
			submenu = gtk_menu_new();
			item = create_icon_menu(mymenu[i].text, mymenu[i].image, TRUE);
			/* record the English name for /menu */
			g_object_set_data(G_OBJECT(item), "name", mymenu[i].text);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			break;

		/*case M_END: */ default:
			if (!submenu) {
				if (menu) {
					gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
					menu_add_plugin_mainmenu_items(menu_bar);
				}
#if 0
				if (usermenu)
					usermenu_create(usermenu);
#endif
				return (menu_bar);
			}
			submenu = NULL;
		}

		/* record this GtkWidget * so it's state might be changed later */
		if (mymenu[i].id != 0 && menu_widgets)
			/* this ends up in sess->gui->menu_item[MENU_ID_XXX] */
			menu_widgets[mymenu[i].id] = item;
		i++;
	}
}

/********************************************* USERLISTGUI ****************************/
static int ustatus_strip_descr(int status);	/* forward */

static gint userlist_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata) {
	int a1, b1;	/* stored in tree */
	int a2, b2;	/* after ustatus_strip_descr() */
		/* XXX, save in tree a2, b2? */
	int a3, b3;	/* what we've got in contacts_order */

	gint sortcol = GPOINTER_TO_INT(userdata);

	if (sortcol != USERLIST_STATUS) {
		printf("userlist_sort_func() IE\n");
		return 0;
	}

	gtk_tree_model_get(model, a, USERLIST_STATUS, &a1, -1);
	gtk_tree_model_get(model, b, USERLIST_STATUS, &b1, -1);

	a2 = ustatus_strip_descr(a1);
	b2 = ustatus_strip_descr(b1);

	a3 = contacts_order[a2];
	b3 = contacts_order[b2];

	return (a3 - b3);
}

void fe_userlist_numbers(window_t *sess) {
	if (sess == window_current || !gtk_private_ui(sess)->is_tab) {
#if 0
		char tbuf[256];
		if (sess->total) {
			snprintf(tbuf, sizeof(tbuf), _("%d ops, %d total"), sess->ops,
				 sess->total);
			tbuf[sizeof(tbuf) - 1] = 0;
			gtk_label_set_text(GTK_LABEL(sess->gui->namelistinfo), tbuf);
		} else {
			gtk_label_set_text(GTK_LABEL(sess->gui->namelistinfo), NULL);
		}

		if (sess->type == SESS_CHANNEL && prefs.gui_tweaks & 1)
			fe_set_title(sess);
#endif
		gtk_label_set_text(GTK_LABEL(gtk_private_ui(sess)->namelistinfo), "%d avail %d not avail....");
	}
}

char **userlist_selection_list(GtkWidget *widget, int *num_ret) {
	GtkTreeIter iter;
	GtkTreeView *treeview = (GtkTreeView *) widget;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int i, num_sel;
	char **nicks;

	*num_ret = 0;
	/* first, count the number of selections */
	num_sel = 0;
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			if (gtk_tree_selection_iter_is_selected(selection, &iter))
				num_sel++;
		} while (gtk_tree_model_iter_next(model, &iter));
	}

	if (num_sel < 1)
		return NULL;

	nicks = xmalloc(sizeof(char *) * (num_sel + 1));

	i = 0;
	gtk_tree_model_get_iter_first(model, &iter);
	do {
		if (gtk_tree_selection_iter_is_selected(selection, &iter)) {
			gtk_tree_model_get(model, &iter, USERLIST_NICKNAME, &nicks[i], -1);
			i++;
			nicks[i] = NULL;
		}
	} while (gtk_tree_model_iter_next(model, &iter));

	*num_ret = i;
	return nicks;
}

static GtkTreeIter *find_row(GtkTreeView *treeview, GtkTreeModel *model, struct userlist *user, int *selected) {
	static GtkTreeIter iter;
	struct userlist *row_user;

	*selected = FALSE;
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter, USERLIST_USER, &row_user, -1);
			if (row_user == user) {
				if (gtk_tree_view_get_model(treeview) == model) {
					if (gtk_tree_selection_iter_is_selected
					    (gtk_tree_view_get_selection(treeview), &iter))
						*selected = TRUE;
				}
				return &iter;
			}
		} while (gtk_tree_model_iter_next(model, &iter));
	}

	return NULL;
}

void userlist_set_value(GtkWidget *treeview, gfloat val) {
	gtk_adjustment_set_value(gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(treeview)), val);
}

gfloat userlist_get_value(GtkWidget *treeview) {
	return gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(treeview))->value;
}

int fe_userlist_remove(window_t *sess, struct userlist *user) {
	GtkTreeIter *iter;
/*	GtkAdjustment *adj;
	gfloat val, end;*/
	int sel;

	iter = find_row(GTK_TREE_VIEW(sess->gui->user_tree), sess->user_model, user, &sel);
	if (!iter)
		return 0;

/*	adj = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (sess->gui->user_tree));
	val = adj->value;*/

	gtk_list_store_remove(sess->user_model, iter);

	/* is it the front-most tab? */
/*	if (gtk_tree_view_get_model (GTK_TREE_VIEW (sess->gui->user_tree))
		 == sess->res->user_model)
	{
		end = adj->upper - adj->lower - adj->page_size;
		if (val > end)
			val = end;
		gtk_adjustment_set_value (adj, val);
	}*/

	return sel;
}

static int ustatus_strip_descr(int status) {
	switch (status) {
		case GG_STATUS_AVAIL:
		case GG_STATUS_AVAIL_DESCR:
			return PIXBUF_AVAIL;

		case GG_STATUS_BUSY:
		case GG_STATUS_BUSY_DESCR:
			return PIXBUF_AWAY;

		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_NOT_AVAIL_DESCR:
			return PIXBUF_NOTAVAIL;

		case GG_STATUS_INVISIBLE:
		case GG_STATUS_INVISIBLE_DESCR:
			return PIXBUF_INVISIBLE;

		case GG_STATUS_BLOCKED:
			return PIXBUF_BLOCKED;
	}

	printf("ustatus_strip_descr() unk: %d\n", status);
	return PIXBUF_NOTAVAIL;
}


gboolean fe_userlist_rehash(window_t *sess, struct userlist *u) {
	GtkTreeIter *iter;
	int sel;
	int do_away = TRUE;

	if (!(iter = find_row(GTK_TREE_VIEW(sess->gui->user_tree), sess->user_model, u, &sel)))
		return 0;
#if 0
	if (prefs.away_size_max < 1 || !prefs.away_track)
		do_away = FALSE;
#endif
	gtk_list_store_set(GTK_LIST_STORE(sess->user_model), iter,
					  USERLIST_STATUS, u->status,
					  USERLIST_NICKNAME, u->nickname,
					  USERLIST_UIN, u->uin,
					  USERLIST_DESCRIPTION, u->descr,
//					  USERLIST_COLOR, /* (do_away) */ FALSE, ? (newuser->away ? &colors[COL_AWAY] : NULL) : */ (NULL),
					  -1);
	
	return 0;
}

void fe_userlist_insert(window_t *sess, struct userlist *u) {
	GtkTreeModel *model = gtk_private(sess)->user_model;
	GtkTreeIter iter;
	int do_away = TRUE;

	int sel = 0;

#if 0
	if (prefs.away_size_max < 1 || !prefs.away_track)
		do_away = FALSE;
#endif
	gtk_list_store_insert_with_values(GTK_LIST_STORE(model), &iter, -1,
					  USERLIST_STATUS, u->status,
					  USERLIST_NICKNAME, u->nickname,
					  USERLIST_UIN, u->uin,
					  USERLIST_DESCRIPTION, u->descr,
					  USERLIST_USER, u,
//					  USERLIST_COLOR, /* (do_away) */ FALSE, ? (newuser->away ? &colors[COL_AWAY] : NULL) : */ (NULL),
					  -1);

#if DARK
	/* is it me? */
	if (newuser->me && sess->gui->nick_box) {
		if (!sess->gui->is_tab || sess == current_tab)
			mg_set_access_icon(sess->gui, pix, sess->server->is_away);
	}
#if 0				/* not mine IF !! */
	if (prefs.hilitenotify && notify_isnotify(sess, newuser->nick)) {
		gtk_clist_set_foreground((GtkCList *) sess->gui->user_clist, row,
					 &colors[prefs.nu_color]);
	}
#endif

#endif
}

void fe_userlist_clear(window_t *sess) {
	gtk_list_store_clear(gtk_private(sess)->user_model);
}

void *userlist_create_model(void)
{
	GtkTreeSortable *sortable;

	void *liststore;

	liststore = gtk_list_store_new(USERLIST_COLS, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_COLOR);

	sortable = GTK_TREE_SORTABLE(liststore);
	
	gtk_tree_sortable_set_sort_func(sortable, USERLIST_STATUS, userlist_sort_func, GINT_TO_POINTER(USERLIST_STATUS), NULL);

/* initial sort */
	gtk_tree_sortable_set_sort_column_id(sortable, USERLIST_STATUS, GTK_SORT_ASCENDING);

	return liststore;
}

static void userlist_render_pixmap(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data) {
	int status;
	gtk_tree_model_get(tree_model, iter, GPOINTER_TO_INT(data), &status, -1);

	g_object_set(cell, "pixbuf", gg_pixs[ustatus_strip_descr(status)], NULL);
}

static void userlist_add_columns(GtkTreeView * treeview) {
	GtkCellRenderer *renderer;

/* icon column */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(treeview), -1, NULL, renderer, userlist_render_pixmap, (gpointer) USERLIST_STATUS, NULL);

/* nick column */
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(renderer), 1);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, NULL, renderer, "text", USERLIST_NICKNAME, "foreground-gdk", USERLIST_COLOR, NULL);

/* description column (?) */
	if (show_descr_in_userlist_config) {
		renderer = gtk_cell_renderer_text_new();
		gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(renderer), 1);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, NULL, renderer, "text", USERLIST_DESCRIPTION, NULL);
	}
}

static gint userlist_click_cb(GtkWidget *widget, GdkEventButton * event, gpointer userdata) {
	char **nicks;
	int i;
	GtkTreeSelection *sel;
	GtkTreePath *path;

	if (!event)
		return FALSE;

	if (!(event->state & GDK_CONTROL_MASK) && event->type == GDK_2BUTTON_PRESS /* && prefs.doubleclickuser[0] */) {
		nicks = userlist_selection_list(widget, &i);
		if (nicks) {
/*			nick_command_parse(current_sess, prefs.doubleclickuser, nicks[0], nicks[0]); */
			command_exec_format(window_current->target, 0, ("/query %s"), nicks[0]);

			while (i) {
				i--;
				g_free(nicks[i]);
			}
			free(nicks);
		}
		return TRUE;
	}

	if (event->button == 3) {
		/* do we have a multi-selection? */
		nicks = userlist_selection_list(widget, &i);
		if (nicks && i > 1) {
			menu_nickmenu(window_current, event, nicks[0], i);
			while (i) {
				i--;
				g_free(nicks[i]);
			}
			free(nicks);
			return TRUE;
		}
		if (nicks) {
			g_free(nicks[0]);
			free(nicks);
		}

		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
						  event->x, event->y, &path, 0, 0, 0)) {
			gtk_tree_selection_unselect_all(sel);
			gtk_tree_selection_select_path(sel, path);
			gtk_tree_path_free(path);
			nicks = userlist_selection_list(widget, &i);
			if (nicks) {
				menu_nickmenu(window_current, event, nicks[0], i);
				while (i) {
					i--;
					g_free(nicks[i]);
				}
				free(nicks);
			}
		} else {
			gtk_tree_selection_unselect_all(sel);
		}

		return TRUE;
	}
	return FALSE;
}

static gboolean userlist_key_cb(GtkWidget *wid, GdkEventKey * evt, gpointer userdata)
{
#if 0
	if (evt->keyval >= GDK_asterisk && evt->keyval <= GDK_z) {
		/* dirty trick to avoid auto-selection */
		SPELL_ENTRY_SET_EDITABLE(current_sess->gui->input_box, FALSE);
		gtk_widget_grab_focus(current_sess->gui->input_box);
		SPELL_ENTRY_SET_EDITABLE(current_sess->gui->input_box, TRUE);
		gtk_widget_event(current_sess->gui->input_box, (GdkEvent *) evt);
		return TRUE;
	}
#endif
	return FALSE;
}

GtkWidget *userlist_create(GtkWidget *box)
{
	GtkWidget *sw, *treeview;

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
			show_descr_in_userlist_config ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER, 
			GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	treeview = gtk_tree_view_new();
	gtk_widget_set_name(treeview, "xchat-userlist");
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection
				    (GTK_TREE_VIEW(treeview)), GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(treeview), "button_press_event",
			 G_CALLBACK(userlist_click_cb), 0);
	g_signal_connect(G_OBJECT(treeview), "key_press_event", G_CALLBACK(userlist_key_cb), 0);

#warning "xchat->ekg2: drag & drop"

	userlist_add_columns(GTK_TREE_VIEW(treeview));

	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	return treeview;
}

void userlist_show(window_t *sess)
{
	gtk_tree_view_set_model(GTK_TREE_VIEW(gtk_private_ui(sess)->user_tree), gtk_private(sess)->user_model);
}

/********************************************* GTKUTIL ********************************/

void add_tip(GtkWidget *wid, char *text) {
	static GtkTooltips *tip = NULL;
	if (!tip)
		tip = gtk_tooltips_new();
	gtk_tooltips_set_tip(tip, wid, text, 0);
}

void gtkutil_set_icon(GtkWidget *win) {
	gtk_window_set_icon(GTK_WINDOW(win), pix_ekg);
}

GtkWidget *gtkutil_button(GtkWidget *box, char *stock, char *tip, void *callback, void *userdata, char *labeltext) {
	GtkWidget *wid, *img, *bbox;

	wid = gtk_button_new();

	if (labeltext) {
		gtk_button_set_label(GTK_BUTTON(wid), labeltext);
		gtk_button_set_image(GTK_BUTTON(wid),
				     gtk_image_new_from_stock(stock, GTK_ICON_SIZE_MENU));
		if (box)
			gtk_container_add(GTK_CONTAINER(box), wid);
	} else {
		bbox = gtk_hbox_new(0, 0);
		gtk_container_add(GTK_CONTAINER(wid), bbox);
		gtk_widget_show(bbox);

		img = gtk_image_new_from_stock(stock, GTK_ICON_SIZE_MENU);
		if (stock == GTK_STOCK_GOTO_LAST)
			gtk_widget_set_usize(img, 10, 6);
		gtk_container_add(GTK_CONTAINER(bbox), img);
		gtk_widget_show(img);
		gtk_box_pack_start(GTK_BOX(box), wid, 0, 0, 0);
	}

	g_signal_connect(G_OBJECT(wid), "clicked", G_CALLBACK(callback), userdata);
	gtk_widget_show(wid);
	if (tip)
		add_tip(wid, tip);
	return wid;
}

extern GtkWidget *parent_window;	/* maingui.c */


GtkWidget *
gtkutil_window_new(char *title, char *role, int width, int height, int flags)
{
	GtkWidget *win;

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtkutil_set_icon(win);
	gtk_window_set_title(GTK_WINDOW(win), title);
	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_role(GTK_WINDOW(win), role);
	if (flags & 1)
		gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_MOUSE);
	if ((flags & 2) && parent_window) {
		gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
		gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(parent_window));
	}

	return win;
}

char *gtk_window_target(window_t *window) {
	if (!window)			return "";

	if (window->target)		return window->target;
	else if (window->id == 1)	return "__status";
	else if (window->id == 0)	return "__debug";
        else                            return "";
}


static PangoAttrList *mg_attr_list_create(GdkColor *col, int size) {
	PangoAttribute *attr;
	PangoAttrList *list;

	list = pango_attr_list_new();

	if (col) {
		attr = pango_attr_foreground_new(col->red, col->green, col->blue);
		attr->start_index = 0;
		attr->end_index = 0xffff;
		pango_attr_list_insert(list, attr);
	}

	if (size > 0) {
		attr = pango_attr_scale_new(size == 1 ? PANGO_SCALE_SMALL : PANGO_SCALE_X_SMALL);
		attr->start_index = 0;
		attr->end_index = 0xffff;
		pango_attr_list_insert(list, attr);
	}

	return list;
}

static void mg_create_tab_colors(void) {
	if (plain_list) {
		pango_attr_list_unref(plain_list);
		pango_attr_list_unref(newmsg_list);
		pango_attr_list_unref(newdata_list);
		pango_attr_list_unref(nickseen_list);
		pango_attr_list_unref(away_list);
	}

	plain_list = mg_attr_list_create(NULL, tab_small_config);
	newdata_list = mg_attr_list_create(&colors[COL_NEW_DATA], tab_small_config);
	nickseen_list = mg_attr_list_create(&colors[COL_HILIGHT], tab_small_config);
	newmsg_list = mg_attr_list_create(&colors[COL_NEW_MSG], tab_small_config);
	away_list = mg_attr_list_create(&colors[COL_AWAY], FALSE);
}

#ifdef USE_XLIB
#include <gdk/gdkx.h>

static void set_window_urgency(GtkWidget *win, gboolean set) {
	XWMHints *hints;

	hints = XGetWMHints(GDK_WINDOW_XDISPLAY(win->window), GDK_WINDOW_XWINDOW(win->window));
	if (set)
		hints->flags |= XUrgencyHint;
	else
		hints->flags &= ~XUrgencyHint;
	XSetWMHints(GDK_WINDOW_XDISPLAY(win->window), GDK_WINDOW_XWINDOW(win->window), hints);
	XFree(hints);
}

static void flash_window(GtkWidget *win) {
	set_window_urgency(win, TRUE);
}

static void unflash_window(GtkWidget *win) {
	set_window_urgency(win, FALSE);
}

#endif

int fe_gui_info(window_t *sess, int info_type) {	/* code from fe-gtk.c */
	switch (info_type) {
		case 0:	/* window status */
			if (!GTK_WIDGET_VISIBLE(GTK_WINDOW(gtk_private_ui(sess)->window)))
				return 2;	/* hidden (iconified or systray) */

#warning "GTK issue."
	/* 2.4.0 -> gtk_window_is_active(GTK_WINDOW(gtk_private_ui(sess)->window))
	 * 2.2.0 -> GTK_WINDOW(gtk_private_ui(sess)->window)->is_active)
	 *
	 * 		return 1
	 */
		return 0;		/* normal (no keyboard focus or behind a window) */
	}

	return -1;
}

/* flash the taskbar button */

void fe_flash_window(window_t *sess) {
#if defined(WIN32) || defined(USE_XLIB)
	if (fe_gui_info(sess, 0) != 1)	/* only do it if not focused */
		flash_window(gtk_private_ui(sess)->window);
#endif
}

/* set a tab plain, red, light-red, or blue */

void fe_set_tab_color(window_t *sess, int col) {
	if (!gtk_private_ui(sess)->is_tab)
		return;
	
//    col value, what todo                                            values                                                  comment.
//      0: chan_set_color(sess->tab, plain_list);           [new_data = NULL, msg_said = NULL, nick_said = NULL]    /* no particular color (theme default) */
//      1: chan_set_color(sess->tab, newdata_list);         [new_data = TRUE, msg_said = NULL, nick_said = NULL]    /* new data has been displayed (dark red) */
//      2: chan_set_color(sess->tab, newmsg_list);          [new_data = NULL, msg_said = TRUE, nick_said = NULL]    /* new message arrived in channel (light red) */
//      3: chan_set_color(sess->tab, nickseen_list) ;       [new_data = NULL, msg_said = NULL, nick_said = TRUE]    /* your nick has been seen (blue) */    

	if (col == 0) chan_set_color(gtk_private(sess)->tab, plain_list);
	if (col == 1) chan_set_color(gtk_private(sess)->tab, newdata_list);
	if (col == 2) chan_set_color(gtk_private(sess)->tab, newmsg_list);

}

#if 0

static void mg_set_myself_away(gtk_window_ui_t *gui, gboolean away) {
	gtk_label_set_attributes(GTK_LABEL(GTK_BIN(gui->nick_label)->child),
				 away ? away_list : NULL);
}

/* change the little icon to the left of your nickname */

void mg_set_access_icon(gtk_window_ui_t *gui, GdkPixbuf *pix, gboolean away) {
	if (gui->op_xpm) {
		gtk_widget_destroy(gui->op_xpm);
		gui->op_xpm = 0;
	}

	if (pix) {
		gui->op_xpm = gtk_image_new_from_pixbuf(pix);
		gtk_box_pack_start(GTK_BOX(gui->nick_box), gui->op_xpm, 0, 0, 0);
		gtk_widget_show(gui->op_xpm);
	}

	mg_set_myself_away(gui, away);
}

#endif

static gboolean mg_inputbox_focus(GtkWidget *widget, GdkEventFocus *event, gtk_window_ui_t *gui) {
	list_t l;

	if (gui->is_tab)
		return FALSE;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (gtk_private(w)->gui == gui) {
			ui_gtk_window_switch(w->id);
			return FALSE;
		}

	}

	printf("mg_inputbox_focus() internal error!\n");

	return FALSE;
}

void mg_inputbox_cb(GtkWidget *igad, gtk_window_ui_t *gui) {
	static int ignore = FALSE;
	window_t *sess = NULL;
	char *cmd;

	if (ignore)
		return;

	cmd = GTK_ENTRY(igad)->text;
	if (cmd[0] == '\0')
		return;

	cmd = xstrdup(cmd);

	/* avoid recursive loop */
	ignore = TRUE;
	gtk_entry_set_text(GTK_ENTRY(igad), "");
	ignore = FALSE;

	/* where did this event come from? */
	if (gui->is_tab) {
		sess = window_current;
	} else {
		list_t l;

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (gtk_private_ui(w) == gui) {
				sess = w;
				break;
			}

		}
		if (!sess)
			printf("FATAL, not found.\n");
	}

	if (sess) {
		/* XXX, recode from utf-8 to latin2 */
		command_exec(sess->target, cmd, 0);

		history[0] = cmd;
		xfree(history[HISTORY_MAX - 1]);

		memmove(&history[1], &history[0], sizeof(history) - sizeof(history[0]));

		history_index = 0;
		history[0] = NULL;

		return;
	}

	xfree(cmd);
}

void fe_set_title(window_t *sess) {
	gtk_window_ui_t *n = gtk_private_ui(sess);

	if (n->is_tab && sess != window_current)
		return;

	gtk_window_set_title(GTK_WINDOW(n->window), "ekg");
}


static gboolean mg_windowstate_cb(GtkWindow * wid, GdkEventWindowState * event, gpointer userdata) {
#if 0
	prefs.gui_win_state = 0;
	if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
		prefs.gui_win_state = 1;

	if ((event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) &&
	    (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) && (prefs.gui_tray_flags & 4)) {
		tray_toggle_visibility(TRUE);
		gtk_window_deiconify(wid);
	}
#endif
	return FALSE;
}

static gboolean mg_configure_cb(GtkWidget *wid, GdkEventConfigure * event, window_t *sess) {
#if 0
	if (sess == NULL) {	/* for the main_window */
		if (mg_gui) {
			if (prefs.mainwindow_save) {
				sess = current_sess;
				gtk_window_get_position(GTK_WINDOW(wid), &prefs.mainwindow_left,
							&prefs.mainwindow_top);
				gtk_window_get_size(GTK_WINDOW(wid), &prefs.mainwindow_width,
						    &prefs.mainwindow_height);
			}
		}
	}

	if (sess) {
		if (sess->type == SESS_DIALOG && prefs.mainwindow_save) {
			gtk_window_get_position(GTK_WINDOW(wid), &prefs.dialog_left,
						&prefs.dialog_top);
			gtk_window_get_size(GTK_WINDOW(wid), &prefs.dialog_width,
					    &prefs.dialog_height);
		}

		if (((GtkXText *) sess->gui->xtext)->transparent)
			gtk_widget_queue_draw(sess->gui->xtext);
	}
#endif
	return FALSE;
}

#if 0

/* move to a non-irc tab */

static void mg_show_generic_tab(GtkWidget *box) {
	int num;
	GtkWidget *f = NULL;

	if (current_sess && GTK_WIDGET_HAS_FOCUS(current_sess->gui->input_box))
		f = current_sess->gui->input_box;

	num = gtk_notebook_page_num(GTK_NOTEBOOK(mg_gui->note_book), box);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(mg_gui->note_book), num);
	gtk_tree_view_set_model(GTK_TREE_VIEW(mg_gui->user_tree), NULL);
	gtk_window_set_title(GTK_WINDOW(mg_gui->window),
			     g_object_get_data(G_OBJECT(box), "title"));
	gtk_widget_set_sensitive(mg_gui->menu, FALSE);

	if (f)
		gtk_widget_grab_focus(f);
}

#endif

/* a channel has been focused */

static void mg_focus(window_t *sess) {
#if 0
	if (sess->gui->is_tab)
		current_tab = sess;
	current_sess = sess;

	/* dirty trick to avoid auto-selection */
	SPELL_ENTRY_SET_EDITABLE(sess->gui->input_box, FALSE);
	gtk_widget_grab_focus(sess->gui->input_box);
	SPELL_ENTRY_SET_EDITABLE(sess->gui->input_box, TRUE);

	sess->server->front_session = sess;

	if (sess->server->server_session != NULL) {
		if (sess->server->server_session->type != SESS_SERVER)
			sess->server->server_session = sess;
	} else {
		sess->server->server_session = sess;
	}

	if (sess->new_data || sess->nick_said || sess->msg_said) {
		sess->nick_said = FALSE;
		sess->msg_said = FALSE;
		sess->new_data = FALSE;
		/* when called via mg_changui_new, is_tab might be true, but
		   sess->res->tab is still NULL. */
		if (sess->res->tab)
			fe_set_tab_color(sess, 0);
	}
#endif
}

#if 0

void mg_set_topic_tip(session *sess) {
	char *text;

	switch (sess->type) {
	case SESS_CHANNEL:
		if (sess->topic) {
			text = g_strdup_printf(_("Topic for %s is: %s"), sess->channel,
					       sess->topic);
			add_tip(sess->gui->topic_entry, text);
			g_free(text);
		} else
			add_tip(sess->gui->topic_entry, _("No topic is set"));
		break;
	default:
		if (GTK_ENTRY(sess->gui->topic_entry)->text &&
		    GTK_ENTRY(sess->gui->topic_entry)->text[0])
			add_tip(sess->gui->topic_entry, GTK_ENTRY(sess->gui->topic_entry)->text);
		else
			add_tip(sess->gui->topic_entry, NULL);
	}
}

#endif

static void mg_hide_empty_pane(GtkPaned * pane) {
	if ((pane->child1 == NULL || !GTK_WIDGET_VISIBLE(pane->child1)) &&
	    (pane->child2 == NULL || !GTK_WIDGET_VISIBLE(pane->child2))) {
		gtk_widget_hide(GTK_WIDGET(pane));
		return;
	}

	gtk_widget_show(GTK_WIDGET(pane));
}

static void mg_hide_empty_boxes(gtk_window_ui_t *gui) {
	/* hide empty vpanes - so the handle is not shown */
	mg_hide_empty_pane((GtkPaned *) gui->vpane_right);
	mg_hide_empty_pane((GtkPaned *) gui->vpane_left);
}

static void mg_userlist_showhide(window_t *sess, int show) {
	gtk_window_ui_t *gui = gtk_private_ui(sess);
	int handle_size;

	if (show) {
		gtk_widget_show(gui->user_box);
		gui->ul_hidden = 0;

		gtk_widget_style_get(GTK_WIDGET(gui->hpane_right), "handle-size", &handle_size,
				     NULL);
		gtk_paned_set_position(GTK_PANED(gui->hpane_right),
				       GTK_WIDGET(gui->hpane_right)->allocation.width -
				       (gui_pane_right_size_config + handle_size));
	} else {
		gtk_widget_hide(gui->user_box);
		gui->ul_hidden = 1;
	}

	mg_hide_empty_boxes(gui);
}

/* decide if the userlist should be shown or hidden for this tab */

void mg_decide_userlist(window_t *sess, gboolean switch_to_current) {
	/* when called from menu.c we need this */
	if (gtk_private_ui(sess) == mg_gui && switch_to_current)
		sess = window_current;

	if (!config_contacts) {
		mg_userlist_showhide(sess, FALSE);
		return;
	}

	/* xchat->ekg2 XXX, here: mg_is_userlist_and_tree_combined() stuff */

	mg_userlist_showhide(sess, TRUE);	/* show */
}


static void mg_userlist_toggle_cb(GtkWidget *button, gpointer userdata) {
	config_contacts = !config_contacts;

	mg_decide_userlist(window_current, FALSE);

	gtk_widget_grab_focus(gtk_private_ui(window_current)->input_box);
}

gboolean mg_populate_userlist(window_t *sess) {
	gtk_window_ui_t *gui;
	list_t l;

	fe_userlist_clear(sess);

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;
		int status;

		if (!u || !u->display || !u->uin)
			continue;

		status = ustatus_strip_descr(u->status);

		if (contacts_order[status] < 0 || contacts_order[status] > 4)
			continue;

		fe_userlist_insert(sess, u);
	}

	gui = gtk_private_ui(sess);
#if 0
	if (sess->type == SESS_DIALOG)
		mg_set_access_icon(sess->gui, NULL, sess->server->is_away);
	else
		mg_set_access_icon(sess->gui, get_user_icon(sess->server, sess->me),
				sess->server->is_away);
#endif
	userlist_show(sess);
	userlist_set_value(gtk_private_ui(sess)->user_tree, gtk_private(sess)->old_ul_value);
	return 0;
}

/* fill the irc tab with a new channel */

/* static */ void mg_populate(window_t *sess) {
	gtk_window_ui_t *gui = sess->gui;

	int render = TRUE;
	guint16 vis = gui->ul_hidden;

#if 0
	switch (sess->type) {
	case SESS_DIALOG:
		/* show the dialog buttons */
		gtk_widget_show(gui->dialogbutton_box);
		/* hide the chan-mode buttons */
		gtk_widget_hide(gui->topicbutton_box);
		/* hide the userlist */
		mg_decide_userlist(sess, FALSE);
		/* shouldn't edit the topic */
		gtk_editable_set_editable(GTK_EDITABLE(gui->topic_entry), FALSE);
		break;
	case SESS_SERVER:
		if (prefs.chanmodebuttons)
			gtk_widget_show(gui->topicbutton_box);
		/* hide the dialog buttons */
		gtk_widget_hide(gui->dialogbutton_box);
		/* hide the userlist */
		mg_decide_userlist(sess, FALSE);
		/* shouldn't edit the topic */
		gtk_editable_set_editable(GTK_EDITABLE(gui->topic_entry), FALSE);
		break;
	default:
		/* hide the dialog buttons */
		gtk_widget_hide(gui->dialogbutton_box);
		if (prefs.chanmodebuttons)
			gtk_widget_show(gui->topicbutton_box);
		/* show the userlist */
		mg_decide_userlist(sess, FALSE);
		/* let the topic be editted */
		gtk_editable_set_editable(GTK_EDITABLE(gui->topic_entry), TRUE);
	}
#else
		mg_decide_userlist(sess, FALSE);
#endif
	/* move to THE irc tab */
	if (gui->is_tab)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gui->note_book), 0);

	/* xtext size change? Then don't render, wait for the expose caused
	   by showing/hidding the userlist */
	if (vis != gui->ul_hidden && gui->user_box->allocation.width > 1)
		render = FALSE;

	gtk_xtext_buffer_show(GTK_XTEXT(gui->xtext), sess->buffer, render);
	if (gui->is_tab)
		gtk_widget_set_sensitive(gui->menu, TRUE);

	mg_focus(sess);
	fe_set_title(sess);

	fe_userlist_numbers(sess);
#if 0
	/* menu items */
	GTK_CHECK_MENU_ITEM(gui->menu_item[MENU_ID_AWAY])->active = sess->server->is_away;
	gtk_widget_set_sensitive(gui->menu_item[MENU_ID_AWAY], sess->server->connected);
	gtk_widget_set_sensitive(gui->menu_item[MENU_ID_JOIN], sess->server->end_of_motd);
	gtk_widget_set_sensitive(gui->menu_item[MENU_ID_DISCONNECT],
				 sess->server->connected || sess->server->recondelay_tag);

	mg_set_topic_tip(sess);

	plugin_emit_dummy_print(sess, "Focus Tab");
#endif
}

#if 0

void mg_bring_tofront_sess(session *sess) {				/* IRC tab or window */
	if (sess->gui->is_tab)
		chan_focus(sess->res->tab);
	else
		gtk_window_present(GTK_WINDOW(sess->gui->window));
}

void mg_bring_tofront(GtkWidget *vbox) {				/* non-IRC tab or window */
	chan *ch;

	ch = g_object_get_data(G_OBJECT(vbox), "ch");
	if (ch)
		chan_focus(ch);
	else
		gtk_window_present(GTK_WINDOW(gtk_widget_get_toplevel(vbox)));
}

#endif

void mg_switch_page(int relative, int num) {
	if (mg_gui)
		chanview_move_focus(mg_gui->chanview, relative, num);
}

/* a toplevel IRC window was destroyed */

static void mg_topdestroy_cb(GtkWidget *win, window_t *sess) {
#warning "xchat->ekg2: mg_topdestroy_cb() BIG XXX"
	printf("mg_topdestroy_cb() XXX\n");
#if 0
/*	printf("enter mg_topdestroy. sess %p was destroyed\n", sess);*/

	/* kill the text buffer */
	gtk_xtext_buffer_free(gtk_private(sess)->buffer);
#if 0
	/* kill the user list */
	g_object_unref(G_OBJECT(sess->res->user_model));
#endif
	gtk_window_kill(sess);	/* XXX, session_free(sess) */
#endif
}

#if 0

/* cleanup an IRC tab */

static void mg_ircdestroy(session *sess) {
	GSList *list;

	/* kill the text buffer */
	gtk_xtext_buffer_free(sess->res->buffer);
	/* kill the user list */
	g_object_unref(G_OBJECT(sess->res->user_model));

	session_free(sess);	/* tell xchat.c about it */

	if (mg_gui == NULL) {
/*		puts("-> mg_gui is already NULL");*/
		return;
	}

	list = sess_list;
	while (list) {
		sess = list->data;
		if (sess->gui->is_tab) {
/*			puts("-> some tabs still remain");*/
			return;
		}
		list = list->next;
	}

/*	puts("-> no tabs left, killing main tabwindow");*/
	gtk_widget_destroy(mg_gui->window);
	active_tab = NULL;
	mg_gui = NULL;
	parent_window = NULL;
}

#endif

void mg_tab_close(window_t *sess) {
#warning "xchat->ekg2: mg_tab_close() XXX"
	if (chan_remove(gtk_private(sess)->tab, FALSE))
#if 0
		mg_ircdestroy(sess);
#else
		;
#endif
}

#if 0

static void mg_traymsg_cb(GtkCheckMenuItem * item, session *sess) {
	sess->tray = FALSE;
	if (item->active)
		sess->tray = TRUE;
}

static void mg_beepmsg_cb(GtkCheckMenuItem * item, session *sess) {
	sess->beep = FALSE;
	if (item->active)
		sess->beep = TRUE;
}

static void mg_hidejp_cb(GtkCheckMenuItem * item, session *sess) {
	sess->hide_join_part = TRUE;
	if (item->active)
		sess->hide_join_part = FALSE;
}
#endif

static void mg_menu_destroy(GtkWidget *menu, gpointer userdata) {
	gtk_widget_destroy(menu);
	g_object_unref(menu);
}

void mg_create_icon_item(char *label, char *stock, GtkWidget *menu, void *callback, void *userdata) {
	GtkWidget *item;

	item = create_icon_menu(label, stock, TRUE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), userdata);
	gtk_widget_show(item);
}

void mg_open_quit_dialog(gboolean minimize_button) {
	static GtkWidget *dialog = NULL;
	GtkWidget *dialog_vbox1;
	GtkWidget *table1;
	GtkWidget *image;
	GtkWidget *checkbutton1;
	GtkWidget *label;
	GtkWidget *dialog_action_area1;
	GtkWidget *button;
	char *text;

	if (dialog) {
		gtk_window_present(GTK_WINDOW(dialog));
		return;
	}

	if (!gui_quit_dialog_config) {
		ekg_exit();
		return;
	}

	if (config_save_question == 1) {
#warning "Display question if user want to /save config"
/*
		if (config_changed) 				format_find("config_changed")
		else if (config_keep_reason && reason_changed)	format_find("quit_keep_reason");
*/
		config_save_question = 0;
	}

#warning "xchat->ekg2 XXX"
	/* 	xchat count dcc's + connected network, and display warning about it.
	 *
	 * 		"<span weight=\"bold\" size=\"larger\">Are you sure you want to quit?</span>\n
	 * 			"You are connected to %i IRC networks."
	 * 			"Some file transfers are still active."
	 */


	dialog = gtk_dialog_new();
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_window_set_title(GTK_WINDOW(dialog), "Wyjsc z ekg?");
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent_window));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

	dialog_vbox1 = GTK_DIALOG(dialog)->vbox;
	gtk_widget_show(dialog_vbox1);

	table1 = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table1);
	gtk_box_pack_start(GTK_BOX(dialog_vbox1), table1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table1), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table1), 12);
	gtk_table_set_col_spacings(GTK_TABLE(table1), 12);

	image = gtk_image_new_from_stock("gtk-dialog-warning", GTK_ICON_SIZE_DIALOG);
	gtk_widget_show(image);
	gtk_table_attach(GTK_TABLE(table1), image, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

	checkbutton1 = gtk_check_button_new_with_mnemonic("Nie pytaj nastepnym razem.");
	gtk_widget_show(checkbutton1);
	gtk_table_attach(GTK_TABLE(table1), checkbutton1, 0, 2, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 4);

	text = saprintf("<span weight=\"bold\" size=\"larger\">%s</span>\n", "Czy na pewno chcesz wyjsc?");
	label = gtk_label_new(text);
	xfree(text);

	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table1), label, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	dialog_action_area1 = GTK_DIALOG(dialog)->action_area;
	gtk_widget_show(dialog_action_area1);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(dialog_action_area1), GTK_BUTTONBOX_END);

	if (minimize_button) {
		button = gtk_button_new_with_mnemonic("Minimalizuj do ikony");
		gtk_widget_show(button);
		gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, 1);
	}

	button = gtk_button_new_from_stock("gtk-cancel");
	gtk_widget_show(button);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, GTK_RESPONSE_CANCEL);
	gtk_widget_grab_focus(button);

	button = gtk_button_new_from_stock("gtk-quit");
	gtk_widget_show(button);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, 0);

	gtk_widget_show(dialog);

	switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
	case 0:
#if 0
		if (GTK_TOGGLE_BUTTON(checkbutton1)->active)
			gui_quit_dialog_config = 0;
		xchat_exit();
#endif
		ekg_exit();
		break;
	case 1:		/* minimize to tray */
#if 0
		if (GTK_TOGGLE_BUTTON(checkbutton1)->active) {
			gui_tray_flags_config |= 1;
			/*prefs.gui_quit_dialog = 0; */
		}
		tray_toggle_visibility(TRUE);
#endif
		break;
	}

	gtk_widget_destroy(dialog);
	dialog = NULL;
}

void mg_close_sess(window_t *sess) {
	if (sess == window_status) {	/* status window? */
		mg_open_quit_dialog(FALSE);
		return;
	}
	ui_gtk_window_kill(sess, 1);	/* fe_close_window() */
}

static int mg_chan_remove(chan * ch) {
	/* remove the tab from chanview */
	chan_remove(ch, TRUE);
	/* any tabs left? */
	if (chanview_get_size(mg_gui->chanview) < 1) {
		/* if not, destroy the main tab window */
		gtk_widget_destroy(mg_gui->window);
#if DARK
		current_tab = NULL;
#endif
		active_tab = NULL;
		mg_gui = NULL;
		parent_window = NULL;
		return TRUE;
	}
	return FALSE;
}

/* the "X" close button has been pressed (tab-view) */

static void mg_xbutton_cb(chanview * cv, chan * ch, int tag, gpointer userdata) {
	printf("mg_xbutoon_cb(%p) [%d [TAG_IRC: %d]\n", userdata, tag, TAG_IRC);
	if (tag == TAG_IRC)	/* irc tab */
		mg_close_sess(userdata);

#warning "xchat->ekg2, removed support for generic tabs"
}


static void mg_detach_tab_cb(GtkWidget *item, chan * ch) {
	if (chan_get_tag(ch) == TAG_IRC) {	/* IRC tab */
		/* userdata is session * */
		mg_link_irctab(chan_get_userdata(ch), 1);
		return;
	}
#warning "xchat->ekg2, removed support for generic tabs"
}

static void mg_destroy_tab_cb(GtkWidget *item, chan * ch) {
	/* treat it just like the X button press */
	mg_xbutton_cb(mg_gui->chanview, ch, chan_get_tag(ch), chan_get_userdata(ch));
}

static int key_action_insert(GtkWidget *wid, char *buf) {
	int tmp_pos;

	if (!buf)
		return 1;

	tmp_pos = gtk_editable_get_position(GTK_EDITABLE(wid));
	gtk_editable_insert_text(GTK_EDITABLE(wid), buf, strlen(buf), &tmp_pos);
	gtk_editable_set_position(GTK_EDITABLE(wid), tmp_pos);
	return 2;
}

static void mg_color_insert(GtkWidget *item, gpointer userdata) {
	int num = GPOINTER_TO_INT(userdata);
	char buf[3];

	if (!num) {
		printf("mg_color_insert() stub!\n");
		return;
	}

	switch (num) {
		case 2:
		case 20:
		case 31:
			buf[0] = num;
			buf[1] = '\0';
			break;

		default:		/* kolorki -> Ctrl-R + kolorek */
			buf[0] = 18;
			buf[1] = num;
			buf[2] = '\0';
	}

	key_action_insert(window_current->gui->input_box, buf);
}

static void mg_markup_item(GtkWidget *menu, char *text, int arg) {
	GtkWidget *item;

	item = gtk_menu_item_new_with_label("");
	gtk_label_set_markup(GTK_LABEL(GTK_BIN(item)->child), text);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(mg_color_insert), GINT_TO_POINTER(arg));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);
}

GtkWidget * mg_submenu(GtkWidget *menu, char *text) {
	GtkWidget *submenu, *item;

	item = gtk_menu_item_new_with_mnemonic(text);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	gtk_widget_show(submenu);

	return submenu;
}

static void mg_create_color_menu(GtkWidget *menu, window_t *sess) {
	GtkWidget *submenu;
	GtkWidget *subsubmenu;
	char buf[256];
	int i;

	submenu = mg_submenu(menu, "Wstaw atrybut lub kolorek");

	mg_markup_item(submenu, "<b>Pogrubiony</b>", 2);		/* Ctrl-B */
	mg_markup_item(submenu, "<u>Podkreslony</u>", 31);		/* Ctrl-_ */
	mg_markup_item(submenu, "<i>Kursywa</i>", 20);			/* Ctrl-T */
	mg_markup_item(submenu, "Normalny", 0);				/* XXX XXX */

	subsubmenu = mg_submenu(submenu, "Podstawowe kolory:");

	for (i = 0; i < 16; i++) {
		sprintf(buf, "<tt><sup>%c</sup> <span background=\"#%02x%02x%02x\">"
			"   </span></tt>",
			default_color_map[i].color, default_color_map[i].r, default_color_map[i].g, default_color_map[i].b);
		mg_markup_item(subsubmenu, buf, default_color_map[i].color);
	}

	subsubmenu = mg_submenu(submenu, "Kolory gg:");

	for (i = 16; i < 26; i++) {
		sprintf(buf, "<tt><sup>%c</sup> <span background=\"#%02x%02x%02x\">"
			"   </span></tt>",
			default_color_map[i].color, default_color_map[i].r, default_color_map[i].g, default_color_map[i].b);
		mg_markup_item(subsubmenu, buf, default_color_map[i].color);
	}
}

static gboolean mg_tab_contextmenu_cb(chanview * cv, chan * ch, int tag, gpointer ud, GdkEventButton * event) {
	GtkWidget *menu, *item;
	window_t *sess = ud;

	/* shift-click to close a tab */
	if ((event->state & GDK_SHIFT_MASK) && event->type == GDK_BUTTON_PRESS) {
		mg_xbutton_cb(cv, ch, tag, ud);
		return FALSE;
	}

	if (event->button != 3)
		return FALSE;

	menu = gtk_menu_new();

	if (tag == TAG_IRC) {
		char buf[256];

		const char *w_target = gtk_window_target(sess);
		char *target = g_markup_escape_text(w_target[0] ? w_target : "<none>", -1);

		snprintf(buf, sizeof(buf), "<span foreground=\"#3344cc\"><b>%s</b></span>", target);
		g_free(target);

		item = gtk_menu_item_new_with_label("");
		gtk_label_set_markup(GTK_LABEL(GTK_BIN(item)->child), buf);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

#if 0
		/* separator */
		item = gtk_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

		menu_toggle_item(_("Beep on message"), menu, mg_beepmsg_cb, sess, sess->beep);
		if (prefs.gui_tray)
			menu_toggle_item(_("Blink tray on message"), menu, mg_traymsg_cb, sess,
					 sess->tray);
		if (sess->type == SESS_CHANNEL)
			menu_toggle_item(_("Show join/part messages"), menu, mg_hidejp_cb,
					 sess, !sess->hide_join_part);
#endif

	}
	/* separator */
	item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	mg_create_icon_item("_Zamknij okno", GTK_STOCK_CLOSE, menu, mg_destroy_tab_cb, ch);
	mg_create_icon_item("_Okno plywajace", GTK_STOCK_REDO, menu, mg_detach_tab_cb, ch);
#if 0

	if (sess && tabmenu_list)
		menu_create(menu, tabmenu_list, sess->channel, FALSE);
	menu_add_plugin_items(menu, "\x4$TAB", sess->channel);
#endif

	if (event->window)
		gtk_menu_set_screen(GTK_MENU(menu), gdk_drawable_get_screen(event->window));
	g_object_ref(menu);
	g_object_ref_sink(menu);
	g_object_unref(menu);
	g_signal_connect(G_OBJECT(menu), "selection-done", G_CALLBACK(mg_menu_destroy), NULL);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, event->time);
	return TRUE;
}

/* add a tabbed channel */

static void mg_add_chan(window_t *sess) {
	GdkPixbuf *icon = NULL;	/* pix_channel || pix_server || pix_dialog */

	gtk_private(sess)->tab = chanview_add(gtk_private_ui(sess)->chanview, gtk_window_target(sess),	/* sess->session, */
						  sess, FALSE, TAG_IRC, icon);
	if (plain_list == NULL)
		mg_create_tab_colors();
	chan_set_color(gtk_private(sess)->tab, plain_list);

	if (gtk_private(sess)->buffer == NULL) {
		gtk_private(sess)->buffer =
			gtk_xtext_buffer_new(GTK_XTEXT(gtk_private_ui(sess)->xtext));
		gtk_xtext_set_time_stamp(gtk_private(sess)->buffer, config_timestamp_show);
		gtk_private(sess)->user_model = userlist_create_model();
	}
}

#if 0

/* mg_userlist_button() do przemyslenia */
/* mg_create_userlistbuttons() */

static void mg_topic_cb(GtkWidget *entry, gpointer userdata) {
	session *sess = current_sess;
	char *text;

	if (sess->channel[0] && sess->server->connected && sess->type == SESS_CHANNEL) {
		text = GTK_ENTRY(entry)->text;
		if (text[0] == 0)
			text = NULL;
		sess->server->p_topic(sess->server, sess->channel, text);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "");
	/* restore focus to the input widget, where the next input will most
	   likely be */
	gtk_widget_grab_focus(sess->gui->input_box);
}

#endif

static void mg_tabwindow_kill_cb(GtkWidget *win, gpointer userdata) {
#if 0
	GSList *list, *next;
	session *sess;

/*	puts("enter mg_tabwindow_kill_cb");*/
	xchat_is_quitting = TRUE;

	/* see if there's any non-tab windows left */
	list = sess_list;
	while (list) {
		sess = list->data;
		next = list->next;
		if (!sess->gui->is_tab) {
			xchat_is_quitting = FALSE;
/*			puts("-> will not exit, some toplevel windows left");*/
		} else {
			mg_ircdestroy(sess);
		}
		list = next;
	}

	current_tab = NULL;
	active_tab = NULL;
	mg_gui = NULL;
	parent_window = NULL;
#endif
}

static GtkWidget *mg_changui_destroy(window_t *sess) {
	GtkWidget *ret = NULL;

	if (gtk_private_ui(sess)->is_tab) {
		/* avoid calling the "destroy" callback */
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtk_private_ui(sess)->window),
						     mg_tabwindow_kill_cb, 0);
		/* remove the tab from the chanview */
		if (!mg_chan_remove(gtk_private(sess)->tab))
			/* if the window still exists, restore the signal handler */
			g_signal_connect(G_OBJECT(gtk_private_ui(sess)->window), "destroy",
					 G_CALLBACK(mg_tabwindow_kill_cb), 0);
	} else {
		/* avoid calling the "destroy" callback */
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtk_private_ui(sess)->window),
						     mg_topdestroy_cb, sess);
		/*gtk_widget_destroy (sess->gui->window); */
		/* don't destroy until the new one is created. Not sure why, but */
		/* it fixes: Gdk-CRITICAL **: gdk_colormap_get_screen: */
		/*           assertion `GDK_IS_COLORMAP (cmap)' failed */
		ret = gtk_private_ui(sess)->window;
		free(gtk_private_ui(sess));
		gtk_private(sess)->gui = NULL;
	}
	return ret;
}

static void mg_link_irctab(window_t *sess, int focus) {
	GtkWidget *win;

	if (gtk_private_ui(sess)->is_tab) {
		win = mg_changui_destroy(sess);
		mg_changui_new(sess, 0, focus);
		mg_populate(sess);
#if 0
		xchat_is_quitting = FALSE;
#endif
		if (win)
			gtk_widget_destroy(win);
		return;
	}

	win = mg_changui_destroy(sess);
	mg_changui_new(sess, 1, focus);
	/* the buffer is now attached to a different widget */
	((xtext_buffer *) sess->buffer)->xtext = (GtkXText *) sess->gui->xtext;
	if (win)
		gtk_widget_destroy(win);
}

static void mg_detach(window_t *sess, int mode) {
	switch (mode) {
	/* detach only */
		case 1:
			if (sess->gui->is_tab)
				mg_link_irctab(sess, 1);
			break;
	/* attach only */
		case 2:
			if (!sess->gui->is_tab)
				mg_link_irctab(sess, 1);
			break;
	/* toggle */
		default:
			mg_link_irctab(sess, 1);
	}
}

static void mg_apply_entry_style(GtkWidget *entry) {
	gtk_widget_modify_base(entry, GTK_STATE_NORMAL, &colors[COL_BG]);
	gtk_widget_modify_text(entry, GTK_STATE_NORMAL, &colors[COL_FG]);
	gtk_widget_modify_font(entry, input_style->font_desc);
}

#if 0

static void mg_dialog_button_cb(GtkWidget *wid, char *cmd) {
	/* the longest cmd is 12, and the longest nickname is 64 */
	char buf[128];
	char *host = "";
	char *topic;

	topic = (char *)(GTK_ENTRY(gtk_private(window_current)->gui->topic_entry)->text);
	topic = strrchr(topic, '@');
	if (topic)
		host = topic + 1;

	auto_insert(buf, sizeof(buf), cmd, 0, 0, "", "", "",
		    server_get_network(current_sess->server, TRUE), host, "",
		    current_sess->channel);

	handle_command(current_sess, buf, TRUE);

	/* dirty trick to avoid auto-selection */
	SPELL_ENTRY_SET_EDITABLE(current_sess->gui->input_box, FALSE);
	gtk_widget_grab_focus(current_sess->gui->input_box);
	SPELL_ENTRY_SET_EDITABLE(current_sess->gui->input_box, TRUE);
}

static void mg_dialog_button(GtkWidget *box, char *name, char *cmd) {
	GtkWidget *wid;

	wid = gtk_button_new_with_label(name);
	gtk_box_pack_start(GTK_BOX(box), wid, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(wid), "clicked", G_CALLBACK(mg_dialog_button_cb), cmd);
	gtk_widget_set_size_request(wid, -1, 0);
}

static void mg_create_dialogbuttons(GtkWidget *box) {
	struct popup *pop;
	GSList *list = dlgbutton_list;

	while (list) {
		pop = list->data;
		if (pop->cmd[0])
			mg_dialog_button(box, pop->name, pop->cmd);
		list = list->next;
	}
}

#endif

static void
mg_create_topicbar(window_t *sess, GtkWidget *box)
{
	GtkWidget *hbox, *topic, *bbox;
	gtk_window_ui_t *gui = gtk_private_ui(sess);

	gui->topic_bar = hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, 0, 0, 0);

	if (!gui->is_tab)
		gtk_private(sess)->tab = NULL;

	gui->topic_entry = topic = gtk_entry_new();
	gtk_widget_set_name(topic, "xchat-inputbox");
	gtk_container_add(GTK_CONTAINER(hbox), topic);
#if 0
	g_signal_connect(G_OBJECT(topic), "activate", G_CALLBACK(mg_topic_cb), 0);
#endif

	if (style_inputbox_config)
		mg_apply_entry_style(topic);

	gui->topicbutton_box = bbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), bbox, 0, 0, 0);

	gui->dialogbutton_box = bbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), bbox, 0, 0, 0);
#if 0
	mg_create_dialogbuttons(bbox);
#endif

	if (!paned_userlist_config)
		gtkutil_button(hbox, GTK_STOCK_GOTO_LAST, "Pokaz/Ukryj list kontaktow",
			       mg_userlist_toggle_cb, 0, 0);
}

/* check if a word is clickable */

static int
mg_word_check(GtkWidget *xtext, char *word, int len)
{
#warning "xchat->ekg2: mg_word_check() nice functionality XXX"
	return 0;
}

/* mouse click inside text area */

static void
mg_word_clicked(GtkWidget *xtext, char *word, GdkEventButton * even)
{
#warning "xchat->ekg2: mg_word_clicked() nice functionality XXX"
}

void
mg_update_xtext(GtkWidget *wid)
{
	GtkXText *xtext = GTK_XTEXT(wid);

	gtk_xtext_set_palette(xtext, colors);
	gtk_xtext_set_max_lines(xtext, config_backlog_size);
	gtk_xtext_set_tint(xtext, tint_red_config, tint_green_config, tint_blue_config);
//      gtk_xtext_set_background (xtext, channelwin_pix, transparent_config);
	gtk_xtext_set_wordwrap(xtext, wordwrap_config);
	gtk_xtext_set_show_marker(xtext, show_marker_config);
	gtk_xtext_set_show_separator(xtext, indent_nicks_config ? show_separator_config : 0);
	gtk_xtext_set_indent(xtext, indent_nicks_config);

	if (!gtk_xtext_set_font(xtext, font_normal_config)) {
		printf("Failed to open any font. I'm out of here!");	/* FE_MSG_WAIT | FE_MSG_ERROR */
		exit(1);
	}

	gtk_xtext_refresh(xtext, FALSE);
}

/* handle errors reported by xtext */

static void mg_xtext_error(int type) {
	printf("mg_xtext_error() %d\n", type);

	/* @ type == 0 "Unable to set transparent background!\n\n"
	 *              "You may be using a non-compliant window\n"
	 *              "manager that is not currently supported.\n"), FE_MSG_WARN);
	 *
	 *              config_transparent = 0; 
	 */
}

static void mg_create_textarea(window_t *sess, GtkWidget *box) {
	GtkWidget *inbox, *vbox, *frame;
	GtkXText *xtext;
	gtk_window_ui_t *gui = gtk_private_ui(sess);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(box), vbox);

	inbox = gtk_hbox_new(FALSE, SCROLLBAR_SPACING);
	gtk_container_add(GTK_CONTAINER(vbox), inbox);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(inbox), frame);

	gui->xtext = gtk_xtext_new(colors, TRUE);
	xtext = GTK_XTEXT(gui->xtext);
	gtk_xtext_set_max_indent(xtext, max_auto_indent_config);
	gtk_xtext_set_thin_separator(xtext, thin_separator_config);
	gtk_xtext_set_error_function(xtext, mg_xtext_error);
	gtk_xtext_set_urlcheck_function(xtext, mg_word_check);
	gtk_xtext_set_max_lines(xtext, config_backlog_size);
	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(xtext));
	mg_update_xtext(GTK_WIDGET(xtext));

	g_signal_connect(G_OBJECT(xtext), "word_click", G_CALLBACK(mg_word_clicked), NULL);
	gui->vscrollbar = gtk_vscrollbar_new(GTK_XTEXT(xtext)->adj);
	gtk_box_pack_start(GTK_BOX(inbox), gui->vscrollbar, FALSE, TRUE, 0);

#warning "xchat->ekg2: g_signal_connect() \"drag_begin\", \"drag_drop\", \"drag_motion\", \"drag_end\", \"drag_data_received\" && gtk_drag_dest_set() do zaimplementowania"
}

static void mg_create_userlist(gtk_window_ui_t *gui, GtkWidget *box) {
	GtkWidget *frame, *ulist, *vbox;

	vbox = gtk_vbox_new(0, 1);
	gtk_container_add(GTK_CONTAINER(box), vbox);

	frame = gtk_frame_new(NULL);
	if (!(gui_tweaks_config & 1))
		gtk_box_pack_start(GTK_BOX(vbox), frame, 0, 0, GUI_SPACING);

	gui->namelistinfo = gtk_label_new(NULL);
	gtk_container_add(GTK_CONTAINER(frame), gui->namelistinfo);

	gui->user_tree = ulist = userlist_create(vbox);
#if 0
	if (prefs.style_namelistgad) {
		gtk_widget_set_style(ulist, input_style);
		gtk_widget_modify_base(ulist, GTK_STATE_NORMAL, &colors[COL_BG]);
	}
#endif
}

static void mg_leftpane_cb(GtkPaned * pane, GParamSpec * param, gtk_window_ui_t* gui) {
	gui_pane_left_size_config = gtk_paned_get_position(pane);
}

static void mg_rightpane_cb(GtkPaned * pane, GParamSpec * param, gtk_window_ui_t* gui) {
	int handle_size;

/*	if (pane->child1 == NULL || (!GTK_WIDGET_VISIBLE (pane->child1)))
		return;
	if (pane->child2 == NULL || (!GTK_WIDGET_VISIBLE (pane->child2)))
		return;*/

	gtk_widget_style_get(GTK_WIDGET(pane), "handle-size", &handle_size, NULL);

	/* record the position from the RIGHT side */
	gui_pane_right_size_config =
		GTK_WIDGET(pane)->allocation.width - gtk_paned_get_position(pane) - handle_size;
}

static gboolean mg_add_pane_signals(gtk_window_ui_t* gui) {
	g_signal_connect(G_OBJECT(gui->hpane_right), "notify::position", G_CALLBACK(mg_rightpane_cb), gui);
	g_signal_connect(G_OBJECT(gui->hpane_left), "notify::position", G_CALLBACK(mg_leftpane_cb), gui);
	return FALSE;
}

static void mg_create_center(window_t *sess, gtk_window_ui_t *gui, GtkWidget *box) {
	GtkWidget *vbox, *hbox, *book;

	/* sep between top and bottom of left side */
	gui->vpane_left = gtk_vpaned_new();

	/* sep between top and bottom of right side */
	gui->vpane_right = gtk_vpaned_new();

	/* sep between left and xtext */
	gui->hpane_left = gtk_hpaned_new();
	gtk_paned_set_position(GTK_PANED(gui->hpane_left), gui_pane_left_size_config);

	/* sep between xtext and right side */
	gui->hpane_right = gtk_hpaned_new();

	if (gui_tweaks_config & 4) {
		gtk_paned_pack2(GTK_PANED(gui->hpane_left), gui->vpane_left, FALSE, TRUE);
		gtk_paned_pack1(GTK_PANED(gui->hpane_left), gui->hpane_right, TRUE, TRUE);
	} else {
		gtk_paned_pack1(GTK_PANED(gui->hpane_left), gui->vpane_left, FALSE, TRUE);
		gtk_paned_pack2(GTK_PANED(gui->hpane_left), gui->hpane_right, TRUE, TRUE);
	}
	gtk_paned_pack2(GTK_PANED(gui->hpane_right), gui->vpane_right, FALSE, TRUE);

	gtk_container_add(GTK_CONTAINER(box), gui->hpane_left);

	gui->note_book = book = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(book), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(book), FALSE);
	gtk_paned_pack1(GTK_PANED(gui->hpane_right), book, TRUE, TRUE);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_paned_pack1(GTK_PANED(gui->vpane_right), hbox, FALSE, TRUE);
	mg_create_userlist(gui, hbox);

	gui->user_box = hbox;

	vbox = gtk_vbox_new(FALSE, 3);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox, NULL);

	mg_create_topicbar(sess, vbox);
	mg_create_textarea(sess, vbox);
	mg_create_entry(sess, vbox);

	g_idle_add ((GSourceFunc)mg_add_pane_signals, gui);
}

/* make sure chanview and userlist positions are sane */

static void mg_sanitize_positions(int *cv, int *ul) {
	if (tab_layout_config == 2) {
		/* treeview can't be on TOP or BOTTOM */
		if (*cv == POS_TOP || *cv == POS_BOTTOM)
			*cv = POS_TOPLEFT;
	}

	/* userlist can't be on TOP or BOTTOM */
	if (*ul == POS_TOP || *ul == POS_BOTTOM)
		*ul = POS_TOPRIGHT;

	/* can't have both in the same place */
	if (*cv == *ul) {
		*cv = POS_TOPRIGHT;
		if (*ul == POS_TOPRIGHT)
			*cv = POS_BOTTOMRIGHT;
	}
}

static void mg_place_userlist_and_chanview_real(gtk_window_ui_t *gui, GtkWidget *userlist, GtkWidget *chanview) {
	int unref_userlist = FALSE;
	int unref_chanview = FALSE;

	/* first, remove userlist/treeview from their containers */
	if (userlist && userlist->parent) {
		g_object_ref(userlist);
		gtk_container_remove(GTK_CONTAINER(userlist->parent), userlist);
		unref_userlist = TRUE;
	}

	if (chanview && chanview->parent) {
		g_object_ref(chanview);
		gtk_container_remove(GTK_CONTAINER(chanview->parent), chanview);
		unref_chanview = TRUE;
	}

	if (chanview) {
		gtk_table_set_row_spacing(GTK_TABLE(gui->main_table), 1, 0);
		gtk_table_set_row_spacing(GTK_TABLE(gui->main_table), 2, 2);

		/* then place them back in their new positions */
		switch (tab_pos_config) {
		case POS_TOPLEFT:
			gtk_paned_pack1(GTK_PANED(gui->vpane_left), chanview, FALSE, TRUE);
			break;
		case POS_BOTTOMLEFT:
			gtk_paned_pack2(GTK_PANED(gui->vpane_left), chanview, FALSE, TRUE);
			break;
		case POS_TOPRIGHT:
			gtk_paned_pack1(GTK_PANED(gui->vpane_right), chanview, FALSE, TRUE);
			break;
		case POS_BOTTOMRIGHT:
			gtk_paned_pack2(GTK_PANED(gui->vpane_right), chanview, FALSE, TRUE);
			break;
		case POS_TOP:
			gtk_table_set_row_spacing(GTK_TABLE(gui->main_table), 1, GUI_SPACING - 1);
			gtk_table_attach(GTK_TABLE(gui->main_table), chanview,
					 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
			break;
		case POS_HIDDEN:
			break;
		default:	/* POS_BOTTOM */
			gtk_table_set_row_spacing(GTK_TABLE(gui->main_table), 2, 3);
			gtk_table_attach(GTK_TABLE(gui->main_table), chanview,
					 1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
		}
	}

	if (userlist) {
		switch (gui_ulist_pos_config) {
		case POS_TOPLEFT:
			gtk_paned_pack1(GTK_PANED(gui->vpane_left), userlist, FALSE, TRUE);
			break;
		case POS_BOTTOMLEFT:
			gtk_paned_pack2(GTK_PANED(gui->vpane_left), userlist, FALSE, TRUE);
			break;
		case POS_BOTTOMRIGHT:
			gtk_paned_pack2(GTK_PANED(gui->vpane_right), userlist, FALSE, TRUE);
			break;
		case POS_HIDDEN:
			break;
		default:	/* POS_TOPRIGHT */
			gtk_paned_pack1(GTK_PANED(gui->vpane_right), userlist, FALSE, TRUE);
		}
	}

	if (unref_chanview)
		g_object_unref(chanview);
	if (unref_userlist)
		g_object_unref(userlist);

	mg_hide_empty_boxes(gui);
}

static void mg_place_userlist_and_chanview(gtk_window_ui_t *gui) {
	GtkOrientation orientation;
	GtkWidget *chanviewbox = NULL;
	int pos;

	mg_sanitize_positions(&tab_pos_config, &gui_ulist_pos_config);

	if (gui->chanview) {
		pos = tab_pos_config;

		orientation = chanview_get_orientation(gui->chanview);
		if ((pos == POS_BOTTOM || pos == POS_TOP)
		    && orientation == GTK_ORIENTATION_VERTICAL)
			chanview_set_orientation(gui->chanview, FALSE);
		else if ((pos == POS_TOPLEFT || pos == POS_BOTTOMLEFT || pos == POS_TOPRIGHT
			  || pos == POS_BOTTOMRIGHT) && orientation == GTK_ORIENTATION_HORIZONTAL)
			chanview_set_orientation(gui->chanview, TRUE);
		chanviewbox = chanview_get_box(gui->chanview);
	}

	mg_place_userlist_and_chanview_real(gui, gui->user_box, chanviewbox);
}

void mg_change_layout(int type) {
	if (mg_gui) {
		/* put tabs at the bottom */
		if (type == 0 && tab_pos_config != POS_BOTTOM && tab_pos_config != POS_TOP)
			tab_pos_config = POS_BOTTOM;

		mg_place_userlist_and_chanview(mg_gui);
		chanview_set_impl(mg_gui->chanview, type);
	}
}

static void mg_inputbox_rightclick(GtkEntry * entry, GtkWidget *menu) {
	mg_create_color_menu(menu, NULL);
}

static void mg_create_entry(window_t *sess, GtkWidget *box) {
	GtkWidget *hbox, *entry;
	gtk_window_ui_t *gui = gtk_private_ui(sess);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, 0, 0, 0);

	gui->nick_box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gui->nick_box, 0, 0, 0);

	gui->nick_label = gtk_label_new(itoa(config_uin));
	gtk_box_pack_end(GTK_BOX(gui->nick_box), gui->nick_label, 0, 0, 0);

	gui->input_box = entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(gui->input_box), 2048);
	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(mg_inputbox_cb), gui);

	gtk_container_add(GTK_CONTAINER(hbox), entry);

	gtk_widget_set_name(entry, "xchat-inputbox");

	g_signal_connect(G_OBJECT(entry), "key_press_event", G_CALLBACK(key_handle_key_press), NULL);

	g_signal_connect(G_OBJECT(entry), "focus_in_event", G_CALLBACK(mg_inputbox_focus), gui);
	g_signal_connect(G_OBJECT(entry), "populate_popup", G_CALLBACK(mg_inputbox_rightclick), NULL);

	gtk_widget_grab_focus(entry);

	if (style_inputbox_config)
		mg_apply_entry_style(entry);
}

static void mg_switch_tab_cb(chanview *cv, chan *ch, int tag, gpointer ud) {
	chan *old;
	window_t *sess = ud;

	old = active_tab;
	active_tab = ch;

	if (active_tab != old) {
		mg_populate(sess);

		ui_gtk_window_switch(sess->id);
	}
}

/* compare two tabs (for tab sorting function) */

static int mg_tabs_compare(window_t *a, window_t *b) {	/* it's lik: window_new_compare() */
	return (a->id - b->id);
}

static void mg_create_tabs(gtk_window_ui_t *gui) {
	gui->chanview = chanview_new(tab_layout_config, truncchans_config,
				     tab_sort_config, tab_icons_config,
				     style_namelistgad_config ? input_style : NULL);
	chanview_set_callbacks(gui->chanview, mg_switch_tab_cb, mg_xbutton_cb,
			       mg_tab_contextmenu_cb, (void *) mg_tabs_compare);
	mg_place_userlist_and_chanview(gui);
}

static gboolean mg_tabwin_focus_cb(GtkWindow * win, GdkEventFocus * event, gpointer userdata) {
#if 0
	current_sess = current_tab;
	if (current_sess) {
		gtk_xtext_check_marker_visibility(GTK_XTEXT(current_sess->gui->xtext));
		plugin_emit_dummy_print(current_sess, "Focus Window");
	}
#endif
#ifdef USE_XLIB
	unflash_window(GTK_WIDGET(win));
#endif
	return FALSE;
}

static gboolean mg_topwin_focus_cb(GtkWindow * win, GdkEventFocus * event, window_t *sess) {
#if 0
	current_sess = sess;
	if (!sess->server->server_session)
		sess->server->server_session = sess;
	gtk_xtext_check_marker_visibility(GTK_XTEXT(current_sess->gui->xtext));
#ifdef USE_XLIB
	unflash_window(GTK_WIDGET(win));
#endif
	plugin_emit_dummy_print(sess, "Focus Window");
#endif
	return FALSE;
}

static void mg_create_menu(gtk_window_ui_t *gui, GtkWidget *table, int away_state) {
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(table)), accel_group);
	g_object_unref(accel_group);

	gui->menu = menu_create_main(accel_group, TRUE, away_state, !gui->is_tab, gui->menu_item);
	gtk_table_attach(GTK_TABLE(table), gui->menu, 0, 3, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
}

static void mg_create_irctab(window_t *sess, GtkWidget *table) {
	GtkWidget *vbox;
	gtk_window_ui_t *gui = gtk_private_ui(sess);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	mg_create_center(sess, gui, vbox);
}

static void mg_create_topwindow(window_t *sess) {
	GtkWidget *win;
	GtkWidget *table;

	win = gtkutil_window_new("ekg2", NULL, mainwindow_width_config,
				 mainwindow_height_config, 0);

	gtk_private_ui(sess)->window = win;
	gtk_container_set_border_width(GTK_CONTAINER(win), GUI_BORDER);

	g_signal_connect(G_OBJECT(win), "focus_in_event", G_CALLBACK(mg_topwin_focus_cb), sess);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(mg_topdestroy_cb), sess);
	g_signal_connect(G_OBJECT(win), "configure_event", G_CALLBACK(mg_configure_cb), sess);

	palette_alloc(win);

	table = gtk_table_new(4, 3, FALSE);
	/* spacing under the menubar */
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, GUI_SPACING);
	/* left and right borders */
	gtk_table_set_col_spacing(GTK_TABLE(table), 0, 1);
	gtk_table_set_col_spacing(GTK_TABLE(table), 1, 1);
	gtk_container_add(GTK_CONTAINER(win), table);

	mg_create_irctab(sess, table);
	/* vvvvv sess->server->is_away */
	mg_create_menu(gtk_private_ui(sess), table, 0);

	if (gtk_private(sess)->buffer == NULL) {
		gtk_private(sess)->buffer =
			gtk_xtext_buffer_new(GTK_XTEXT(gtk_private_ui(sess)->xtext));
		gtk_xtext_buffer_show(GTK_XTEXT(gtk_private_ui(sess)->xtext),
				      gtk_private(sess)->buffer, TRUE);
		gtk_xtext_set_time_stamp(gtk_private(sess)->buffer, config_timestamp_show);
		sess->user_model = userlist_create_model();
	}
	userlist_show(sess);

	gtk_widget_show_all(table);

	if (hidemenu_config)
		gtk_widget_hide(gtk_private_ui(sess)->menu);

	if (!topicbar_config)
		gtk_widget_hide(gtk_private_ui(sess)->topic_bar);

	if (gui_tweaks_config & 2)
		gtk_widget_hide(gtk_private_ui(sess)->nick_box);

	mg_decide_userlist(sess, FALSE);

#if DARK
	if (sess->type == SESS_DIALOG) {
		/* hide the chan-mode buttons */
		gtk_widget_hide(sess->gui->topicbutton_box);
	} else {
		gtk_widget_hide(sess->gui->dialogbutton_box);

		if (!prefs.chanmodebuttons)
			gtk_widget_hide(sess->gui->topicbutton_box);
	}
#endif

	mg_place_userlist_and_chanview(gtk_private_ui(sess));

	gtk_widget_show(win);
}

static gboolean mg_tabwindow_de_cb(GtkWidget *widget, GdkEvent * event, gpointer user_data) {
#if 0
	if ((gui_tray_flags_config & 1) && tray_toggle_visibility(FALSE))
		return TRUE;

	/* check for remaining toplevel windows */
	list = sess_list;
	while (list) {
		sess = list->data;
		if (!sess->gui->is_tab)
			return FALSE;
		list = list->next;
	}
#endif

	mg_open_quit_dialog(TRUE);
	return TRUE;
}

static void mg_create_tabwindow(window_t *sess) {
	GtkWidget *win;
	GtkWidget *table;

	win = gtkutil_window_new("ekg2", NULL, mainwindow_width_config, mainwindow_height_config,
				 0);

	gtk_private_ui(sess)->window = win;
	gtk_window_move(GTK_WINDOW(win), mainwindow_left_config, mainwindow_top_config);

	if (gui_win_state_config)
		gtk_window_maximize(GTK_WINDOW(win));

	gtk_container_set_border_width(GTK_CONTAINER(win), GUI_BORDER);

	g_signal_connect(G_OBJECT(win), "delete_event", G_CALLBACK(mg_tabwindow_de_cb), 0);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(mg_tabwindow_kill_cb), 0);
	g_signal_connect(G_OBJECT(win), "focus_in_event", G_CALLBACK(mg_tabwin_focus_cb), NULL);
	g_signal_connect(G_OBJECT(win), "configure_event", G_CALLBACK(mg_configure_cb), NULL);
	g_signal_connect(G_OBJECT(win), "window_state_event", G_CALLBACK(mg_windowstate_cb), NULL);

	palette_alloc(win);

	gtk_private_ui(sess)->main_table = table = gtk_table_new(4, 3, FALSE);
	/* spacing under the menubar */
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, GUI_SPACING);
	/* left and right borders */
	gtk_table_set_col_spacing(GTK_TABLE(table), 0, 1);
	gtk_table_set_col_spacing(GTK_TABLE(table), 1, 1);
	gtk_container_add(GTK_CONTAINER(win), table);

	mg_create_irctab(sess, table);
	mg_create_tabs(gtk_private_ui(sess));
						/* vvvvvv sess->server->is_away */
	mg_create_menu(gtk_private_ui(sess), table, 0);

	mg_focus(sess);

	gtk_widget_show_all(table);

	if (hidemenu_config)
		gtk_widget_hide(gtk_private_ui(sess)->menu);

	mg_decide_userlist(sess, FALSE);

	if (!topicbar_config)
		gtk_widget_hide(gtk_private_ui(sess)->topic_bar);

	if (!chanmodebuttons_config)
		gtk_widget_hide(gtk_private_ui(sess)->topicbutton_box);

	if (gui_tweaks_config & 2)
		gtk_widget_hide(gtk_private_ui(sess)->nick_box);

	mg_place_userlist_and_chanview(gtk_private_ui(sess));
	gtk_widget_show(win);
}

void mg_apply_setup(void) {
	int done_main = FALSE;
	list_t l;

	mg_create_tab_colors();

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		gtk_xtext_set_time_stamp(gtk_private(w)->buffer, config_timestamp_show);
		((xtext_buffer *) gtk_private(w)->buffer)->needs_recalc = TRUE;

		if (!gtk_private_ui(w)->is_tab || !done_main)
			mg_place_userlist_and_chanview(gtk_private_ui(w));

		if (gtk_private_ui(w)->is_tab)
			done_main = TRUE;
	}
}

#if 0
static chan *
mg_add_generic_tab(char *name, char *title, void *family, GtkWidget *box)
{
	chan *ch;

	gtk_notebook_append_page(GTK_NOTEBOOK(mg_gui->note_book), box, NULL);
	gtk_widget_show(box);

	ch = chanview_add(mg_gui->chanview, name, NULL, box, TRUE, TAG_UTIL, pix_util);
	chan_set_color(ch, plain_list);
	/* FIXME: memory leak */
	g_object_set_data(G_OBJECT(box), "title", strdup(title));
	g_object_set_data(G_OBJECT(box), "ch", ch);

	if (prefs.newtabstofront)
		chan_focus(ch);

	return ch;
}
#endif

#if 0

void
fe_clear_channel(window_t *sess)
{
	char tbuf[CHANLEN + 6];
	gtk_window_ui_t *gui = gtk_private(sess);

	if (gui->is_tab) {
		if (sess->waitchannel[0]) {
			if (prefs.truncchans > 2
			    && g_utf8_strlen(sess->waitchannel, -1) > prefs.truncchans) {
				/* truncate long channel names */
				tbuf[0] = '(';
				strcpy(tbuf + 1, sess->waitchannel);
				g_utf8_offset_to_pointer(tbuf, prefs.truncchans)[0] = 0;
				strcat(tbuf, "..)");
			} else {
				sprintf(tbuf, "(%s)", sess->waitchannel);
			}
		} else
			strcpy(tbuf, _("<none>"));
		chan_rename(sess->res->tab, tbuf, prefs.truncchans);
	}

	if (!gui->is_tab || sess == current_tab) {
		gtk_entry_set_text(GTK_ENTRY(gui->topic_entry), "");

		if (gui->op_xpm) {
			gtk_widget_destroy(gui->op_xpm);
			gui->op_xpm = 0;
		}
	} else {
	}
}

void
fe_dlgbuttons_update(window_t *sess)
{
	GtkWidget *box;
	gtk_window_ui_t *gui = gtk_private(sess);

	gtk_widget_destroy(gui->dialogbutton_box);

	gui->dialogbutton_box = box = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(gui->topic_bar), box, 0, 0, 0);
	gtk_box_reorder_child(GTK_BOX(gui->topic_bar), box, 3);
	mg_create_dialogbuttons(box);

	gtk_widget_show_all(box);

	if (current_tab && current_tab->type != SESS_DIALOG)
		gtk_widget_hide(current_tab->gui->dialogbutton_box);
}

/* fe_set_nick() nieciekawe */

#endif

void fe_set_away(void) {
	list_t l;

	for (l = windows; l; l = l->next) {
#if DARK
		window_t *w = l->data;

		if (!sess->gui->is_tab || sess == current_tab) {
			GTK_CHECK_MENU_ITEM(sess->gui->menu_item[MENU_ID_AWAY])->active =
				serv->is_away;
			/* gray out my nickname */
			mg_set_myself_away(sess->gui, serv->is_away);
		}
#endif
	}
}

void fe_set_channel(window_t *sess) {
	if (gtk_private(sess)->tab != NULL)
		chan_rename(gtk_private(sess)->tab, gtk_window_target(sess), truncchans_config);
}

void mg_changui_new(window_t *sess, int tab, int focus) {
	int first_run = FALSE;
	gtk_window_ui_t *gui;

#if DARK
	struct User *user = NULL;

	if (!sess->server->front_session)
		sess->server->front_session = sess;

	if (!is_channel(sess->server, sess->channel))
		user = userlist_find_global(sess->server, sess->channel);
#endif
	if (!tab) {
		gui = xmalloc(sizeof(gtk_window_ui_t));
		gui->is_tab = FALSE;

		sess->gui = gui;
		mg_create_topwindow(sess);
		fe_set_title(sess);
#if DARK
		if (user && user->hostname)
			set_topic(sess, user->hostname);
#endif
		return;
	}

	if (mg_gui == NULL) {
		first_run = TRUE;
		gui = &static_mg_gui;
		memset(gui, 0, sizeof(gtk_window_ui_t));
		gui->is_tab = TRUE;
		sess->gui = gui;
		mg_create_tabwindow(sess);
		mg_gui = gui;
		parent_window = gui->window;
	} else {
		sess->gui = gui = mg_gui;
		
		gui->is_tab = TRUE;
	}
#if 0
	if (user && user->hostname)
		set_topic(sess, user->hostname);
#endif
	mg_add_chan(sess);

	if (first_run || (newtabstofront_config == FOCUS_NEW_ONLY_ASKED && focus)
	    || newtabstofront_config == FOCUS_NEW_ALL)
		chan_focus(gtk_private(sess)->tab);
}

#if 0

GtkWidget *
mg_create_generic_tab(char *name, char *title, int force_toplevel,
		      int link_buttons,
		      void *close_callback, void *userdata,
		      int width, int height, GtkWidget **vbox_ret, void *family)
{
	GtkWidget *vbox, *win;

	if (tab_pos_config == POS_HIDDEN && prefs.windows_as_tabs)
		prefs.windows_as_tabs = 0;

	if (force_toplevel || !prefs.windows_as_tabs) {
		win = gtkutil_window_new(title, name, width, height, 3);
		vbox = gtk_vbox_new(0, 0);
		*vbox_ret = vbox;
		gtk_container_add(GTK_CONTAINER(win), vbox);
		gtk_widget_show(vbox);
		if (close_callback)
			g_signal_connect(G_OBJECT(win), "destroy",
					 G_CALLBACK(close_callback), userdata);
		return win;
	}

	vbox = gtk_vbox_new(0, 2);
	g_object_set_data(G_OBJECT(vbox), "w", GINT_TO_POINTER(width));
	g_object_set_data(G_OBJECT(vbox), "h", GINT_TO_POINTER(height));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
	*vbox_ret = vbox;

	if (close_callback)
		g_signal_connect(G_OBJECT(vbox), "destroy", G_CALLBACK(close_callback), userdata);

	mg_add_generic_tab(name, title, family, vbox);

/*	if (link_buttons)
	{
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, 0, 0, 0);
		mg_create_link_buttons (hbox, ch);
		gtk_widget_show (hbox);
	}*/

	return vbox;
}

void
mg_move_tab(window_t *sess, int delta)
{
	if (sess->gui->is_tab)
		chan_move(sess->res->tab, delta);
}

void
mg_set_title(GtkWidget *vbox, char *title)
{				/* for non-irc tab/window only */
	char *old;

	old = g_object_get_data(G_OBJECT(vbox), "title");
	if (old) {
		g_object_set_data(G_OBJECT(vbox), "title", xstrdup(title));
		free(old);
	} else {
		gtk_window_set_title(GTK_WINDOW(vbox), title);
	}
}

#endif

/* called when a session is being killed */

void fe_close_window(window_t *sess) {
	if (gtk_private_ui(sess)->is_tab)
		mg_tab_close(sess);
	else
		gtk_widget_destroy(gtk_private_ui(sess)->window);

	if (gtk_private_ui(sess) != &static_mg_gui)
		xfree(gtk_private_ui(sess));		/* free gui, if not static */
}

/* NOT COPIED:
 *
 * is_child_of()  mg_handle_drop() mg_drag_begin_cb() mg_drag_end_cb() mg_drag_drop_cb()
 * mg_drag_motion_cb() 
 * mg_dialog_dnd_drop()
 * mg_dnd_drop_file()
 */

/* mg_count_dccs() mg_count_networks() */

/* inne okienka, ,,generic'':
 *	mg_link_gentab() wywolywany z mg_detach_tab_cb() 
 *	mg_close_gen() wywolywany z mg_xbutton_cb() 
 */
