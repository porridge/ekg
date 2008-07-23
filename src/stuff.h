/* $Id$ */

/*
 *  (C) Copyright 2001-2005 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@go2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
 *                          Adam Wysocki <gophi@ekg.chmurka.net>
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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "dynstuff.h"
#include "libgadu.h"
#include "ioctld.h"

#define DEBUG_MAX_LINES	50	/* ile linii z debug zrzucaæ do pliku */

#define TOGGLE_BIT(x) (1 << (x - 1))

#define SPYING_RESPONSE_TIMEOUT 15

enum event_t {
	EVENT_MSG = TOGGLE_BIT(1),
	EVENT_CHAT = TOGGLE_BIT(2),
	EVENT_CONFERENCE = TOGGLE_BIT(3),
	EVENT_QUERY = TOGGLE_BIT(4),
	EVENT_AVAIL = TOGGLE_BIT(5),
	EVENT_ONLINE = TOGGLE_BIT(6),
	EVENT_NOT_AVAIL = TOGGLE_BIT(7),
	EVENT_AWAY = TOGGLE_BIT(8),
	EVENT_INVISIBLE = TOGGLE_BIT(9),
	EVENT_DESCR = TOGGLE_BIT(10),
	EVENT_DCC = TOGGLE_BIT(11),
	EVENT_SIGUSR1 = TOGGLE_BIT(12),
	EVENT_SIGUSR2 = TOGGLE_BIT(13),
	EVENT_DELIVERED = TOGGLE_BIT(14),
	EVENT_QUEUED = TOGGLE_BIT(15),
	EVENT_FILTERED = TOGGLE_BIT(16),
	EVENT_MBOXFULL = TOGGLE_BIT(17),
	EVENT_NOT_DELIVERED = TOGGLE_BIT(18),
	EVENT_NEWMAIL = TOGGLE_BIT(19),
	EVENT_BLOCKED = TOGGLE_BIT(20),
	EVENT_DCCFINISH = TOGGLE_BIT(21),
	EVENT_CONNECTED = TOGGLE_BIT(22),
	EVENT_DISCONNECTED = TOGGLE_BIT(23),
	EVENT_CONNECTIONLOST = TOGGLE_BIT(24),
	EVENT_IMAGE = TOGGLE_BIT(25),

	EVENT_ALL = TOGGLE_BIT(26) - 1,
	INACTIVE_EVENT = TOGGLE_BIT(26)
};

struct event_label {
	int event;
	char *name;
};

#define EVENT_LABELS_COUNT 25	/* uaktualniæ ! */
struct event_label event_labels[EVENT_LABELS_COUNT + 2];

struct process {
	int pid;		/* id procesu */
	int died;		/* proces umar³, ale najpierw pobieramy do koñca z bufora dane */
	char *name;		/* nazwa. je¶li poprzedzona \2 to nie obchodzi */
				/* nas w jaki sposób siê zakoñczy³o, \3 to samo, */
				/* ale nie wy¶wietla na li¶cie procesów */
};

struct alias {
	char *name;		/* nazwa aliasu */
	list_t commands;	/* commands->data to (char*) */
};

struct transfer {
	uin_t uin;
	char *filename;
	struct gg_dcc *dcc;
	struct gg_dcc7 *dcc7;
	time_t start;
	int type;
	int id;
	int protocol;
};

struct event {
	char *name;	/* identyfikator */
        char *target;	/* uin, alias lub grupa */
        int flags;	/* flagi zdarzenia */
        char *action;	/* akcja! */
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

enum mesg_t {
	MESG_CHECK = -1,
	MESG_OFF,
	MESG_ON,
	MESG_DEFAULT
};

enum timer_t {
	TIMER_SCRIPT,
	TIMER_UI,
	TIMER_COMMAND
};

struct timer {
	struct timeval ends;	/* kiedy siê koñczy? */
	time_t period;		/* ile sekund ma trwaæ czekanie */
	int persistent;		/* czy ma byæ na zawsze? */
	int type;		/* rodzaj timera */
	int at;			/* at czy zwyk³y timer? */
	char *name;		/* nazwa timera */
	char *command;		/* komenda do wywo³ania */
	char *id;
};

struct spied {
	uin_t uin;
	int timeout;
	struct timeval request_sent;
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

enum buffer_t {
	BUFFER_DEBUG,	/* na zapisanie n ostatnich linii debug */
	BUFFER_EXEC,	/* na buforowanie tego, co wypluwa exec */
	BUFFER_SPEECH	/* na wymawiany tekst */
};

struct buffer {
	int type;
	char *target;
	char *line;
};

struct color_map {
	int color;
	unsigned char r, g, b;
};

list_t autofinds;
list_t children;
list_t aliases;
list_t watches;
list_t transfers;
list_t events;
list_t bindings;
list_t timers;
list_t conferences;
list_t sms_away;
list_t buffers;
list_t searches;
list_t spiedlist;

struct gg_session *sess;

time_t last_save;
char *config_profile;
int config_changed;

pid_t speech_pid;

int old_stderr;
int mesg_startup;

char *config_audio_device;
char *config_away_reason;
int config_auto_away;
int config_auto_away_keep_descr;
int config_auto_back;
int config_auto_find;
int config_auto_conference;
int config_auto_reconnect;
int config_auto_save;
#ifdef WITH_ASPELL
int config_aspell;
char *config_aspell_lang;
char *config_aspell_encoding;
#endif
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
char *config_contacts_groups;
char *config_contacts_options;
int config_contacts_size;
int config_ctrld_quits;
int config_dcc;
int config_dcc_backups;
char *config_dcc_ip;
char *config_dcc_dir;
int config_dcc_filter;
char *config_dcc_limit;
int config_dcc_port;
int config_display_ack;
int config_display_color;
char *config_display_color_map;
int config_display_crap;
int config_display_daychanges;
int config_display_notify;
int config_display_pl_chars;
int config_display_sent;
#if defined HAVE_LIBJPEG || defined HAVE_LIBUNGIF
int config_display_token;
#endif
int config_display_welcome;
int config_display_transparent;
char *config_email;
int config_emoticons;
int config_encryption;
int config_enter_scrolls;
int config_events_delay;
int config_files_mode_config;
int config_files_mode_received;
int config_files_mode_config_int;
int config_files_mode_received_int;
char *config_interface;
int config_header_size;
int config_ignore_unknown_sender;
int config_ignore_empty_msg;
int config_irssi_set_mode;
int config_keep_reason;
int config_last;
int config_last_size;
int config_last_sysmsg;
int config_last_sysmsg_changed;
char *config_local_ip;
int config_log;
int config_log_ignored;
char *config_log_path;
int config_log_status;
char *config_log_timestamp;
int config_make_window;
int config_msg_as_chat;
int config_mesg;
#ifdef WITH_UI_NCURSES
int config_mouse;
#endif
char *config_nick;
char *config_password;
int config_password_cp1250;
int config_protocol;
char *config_proxy;
char *config_proxy_forwarding;
int config_query_commands;
char *config_quit_reason;
int config_random_reason;
#ifdef HAVE_REGEX_H
int config_regex_flags;
#endif
char *config_reason;
int config_reason_limit;
int config_receive_images;
int config_image_size;
int config_save_question;
int config_save_password;
char *config_server;
int config_server_save;
int config_slash_messages;
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
int config_status_window;
int config_statusbar_size;
int config_statusbar_fgcolor;
int config_statusbar_bgcolor;
char *config_tab_command;
char *config_theme;
int config_time_deviation;
char *config_datestamp;
char *config_timestamp;
int config_uin;
int config_userlist_backup;
char *config_windows_layout;
int config_windows_save;
#ifdef WITH_WAP
int config_wap_enabled;
#endif
#ifdef WITH_IOCTLD
int config_ioctld_enable;
int config_ioctld_net_port;
#endif

char *home_dir;
char *config_dir;
int command_processing;
int in_autoexec;
int reconnect_timer;
time_t last_action;
int connecting;
time_t last_conn_event;
time_t ekg_started;
int server_index;
int in_auto_away;
int quit_command;

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
int ioctld_sock;
struct color_map default_color_map[16+10];

char *last_search_first_name;
char *last_search_last_name;
char *last_search_nickname;
uin_t last_search_uin;

char *last_tokenid;

int strcoll_usable;

int alias_add(const char *string, int quiet, int append);
int alias_remove(const char *name, int quiet);
void alias_free(void);

char *base64_encode(const char *buf);
char *base64_decode(const char *buf);

void binding_list(int quiet, const char *name, int all);
void binding_free(void);

int buffer_add(int type, const char *target, const char *line, int max_lines);
int buffer_count(int type);
char *buffer_flush(int type, const char *target);
char *buffer_tail(int type);
void buffer_free(void);

void changed_auto_save(const char *var);
#ifdef WITH_ASPELL
void changed_aspell(const char *var);
#endif
void changed_backlog_size(const char *var);
void changed_dcc(const char *var);
void changed_local_ip(const char *var);
void changed_mesg(const char *var);
void changed_proxy(const char *var);
void changed_theme(const char *var);
void changed_uin(const char *var);
void changed_xxx_reason(const char *var);
void changed_files_mode(const char *var);

struct conference *conference_add(const char *string, const char *nicklist, int quiet);
int conference_remove(const char *name, int quiet);
struct conference *conference_create(const char *nicks);
struct conference *conference_find(const char *name);
struct conference *conference_find_by_uins(uin_t from, uin_t *recipients, int count, int quiet);
int conference_set_ignore(const char *name, int flag, int quiet);
int conference_rename(const char *oldname, const char *newname, int quiet);
int conference_participant(struct conference *c, uin_t uin);
void conference_free(void);

void ekg_connect(void);
void ekg_reconnect(void);
void ekg_logoff(struct gg_session *sess, const char *reason);

int ekg_hash(const char *name);

int event_add(int flags, const char *target, const char *action, int quiet);
int event_remove(const char *name, int quiet);
const char *event_format(int flags);
const char *event_format_target(const char *target);
int event_flags(const char *events);
int event_check(int event, uin_t uin, const char *data);
void event_free(void);

int mesg_set(int what);
int msg_encrypt(uin_t uin, unsigned char **msg);
void cp_to_iso(unsigned char *buf);
char *utf8_to_iso(char *buf);
void iso_to_cp(unsigned char *buf);
void iso_to_ascii(unsigned char *buf);
char *iso_to_utf8(char *buf);
char *strip_chars(char *line, unsigned char what); 
char *strip_spaces(char *line);

int play_sound(const char *sound_path);

int process_add(int pid, const char *name);
int process_remove(int pid);

const char *prepare_path(const char *filename, int do_mkdir);
char *random_line(const char *path);
char *read_file(FILE *f);

int send_sms(const char *recipient, const char *message, int quiet);
void sms_away_add(uin_t uin);
int sms_away_check(uin_t uin);
void sms_away_free(void);

int ioctld_socket(const char *path);
int ioctld_send(const char *seq, int act, int quiet);
int init_control_pipe(const char *path);

const char *timestamp(const char *format);
const char *timestamp_time(const char *format, time_t t);
void unidle(void);
int on_off(const char *value);
int transfer_id(void);
char *xstrmid(const char *str, int start, int length);
const char *http_error_string(int h);
char color_map(unsigned char r, unsigned char g, unsigned char b);
char *strcasestr(const char *haystack, const char *needle);
int say_it(const char *str);
time_t parsetimestr(const char *p);

/* makra, dziêki którym pozbywamy siê warning'ów */
#define xisxdigit(c) isxdigit((int) (unsigned char) c)
#define xisdigit(c) isdigit((int) (unsigned char) c)
#define xisalpha(c) isalpha((int) (unsigned char) c)
#define xisalnum(c) isalnum((int) (unsigned char) c)
#define xisspace(c) isspace((int) (unsigned char) c)
#define xtolower(c) tolower((int) (unsigned char) c)
#define xtoupper(c) toupper((int) (unsigned char) c)

struct timer *timer_add(time_t period, int persistent, int type, int at, const char *name, const char *command);
int timer_remove(const char *name, int at, const char *command);
int timer_remove_user(int at);
void timer_free(void);

void update_status(void);
void update_status_myip(void);
void change_status(int status, const char *arg, int autom);
const char *ekg_status_label(int status, const char *prefix);
int ekg_hide_descr_status(int status);
unsigned char *unique_name (unsigned char *path);

/* funkcje poza stuff.c */
void ekg_wait_for_key(void);
void ekg_exit(void);
int check_conn(uin_t uin);
void save_windows(void);

#ifdef WITH_ASPELL
void spellcheck_init(void);
void spellcheck_deinit(void);
#endif

#endif /* __STUFF_H */
