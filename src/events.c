/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Piotr Wysocki <wysek@linux.bydg.org>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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
#ifdef WITH_PYTHON
#  include "python.h"
#endif
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

void handle_msg(), handle_ack(), handle_status(), handle_notify(),
	handle_success(), handle_failure(), handle_search50(),
	handle_change50(), handle_status60(), handle_notify60();

static int hide_notavail = 0;	/* czy ma ukrywa� niedost�pnych -- tylko zaraz po po��czeniu */

static int dcc_limit_time = 0;	/* czas pierwszego liczonego po��czenia */
static int dcc_limit_count = 0;	/* ilo�� po��cze� od ostatniego razu */

static int auto_find_limit = 100; /* ilo�� os�b, kt�rych nie znamy, a szukali�my po odebraniu msg */

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
	{ 0, NULL }
};

/*
 * print_message()
 *
 * funkcja �adnie formatuje tre�� wiadomo�ci, zawija linijki, wy�wietla
 * kolorowe ramki i takie tam.
 *
 *  - e - zdarzenie wiadomo�ci
 *  - u - wpis u�ytkownika w userli�cie
 *  - chat - rodzaj wiadomo�ci (0 - msg, 1 - chat, 2 - sysmsg)
 *  - secure - czy wiadomo�� jest bezpieczna
 *
 * nie zwraca niczego. efekt wida� na ekranie.
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

	/* tworzymy map� formatowanego tekstu. dla ka�dego znaku wiadomo�ci
	 * zapisujemy jeden znak koloru z docs/themes.txt lub \0 je�li nie
	 * trzeba niczego zmienia�. */

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

		/* teraz powtarzamy formaty tam, gdzie jest przej�cie do
		 * nowej linii i odst�py. dzi�ki temu oszcz�dzamy sobie
		 * mieszania ni�ej w kodzie. */

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

	/* je�eli chcemy, dodajemy do bufora ,,last'' wiadomo��... */
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

		/* zmniejsz d�ugo�� pierwszej linii o d�ugo�� prefiksu z rozm�wc�, timestampem itd. */
		tmp = format_string(format_find(format), "", format_user(e->event.msg.sender), timestr, cname);
		mem_width = width + strlen(tmp);
		for (p = tmp; *p && *p != '\n'; p++) {
			if (*p == 27) {
				/* pomi� kolorki */
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
	mesg = save = xstrdup(e->event.msg.message);

	for (i = 0; i < strlen(mesg); i++)	/* XXX �adniejsze taby */
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

			if (config_wap_enabled == 2) {
				strftime(waptime2, sizeof(waptime2), "%H:%M%S", tm);
				snprintf(waptime, sizeof(waptime), "wap%7s_%d", waptime2, e->event.msg.sender);

				if ((waplog = prepare_path(waptime, 1)) && (wap = fopen(waplog, "a"))) {
					fprintf(wap, "%s;%s\n", target, line);
					fclose(wap);
				}

			} else {
				strftime(waptime2, sizeof(waptime2), "%H:%M", tm);
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
 * funkcja obs�uguje przychodz�ce wiadomo�ci.
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
	


	if (!(ignored_check(e->event.msg.sender) & IGNORE_EVENTS))
		event_check((chat) ? EVENT_CHAT : EVENT_MSG, e->event.msg.sender, e->event.msg.message);
			
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

	if (config_sms_away && (GG_S_B(config_status) || (GG_S_I(config_status) && config_sms_away & 4)) && config_sms_app && config_sms_number) {
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

			/* niech nie wysy�a sms�w, je�li brakuje format�w */
			if (strcmp(foo, ""))
				send_sms(config_sms_number, foo, 0);
	
			xfree(foo);
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

	if (e->event.msg.formats_length > 0) {
		int i;
		
		gg_debug(GG_DEBUG_MISC, "// ekg: received formatting info (len=%d):", e->event.msg.formats_length);
		for (i = 0; i < e->event.msg.formats_length; i++)
			gg_debug(GG_DEBUG_MISC, " %.2x", ((unsigned char*)e->event.msg.formats)[i]);
		gg_debug(GG_DEBUG_MISC, "\n");
	}
}

/*
 * handle_ack()
 *
 * funkcja obs�uguje potwierdzenia wiadomo�ci.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_ack(struct gg_event *e)
{
	struct userlist *u = userlist_find(e->event.ack.recipient, NULL);
	int queued = (e->event.ack.status == GG_ACK_QUEUED), filtered = 0;
	const char *tmp, *target = ((u && u->display) ? u->display : itoa(e->event.ack.recipient));

	if (!e->event.ack.seq)	/* ignorujemy potwierdzenia ctcp */
		return;

	msg_queue_remove(e->event.ack.seq);

	if (!(ignored_check(e->event.ack.recipient) & IGNORE_EVENTS))
		event_check((queued) ? EVENT_QUEUED : EVENT_DELIVERED, e->event.ack.recipient, NULL);

	if (u && !queued && GG_S_NA(u->status) && !(ignored_check(u->uin) & IGNORE_STATUS)) {
		filtered = 1;
		print_window(target, 0, "ack_filtered", format_user(e->event.ack.recipient));
	}

	if (!config_display_ack)
		return;

	if (config_display_ack == 2 && queued)
		return;

	if (config_display_ack == 3 && !queued)
		return;

	if (!filtered) {
		tmp = queued ? "ack_queued" : "ack_delivered";
		print_window(target, 0, tmp, format_user(e->event.ack.recipient));
	}
}

/*
 * handle_common()
 *
 * ujednolicona obs�uga zmiany w userli�cie dla handle_status()
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
static void handle_common(uin_t uin, int status, const char *idescr, int dtime, uint32_t ip, uint16_t port, int version, int image_size)
{
	struct userlist *u;
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
	int ignore_status, ignore_status_descr, ignore_events, ignore_notify;
	unsigned char *descr = NULL;
#ifdef WITH_PYTHON
	list_t l;
#endif

	/* nie pokazujemy nieznajomych */
	if (!(u = userlist_find(uin, NULL)))
		return;

	ignore_status = ignored_check(uin) & IGNORE_STATUS;
	ignore_status_descr = ignored_check(uin) & IGNORE_STATUS_DESCR;
	ignore_events = ignored_check(uin) & IGNORE_EVENTS;
	ignore_notify = ignored_check(uin) & IGNORE_NOTIFY;

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

#define __USER_QUITING ((GG_S_NA(status) || GG_S_I(status)) && !(GG_S_NA(u->status) || GG_S_I(u->status)))

	if (GG_S_BL(status) && !GG_S_BL(u->status)) {
		u->status = status;	/* poza list� stan�w */
		if (!ignore_events)
			event_check(EVENT_BLOCKED, uin, NULL);
	}

	if (!descr) 
		descr = xstrdup(idescr);
	
	/* zapami�taj adres, port i protok� */
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

	/* je�li status taki sam i ewentualnie opisy te same, ignoruj */
	if (!GG_S_D(status) && (u->status == status)) {
		xfree(descr);
		return;
	}
	
	/* je�li stan z opisem, a opisu brak, wpisz pusty tekst */
	if (GG_S_D(status) && !descr)
		descr = xstrdup("");

	if (descr) {
		unsigned char *tmp;

		for (tmp = descr; *tmp; tmp++) {
			if (*tmp == 13 || *tmp == 10 || *tmp == 9)
				*tmp = ' ';
			if (*tmp < 32)
				*tmp = '?';
		}

		cp_to_iso(descr);
	}

	if (GG_S_D(status) && (u->status == status) && u->descr && !strcmp(u->descr, descr)) {
		xfree(descr);
		return;
	}

	/* jesli kto� nam znika, to sobie to zapamietujemy */
	if (__USER_QUITING) {
		u->last_seen = time(NULL);
		xfree(u->last_descr);
		u->last_descr = xstrdup(u->descr);
	}

#undef __USER_QUITING

	prev_status = u->status;
	
	for (s = st; s->status; s++) {
		/* je�li nie ten, sprawdzaj dalej */
		if (status != s->status)
			continue;

		if (GG_S_NA(s->status)) {
			memset(&u->ip, 0, sizeof(struct in_addr));
			u->port = 0;
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

		if (ignore_status || ignore_notify)
			break;

		/* zaloguj */
		if (config_log_status && (!GG_S_D(s->status) || !descr))
			put_log(uin, "status,%ld,%s,%s:%d,%s,%s\n", uin, ((u->display) ? u->display : ""), inet_ntoa(u->ip), u->port, log_timestamp(time(NULL)), s->log);
		if (config_log_status && GG_S_D(s->status) && descr)
		    	put_log(uin, "status,%ld,%s,%s:%d,%s,%s,%s\n", uin, ((u->display) ? u->display : ""), inet_ntoa(u->ip), u->port, log_timestamp(time(NULL)), s->log, descr);

		/* jak dost�pny lub zaj�ty, dopiszmy do taba
		 * jak niedost�pny, usu�my */
		if (GG_S_A(s->status) && config_completion_notify && u->display) 
			add_send_nick(u->display);
		if (GG_S_B(s->status) && (config_completion_notify & 4) && u->display)
			add_send_nick(u->display);
		if (GG_S_NA(s->status) && (config_completion_notify & 2) && u->display)
			remove_send_nick(u->display);

		/* wy�wietla� na ekranie ? */
		if (!config_display_notify || hide)
			break;

		if (config_display_notify == 2) {
			/* je�li na zaj�ty, ignorujemy */
			if (GG_S_B(s->status) && !GG_S_NA(prev_status))
				break;

			/* je�li na dost�pny i nie by� niedost�pny, ignoruj */
			if (GG_S_A(s->status) && !GG_S_NA(prev_status))
				break;
		}

		/* czy ukrywa� niedost�pnych */
		if (hide_notavail) {
			if (GG_S_NA(s->status) && GG_S_NA(u->status))
				break;
			else if (time(NULL) - last_conn_event >= config_events_delay)
				hide_notavail = 0;
		}

		/* daj zna� d�wi�kiem */
		if (config_beep && config_beep_notify)
			ui_beep();

		/* i muzyczk� */
		if (config_sound_notify_file && strcmp(config_sound_notify_file, "") && (!config_events_delay || (time(NULL) - last_conn_event) >= config_events_delay))
			play_sound(config_sound_notify_file);

#ifdef WITH_UI_NCURSES
		if (ui_init == ui_ncurses_init && config_contacts == 2)
			break;
#endif
			
		/* no dobra, poka� */
		if (u->display)
			print_window(u->display, 0, s->format, format_user(uin), (u->first_name) ? u->first_name : u->display, descr);

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
 * funkcja obs�uguje list� obecnych.
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
 * funkcja obs�uguje list� obecnych w wersji 6.0.
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
 * funkcja obs�uguje zmian� stanu ludzi z listy kontakt�w.
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
 * funkcja obs�uguje zmian� stanu ludzi z listy kontakt�w w wersji 6.0.
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
 * funkcja obs�uguje b��dy przy po��czeniu.
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

	/* je�li si� nie powiod�o, usuwamy nasz serwer i ��czymy przez huba */
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
 * funkcja obs�uguje udane po��czenia.
 *
 *  - e - opis zdarzenia.
 */
void handle_success(struct gg_event *e)
{
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
		struct in_addr addr;

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

	update_status();
	update_status_myip();

	last_conn_event = time(NULL);
}

/*
 * handle_event()
 *
 * funkcja obs�uguje wszystkie zdarzenia dotycz�ce danej sesji GG.
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
		print("conn_broken");
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
 * funkcja zajmuj�ca si� wszelkimi zdarzeniami http opr�cz szukania.
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
		xfree(config_email);
		config_email = reg_email;
		reg_email = NULL;
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
 * handle_userlist()
 *
 * funkcja zajmuj�ca si� zdarzeniami userlisty.
 *
 *  - h - delikwent.
 *
 * nie zwraca niczego.
 */
void handle_userlist(struct gg_http *h)
{
	const char *format_ok, *format_error;
	
	if (!h)
		return;
	
	if (!h->user_data) {
		format_ok = (h->type == GG_SESSION_USERLIST_GET) ? "userlist_get_ok" : "userlist_put_ok";
		format_error = (h->type == GG_SESSION_USERLIST_GET) ? "userlist_get_error" : "userlist_put_error";
	} else {
		if (h->user_data == (char *) 1) {
			format_ok = (h->type == GG_SESSION_USERLIST_GET) ? "userlist_config_get_ok" : "userlist_config_put_ok";
			format_error = (h->type == GG_SESSION_USERLIST_GET) ? "userlist_config_get_error" : "userlist_config_put_error";
		} else {
			format_ok = (h->user_data == (char *) 2) ? "userlist_clear_ok" : "userlist_config_clear_ok";
			format_error = (h->user_data == (char *) 2) ? "userlist_clear_error" : "userlist_config_clear_error";
		}
	}

	if (h->callback(h) || h->state == GG_STATE_ERROR) {
		print(format_error, strerror(errno));
		list_remove(&watches, h, 0);
		h->destroy(h);
		return;
	}
	
	if (h->state != GG_STATE_DONE)
		return;

	print((h->data) ? format_ok : format_error);
		
	if (h->type == GG_SESSION_USERLIST_GET && h->data) {
		list_t l;

		for (l = userlist; l; l = l->next) {
			struct userlist *u = l->data;
			if (sess)
				gg_remove_notify_ex(sess, u->uin, userlist_type(u));
		}

		cp_to_iso(h->data);
		userlist_set(h->data, (h->user_data) ? 1 : 0);
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

	list_remove(&watches, h, 0);
	h->destroy(h);
}

/*
 * handle_disconnect()
 *
 * funkcja obs�uguje ostrze�enie o roz��czeniu.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_disconnect(struct gg_event *e)
{
	print("conn_disconnected");
	ui_event("disconnected");

	gg_logoff(sess);	/* a zobacz.. mo�e si� uda ;> */
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
 * znajduje struktur� ,,transfer'' dotycz�c� danego po��czenia.
 *
 *  - d - struktura gg_dcc, kt�rej szukamy.
 *
 * wska�nik do struktury ,,transfer'' lub NULL, je�li nie znalaz�.
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
 * usuwa z listy transfer�w ten, kt�ry dotyczy podanego po��czenia dcc.
 *
 *  - d - po��czenie.
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
 * funkcja zajmuje si� obs�ug� wszystkich zdarze� zwi�zanych z DCC.
 *
 *  - d - struktura danego po��czenia.
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
			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_CLIENT_ACCEPT\n");
			
			if (!userlist_find(d->peer_uin, NULL) || config_uin != d->uin) {
				gg_debug(GG_DEBUG_MISC, "## unauthorized client (uin=%ld), closing connection\n", d->peer_uin);
				list_remove(&watches, d, 0);
				gg_free_dcc(d);
				return;
			}
			break;	

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
					if (gg_dcc_fill_file_info(d, t->filename) == -1) {
						gg_debug(GG_DEBUG_MISC, "## gg_dcc_fill_file_info() failed (%s)\n", strerror(errno));
						print("dcc_open_error", t->filename);
						remove_transfer(d);
						list_remove(&watches, d, 0);
						gg_free_dcc(d);
						break;
					}
					break;
				}
			}
			break;
			
		case GG_EVENT_DCC_NEED_FILE_ACK:
		{
			char *path;
			struct stat st;

			gg_debug(GG_DEBUG_MISC, "## GG_EVENT_DCC_NEED_FILE_ACK\n");
			/* �eby nie sprawdza�o, p�ki luser nie odpowie */
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
			/* �eby nie sprawdza�o, p�ki luser nie odpowie */
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
 * obs�uga danych przychodz�cych z urz�dzenia wej�ciowego.
 *
 *  - c - struktura opisuj�ca urz�dzenie wej�ciowe.
 *
 * brak.
 */
void handle_voice(struct gg_common *c)
{
#ifdef HAVE_VOIP
	list_t l;
	struct gg_dcc *d = NULL;
	char buf[GG_DCC_VOICE_FRAME_LENGTH_505];	/* d�u�szy z bufor�w */
	int length = GG_DCC_VOICE_FRAME_LENGTH;
	
	for (l = transfers; l; l = l->next) {
		struct transfer *t = l->data;

		if (t->type == GG_SESSION_DCC_VOICE && t->dcc && (t->dcc->state == GG_STATE_READING_VOICE_HEADER || t->dcc->state == GG_STATE_READING_VOICE_SIZE || t->dcc->state == GG_STATE_READING_VOICE_DATA)) {
			d = t->dcc;
			length = (t->protocol >= 0x1b) ? GG_DCC_VOICE_FRAME_LENGTH_505 : GG_DCC_VOICE_FRAME_LENGTH;
			break;
		}
	}

	/* p�ki nie mamy po��czenia, i tak czytamy z /dev/dsp */

	if (!d) {
		voice_record(buf, length, 1);	/* XXX b��dy */
		return;
	} else {
		voice_record(buf, length, 0);	/* XXX b��dy */
		if (config_audio_device && config_audio_device[0] != '-')
			gg_dcc_voice_send(d, buf, length);	/* XXX b��dy */
	}
#endif /* HAVE_VOIP */
}

/*
 * handle_search50()
 *
 * zajmuje si� obs�ug� wyniku przeszukiwania katalogu publicznego.
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

	/* je�li mieli�my ,,/find --all'', szukamy dalej */
	for (l = searches; l; l = l->next) {
		gg_pubdir50_t req = l->data;
		uin_t next;

		if (gg_pubdir50_seq(req) != gg_pubdir50_seq(res))
			continue;

		/* nie ma dalszych? to dzi�kujemy */
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
 * zajmuje si� obs�ug� zmiany danych w katalogu publicznym.
 *
 *  - e - opis zdarzenia
 */
void handle_change50(struct gg_event *e)
{
	if (!change_quiet)
		print("change");
}
