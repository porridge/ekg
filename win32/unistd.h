/* 
 * <unistd.h>
 *
 * na win32 widocznie nie ma strncasecmp().
 *
 * $Id$
 */

#ifndef COMPAT_UNISTD_H
#define COMPAT_UNISTD_H

#define strncasecmp strnicmp

#endif /* COMPAT_UNISTD_H */
