/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "libgadu.h"
#include "dynstuff.h"
#include "ioctl_daemon.h"

enum { EVENT_MSG = 1, EVENT_CHAT = 2, EVENT_AVAIL = 4, EVENT_NOT_AVAIL = 8, EVENT_AWAY = 16, EVENT_DCC = 32 };

struct process {
	int pid;
	char *name;
};

struct alias {
	char *alias;
	char *cmd;
};

struct transfer {
	uin_t uin;
	char *filename;
	struct gg_dcc *dcc;
	int type;
	int id;
};

struct event {
        uin_t uin;
        int flags;
        char *action;
};

struct list *children;
struct list *aliases;
struct list *watches;
struct list *transfers;
struct list *events;
int in_readline, away, in_autoexec, auto_reconnect, reconnect_timer;
int use_dcc;
time_t last_action;
struct gg_session *sess;
int auto_away;
int log;
char *log_path;
int log_ignored;
int display_color;
int enable_beep, enable_beep_msg, enable_beep_chat, enable_beep_notify, display_debug;
int config_uin;
int last_sysmsg;
char *config_password;
char *config_user;
int sms_away;
char *sms_number;
char *sms_send_app;
int sms_max_length;
int no_prompt;
int config_changed;
int display_ack;
int completion_notify;
char *bold_font;
char *default_theme;
int private_mode;
int connecting;
char *sound_msg_file;
char *sound_chat_file;
char *sound_sysmsg_file;
char *sound_app;
int display_notify;
int default_status;
int use_proxy;
int proxy_port;
char *proxy_host;
char *reg_password;
char *query_nick;
uin_t query_uin;
int sock;
int length;
struct sockaddr_un addr;

void my_puts(char *format, ...);
char *my_readline();
int read_config(char *filename);
int read_sysmsg(char *filename);
int write_config(char *filename);
int write_sysmsg(char *filename);
void cp_to_iso(unsigned char *buf);
void iso_to_cp(unsigned char *buf);
void unidle();
char *timestamp(char *format);
char *prepare_path(char *filename);
void parse_autoexec(char *filename);
void send_userlist();
void do_reconnect();
void put_log(uin_t uin, char *format, ...);
char *full_timestamp();
int send_sms(char *recipient, char *message, int show_result);
char *read_file(FILE *f);
int add_process(int pid, char *name);
int del_process(int pid);
int on_off(char *value);
int add_alias(char *string, int quiet);
int del_alias(char *name);
char *is_alias(char *foo);
int play_sound(char *sound_path);
char *encode_base64(char *buf);
char *decode_base64(char *buf);
void reset_prompt();
void changed_debug(char *var);
void changed_dcc(char *var);
void changed_theme(char *var);
void prepare_connect();
int transfer_id();
int add_event(int flags, uin_t uin, char *action);
int del_event(int flags, uin_t uin);
int get_flags(char *events);
char *format_events(int flags);
int check_event(int event, uin_t uin);
int run_event(char *action);
int send_event(char *seq, int act);
int correct_event(char *action);
int events_parse_seq(char *seq, struct action_data *data);
int init_socket();
char *get_token(char **ptr, char sep);
char *strdup_null(char *ptr);

#endif
