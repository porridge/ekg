/* $Id$ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>
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
#include <unistd.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <errno.h>
#include <curses.h>
#include "libgg.h"
#include "stuff.h"
#include "events.h"
#include "commands.h"
#include "themes.h"

void handle_msg(), handle_ack(), handle_status(), handle_notify(),
	handle_success(), handle_failure();

static struct handler handlers[] = {
	{ GG_EVENT_MSG, handle_msg },
	{ GG_EVENT_ACK, handle_ack },
	{ GG_EVENT_STATUS, handle_status },
	{ GG_EVENT_NOTIFY, handle_notify },
	{ GG_EVENT_CONN_SUCCESS, handle_success },
	{ GG_EVENT_CONN_FAILED, handle_failure },
	{ 0, NULL }
};

/*
 * print_message_body()
 *
 * funkcja ³adnie formatuje tre¶æ wiadomo¶ci, zawija linijki, wy¶wietla
 * kolorowe ramki i takie tam.
 *
 *  - str - tre¶æ,
 *  - chat - rodzaj wiadomo¶ci (0 - msg, 1 - chat, 2 - sysmsg)
 *
 * nie zwraca niczego. efekt widaæ na ekranie.
 */
void print_message_body(char *str, int chat)
{
	int width, i, j;
	char *mesg, *buf, *line, *next, *format = NULL, *save;
	char *line_width = NULL; 
	
	switch (chat) {
		case 0:
		    format = "message_line";
		    line_width = "message_line_width";
		    break;		
		case 1:
		    format = "chat_line"; 
		    line_width = "chat_line_width";
		    break;
		case 2:
		    format = "sysmsg_line"; 
		    line_width = "sysmsg_line_width";
		    break;
	}	

	if (!(width = atoi(find_format(line_width))))
		width = 78;
	
	if (!(buf = malloc(width + 1)) || !(mesg = save = strdup(str))) {
		my_puts(str);			/* emergency ;) */
		return;
	}

	for (i = 0; i < strlen(mesg); i++)	/* XXX ³adniejsze taby */
		if (mesg[i] == '\t')
			mesg[i] = ' ';
	
	while ((line = gg_get_line(&mesg))) {
		for (; strlen(line); line = next) {
			if (strlen(line) <= width) {
				strcpy(buf, line);
				next = line + strlen(line);
			} else {
				int len = width;
				
				for (j = width; j; j--)
					if (line[j] == ' ') {
						len = j;
						break;
					}

				strncpy(buf, line, len);
				buf[len] = 0;
				next = line + len;

				while (*next == ' ')
					next++;
			}

			my_printf(format, buf);
		}
	}

	free(buf);
	free(save);
}

/*
 * handle_msg()
 *
 * funkcja obs³uguje przychodz±ce wiadomo¶ci.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_msg(struct gg_event *e)
{
	struct userlist *u = find_user(e->event.msg.sender, NULL);
	int chat = ((e->event.msg.msgclass & 0x0f) == GG_CLASS_CHAT);
	char sender[100];
	struct tm *tm;
	char czas[100];

	
	if (is_ignored(e->event.msg.sender)) {
		if (log_ignored) {
			cp_to_iso(e->event.msg.message);
			if (u)
				snprintf(sender, sizeof(sender), "%s/%lu", u->comment, u->uin);
			else
				snprintf(sender, sizeof(sender), "%lu", e->event.msg.sender);
			put_log(e->event.msg.sender, ">> ignorowana %s %s (%s)\n%s\n", (chat) ?
			    "Rozmowa od" : "Wiadomo¶æ od", sender, full_timestamp(), e->event.msg.message);
		}
		return;
	};
	
	if (e->event.msg.sender == 0) {
		if (e->event.msg.msgclass > last_sysmsg) {
		    my_printf("sysmsg_header");
		    cp_to_iso(e->event.msg.message);
		    print_message_body(e->event.msg.message, 2);
		    my_printf("sysmsg_footer");

		    if (enable_beep)
			    my_puts("\007");
		    play_sound(sound_sysmsg_file);
		    last_sysmsg = e->event.msg.msgclass;
		    write_sysmsg(NULL);
		};
		return;
	};
			
	if (u)
		add_send_nick(u->comment);
	else {
		char tmp[20];

		snprintf(tmp, sizeof(tmp), "%lu", e->event.msg.sender);
		add_send_nick(tmp);
	}	

	tm = localtime(&e->event.msg.time);
	strftime(czas, 100, find_format("timestamp"), tm);

	cp_to_iso(e->event.msg.message);
	my_printf((chat) ? "chat_header" : "message_header", format_user(e->event.msg.sender), czas);

	print_message_body(e->event.msg.message, chat);
	my_printf((chat) ? "chat_footer" : "message_footer");

	if (enable_beep && ((chat) ? enable_beep_chat : enable_beep_msg))
		my_puts("\007");

	play_sound((chat) ? sound_chat_file : sound_msg_file);

	if (u)
		snprintf(sender, sizeof(sender), "%s/%lu", u->comment, u->uin);
	else
		snprintf(sender, sizeof(sender), "%lu", e->event.msg.sender);

	/* wiem, niegramatycznie, ale jako¶ trzeba rozró¿niæ */

	put_log(e->event.msg.sender, ">> %s %s (%s)\n%s\n", (chat) ?
		"Rozmowa od" : "Wiadomo¶æ od", sender, full_timestamp(),
		e->event.msg.message);

	if (away && sms_away && sms_send_app && sms_number) {
		char *foo;

		if (strlen(e->event.msg.message) > sms_max_length)
			e->event.msg.message[sms_max_length] = 0;

		foo = format_string(find_format((chat) ? "sms_chat" : "sms_msg"), sender, e->event.msg.message);

		/* niech nie wysy³a smsów, je¶li brakuje formatów */
		if (strcmp(foo, ""))
			send_sms(sms_number, foo, 0);

		free(foo);
	}
}

/*
 * handle_ack()
 *
 * funkcja obs³uguje potwierdzenia wiadomo¶ci.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_ack(struct gg_event *e)
{
	char *tmp;
	int queued = (e->event.ack.status == GG_ACK_QUEUED);

	if (!e->event.ack.seq)	/* ignorujemy potwierdzenia ctcp */
		return;

	if (!display_ack)
		return;

	if (display_ack == 2 && queued)
		return;

	if (display_ack == 3 && !queued)
		return;

	tmp = queued ? "ack_queued" : "ack_delivered";
	my_printf(tmp, format_user(e->event.ack.recipient));
}

/*
 * handle_notify()
 *
 * funkcja obs³uguje listê obecnych.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_notify(struct gg_event *e)
{
	struct gg_notify_reply *n = e->event.notify;
	struct userlist *u;		

	while (n->uin) {
		if (is_ignored(n->uin)) {
			n++;
			continue;
		}

		if ((u = find_user(n->uin, NULL))) {
			u->status = n->status;
			u->ip = n->remote_ip;
			u->port = n->remote_port;
		}
		if (n->status == GG_STATUS_AVAIL) {
			my_printf("status_avail", format_user(n->uin));
			if (u && completion_notify)
				add_send_nick(u->comment);
			if (enable_beep && enable_beep_notify)
				my_puts("\007");
		} else if (n->status == GG_STATUS_BUSY)
			my_printf("status_busy", format_user(n->uin));
		n++;
	}
}

/*
 * handle_status()
 *
 * funkcja obs³uguje zmianê stanu ludzi z listy kontaktów.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_status(struct gg_event *e)
{
	struct userlist *u;

	if (is_ignored(e->event.status.uin))
		return;
	
	if (!(u = find_user(e->event.status.uin, NULL)))
		return;

	if (display_notify) {
		if (e->event.status.status == GG_STATUS_AVAIL && u->status != GG_STATUS_AVAIL) {
			if (u && completion_notify)
				add_send_nick(u->comment);
			my_printf("status_avail", format_user(e->event.status.uin));
			if (enable_beep && enable_beep_notify)
				my_puts("\007");
		} else if (e->event.status.status == GG_STATUS_BUSY && u->status != GG_STATUS_BUSY)
			my_printf("status_busy", format_user(e->event.status.uin));
		else if (e->event.status.status == GG_STATUS_NOT_AVAIL && u->status != GG_STATUS_NOT_AVAIL)
			my_printf("status_not_avail", format_user(e->event.status.uin));
	}
	
	u->status = e->event.status.status;
}

/*
 * handle_failure()
 *
 * funkcja obs³uguje b³êdy przy po³±czeniu.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_failure(struct gg_event *e)
{
	my_printf("conn_failed", strerror(errno));
	list_remove(&watches, sess, 0);
	gg_free_session(sess);
	sess = NULL;
	do_reconnect();
}

/*
 * handle_success()
 *
 * funkcja obs³uguje udane po³±czenia.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_success(struct gg_event *e)
{
        int status_table[3] = { GG_STATUS_AVAIL, GG_STATUS_BUSY, GG_STATUS_INVISIBLE };

	my_printf("connected");
	send_userlist();

	if (away || private_mode)
		gg_change_status(sess, status_table[away] | ((private_mode) ? GG_STATUS_FRIENDS_MASK : 0));

}

/*
 * handle_event()
 *
 * funkcja obs³uguje wszystkie zdarzenia dotycz±ce danej sesji GG.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_event(struct gg_session *s)
{
	struct gg_event *e;
	struct handler *h;

	if (!(e = gg_watch_fd(sess))) {
		my_printf("conn_broken", strerror(errno));
		list_remove(&watches, sess, 0);
		gg_free_session(sess);
		sess = NULL;
		do_reconnect();

		return;
	}

	for (h = handlers; h->type; h++)
		if (h->type == e->type)
			(h->handler)(e);

	gg_free_event(e);
}

/*
 * handle_search()
 *
 * funkcja obs³uguje zdarzenia dotycz±ce wyszukiwania u¿ytkowników.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_search(struct gg_http *h)
{
	struct gg_search *s = NULL;
	int i;

	if (gg_search_watch_fd(h) || h->state == GG_STATE_ERROR) {
		my_printf("search_failed", strerror(errno));
		free(h->user_data);
		list_remove(&watches, h, 0);
		gg_free_search(h);
		return;
	}
	
	if (h->state != GG_STATE_DONE)
		return;

	gg_debug(GG_DEBUG_MISC, "++ gg_search()... done\n");

	if (!h || !(s = h->data) || !(s->count))
		my_printf("search_not_found");

	for (i = 0; i < s->count; i++) {
		char uin[16], born[16], *active, *gender, *name;

		snprintf(uin, sizeof(uin), "%lu", s->results[i].uin);
		
		cp_to_iso(s->results[i].first_name);
		cp_to_iso(s->results[i].last_name);
		cp_to_iso(s->results[i].nickname);
		cp_to_iso(s->results[i].city);

		name = gg_alloc_sprintf("%s %s", s->results[i].first_name, s->results[i].last_name);

		if (s->results[i].born)
			snprintf(born, sizeof(born), "%d", s->results[i].born);
		else
			snprintf(born, sizeof(born), "-");

		if (!(h->id & 1)) {
			active = find_format((s->results[i].active) ? "search_results_single_active" : "search_results_single_inactive");
			if (s->results[i].gender == GG_GENDER_FEMALE)
				gender = find_format("search_results_single_female");
			else if (s->results[i].gender == GG_GENDER_MALE)
				gender = find_format("search_results_single_male");
			else
				gender = find_format("search_results_single_male");
		} else {
			active = find_format((s->results[i].active) ? "search_results_multi_active" : "search_results_multi_inactive");
			if (s->results[i].gender == GG_GENDER_FEMALE)
				gender = find_format("search_results_multi_female");
			else if (s->results[i].gender == GG_GENDER_MALE)
				gender = find_format("search_results_multi_male");
			else
				gender = find_format("search_results_multi_unknown");
		}

		active = format_string(active);
		gender = format_string(gender);

		my_printf((h->id & 1) ? "search_results_multi" : "search_results_single", uin, (name) ? name : "", s->results[i].nickname, s->results[i].city, born, gender, active);

		free(name);
		free(active);
		free(gender);
	}

	free(h->user_data);
	list_remove(&watches, h, 0);
	gg_free_search(h);
}

/*
 * handle_pubdir()
 *
 * funkcja zajmuj±ca siê wszelkimi zdarzeniami http oprócz szukania.
 *
 *  - h - delikwent.
 *
 * nie zwraca niczego.
 */
void handle_pubdir(struct gg_http *h)
{
	struct gg_pubdir *s = NULL;
	char uin[16], *good = "", *bad = "";

	if (!h)
		return;
	
	switch (h->type) {
		case GG_SESSION_REGISTER:
			good = "register";
			bad = "register_failed";
			break;
		case GG_SESSION_PASSWD:
			good = "passwd";
			bad = "passwd_failed";
			break;
		case GG_SESSION_REMIND:
			good = "remind";
			bad = "remind_failed";
			break;
		case GG_SESSION_CHANGE:
			good = "change";
			bad = "change_failed";
			break;
	}

	if (gg_pubdir_watch_fd(h) || h->state == GG_STATE_ERROR) {
		my_printf(bad, strerror(errno));
		list_remove(&watches, h, 0);
		if (h->type == GG_SESSION_REGISTER) {
			free(reg_password);
			reg_password = NULL;
		}
		gg_free_pubdir(h);
		
		return;
	}
	
	if (h->state != GG_STATE_DONE)
		return;

	if (!(s = h->data) || !s->success) {
		my_printf(bad);
		return;
	}

	if (h->type == GG_SESSION_PASSWD) {
		config_password = reg_password;
		reg_password = NULL;
	}

	if (h->type == GG_SESSION_REGISTER) {
		if (!s->uin) {
			my_printf(bad);
			return;
		}
		
		if (!config_uin && !config_password && reg_password) {
			config_uin = s->uin;
			config_password = reg_password;
			reg_password = NULL;
		}
	}
	
	snprintf(uin, sizeof(uin), "%lu", s->uin);
	my_printf(good, uin);

	list_remove(&watches, h, 0);
	if (h->type == GG_SESSION_REGISTER || h->type == GG_SESSION_PASSWD) {
		free(reg_password);
		reg_password = NULL;
	}
	gg_free_pubdir(h);
}

/*
 * find_transfer()
 *
 * znajduje strukturê ,,transfer'' dotycz±c± danego po³±czenia.
 *
 *  - d - struktura gg_dcc, której szukamy.
 *
 * wska¼nik do struktury ,,transfer'' lub NULL, je¶li nie znalaz³.
 */
static struct transfer *find_transfer(struct gg_dcc *d)
{
	struct list *l;

	for (l = transfers; l; l = l->next) {
		struct transfer *t = l->data;

		if (t->dcc == d)
			return t;
	}

	return NULL;
}

/*
 * remove_transfer()
 *
 * usuwa z listy transferów ten, który dotyczy podanego po³±czenia dcc.
 *
 *  - d - po³±czenie.
 *
 * nie zwraca nic.
 */
static void remove_transfer(struct gg_dcc *d)
{
	struct transfer *t = find_transfer(d);

	if (t) {
		free(t->filename);
		list_remove(&transfers, t, 1);
	}
}

/*
 * handle_dcc()
 *
 * funkcja zajmuje siê obs³ug± wszystkich zdarzeñ zwi±zanych z DCC.
 *
 *  - d - struktura danego po³±czenia.
 *
 * nie zwraca niczego.
 */
void handle_dcc(struct gg_dcc *d)
{
	struct gg_event *e;
	struct transfer *t, tt;
	struct list *l;
	char buf1[16], buf2[16], *p;
	int tmp;

	if (!(e = gg_dcc_watch_fd(d))) {
		my_printf("dcc_error", strerror(errno));
		if (d->type != GG_SESSION_DCC_SOCKET) {
			remove_transfer(d);
			list_remove(&watches, d, 0);
			gg_free_dcc(d);
		}
		return;
	}

	switch (e->type) {
		case GG_EVENT_DCC_NEW:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_CLIENT_NEW\n");
			list_add(&watches, e->event.dcc_new, 0);
			e->event.dcc_new = NULL;
			break;

		case GG_EVENT_DCC_CLIENT_ACCEPT:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_CLIENT_ACCEPT\n");
			
			for (tmp = 0, l = transfers; l; l = l->next) {
				struct transfer *t = l->data;

				if (t->uin == d->peer_uin && config_uin == d->uin) {
					tmp = 1;
					break;
				}
			}

			if (!tmp) {
				gg_debug(GG_DEBUG_MISC, "## unauthorized client (uin=%ld), closing connection\n", d->peer_uin);
				list_remove(&watches, d, 0);
				gg_free_dcc(d);
				return;
			}
			break;	

		case GG_EVENT_DCC_NEED_FILE_INFO:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_NEED_FILE_INFO\n");

			for (l = transfers; l; l = l->next) {
				struct transfer *t = l->data;

				if (t->uin == d->peer_uin || !t->dcc) {
					if (gg_dcc_fill_file_info(d, t->filename) == -1) {
						gg_debug(GG_DEBUG_MISC, "## gg_dcc_fill_file_info() failed (%s)\n", strerror(errno));
						my_printf("dcc_open_error", t->filename);
						remove_transfer(d);
						list_remove(&watches, d, 0);
						gg_free_dcc(d);
						break;
					}
					t->dcc = d;
					t->type = GG_SESSION_DCC_SEND;
					break;
				}
			}
			break;
			
		case GG_EVENT_DCC_NEED_FILE_ACK:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_NEED_FILE_ACK\n");
			/* ¿eby nie sprawdza³o, póki luser nie odpowie */
			list_remove(&watches, d, 0);

			if (!(t = find_transfer(d))) {
				tt.uin = d->peer_uin;
				tt.type = GG_SESSION_DCC_GET;
				tt.filename = NULL;
				tt.dcc = d;
				tt.id = transfer_id();
				if (!(t = list_add(&transfers, &tt, sizeof(tt)))) {
					gg_free_dcc(d);
					break;
				}
			}
			
			snprintf(buf1, sizeof(buf1), "%d", t->id);
			snprintf(buf2, sizeof(buf2), "%ld", d->file_info.size);
			t->filename = strdup(d->file_info.filename);

			for (p = d->file_info.filename; *p; p++)
				if (*p < 32 || *p == '\\' || *p == '/')
					*p = '_';

			my_printf("dcc_get_offer", format_user(t->uin), t->filename, buf2, buf1);

			break;
			
		case GG_EVENT_DCC_DONE:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_DONE\n");

			if (!(t = find_transfer(d))) {
				gg_free_dcc(d);
				break;
			}

			my_printf((t->dcc->type == GG_SESSION_DCC_SEND) ? "dcc_done_send" : "dcc_done_get", format_user(t->uin), t->filename);
			
			remove_transfer(d);
			list_remove(&watches, d, 0);
			gg_free_dcc(d);

			break;
			
		case GG_EVENT_DCC_ERROR:
			switch (e->event.dcc_error) {
				case GG_ERROR_DCC_HANDSHAKE:
					my_printf("dcc_error_handshake");
					break;
				default:
					my_printf("dcc_error_unknown");
			}
			list_remove(&watches, d, 0);
			gg_free_dcc(d);
			break;
	}

	gg_free_event(e);
	
	return;
}

