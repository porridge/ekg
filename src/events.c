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

struct handler handlers[] = {
	{ GG_EVENT_MSG, handle_msg },
	{ GG_EVENT_ACK, handle_ack },
	{ GG_EVENT_STATUS, handle_status },
	{ GG_EVENT_NOTIFY, handle_notify },
	{ GG_EVENT_CONN_SUCCESS, handle_success },
	{ GG_EVENT_CONN_FAILED, handle_failure },
	{ 0, NULL }
};

/*
 * ta funkcja kiedy¶ bêdzie siê zajmowaæ dzieleniem linii na wyrazy,
 * robieniem ³adnego t³a pod wiadomo¶ci itd, itd.
 */
void print_message_body(char *str, int chat)
{
	my_puts("%s\n", str);
}

void handle_msg(struct gg_event *e)
{
	struct userlist *u = find_user(e->event.msg.sender, NULL);
	int chat = ((e->event.msg.msgclass & 0x0f) == GG_CLASS_CHAT);
	char sender[100];

	if (is_ignored(e->event.msg.sender))
		return;
	
	if (u)
		add_send_nick(u->comment);
	else {
		char tmp[20];

		snprintf(tmp, sizeof(tmp), "%u", e->event.msg.sender);
		add_send_nick(tmp);
	}	

	cp_to_iso(e->event.msg.message);
	my_printf((chat) ? "chat_header" : "message_header", format_user(e->event.msg.sender));

	print_message_body(e->event.msg.message, chat);
	my_printf((chat) ? "chat_footer" : "message_footer");

	if (enable_beep && ((chat) ? enable_beep_chat : enable_beep_msg))
		my_puts("\007");

	if (u)
		snprintf(sender, sizeof(sender), "%s/%u", u->comment, u->uin);
	else
		snprintf(sender, sizeof(sender), "%u", e->event.msg.sender);

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

void handle_ack(struct gg_event *e)
{
	char *tmp;

	if (!display_ack)
		return;

	tmp = (e->event.ack.status == GG_ACK_QUEUED) ? "ack_queued" : "ack_delivered";
	my_printf(tmp, format_user(e->event.ack.recipient));
}

void handle_notify(struct gg_event *e)
{
	struct gg_notify_reply *n = e->event.notify;
	struct userlist *u;		

	while (n->uin) {
		if (is_ignored(n->uin)) {
			n++;
			continue;
		}

		if ((u = find_user(n->uin, NULL)))
			u->status = n->status;
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

void handle_status(struct gg_event *e)
{
	struct userlist *u;

	if (is_ignored(e->event.status.uin))
		return;

	if ((u = find_user(e->event.status.uin, NULL)))
		u->status = e->event.status.status;

	if (e->event.status.status == GG_STATUS_AVAIL) {
		if (u && completion_notify)
			add_send_nick(u->comment);
		my_printf("status_avail", format_user(e->event.status.uin));
		if (enable_beep && enable_beep_notify)
			my_puts("\007");
	} else if (e->event.status.status == GG_STATUS_BUSY)
		my_printf("status_busy", format_user(e->event.status.uin));
	else if (e->event.status.status == GG_STATUS_NOT_AVAIL)
		my_printf("status_not_avail", format_user(e->event.status.uin));
}

void handle_failure(struct gg_event *e)
{
	my_printf("conn_failed", strerror(errno));
	gg_free_session(sess);
	sess = NULL;
	do_reconnect();
}

void handle_success(struct gg_event *e)
{
        int status_table[3] = { GG_STATUS_AVAIL, GG_STATUS_BUSY, GG_STATUS_INVISIBLE };

	my_printf("connected");
	send_userlist();

	if (away || private_mode)
		gg_change_status(sess, status_table[away] | ((private_mode) ? GG_STATUS_FRIENDS_MASK : 0));

}

int handle_event()
{
	struct gg_event *e;
	struct handler *h;

	if (!(e = gg_watch_fd(sess))) {
		my_printf("conn_broken", strerror(errno));
		gg_free_session(sess);
		clear_userlist();
		sess = NULL;
		do_reconnect();

		return 0;
	}

	for (h = handlers; h->type; h++)
		if (h->type == e->type)
			(h->handler)(e);

	gg_free_event(e);

	return 0;
}

void handle_search(struct gg_search *s)
{
	int i;

	if (!s->count)
		my_printf("search_not_found");

	for (i = 0; i < s->count; i++) {
		char uin[16], born[16], *active, *gender, *name;

		snprintf(uin, sizeof(uin), "%u", s->results[i].uin);

		name = gg_alloc_sprintf("%s %s", s->results[i].first_name, s->results[i].last_name);

		if (s->results[i].born)
			snprintf(born, sizeof(born), "%d", s->results[i].born);
		else
			snprintf(born, sizeof(born), "-");

		if (search_type == 1) {
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

		cp_to_iso(s->results[i].first_name);
		cp_to_iso(s->results[i].last_name);
		cp_to_iso(s->results[i].nickname);
		cp_to_iso(s->results[i].city);

		my_printf((search_type == 1) ? "search_results_single" : "search_results_multi", uin, (name) ? name : "", s->results[i].nickname, s->results[i].city, born, gender, active);

		free(name);
		free(active);
		free(gender);

	}
}

