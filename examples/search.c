/* $Id$ */

#include <stdio.h>
#include "libgg.h"

#ifdef ASYNC

#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

void sigchld()
{
	wait(NULL);
	signal(SIGCHLD, sigchld);
}

#endif

int main()
{
	struct gg_search *foo;
	struct gg_search_request r;
	int i;

	gg_debug_level = 255;
	
	r.first_name = "Ania";
	r.last_name = NULL;
	r.nickname = NULL;
	r.city = NULL;
	r.min_birth = 0;
	r.max_birth = 0;
	r.gender = 0;
	r.phone = NULL;
	r.email = NULL;
	r.uin = 0;
	r.active = 0;

#ifndef ASYNC

	if (!(foo = gg_search(&r, 0)))
		return 1;

#else

	signal(SIGCHLD, sigchld);

	if (!(foo = gg_search(&r, 1)))
		return 1;

        while (1) {
                fd_set rd, wr, ex;

                FD_ZERO(&rd);
                FD_ZERO(&wr);
                FD_ZERO(&ex);

                if ((foo->check & GG_CHECK_READ))
                        FD_SET(foo->fd, &rd);
                if ((foo->check & GG_CHECK_WRITE))
                        FD_SET(foo->fd, &wr);
                FD_SET(foo->fd, &ex);

                if (select(foo->fd + 1, &rd, &wr, &ex, NULL) == -1 || FD_ISSET(foo->fd, &ex)) {
			if (errno == EINTR)
				continue;
			gg_free_search(foo);
			perror("select");
			return 1;
		}

                if (FD_ISSET(foo->fd, &rd) || FD_ISSET(foo->fd, &wr)) {
			if (gg_search_watch_fd(foo) == -1) {
				gg_free_search(foo);
				fprintf(stderr, "no b³±d jak b³±d\n");
				return 1;
			}
			if (foo->state == GG_STATE_IDLE) {
				gg_free_search(foo);
				fprintf(stderr, "jaki¶tam b³±d\n");
				return 1;
			}
			if (foo->state == GG_STATE_FINISHED)
				break;
		}
        }

#endif

	printf("count=%d\n", foo->count);

	for (i = 0; i < foo->count; i++) {
		printf("%ld: %s %s (%s), %d, %s\n", foo->results[i].uin, foo->results[i].first_name, foo->results[i].last_name, foo->results[i].nickname, foo->results[i].born, foo->results[i].city);
	}

	gg_free_search(foo);

	return 0;
}

