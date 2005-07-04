/* $Id$ */

/*
 *  (C) Copyright 2001-2005 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Piotr Wysocki <wysek@linux.bydg.org>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
 *                          Adam Czerwiñski <acze@acze.net>
 *                          Adam Wysocki <gophi@ekg.apcoh.org>
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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "emoticons.h"
#include "events.h"
#include "libgadu.h"
#include "log.h"
#include "msgqueue.h"
#ifdef HAVE_OPENSSL
#  include "simlite.h"
#endif
#ifndef HAVE_STRLCPY
#  include "../compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "userlist.h"
#include "voice.h"
#include "xmalloc.h"
#include "token.h"
#ifdef WITH_PYTHON
#  include "python.h"
#endif
#ifdef HAVE_JPEGLIB_H
#  include <jpeglib.h>
#endif

void handle_msg(), handle_ack(), handle_status(), handle_notify(),
	handle_success(), handle_failure(), handle_search50(),
	handle_change50(), handle_status60(), handle_notify60(),
	handle_userlist(), handle_image_request(), handle_image_reply();

static int hide_notavail = 0;	/* czy ma ukrywaæ niedostêpnych -- tylko zaraz po po³±czeniu */

static int dcc_limit_time = 0;	/* czas pierwszego liczonego po³±czenia */
static int dcc_limit_count = 0;	/* ilo¶æ po³±czeñ od ostatniego razu */

static int auto_find_limit = 100; /* ilo¶æ osób, których nie znamy, a szukali¶my po odebraniu msg */

static struct handler handlers[] = {
	{ GG_EVENT_MSG, handle_msg },
	{ GG_EVENT_ACK, handle_ack },
	{ GG_EVENT_STATUS, handle_status },
	{ GG_EVENT_NOTIFY, handle_notify },
	{ GG_EVENT_STATUS60, handle_status60 },
	{ GG_EVENT_NOTIFY60, handle_notify60 },
	{ GG_EVENT_NOTIFY_DESCR, handle_notify },
	{ GG_EVENT_CONN_SUCCESS, handle_success },
	{ GG_EVENT_CONN_FAILED, handle_failure },
	{ GG_EVENT_DISCONNECT, handle_disconnect },
	{ GG_EVENT_PUBDIR50_SEARCH_REPLY, handle_search50 },
	{ GG_EVENT_PUBDIR50_WRITE, handle_change50 },
	{ GG_EVENT_USERLIST, handle_userlist },
	{ GG_EVENT_IMAGE_REQUEST, handle_image_request },
	{ GG_EVENT_IMAGE_REPLY, handle_image_reply },
	{ 0, NULL }
};

/*
 * print_message()
 *
 * funkcja ³adnie formatuje tre¶æ wiadomo¶ci, zawija linijki, wy¶wietla
 * kolorowe ramki i takie tam.
 *
 *  - e - zdarzenie wiadomo¶ci
 *  - u - wpis u¿ytkownika w userli¶cie
 *  - chat - rodzaj wiadomo¶ci (0 - msg, 1 - chat, 2 - sysmsg)
 *  - secure - czy wiadomo¶æ jest bezpieczna
 *
 * nie zwraca niczego. efekt widaæ na ekranie.
 */
void print_message(struct gg_event *e, struct userlist *u, int chat, int secure)
{
	int width, next_width, i, j, mem_width = 0;
	time_t tt, t = e->event.msg.time;
	int separate = (e->event.msg.sender != config_uin || chat == 3);
	int timestamp_type = 0;
	char *mesg, *buf, *line, *next, *format = NULL, *format_first = "";
	char *next_format = NULL, *head = NULL, *foot = NULL;
	char *timestamp = NULL, *save, *secure_label = NULL;
	char *line_width = NULL, timestr[100], *target, *cname;
	char *formatmap = NULL;
	struct tm *tm;
	int now_days;
	struct conference *c = NULL;

	/* tworzymy mapê formatowanego tekstu. dla ka¿dego znaku wiadomo¶ci
	 * zapisujemy jeden znak koloru z docs/themes.txt lub \0 je¶li nie
	 * trzeba niczego zmieniaæ. */

	if (e->event.msg.formats && e->event.msg.formats_length) {
		unsigned char *p = e->event.msg.formats;
		char last_attr = 0, *attrmap;

		if (config_display_color_map && strlen(config_display_color_map) == 8)
			attrmap = config_display_color_map;
		else
			attrmap = "nTgGbBrR";

		formatmap = xcalloc(1, strlen(e->event.msg.message));
		
		for (i = 0; i < e->event.msg.formats_length; ) {
			int pos = p[i] + p[i + 1] * 256;

			if (pos >= strlen(e->event.msg.message)) {
				xfree(formatmap);
				formatmap = NULL;
				break;
			}

			if ((p[i + 2] & GG_FONT_COLOR)) {
				formatmap[pos] = color_map(p[i + 3], p[i + 4], p[i + 5]);
				if (formatmap[pos] == 'k')
					formatmap[pos] = 'n';
			}

			if ((p[i + 2] & 7) || !p[i + 2] || !(p[i + 2] && GG_FONT_COLOR) || ((p[i + 2] & GG_FONT_COLOR) && !p[i + 3] && !p[i + 4] && !p[i + 5]))
				formatmap[pos] = attrmap[p[i + 2] & 7];

			i += (p[i + 2] & GG_FONT_COLOR) ? 6 : 3;
		}

		/* teraz powtarzamy formaty tam, gdzie jest przej¶cie do
		 * nowej linii i odstêpy. dziêki temu oszczêdzamy sobie
		 * mieszania ni¿ej w kodzie. */

		for (i = 0; formatmap && i < strlen(e->event.msg.message); i++) {
			if (formatmap[i])
				last_attr = formatmap[i];

			if (i > 0 && strchr(" \n", e->event.msg.message[i - 1]))
				formatmap[i] = last_attr;
		}
	}

	if (secure)
		secure_label = format_string(format_find("secure"));
	
	if (e->event.msg.recipients) {
		c = conference_find_by_uins(e->event.msg.sender, 
			e->event.msg.recipients, e->event.msg.recipients_count, 0);

		if (!c) {
			string_t tmp = string_init(NULL);
			int first = 0, i;

			for (i = 0; i < e->event.msg.recipients_count; i++) {
				if (first++) 
					string_append_c(tmp, ',');

			        string_append(tmp, itoa(e->event.msg.recipients[i]));
			}

			string_append_c(tmp, ' ');
			string_append(tmp, itoa(e->event.msg.sender));

			c = conference_create(tmp->str);

			string_free(tmp, 1);
		}
		
		if (c)
			target = xstrdup(c->name);
		else
			target = xstrdup((chat == 2) ? "__status" : ((u && u->display) ? u->display : itoa(e->event.msg.sender)));
	} else
	        target = xstrdup((chat == 2) ? "__status" : ((u && u->display) ? u->display : itoa(e->event.msg.sender)));

	cname = (c ? c->name : "");

	tt = time(NULL);
	tm = localtime(&tt);
	now_days = tm->tm_yday;

	tm = localtime(&e->event.msg.time);

	if (t - config_time_deviation <= tt && tt <= t + config_time_deviation)
		timestamp_type = 2;
	else if (now_days == tm->tm_yday)
		timestamp_type = 1;
	
	switch (chat) {
		case 0:
			format = "message_line";
			format_first = (c) ? "message_conference_line_first" : "message_line_first";
			line_width = "message_line_width";
			head = (c) ? "message_conference_header" : "message_header";
			foot = "message_footer";

			if (timestamp_type == 1)
				timestamp = "message_timestamp_today";
			else if (timestamp_type == 2)
				timestamp = "message_timestamp_now";
			else
				timestamp = "message_timestamp";
			
			break;
			
		case 1:
			format = "chat_line"; 
			format_first = (c) ? "chat_conference_line_first" : "chat_line_first";
			line_width = "chat_line_width";
			head = (c) ? "chat_conference_header" : "chat_header";
			foot = "chat_footer";
			
			if (timestamp_type == 1)
				timestamp = "chat_timestamp_today";
			else if (timestamp_type == 2)
				timestamp = "chat_timestamp_now";
			else
				timestamp = "chat_timestamp";

			break;
			
		case 2:
			format = "sysmsg_line"; 
			line_width = "sysmsg_line_width";
			head = "sysmsg_header";
			foot = "sysmsg_footer";
			break;
			
		case 3:
		case 4:
			format = "sent_line"; 
			format_first = (c) ? "sent_conference_line_first" : "sent_line_first";
			line_width = "sent_line_width";
			head = (c) ? "sent_conference_header" : "sent_header";
			foot = "sent_footer";
			
			if (timestamp_type == 1)
				timestamp = "sent_timestamp_today";
			else if (timestamp_type == 2)
				timestamp = "sent_timestamp_now";
			else
				timestamp = "sent_timestamp";

			break;
	}	

	/* je¿eli chcemy, dodajemy do bufora ,,last'' wiadomo¶æ... */
	if (config_last & 3 && (chat >= 0 && chat <= 2))
	       last_add(0, e->event.msg.sender, tt, e->event.msg.time, e->event.msg.message);
	
	strftime(timestr, sizeof(timestr), format_find(timestamp), tm);

	if (!(width = atoi(format_find(line_width))))
		width = ui_screen_width - 2;

	if (width < 0) {
		width = ui_screen_width + width;

		if (config_timestamp)
			width -= strlen(config_timestamp) - 6;
	}

	next_width = width;
	
	if (!strcmp(format_find(format_first), "")) {
		print_window(target, separate, head, format_user(e->event.msg.sender), timestr, cname, (secure) ? secure_label : "");
		next_format = format;
		mem_width = width + 1;
	} else {
		char *tmp, *p;

		next_format = format;
		format = format_first;

		/* zmniejsz d³ugo¶æ pierwszej linii o d³ugo¶æ prefiksu z rozmówc±, timestampem itd. */
		tmp = format_string(format_find(format), "", format_user(e->event.msg.sender), timestr, cname);
		mem_width = width + strlen(tmp);
		for (p = tmp; *p && *p != '\n'; p++) {
			if (*p == 27) {
				/* pomiñ kolorki */
				while (*p && *p != 'm')
					p++;
			} else
				width--;
		}
		
		xfree(tmp);

		tmp = format_string(format_find(next_format), "", "", "", "");
		next_width -= strlen(tmp);
		xfree(tmp);
	}

	buf = xmalloc(mem_width);
	mesg = save = (strlen(e->event.msg.message) > 0) ? xstrdup(e->event.msg.message) : xstrdup(" ");

	for (i = 0; i < strlen(mesg); i++)	/* XXX ³adniejsze taby */
		if (mesg[i] == '\t')
			mesg[i] = ' ';
	
	while ((line = gg_get_line(&mesg))) {
		const char *last_format_ansi = "";
		int buf_offset;

#ifdef WITH_WAP
		if (config_wap_enabled && e->event.msg.sender != config_uin) {
			FILE *wap;
			char waptime[25], waptime2[10];
			const char *waplog;
			struct tm *waptm;
			
			waptm = localtime(&tt);

			if (config_wap_enabled == 2) {
				strftime(waptime2, sizeof(waptime2), "%H:%M%S", waptm);
				snprintf(waptime, sizeof(waptime), "wap%7s_%d", waptime2, e->event.msg.sender);

				if ((waplog = prepare_path(waptime, 1)) && (wap = fopen(waplog, "a"))) {
					fprintf(wap, "%s;%s\n", target, line);
					fclose(wap);
				}

			} else {
				strftime(waptime2, sizeof(waptime2), "%H:%M", waptm);
				sprintf(waptime, "wap%5s", waptime2);

				if ((waplog = prepare_path(waptime, 1)) && (wap = fopen(waplog, "a"))) {
					fprintf(wap,"%s(%s):%s\n", target, waptime2, line);
					fclose(wap);
				}
			}
		}
#endif

		for (; strlen(line); line = next) {
			char *emotted = NULL, *formatted;

			if (strlen(line) <= width) {
				strlcpy(buf, line, mem_width);
				next = line + strlen(line);
			} else {
				int len = width;
				
				for (j = width; j; j--)
					if (line[j] == ' ') {
						len = j;
						break;
					}

				strlcpy(buf, line, len + 1);
				buf[len] = 0;
				next = line + len;

				while (*next == ' ')
					next++;
			}
			
			buf_offset = (int) (line - save);

			if (formatmap) {
				string_t s = string_init("");
				int i;

				string_append(s, last_format_ansi);

				for (i = 0; i < strlen(buf); i++) {
					if (formatmap[buf_offset + i]) {
						last_format_ansi = format_ansi(formatmap[buf_offset + i]);
						string_append(s, last_format_ansi);
					}

					string_append_c(s, buf[i]);
				}
				formatted = string_free(s, 0);
			} else
				formatted = xstrdup(buf);

			if (config_emoticons)
				emotted = emoticon_expand(formatted);

			print_window(target, separate, format, (emotted) ? emotted : formatted , format_user(e->event.msg.sender), timestr, cname);

			width = next_width;
			format = next_format;

			xfree(emotted);
			xfree(formatted);
		}
	}
	
	xfree(buf);
	xfree(save);
	xfree(secure_label);
	xfree(formatmap);

	if (!strcmp(format_find(format_first), ""))
		print_window(target, separate, foot);

	xfree(target);
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
	struct userlist *u = userlist_find(e->event.msg.sender, NULL);
	int chat = ((e->event.msg.msgclass & 0x0f) == GG_CLASS_CHAT), secure = 0, hide = 0;

	if (!e->event.msg.message)
		return;

	if ((e->event.msg.msgclass & GG_CLASS_CTCP)) {
		list_t l;
		int dccs = 0;

		gg_debug(GG_DEBUG_MISC, "// ekg: received ctcp\n");

		for (l = watches; l; l = l->next) {
			struct gg_dcc *d = l->data;

			if (d->type == GG_SESSION_DCC)
				dccs++;
		}

		if (dccs > 50) {
			char *tmp = saprintf("/ignore %d", e->event.msg.sender);
			print_status("dcc_attack", format_user(e->event.msg.sender));
			command_exec(NULL, tmp, 0);
			xfree(tmp);

			return;
		}

		if (config_dcc && u) {
			struct gg_dcc *d;

                        if (!(d = gg_dcc_get_file(u->ip.s_addr, u->port, config_uin, e->event.msg.sender))) {
				print_status("dcc_error", strerror(errno));
				return;
			}

			list_add(&watches, d, 0);
		}

		return;
	}

	if (e->event.msg.formats_length > 0) {
		unsigned char *p = e->event.msg.formats;
		int i, imageno = 0;
		
		gg_debug(GG_DEBUG_MISC, "// ekg: received formatting info (len=%d):", e->event.msg.formats_length);
		for (i = 0; i < e->event.msg.formats_length; i++)
			gg_debug(GG_DEBUG_MISC, " %.2x", p[i]);
		gg_debug(GG_DEBUG_MISC, "\n");

		for (i = 0; i < e->event.msg.formats_length; ) {
			unsigned char font = p[i + 2];

			i += 3;

			if ((font & GG_FONT_IMAGE)) {
				struct gg_msg_richtext_image *m = (void*) &p[i];

				gg_debug(GG_DEBUG_MISC, "// ekg: inline image: sender=%d, size=%d, crc32=%.8x\n", e->event.msg.sender, m->size, m->crc32);

				imageno++;

				i += sizeof(*m);
			}

			if ((font & GG_FONT_COLOR))
				i += 3;
		}

		/* ignorujemy wiadomo¶ci bez tre¶ci zawieraj±ce jedynie obrazek(ki) */
		if (config_ignore_empty_msg && imageno && strlen(e->event.msg.message) == 0)
			return;
	}

#ifdef HAVE_OPENSSL
	if (config_encryption) {
		char *msg = sim_message_decrypt(e->event.msg.message, e->event.msg.sender);

		if (msg) {
			strlcpy(e->event.msg.message, msg, strlen(e->event.msg.message) + 1);
			xfree(msg);
			secure = 1;
		} else
			gg_debug(GG_DEBUG_MISC, "// ekg: simlite decryption failed: %s\n", sim_strerror(sim_errno));
	}
#endif

	cp_to_iso(e->event.msg.message);
	
#ifdef WITH_PYTHON
	PYTHON_HANDLE_HEADER(msg, "(isisii)", e->event.msg.sender, ((u && u->display) ? u->display : ""), e->event.msg.msgclass, e->event.msg.message, e->event.msg.time, secure)
	{
		char *b, *d;
		int f;

		PYTHON_HANDLE_RESULT("isisii", &e->event.msg.sender, &b, &e->event.msg.msgclass, &d, &e->event.msg.time, &f)
		{
			xfree(e->event.msg.message);
			e->event.msg.message = xstrdup(d);
		}
	}
	
	PYTHON_HANDLE_FOOTER()

	switch (python_handle_result) {
		case 0:
			return;
		case 2:
			hide = 1;
	}
#endif

	if (e->event.msg.sender == 0) {
		if (e->event.msg.msgclass > config_last_sysmsg) {
			if (!hide)
				print_message(e, u, 2, 0);

			if (config_beep)
				ui_beep();
		    
			play_sound(config_sound_sysmsg_file);
			config_last_sysmsg = e->event.msg.msgclass;
			config_last_sysmsg_changed = 1;
		}

		return;
	}
	
	if (e->event.msg.recipients_count) {
		struct conference *c = conference_find_by_uins(
			e->event.msg.sender, e->event.msg.recipients,
			e->event.msg.recipients_count, 0);

		if (c && c->ignore)
			return;
	}

	if ((!u && config_ignore_unknown_sender) || ignored_check(e->event.msg.sender) & IGNORE_MSG) {
		if (config_log_ignored)
			put_log(e->event.msg.sender, "%sign,%ld,%s,%s,%s,%s\n", (chat) ? "chatrecv" : "msgrecv", e->event.msg.sender, ((u && u->display) ? u->display : ""), log_timestamp(time(NULL)), log_timestamp(e->event.msg.time), e->event.msg.message);

		return;
	}

#ifdef HAVE_OPENSSL
	if (config_encryption && !strncmp(e->event.msg.message, "-----BEGIN RSA PUBLIC KEY-----", 20)) {
		char *name;
		const char *target = ((u && u->display) ? u->display : itoa(e->event.msg.sender));
		FILE *f;

		print_window(target, 0, "key_public_received", format_user(e->event.msg.sender));	

		if (mkdir(prepare_path("keys", 1), 0700) && errno != EEXIST) {
			print_window(target, 0, "key_public_write_failed", strerror(errno));
			return;
		}

		name = saprintf("%s/%d.pem", prepare_path("keys", 0), e->event.msg.sender);

		if (!(f = fopen(name, "w"))) {
			print_window(target, 0, "key_public_write_failed", strerror(errno));
			xfree(name);
			return;
		}
		
		fprintf(f, "%s", e->event.msg.message);
		fclose(f);
		xfree(name);

		return;
	}
#endif

	if (u && u->display)
		add_send_nick(u->display);
	else
		add_send_nick(itoa(e->event.msg.sender));

	if (!hide) {
		print_message(e, u, chat, secure);

		if (config_beep && ((chat) ? config_beep_chat : config_beep_msg))
			ui_beep();

		play_sound((chat) ? config_sound_chat_file : config_sound_msg_file);
	}

	put_log(e->event.msg.sender, "%s,%ld,%s,%s,%s,%s\n", (chat) ? "chatrecv" : "msgrecv", e->event.msg.sender, ((u && u->display) ? u->display : ""), log_timestamp(time(NULL)), log_timestamp(e->event.msg.time), e->event.msg.message);

	if (!(ignored_check(e->event.msg.sender) & IGNORE_EVENTS))
		event_check((chat) ? EVENT_CHAT : EVENT_MSG, e->event.msg.sender, e->event.msg.message);
			
	if (config_sms_away && (GG_S_B(config_status) || (GG_S_I(config_status) && config_sms_away & 4)) && config_sms_app && config_sms_number) {
		if (!(ignored_check(e->event.msg.sender) & IGNORE_SMSAWAY)) {
			char *foo, sender[100];

			sms_away_add(e->event.msg.sender);

			if (sms_away_check(e->event.msg.sender)) {
				if (u && u->display)
					snprintf(sender, sizeof(sender), "%s/%u", u->display, u->uin);
				else
					snprintf(sender, sizeof(sender), "%u", e->event.msg.sender);

				if (config_sms_max_length && strlen(e->event.msg.message) > config_sms_max_length)
					e->event.msg.message[config_sms_max_length] = 0;

				if (e->event.msg.recipients_count)
					foo = format_string(format_find("sms_conf"), sender, e->event.msg.message);
				else
					foo = format_string(format_find((chat) ? "sms_chat" : "sms_msg"), sender, e->event.msg.message);

				/* niech nie wysy³a smsów, je¶li brakuje formatów */
				if (strcmp(foo, ""))
					send_sms(config_sms_number, foo, 0);
		
				xfree(foo);
			}
		}
	}

	if (!u && config_auto_find) {
		list_t l;
		int do_find = 1, i;

		for (l = autofinds, i = 0; l; l = l->next, i++) {
			uin_t *d = l->data;	

			if (*d == e->event.msg.sender) {
				do_find = 0;	
				break;
			}
		}

		if (do_find) {
			char *tmp;

			if (i == auto_find_limit) {
				gg_debug(GG_DEBUG_MISC, "// autofind reached %d limit, removing the oldest uin: %d\n", auto_find_limit, *((uin_t *)autofinds->data));
				list_remove(&autofinds, autofinds->data, 1);
			}

			list_add(&autofinds, &e->event.msg.sender, sizeof(uin_t));

			tmp = saprintf("/find -u %d", e->event.msg.sender);
			command_exec(itoa(e->event.msg.sender), tmp, 0);
			xfree(tmp);
		}
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
	struct userlist *u = userlist_find(e->event.ack.recipient, NULL);
	int queued = (e->event.ack.status == GG_ACK_QUEUED);
	const char *tmp, *target = ((u && u->display) ? u->display : itoa(e->event.ack.recipient));
	static int show_short_ack_filtered = 0;

	if (!e->event.ack.seq)	/* ignorujemy potwierdzenia ctcp */
		return;

	msg_queue_remove(e->event.ack.seq);

	if (!(ignored_check(e->event.ack.recipient) & IGNORE_EVENTS))
		event_check((queued) ? EVENT_QUEUED : EVENT_DELIVERED, e->event.ack.recipient, NULL);

	if (u && !queued && GG_S_NA(u->status) && !(ignored_check(u->uin) & IGNORE_STATUS)) {
		char *ack_filtered;
		if (show_short_ack_filtered) {
			ack_filtered = "ack_filtered_short";
		} else {
			ack_filtered = "ack_filtered";
			show_short_ack_filtered = 1;
		}
		print_window(target, 0, ack_filtered, format_user(e->event.ack.recipient));
		return;
	}

	if (!config_display_ack)
		return;

	if (config_display_ack == 2 && queued)
		return;

	if (config_display_ack == 3 && !queued)
		return;

	tmp = queued ? "ack_queued" : "ack_delivered";
	print_window(target, 0, tmp, format_user(e->event.ack.recipient));
}

/*
 * handle_common()
 *
 * ujednolicona obs³uga zmiany w userli¶cie dla handle_status()
 * i handle_notify(). utrzymywanie tego samego kodu w kilku miejscach
 * jest kompletnie bez sensu.
 *  
 *  - uin - numer delikwenta,
 *  - status - nowy stan,
 *  - idescr - nowy opis,
 *  - itime - nowy czas powrotu,
 *  - ip - nowy adres IP,
 *  - port - nowy port,
 *  - version - nowa wersja,
 *  - image_size - nowy rozmiar obrazka.
 */
void handle_common(uin_t uin, int status, const char *idescr, int dtime, uint32_t ip, uint16_t port, int version, int image_size)
{
	struct userlist *u;
	static struct userlist ucache[20];
	static time_t seencache[20];
	struct status_table {
		int status;
		int event;
		char *log;
		char *format;
	};
	struct status_table st[] = {
		{ GG_STATUS_AVAIL, EVENT_AVAIL, "avail", "status_avail" },
		{ GG_STATUS_AVAIL_DESCR, EVENT_AVAIL, "avail", "status_avail_descr" },
		{ GG_STATUS_BUSY, EVENT_AWAY, "busy", "status_busy" },
		{ GG_STATUS_BUSY_DESCR, EVENT_AWAY, "busy", "status_busy_descr" },
		{ GG_STATUS_INVISIBLE, EVENT_INVISIBLE, "invisible", "status_invisible" },
		{ GG_STATUS_INVISIBLE_DESCR, EVENT_INVISIBLE, "invisible", "status_invisible_descr" },
		{ GG_STATUS_NOT_AVAIL, EVENT_NOT_AVAIL, "notavail", "status_not_avail" },
		{ GG_STATUS_NOT_AVAIL_DESCR, EVENT_NOT_AVAIL, "notavail", "status_not_avail_descr" },
		{ 0, 0, NULL, NULL }
	};
	struct status_table *s;
	int prev_status, hide = 0;
	int have_unknown = 0;
	int ignore_status, ignore_status_descr, ignore_events, ignore_notify;
	unsigned char *descr = NULL;
#ifdef WITH_PYTHON
	list_t l;
#endif

	/* nie pokazujemy nieznajomych, chyba ze display_notify & 4 */
	if (!(u = userlist_find(uin, NULL))) {
		if (!(config_display_notify & 4))
			return;
		else {
			int i;
			time_t cur = time(NULL);

			have_unknown = 1;

			/* najpierw przeszukanie cache numerków. */
			for (i = 0; i < sizeof(ucache) / sizeof(ucache[0]); i++) {
				if (ucache[i].uin == uin) {
					u = &ucache[i];
					seencache[i] = cur;
					break;
				}
			}

			/* je¶li nie znale¼li¶my w cache, to wybieramy najdawniej 
			 * widziany element cache i zastêpujemy go nowym. je¶li 
			 * które¶ pole w cache jest wolne, to jest przypisywane 
			 * aktualnemu numerkowi (bo wtedy oldest == 0 i nic nie 
			 * bêdzie od niego mniejsze). */
			if (!u) {
				time_t oldest = time(NULL), *sptr = (time_t *) NULL;

				for (i = 0; i < sizeof(ucache) / sizeof(ucache[0]); i++) {
					if (seencache[i] < oldest) {
						sptr = &seencache[i];
						oldest = *sptr;
						u = &ucache[i];
					}
				}

				memset(u, 0, sizeof(struct userlist));
				u->uin = uin;
				u->status = GG_STATUS_NOT_AVAIL;
				*sptr = time(NULL);
			}
		}
	}

	ignore_status = ignored_check(uin) & IGNORE_STATUS;
	ignore_status_descr = ignored_check(uin) & IGNORE_STATUS_DESCR;
	ignore_events = ignored_check(uin) & IGNORE_EVENTS;
	ignore_notify = ignored_check(uin) & IGNORE_NOTIFY;

	if (GG_S_B(status) || GG_S_A(status)) {
		list_t l;

		for (l = spiedlist; l; l = l->next) {
			struct spied *s = l->data;

			if (s->uin == uin) {
				list_remove(&spiedlist, s, 1);
				break;
			}
		}
	}

	/* je¶li kto¶ odchodzi albo dostajemy powiadomienie, ¿e go nie ma (i go nie by³o)... */
	if (GG_S_NA(status)) {

		/* je¶li go podgl±damy, to sprawd¼my, czy siê nie ukrywa */
		if (group_member(u, "spied"))

			/* je¶li rozpoczêli¶my sprawdanie, to na razie nie informuj o zmianie stanu */
			if (check_conn(u->uin) == 0)
				ignore_notify = 1;
	}

#ifdef WITH_PYTHON
	for (l = modules; l; l = l->next) {
		struct module *m = l->data;
		PyObject *res;

		if (!m->handle_status)
			continue;

		res = PyObject_CallFunction(m->handle_status, "(isis)", uin, ((u && u->display) ? u->display : NULL), status, idescr);

		if (!res)
			PyErr_Print();

		if (res && PyInt_Check(res)) {
			switch (PyInt_AsLong(res)) {
				case 0:
					Py_XDECREF(res);
					return;
				case 2:
					hide = 1;
			}
		}

		if (res && PyTuple_Check(res)) {
			unsigned char *newnick, *newdescr;

			if (PyArg_ParseTuple(res, "isis", &uin, &newnick, &status, &newdescr)) {
				descr = xstrdup(newdescr);
			} else
				PyErr_Print();
		}

		Py_XDECREF(res);
	}
#endif

#define __USER_QUITING (GG_S_NA(status) && !GG_S_NA(u->status))

	if (GG_S_BL(status) && !GG_S_BL(u->status)) {
		u->status = status;	/* poza list± stanów */
		if (!ignore_events)
			event_check(EVENT_BLOCKED, uin, NULL);
	}

	if (!descr) 
		descr = xstrdup(idescr);

	/* zapamiêtaj adres, port i protokó³ */
	if (__USER_QUITING) {
		u->last_ip.s_addr = u->ip.s_addr;
		u->last_port = u->port;
	}

	if (ip)
		u->ip.s_addr = ip;
	if (port)
		u->port = port;
	if (version)
		u->protocol = version;
	if (image_size)
		u->image_size = image_size;

	/* je¶li status taki sam i ewentualnie opisy te same, ignoruj */
	if (!GG_S_D(status) && (u->status == status)) {
		xfree(descr);
		return;
	}
	
	/* je¶li stan z opisem, a opisu brak, wpisz pusty tekst */
	if (GG_S_D(status) && !descr)
		descr = xstrdup("");

	if (descr) {
		unsigned char *tmp;

		for (tmp = descr; *tmp; tmp++) {
			/* usuwamy \r, interesuje nas tylko \n w opisie */
			if (*tmp == 13)
				memmove(tmp, tmp + 1, strlen(tmp));
			/* tabulacja na spacje */
			if (*tmp == 9)
				*tmp = ' ';
			/* resztê poza \n zamieniamy na ? */
			if (*tmp < 32 && *tmp != 10)
				*tmp = '?';
		}

		cp_to_iso(descr);
	}

	if (GG_S_D(status) && (u->status == status) && u->descr && !strcmp(u->descr, descr)) {
		xfree(descr);
		return;
	}

	/* jesli kto¶ nam znika, to sobie to zapamietujemy */
	if (__USER_QUITING) {
		u->last_seen = time(NULL);
		xfree(u->last_descr);
		u->last_descr = xstrdup(u->descr);
	}

#undef __USER_QUITING

	prev_status = u->status;
	
	for (s = st; s->status; s++) {
		/* je¶li nie ten, sprawdzaj dalej */
		if (status != s->status)
			continue;

		if (GG_S_NA(s->status)) {
			memset(&u->ip, 0, sizeof(struct in_addr));
			u->port = 0;
			u->protocol = 0;
			u->image_size = 0;
		}

#define __SAME_GG_S(x, y)	((GG_S_A(x) && GG_S_A(y)) || (GG_S_B(x) && GG_S_B(y)) || (GG_S_I(x) && GG_S_I(y)) || (GG_S_NA(x) && GG_S_NA(y)))

		if (!ignore_events && (!config_events_delay || (time(NULL) - last_conn_event) >= config_events_delay)) {
			if ((descr && u->descr && strcmp(descr, u->descr)) || (!u->descr && descr))
				event_check(EVENT_DESCR, uin, descr);

			if (!__SAME_GG_S(prev_status, status)) {
				if (!ignore_status && GG_S_NA(prev_status) && GG_S_A(s->status))
					event_check(EVENT_ONLINE, uin, descr);
				else
					event_check(s->event, uin, descr);
			}
		}

		if (ignore_status_descr && GG_S_D(status)) {
			s--;
			status = s->status;

			if (__SAME_GG_S(prev_status, status))
				break;
		}

#undef __SAME_GG_S

		if ((ignore_status || ignore_notify) && !config_log_ignored)
			break;

		/* zaloguj */
		if ((config_log_status == 1) && (!GG_S_D(s->status) || !descr))
			put_log(uin, "status,%ld,%s,%s:%d,%s,%s\n", uin, ((u->display) ? u->display : ""), inet_ntoa(u->ip), u->port, log_timestamp(time(NULL)), s->log);
		if (config_log_status && GG_S_D(s->status) && descr)
		    	put_log(uin, "status,%ld,%s,%s:%d,%s,%s,%s\n", uin, ((u->display) ? u->display : ""), inet_ntoa(u->ip), u->port, log_timestamp(time(NULL)), s->log, descr);

		if (ignore_status || ignore_notify)
			break;

		/* jak dostêpny lub zajêty i mamy go na li¶cie, dopiszmy do taba
		 * jak niedostêpny, usuñmy. nie dotyczy osób spoza listy. */
		if (!have_unknown) {
			if (GG_S_A(s->status) && config_completion_notify && u->display) 
				add_send_nick(u->display);
			if (GG_S_B(s->status) && (config_completion_notify & 4) && u->display)
				add_send_nick(u->display);
			if (GG_S_I(s->status) && (config_completion_notify & 8) && u->display)
				add_send_nick(u->display);
			if (GG_S_NA(s->status) && (config_completion_notify & 2) && u->display)
				remove_send_nick(u->display);
		}

		/* wy¶wietlaæ na ekranie ? */
		if (!(config_display_notify & ~4) || hide)
			break;

		if ((config_display_notify & ~4) == 2) {
			/* je¶li na zajêty, ignorujemy */
			if (GG_S_B(s->status) && !GG_S_NA(prev_status))
				break;

			/* je¶li na dostêpny i nie by³ niedostêpny, ignoruj */
			if (GG_S_A(s->status) && !GG_S_NA(prev_status))
				break;
		}

		/* czy ukrywaæ niedostêpnych */
		if (hide_notavail) {
			if (GG_S_NA(s->status) && GG_S_NA(u->status))
				break;
			else if (time(NULL) - last_conn_event >= config_events_delay)
				hide_notavail = 0;
		}

		/* daj znaæ d¿wiêkiem */
		if (config_beep && config_beep_notify && (!config_events_delay || (time(NULL) - last_conn_event) >= config_events_delay))
			ui_beep();

		/* i muzyczk± */
		if (config_sound_notify_file && strcmp(config_sound_notify_file, "") && (!config_events_delay || (time(NULL) - last_conn_event) >= config_events_delay))
			play_sound(config_sound_notify_file);

#ifdef WITH_UI_NCURSES
		if (ui_init == ui_ncurses_init && config_contacts == 2)
			break;
#endif
			
		/* no dobra, poka¿ */
		if (u->display || have_unknown) {
			char *tmp = xstrdup(descr), *p;
			char *target, *display;

			for (p = tmp; p && *p; p++) {
				if (*p == 13 || *p == 10)
					*p = '|';
			}

			if (have_unknown) {
				target = (char *) itoa(uin);
				display = target;
			} else {
				target = u->display;
				display = (u->first_name) ? u->first_name : u->display;
			}

			if (config_status_window == 1)
				target = "__current";
			else if (config_status_window == 2)
				target = "__status";

			print_window(target, 0, s->format, format_user(uin), display, tmp);

			xfree(tmp);
		}
	
		break;
	}

	if (!ignore_status) {
		u->status = status;
		xfree(u->descr);
		u->descr = descr;
		ui_event("status", u->uin, ((u->display) ? u->display : ""), status, (ignore_status_descr) ? NULL : u->descr);
	 } else
		xfree(descr);
}

/*
 * handle_notify()
 *
 * funkcja obs³uguje listê obecnych.
 *
 *  - e - opis zdarzenia.
 */
void handle_notify(struct gg_event *e)
{
	struct gg_notify_reply *n;

	if (batch_mode)
		return;

	n = (e->type == GG_EVENT_NOTIFY) ? e->event.notify : e->event.notify_descr.notify;

	for (; n->uin; n++) {
		char *descr = (e->type == GG_EVENT_NOTIFY_DESCR) ? e->event.notify_descr.descr : NULL;
		
		handle_common(n->uin, n->status, descr, 0, n->remote_ip, n->remote_port, n->version, 0);
	}
}

/*
 * handle_notify60()
 *
 * funkcja obs³uguje listê obecnych w wersji 6.0.
 *
 *  - e - opis zdarzenia.
 */
void handle_notify60(struct gg_event *e)
{
	int i;
	
	if (batch_mode)
		return;

	for (i = 0; e->event.notify60[i].uin; i++)
		handle_common(e->event.notify60[i].uin, e->event.notify60[i].status, e->event.notify60[i].descr, e->event.notify60[i].time, e->event.notify60[i].remote_ip, e->event.notify60[i].remote_port, e->event.notify60[i].version, e->event.notify60[i].image_size);
}

/*
 * handle_status()
 *
 * funkcja obs³uguje zmianê stanu ludzi z listy kontaktów.
 *
 *  - e - opis zdarzenia.
 */
void handle_status(struct gg_event *e)
{
	if (batch_mode)
		return;

	handle_common(e->event.status.uin, e->event.status.status, e->event.status.descr, 0, 0, 0, 0, 0);
}

/*
 * handle_status60()
 *
 * funkcja obs³uguje zmianê stanu ludzi z listy kontaktów w wersji 6.0.
 *
 *  - e - opis zdarzenia.
 */
void handle_status60(struct gg_event *e)
{
	if (batch_mode)
		return;

	handle_common(e->event.status60.uin, e->event.status60.status, e->event.status60.descr, e->event.status60.time, e->event.status60.remote_ip, e->event.status60.remote_port, e->event.status60.version, e->event.status60.image_size);
}

/*
 * handle_failure()
 *
 * funkcja obs³uguje b³êdy przy po³±czeniu.
 *
 *  - e - opis zdarzenia.
 */
void handle_failure(struct gg_event *e)
{
	struct { int type; char *str; } reason[] = {
		{ GG_FAILURE_RESOLVING, "conn_failed_resolving" },
		{ GG_FAILURE_CONNECTING, "conn_failed_connecting" },
		{ GG_FAILURE_INVALID, "conn_failed_invalid" },
		{ GG_FAILURE_READING, "conn_failed_disconnected" },
		{ GG_FAILURE_WRITING, "conn_failed_disconnected" },
		{ GG_FAILURE_PASSWORD, "conn_failed_password" },
		{ GG_FAILURE_404, "conn_failed_404" },
		{ GG_FAILURE_TLS, "conn_failed_tls" },
		{ 0, NULL }
	};

	char *tmp = NULL;
	int i;

	for (i = 0; reason[i].type; i++) {
		if (reason[i].type == e->event.failure) {
			tmp = format_string(format_find(reason[i].str));
			break;
		}
	}

	print("conn_failed", (tmp) ? tmp : "?");
	xfree(tmp);

	/* je¶li siê nie powiod³o, usuwamy nasz serwer i ³±czymy przez huba */
	if (config_server_save) {
#ifdef __GG_LIBGADU_HAVE_OPENSSL
		if (sess->ssl && config_server && !strncasecmp(config_server, "tls", 3)) {
			xfree(config_server);
			config_server = xstrdup("tls");	
		} else
#endif
		{
			xfree(config_server);
			config_server = NULL;
		}
	}

	list_remove(&watches, sess, 0);
	gg_logoff(sess);
	gg_free_session(sess);
	sess = NULL;
	userlist_clear_status(0);
	ekg_reconnect();
}

/*
 * handle_success()
 *
 * funkcja obs³uguje udane po³±czenia.
 *
 *  - e - opis zdarzenia.
 */
void handle_success(struct gg_event *e)
{
	struct in_addr addr;
	list_t l;

	if (config_reason && GG_S_D(config_status)) {
		char *r1, *r2;

		r1 = xstrmid(config_reason, 0, GG_STATUS_DESCR_MAXSIZE);
		r2 = xstrmid(config_reason, GG_STATUS_DESCR_MAXSIZE, -1);
		print("connected_descr", r1, r2);
		xfree(r2);
		xfree(r1);
	} else
		print("connected");
	
	ui_event("connected");

	userlist_send();

	if (!msg_queue_flush())
		print("queue_flush");

	/* zapiszmy adres serwera */
	if (config_server_save) {
		addr.s_addr = sess->server_addr;
		
		xfree(config_server);
#ifdef __GG_LIBGADU_HAVE_OPENSSL
		if (sess->ssl)
			config_server = saprintf("tls:%s:%d", inet_ntoa(addr), sess->port);
		else
#endif
		{
			if (sess->port != GG_DEFAULT_PORT)
				config_server = saprintf("%s:%d", inet_ntoa(addr), sess->port);
			else
				config_server = xstrdup(inet_ntoa(addr));
		}
	}
	
	if (batch_mode && batch_line) {
 		command_exec(NULL, batch_line, 0);
 		xfree(batch_line);
 		batch_line = NULL;
 	}

	hide_notavail = 1;
	 
	/* ustawiamy swój status */
	change_status(config_status, config_reason, 2);

	update_status();
	update_status_myip();

	last_conn_event = time(NULL);

	addr.s_addr = sess->server_addr;

	event_check(EVENT_CONNECTED, 0, inet_ntoa(addr));

	list_destroy(spiedlist, 1);
	spiedlist = NULL;

	for (l = userlist; l; l = l->next) {
		struct userlist *u = l->data;

		if (group_member(u, "spied"))
			check_conn(u->uin);
	}
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
		struct in_addr addr;
		print("conn_broken");

		addr.s_addr = sess->server_addr;
		event_check(EVENT_DISCONNECTED, 0, inet_ntoa(addr));

		list_remove(&watches, sess, 0);
		gg_logoff(sess);
		gg_free_session(sess);
		sess = NULL;
		userlist_clear_status(0);
		ui_event("disconnected");
		last_conn_event = time(NULL);
		ekg_reconnect();

		return;
	}

	for (h = handlers; h->type; h++)
		if (h->type == e->type)
			(h->handler)(e);

	gg_event_free(e);
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
	const char *good = "", *bad = "";

	if (!h)
		return;
	
	switch (h->type) {
		case GG_SESSION_REGISTER:
			good = "register";
			bad = "register_failed";
			break;
		case GG_SESSION_UNREGISTER:
			good = "unregister";
			bad = "unregister_failed";
			break;
		case GG_SESSION_PASSWD:
			good = "passwd";
			bad = "passwd_failed";
			break;
		case GG_SESSION_REMIND:
			good = "remind";
			bad = "remind_failed";
			break;
	}

	if (gg_pubdir_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print(bad, http_error_string(h->error));
		goto fail;
	}
	
	if (h->state != GG_STATE_DONE)
		return;

	if (!(s = h->data) || !s->success) {
		print(bad, http_error_string(h->error));
		goto fail;
	}

	if (h->type == GG_SESSION_PASSWD) {
		xfree(config_password);
		config_password = reg_password;
		reg_password = NULL;
		if (reg_email) {
			xfree(config_email);
			config_email = reg_email;
			reg_email = NULL;
		}
	}

	if (h->type == GG_SESSION_REGISTER) {
		if (!s->uin) {
			print("register_failed", "");
			goto fail;
		}
		
		if (!config_uin && !config_password && reg_password && !config_email && reg_email) {
			config_uin = s->uin;
			
			config_password = reg_password;
			reg_password = NULL;

			config_email = reg_email;
			reg_email = NULL;
		}

		registered_today = 1;
	}

	if (h->type == GG_SESSION_UNREGISTER) {
		if (!s->uin) {
			print("unregister_failed", "");
			goto fail;
		}

		if (s->uin == config_uin) {
			config_uin = 0;
			config_password = 0;
			config_changed = 1;
			command_exec(NULL, "disconnect", 0);
			print("no_config");
		}
	}
	
	print(good, itoa(s->uin));

fail:
	list_remove(&watches, h, 0);
	if (h->type == GG_SESSION_REGISTER || h->type == GG_SESSION_PASSWD) {
		xfree(reg_password);
		reg_password = NULL;
		xfree(reg_email);
		reg_email = NULL;
	}
	gg_free_pubdir(h);
}

/*
 * token_check()
 * 
 * funkcja sprawdza czy w danym miejscu znajduje siê zaproponowany znaczek
 * 
 *  - n - numer od 0 do 15 (znaczki od 0 do f)
 *  - x, y - wspó³rzêdne znaczka w tablicy ocr
 */
static int token_check(int nr, int x, int y, const char *ocr, int maxx, int maxy)
{
	int i;

	for (i = nr * token_char_height; i < (nr + 1) * token_char_height; i++, y++) {
		int j, xx = x;

		for (j = 0; token_id[i][j] && j + xx < maxx; j++, xx++) {
			if (token_id[i][j] != ocr[y * (maxx + 1) + xx])
				return 0;
		}
	}

	gg_debug(GG_DEBUG_MISC, "token_check(nr=%d,x=%d,y=%d,ocr=%p,maxx=%d,maxy=%d\n", nr, x, y, ocr, maxx, maxy);

	return 1;
}

/*
 * token_ocr()
 *
 * zwraca tre¶æ tokenu
 */
char *token_ocr(const char *ocr, int width, int height, int length)
{
	int x, y, count = 0;
	char *token;

	token = xmalloc(length + 1);
	memset(token, 0, length + 1);
		
	for (x = 0; x < width; x++) {
		for (y = 0; y < height - token_char_height; y++) {
			int result = 0, token_part = 0;
		      
			do
				result = token_check(token_part++, x, y, ocr, width, height);
			while (!result && token_part < 16);
			
			if (result && count < length)
				token[count++] = token_id_char[token_part - 1];
		}
	}

	if (count == length)
		return token;
	
	xfree(token);

	return NULL;
}

/*
 * handle_token()
 *
 * funkcja zajmuj±ca siê zdarzeniami zwi±zanymi z pobieraniem tokenu.
 *
 *  - h - delikwent.
 *
 * nie zwraca niczego.
 */
void handle_token(struct gg_http *h)
{
	struct gg_token *t = NULL;
	char *file = NULL;
	int fd;

	if (!h)
		return;

	if (gg_token_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print("token_failed", http_error_string(h->error));
		goto fail;
	}
	
	if (h->state != GG_STATE_DONE)
		return;

	if (!(t = h->data) || !h->body) {
		print("token_failed", http_error_string(h->error));
		goto fail;
	}

	xfree(last_tokenid);
	last_tokenid = xstrdup(t->tokenid);

#ifdef HAVE_MKSTEMP

	file = saprintf("%s/token.XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");

	if ((fd = mkstemp(file)) == -1) {
		print("token_failed", strerror(errno));
		goto fail;
	}

	if ((write(fd, h->body, h->body_size) != h->body_size) || (close(fd) != 0)) {
		print("token_failed", strerror(errno));
		close(fd);
		unlink(file);
		goto fail;
	}

#ifdef HAVE_LIBJPEG
	if (config_display_token) {
		struct jpeg_decompress_struct j;
		struct jpeg_error_mgr e;
		JSAMPROW buf[1];
		int size;
		char *token, *tmp;
		FILE *f;
		int h = 0;

		if (!(f = fopen(file, "rb"))) {
			print("token_failed", strerror(errno));
			goto fail;
		}

		j.err = jpeg_std_error(&e);
		jpeg_create_decompress(&j);
		jpeg_stdio_src(&j, f);
		jpeg_read_header(&j, TRUE);
		jpeg_start_decompress(&j);

		size = j.output_width * j.output_components;
		buf[0] = xmalloc(size);
                
                token = xmalloc((j.output_width + 1) * j.output_height);
		
		while (j.output_scanline < j.output_height) {
			int i;

			jpeg_read_scanlines(&j, buf, 1);

			for (i = 0; i < j.output_width; i++, h++)
				token[h] = (buf[0][i*3] + buf[0][i*3+1] + buf[0][i*3+2] < 384) ? '#' : '.';
			
			token[h++] = 0;
		}

		if (!(tmp = token_ocr(token, j.output_width, j.output_height, t->length))) {
			int i;

			for (i = 0; i < j.output_height; i++)
				print("token_body", &token[i * (j.output_width + 1)]);
		} else {
			print("token_ocr", tmp);
			xfree(tmp);
		}

		xfree(token);

		jpeg_finish_decompress(&j);
		jpeg_destroy_decompress(&j);

		xfree(buf[0]);
		fclose(f);
		
		unlink(file);
	} else
#else	/* HAVE_LIBJPEG */
	{
		char *file2 = saprintf("%s.jpg", file);

		if (rename(file, file2) == -1)
			print("token", file);
		else
			print("token", file2);

		xfree(file2);
	}
#endif	/* HAVE_LIBJPEG */

#else	/* HAVE_MKSTEMP */
	print("token_unsupported");
#endif	/* HAVE_MKSTEMP */


fail:
#ifdef HAVE_MKSTEMP
	xfree(file);
#endif
	list_remove(&watches, h, 0);
	gg_token_free(h);
}

/*
 * handle_userlist()
 *
 * funkcja zajmuj±ca siê zdarzeniami userlisty.
 *
 *  - h - delikwent.
 *
 * nie zwraca niczego.
 */
void handle_userlist(struct gg_event *e)
{
	switch (e->event.userlist.type) {
		case GG_USERLIST_GET_REPLY:
		{
			if (!userlist_get_config)
				print("userlist_get_ok");
			else
				print("userlist_config_get_ok");
			
			if (e->event.userlist.reply) {
				list_t l;

				for (l = userlist; l; l = l->next) {
					struct userlist *u = l->data;
					if (sess)
						gg_remove_notify_ex(sess, u->uin, userlist_type(u));
				}

				cp_to_iso(e->event.userlist.reply);
				userlist_set(e->event.userlist.reply, userlist_get_config);
				userlist_send();
				update_status();
				update_status_myip();

				for (l = userlist; l; l = l->next) {
					struct userlist *u = l->data;

					if (u->display)
						ui_event("userlist_changed", itoa(u->uin), u->display, NULL);
				}

				config_changed = 1;
			}

			break;
		}

		case GG_USERLIST_PUT_REPLY:
		{
			switch (userlist_put_config) {
				case 0:
					print("userlist_put_ok");
					break;
				case 1:
					print("userlist_config_put_ok");
					break;
				case 2:
					print("userlist_clear_ok");
					break;
				case 3:
					print("userlist_config_clear_ok");
					break;
			}

			break;
		}
	}
}

/*
 * handle_disconnect()
 *
 * funkcja obs³uguje ostrze¿enie o roz³±czeniu.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_disconnect(struct gg_event *e)
{
	struct in_addr addr;

	print("conn_disconnected");
	ui_event("disconnected");

	addr.s_addr = sess->server_addr;
	event_check(EVENT_CONNECTIONLOST, 0, inet_ntoa(addr));

	gg_logoff(sess);	/* a zobacz.. mo¿e siê uda ;> */
	list_remove(&watches, sess, 0);
	gg_free_session(sess);
	sess = NULL;	
	userlist_clear_status(0);
	update_status();
	last_conn_event = time(NULL);
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
	list_t l;

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
void remove_transfer(struct gg_dcc *d)
{
	struct transfer *t = find_transfer(d);

	if (t) {
		xfree(t->filename);
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
	list_t l;
	char *p;

	if (ignored_check(d->peer_uin) & IGNORE_DCC) {
		remove_transfer(d);
		list_remove(&watches, d, 0);
		gg_free_dcc(d);
		return;
	}
	
	if (!(e = gg_dcc_watch_fd(d))) {
		print("dcc_error", strerror(errno));
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

			if (config_dcc_limit) {
				int c, t = 60;
				char *tmp;
				
				if ((tmp = strchr(config_dcc_limit, '/')))
					t = atoi(tmp + 1);

				c = atoi(config_dcc_limit);

				if (time(NULL) - dcc_limit_time > t) {
					dcc_limit_time = time(NULL);
					dcc_limit_count = 0;
				}

				dcc_limit_count++;

				if (dcc_limit_count > c) {
					print("dcc_limit");
					config_dcc = 0;
					changed_dcc("dcc");

					dcc_limit_time = 0;
					dcc_limit_count = 0;

					gg_dcc_free(e->event.dcc_new);
					e->event.dcc_new = NULL;
					break;
				}
			}

			list_add(&watches, e->event.dcc_new, 0);
			e->event.dcc_new = NULL;
			break;

		case GG_EVENT_DCC_CLIENT_ACCEPT:
		{
			struct userlist *u;
			
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_CLIENT_ACCEPT\n");
			
			if (!(u = userlist_find(d->peer_uin, NULL)) || config_uin != d->uin) {
				gg_debug(GG_DEBUG_MISC, "## unauthorized client (uin=%ld), closing connection\n", d->peer_uin);
				list_remove(&watches, d, 0);
				gg_free_dcc(d);
				return;
			}

			if (config_dcc_filter && d->remote_addr != u->ip.s_addr) {
				char tmp[20];

				snprintf(tmp, sizeof(tmp), "%s", inet_ntoa(*((struct in_addr*) &d->remote_addr)));
				
				print("dcc_spoof", format_user(d->peer_uin), inet_ntoa(u->ip), tmp);
			}

			break;
		}

		case GG_EVENT_DCC_CALLBACK:
		{
			int found = 0;
			
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_CALLBACK\n");
			
			for (l = transfers; l; l = l->next) {
				struct transfer *t = l->data;

				gg_debug(GG_DEBUG_MISC, "// transfer id=%d, uin=%d, type=%d\n", t->id, t->uin, t->type);

				if (t->uin == d->peer_uin && !t->dcc) {
					gg_debug(GG_DEBUG_MISC, "## found transfer, uin=%d, type=%d\n", d->peer_uin, t->type);
					t->dcc = d;
					gg_dcc_set_type(d, t->type);
					found = 1;
					break;
				}
			}
			
			if (!found) {
				gg_debug(GG_DEBUG_MISC, "## connection from %d not found\n", d->peer_uin);
				list_remove(&watches, d, 0);
				gg_dcc_free(d);
			}
			
			break;	
		}

		case GG_EVENT_DCC_NEED_FILE_INFO:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_NEED_FILE_INFO\n");

			for (l = transfers; l; l = l->next) {
				struct transfer *t = l->data;

				if (t->dcc == d) {
					char *remote;

					remote = xstrdup(t->filename);
					iso_to_cp(remote);

					if (gg_dcc_fill_file_info2(d, remote, t->filename) == -1) {
						gg_debug(GG_DEBUG_MISC, "## gg_dcc_fill_file_info() failed (%s)\n", strerror(errno));
						print("dcc_open_error", t->filename);
						remove_transfer(d);
						list_remove(&watches, d, 0);
						gg_free_dcc(d);
						xfree(remote);
						break;
					}

					xfree(remote);
					
					break;
				}
			}
			break;
			
		case GG_EVENT_DCC_NEED_FILE_ACK:
		{
			char *path;
			struct stat st;

			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_NEED_FILE_ACK\n");
			/* ¿eby nie sprawdza³o, póki luser nie odpowie */
			list_remove(&watches, d, 0);

			if (!(t = find_transfer(d))) {
				tt.uin = d->peer_uin;
				tt.type = GG_SESSION_DCC_GET;
				tt.filename = NULL;
				tt.dcc = d;
				tt.id = transfer_id();
				t = list_add(&transfers, &tt, sizeof(tt));
			}

			for (p = d->file_info.filename; *p; p++)
				if (*p < 32 || *p == '\\' || *p == '/')
					*p = '_';

			if (d->file_info.filename[0] == '.')
				d->file_info.filename[0] = '_';

			t->type = GG_SESSION_DCC_GET;
			t->filename = xstrdup(d->file_info.filename);
			cp_to_iso(t->filename);

			print("dcc_get_offer", format_user(t->uin), t->filename, itoa(d->file_info.size), itoa(t->id));

			if (config_dcc_dir)
				path = saprintf("%s/%s", config_dcc_dir, t->filename);
			else
				path = xstrdup(t->filename);

			if (!stat(path, &st) && st.st_size < d->file_info.size)
				print("dcc_get_offer_resume", format_user(t->uin), t->filename, itoa(d->file_info.size), itoa(t->id));
			
			xfree(path);

			if (!(ignored_check(t->uin) & IGNORE_EVENTS))
				event_check(EVENT_DCC, t->uin, t->filename);

			break;
		}
			
		case GG_EVENT_DCC_NEED_VOICE_ACK:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_NEED_VOICE_ACK\n");
#ifdef HAVE_VOIP
			/* ¿eby nie sprawdza³o, póki luser nie odpowie */
			list_remove(&watches, d, 0);

			if (!(t = find_transfer(d))) {
				tt.uin = d->peer_uin;
				tt.type = GG_SESSION_DCC_VOICE;
				tt.filename = NULL;
				tt.dcc = d;
				tt.id = transfer_id();
				if (!(t = list_add(&transfers, &tt, sizeof(tt)))) {
					gg_free_dcc(d);
					break;
				}
			}
			
			t->type = GG_SESSION_DCC_VOICE;

			print("dcc_voice_offer", format_user(t->uin), itoa(t->id));
#else
			list_remove(&watches, d, 0);
			remove_transfer(d);
			gg_free_dcc(d);
#endif
			break;

		case GG_EVENT_DCC_VOICE_DATA:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_VOICE_DATA\n");

#ifdef HAVE_VOIP
			voice_open();
			voice_play(e->event.dcc_voice_data.data, e->event.dcc_voice_data.length, 0);
#endif
			break;
			
		case GG_EVENT_DCC_DONE:
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_DONE\n");

			if (!(t = find_transfer(d))) {
				gg_free_dcc(d);
				break;
			}

			event_check(EVENT_DCCFINISH, t->uin, t->filename);

			print((t->dcc->type == GG_SESSION_DCC_SEND) ? "dcc_done_send" : "dcc_done_get", format_user(t->uin), t->filename);
			
			remove_transfer(d);
			list_remove(&watches, d, 0);
			gg_free_dcc(d);

			break;
			
		case GG_EVENT_DCC_ERROR:
		{
			struct in_addr addr;
			unsigned short port = d->remote_port;
			char *tmp;
		
			addr.s_addr = d->remote_addr;

			if (d->peer_uin) {
				struct userlist *u = userlist_find(d->peer_uin, NULL);
				if (!addr.s_addr && u) {
					addr.s_addr = u->ip.s_addr;
					port = u->port;
				}
				tmp = saprintf("%s (%s:%d)", xstrdup(format_user(d->peer_uin)), inet_ntoa(addr), port);
			} else 
				tmp = saprintf("%s:%d", inet_ntoa(addr), port);
			
			switch (e->event.dcc_error) {
				case GG_ERROR_DCC_HANDSHAKE:
					print("dcc_error_handshake", tmp);
					break;
				case GG_ERROR_DCC_NET:
					print("dcc_error_network", tmp);
					break;
				case GG_ERROR_DCC_REFUSED:
					print("dcc_error_refused", tmp);
					break;
				default:
					print("dcc_error_unknown", tmp);
			}

			xfree(tmp);

#ifdef HAVE_VOIP
			if (d->type == GG_SESSION_DCC_VOICE)
				voice_close();
#endif  /* HAVE_VOIP */

			remove_transfer(d);
			list_remove(&watches, d, 0);
			gg_free_dcc(d);

			break;
		}
	}

	gg_event_free(e);
	
	return;
}

/*
 * handle_voice()
 *
 * obs³uga danych przychodz±cych z urz±dzenia wej¶ciowego.
 *
 *  - c - struktura opisuj±ca urz±dzenie wej¶ciowe.
 *
 * brak.
 */
void handle_voice(struct gg_common *c)
{
#ifdef HAVE_VOIP
	list_t l;
	struct gg_dcc *d = NULL;
	char buf[GG_DCC_VOICE_FRAME_LENGTH_505];	/* d³u¿szy z buforów */
	int length = GG_DCC_VOICE_FRAME_LENGTH;
	
	for (l = transfers; l; l = l->next) {
		struct transfer *t = l->data;

		if (t->type == GG_SESSION_DCC_VOICE && t->dcc && (t->dcc->state == GG_STATE_READING_VOICE_HEADER || t->dcc->state == GG_STATE_READING_VOICE_SIZE || t->dcc->state == GG_STATE_READING_VOICE_DATA)) {
			d = t->dcc;
			length = (t->protocol >= 0x1b) ? GG_DCC_VOICE_FRAME_LENGTH_505 : GG_DCC_VOICE_FRAME_LENGTH;
			break;
		}
	}

	/* póki nie mamy po³±czenia, i tak czytamy z /dev/dsp */

	if (!d) {
		voice_record(buf, length, 1);	/* XXX b³êdy */
		return;
	} else {
		voice_record(buf, length, 0);	/* XXX b³êdy */
		if (config_audio_device && config_audio_device[0] != '-')
			gg_dcc_voice_send(d, buf, length);	/* XXX b³êdy */
	}
#endif /* HAVE_VOIP */
}

/*
 * handle_search50()
 *
 * zajmuje siê obs³ug± wyniku przeszukiwania katalogu publicznego.
 *
 *  - e - opis zdarzenia
 */
void handle_search50(struct gg_event *e)
{
	gg_pubdir50_t res = e->event.pubdir50;
	int i, count, all = 0;
	list_t l;
	uin_t last_uin = 0;

	if ((count = gg_pubdir50_count(res)) < 1) {
		print("search_not_found");
		return;
	}

	gg_debug(GG_DEBUG_MISC, "handle_search50, count = %d\n", gg_pubdir50_count(res));

	for (l = searches; l; l = l->next) {
		gg_pubdir50_t req = l->data;

		if (gg_pubdir50_seq(req) == gg_pubdir50_seq(res)) {
			all = 1;
			break;
		}
	}

	for (i = 0; i < count; i++) {
		const char *__fmnumber = gg_pubdir50_get(res, i, "fmnumber");
		const char *uin = (__fmnumber) ? __fmnumber : "?";

		const char *__firstname = gg_pubdir50_get(res, i, "firstname");
		char *firstname = xstrdup((__firstname) ? __firstname : "");

		const char *__lastname = gg_pubdir50_get(res, i, "lastname");
		char *lastname = xstrdup((__lastname) ? __lastname : "");
		
		const char *__nickname = gg_pubdir50_get(res, i, "nickname");
		char *nickname = xstrdup((__nickname) ? __nickname : "");

		const char *__fmstatus = gg_pubdir50_get(res, i, "fmstatus");
		int status = (__fmstatus) ? atoi(__fmstatus) : GG_STATUS_NOT_AVAIL;

		const char *__birthyear = gg_pubdir50_get(res, i, "birthyear");
		const char *birthyear = (__birthyear && strcmp(__birthyear, "0")) ? __birthyear : "-";

		const char *__city = gg_pubdir50_get(res, i, "city");
		char *city = xstrdup((__city) ? __city : "");

		char *name, *active, *gender;

		const char *target = NULL;

		cp_to_iso(firstname);
		cp_to_iso(lastname);
		cp_to_iso(nickname);
		cp_to_iso(city);

		if (count == 1 && !all) {
			xfree(last_search_first_name);
			xfree(last_search_last_name);
			xfree(last_search_nickname);
			last_search_first_name = xstrdup(firstname);
			last_search_last_name = xstrdup(lastname);
			last_search_nickname = xstrdup(nickname);
			last_search_uin = atoi(uin);
		}

		name = saprintf("%s %s", firstname, lastname);

#define __format(x) ((count == 1 && !all) ? "search_results_single" x : "search_results_multi" x)

		switch (status & 0x7f) {
			case GG_STATUS_AVAIL:
			case GG_STATUS_AVAIL_DESCR:
				active = format_string(format_find(__format("_active")), (__firstname) ? __firstname : nickname);
				break;
			case GG_STATUS_BUSY:
			case GG_STATUS_BUSY_DESCR:
				active = format_string(format_find(__format("_busy")), (__firstname) ? __firstname : nickname);
				break;
			case GG_STATUS_INVISIBLE:
			case GG_STATUS_INVISIBLE_DESCR:
				active = format_string(format_find(__format("_invisible")), (__firstname) ? __firstname : nickname);
				break;
			default:
				active = format_string(format_find(__format("_inactive")), (__firstname) ? __firstname : nickname);
		}

		gender = format_string(format_find(__format("_unknown")), "");

		for (l = autofinds; l; l = l->next) {
			uin_t *d = l->data;

			if (*d == atoi(uin)) {
				target = uin;
				break;
			}
		}
		
		print_window(target, 0, __format(""), uin, name, nickname, city, birthyear, gender, active);

#undef __format

		xfree(name);
		xfree(active);
		xfree(gender);

		xfree(firstname);
		xfree(lastname);
		xfree(nickname);
		xfree(city);

		last_uin = atoi(uin);
	}

	/* je¶li mieli¶my ,,/find --all'', szukamy dalej */
	for (l = searches; l; l = l->next) {
		gg_pubdir50_t req = l->data;
		uin_t next;

		if (gg_pubdir50_seq(req) != gg_pubdir50_seq(res))
			continue;

		/* nie ma dalszych? to dziêkujemy */
		if (!(next = gg_pubdir50_next(res)) || !sess || next < last_uin) {
			list_remove(&searches, req, 0);
			gg_pubdir50_free(req);
			break;
		}

		gg_pubdir50_add(req, GG_PUBDIR50_START, itoa(next));
		gg_pubdir50(sess, req);

		break;
	}
}

/*
 * handle_change50()
 *
 * zajmuje siê obs³ug± zmiany danych w katalogu publicznym.
 *
 *  - e - opis zdarzenia
 */
void handle_change50(struct gg_event *e)
{
	if (!change_quiet)
		print("change");
}

/*
 * handle_image_request()
 *
 * obs³uguje ¿±danie wys³ania obrazków.
 *
 *  - e - opis zdarzenia
 */
void handle_image_request(struct gg_event *e)
{
	gg_debug(GG_DEBUG_MISC, "// ekg: image_request: sender=%d, size=%d, crc32=%.8x\n", e->event.image_request.sender, e->event.image_request.size, e->event.image_request.crc32);
}

/*
 * handle_image_reply()
 *
 * obs³uguje odpowied¼ obrazków.
 * 
 *  - e - opis zdarzenia
 */
void handle_image_reply(struct gg_event *e)
{
	struct userlist *u;
	list_t l;

	gg_debug(GG_DEBUG_MISC, "// ekg: image_reply: sender=%d, filename=\"%s\", size=%d, crc32=%.8x\n", e->event.image_reply.sender, e->event.image_reply.filename, e->event.image_reply.size, e->event.image_reply.crc32);

	if (e->event.image_request.crc32 != GG_CRC32_INVISIBLE)
		return;

	u = userlist_find(e->event.image_reply.sender, NULL);

	if (u) {
		for (l = spiedlist; l; l = l->next) {
			struct spied *s = l->data;

			if (s->uin == u->uin) {
				int sec;
				int msec;
				struct timeval now;

				gettimeofday(&now, NULL);

				if (now.tv_usec < s->request_sent.tv_usec) {
					sec = now.tv_sec - s->request_sent.tv_sec - 1;
					msec = (now.tv_usec - s->request_sent.tv_usec + 1000000) / 1000;
				} else {
					sec = now.tv_sec - s->request_sent.tv_sec;
					msec = (now.tv_usec - s->request_sent.tv_usec) / 1000;
				}

				gg_debug(GG_DEBUG_MISC, "// ekg: image_reply: round-trip-time %d.%03d\n", sec, msec);

				list_remove(&spiedlist, s, 1);
				break;
			}
		}
	}

	if (u && group_member(u, "spied")) {

		if (GG_S_NA(u->status)) {
			int status = (GG_S_D(u->status)) ? GG_STATUS_INVISIBLE_DESCR : GG_STATUS_INVISIBLE;
			iso_to_cp(u->descr);
			handle_common(u->uin, status, u->descr, time(NULL), u->ip.s_addr, u->port, u->protocol, u->image_size);
		}

	} else {

		if (u)
			print("user_is_connected", format_user(e->event.image_reply.sender), (u->first_name) ? u->first_name : u->display); 
		else
			print("user_is_connected", format_user(e->event.image_reply.sender), itoa(e->event.image_reply.sender)); 
	}
}
