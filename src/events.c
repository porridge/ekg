/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include <stdio.h>
#include <unistd.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "config.h"
#include "libgadu.h"
#include "stuff.h"
#include "events.h"
#include "commands.h"
#include "themes.h"
#include "userlist.h"
#include "voice.h"
#include "xmalloc.h"
#include "ui.h"
#ifdef HAVE_OPENSSL
#  include "sim.h"
#endif
#include "msgqueue.h"
#ifdef WITH_PYTHON
#  include "python.h"
#endif

void handle_msg(), handle_ack(), handle_status(), handle_notify(),
	handle_success(), handle_failure();

static int hide_notavail = 0;	/* czy ma ukrywaæ niedostêpnych -- tylko zaraz po po³±czeniu */

static struct handler handlers[] = {
	{ GG_EVENT_MSG, handle_msg },
	{ GG_EVENT_ACK, handle_ack },
	{ GG_EVENT_STATUS, handle_status },
	{ GG_EVENT_NOTIFY, handle_notify },
	{ GG_EVENT_NOTIFY_DESCR, handle_notify },
	{ GG_EVENT_CONN_SUCCESS, handle_success },
	{ GG_EVENT_CONN_FAILED, handle_failure },
	{ GG_EVENT_DISCONNECT, handle_disconnect },
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
	int width, next_width, i, j, mem_width = 0, tt, t = e->event.msg.time;
	int separate = (e->event.msg.sender != config_uin || chat == 3);
	int timestamp_type = 0;
	char *mesg, *buf, *line, *next, *format = NULL, *format_first = "";
	char *next_format = NULL, *head = NULL, *foot = NULL;
	char *timestamp = NULL, *save, *secure_label = NULL;
	char *line_width = NULL, timestr[100], *target, *cname;
	char *formatmap = NULL;
	struct tm *tm;
	struct conference *c = NULL;

	/* tworzymy mapê formatowanego tekstu. dla ka¿dego znaku wiadomo¶ci
	 * zapisujemy jeden znak koloru z docs/themes.txt lub 0 je¶li nie
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

			if ((p[i + 2] & GG_FONT_COLOR)) {
				/* XXX mapowanie kolorów */
			}

			if ((p[i + 2] & 7))
				formatmap[pos] = attrmap[p[i + 2] & 7];

			i += (p[i + 2] & GG_FONT_COLOR) ? 6 : 3;
		}

		/* teraz powtarzamy formaty tam, gdzie jest przej¶cie do
		 * nowej linii. dziêki temu oszczêdzamy sobie mieszania
		 * ni¿ej w kodzie. */

		for (i = 0; i < strlen(e->event.msg.message); i++) {
			if (formatmap[i])
				last_attr = formatmap[i];

			if (i > 0 && e->event.msg.message[i - 1] == '\n')
				formatmap[i] = last_attr;
		}
	}

	if (secure)
		secure_label = format_string(format_find("secure"));
	
	if (e->event.msg.recipients) {
		c = conference_find_by_uins(e->event.msg.sender, 
			e->event.msg.recipients, e->event.msg.recipients_count);

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
			target = xstrdup((chat == 2) ? "__status" : ((u) ? u->display : itoa(e->event.msg.sender)));
	} else
	        target = xstrdup((chat == 2) ? "__status" : ((u) ? u->display : itoa(e->event.msg.sender)));
	cname = (c ? c->name : "");

	tt = time(NULL);

	if (t - config_time_deviation <= tt && tt <= t + config_time_deviation)
		timestamp_type = 2;
	else if (tt / 86400 == t / 86400)
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
	
	tm = localtime(&e->event.msg.time);
	strftime(timestr, sizeof(timestr), format_find(timestamp), tm);

	if (!(width = atoi(format_find(line_width))))
		width = ui_screen_width - 2;

	if (width < 0)
		width = ui_screen_width + width;

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
	mesg = save = xstrdup(e->event.msg.message);

	for (i = 0; i < strlen(mesg); i++)	/* XXX ³adniejsze taby */
		if (mesg[i] == '\t')
			mesg[i] = ' ';
	
	while ((line = gg_get_line(&mesg))) {
		int buf_offset;

#ifdef WITH_WAP
		{
			FILE *wap;
			char waptime[10], waptime2[10];
			const char *waplog;

			strftime(waptime2, sizeof(waptime2), "%H:%M", tm);

			sprintf(waptime, "wap%5s", waptime2);
			if ((waplog = prepare_path(waptime, 1))) {
				if ((wap = fopen(waplog, "a"))) {
					fprintf(wap,"%s(%s):%s\n", target, waptime2, line);
					fclose(wap);
				}
			}
		}
#endif

		for (; strlen(line); line = next) {
			char *emotted = NULL, *formatted;

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
			
			buf_offset = (int) (line - save);

			if (formatmap) {
				string_t s = string_init("");
				int i;

				for (i = 0; i < strlen(buf); i++) {
					if (formatmap[buf_offset + i])
						string_append(s, format_ansi(formatmap[buf_offset + i]));

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
	char *tmp;
	
#ifdef WITH_PYTHON
	list_t l;
	
	for (l = modules; l; l = l->next) {
		struct module *m = l->data;
		PyObject *res;

		if (!m->handle_msg)
			continue;

		cp_to_iso(e->event.msg.message);

		res = PyObject_CallFunction(m->handle_msg, "(isisii)", e->event.msg.sender, (u) ? u->display : "", e->event.msg.msgclass, e->event.msg.message, e->event.msg.time, 0);

		iso_to_cp(e->event.msg.message);

		if (!res)
			PyErr_Print();

		if (res && PyInt_Check(res)) {
			switch (PyInt_AsLong(res)) {
				case 0:
					Py_XDECREF(res);
					return;
				case 2:
					hide = 2;
			}
		}

		if (res && PyTuple_Check(res)) {
			char *b, *d;
			int f;

			if (PyArg_ParseTuple(res, "isisii", &e->event.msg.sender, &b, &e->event.msg.msgclass, &d, &e->event.msg.time, &f)) {
				xfree(e->event.msg.message);
				e->event.msg.message = xstrdup(d);
			} else
				PyErr_Print();
		}

		Py_XDECREF(res);
	}
#endif
	
	if (!e->event.msg.message)
		return;

	if (e->event.msg.recipients_count) {
		struct conference *c = conference_find_by_uins(
			e->event.msg.sender, e->event.msg.recipients,
			e->event.msg.recipients_count);

		if (c && c->ignore)
			return;
	}

	if (ignored_check(e->event.msg.sender)) {
		if (config_log_ignored) {
			char *tmp;
			cp_to_iso(e->event.msg.message);
			tmp = log_escape(e->event.msg.message);
			/* XXX eskejpowanie */
			put_log(e->event.msg.sender, "%sign,%ld,%s,%s,%s,%s\n", (chat) ? "chatrecv" : "msgrecv", e->event.msg.sender, (u) ? u->display : "", log_timestamp(time(NULL)), log_timestamp(e->event.msg.time), tmp);
			xfree(tmp);
		}
		return;
	}

	if ((e->event.msg.msgclass & GG_CLASS_CTCP)) {
		gg_debug(GG_DEBUG_MISC, "// ekg: received ctcp\n");

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
	if (config_encryption == 1 && !strncmp(e->event.msg.message, "-----BEGIN RSA PUBLIC KEY-----", 20)) {
		char *name;
		const char *target = (u) ? u->display : itoa(e->event.msg.sender);
		FILE *f;

		print_window(target, 0, "public_key_received", format_user(e->event.msg.sender));	

		mkdir(prepare_path("keys", 1), 0700);
		name = saprintf("%s/%d.pem", prepare_path("keys", 0), e->event.msg.sender);

		if (!(f = fopen(name, "w"))) {
			print_window(target, 0, "public_key_write_failed");
			xfree(name);
			return;
		}
		
		fprintf(f, "%s", e->event.msg.message);
		fclose(f);
		xfree(name);

		SIM_KC_Free(SIM_KC_Find(e->event.msg.sender));

		return;
	}

	if (config_encryption == 1) {
		char *dec;
		int len = strlen(e->event.msg.message);

		dec = xmalloc(len);
		memset(dec, 0, len);
		
		len = SIM_Message_Decrypt(e->event.msg.message, dec, len, e->event.msg.sender);

		if (len > 0) {
			strcpy(e->event.msg.message, dec);
			secure = 1;
		}

		xfree(dec);

		if (len == -1)
			gg_debug(GG_DEBUG_MISC, "// ekg: private key not found\n");
		if (len == -2)
			gg_debug(GG_DEBUG_MISC, "// ekg: rsa decryption failed\n");
		if (len == -3)
			gg_debug(GG_DEBUG_MISC, "// ekg: magic number mismatch\n");
	}
#endif
	
	cp_to_iso(e->event.msg.message);

	event_check((chat) ? EVENT_CHAT : EVENT_MSG, e->event.msg.sender, e->event.msg.message);
	
	if (e->event.msg.sender == 0) {
		if (e->event.msg.msgclass > last_sysmsg) {
			if (!hide)
				print_message(e, u, 2, 0);

			if (config_beep)
				ui_beep();
		    
			play_sound(config_sound_sysmsg_file);
			last_sysmsg = e->event.msg.msgclass;
			sysmsg_write();
		}

		return;
	};
			
	if (u)
		add_send_nick(u->display);
	else
		add_send_nick(itoa(e->event.msg.sender));

	if (!hide)
		print_message(e, u, chat, secure);

	if (config_beep && ((chat) ? config_beep_chat : config_beep_msg))
		ui_beep();

	play_sound((chat) ? config_sound_chat_file : config_sound_msg_file);

	tmp = log_escape(e->event.msg.message);
	put_log(e->event.msg.sender, "%s,%ld,%s,%s,%s,%s\n", (chat) ? "chatrecv" : "msgrecv", e->event.msg.sender, (u) ? u->display : "", log_timestamp(time(NULL)), log_timestamp(e->event.msg.time), tmp);
	xfree(tmp);

	if (away && away != 4 && config_sms_away && config_sms_app && config_sms_number) {
		char *foo, sender[100];

		sms_away_add(e->event.msg.sender);

		if (sms_away_check(e->event.msg.sender)) {
			if (u)
				snprintf(sender, sizeof(sender), "%s/%u", u->display, u->uin);
			else
				snprintf(sender, sizeof(sender), "%u", e->event.msg.sender);

			if (config_sms_max_length && strlen(e->event.msg.message) > config_sms_max_length)
				e->event.msg.message[config_sms_max_length] = 0;

			foo = format_string(format_find((chat) ? "sms_chat" : "sms_msg"), sender, e->event.msg.message);

			/* niech nie wysy³a smsów, je¶li brakuje formatów */
			if (strcmp(foo, ""))
				send_sms(config_sms_number, foo, 0);
	
			xfree(foo);
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
	const char *tmp, *target = (u) ? u->display : itoa(e->event.ack.recipient);

	if (!e->event.ack.seq)	/* ignorujemy potwierdzenia ctcp */
		return;

	msg_queue_remove(e->event.ack.seq);

	event_check((queued) ? EVENT_QUEUED : EVENT_DELIVERED, e->event.ack.recipient, NULL);

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
 * i handle_notify(). utrzymywanie tego samego kodu w dwóch miejscach
 * jest kompletnie bez sensu.
 *  
 *  - uin - numer delikwenta,
 *  - status - nowy stan,
 *  - descr - nowy opis,
 *  - n - dodatki od gg_notify.
 */
static void handle_common(uin_t uin, int status, const char *idescr, struct gg_notify_reply *n)
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
		{ GG_STATUS_NOT_AVAIL, EVENT_NOT_AVAIL, "notavail", "status_not_avail" },
		{ GG_STATUS_NOT_AVAIL_DESCR, EVENT_NOT_AVAIL, "notavail", "status_not_avail_descr" },
		{ 0, 0, NULL, NULL },
	};
	struct status_table *s;
	int prev_status, hide = 0;
	char *descr = NULL;
#ifdef WITH_PYTHON
	list_t l;
#endif

	/* je¶li ignorujemy, nie wy¶wietlaj */
	if (ignored_check(uin))
		return;
	
	/* nie pokazujemy nieznajomych */
	if (!(u = userlist_find(uin, NULL)))
		return;

#ifdef WITH_PYTHON
	for (l = modules; l; l = l->next) {
		struct module *m = l->data;
		PyObject *res;

		if (!m->handle_status)
			continue;

		res = PyObject_CallFunction(m->handle_status, "(isis)", uin, (u) ? u->display : NULL, status, idescr);

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
			char *newnick, *newdescr;

			if (PyArg_ParseTuple(res, "isis", &uin, &newnick, &status, &newdescr)) {
				descr = xstrdup(newdescr);
			} else
				PyErr_Print();
		}

		Py_XDECREF(res);
	}
#endif

	if (!descr)
		descr = xstrdup(idescr);
	
	/* zapamiêtaj adres i port */
	if (n) {
		u->port = n->remote_port;
		u->ip.s_addr = n->remote_ip;
	}

	/* je¶li status taki sam i ewentualnie opisy te same, ignoruj */
	if (!GG_S_D(status) && (u->status == status)) {
		xfree(descr);
		return;
	}
	
	if (GG_S_D(status) && (u->status == status) && u->descr && descr && !strcmp(u->descr, descr)) {
		xfree(descr);
		return;
	}

	/* usuñ poprzedni opis */
	xfree(u->descr);
	u->descr = NULL;

	/* je¶li stan z opisem, a opisu brak, wpisz pusty tekst */
	if (GG_S_D(status) && !descr)
		u->descr = xstrdup("");

	/* a je¶li jest opis, to go zapamiêtaj */
	if (descr) {
		u->descr = xstrdup(descr);
		cp_to_iso(u->descr);
	}

	/* zapamiêtaj stary stan, ustaw nowy */
	prev_status = u->status;
	u->status = status;

	/* poinformuj ui */
	ui_event("status", u->uin, u->display, status, u->descr);
	
	for (s = st; s->status; s++) {
		/* je¶li nie ten, sprawdzaj dalej */
		if (status != s->status)
			continue;

		/* je¶li nie jest opisowy i taki sam, ignoruj */
		if (!GG_S_D(s->status) && prev_status == s->status)
			break;

		/* eventy */
		if (GG_S_D(s->status) && prev_status == s->status) /* tylko zmiana opisu */
			event_check(EVENT_DESCR, uin, NULL);

	    	event_check(s->event, uin, NULL);

		/* zaloguj */
		if (config_log_status && !GG_S_D(s->status))
			put_log(uin, "status,%ld,%s,%s,%s,%s\n", uin, u->display, inet_ntoa(u->ip), log_timestamp(time(NULL)), s->log);
		if (config_log_status && GG_S_D(s->status) && u->descr)
		    	put_log(uin, "status,%ld,%s,%s,%s,%s,%s\n", uin, u->display, inet_ntoa(u->ip), log_timestamp(time(NULL)), s->log, u->descr);

		/* jak dostêpny lub zajêty, dopiszmy do taba
		 * jak niedostêpny, usuñmy */
		if (GG_S_A(s->status) && config_completion_notify) 
			add_send_nick(u->display);
		if (GG_S_B(s->status) && (config_completion_notify & 4))
			add_send_nick(u->display);
		if (GG_S_NA(s->status) && (config_completion_notify & 2))
			remove_send_nick(u->display);
		
		/* czy mamy wy¶wietlaæ na ekranie? */
		if (!config_display_notify || config_contacts == 2)
			break;
		
		if (config_display_notify == 2) {
			/* je¶li na zajêty, ignorujemy */
			if (GG_S_B(s->status) && !GG_S_NA(prev_status))
				break;

			/* je¶li na dostêpny i nie by³ niedostêpny, ignoruj */
			if (GG_S_A(s->status) && !GG_S_NA(prev_status))
				break;
		}

		/* czy ukrywaæ niedostêpnych */
		if (hide_notavail) {
			if (GG_S_NA(s->status))
				break;
			else
				hide_notavail = 0;
		}
			
		/* no dobra, poka¿ */
		if (!hide)
			print_window(u->display, 0, s->format, format_user(uin), (u->first_name) ? u->first_name : u->display, u->descr);

		/* daj znaæ d¿wiêkiem */
		if (config_beep && config_beep_notify)
			ui_beep();

		/* lub muzyczk± */
		if (config_sound_notify_file)
			play_sound(config_sound_notify_file);

		break;
	}

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
		
		handle_common(n->uin, n->status, descr, n);
	}
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

	handle_common(e->event.status.uin, e->event.status.status, e->event.status.descr, NULL);
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
		{ 0, NULL },
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
		xfree(config_server);
		config_server = NULL;
	}

	list_remove(&watches, sess, 0);
	gg_logoff(sess);
	gg_free_session(sess);
	userlist_clear_status();
	sess = NULL;
	do_reconnect();
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

	/* je¶li mieli¶my zachowany stan i/lub opis, zrób z niego u¿ytek */
	if (away || private_mode) {
		if (!config_reason || !GG_S_D(config_status)) 
			gg_change_status(sess, config_status);
		else {
			iso_to_cp(config_reason);
			gg_change_status_descr(sess, config_status, config_reason);
			cp_to_iso(config_reason);
		}
	}

	if (!msg_queue_flush())
		print("queue_flush");

	/* zapiszmy adres serwera */
	if (config_server_save) {
		struct in_addr addr;

		addr.s_addr = sess->server_addr;
		
		xfree(config_server);
		config_server = xstrdup(inet_ntoa(addr));
	}
	
	if (batch_mode && batch_line) {
 		command_exec(NULL, batch_line);
 		xfree(batch_line);
 		batch_line = NULL;
 	}

	hide_notavail = 1;

	update_status();

	last_conn_event = time(NULL);
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
		print("conn_broken");
		list_remove(&watches, sess, 0);
		gg_free_session(sess);
		sess = NULL;
		ui_event("disconnected");
		last_conn_event = time(NULL);
		do_reconnect();

		return;
	}

	for (h = handlers; h->type; h++)
		if (h->type == e->type)
			(h->handler)(e);

	gg_event_free(e);
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
	struct gg_search_request *r;
	struct gg_search *s = NULL;
	int i;

	if (gg_search_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print("search_failed", http_error_string((h) ? h->state : 0));
		list_remove(&watches, h, 0);
		gg_search_request_free((struct gg_search_request*) h->user_data);
		gg_search_free(h);
		return;
	}
	
	if (h->state != GG_STATE_DONE)
		return;

	gg_debug(GG_DEBUG_MISC, "++ gg_search()... done\n");

	if (!h || !(s = h->data) || !(s->count))
		print("search_not_found");

	for (i = 0; i < s->count; i++) {
		const char *active_format, *gender_format;
		char *name, *active, *gender;

		cp_to_iso(s->results[i].first_name);
		cp_to_iso(s->results[i].last_name);
		cp_to_iso(s->results[i].nickname);
		cp_to_iso(s->results[i].city);

		name = saprintf("%s %s", s->results[i].first_name, s->results[i].last_name);

		if (!(h->id & 1)) {
			switch (s->results[i].active) {
				case GG_STATUS_AVAIL:
					active_format = format_find("search_results_single_active");
					break;

				case GG_STATUS_BUSY:
					active_format = format_find("search_results_single_busy");
					break;
				default:
					active_format = format_find("search_results_single_inactive");
			}

			if (s->results[i].gender == GG_GENDER_FEMALE)
				gender_format = format_find("search_results_single_female");
			else if (s->results[i].gender == GG_GENDER_MALE)
				gender_format = format_find("search_results_single_male");
			else
				gender_format = format_find("search_results_single_unknown");
		} else {
			switch (s->results[i].active) {
				case GG_STATUS_AVAIL:
					active_format = format_find("search_results_multi_active");
					break;

				case GG_STATUS_BUSY:
					active_format = format_find("search_results_multi_busy");
					break;
				default:
					active_format = format_find("search_results_multi_inactive");
			}

			if (s->results[i].gender == GG_GENDER_FEMALE)
				gender_format = format_find("search_results_multi_female");
			else if (s->results[i].gender == GG_GENDER_MALE)
				gender_format = format_find("search_results_multi_male");
			else
				gender_format = format_find("search_results_multi_unknown");
		}

		active = format_string(active_format, s->results[i].nickname);
		gender = format_string(gender_format);

		print((h->id & 1) ? "search_results_multi" : "search_results_single", itoa(s->results[i].uin), (name) ? name : "", s->results[i].nickname, s->results[i].city, (s->results[i].born) ? itoa(s->results[i].born) : "-", gender, active);

		xfree(name);
		xfree(active);
		xfree(gender);
	}

	r = (void*) h->user_data;

	if (s->count > 19 && (r->start & 0x80000000L)) {
		struct gg_http *h2;
		list_t l;
		int id = 1;
		
		r->start = s->results[19].uin | 0x80000000L;
		list_remove(&watches, h, 0);
		gg_free_search(h);
		
		for (l = watches; l; l = l->next)
		{
			struct gg_http *h3 = l->data;

			if (h3->type != GG_SESSION_SEARCH)
				continue;

			if (h3->id / 2 >= id)
				id = h3->id / 2 + 1;
		}
		if (!(h2 = gg_search(r, 1)))
		{
			print("search_failed", http_error_string(0));
			gg_search_request_free(r);
			return;
		}
		h2->id = id;
		h2->user_data = (char*) r;
		list_add(&watches, h2, 0);
		return;
	}
	
	list_remove(&watches, h, 0);
	gg_search_request_free(r);
	gg_search_free(h);
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
	char *good = "", *bad = "";

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
		case GG_SESSION_CHANGE:
			good = "change";
			bad = "change_failed";
			break;
	}

	if (gg_pubdir_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print(bad, strerror(errno));
		goto fail;
	}
	
	if (h->state != GG_STATE_DONE)
		return;

	if (!(s = h->data) || !s->success) {
		print(bad, strerror(errno));
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
			print(bad);
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
			print(bad);
			goto fail;
		}

		if (s->uin == config_uin) {
			config_uin = 0;
			config_password = 0;
			config_changed = 1;
			command_exec(NULL, "disconnect");
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
 * funkcja zajmuj±ca siê zdarzeniami userlisty.
 *
 *  - h - delikwent.
 *
 * nie zwraca niczego.
 */
void handle_userlist(struct gg_http *h)
{
	char *format_ok, *format_error;
	
	if (!h)
		return;
	
	format_ok = (h->type == GG_SESSION_USERLIST_GET) ? "userlist_get_ok" : "userlist_put_ok";
	format_error = (h->type == GG_SESSION_USERLIST_GET) ? "userlist_get_error" : "userlist_put_error";

	if (h->callback(h) || h->state == GG_STATE_ERROR) {
		print(format_error, strerror(errno));
		list_remove(&watches, h, 0);
		xfree(h->user_data);
		return;
	}
	
	if (h->state != GG_STATE_DONE)
		return;

	print((h->data) ? format_ok : format_error);
		
	if (h->type == GG_SESSION_USERLIST_GET && h->data) {
		cp_to_iso(h->data);
		userlist_set(h->data, (h->user_data) ? 1 : 0);
		userlist_send();
		config_changed = 1;
	}

	list_remove(&watches, h, 0);
	h->destroy(h);
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
	print("conn_broken");
	ui_event("disconnected");

	gg_logoff(sess);	/* a zobacz.. mo¿e siê uda ;> */
	list_remove(&watches, sess, 0);
	userlist_clear_status();
	gg_free_session(sess);
	sess = NULL;	
	update_status();
	last_conn_event = time(NULL);
	do_reconnect();
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
static void remove_transfer(struct gg_dcc *d)
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

	event_check(EVENT_DCC, d->peer_uin, NULL);
	
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
			
			t->type = GG_SESSION_DCC_GET;
			t->filename = xstrdup(d->file_info.filename);

			for (p = d->file_info.filename; *p; p++)
				if (*p < 32 || *p == '\\' || *p == '/')
					*p = '_';

			print("dcc_get_offer", format_user(t->uin), t->filename, itoa(d->file_info.size), itoa(t->id));

			break;
			
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

			print((t->dcc->type == GG_SESSION_DCC_SEND) ? "dcc_done_send" : "dcc_done_get", format_user(t->uin), t->filename);
			
			remove_transfer(d);
			list_remove(&watches, d, 0);
			gg_free_dcc(d);

			break;
			
		case GG_EVENT_DCC_ERROR:
		{
			struct in_addr addr;
			char *tmp;
		
			addr.s_addr = d->remote_addr;

			if (d->peer_uin)
				tmp = saprintf("%s (%s:%d)", xstrdup(format_user(d->peer_uin)), inet_ntoa(addr), d->remote_port);
			else 
				tmp = saprintf("%s:%d", inet_ntoa(addr), d->remote_port);
			
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
 * brak.
 */
void handle_voice()
{
#ifdef HAVE_VOIP
	list_t l;
	struct gg_dcc *d = NULL;
	char buf[GG_DCC_VOICE_FRAME_LENGTH];
	
	for (l = transfers; l; l = l->next) {
		struct transfer *t = l->data;

		if (t->type == GG_SESSION_DCC_VOICE && t->dcc && (t->dcc->state == GG_STATE_READING_VOICE_HEADER || t->dcc->state == GG_STATE_READING_VOICE_SIZE || t->dcc->state == GG_STATE_READING_VOICE_DATA)) {
			d = t->dcc;
			break;
		}
	}

	/* póki nie mamy po³±czenia, i tak czytamy z /dev/dsp */

	if (!d) {
		voice_record(buf, GG_DCC_VOICE_FRAME_LENGTH, 1);	/* XXX b³êdy */
		return;
	} else {
		voice_record(buf, GG_DCC_VOICE_FRAME_LENGTH, 0);	/* XXX b³êdy */
		gg_dcc_voice_send(d, buf, GG_DCC_VOICE_FRAME_LENGTH);	/* XXX b³êdy */
	}
#endif /* HAVE_VOIP */
}
