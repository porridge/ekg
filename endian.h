/* $Id$ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>
 *                     Robert J. Wozny <speedy@atman.pl>
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

#ifndef __ENDIAN_H
#define __ENDIAN_H

#include "config.h"

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321

#ifdef WORDS_BIGENDIAN 
  #define __BYTE_ORDER __BIG_ENDIAN
#else
  #define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#endif
