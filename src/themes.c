/* $Id$ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdarg.h>
#include <limits.h>
#include "stuff.h"
#include "dynstuff.h"
#include "themes.h"

char *prompt_cache = NULL, *prompt2_cache = NULL, *error_cache = NULL, *timestamp_cache = NULL;
char *readline_prompt = NULL, *readline_prompt_away = NULL;

struct list *formats = NULL;

/*
 * find_format()
 *
 * odnajduje warto¶æ danego formatu. je¶li nie znajdzie, zwraca pusty ci±g,
 * ¿eby nie musieæ uwa¿aæ na ¿adne null-references.
 *
 *  - name.
 */
char *find_format(char *name)
{
	struct list *l;
	
	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		if (!strcasecmp(f->name, name))
			return f->value;
	}
	
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
char *va_format_string(char *format, va_list ap)
{
	struct string *buf;
	char *p, *args[9];
	int i;
	// void **args = (void**) ap;

	for (i = 0; i < 9; i++)
		args[i] = NULL;

	for (i = 0; i < 9; i++) {
		if (!(args[i] = va_arg(ap, char*)))
			break;
	}

	if (!prompt_cache) {
		prompt_cache = "dummy";
		prompt_cache = format_string(find_format("prompt"));
	}
	if (!prompt2_cache) {
		prompt2_cache = "dummy";
		prompt2_cache = format_string(find_format("prompt2"));
	}
	if (!error_cache) {
		error_cache = "dummy";
		error_cache = format_string(find_format("error"));
	}
	if (!timestamp_cache)
		timestamp_cache = find_format("timestamp");

	if (!(buf = string_init("")))
		return NULL;

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
			if (*p == '#')
				string_append(buf, timestamp(timestamp_cache));
			if (display_color) {
				if (*p == 'k')
					string_append(buf, "\033[0;30m");
				if (*p == 'K')
					string_append(buf, "\033[1;30m");
				if (*p == 'l')
					string_append(buf, "\033[40m");
				if (*p == 'r')
					string_append(buf, "\033[0;31m");
				if (*p == 'R')
					string_append(buf, "\033[1;31m");
				if (*p == 's')
					string_append(buf, "\033[41m");
				if (*p == 'g')
					string_append(buf, "\033[0;32m");
				if (*p == 'G')
					string_append(buf, "\033[1;32m");
				if (*p == 'h')
					string_append(buf, "\033[42m");
				if (*p == 'y')
					string_append(buf, "\033[0;33m");
				if (*p == 'Y')
					string_append(buf, "\033[1;33m");
				if (*p == 'z')
					string_append(buf, "\033[43m");
				if (*p == 'b')
					string_append(buf, "\033[0;34m");
				if (*p == 'B')
					string_append(buf, "\033[1;34m");
				if (*p == 'e')
					string_append(buf, "\033[44m");
				if (*p == 'm' || *p == 'p')
					string_append(buf, "\033[0;35m");
				if (*p == 'M' || *p == 'P')
					string_append(buf, "\033[1;35m");
				if (*p == 'q')
					string_append(buf, "\033[45m");
				if (*p == 'c')
					string_append(buf, "\033[0;36m");
				if (*p == 'C')
					string_append(buf, "\033[1;36m");
				if (*p == 'd')
					string_append(buf, "\033[46m");
				if (*p == 'w')
					string_append(buf, "\033[0;37m");
				if (*p == 'W')
					string_append(buf, "\033[1;37m");
				if (*p == 'x')
					string_append(buf, "\033[47m");
				if (*p == 'n')
					string_append(buf, "\033[0m");
			}

			fill_before = 0;
			fill_after = 0;
			fill_length = 0;
			fill_char = ' ';
			fill_soft = 1;

			if (*p == '[' || *p == '(') {
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

				fill_length = strtol(p, &p, 0);
				if (fill_length > 0)
					fill_after = 1;
				else {
					fill_length = -fill_length;
					fill_before = 1;
				}
				p++;
			}

			if (*p >= '1' && *p <= '9') {
				char *str = (char*) args[*p - '1'];
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

	return string_free(buf, 0);
}

/*
 * format_string()
 *
 * j.w. tyle ¿e nie potrzeba dawaæ mu va_list, a wystarcz± zwyk³e parametry.
 *
 *  - format... - j.w.,
 */
char *format_string(char *format, ...)
{
	va_list ap;
	char *tmp;
	
	va_start(ap, format);
	tmp = va_format_string(format, ap);
	va_end(ap);

	return tmp;
}

/*
 * my_printf()
 *
 * drukuje na stdout tekst, bior±c pod uwagê nazwê, nie warto¶æ formatu.
 * parametry takie jak zdefiniowano. pierwszy to %1, drugi to %2.
 */
void my_printf(char *theme, ...)
{
	va_list ap;
	char *tmp;
	
	va_start(ap, theme);
	tmp = va_format_string(find_format(theme), ap);
	my_puts("%s", (tmp) ? tmp : "");
	free(tmp);
	va_end(ap);
}

/*
 * reset_theme_cache()
 *
 * usuwa cache'owane prompty. przydaje siê przy zmianie theme'u.
 */
inline void reset_theme_cache()
{
	free(prompt_cache);
	free(prompt2_cache);
	free(error_cache);
	
	prompt_cache = prompt2_cache = error_cache = NULL;
	readline_prompt = readline_prompt_away = NULL;
}

/*
 * add_format()
 *
 * dodaje dan± formatkê do listy.
 *
 *  - name - nazwa,
 *  - value - warto¶æ,
 *  - replace - je¶li znajdzie, to zostawia (=0) lub zamienia (=1).
 */
int add_format(char *name, char *value, int replace)
{
	struct format f;
	struct list *l;

	for (l = formats; l; l = l->next) {
		struct format *g = l->data;

		if (!strcasecmp(name, g->name)) {
			if (replace) {
				free(g->value);
				g->value = strdup(value);
			}

			return 0;
		}
	}

	f.name = strdup(name);
	f.value = strdup(value);
	list_add(&formats, &f, sizeof(f));
	
	return 0;
}

/*
 * del_format()
 *
 * usuwa formatkê o danej nazwie.
 *
 *  - name.
 */
int del_format(char *name)
{
	struct list *l;

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		if (!strcasecmp(f->name, name)) {
			free(f->value);
			free(f->name);
			list_remove(&formats, f, 1);
		
			return 0;
		}
	}

        return -1;
}

/*
 * read_theme()
 *
 * wczytuje opis wygl±du z podanego pliku. 
 *
 *  - filename.
 */
int read_theme(char *filename, int replace)
{
        char *buf, buf2[PATH_MAX];
        FILE *f;

        if (!filename) {
                if (!(filename = prepare_path("default.theme")))
                        return -1;
        } else {
		if (!strchr(filename, '/'))
			filename = prepare_path(filename);
	}

        if (!(f = fopen(filename, "r"))) {
		snprintf(buf2, sizeof(buf2) - 1, "%s.theme", filename);
		if (!(f = fopen(buf2, "r")))
	                return -1;
	}

        while ((buf = read_file(f))) {
                char *value, *p;

                if (buf[0] == '#') {
			free(buf);
                        continue;
		}

                if (!(value = strchr(buf, ' '))) {
			free(buf);
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
			del_format(buf + 1);
		else
			add_format(buf, value, replace);

		free(buf);
        }

        fclose(f);

        return 0;
}

/*
 * init_theme()
 *
 * ustawia domy¶lne warto¶ci formatek.
 */
void init_theme()
{
	add_format("prompt", "%g-%G>%g-%n", 1);
	add_format("prompt2", "%c-%C>%c-%n", 1);
	add_format("error", "%r-%R>%r-%n", 1);
	add_format("timestamp", "%H:%M", 1);
	add_format("readline_prompt", "% ", 1);
	add_format("readline_prompt_away", "/ ", 1);

	add_format("known_user", "%W%1%n/%2", 1);
	add_format("unknown_user", "%W%1%n", 1);
	add_format("user_not_given", "Nie podany u¿ytkownik!\n", 1);
	
	add_format("none", "%1\n", 1);
	add_format("not_enough_params", "%! Za ma³o parametrów\n", 1);
	add_format("invalid_uin", "%! Nieprawid³owy numer u¿ytkownika\n", 1);
	add_format("user_added", "%> U¿ytkownik %1 zosta³ dopisany do listy kontaktów\n", 1);
	add_format("error_adding", "%! Wyst±pi³ b³±d podczas dopisywania u¿ytkownika\n", 1);
	add_format("away", "%> Zmieniono stan na zajêty %c(%C%#%c)%n\n", 1);
	add_format("back", "%> Zmieniono stan na dostêpny %c(%C%#%c)%n\n", 1);
	add_format("invisible", "%> Zmieniono stan na niewidoczny %c(%C%#%c)%n\n", 1);
	add_format("user_not_found", "%! Nie znaleziono u¿ytkownika %W%1%n\n", 1);
	add_format("user_deleted", "%> U¿ytkownik %1 zosta³ usuniêty z listy kontaktów\n", 1);
	add_format("error_deleting", "%! Wyst±pi³ b³±d podczas usuwania u¿ytkownika\n", 1);
	add_format("user_exists", "%! U¿ytkownik %W%1%n ju¿ istnieje w li¶cie kontaktów\n", 1);
	add_format("help", "%> %1%2 - %3\n", 1);
	add_format("help_more", "%) %1\n", 1);
	add_format("generic", "%> %1\n", 1);
	add_format("ignored_list", "%> %1\n", 1);
	add_format("ignored_list_empty", "%! Lista ignorowanych u¿ytkowników jest pusta\n", 1);
	add_format("ignored_added", "%> U¿ytkownika %W%1%n dodano do listy ignorowanych\n", 1);
	add_format("error_adding_ignored", "%! Dodanie do listy ignorowanych nie powiod³o siê\n", 1);
	add_format("ignored_deleted", "%> U¿ytkownika %W%1%n usuniêto z listy ignorowanych\n", 1);
	add_format("error_not_ignored", "%! U¿ytkownik %W%1%n nie jest ignorowany\n", 1);
	add_format("list_empty", "%! Lista kontaktów jest pusta\n", 1);
	add_format("list_avail", "%> %1 %Y(dostêpny)%n %b%2:%3%n\n", 1);
	add_format("list_busy", "%> %1 %G(zajêty)%n %b%2:%3%n\n", 1);
	add_format("list_not_avail", "%> %1 %r(niedostêpny)%n\n", 1);
	add_format("list_unknown", "%> %1\n", 1);
	add_format("saved", "%> Zapisano ustawiania\n", 1);
	add_format("error_saving", "%! Podczas zapisu ustawieñ wyst±pi³ b³±d\n", 1);
	add_format("quit", "%> Papa\n", 1);
	add_format("message_header", "%) Wiadomo¶æ od %1 %c(%C%#%c)%n\n", 1);
	add_format("message_footer", "%n", 1);
	add_format("chat_header", "%) Rozmowa z %1 %c(%C%#%c)%n\n", 1);
	add_format("chat_footer", "%n", 1);
	add_format("ack_queued", "%> Wiadomo¶æ do %1 zostanie dostarczona pó¼niej %c(%C%#%c)%n\n", 1);
	add_format("ack_delivered", "%> Wiadomo¶æ do %1 zosta³a dostarczona %c(%C%#%c)%n\n", 1);
	add_format("status_avail", "%> %1 jest dostêpny %c(%C%#%c)%n\n", 1);
	add_format("status_busy", "%> %1 jest zajêty %c(%C%#%c)%n\n", 1);
	add_format("status_not_avail", "%> %1 jest niedostêpny %c(%C%#%c)%n\n", 1);
	add_format("conn_broken", "%! Serwer zerwa³ po³±czenie: %1 %c(%C%#%c)%n\n", 1);
	add_format("auto_away", "%> Automagicznie zmieniono stan na zajêty po %1 nieaktywno¶ci %c(%C%#%c)%n\n", 1);
	add_format("welcome", "%> EKG-%1 (Eksperymentalny Klient Gadu-gadu)\n%> (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>\n%> Program jest rozprowadzany na zasadach licencji GPL\n\n", 1);
	add_format("error_reading_config", "%! Nie mo¿na odczytaæ pliku konfiguracyjnego: %1\n", 1);
	add_format("offline_mode", "%! Tryb off-line\n", 1);
	add_format("connecting", "%> £±czê siê z serwerem...\n", 1);
	add_format("conn_failed", "%! Po³±czenie nie uda³o siê: %1\n", 1);
	add_format("connected", "%> Po³±czono %c(%C%#%c)%n\n", 1);
	add_format("disconnected", "%> Roz³±czono %c(%C%#%c)%n\n", 1);
	add_format("theme_loaded", "%> Wczytano opis wygl±du o nazwie %W%1%n\n", 1);
	add_format("theme_default", "%> Ustawiono domy¶lny opis wygl±du\n", 1);
	add_format("error_loading_theme", "%! Wyst±pi³ b³±d podczas ³adowania opisu wygl±du: %1\n", 1);
	add_format("not_connected", "%! Brak po³±czenia z serwerem\n", 1);
	add_format("variable", "%> %1 = %2\n", 1);
	add_format("variable_not_found", "%! Nieznana zmienna: %1\n", 1);
	add_format("variable_invalid", "%! Nieprawid³owa warto¶æ zmiennej\n", 1);
	add_format("not_implemented", "%! Tej funkcji jeszcze nie ma\n", 1);
	add_format("no_config", "%! Niekompletna konfiguracja. Wpisz:\n%!   %Wset uin <numerek-gg>%n\n%!   %Wset password <has³o>%n\n%!   %Wsave%n\n%! Nastêpnie wydaj polecenie:\n%!   %Wconnect%n\n\n", 1);

	add_format("sms_msg", "EKG: msg %1 %# >> %2", 1);
	add_format("sms_chat", "EKG: chat %1 %# >> %2", 1);
	add_format("sms_error", "%! B³±d wysy³ania SMS\n", 1);
	add_format("sms_unknown", "%! U¿ytkownik %1 nie ma podanego numeru komórki\n", 1);
	add_format("sms_sent", "%> SMS do %W%1%n zosta³ wys³any\n", 1);
	add_format("sms_failed", "%! SMS do %W%1%n nie zosta³ wys³any\n", 1);

	add_format("already_connected", "%! Klient jest ju¿ po³±czony\n", 1);
	add_format("during_connect", "%! £±czenie trwa\n", 1);
	add_format("search_failed", "%! Wyst±pi³ b³±d podczas szukania: %1\n", 1);
	add_format("search_not_found", "%! Nie znaleziono\n", 1);
	add_format("unknown_command", "%! Nieznane polecenie: %W%1%n\n", 1);
	add_format("already_searching", "%! Szukanie trwa. Poczekaj, albo u¿yj %Wfind -stop%n\n", 1);

	/* 1 uin, 2 name, 3 nick, 4 city, 5 born, 6 gender, 7 active */

	add_format("search_results_multi_active", "%G!%n", 1);
	add_format("search_results_multi_inactive", " ", 1);
	add_format("search_results_multi_unknown", "-", 1);
	add_format("search_results_multi_female", "k", 1);
	add_format("search_results_multi_male", "m", 1);
	add_format("search_results_multi", "%7 %[-10]1 %K/%n %[12]3 %K/%n %6 %K/%n %[20]2 %K/%n %[4]5 %K/%n %[16]4\n", 1);

	add_format("search_results_single_active", "%G(aktywny)%n", 1);
	add_format("search_results_single_inactive", "%r(nieaktywny)%n", 1);
	add_format("search_results_single_unknown", "%W-%n", 1);
	add_format("search_results_single_female", "%Mkobieta%n", 1);
	add_format("search_results_single_male", "%Cmê¿czyzna%n", 1);
	add_format("search_results_single", "%) Nick: %W%3%n\n%) Numerek: %W%1%n\n%) Imiê i nazwisko: %W%2%n\n%) Miejscowo¶æ: %W%4%n\n%) Rok urodzenia: %W%5%n\n%) P³eæ: %6\n", 1);

	add_format("process", "%> %(-5)1 %2\n", 1);
	add_format("no_processes", "%! Nie ma dzia³aj±cych procesów\n", 1);
	add_format("process_exit", "%> Proces %1 (%2) zakoñczy³ dzia³anie z wynikiem %3\n", 1);

	add_format("modify_done", "%> Zmieniono wpis w li¶cie kontaktów\n", 1);
	add_format("user_info", "%) Imiê i nazwisko: %W%1 %2%n\n%) Pseudonim: %W%3%n\n%) Alias: %W%4%n\n%) Numer telefonu: %W%5%n\n%) Grupa: %W%6%n\n", 1);

	add_format("config_changed", "%> Zapisaæ now± konfiguracjê? (tak/nie) ", 1);
	add_format("config_unknown", "%! Zmiana tego ustawienia mo¿e nie odnie¶æ zamierzonego efektu\n", 1);

	add_format("private_mode_is_on", "%> Tryb ,,tylko dla przyjació³'' jest w³±czony\n", 1);
	add_format("private_mode_is_off", "%> Tryb ,,tylko dla przyjació³'' jest wy³±czony\n", 1);
	add_format("private_mode_on", "%> W³±czono tryb ,,tylko dla przyjació³''\n", 1);
	add_format("private_mode_off", "%> Wy³±czono tryb ,,tylko dla przyjació³''\n", 1);
	add_format("private_mode_invalid", "%! Nieprawid³owa warto¶æ\n", 1);
	
	add_format("show_status", "%) Aktywny stan: %1%2\n", 1);
	add_format("show_status_avail", "%Ydostêpny%n", 1);
	add_format("show_status_busy", "%Gzajêty%n", 1);
	add_format("show_status_invisible", "%bniewidoczny%n", 1);
	add_format("show_status_not_avail", "%rniedostêpny%n", 1);
	add_format("show_status_private_on", ", tylko dla znajomych", 1);
	add_format("show_status_private_off", "", 1);

	add_format("aliases_invalid", "%! Nieprawid³owy parametr\n", 1);
	add_format("aliases_list_empty", "%! Brak aliasów\n", 1);
	add_format("aliases_list", "%> %W%1 %G-%Y %2%n\n", 1);
	add_format("aliases_add", "%> Dodano alias %W%1 %n-> %2\n", 1);
	add_format("aliases_del", "%) Usuniêto alias %W%1%n\n", 1);
	add_format("aliases_exist", "%! Alias %W%1%n ju¿ istnieje\n", 1);
	add_format("aliases_noexist", "%! Alias %W%1%n nie istnieje\n", 1);
};

