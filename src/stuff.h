/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@go2.pl>
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

#ifndef __STUFF_H
#define __STUFF_H

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"
#include "libgadu.h"
#include "dynstuff.h"
#include "ioctld.h"

/* malutki aliasik, ¿eby nie rzucaæ d³ugimi nazwami wszêdzie */
#define saprintf gg_saprintf

enum event_t {
	EVENT_MSG = 1,
	EVENT_CHAT = 2,
	EVENT_AVAIL = 4,
	EVENT_NOT_AVAIL = 8,
	EVENT_AWAY = 16,
	EVENT_DCC = 32,
	EVENT_INVISIBLE = 64,
	EVENT_EXEC = 128,
	EVENT_SIGUSR1 = 256,
	EVENT_SIGUSR2 = 512,

	EVENT_ALL = 1023,	/* uaktualniaæ za ka¿d± zmian± */
};

struct process {
	int pid;		/* id procesu */
	char *name;		/* nazwa. je¶li poprzedzona \2 to nie obchodzi nas w jaki sposób siê zakoñczy³o */
};

struct alias {
	char *name;		/* nazwa aliasu */
	list_t commands;	/* commands->data to (char*) */
};

struct transfer {
	uin_t uin;
	char *filename;
	struct gg_dcc *dcc;
	int type;
	int id;
};

struct event {
        uin_t uin;	/* numerek dla którego zdarzenie zachodzi */
        int flags;	/* flagi zdarzenia */
        char *action;	/* akcja! */
};

struct emoticon {
	char *name;	/* nazwa emoticona typu "<cmok>" */
	char *value;	/* tre¶æ emoticona typu ":-*" */
};

struct sequence {
	char *seq;
	char *command;
};

struct timer {
	int started;	/* kiedy ustawiono timer */
	int period;	/* ile sekund ma trwaæ czekanie */
	int script;	/* czy obs³ugiwany przez skrypt */
	int ui;		/* czy obs³ugiwany przez ui */
	char *name;	/* nazwa timera */
	char *command;	/* komenda do wywo³ania */
};

struct last {
	unsigned int type;	/* 0 - przychodz±ca, 1 - wychodz±ca */
	uin_t uin;		/* od kogo, lub do kogo przy wysy³anych */
	time_t time;		/* czas */
	unsigned char *message;	/* wiadomo¶æ */
};

struct last_count {
	uin_t uin;
	int count;
};

list_t children;
list_t aliases;
list_t watches;
list_t transfers;
list_t events;
list_t emoticons;
list_t sequences;
list_t timers;
list_t lasts;
list_t lasts_count;
struct gg_session *sess;

int config_dcc;
char *config_dcc_ip;
char *config_dcc_dir;
int config_auto_away;
int config_auto_save;
time_t last_save;
int config_log;
char *config_log_path;
int config_log_ignored;
int config_log_status;
int config_display_color;
int config_beep;
int config_beep_msg;
int config_beep_chat;
int config_beep_notify;
int config_debug;
int config_uin;
char *config_password;
char *config_user;
int config_sms_away;
char *config_sms_number;
char *config_sms_app;
int config_sms_max_length;
int config_changed;
int config_display_ack;
int config_completion_notify;
char *config_theme;
char *config_sound_msg_file;
char *config_sound_chat_file;
char *config_sound_sysmsg_file;
char *config_sound_app;
int config_display_notify;
int config_status;
int config_auto_reconnect;
char *config_quit_reason;
char *config_away_reason;
char *config_back_reason;
int config_random_reason;
int config_query_commands;
char *config_proxy;
char *config_server;
int config_protocol;
int config_emoticons;
int config_make_window;
char *config_reason;
char *config_tab_command;
int config_ctrld_quits;
int config_save_password;
char *config_timestamp;
int config_display_sent;
int config_sort_windows;
int config_last_size;
int config_last;

char *home_dir;
char *config_dir;
int away;
int in_autoexec;
int reconnect_timer;
time_t last_action;
int last_sysmsg;
int private_mode;
int connecting;

int use_proxy;
int proxy_port;
char *proxy_host;
char *reg_password;
int quit_message_send;
int registered_today;
int pipe_fd;
int batch_mode;
char *batch_line;
int immediately_quit;
int ekg_segv_handler;
int ioctld_sock;

void unidle();
const char *timestamp(const char *format);
const char *prepare_path(const char *filename, int do_mkdir);
void send_userlist();
void do_reconnect();
void put_log(uin_t uin, const char *format, ...);
int send_sms(const char *recipient, const char *message, int show_result);
char *read_file(FILE *f);
int init_control_pipe(const char *path);
char *random_line(const char *path);
int print_history(uin_t uin, int no);
void do_connect();
int transfer_id();
void ekg_logoff(struct gg_session *sess, const char *reason);
void ekg_wait_for_key();
int ekg_hash(const char *name);
void ekg_exit();
char *log_escape(const char *str);

int process_add(int pid, const char *name);
int process_remove(int pid);

int on_off(const char *value);
int play_sound(const char *sound_path);

int config_read();
int config_write();
void config_write_crash();

int sysmsg_read();
int sysmsg_write();

void cp_to_iso(unsigned char *buf);
void iso_to_cp(unsigned char *buf);

int alias_add(const char *string, int quiet, int append);
int alias_remove(const char *name);
list_t alias_check(const char *foo);
void alias_free();

char *base64_encode(const char *buf);
char *base64_decode(const char *buf);

void changed_debug(const char *var);
void changed_dcc(const char *var);
void changed_theme(const char *var);
void changed_proxy(const char *var);

int event_add(int flags, uin_t uin, const char *action, int quiet);
int event_remove(int flags, uin_t uin);
int event_flags(const char *events);
const char *event_format(int flags);
int event_check(int event, uin_t uin, const char *data);
int event_correct(const char *action);
void event_free();
int ioctld_socket();

int emoticon_read();
char *emoticon_expand(const char *s);
void emoticon_free();

struct timer *timer_add(int period, const char *name, const char *command);
int timer_remove(const char *name, const char *command);
void timer_free();
int last_add(unsigned int type, uin_t uin, time_t t, const char *msg);
int get_last_count(uin_t uin);
int last_count_del(uin_t uin);
int last_count_add(uin_t uin);

#endif /* __STUFF_H */
