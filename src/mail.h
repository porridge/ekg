/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Piotr Domagalski <szalik@szalik.net>
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

#ifndef __MAIL_H
#define __MAIL_H

#include <time.h>
#include <sys/types.h>
#include "config.h"
#include "libgadu.h"
#include "dynstuff.h"

struct mail_folder {
	int fhash;
	char *fname;
	time_t mtime;
	off_t size;
	int count;
	int check;
};

list_t mail_folders;

int mail_count;
int last_mail_count;

int check_mail();
int check_mail_mbox();
int check_mail_maildir();
int check_mail_update(const char *s, int more);
void check_mail_free();

void changed_check_mail(const char *var);
void changed_check_mail_folders(const char *var);

#endif	/* __MAIL_H */
