/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "commands.h"
#include "dynstuff.h"
#include "stuff.h"
#include "ui.h"
#include "vars.h"
#include "xmalloc.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

/*
 * config_read()
 *
 * czyta z pliku ~/.gg/config lub podanego konfiguracjê.
 *
 *  - filename,
 *
 * 0/-1
 */
int config_read(const char *filename)
{
	char *buf, *foo;
	FILE *f;
	int i = 0, good_file = 0, ret = 1, home = ((filename) ? 0 : 1);
	struct stat st;

	if (!filename && !(filename = prepare_path("config", 0)))
		return -1;

	if (!(f = fopen(filename, "r")))
		return -1;

	if (stat(filename, &st) || !S_ISREG(st.st_mode)) {
		if (S_ISDIR(st.st_mode))
			errno = EISDIR;
		else
			errno = EINVAL;
		fclose(f);
		return -1;
	}

	gg_debug(GG_DEBUG_MISC, "// config_read();\n");

	if (!in_autoexec) {
		list_t l;

		for (l = bindings; l; ) {
			struct binding *b = l->data;

			l = l->next;

			if (!b->internal)
				ui_event("command", 1, "bind", "--del", b->key, NULL, NULL);
		}

		alias_free();
		timer_remove_user(-1);
		event_free();
		variable_free();
		variable_init();
		variable_set_default();

		gg_debug(GG_DEBUG_MISC, "\tflushed previous config\n");
	}

	while ((buf = read_file(f))) {
		i++;

		if (buf[0] == '#' || buf[0] == ';' || (buf[0] == '/' && buf[1] == '/')) {
			xfree(buf);
			continue;
		}

		if (!(foo = strchr(buf, ' '))) {
			xfree(buf);
			continue;
		}

		*foo++ = 0;

		if (!strcasecmp(buf, "set")) {
			char *bar;

			if (!(bar = strchr(foo, ' ')))
				ret = variable_set(foo, NULL, 1);
			else {
				*bar++ = 0;
				ret = variable_set(foo, bar, 1);
			}

			if (ret)
				gg_debug(GG_DEBUG_MISC, "\tunknown variable %s\n", foo);
		
		} else if (!strcasecmp(buf, "alias")) {
			gg_debug(GG_DEBUG_MISC, "\talias %s\n", foo);
			ret = alias_add(foo, 1, 1);
		} else if (!strcasecmp(buf, "on")) {
                        int flags;
                        char **pms = array_make(foo, " \t", 3, 1, 0);

                        if (array_count(pms) == 3 && (flags = event_flags(pms[0]))) {
				gg_debug(GG_DEBUG_MISC, "\ton %s %s %s\n", pms[0], pms[1], pms[2]);
                                ret = event_add(flags, pms[1], pms[2], 1);
			}

			array_free(pms);
		} else if (!strcasecmp(buf, "bind")) {
			char **pms = array_make(foo, " \t", 2, 1, 0);

			if (array_count(pms) == 2) {
				gg_debug(GG_DEBUG_MISC, "\tbind %s %s\n", pms[0], pms[1]);
				ui_event("command", 1, "bind", "--add", pms[0], pms[1], NULL);
			}

			array_free(pms);
		} else if (!strcasecmp(buf, "at")) {
			char **p = array_make(foo, " \t", 2, 1, 0);

			if (array_count(p) == 2) {
				char *name = NULL, *tmp;

				gg_debug(GG_DEBUG_MISC, "\tat %s %s\n", p[0], p[1]);

				if (strcmp(p[0], "(null)"))
					name = p[0];

				tmp = saprintf("/at -a %s %s", ((name) ? name : ""), p[1]);
				ret = command_exec(NULL, tmp, 1);
				xfree(tmp);
			}

			array_free(p);
		} else if (!strcasecmp(buf, "timer")) {
			char **p = array_make(foo, " \t", 3, 1, 0);
			char *tmp = NULL, *period_str = NULL, *name = NULL;
			time_t period;

			if (array_count(p) == 3) {
				gg_debug(GG_DEBUG_MISC, "\ttimer %s %s %s\n", p[0], p[1], p[2]);

				if (strcmp(p[0], "(null)"))
					name = p[0];

				if (!strncmp(p[1], "*/", 2)) {
					period = atoi(p[1] + 2);
					period_str = saprintf("*/%ld", (long) period);
				} else {
					period = atoi(p[1]) - time(NULL);
					period_str = saprintf("%ld", (long) period);
				}
		
				if (period > 0) {
					tmp = saprintf("/timer --add %s %s %s", (name) ? name : "", period_str, p[2]);
					ret = command_exec(NULL, tmp, 1);
					xfree(tmp);
				}

				xfree(period_str);
			}
				array_free(p);
                } else {
			ret = variable_set(buf, foo, 1);

			if (ret)
				gg_debug(GG_DEBUG_MISC, "\tunknown variable %s\n", buf);
		}

		if (!ret)
			good_file = 1;

		if (!good_file && i > 100) {
			xfree(buf);
			break;
		}

		xfree(buf);
	}
	
	fclose(f);

	if (!good_file && !home && !in_autoexec) {
		config_read(NULL);
		errno = EINVAL;
		return -2;
	}
	
	return 0;
}

/*
 * config_read_variable()
 *
 * czyta z pliku ~/.gg/config jedn± zmienn± nie interesuj±c siê typem,
 * znaczeniem, ani poprawno¶ci±.
 *
 *  - name - nazwa zmiennej.
 *
 * zaalokowany bufor z tre¶ci± zmiennej lub NULL, je¶li nie znaleziono.
 */
char *config_read_variable(const char *name)
{
	const char *filename;
	char *line;
	FILE *f;

	if (!name)
		return NULL;
	
	if (!(filename = prepare_path("config", 0)))
		return NULL;

	if (!(f = fopen(filename, "r")))
		return NULL;

	gg_debug(GG_DEBUG_MISC, "// config_read_variable(\"%s\");\n", name);

	while ((line = read_file(f))) {
		char *tmp;

		if (line[0] == '#' || line[0] == ';' || (line[0] == '/' && line[1] == '/')) {
			xfree(line);
			continue;
		}

		if (!(tmp = strchr(line, ' '))) {
			xfree(line);
			continue;
		}

		*tmp++ = 0;

		if (!strcasecmp(line, name)) {
			char *result = xstrdup(tmp);

			xfree(line);
			fclose(f);

			return result;
		}

		if (!strcasecmp(line, "set")) {
			char *foo;

			if ((foo = strchr(tmp, ' '))) {
				char *result;

				*foo++ = 0;
				result = xstrdup(foo);

				xfree(line);
				fclose(f);

				return result;
			}
		}
		
		xfree(line);
	}
	
	fclose(f);

	return NULL;
}

/*
 * config_write_variable()
 *
 * zapisuje jedn± zmienn± do pliku konfiguracyjnego.
 *
 *  - f - otwarty plik konfiguracji,
 *  - v - wpis zmiennej,
 *  - base64 - czy wolno nam u¿ywaæ base64 i zajmowaæ pamiêæ?
 */
void config_write_variable(FILE *f, struct variable *v, int base64)
{
	if (!f || !v)
		return;

	if (v->type == VAR_STR) {
		if (*(char**)(v->ptr)) {
			if (!v->display && base64) {
				char *tmp = base64_encode(*(char**)(v->ptr));
				if (config_save_password)
					fprintf(f, "%s \001%s\n", v->name, tmp);
				xfree(tmp);
			} else 	
				fprintf(f, "%s %s\n", v->name, *(char**)(v->ptr));
		}
	} else if (v->type == VAR_FOREIGN)
		fprintf(f, "%s %s\n", v->name, (char*) v->ptr);
	else
		fprintf(f, "%s %d\n", v->name, *(int*)(v->ptr));
}

/*
 * config_write_main()
 *
 * w³a¶ciwa funkcja zapisuj±ca konfiguracjê do podanego pliku.
 *
 *  - f - plik, do którego piszemy,
 *  - base64 - czy kodowaæ ukryte pola?
 */
void config_write_main(FILE *f, int base64)
{
	list_t l;

	if (!f)
		return;

	for (l = variables; l; l = l->next)
		config_write_variable(f, l->data, base64);

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;
		list_t m;

		for (m = a->commands; m; m = m->next)
			fprintf(f, "alias %s %s\n", a->name, (char*) m->data);
	}

        for (l = events; l; l = l->next) {
                struct event *e = l->data;

                fprintf(f, "on %s %s %s\n", event_format(e->flags), e->target, e->action);
        }

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;

		if (b->internal)
			continue;

		fprintf(f, "bind %s %s\n", b->key, b->action);
	}

	for (l = timers; l; l = l->next) {
		struct timer *t = l->data;
		const char *name = NULL;

		if (t->type != TIMER_COMMAND)
			continue;

		/* nie ma sensu zapisywaæ */
		if (!t->persistent && t->ends.tv_sec - time(NULL) < 5)
			continue;

		/* posortuje, je¶li nie ma nazwy */
		if (t->name && !xisdigit(t->name[0]))
			name = t->name;
		else
			name = "(null)";

		if (t->at) {
			char buf[100];
			time_t foo = (time_t) t->ends.tv_sec;
			struct tm *tt = localtime(&foo);

			strftime(buf, sizeof(buf), "%G%m%d%H%M.%S", tt);

			if (t->persistent)
				fprintf(f, "at %s %s/%s %s\n", name, buf, itoa(t->period), t->command);
			else
				fprintf(f, "at %s %s %s\n", name, buf, t->command);
		} else {
			char *foo;

			if (t->persistent)
				foo = saprintf("*/%s", itoa(t->period));
			else
				foo = saprintf("%s", itoa(t->ends.tv_sec));

			fprintf(f, "timer %s %s %s\n", name, foo, t->command);

			xfree(foo);
		}
	}

}

/*
 * config_write()
 *
 * zapisuje aktualn± konfiguracjê do pliku ~/.gg/config lub podanego.
 *
 * 0/-1
 */
int config_write(const char *filename)
{
	char tmp[PATH_MAX];
	FILE *f;

	if (!filename && !(filename = prepare_path("config", 1)))
		return -1;

	snprintf(tmp, sizeof(tmp), "%s.%d.%ld", filename, (int) getpid(), (long) time(NULL));
	
	if (!(f = fopen(tmp, "w")))
		return -1;
	
	fchmod(fileno(f), 0600);

	config_write_main(f, 1);

	if (fclose(f) == EOF) {
		return -1;
		unlink(tmp);
	}

	rename(tmp, filename);
	
	return 0;
}

/*
 * config_write_partly()
 *
 * zapisuje podane zmienne, nie zmieniaj±c reszty konfiguracji.
 *  
 *  - vars - tablica z nazwami zmiennych do zapisania.
 * 
 * 0/-1
 */
int config_write_partly(char **vars)
{
	const char *filename;
	char *newfn, *line;
	FILE *fi, *fo;
	int *wrote, i;

	if (!vars)
		return -1;

	if (!(filename = prepare_path("config", 1)))
		return -1;
	
	if (!(fi = fopen(filename, "r")))
		return -1;

	newfn = saprintf("%s.%d.%ld", filename, (int) getpid(), (long) time(NULL));

	if (!(fo = fopen(newfn, "w"))) {
		xfree(newfn);
		fclose(fi);
		return -1;
	}
	
	wrote = xcalloc(array_count(vars) + 1, sizeof(int));
	
	fchmod(fileno(fo), 0600);

	while ((line = read_file(fi))) {
		char *tmp;

		if (line[0] == '#' || line[0] == ';' || (line[0] == '/' && line[1] == '/'))
			goto pass;

		if (!strchr(line, ' '))
			goto pass;

		if (!strncasecmp(line, "alias ", 6))
			goto pass;

		if (!strncasecmp(line, "on ", 3))
			goto pass;

		if (!strncasecmp(line, "bind ", 5))
			goto pass;

		tmp = line;

		if (!strncasecmp(tmp, "set ", 4))
			tmp += 4;
		
		for (i = 0; vars[i]; i++) {
			int len = strlen(vars[i]);

			if (strlen(tmp) < len + 1)
				continue;

			if (strncasecmp(tmp, vars[i], len) || tmp[len] != ' ')
				continue;
			
			config_write_variable(fo, variable_find(vars[i]), 1);

			wrote[i] = 1;
			
			xfree(line);
			line = NULL;
			
			break;
		}

		if (!line)
			continue;

pass:
		fprintf(fo, "%s\n", line);
		xfree(line);
	}

	for (i = 0; vars[i]; i++) {
		if (wrote[i])
			continue;

		config_write_variable(fo, variable_find(vars[i]), 1);
	}

	xfree(wrote);
	
	fclose(fi);

	if (fclose(fo) == EOF) {
		unlink(newfn);
		xfree(newfn);
		return -1;
	}
	
	rename(newfn, filename);

	xfree(newfn);

	return 0;
}

/*
 * config_write_crash()
 *
 * funkcja zapisuj±ca awaryjnie konfiguracjê. nie powinna alokowaæ ¿adnej
 * pamiêci.
 */
void config_write_crash()
{
	char name[32];
	FILE *f;

	chdir(config_dir);

	snprintf(name, sizeof(name), "config.%d", (int) getpid());
	if (!(f = fopen(name, "w")))
		return;

	chmod(name, 0400);
	
	config_write_main(f, 0);
	
	fclose(f);
}

/*
 * debug_write_crash()
 *
 * zapisuje ostatnie linie z debug.
 */
void debug_write_crash()
{
	char name[32];
	FILE *f;
	list_t l;

	chdir(config_dir);

	snprintf(name, sizeof(name), "debug.%d", (int) getpid());
	if (!(f = fopen(name, "w")))
		return;

	chmod(name, 0400);
	
	for (l = buffers; l; l = l->next) {
		struct buffer *b = l->data;

		if (b->type == BUFFER_DEBUG)
			fprintf(f, "%s\n", b->line);
	}
	
	fclose(f);
}
