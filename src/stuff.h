/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@go2.pl>
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

#ifndef __STUFF_H
#define __STUFF_H

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"
#include "libgadu.h"
#include "dynstuff.h"
#include "ioctld.h"

#define DEBUG_MAX_LINES	50	/* ile linii z debug zrzucaæ do pliku */

enum event_t {
	EVENT_MSG = 1,
	EVENT_CHAT = 2,
	EVENT_AVAIL = 4,
	EVENT_NOT_AVAIL = 8,
	EVENT_AWAY = 16,
	EVENT_DCC = 32,
	EVENT_INVISIBLE = 64,
	EVENT_DESCR = 128,
	EVENT_EXEC = 256,
	EVENT_SIGUSR1 = 512,
	EVENT_SIGUSR2 = 1024,
	EVENT_DELIVERED = 2048,
	EVENT_QUEUED = 4096,
	EVENT_NEW_MAIL = 8192,
	EVENT_QUERY = 16384,

	EVENT_ALL = 32767,	/* uaktualniaæ za ka¿d± zmian± */

	INACTIVE_EVENT = 32768	/* nieaktywne zdarzenie */
};

#define EVENT_LABELS_MAX 17	/* uaktualniæ ! */
struct event_label event_labels[EVENT_LABELS_MAX];

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
	char *name;	/* identyfikator */
        uin_t uin;	/* numerek dla którego zdarzenie zachodzi */
        int flags;	/* flagi zdarzenia */
        char *action;	/* akcja! */
};

struct event_label {
	int event;
	char *name;
};

struct emoticon {
	char *name;	/* nazwa emoticona typu "<cmok>" */
	char *value;	/* tre¶æ emoticona typu ":-*" */
};

struct binding {
	char *key;

	char *action;			/* akcja */
	int internal;			/* czy domy¶lna kombinacja? */
	void (*function)(const char *arg);	/* funkcja obs³uguj±ca */
	char *arg;			/* argument funkcji */

	char *default_action;		/* domy¶lna akcja */
	void (*default_function)(const char *arg);	/* domy¶lna funkcja */
	char *default_arg;		/* domy¶lny argument */
};

enum timer_type {
	TIMER_SCRIPT,
	TIMER_UI,
	TIMER_COMMAND
};

struct timer {
	struct timeval ends;	/* kiedy siê koñczy? */
	int period;		/* ile sekund ma trwaæ czekanie */
	int persistent;		/* czy ma byæ na zawsze? */
	int type;		/* rodzaj timera */
	int at;			/* at czy zwyk³y timer? */
	char *name;		/* nazwa timera */
	char *command;		/* komenda do wywo³ania */
	char *id;		/* identyfikator timera */
};

struct last {
	int type;		/* 0 - przychodz±ca, 1 - wychodz±ca */
	uin_t uin;		/* od kogo, lub do kogo przy wysy³anych */
	time_t time;		/* czas */
	time_t sent_time;	/* czas wys³ania wiadomo¶ci przychodz±cej */
	unsigned char *message;	/* wiadomo¶æ */
};

struct sms_away {
	uin_t uin;
	int count;
};

struct conference {
	char *name;
	int ignore;
	list_t recipients;
};

struct gg_exec {
	gg_common_head(struct gg_exec)
	
	int msg;	/* czy wysy³amy stdout komu¶? 1 - tak, 2 - tak, buforujemy */
	string_t buf;	/* bufor na stdout procesu */
	char *target;	/* okno, do którego ma lecieæ wynik */
	int quiet;	/* czy byæ cicho ? */
};

enum buffer_type {
	BUFFER_DEBUG,	/* na zapisanie n ostatnich linii debug */
	BUFFER_EXEC	/* na buforowanie tego, co wypluwa exec */
};

struct buffer {
	int type;
	char *target;
	char *line;
};

list_t children;
list_t aliases;
list_t watches;
list_t transfers;
list_t events;
list_t emoticons;
list_t bindings;
list_t timers;
list_t lasts;
list_t conferences;
list_t sms_away;
list_t buffers;
struct gg_session *sess;

time_t last_save;
char *config_profile;
int config_changed;

int old_stderr;

char *config_audio_device;
char *config_away_reason;
int config_auto_away;
int config_auto_back;
int config_auto_reconnect;
int config_auto_save;
char *config_back_reason;
int config_beep;
int config_beep_msg;
int config_beep_chat;
int config_beep_notify;
int config_beep_mail;
int config_check_mail;
int config_check_mail_frequency;
char *config_check_mail_folders;
int config_completion_notify;
int config_contacts;
int config_contacts_descr;
int config_contacts_size;
int config_ctrld_quits;
int config_dcc;
char *config_dcc_ip;
char *config_dcc_dir;
int config_debug;
int config_display_ack;
int config_display_color;
char *config_display_color_map;
int config_display_crap;
int config_display_notify;
int config_display_pl_chars;
int config_display_sent;
int config_display_welcome;
int config_display_transparent;
char *config_email;
int config_emoticons;
int config_encryption;
int config_enter_scrolls;
char *config_interface;
int config_header_size;
int config_keep_reason;
int config_last;
int config_last_size;
int config_log;
int config_log_ignored;
char *config_log_path;
int config_log_status;
char *config_log_timestamp;
int config_make_window;
int config_mesg_allow;
char *config_password;
int config_protocol;
char *config_proxy;
char *config_proxy_forwarding;
int config_query_commands;
char *config_quit_reason;
int config_random_reason;
char *config_reason;
int config_save_password;
char *config_server;
int config_server_save;
char *config_sms_app;
int config_sms_away;
int config_sms_away_limit;
int config_sms_max_length;
char *config_sms_number;
int config_sort_windows;
char *config_sound_app;
char *config_sound_chat_file;
char *config_sound_msg_file;
char *config_sound_sysmsg_file;
char *config_sound_notify_file;
char *config_sound_mail_file;
char *config_speech_app;
int config_status;
int config_statusbar_size;
char *config_tab_command;
char *config_theme;
int config_time_deviation;
char *config_timestamp;
int config_uin;
char *config_windows_layout;
int config_windows_save;

char *home_dir;
char *config_dir;
int in_autoexec;
int reconnect_timer;
time_t last_action;
int last_sysmsg;
int connecting;
time_t last_conn_event;
int server_index;
int in_auto_away;

int use_proxy;
int proxy_port;
char *proxy_host;
char *reg_password;
char *reg_email;
int quit_message_send;
int registered_today;
int pipe_fd;
int batch_mode;
char *batch_line;
int immediately_quit;
int ekg_segv_handler;
int ioctld_sock;


int alias_add(const char *string, int quiet, int append);
int alias_remove(const char *name, int quiet);
void alias_free();

char *base64_encode(const char *buf);
char *base64_decode(const char *buf);

void binding_list(int quiet, int all);
void binding_free();

int buffer_add(int type, const char *target, const char *line, int max_lines);
int buffer_count(int type);
char *buffer_flush(int type, const char *target);
void buffer_free();

void changed_backlog_size(const char *var);
void changed_dcc(const char *var);
void changed_proxy(const char *var);
void changed_theme(const char *var);
void changed_uin(const char *var);
void changed_xxx_reason(const char *var);

const char *compile_time();

struct conference *conference_add(const char *string, const char *nicklist, int quiet);
int conference_remove(const char *name, int quiet);
struct conference *conference_create(const char *nicks);
struct conference *conference_find(const char *name);
struct conference *conference_find_by_uins(uin_t from, uin_t *recipients, int count);
int conference_set_ignore(const char *name, int flag, int quiet);
int conference_rename(const char *oldname, const char *newname, int quiet);
void conference_free();

int config_read();
int config_write();
int config_write_partly(char **vars);
void config_write_crash();
void debug_write_crash();
int sysmsg_read();
int sysmsg_write();

void do_connect();
void do_reconnect();
void ekg_logoff(struct gg_session *sess, const char *reason);

int ekg_hash(const char *name);

int emoticon_read();
char *emoticon_expand(const char *s);
void emoticon_free();

int event_add(int flags, uin_t uin, const char *action, int quiet);
int event_remove(const char *name, int quiet);
const char *event_format(int flags);
int event_flags(const char *events);
int event_check(int event, uin_t uin, const char *data);
void event_free();

void last_add(int type, uin_t uin, time_t t, time_t st, const char *msg);
void last_del(uin_t uin);
int last_count(uin_t uin);
void last_free();

void put_log(uin_t uin, const char *format, ...);
char *log_escape(const char *str);
const char *log_timestamp(time_t t);

int mesg_set(int what);
void mesg_changed();

int msg_encrypt(uin_t uin, unsigned char **msg);

void cp_to_iso(unsigned char *buf);
void iso_to_cp(unsigned char *buf);
unsigned char hide_pl(const unsigned char *c);
char *strip_spaces(char *line);

int find_in_uins(int uin_count, uin_t *uins, uin_t uin);
uin_t str_to_uin(const char *text);
int valid_nick(const char *nick);

int play_sound(const char *sound_path);

int process_add(int pid, const char *name);
int process_remove(int pid);

const char *prepare_path(const char *filename, int do_mkdir);
char *random_line(const char *path);
char *read_file(FILE *f);

int send_sms(const char *recipient, const char *message, int show_result);
void sms_away_add(uin_t uin);
int sms_away_check(uin_t uin);
void sms_away_free();

int ioctld_socket();
int init_control_pipe(const char *path);

const char *timestamp(const char *format);
void unidle();
int on_off(const char *value);
int transfer_id();

struct timer *timer_add(time_t period, int persistent, int type, int at, const char *name, const char *command);
int timer_remove(const char *name, int at, const char *command);
void timer_free();

char *xstrmid(const char *str, int start, int length);
const char *http_error_string(int h);

void update_status();
void update_status_myip();
void change_status(int status, const char *arg, int autom);
const char *ekg_status_label(int status, const char *prefix);

/* funkcje poza stuff.c */
void contacts_rebuild();
void send_userlist();
void ekg_wait_for_key();
void ekg_exit();

#endif /* __STUFF_H */
