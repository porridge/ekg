/* $Id$ */

/*
 *  (C) Copyright 2002 Wojtek Kaniewski <wojtekka@irc.pl>
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

#ifndef __UI_H
#define __UI_H

#define ui_init ui_readline_init
#define ui_loop ui_readline_loop
#define ui_print ui_readline_print
#define ui_beep ui_readline_beep
#define ui_new_target ui_readline_new_target
#define ui_query ui_readline_query
#define ui_deinit ui_readline_deinit

#include "ui-readline.h"

#endif /* __UI_H */

