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
struct gg_session *sess;

int config_dcc;
int config_auto_away;
int config_auto_save;
time_t last_save;
int config_log;
char *config_log_path;
int config_log_ignored;
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

int in_readline;
int away;
int in_autoexec;
int reconnect_timer;
time_t last_action;
int last_sysmsg;
int no_prompt;
int private_mode;
int connecting;

int use_proxy;
int proxy_port;
char *proxy_host;
char *reg_password;
char *query_nick;
uin_t query_uin;
int sock;
int length;
struct sockaddr_un addr;

int config_read(char *filename);
int config_write(char *filename);

void my_puts(char *format, ...);
char *my_readline();
int read_sysmsg(char *filename);
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

char *base64_encode(char *buf);
char *base64_decode(char *buf);

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
