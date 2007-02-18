/*
 * $Id$
 *
 * listmerge (C) 2007 Adam Wysocki <gophi@ekg.chmurka.net>
 */

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

extern char *optarg;
extern int optind, opterr, optopt, errno;

struct opt_t {
	int ignore_invalid;
	const char *outfn;
	const char *newline;
};

struct inode_t {
	const char *fname;
	ino_t inode;
};

struct userlist_entry_t {
	char *nick, *uin, *line;
};

struct userlist_t {
	const char *fname;
	struct userlist_entry_t *entries;
	size_t num_entries;
};

static struct opt_t opt;

static void version(void)
{
	fputs("$Id$\n", stderr);
}

static void fasthelp(void)
{
	const char msg[] = 
		"Narzêdzie s³u¿y do ³±czenia niekompletnych list kontaktów (userlist) w ekg.\n"
		"\n"
		"Sk³adnia: listmerge [-hvig] [-o outfile] infile1 infile2 [...]\n"
		"\n"
		"Opcje:\n"
		"  -h: Ten ekran pomocy.\n"
		"  -v: Wypisanie informacji o wersji.\n"
		"  -i: Nie ignorowanie nieprawid³owych wpisów.\n"
		"  -o: Nazwa pliku wyj¶ciowego.\n"
		"  -n: Zapisywanie nowych linii w postaci \\n, zamiast \\r\\n.\n"
		"\n"
		"Je¿eli plik wyj¶ciowy nie zostanie okre¶lony, to program wyrzuci userlistê \n"
		"na stdout. Wpisy o powtarzaj±cych siê nazwach i/lub uinach bêd± brane z pliku \n"
		"podanego wcze¶niej na li¶cie poleceñ.\n"
		"\n"
		"Skargi, wnioski i bugi: http://ekg.chmurka.net/kontakt.php\n";

	version();
	fputs(msg, stderr);
}

static ino_t get_inode(const char *fname)
{
	struct stat st;

	if (stat(fname, &st) == -1) {
		fprintf(stderr, "B³±d sprawdzania statusu (stat(2)) pliku '%s': %s.\n", fname, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return st.st_ino;
}

static int inode_comparator(const void *p1, const void *p2)
{
	const struct inode_t *i1 = p1, *i2 = p2;
	int rs;
	static int duplicate_inode_found = 0;

	if (!p1 && !p2)
		return duplicate_inode_found;

	rs = i1->inode - i2->inode;

	if (!rs) {
		fprintf(stderr, "Podane pliki ('%s' i '%s') to te same pliki.\n", i1->fname, i2->fname);
		duplicate_inode_found = 1;
	}

	return rs;
}

static int check_dupe_files(int ac, char * const av[])
{
	struct inode_t *inodes;
	size_t i, num = ac - optind;

	assert((inodes = (struct inode_t *) calloc(num, sizeof(struct inode_t))));

	for (i = 0; i < num; i++) {
		inodes[i].fname = av[i + optind];
		inodes[i].inode = get_inode(inodes[i].fname);
	}

	qsort(inodes, num, sizeof(struct inode_t), inode_comparator);
	free(inodes);

	return inode_comparator(NULL, NULL);
}

static void parse_opts(int ac, char * const av[])
{
	int done = 0;
	struct stat st;

	opt.ignore_invalid = 1;
	opt.outfn = NULL;
	opt.newline = "\r\n";

	while (!done) {
		switch (getopt(ac, av, "hvio:n")) {
			case 'h':
				done = 2;
				break;

			case 'v':
				done = 3;
				break;

			case 'i':
				opt.ignore_invalid = 0;
				break;

			case 'o':
				opt.outfn = optarg;
				break;

			case 'n':
				opt.newline = "\n";
				break;

			case -1:
				done = 1;
				break;

			default:
				exit(EXIT_FAILURE);
		}
	}

	if (done == 1 && ((unsigned) (ac - optind) < 2))
		done = 2;

	if (done == 2 || done == 3) {
		if (done == 2)
			fasthelp();
		else
			version();

		exit(EXIT_SUCCESS);
	}

	if (opt.outfn && stat(opt.outfn, &st) != -1) {
		fprintf(stderr, "Podany plik wyj¶ciowy istnieje.\n");
		exit(EXIT_FAILURE);
	}

	if (check_dupe_files(ac, av))
		exit(EXIT_FAILURE);
}

/* z ekg/compat/strlcpy.c - nie chcialem robic zaleznosci wiec 
 * przemianowalem funkcje na xstrlcpy
 */
static size_t xstrlcpy(char *dst, const char *src, size_t size)
{
	register size_t i, n = size;

	for (i = 0; n > 1 && src[i]; i++, n--)
		dst[i] = src[i];

	if (n)
		dst[i] = 0;

	while (src[i])
		i++;

	return i;
}

/* z ekg/src/stuff.c */
static char *read_file(FILE *f)
{
	char buf[1024], *res = NULL;

	while (fgets(buf, sizeof(buf), f)) {
		int first = (res) ? 0 : 1;
		size_t new_size = ((res) ? strlen(res) : 0) + strlen(buf) + 1;

		assert((res = realloc(res, new_size)));
		if (first)
			*res = 0;
		xstrlcpy(res + strlen(res), buf, new_size - strlen(res));

		if (strchr(buf, '\n'))
			break;
	}

	if (res && strlen(res) > 0 && res[strlen(res) - 1] == '\n')
		res[strlen(res) - 1] = 0;
	if (res && strlen(res) > 0 && res[strlen(res) - 1] == '\r')
		res[strlen(res) - 1] = 0;

	return res;
}

/* z ekg/src/dynstuff.c, string_t zamienione na realloc */
static char *unescape(const char *src)
{
	int state = 0;
	char *buf = NULL;
	size_t bufsz = 0;
	unsigned char hex_msb = 0;

#define string_append_c(ch) do { \
	assert((buf = (char *) realloc(buf, bufsz + 1))); \
	buf[bufsz++] = ch; \
} while (0)

	if (!src)
		return NULL;

	for (; *src; src++) {
		char ch = *src;

		if (state == 0) {		/* normalny tekst */
			/* sprawdzamy czy mamy cos po '\\', bo jezeli to ostatni 
			 * znak w stringu, to nie zostanie nigdy dodany. */
			if (ch == '\\' && *(src + 1)) {
				state = 1;
				continue;
			}
			string_append_c(ch);
		} else if (state == 1) {	/* kod ucieczki */
			if (ch == 'a')
				ch = '\a';
			else if (ch == 'b')
				ch = '\b';
			else if (ch == 't')
				ch = '\t';
			else if (ch == 'n')
				ch = '\n';
			else if (ch == 'v')
				ch = '\v';
			else if (ch == 'f')
				ch = '\f';
			else if (ch == 'r')
				ch = '\r';
			else if (ch == 'x' && *(src + 1) && *(src + 2)) {
				state = 2;
				continue;
			} else if (ch != '\\')
				string_append_c('\\');	/* fallback - nieznany kod */
			string_append_c(ch);
			state = 0;
		} else if (state == 2) {	/* pierwsza cyfra kodu szesnastkowego */
			hex_msb = ch;
			state = 3;
		} else if (state == 3) {	/* druga cyfra kodu szesnastkowego */
#define unhex(x) (unsigned char) ((x >= '0' && x <= '9') ? (x - '0') : \
	(x >= 'A' && x <= 'F') ? (x - 'A' + 10) : \
	(x >= 'a' && x <= 'f') ? (x - 'a' + 10) : 0)
			string_append_c(unhex(ch) | (unhex(hex_msb) << 4));
#undef unhex
			state = 0;
		}
	}

	string_append_c(0);

#undef string_append_c

	return buf;
}

static size_t count_chars(const char *buf, char ch)
{
	size_t rs = 0;

	while (*buf)
		if (*buf++ == ch)
			rs++;

	return rs;
}

static char *split(const char *buf, char ch, size_t num)
{
	char *s = NULL;
	size_t i = 0, sz = 0;

	while (*buf) {
		if (*buf == ch)
			i++;
		else if (i == num) {
			assert((s = (char *) realloc(s, sz + 1)));
			s[sz++] = *buf;
		}

		buf++;
	}

	assert((s = (char *) realloc(s, sz + 1)));
	s[sz] = 0;

	return s;
}

static void add_userlist_entry(struct userlist_t *userlist, const char *line, const char *uin, const char *nick)
{
	struct userlist_entry_t e;

	e.line = line ? strdup(line) : NULL;
	e.uin = uin ? strdup(uin) : NULL;
	e.nick = nick ? strdup(nick) : NULL;

	assert((userlist->entries = (struct userlist_entry_t *) realloc(userlist->entries, (userlist->num_entries + 1) * sizeof(struct userlist_entry_t))));
	memcpy(userlist->entries + userlist->num_entries++, &e, sizeof(struct userlist_entry_t));
}

static void load_userlist_entries(struct userlist_t *userlist)
{
	FILE *fp;
	char *line;

	fp = fopen(userlist->fname, "rb");
	if (!fp) {
		fprintf(stderr, "Nie mo¿na otworzyæ pliku '%s': %s.\n", userlist->fname, strerror(errno));
		exit(EXIT_FAILURE);
	}

	while ((line = read_file(fp))) {
		char *uin = NULL, *nick = NULL;

		if (line[0] == '#' || (line[0] == '/' && line[1] == '/')) {
			free(line);
			continue;
		}

		if (count_chars(line, ';') < 7)
			goto invalid_entry;

		uin = split(line, ';', 6);

		if (!strncasecmp(uin, "gg:", 3))
			uin += 3;

		if (!strcmp(uin, "") || !atoi(uin))
			goto invalid_entry;

		if ((nick = split(line, ';', 3))) {
			char *tmp = unescape(nick);

			free(nick);
			nick = tmp;
		}

		add_userlist_entry(userlist, line, uin, nick);

		goto do_continue;

invalid_entry:
		fprintf(stderr, "Ostrze¿enie: Nieprawid³owy wpis na li¶cie '%s': %s\n", userlist->fname, line);

		if (!opt.ignore_invalid)
			add_userlist_entry(userlist, line, NULL, NULL);

do_continue:
		free(uin);
		free(nick);
		free(line);

		continue;
	}

	fclose(fp);
}

static size_t userlists_load(int ac, char * const av[], struct userlist_t **userlists)
{
	struct userlist_t *u;
	size_t i, num = ac - optind;

	assert((u = (struct userlist_t *) calloc(num, sizeof(struct userlist_t))));

	for (i = 0; i < num; i++) {
		u[i].fname = av[i + optind];
		u[i].entries = NULL;
		u[i].num_entries = 0;

		load_userlist_entries(u + i);
	}

	*userlists = u;
	return num;
}

static void userlists_free(struct userlist_t *userlists, size_t num)
{
	struct userlist_t *u = userlists;

	while (num--) {
		size_t i;

		for (i = 0; i < u->num_entries; i++) {
			free(u->entries[i].nick);
			free(u->entries[i].uin);
			free(u->entries[i].line);
		}

		free(u->entries);
		u++;
	}

	free(userlists);
}

static struct userlist_entry_t *find_dupe(struct userlist_t *userlist, const struct userlist_entry_t *src)
{
	size_t i;
	const char *uin, *nick;
	struct userlist_entry_t *e = userlist->entries;

	uin = src->uin;
	nick = src->nick;

	if (uin && !strncasecmp(uin, "gg:", 3))
		uin += 3;

	for (i = 0; i < userlist->num_entries; i++, e++) {
		if (uin && e->uin && *uin && *e->uin && !strcasecmp(uin, e->uin))
			return e;

		if (nick && e->nick && *nick && *e->nick && !strcasecmp(nick, e->nick))
			return e;
	}

	return NULL;
}

static void merge_one_userlist_entry(struct userlist_t *dst, const struct userlist_entry_t *src)
{
	struct userlist_entry_t *found;

	if ((found = find_dupe(dst, src))) {
		fprintf(stderr, "Usuniêcie wpisu '%s', bo ju¿ jest na li¶cie jako '%s'.\n", 
			src->line, found->line);
		return;
	}

	add_userlist_entry(dst, src->line, src->uin, src->nick);
}

static void merge_one_userlist(struct userlist_t *dst, const struct userlist_t *src)
{
	size_t i;

	for (i = 0; i < src->num_entries; i++)
		merge_one_userlist_entry(dst, src->entries + i);
}

static int userlist_entry_comparator(const void *p1, const void *p2)
{
	struct userlist_entry_t *e1 = (struct userlist_entry_t *) p1;
	struct userlist_entry_t *e2 = (struct userlist_entry_t *) p2;

	if (!e1->nick || !e2->nick)
		return 1;

	/* xxx strcoll - wpisy na wynikowej li¶cie bêd± sortowane leksykograficznie 
	 * bez uwzglêdnienia polskich znaków - to nie ma takiego znaczenia, ekg i 
	 * tak posortuje. u¿ycie strcoll tutaj wymaga³oby dodatkowych testów przed 
	 * kompilacj± przy u¿yciu autoconfa albo czego¶ podobnego. */

	return strcasecmp(e1->nick, e2->nick);
}

static struct userlist_t *userlists_merge(const struct userlist_t *userlists, size_t num)
{
	size_t i;
	struct userlist_t *final;

	assert((final = (struct userlist_t *) malloc(sizeof(struct userlist_t))));
	final->fname = opt.outfn;
	final->entries = NULL;
	final->num_entries = 0;

	for (i = 0; i < num; i++)
		merge_one_userlist(final, userlists + i);

	qsort(final->entries, final->num_entries, sizeof(struct userlist_entry_t), userlist_entry_comparator);

	return final;
}

static void save_final_list(struct userlist_t *u)
{
	FILE *fp = stdout;
	size_t i;
	struct userlist_entry_t *e = u->entries;

	if (u->fname && !(fp = fopen(u->fname, "w"))) {
		fprintf(stderr, "Nie uda³o siê otworzyæ pliku '%s': %s.\n", u->fname, strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < u->num_entries; i++, e++)
		fprintf(fp, "%s%s", e->line, opt.newline);

	if (u->fname)
		fclose(fp);
}

int main(int ac, char * const av[])
{
	struct userlist_t *userlists, *final_list;
	size_t userlists_num = 0;

	parse_opts(ac, av);
	userlists_num = userlists_load(ac, av, &userlists);

	final_list = userlists_merge(userlists, userlists_num);
	userlists_free(userlists, userlists_num);

	save_final_list(final_list);
	userlists_free(final_list, 1);

	return 0;
}
