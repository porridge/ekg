/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dynstuff.h"
#include "stuff.h"
#include "themes.h"
#include "xmalloc.h"
#include "ui.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

char *prompt_cache = NULL, *prompt2_cache = NULL, *error_cache = NULL;
const char *timestamp_cache = NULL;

int no_prompt_cache = 0;

list_t formats = NULL;

/*
 * format_find()
 *
 * odnajduje warto¶æ danego formatu. je¶li nie znajdzie, zwraca pusty ci±g,
 * ¿eby nie musieæ uwa¿aæ na ¿adne null-references.
 *
 *  - name.
 */
const char *format_find(const char *name)
{
	const char *tmp;
	int hash;
	list_t l;

	if (!name)
		return "";

	hash = ekg_hash(name);

	if (config_speech_app && !strchr(name, ',')) {
		char *name2 = saprintf("%s,speech", name);
		const char *tmp;
		
		if (strcmp((tmp = format_find(name2)), "")) {
			xfree(name2);
			return tmp;
		}
		
		xfree(name2);
	}

	if (config_theme && (tmp = strchr(config_theme, ',')) && !strchr(name, ',')) {
		char *name2 = saprintf("%s,%s", name, tmp + 1);
		const char *tmp;
		
		if (strcmp((tmp = format_find(name2)), "")) {
			xfree(name2);
			return tmp;
		}
		
		xfree(name2);
	}
	
	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		if (hash == f->name_hash && !strcasecmp(f->name, name))
			return f->value;
	}
	
	return "";
}

/*
 * format_ansi()
 *
 * zwraca sekwencjê ansi odpowiadaj±c± danemu kolorkowi z thememów ekg.
 */
const char *format_ansi(char ch)
{
	if (ch == 'k')
		return "\033[0;30m";
	if (ch == 'K')
		return "\033[1;30m";
	if (ch == 'l')
		return "\033[40m";
	if (ch == 'r')
		return "\033[0;31m";
	if (ch == 'R')
		return "\033[1;31m";
	if (ch == 's')
		return "\033[41m";
	if (ch == 'g')
		return "\033[0;32m";
	if (ch == 'G')
		return "\033[1;32m";
	if (ch == 'h')
		return "\033[42m";
	if (ch == 'y')
		return "\033[0;33m";
	if (ch == 'Y')
		return "\033[1;33m";
	if (ch == 'z')
		return "\033[43m";
	if (ch == 'b')
		return "\033[0;34m";
	if (ch == 'B')
		return "\033[1;34m";
	if (ch == 'e')
		return "\033[44m";
	if (ch == 'm' || ch == 'p')
		return "\033[0;35m";
	if (ch == 'M' || ch == 'P')
		return "\033[1;35m";
	if (ch == 'q')
		return "\033[45m";
	if (ch == 'c')
		return "\033[0;36m";
	if (ch == 'C')
		return "\033[1;36m";
	if (ch == 'd')
		return "\033[46m";
	if (ch == 'w')
		return "\033[0;37m";
	if (ch == 'W')
		return "\033[1;37m";
	if (ch == 'x')
		return "\033[47m";
	if (ch == 'i')
		return "\033[5m";
	if (ch == 'n')
		return "\033[0m";
	if (ch == 'T')
		return "\033[1m";

	return "";
}

/*
 * va_format_string()
 *
 * formatuje zgodnie z podanymi parametrami ci±g znaków.
 *
 *  - format - warto¶æ, nie nazwa formatu,
 *  - ap - argumenty.
 */
char *va_format_string(const char *format, va_list ap)
{
	static int dont_resolve = 0;
	string_t buf = string_init(NULL);
	const char *p, *args[9];
	int i, argc = 0;

	/* liczymy ilo¶æ argumentów */
	for (p = format; *p; p++) {
		if (*p != '%')
			continue;

		p++;

		if (!*p)
			break;

		if (*p == '@') {
			p++;

			if (!*p)
				break;

			if ((*p - '0') > argc)
				argc = *p - '0';
			
		} else if (*p == '(' || *p == '[') {
			if (*p == '(') {
				while (*p && *p != ')')
					p++;
			} else {
				while (*p && *p != ']')
					p++;
			}

			if (*p)
				p++;
			
			if (!*p)
				break;
			
			if ((*p - '0') > argc)
				argc = *p - '0';
		} else {
			if (*p >= '1' && *p <= '9' && (*p - '0') > argc)
				argc = *p - '0';
		}
	}

	for (i = 0; i < 9; i++)
		args[i] = NULL;

	for (i = 0; i < argc; i++)
		args[i] = va_arg(ap, char*);

	if (!dont_resolve) {
		dont_resolve = 1;
		if (no_prompt_cache) {
			/* zawsze czytaj */
			timestamp_cache = format_find("timestamp");
			prompt_cache = format_string(format_find("prompt"));
			prompt2_cache = format_string(format_find("prompt2"));
			error_cache = format_string(format_find("error"));
		} else {
			/* tylko je¶li nie s± keszowanie */
			if (!timestamp_cache)
				timestamp_cache = format_find("timestamp");
			if (!prompt_cache)
				prompt_cache = format_string(format_find("prompt"));
			if (!prompt2_cache)
				prompt2_cache = format_string(format_find("prompt2"));
			if (!error_cache)
				error_cache = format_string(format_find("error"));
		}
		dont_resolve = 0;
	}
	
	p = format;
	
	while (*p) {
		if (*p == '%') {
			int fill_before, fill_after, fill_soft, fill_length;
			char fill_char;

			p++;
			if (!*p)
				break;
			if (*p == '%')
				string_append_c(buf, '%');
			if (*p == '>')
				string_append(buf, prompt_cache);
			if (*p == ')')
				string_append(buf, prompt2_cache);
			if (*p == '!')
				string_append(buf, error_cache);
			if (*p == '|')
				string_append(buf, "\033[00m");	/* g³upie, wiem */
			if (*p == ']')
				string_append(buf, "\033[000m");	/* jeszcze g³upsze */
			if (*p == '#')
				string_append(buf, timestamp(timestamp_cache));
			else if (config_display_color) {
				string_append(buf, format_ansi(*p));
			}

			if (*p == '@') {
				char *str = (char*) args[*(p + 1) - '1'];

				if (str) {
					char *q = str + strlen(str) - 1;

					while (q >= str && !isalpha_pl_PL(*q))
						q--;

					if (*q == 'a')
						string_append(buf, "a");
					else
						string_append(buf, "y");
				}
				p += 2;
				continue;
			}

			fill_before = 0;
			fill_after = 0;
			fill_length = 0;
			fill_char = ' ';
			fill_soft = 1;

			if (*p == '[' || *p == '(') {
				char *q;

				fill_soft = (*p == '(');

				p++;
				fill_char = ' ';

				if (*p == '.') {
					fill_char = '0';
					p++;
				} else if (*p == ',') {
					fill_char = '.';
					p++;
				} else if (*p == '_') {
					fill_char = '_';
					p++;
				}

				fill_length = strtol(p, &q, 0);
				p = q;
				if (fill_length > 0)
					fill_after = 1;
				else {
					fill_length = -fill_length;
					fill_before = 1;
				}
				p++;
			}

			if (*p >= '1' && *p <= '9') {
				char *str = (char *) args[*p - '1'];
				int i, len;

				if (!str)
					str = "";
				len = strlen(str);

				if (fill_length) {
					if (len >= fill_length) {
						if (!fill_soft)
							len = fill_length;
						fill_length = 0;
					} else
						fill_length -= len;
				}

				if (fill_before)
					for (i = 0; i < fill_length; i++)
						string_append_c(buf, fill_char);

				string_append_n(buf, str, len);

				if (fill_after) 
					for (i = 0; i < fill_length; i++)
						string_append_c(buf, fill_char);

			}
		} else
			string_append_c(buf, *p);

		p++;
	}

	if (!dont_resolve && no_prompt_cache)
		theme_cache_reset();

	if (!config_display_pl_chars)
		iso_to_ascii(buf->str);

	return string_free(buf, 0);
}

/*
 * reformat_string()
 *
 * zamienia sformatowany ci±g znaków ansi na Nowy-i-Lepszy(tm).
 *
 *  - str - ci±g znaków,
 * 
 * zwraca zaalokowan± fstring_t.
 */
fstring_t reformat_string(const char *str)
{
	fstring_t res = xnew(struct fstring_s);
	unsigned char attr = 128;
	int i, j, len = 0;
	
	for (i = 0; str[i]; i++) {
		if (str[i] == 27) {
			if (str[i + 1] != '[')
				continue;

			while (str[i] && !isalpha_pl_PL(str[i]))
				i++;

			i--;
			
			continue;
		}

		if (str[i] == 9) {
			len += (8 - (len % 8));
			continue;
		}

		if (str[i] == 13)
			continue;

		len++;
	}

	res->str = xmalloc(len + 1);
	res->attr = xmalloc(len + 1);
	res->prompt_len = 0;
	res->prompt_empty = 0;

	for (i = 0, j = 0; str[i]; i++) {
		if (str[i] == 27) {
			int tmp = 0;

			if (str[i + 1] != '[')
				continue;

			i += 2;

			/* obs³uguje tylko "\033[...m", tak ma byæ */
			
			for (; str[i]; i++) {
				if (str[i] >= '0' && str[i] <= '9') {
					tmp *= 10;
					tmp += (str[i] - '0');
				}

				if (str[i] == ';' || isalpha_pl_PL(str[i])) {
					if (tmp == 0) {
						attr = 128;

						/* prompt jako \033[00m */
						if (str[i - 1] == '0' && str[i - 2] == '0')
							res->prompt_len = j;

						/* odstêp jako \033[000m */
						if (i > 3 && str[i - 1] == '0' && str[i - 2] == '0' && str[i - 3] == 0) {
							res->prompt_len = j;
							res->prompt_empty = 1;
						}
					}

					if (tmp == 1)
						attr |= 64;

					if (tmp >= 30 && tmp <= 37) {
						attr &= 127;
						attr |= (tmp - 30);
					}

					if (tmp >= 40 && tmp <= 47) {
						attr &= 127;
						attr |= (tmp - 40) << 3;
					}

					tmp = 0;
				}

				if (isalpha_pl_PL(str[i]))
					break;
			}

			continue;
		}

		if (str[i] == 13)
			continue;

		if (str[i] == 9) {
			int k = 0, l = 8 - (j % 8);

			for (k = 0; k < l; j++, k++) {
				res->str[j] = ' ';
				res->attr[j] = attr;
			}

			continue;
		}

		res->str[j] = str[i];
		res->attr[j] = attr;

		j++;
	}

	res->str[j] = 0;
	res->attr[j] = 0;

	return res;
}

/*
 * format_string()
 *
 * j.w. tyle ¿e nie potrzeba dawaæ mu va_list, a wystarcz± zwyk³e parametry.
 *
 *  - format... - j.w.,
 */
char *format_string(const char *format, ...)
{
	va_list ap;
	char *tmp;
	
	va_start(ap, format);
	tmp = va_format_string(format, ap);
	va_end(ap);

	return tmp;
}

/*
 * print()
 *
 * drukuje na stdout tekst, bior±c pod uwagê nazwê, nie warto¶æ formatu.
 * parametry takie jak zdefiniowano. pierwszy to %1, drugi to %2.
 */
void print(const char *theme, ...)
{
	va_list ap;
	char *tmp;
	
	va_start(ap, theme);
	tmp = va_format_string(format_find(theme), ap);
	
	ui_print("__current", 0, (tmp) ? tmp : "");
	
	xfree(tmp);
	va_end(ap);
}

/*
 * print_status()
 *
 * wy¶wietla tekst w oknie statusu.
 */
void print_status(const char *theme, ...)
{
	va_list ap;
	char *tmp;
	
	va_start(ap, theme);
	tmp = va_format_string(format_find(theme), ap);
	
	ui_print("__status", 0, (tmp) ? tmp : "");
	
	xfree(tmp);
	va_end(ap);
}

/*
 * print_window()
 *
 * wy¶wietla tekst w podanym oknie.
 *  
 *  - target - nazwa okna,
 *  - separate - czy niezbêdne jest otwieranie nowego okna?
 *  - theme, ... - tre¶æ.
 */
void print_window(const char *target, int separate, const char *theme, ...)
{
	va_list ap;
	char *tmp;
	
	if (!target)
		target = "__current";

	va_start(ap, theme);
	tmp = va_format_string(format_find(theme), ap);
	
	ui_print(target, separate, (tmp) ? tmp : "");
	
	xfree(tmp);
	va_end(ap);
}

/*
 * theme_cache_reset()
 *
 * usuwa cache'owane prompty. przydaje siê przy zmianie theme'u.
 */
void theme_cache_reset()
{
	xfree(prompt_cache);
	xfree(prompt2_cache);
	xfree(error_cache);
	
	prompt_cache = prompt2_cache = error_cache = NULL;
	timestamp_cache = NULL;
}

/*
 * format_add()
 *
 * dodaje dan± formatkê do listy.
 *
 *  - name - nazwa,
 *  - value - warto¶æ,
 *  - replace - je¶li znajdzie, to zostawia (=0) lub zamienia (=1).
 */
int format_add(const char *name, const char *value, int replace)
{
	struct format f;
	list_t l;
	int hash;

	if (!name || !value)
		return -1;

	hash = ekg_hash(name);

	if (hash == ekg_hash("no_prompt_cache") && !strcasecmp(name, "no_prompt_cache")) {
		no_prompt_cache = 1;
		return 0;
	}
	
	for (l = formats; l; l = l->next) {
		struct format *g = l->data;

		if (hash == g->name_hash && !strcasecmp(name, g->name)) {
			if (replace) {
				xfree(g->value);
				g->value = xstrdup(value);
			}

			return 0;
		}
	}

	f.name = xstrdup(name);
	f.name_hash = ekg_hash(name);
	f.value = xstrdup(value);

	return (list_add(&formats, &f, sizeof(f)) ? 0 : -1);
}

/*
 * format_remove()
 *
 * usuwa formatkê o danej nazwie.
 *
 *  - name.
 */
int format_remove(const char *name)
{
	list_t l;

	if (!name)
		return -1;

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		if (!strcasecmp(f->name, name)) {
			xfree(f->value);
			xfree(f->name);
			list_remove(&formats, f, 1);
		
			return 0;
		}
	}

        return -1;
}

/*
 * try_open() // funkcja wewnêtrzna
 *
 * próbuje otworzyæ plik, je¶li jeszcze nie jest otwarty.
 *
 *  - prevfd - deskryptor z poprzedniego wywo³ania,
 *  - prefix - ¶cie¿ka,
 *  - filename - nazwa pliku.
 */
static FILE *try_open(FILE *prevfd, const char *prefix, const char *filename)
{
	char buf[PATH_MAX];
	int save_errno;
	FILE *f;

	if (prevfd)
		return prevfd;

	if (prefix)
		snprintf(buf, sizeof(buf), "%s/%s", prefix, filename);
	else
		snprintf(buf, sizeof(buf), "%s", filename);

	if ((f = fopen(buf, "r")))
		return f;

	if (prefix)
		snprintf(buf, sizeof(buf), "%s/%s.theme", prefix, filename);
	else
		snprintf(buf, sizeof(buf), "%s.theme", filename);

	save_errno = errno;
	
	if ((f = fopen(buf, "r")))
		return f;

	if (errno == ENOENT)
		errno = save_errno;

	return NULL;
}

/*
 * theme_read()
 *
 * wczytuje opis wygl±du z podanego pliku. 
 *
 *  - filename - nazwa pliku z opisem,
 *  - replace - czy zastêpowaæ istniej±ce wpisy.
 *
 * zwraca 0 je¶li wszystko w porz±dku, -1 w przypadku b³êdu.
 */
int theme_read(const char *filename, int replace)
{
        char *buf;
        FILE *f = NULL;

        if (!filename) {
                filename = prepare_path("default.theme", 0);
		if (!filename || !(f = fopen(filename, "r")))
			return -1;
        } else {
		char *fn = xstrdup(filename), *tmp;

		if ((tmp = strchr(fn, ',')))
			*tmp = 0;
		
		errno = ENOENT;
		f = try_open(NULL, NULL, fn);

		if (!strchr(filename, '/')) {
			f = try_open(f, prepare_path("", 0), fn);
			f = try_open(f, prepare_path("themes", 0), fn);
			f = try_open(f, DATADIR "/themes", fn);
		}

		xfree(fn);

		if (!f)
			return -1;
	}

	theme_free();
	theme_init();
	ui_event("theme_init");

	while ((buf = read_file(f))) {
		char *value, *p;

		if (buf[0] == '#') {
			xfree(buf);
			continue;
		}

		if (!(value = strchr(buf, ' '))) {
			xfree(buf);
			continue;
		}

		*value++ = 0;

		for (p = value; *p; p++) {
			if (*p == '\\') {
				if (!*(p + 1))
					break;
				if (*(p + 1) == 'n')
					*p = '\n';
				memmove(p + 1, p + 2, strlen(p + 1));
			}
		}

		if (buf[0] == '-')
			format_remove(buf + 1);
		else
			format_add(buf, value, replace);

		xfree(buf);
        }

        fclose(f);

	theme_cache_reset();

        return 0;
}

/*
 * theme_free()
 *
 * usuwa formatki z pamiêci.
 */
void theme_free()
{
	list_t l;

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		xfree(f->name);
		xfree(f->value);
	}	

	list_destroy(formats, 1);
	formats = NULL;

	theme_cache_reset();
}

/*
 * theme_init()
 *
 * ustawia domy¶lne warto¶ci formatek.
 */
void theme_init()
{
	theme_cache_reset();

	/* wykorzystywane w innych formatach */
	format_add("prompt", "%K:%g:%G:%n", 1);
	format_add("prompt,speech", " ", 1);
	format_add("prompt2", "%K:%c:%C:%n", 1);
	format_add("prompt2,speech", " ", 1);
	format_add("error", "%K:%r:%R:%n", 1);
	format_add("error,speech", "b³±d!", 1);
	format_add("timestamp", "%H:%M", 1);
	format_add("timestamp,speech", " ", 1);
	
	/* prompty dla ui-readline */
	format_add("readline_prompt", "% ", 1);
	format_add("readline_prompt_away", "/ ", 1);
	format_add("readline_prompt_invisible", ". ", 1);
	format_add("readline_prompt_query", "%1> ", 1);
	format_add("readline_prompt_win", "%1%% ", 1);
	format_add("readline_prompt_away_win", "%1/ ", 1);
	format_add("readline_prompt_invisible_win", "%1. ", 1);
	format_add("readline_prompt_query_win", "%2:%1> ", 1);
	format_add("readline_prompt_win_act", "%1 (act/%2)%% ", 1);
	format_add("readline_prompt_away_win_act", "%1 (act/%2)/ ", 1);
	format_add("readline_prompt_invisible_win_act", "%1 (act/%2). ", 1);
	format_add("readline_prompt_query_win_act", "%2:%1 (act/%3)> ", 1);
					
	format_add("readline_more", "-- Wci¶nij Enter by kontynuowaæ lub Ctrl-D by przerwaæ --", 1);

	/* prompty i statusy dla ui-ncurses */
	format_add("ncurses_prompt_none", "", 1);
	format_add("ncurses_prompt_query", "[%1] ", 1);
	format_add("statusbar", " %c(%w%{time}%c)%w %{?uin %c(%w%{?!nick uin}%{nick}%c/%{?away %w}%{?avail %W}%{?invisible %K}%{?notavail %k}%{uin}%c) }%c%{?window (%wwin%c/%w%{window}}%{?query %c:%W%{query}}%{?debug %c(%Cdebug}%c)%w%{?activity  %c(%wact%c/%w}%{activity}%{?activity %c)%w}%{?mail  %c(%wmail%c/%w}%{mail}%{?mail %c)}%{?more  %c(%Gmore%c)}", 1);
	format_add("header", " %{?query %c(%{?query_away %w}%{?query_avail %W}%{?query_invisible %K}%{?query_notavail %k}%{query}%{?query_descr %c/%w%{query_descr}}%c) %{?query_ip (%wip%c/%w%{query_ip}%c)}}%{?!query %c(%wekg%c/%w%{version}%c) (%w%{url}%c)}", 1);

	/* dla funkcji format_user() */
	format_add("known_user", "%T%1%n/%2", 1);
	format_add("known_user,speech", "%1", 1);
	format_add("unknown_user", "%T%1%n", 1);
	
	/* czêsto wykorzystywane, ró¿ne, przydatne itd. */
	format_add("none", "%1\n", 1);
	format_add("generic", "%> %1\n", 1);
	format_add("generic2", "%) %1\n", 1);
	format_add("generic_error", "%! %1\n", 1);
	format_add("debug", "%n%1\n", 1);
	format_add("not_enough_params", "%! Za ma³o parametrów. Spróbuj %Thelp %1%n\n", 1);
	format_add("invalid_params", "%! Nieprawid³owe parametry. Spróbuj %Thelp %1%n\n", 1);
	format_add("invalid_uin", "%! Nieprawid³owy numer u¿ytkownika\n", 1);
	format_add("invalid_nick", "%! Nieprawid³owa nazwa u¿ytkownika\n", 1);
	format_add("user_not_found", "%! Nie znaleziono u¿ytkownika %T%1%n\n", 1);
	format_add("not_implemented", "%! Tej funkcji jeszcze nie ma\n", 1);
	format_add("unknown_command", "%! Nieznane polecenie: %T%1%n\n", 1);
	format_add("welcome", "%> %TEKG-%1%n (Eksperymentalny Klient Gadu-Gadu)\n%> Program jest rozprowadzany na zasadach licencji GPL v2\n%> %RPrzed u¿yciem wci¶nij F1 lub wpisz ,,help''%n\n\n", 1);
	format_add("welcome,speech", "witamy w e k g", 1);
	format_add("ekg_version", "%) EKG - Eksperymentalny Klient Gadu-Gadu (%T%1%n)\n%) libgadu-%1 (protokó³ %2, klient %3)\n%) skompilowano: %4\n", 1);
	format_add("secure", "%Y(szyfrowane)%n ", 1);
	format_add("log_failed", "%! Nie mo¿na zapisaæ do historii: %1\n", 1);

	/* mail */
	format_add("new_mail_one", "%) Masz now± wiadomo¶æ email\n", 1);
	format_add("new_mail_two_four", "%) Masz %1 nowe wiadomo¶ci email\n", 1);
	format_add("new_mail_more", "%) Masz %1 nowych wiadomo¶ci email\n", 1);

	/* add, del */
	format_add("user_added", "%> Dopisano %T%1%n do listy kontaktów\n", 1);
	format_add("user_deleted", "%) Usuniêto %T%1%n z listy kontaktów\n", 1);
	format_add("user_cleared_list", "%) Wyczyszczono listê kontaktów\n", 1);
	format_add("user_exists", "%! %T%1%n ju¿ istnieje w li¶cie kontaktów\n", 1);
	format_add("user_exists_other", "%! %T%1%n ju¿ istnieje w li¶cie kontaktów jako %2\n", 1);

	/* zmiany stanu */
	format_add("away", "%> Zmieniono stan na zajêty\n", 1);
	format_add("away_descr", "%> Zmieniono stan na zajêty: %T%1%n%2\n", 1);
	format_add("back", "%> Zmieniono stan na dostêpny\n", 1);
	format_add("back_descr", "%> Zmieniono stan na dostêpny: %T%1%n%2%n\n", 1);
	format_add("invisible", "%> Zmieniono stan na niewidoczny\n", 1);
	format_add("invisible_descr", "%> Zmieniono stan na niewidoczny: %T%1%n%2\n", 1);
	format_add("private_mode_is_on", "%> Tryb ,,tylko dla znajomych'' jest w³±czony\n", 1);
	format_add("private_mode_is_off", "%> Tryb ,,tylko dla znajomych'' jest wy³±czony\n", 1);
	format_add("private_mode_on", "%> W³±czono tryb ,,tylko dla znajomych''\n", 1);
	format_add("private_mode_off", "%> Wy³±czono tryb ,,tylko dla znajomych''\n", 1);
	format_add("private_mode_invalid", "%! Nieprawid³owa warto¶æ\n", 1);
	format_add("descr_too_long", "%! D³ugo¶æ opisu przekracza limit. Ilo¶æ uciêtych znaków: %T%1%n\n", 1);
	
	/* pomoc */
	format_add("help", "%> %T%1%n%2 - %3\n", 1);
	format_add("help_more", "%) %|%1\n", 1);
	format_add("help_alias", "%) %T%1%n jest aliasem i nie posiada opisu\n", 1);
	format_add("help_footer", "\n%> %|Wiêcej szczegó³ów na temat komend zwróci %Thelp <komenda>%n. Poprzedzenie komendy znakiem %T^%n spowoduje ukrycie jej wyniku. Zamiast parametru <numer/alias> mo¿na u¿yæ znaku %T$%n oznaczaj±cego aktualnego rozmówcê.\n\n", 1);
	format_add("help_quick", "%> %|Przed u¿yciem przeczytaj ulotkê. Plik %Tdocs/ULOTKA%n zawiera krótki przewodnik po za³±czonej dokumentacji. Je¶li go nie masz, mo¿esz ¶ci±gn±æ pakiet ze strony %Thttp://dev.null.pl/ekg/%n\n", 1);
	format_add("help_set_file_not_found", "%! Nie znaleziono opisu zmiennych (nieprawid³owa instalacja)\n", 1);
	format_add("help_set_var_not_found", "%! Nie znaleziono opisu zmiennej %T%1%n\n", 1);
	format_add("help_set_header", "%> %T%1%n (%2, domy¶lna warto¶æ: %3)\n%>\n", 1);
	format_add("help_set_body", "%> %|%1\n", 1);
	format_add("help_set_footer", "", 1);

	/* ignore, unignore, block, unblock */
	format_add("ignored_added", "%> Dodano %T%1%n do listy ignorowanych\n", 1);
	format_add("ignored_modified", "%> Zmodyfikowano poziom ignorowania %T%1%n\n", 1);
	format_add("ignored_deleted", "%) Usuniêto %1 z listy ignorowanych\n", 1);
	format_add("ignored_deleted_all", "%) Usuniêto wszystkich z listy ignorowanych\n", 1);
	format_add("ignored_exist", "%! %1 jest ju¿ na li¶cie ignorowanych\n", 1);
	format_add("ignored_list", "%> %1 %2\n", 1);
	format_add("ignored_list_unknown_sender", "%> Ignorowanie wiadomo¶ci od nieznanych u¿ytkowników\n", 1);
	format_add("ignored_list_empty", "%! Lista ignorowanych u¿ytkowników jest pusta\n", 1);
	format_add("error_not_ignored", "%! %1 nie jest na li¶cie ignorowanych\n", 1);
	format_add("blocked_added", "%> Dodano %T%1%n do listy blokowanych\n", 1);
	format_add("blocked_deleted", "%) Usuniêto %1 z listy blokowanych\n", 1);
	format_add("blocked_deleted_all", "%) Usuniêto wszystkich z listy blokowanych\n", 1);
	format_add("blocked_exist", "%! %1 jest ju¿ na li¶cie blokowanych\n", 1);
	format_add("blocked_list", "%> %1\n", 1);
	format_add("blocked_list_empty", "%! Lista blokowanych u¿ytkowników jest pusta\n", 1);
	format_add("error_not_blocked", "%! %1 nie jest na li¶cie blokowanych\n", 1);

	/* lista kontaktów */
	format_add("list_empty", "%! Lista kontaktów jest pusta\n", 1);
	format_add("list_avail", "%> %1 %Y(dostêpn%@2)%n %b%3:%4%n\n", 1);
	format_add("list_avail_descr", "%> %1 %Y(dostêpn%@2: %n%5%Y)%n %b%3:%4%n\n", 1);
	format_add("list_busy", "%> %1 %G(zajêt%@2)%n %b%3:%4%n\n", 1);
	format_add("list_busy_descr", "%> %1 %G(zajêt%@2: %n%5%G)%n %b%3:%4%n\n", 1);
	format_add("list_not_avail", "%> %1 %r(niedostêpn%@2)%n\n", 1);
	format_add("list_not_avail_descr", "%> %1 %r(niedostêpn%@2: %n%5%r)%n\n", 1);
	format_add("list_invisible", "%> %1 %c(niewidoczn%@2)%n %b%3:%4%n\n", 1);
	format_add("list_invisible_descr", "%> %1 %c(niewidoczn%@2: %n%5%c)%n %b%3:%4%n\n", 1);
	format_add("list_blocked", "%> %1 %m(blokuj±c%@2)%n\n", 1);
	format_add("list_unknown", "%> %1\n", 1);
	format_add("modify_offline", "%> %1 nie bêdzie widzieæ naszego stanu\n", 1);
	format_add("modify_online", "%> %1 bêdzie widzieæ nasz stan\n", 1);
	format_add("modify_done", "%> Zmieniono wpis w li¶cie kontaktów\n", 1);

	/* lista kontaktów z boku ekranu */
	format_add("contacts_header", "", 1);
	format_add("contacts_header_group", "%K %1%n", 1);
	format_add("contacts_avail_header", "", 1);
	format_add("contacts_avail", " %Y%1%n", 1);
	format_add("contacts_avail_descr", "%Ki%Y%1%n", 1);
	format_add("contacts_avail_descr_full", "%Ki%Y%1%n %2", 1);
	format_add("contacts_avail_footer", "", 1);
	format_add("contacts_busy_header", "", 1);
	format_add("contacts_busy", " %G%1%n", 1);
	format_add("contacts_busy_descr", "%Ki%G%1%n", 1);
	format_add("contacts_busy_descr_full", "%Ki%G%1%n %2", 1);
	format_add("contacts_busy_footer", "", 1);
	format_add("contacts_not_avail_header", "", 1);
	format_add("contacts_not_avail", " %r%1%n", 1);
	format_add("contacts_not_avail_descr", "%Ki%r%1%n", 1);
	format_add("contacts_not_avail_descr_full", "%Ki%r%1%n %2", 1);
	format_add("contacts_not_avail_footer", "", 1);
	format_add("contacts_invisible_header", "", 1);
	format_add("contacts_invisible", " %c%1%n", 1);
	format_add("contacts_invisible_descr", "%Ki%c%1%n", 1);
	format_add("contacts_invisible_descr_full", "%Ki%c%1%n %2", 1);
	format_add("contacts_invisible_footer", "", 1);
	format_add("contacts_blocking_header", "", 1);
	format_add("contacts_blocking", " %m%1%n", 1);
	format_add("contacts_blocking_footer", "", 1);
	format_add("contacts_footer", "", 1);
	format_add("contacts_footer_group", "", 1);
		
	/* ¿egnamy siê, zapisujemy konfiguracjê */
	format_add("quit", "%> Papa\n", 1);
	format_add("quit_descr", "%> Papa: %T%1%n%2\n", 1);
	format_add("config_changed", "Zapisaæ now± konfiguracjê? (tak/nie) ", 1);
	format_add("saved", "%> Zapisano ustawienia\n", 1);
	format_add("error_saving", "%! Podczas zapisu ustawieñ wyst±pi³ b³±d\n", 1);

	/* przychodz±ce wiadomo¶ci */
	format_add("message_header", "%g.-- %n%1 %c%2%n%4%g--- -- -%n\n", 1);
	format_add("message_conference_header", "%g.-- %g[%T%3%g] -- %n%1 %c%2%4%g--- -- -%n\n", 1);
	format_add("message_footer", "%g`----- ---- --- -- -%n\n", 1);
	format_add("message_line", "%g|%n %|%1%n\n", 1);
	format_add("message_line_width", "-8", 1);
	format_add("message_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("message_timestamp_today", "(%H:%M) ", 1);
	format_add("message_timestamp_now", "", 1);

	format_add("message_header,speech", "wiadomo¶æ od %1: ", 1);
	format_add("message_conference_header,speech", "wiadomo¶æ od %1 w konferencji %3: ", 1);
	format_add("message_line,speech", "%1\n", 1);
	format_add("message_footer,speech", ".", 1);

	format_add("chat_header", "%c.-- %n%1 %c%2%n%4%c--- -- -%n\n", 1);
	format_add("chat_conference_header", "%c.-- %c[%T%3%c] -- %n%1 %c%2%4%c--- -- -%n\n", 1);
	format_add("chat_footer", "%c`----- ---- --- -- -%n\n", 1);
	format_add("chat_line", "%c|%n %|%1%n\n", 1);
	format_add("chat_line_width", "-8", 1);
	format_add("chat_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("chat_timestamp_today", "(%H:%M) ", 1);
	format_add("chat_timestamp_now", "", 1);

	format_add("chat_header,speech", "wiadomo¶æ od %1: ", 1);
	format_add("chat_conference_header,speech", "wiadomo¶æ od %1 w konferencji %3: ", 1);
	format_add("chat_line,speech", "%1\n", 1);
	format_add("chat_footer,speech", ".", 1);

	format_add("sent_header", "%b.-- %n%1 %4%b--- -- -%n\n", 1);
	format_add("sent_conference_header", "%b.-- %b[%T%3%b] -- %4%n%1 %b--- -- -%n\n", 1);
	format_add("sent_footer", "%b`----- ---- --- -- -%n\n", 1);
	format_add("sent_line", "%b|%n %|%1%n\n", 1);
	format_add("sent_line_width", "-8", 1);
	format_add("sent_timestamp", "%H:%M", 1);

	format_add("sysmsg_header", "%m.-- %TWiadomo¶æ systemowa%m --- -- -%n\n", 1);
	format_add("sysmsg_line", "%m|%n %|%1%n\n", 1);
	format_add("sysmsg_line_width", "-8", 1);
	format_add("sysmsg_footer", "%m`----- ---- --- -- -%n\n", 1);	

	format_add("sysmsg_header,speech", "wiadomo¶æ systemowa:", 1);
	format_add("sysmsg_line,speech", "%1\n", 1);
	format_add("sysmsg_footer,speech", ".", 1);

	/* potwierdzenia wiadomo¶ci */
	format_add("ack_queued", "%> Wiadomo¶æ do %1 zostanie dostarczona pó¼niej\n", 1);
	format_add("ack_delivered", "%> Wiadomo¶æ do %1 zosta³a dostarczona\n", 1);
	format_add("ack_filtered", "%! %|Wiadomo¶æ do %1 najprawdopodobniej nie zosta³a dostarczona, poniewa¿ dana osoba jest niedostêpna, a serwer twierdzi, ¿e dorêczy³ wiadomo¶æ. Sytuacja taka ma miejsce, gdy wiadomo¶æ zosta³a odrzucona przez filtry serwera (np. zawiera adres strony WWW)\n", 1);
	format_add("message_too_long", "%! Wiadomo¶æ jest zbyt d³uga i zosta³a skrócona\n", 1);

	/* ludzie zmieniaj± stan */
	format_add("status_avail", "%> %1 jest dostêpn%@2\n", 1);
	format_add("status_avail_descr", "%> %1 jest dostêpn%@2: %T%3%n\n", 1);
	format_add("status_busy", "%> %1 jest zajêt%@2\n", 1);
	format_add("status_busy_descr", "%> %1 jest zajêt%@2: %T%3%n\n", 1);
	format_add("status_not_avail", "%> %1 jest niedostêpn%@2\n", 1);
	format_add("status_not_avail_descr", "%> %1 jest niedostêpn%@2: %T%3%n\n", 1);
	format_add("status_invisible", "%> %1 jest niewidoczn%@2\n", 1);
	format_add("status_invisible_descr", "%> %1 jest niewidoczn%@2: %T%3%n\n", 1);

	format_add("auto_away", "%> Automagicznie zmieniono stan na zajêty po %1 nieaktywno¶ci\n", 1);
	format_add("auto_away_descr", "%> Automagicznie zmieniono stan na zajêty po %1 nieaktywno¶ci: %T%2%n%3\n", 1);
	format_add("auto_back", "%> Automagicznie zmieniono stan na dostêpny\n", 1);
	format_add("auto_back_descr", "%> Automagicznie zmieniono stan na dostêpny: %T%2%n%3\n", 1);

	/* po³±czenie z serwerem */
	format_add("connecting", "%> £±czê siê z serwerem...\n", 1);
	format_add("conn_failed", "%! Po³±czenie nie uda³o siê: %1\n", 1);
	format_add("conn_failed_resolving", "Nie znaleziono serwera", 1);
	format_add("conn_failed_connecting", "Nie mo¿na po³±czyæ siê z serwerem", 1);
	format_add("conn_failed_invalid", "Nieprawid³owa odpowied¼ serwera", 1);
	format_add("conn_failed_disconnected", "Serwer zerwa³ po³±czenie", 1);
	format_add("conn_failed_password", "Nieprawid³owe has³o", 1);
	format_add("conn_failed_404", "B³±d serwera HTTP", 1);
	format_add("conn_failed_tls", "B³±d negocjacji TLS", 1);
	format_add("conn_failed_memory", "Brak pamiêci", 1);
	format_add("conn_stopped", "%! Przerwano ³±czenie\n", 1);
	format_add("conn_timeout", "%! Przekroczono limit czasu operacji ³±czenia z serwerem\n", 1);
	format_add("connected", "%> Po³±czono\n", 1);
	format_add("connected_descr", "%> Po³±czono: %T%1%n%2\n", 1);
	format_add("disconnected", "%> Roz³±czono\n", 1);
	format_add("disconnected_descr", "%> Roz³±czono: %T%1%n%2\n", 1);
	format_add("already_connected", "%! Klient jest ju¿ po³±czony. Wpisz %Treconnect%n aby po³±czyæ ponownie\n", 1);
	format_add("during_connect", "%! £±czenie trwa. Wpisz %Tdisconnect%n aby przerwaæ\n", 1);
	format_add("conn_broken", "%! Po³±czenie zosta³o przerwane\n", 1);
	format_add("conn_disconnected", "%! Serwer zerwa³ po³±czenie\n", 1);
	format_add("not_connected", "%! Brak po³±czenia z serwerem. Wpisz %Tconnect%n\n", 1);
	format_add("not_connected_msg_queued", "%! Brak po³±czenia z serwerem. Wiadomo¶æ bêdzie wys³ana po po³±czeniu.%n\n", 1);

	/* obs³uga motywów */
	format_add("theme_loaded", "%> Wczytano motyw %T%1%n\n", 1);
	format_add("theme_default", "%> Ustawiono domy¶lny motyw\n", 1);
	format_add("error_loading_theme", "%! B³±d podczas ³adowania motywu: %1\n", 1);

	/* zmienne, konfiguracja */
	format_add("variable", "%> %1 = %2\n", 1);
	format_add("variable_not_found", "%! Nieznana zmienna: %T%1%n\n", 1);
	format_add("variable_invalid", "%! Nieprawid³owa warto¶æ zmiennej\n", 1);
	format_add("no_config", "%! Niekompletna konfiguracja. Wpisz:\n%!   %Tset uin <numerek-gg>%n\n%!   %Tset password <has³o>%n\n%!   %Tset email <adres-email>%n\n%!   %Tsave%n\n%! Nastêpnie wydaj polecenie:\n%!   %Tconnect%n\n%! Je¶li nie masz swojego numerka, wpisz:\n%!   %Tregister <e-mail> <has³o>%n\n\n%> %|Po po³±czeniu, nowe okna rozmowy bêd± tworzone automatycznie, gdy kto¶ przy¶le wiadomo¶æ. Aby przej¶æ do okna o podanym numerze nale¿y wcisn±æ %TAlt-numer%n lub %TEsc%n, a nastêpnie cyfrê. Aby rozpocz±æ rozmowê, nale¿y u¿yæ polecenia %Tquery%n. Aby dodaæ kogo¶ do listy kontaktów, nale¿y u¿yæ polecenia %Tadd%n. Wszystkie kombinacje klawiszy s± opisane w pliku %TREADME%n, a listê komend wy¶wietla polecenie %Thelp%n.\n\n", 2);
	format_add("no_config,speech", "niekompletna konfiguracja. wpisz set uin, a potem numer gadu-gadu, potem set pas³ord, za tym swoje has³o, a nastêpnie set imejl (bez my¶lnika), a za tym swój adres imejl. wpisz sejf, ¿eby zapisaæ ustawienia. wpisz konekt by siê po³±czyæ. je¶li nie masz swojego numeru gadu-gadu, wpisz red¿ister, a po spacji imejl i has³o. po po³±czeniu, nowe okna rozmowy bêd± tworzone automatycznie, gdy kto¶ przy¶le wiadomo¶æ. aby przej¶æ do okna o podanym numerze, nale¿y wcisn±æ alt-numer lub eskejp, a nastêpnie cyfrê. aby rozpocz±æ rozmowê, nale¿y u¿yæ polecenia k³ery. aby dodaæ kogo¶ do listy kontaktów, nale¿y u¿yæ polecenia edd. wszystkie kombinacje klawiszy s± opisane w pliku ridmi, a listê komend wy¶wietla polecenie help.", 1);
	format_add("error_reading_config", "%! Nie mo¿na odczytaæ pliku konfiguracyjnego: %1\n", 1);
	format_add("config_read_success", "%> Wczytano plik konfiguracyjny %T%1%n\n", 1);
        format_add("config_line_incorrect", "%! Nieprawid³owa linia '%T%1%n', pomijam\n", 1);
	format_add("autosaved", "%> Automatycznie zapisano ustawienia\n", 1);
	
	/* rejestracja nowego numeru */
	format_add("register", "%> Rejestracja poprawna. Wygrany numerek: %T%1%n\n", 1);
	format_add("register_failed", "%! B³±d podczas rejestracji: %1\n", 1);
	format_add("register_pending", "%! Rejestracja w toku\n", 1);
	format_add("register_timeout", "%! Przekroczono limit czasu operacji rejestrowania\n", 1);
	format_add("registered_today", "%! Ju¿ zarejestrowano jeden numer. Nie nadu¿ywaj\n", 1);

	/* kasowanie konta u¿ytkownika z katalogu publiczengo */
	format_add("unregister", "%> Konto %T%1%n wykasowano\n", 1);
	format_add("unregister_timeout", "%! Przekroczono limit czasu operacji usuwania konta\n", 1);
	format_add("unregister_bad_uin", "%! Niepoprawny numer: %T%1%n\n", 1);
	format_add("unregister_failed", "%! B³±d podczas usuwania konta: %1\n", 1);
	
	/* przypomnienie has³a */
	format_add("remind", "%> Has³o zosta³o wys³ane\n", 1);
	format_add("remind_failed", "%! B³±d podczas wysy³ania has³a: %1\n", 1);
	format_add("remind_timeout", "%! Przekroczono limit czasu operacji wys³ania has³a\n", 1);
	
	/* zmiana has³a */
	format_add("passwd", "%> Has³o zosta³o zmienione\n", 1);
	format_add("passwd_failed", "%! B³±d podczas zmiany has³a: %1\n", 1);
	format_add("passwd_timeout", "%! Przekroczono limit czasu operacji zmiany has³a\n", 1);
	
	/* zmiana informacji w katalogu publicznym */
	format_add("change", "%> Informacje w katalogu publicznym zosta³y zmienione\n", 1);
	format_add("change_failed", "%! B³±d podczas zmiany informacji w katalogu publicznym\n", 1);
	
	/* pobieranie tokenu */
	format_add("token", "%> Token\n", 1);
	format_add("token_failed", "%! B³±d podczas pobierania tokenu: %1\n", 1);
	format_add("token_timeout", "%! Przekroczono limit czasu operacji pobierania tokenu\n", 1);

	/* sesemesy */
	format_add("sms_error", "%! B³±d wysy³ania SMS: %1\n", 1);
	format_add("sms_unknown", "%! %1 nie ma podanego numeru komórki\n", 1);
	format_add("sms_sent", "%> SMS do %T%1%n zosta³ wys³any\n", 1);
	format_add("sms_failed", "%! SMS do %T%1%n nie zosta³ wys³any\n", 1);
	format_add("sms_msg", "EKG: msg %1 %# >> %2", 1);
	format_add("sms_chat", "EKG: chat %1 %# >> %2", 1);
	format_add("sms_conf", "EKG: conf %1 %# >> %2", 1);

	/* wyszukiwanie u¿ytkowników */
	format_add("search_failed", "%! Wyst±pi³ b³±d podczas szukania: %1\n", 1);
	format_add("search_not_found", "%! Nie znaleziono\n", 1);
	format_add("search_no_last", "%! Brak wyników ostatniego wyszukiwania\n", 1);
	format_add("search_no_last_nickname", "%! Brak pseudonimu w ostatnim wyszukiwaniu\n", 1);
	format_add("search_stopped", "%> Zatrzymano wyszukiwanie\n", 1);

	/* 1 uin, 2 name, 3 nick, 4 city, 5 born, 6 gender, 7 active */
	format_add("search_results_multi_active", "%Y<>%n", 1);
	format_add("search_results_multi_busy", "%G<>%n", 1);
	format_add("search_results_multi_invisible", "%c<>%n", 1);
	format_add("search_results_multi_inactive", "  ", 1);
	format_add("search_results_multi_unknown", "-", 1);
/*	format_add("search_results_multi_female", "k", 1); */
/*	format_add("search_results_multi_male", "m", 1); */
	format_add("search_results_multi", "%7 %[-7]1 %K|%n %[12]3 %K|%n %[12]2 %K|%n %[4]5 %K|%n %[12]4\n", 1);

	format_add("search_results_single_active", "%Y(dostêpn%@1)%n", 1);
	format_add("search_results_single_busy", "%G(zajêt%@1)%n", 1);
	format_add("search_results_single_inactive", "%r(niedostêpn%@1)%n", 1);
	format_add("search_results_single_invisible", "%c(niewidoczn%@1)%n", 1);
	format_add("search_results_single_unknown", "%T-%n", 1);
/*	format_add("search_results_single_female", "%Mkobieta%n", 1); */
/*	format_add("search_results_single_male", "%Cmê¿czyzna%n", 1); */
	format_add("search_results_single", "%) Pseudonim: %T%3%n\n%) Numerek: %T%1%n %7\n%) Imiê i nazwisko: %T%2%n\n%) Miejscowo¶æ: %T%4%n\n%) Rok urodzenia: %T%5%n\n", 1);

	/* exec */
	format_add("process", "%> %(-5)1 %2\n", 1);
	format_add("no_processes", "%! Nie ma dzia³aj±cych procesów\n", 1);
	format_add("process_exit", "%> Proces %1 (%2) zakoñczy³ dzia³anie z wynikiem %3\n", 1);
	format_add("exec", "%1\n",1);
	format_add("exec_error", "%! B³±d uruchamiania procesu: %1\n", 1);
	format_add("exec_prompt", "$ %1\n", 1);

	/* szczegó³owe informacje o u¿ytkowniku */
	format_add("user_info_header", "%K.--%n %T%1%n/%2 %K--- -- -%n\n", 1);
	format_add("user_info_nickname", "%K| %nPseudonim: %T%1%n\n", 1);
	format_add("user_info_name", "%K| %nImiê i nazwisko: %T%1 %2%n\n", 1);
	format_add("user_info_email", "%K| %nE-mail: %T%1%n\n", 1);
	format_add("user_info_status", "%K| %nStan: %T%1%n\n", 1);
	format_add("user_info_block", "%K| %nBlokowan%@1\n", 1);
	format_add("user_info_offline", "%K| %nNie widzi stanu\n", 1);
	format_add("user_info_not_in_contacts", "%K| %nNie ma nas w swoich kontaktach\n", 1);
	format_add("user_info_firewalled", "%K| %nZnajduje siê za firewall/NAT\n", 1);
	format_add("user_info_ip", "%K| %nAdres: %T%1%n\n", 1);
	format_add("user_info_mobile", "%K| %nTelefon: %T%1%n\n", 1);
	format_add("user_info_groups", "%K| %nGrupy: %T%1%n\n", 1);
	format_add("user_info_never_seen", "%K| %nNie widziano podczas tej sesji\n", 1);
	format_add("user_info_last_seen", "%K| %nOstatnio widziano: %T%1%n\n", 1);
	format_add("user_info_last_seen_time", "%Y-%m-%d %H:%M", 1);
	format_add("user_info_last_descr", "%K| %nOstatni opis: %T%1%n\n", 1);
	format_add("user_info_version", "%K| %nWersja klienta: %T%1%n\n", 1);
	format_add("user_info_voip", "%K| %nObs³uguje rozmowy g³osowe\n", 1);
	format_add("user_info_last_ip","%K| %nOstatni adres IP: %T%1%n\n", 1);
	format_add("user_info_footer", "%K`----- ---- --- -- -%n\n", 1);

	format_add("user_info_avail", "%Ydostêpn%@1%n", 1);
	format_add("user_info_avail_descr", "%Ydostêpn%@1%n (%2)", 1);
	format_add("user_info_busy", "%Gzajêt%@1%n", 1);
	format_add("user_info_busy_descr", "%Gzajêt%@1%n (%2)", 1);
	format_add("user_info_not_avail", "%rniedostêpn%@1%n", 1);
	format_add("user_info_not_avail_descr", "%rniedostêpn%@1%n (%2)", 1);
	format_add("user_info_invisible", "%cniewidoczn%@1%n", 1);
	format_add("user_info_invisible_descr", "%cniewidoczn%@1%n (%2)", 1);
	format_add("user_info_blocked", "%mblokuj±c%@1%n", 1);
	format_add("user_info_unknown", "%Mnieznany%n", 1);

	/* grupy */
	format_add("group_members", "%> %|Grupa %T%1%n: %2\n", 1);
	format_add("group_member_already", "%! %1 nale¿y ju¿ do grupy %T%2%n\n", 1);
	format_add("group_member_not_yet", "%! %1 nie nale¿y do grupy %T%2%n\n", 1);
	format_add("group_empty", "%! Grupa %T%1%n jest pusta\n", 1);

	/* status */
	format_add("show_status_profile", "%) Profil: %T%1%n\n", 1);
	format_add("show_status_uin", "%) Numer: %T%1%n\n", 1);
	format_add("show_status_uin_nick", "%) Numer: %T%1%n (%T%2%n)\n", 1);
	format_add("show_status_status", "%) Aktualny stan: %T%1%2%n\n", 1);
	format_add("show_status_server", "%) Aktualny serwer: %T%1%n:%T%2%n\n", 1);
	format_add("show_status_server_tls", "%) Aktualny serwer: %T%1%n:%T%2%n (po³±czenie szyfrowane)\n", 1);
	format_add("show_status_avail", "%Ydostêpny%n", 1);
	format_add("show_status_avail_descr", "%Ydostêpny%n (%T%1%n%2)", 1);
	format_add("show_status_busy", "%Gzajêty%n", 1);
	format_add("show_status_busy_descr", "%Gzajêty%n (%T%1%n%2)", 1);
	format_add("show_status_invisible", "%cniewidoczny%n", 1);
	format_add("show_status_invisible_descr", "%cniewidoczny%n (%T%1%n%2)", 1);
	format_add("show_status_not_avail", "%rniedostêpny%n", 1);
	format_add("show_status_private_on", ", tylko dla znajomych", 1);
	format_add("show_status_private_off", "", 1);
	format_add("show_status_connected_since", "%) Po³±czony od: %T%1%n\n", 1);
	format_add("show_status_disconnected_since", "%) Roz³±czony od: %T%1%n\n", 1);
	format_add("show_status_last_conn_event", "%Y-%m-%d %H:%M", 1);
	format_add("show_status_last_conn_event_today", "%H:%M", 1);
	format_add("show_status_ekg_started_since", "%) Program dzia³a od: %T%1%n\n", 1);
	format_add("show_status_ekg_started", "%Y-%m-%d %H:%M", 1);
	format_add("show_status_ekg_started_today", "%H:%M", 1);
	format_add("show_status_msg_queue", "%) Ilo¶æ wiadomo¶ci w kolejce do wys³ania: %T%1%n\n", 1);

	/* aliasy */
	format_add("aliases_list_empty", "%! Brak aliasów\n", 1);
	format_add("aliases_list", "%> %T%1%n: %2\n", 1);
	format_add("aliases_list_next", "%> %3  %2\n", 1);
	format_add("aliases_add", "%> Utworzono alias %T%1%n\n", 1);
	format_add("aliases_append", "%> Dodano do aliasu %T%1%n\n", 1);
	format_add("aliases_del", "%) Usuniêto alias %T%1%n\n", 1);
	format_add("aliases_del_all", "%) Usuniêto wszystkie aliasy\n", 1);
	format_add("aliases_exist", "%! Alias %T%1%n ju¿ istnieje\n", 1);
	format_add("aliases_noexist", "%! Alias %T%1%n nie istnieje\n", 1);
	format_add("aliases_command", "%! %T%1%n jest wbudowan± komend±\n", 1);
	format_add("aliases_not_enough_params", "%! Alias %T%1%n wymaga wiêkszej ilo¶ci parametrów\n", 1);

	/* po³±czenia bezpo¶rednie */
	format_add("dcc_attack", "%! %|Program otrzyma³ zbyt wiele ¿±dañ bezpo¶rednich po³±czeñ, ostatnie od %1\n", 1);
	format_add("dcc_limit", "%! %|Przekroczono limit bezpo¶rednich po³±czeñ i dla bezpieczeñstwa zosta³y one wy³±czone. Aby je w³±czyæ ponownie, nale¿y wpisaæ polecenie %Tset dcc 1%n i po³±czyæ siê ponownie. Limit mo¿na zmieniæ za pomoc± zmiennej %Tdcc_limit%n.\n", 1);
	format_add("dcc_create_error", "%! Nie mo¿na w³±czyæ po³±czeñ bezpo¶rednich: %1\n", 1);
	format_add("dcc_error_network", "%! B³±d transmisji z %1\n", 1);
	format_add("dcc_error_refused", "%! Po³±czenie z %1 zosta³o odrzucone\n", 1);
	format_add("dcc_error_unknown", "%! Nieznany b³±d po³±czenia bezpo¶redniego\n", 1);
	format_add("dcc_error_handshake", "%! Nie mo¿na nawi±zaæ po³±czenia z %1\n", 1);
	format_add("dcc_user_aint_dcc", "%! %1 nie ma w³±czonej obs³ugi po³±czeñ bezpo¶rednich\n", 1);
	format_add("dcc_timeout", "%! Przekroczono limit czasu operacji bezpo¶redniego po³±czenia z %1\n", 1);
	format_add("dcc_not_supported", "%! Opcja %T%1%n nie jest jeszcze obs³ugiwana\n", 1);
	format_add("dcc_open_error", "%! Nie mo¿na otworzyæ %T%1%n: %2\n", 1);
	format_add("dcc_show_pending_header", "%> Po³±czenia oczekuj±ce:\n", 1);
	format_add("dcc_show_pending_send", "%) #%1, %2, wysy³anie %T%3%n\n", 1);
	format_add("dcc_show_pending_get", "%) #%1, %2, odbiór %T%3%n\n", 1);
	format_add("dcc_show_pending_voice", "%) #%1, %2, rozmowa\n", 1);
	format_add("dcc_show_active_header", "%> Po³±czenia aktywne:\n", 1);
	format_add("dcc_show_active_send", "%) #%1, %2, wysy³anie %T%3%n, %T%4b%n z %T%5b%n (%6%%)\n", 1);
	format_add("dcc_show_active_get", "%) #%1, %2, odbiór %T%3%n, %T%4b%n z %T%5b%n (%6%%)\n", 1);
	format_add("dcc_show_active_voice", "%) #%1, %2, rozmowa\n", 1);
	format_add("dcc_show_empty", "%! Brak bezpo¶rednich po³±czeñ\n", 1);
	format_add("dcc_receiving_already", "%! Plik %T%1%n od u¿ytkownika %2 jest ju¿ pobierany\n", 1);

	format_add("dcc_done_get", "%> Zakoñczono pobieranie pliku %T%2%n od %1\n", 1);
	format_add("dcc_done_send", "%> Zakoñczono wysy³anie pliku %T%2%n do %1\n", 1);
	format_add("dcc_close", "%) Zamkniêto po³±czenie z %1\n", 1);

	format_add("dcc_voice_offer", "%) %1 chce rozmawiaæ\n%) Wpisz %Tdcc voice #%2%n, by rozpocz±æ rozmowê, lub %Tdcc close #%2%n, by anulowaæ\n", 1);
	format_add("dcc_voice_running", "%! Mo¿na prowadziæ tylko jedn± rozmowê g³osow± na raz\n", 1);
	format_add("dcc_voice_unsupported", "%! Nie wkompilowano obs³ugi rozmów g³osowych. Przeczytaj %Tdocs/voip.txt%n\n", 1);
	format_add("dcc_get_offer", "%) %1 przesy³a plik %T%2%n o rozmiarze %T%3b%n\n%) Wpisz %Tdcc get #%4%n, by go odebraæ, lub %Tdcc close #%4%n, by anulowaæ\n", 1);
	format_add("dcc_get_offer_resume", "%) Plik istnieje ju¿ na dysku, wiêc mo¿na wznowiæ pobieranie poleceniem %Tdcc resume #%4%n\n", 1);
	format_add("dcc_get_getting", "%) Rozpoczêto pobieranie pliku %T%2%n od %1\n", 1);
	format_add("dcc_get_cant_create", "%! Nie mo¿na utworzyæ pliku %T%1%n\n", 1);
	format_add("dcc_not_found", "%! Nie znaleziono po³±czenia %T%1%n\n", 1);
	format_add("dcc_invalid_ip", "%! Nieprawid³owy adres IP\n", 1);
	format_add("dcc_user_not_avail", "%! %1 musi byæ aktywn%@1, by móc nawi±zaæ po³±czenie\n", 1);

	/* query */
	format_add("query_started", "%) Rozpoczêto rozmowê z %T%1%n\n", 1);
	format_add("query_started_window", "%) Wci¶nij %TAlt-G%n by ignorowaæ, %TAlt-K%n by zamkn±æ okno\n", 1);
	format_add("query_finished", "%) Zakoñczono rozmowê z %T%1%n\n", 1);
	format_add("query_exist", "%! Rozmowa z %T%1%n jest ju¿ prowadzona w okienku nr %T%2%n\n", 1);

	/* zdarzenia */
        format_add("events_list_empty", "%! Brak zdarzeñ\n", 1);
        format_add("events_list", "%> %4, on %1 %2 %3\n", 1);
	format_add("events_list_inactive", "%> %4, on %1 %2 %3 %K(nieaktywne)%n\n", 1);
        format_add("events_add", "%> Dodano zdarzenie %T%1%n\n", 1);
        format_add("events_del", "%) Usuniêto zdarzenie %T%1%n\n", 1);
        format_add("events_del_all", "%) Usuniêto wszystkie zdarzenia\n", 1);
        format_add("events_exist", "%! Zdarzenie %T%1%n istnieje dla %2\n", 1);
        format_add("events_del_noexist", "%! Zdarzenie %T%1%n nie istnieje\n", 1);
        format_add("events_seq_not_found", "%! Sekwencja %T%1%n nie znaleziona\n", 1);
	format_add("events_seq_incorrect", "%! Nieprawid³owa sekwencja\n", 1);

	/* lista kontaktów z serwera */
	format_add("userlist_put_ok", "%> Listê kontaktów zachowano na serwerze\n", 1);
	format_add("userlist_put_error", "%! B³±d podczas wysy³ania listy kontaktów\n", 1);
	format_add("userlist_get_ok", "%> Listê kontaktów wczytano z serwera\n", 1);
	format_add("userlist_get_error", "%! B³±d podczas pobierania listy kontaktów\n", 1);
	format_add("userlist_clear_ok", "%) Usuniêto listê kontaktów z serwera\n", 1);
	format_add("userlist_clear_error", "%! B³±d podczas usuwania listy kontaktów\n", 1);
	format_add("userlist_config_put_ok", "%> Listê kontaktów i konfiguracjê zachowano na serwerze\n", 1);
	format_add("userlist_config_put_error", "%! B³±d podczas wysy³ania listy kontaktów i konfiguracji\n", 1);
	format_add("userlist_config_get_ok", "%> Listê kontaktów i konfiguracjê wczytano z serwera\n", 1);
	format_add("userlist_config_get_error", "%! B³±d podczas pobierania listy kontaktów i konfiguracji\n", 1);
	format_add("userlist_config_clear_ok", "%) Usuniêto listê kontaktów i konfiguracjê z serwera\n", 1);
	format_add("userlist_config_clear_error", "%! B³±d podczas usuwania listy kontaktów i konfiguracji\n", 1);

	/* szybka lista kontaktów pod F2 */
	format_add("quick_list", "%)%1\n", 1);
	format_add("quick_list,speech", "lista kontaktów: ", 1);
	format_add("quick_list_avail", " %Y%1%n", 1);
	format_add("quick_list_avail,speech", "%1 jest dostêpny, ", 1);
	format_add("quick_list_busy", " %G%1%n", 1);
	format_add("quick_list_busy,speech", "%1 jest zajêty, ", 1);
	format_add("quick_list_invisible", " %c%1%n", 1);

	/* window */
	format_add("window_add", "%) Utworzono nowe okno\n", 1);
	format_add("window_noexist", "%! Wybrane okno nie istnieje\n", 1);
	format_add("window_no_windows", "%! Nie mo¿na zamkn±æ ostatniego okna\n", 1);
	format_add("window_del", "%) Zamkniêto okno\n", 1);
	format_add("windows_max", "%! Wyczerpano limit ilo¶ci okien\n", 1);
	format_add("window_list_query", "%) %1: rozmowa z %T%2%n\n", 1);
	format_add("window_list_nothing", "%) %1: brak rozmowy\n", 1);
	format_add("window_list_floating", "%) %1: p³ywaj±ce %4x%5 w %2,%3 %T%6%n\n", 1);
	format_add("window_id_query_started", "%) Rozmowa z %T%2%n rozpoczêta w oknie %T%1%n\n", 1);
	format_add("window_kill_status", "%! Nie mo¿na zamkn±æ okna stanu\n", 1);

	/* bind */
	format_add("bind_seq_incorrect", "%! Sekwencja %T%1%n jest nieprawid³owa\n", 1); 
	format_add("bind_seq_add", "%> Dodano sekwencjê %T%1%n\n", 1);
	format_add("bind_seq_remove", "%) Usuniêto sekwencjê %T%1%n\n", 1);	
	format_add("bind_seq_list", "%> %1: %T%2%n\n", 1);
	format_add("bind_seq_exist", "%! Sekwencja %T%1%n ma ju¿ przypisan± akcjê\n", 1);
	format_add("bind_seq_list_empty", "%! Brak przypisanych akcji\n", 1);

	/* at */
	format_add("at_list", "%> %1, %2, %3 %K(%4)%n %5\n", 1);
	format_add("at_added", "%> Utworzono plan %T%1%n\n", 1);
	format_add("at_deleted", "%) Usuniêto plan %T%1%n\n", 1);
	format_add("at_deleted_all", "%) Usuniêto plany u¿ytkownika\n", 1);
	format_add("at_exist", "%! Plan %T%1%n ju¿ istnieje\n", 1);
	format_add("at_noexist", "%! Plan %T%1%n nie istnieje\n", 1);
	format_add("at_empty", "%! Brak planów\n", 1);
	format_add("at_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("at_back_to_past", "%! Gdyby mo¿na by³o cofn±æ czas...\n", 1);

	/* timer */
	format_add("timer_list", "%> %1, %2s, %3 %K(%4)%n %T%5%n\n", 1);
	format_add("timer_added", "%> Utworzono timer %T%1%n\n", 1);
	format_add("timer_deleted", "%) Usuniêto timer %T%1%n\n", 1);
	format_add("timer_deleted_all", "%) Usuniêto timery u¿ytkownika\n", 1);
	format_add("timer_exist", "%! Timer %T%1%n ju¿ istnieje\n", 1);
	format_add("timer_noexist", "%! Timer %T%1%n nie istnieje\n", 1);
	format_add("timer_empty", "%! Brak timerów\n", 1);

	/* last */
	format_add("last_list_in", "%) %Y <<%n [%1] %2 %3\n", 1);
	format_add("last_list_out", "%) %G >>%n [%1] %2 %3\n", 1);
	format_add("last_list_empty", "%! Nie zalogowano wiadomo¶ci\n", 1);
	format_add("last_list_empty_nick", "%! Nie zalogowano wiadomo¶ci dla %T%1%n\n", 1);
	format_add("last_list_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("last_list_timestamp_today", "%H:%M", 1);
	format_add("last_clear", "%) Usuniêto wiadomo¶ci\n", 1);
	format_add("last_clear_one", "%) Usuniêto wiadomo¶æ\n", 1);

	/* queue */
	format_add("queue_list_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("queue_list_message", "%) %G >>%n [%1] %2 %3\n", 1);
	format_add("queue_clear","%) Usuniêto wiadomo¶ci z kolejki\n", 1);
	format_add("queue_clear_uin","%) Usuniêto wiadomo¶ci z kolejki dla %T%1%n\n", 1);
	format_add("queue_wrong_use", "%! Komenda dzia³a tylko w czasie braku po³±czenia z serwerem\n", 1);
	format_add("queue_empty", "%! Kolejka wiadomo¶ci jest pusta\n", 1);
	format_add("queue_empty_uin", "%! Brak wiadomo¶ci w kolejce dla %T%1%n\n", 1);
	format_add("queue_flush", "%> Wys³ano zaleg³e wiadomo¶ci z kolejki\n", 1);

	/* conference */
	format_add("conferences_list_empty", "%! Brak konferencji\n", 1);
	format_add("conferences_list", "%> %T%1%n: %2\n", 1);
	format_add("conferences_list_ignored", "%> %T%1%n: %2 (%yingorowana%n)\n", 1);
	format_add("conferences_add", "%> Utworzono konferencjê %T%1%n\n", 1);
	format_add("conferences_not_added", "%! Nie utworzono konferencji %T%1%n\n", 1);
	format_add("conferences_del", "%) Usuniêto konferencjê %T%1%n\n", 1);
	format_add("conferences_del_all", "%) Usuniêto wszystkie konferencje\n", 1);
	format_add("conferences_exist", "%! Konferencja %T%1%n ju¿ istnieje\n", 1);
	format_add("conferences_noexist", "%! Konferencja %T%1%n nie istnieje\n", 1);
	format_add("conferences_name_error", "%! Nazwa konferencji powinna zaczynaæ siê od %T#%n\n", 1);
	format_add("conferences_rename", "%> Nazwa konferencji zmieniona: %T%1%n --> %T%2%n\n", 1);
	format_add("conferences_ignore", "%> Konferencja %T%1%n bêdzie ignorowana\n", 1);
	format_add("conferences_unignore", "%> Konferencja %T%1%n nie bêdzie ignorowana\n", 1);
	format_add("conferences_joined", "%> Do³±czono %1 do konferencji %T%2%n\n", 1);
	format_add("conferences_already_joined", "%> %1 uczestniczy ju¿ w konferencji %T%2%n\n", 1);
	
	/* wspólne dla us³ug http */
	format_add("http_failed_resolving", "Nie znaleziono serwera", 1);
	format_add("http_failed_connecting", "Nie mo¿na po³±czyæ siê z serwerem", 1);
	format_add("http_failed_reading", "Serwer zerwa³ po³±czenie", 1);
	format_add("http_failed_writing", "Serwer zerwa³ po³±czenie", 1);
	format_add("http_failed_memory", "Brak pamiêci", 1);

#ifdef HAVE_OPENSSL
	/* szyfrowanie */
	format_add("key_generating", "%> Czekaj, generujê klucze...\n", 1);
	format_add("key_generating_success", "%> Wygenerowano i zapisano klucze\n", 1);
	format_add("key_generating_error", "%! Wyst±pi³ b³±d podczas generowania kluczy: %1\n", 1);
	format_add("key_private_exist", "%! Posiadasz ju¿ swoj± parê kluczy\n", 1);
	format_add("key_public_deleted", "%) Klucz publiczny %1 zosta³ usuniêty\n", 1);
	format_add("key_public_not_found", "%! Nie znaleziono klucza publicznego %1\n", 1);
	format_add("key_public_noexist", "%! Brak kluczy publicznych\n", 1);
	format_add("key_public_received", "%> Otrzymano klucz publiczny od %1\n", 1);
	format_add("key_public_write_failed", "%! B³±d podczas zapisu klucza publicznego: %1\n", 1);
	format_add("key_send_success", "%> Wys³ano klucz publiczny do %1\n", 1);
	format_add("key_send_error", "%! B³±d podczas wysy³ania klucza publicznego\n", 1);
	format_add("key_list", "%> %1 (%3)\n%) %2\n", 1);
	format_add("key_list_timestamp", "%Y-%m-%d %H:%M", 1);
#endif

#ifdef WITH_PYTHON
	/* python */
	format_add("python_list", "%> %1\n", 1);
	format_add("python_list_empty", "%! Brak za³adowanych skryptów\n", 1);
	format_add("python_removed", "%) Skrypt zosta³ usuniêty\n", 1);
	format_add("python_need_name", "%! Nie podano nazwy skryptu\n", 1);
	format_add("python_not_found", "%! Nie znaleziono skryptu %T%1%n\n", 1);
	format_add("python_wrong_location", "%! Skrypt nale¿y umie¶ciæ w katalogu %T%1%n\n", 1);
#endif
}
