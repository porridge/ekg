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

#include <stdlib.h>
#include <errno.h>
#ifndef _AIX
#  include <string.h>
#endif
#include "dynstuff.h"

/*
 * list_add_sorted()
 *
 * dodaje do listy dany element. przy okazji mo¿e te¿ skopiowaæ zawarto¶æ.
 * je¶li poda siê jako ostatni parametr funkcjê porównuj±c± zawarto¶æ
 * elementów, mo¿e posortowaæ od razu.
 *
 *  - list - wska¼nik do listy,
 *  - data - wska¼nik do elementu,
 *  - alloc_size - rozmiar elementu, je¶li chcemy go skopiowaæ.
 *
 * zwraca wska¼nik zaalokowanego elementu lub NULL w przpadku b³êdu.
 */
void *list_add_sorted(struct list **list, void *data, int alloc_size, int (*comparision)(void *, void *))
{
	struct list *new, *tmp;

	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	if (!(new = malloc(sizeof(struct list))))
		return NULL;

	new->data = data;
	new->next = NULL;

	if (alloc_size) {
		if (!(new->data = malloc(alloc_size))) { 
			free(new);
			return NULL;
		}
		memcpy(new->data, data, alloc_size);
	}

	if (!(tmp = *list)) {
		*list = new;
	} else {
		if (!comparision) {
			while (tmp->next)
				tmp = tmp->next;
			tmp->next = new;
		} else {
			struct list *prev = NULL;
			
			while (comparision(new->data, tmp->data) > 0) {
				prev = tmp;
				tmp = tmp->next;
				if (!tmp)
					break;
			}
			
			if (!prev) {
				tmp = *list;
				*list = new;
				new->next = tmp;
			} else {
				prev->next = new;
				new->next = tmp;
			}
		}
	}

	return new->data;
}

/*
 * list_add()
 *
 * wrapper do list_add_sorted(), który zachowuje poprzedni± sk³adniê.
 */
void *list_add(struct list **list, void *data, int alloc_size)
{
	return list_add_sorted(list, data, alloc_size, NULL);
}

/*
 * list_remove()
 *
 * usuwa z listy wpis z podanym elementem.
 *
 *  - list - wska¼nik do listy,
 *  - data - element,
 *  - free_data - zwolniæ pamiêæ po elemencie.
 */
int list_remove(struct list **list, void *data, int free_data)
{
	struct list *tmp, *last = NULL;

	if (!list) {
		errno = EFAULT;
		return -1;
	}

	tmp = *list;
	if (tmp->data == data) {
		*list = tmp->next;
	} else {
		for (; tmp && tmp->data != data; tmp = tmp->next)
			last = tmp;
		if (!tmp) {
			errno = ENOENT;
			return -1;
		}
		last->next = tmp->next;
	}

	if (free_data)
		free(tmp->data);
	free(tmp);

	return 0;
}

/*
 * list_count()
 *
 * zwraca ilo¶æ elementów w danej li¶cie.
 *
 *  - list - lista.
 */
int list_count(struct list *list)
{
	int count = 0;

	for (; list; list = list->next)
		count++;

	return count;
}

/*
 * string_append_c()
 *
 * dodaje do danego ci±gu jeden znak, alokuj±c przy tym odpowiedni± ilo¶æ
 * pamiêci.
 *
 *  - s - wska¼nik do `struct string',
 *  - c - znaczek do dopisania.
 */
int string_append_c(struct string *s, char c)
{
	char *new;

	if (!s) {
		errno = EFAULT;
		return -1;
	}
	
	if (!s->str || strlen(s->str) + 2 > s->size) {
		if (!(new = realloc(s->str, s->size + 80)))
			return -1;
		if (!s->str)
			*new = 0;
		s->size += 80;
		s->str = new;
	}

	s->str[strlen(s->str) + 1] = 0;
	s->str[strlen(s->str)] = c;

	return 0;
}

/*
 * string_append_n()
 *
 * dodaje tekst do bufora alokuj±c odpowiedni± ilo¶æ pamiêci.
 *
 *  - s - wska¼nik `struct string',
 *  - str - tekst do dopisania,
 *  - count - ile znaków tego tekstu dopisaæ? (-1 znaczy, ¿e ca³y).
 */
int string_append_n(struct string *s, char *str, int count)
{
	char *new;

	if (!s) {
		errno = EFAULT;
		return -1;
	}

	if (count == -1)
		count = strlen(str);

	if (!s->str || strlen(s->str) + count + 1 > s->size) {
		if (!(new = realloc(s->str, s->size + count + 80)))
			return -1;
		if (!s->str)
			*new = 0;
		s->size += count + 80;
		s->str = new;
	}

	s->str[strlen(s->str) + count] = 0;
	strncpy(s->str + strlen(s->str), str, count);

	return 0;
}

int string_append(struct string *s, char *str)
{
	return string_append_n(s, str, -1);
}

/*
 * string_init()
 *
 * inicjuje strukturê string. alokuje pamiêæ i przypisuje pierwsz± warto¶æ.
 *
 *  - value - je¶li NULL, ci±g jest pusty, inaczej kopiuje tam.
 *
 * zwraca zaalokowan± strukturê `string' lub NULL je¶li pamiêci brak³o.
 */
struct string *string_init(char *value)
{
	struct string *tmp = malloc(sizeof(struct string));

	if (!tmp)
		return NULL;

	if (!value)
		value = "";

	tmp->str = strdup(value);
	tmp->size = strlen(value) + 1;

	return tmp;
}

/*
 * string_free()
 *
 * zwalnia pamiêæ po strukturze string i mo¿e te¿ zwolniæ pamiêæ po samym
 * ci±gu znaków.
 *
 *  - s - struktura, któr± wycinamy,
 *  - free_string - zwolniæ pamiêæ po ci±gu znaków?
 *
 * je¶li free_string=0 zwraca wska¼nik do ci±gu, inaczej NULL.
 */
char *string_free(struct string *s, int free_string)
{
	char *tmp = NULL;

	if (!s)
		return NULL;

	if (free_string)
		free(s->str);
	else
		tmp = s->str;

	free(s);

	return tmp;
}

