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
	struct gg_register *foo;
	char email[100], password[100];
	int i;

	gg_debug_level = 255;
	
	printf("e-mail: ");
	fgets(email, 99, stdin);
	if (email[strlen(email)-1] == '\n')
		email[strlen(email)-1] = 0;
	printf("password: ");
	fgets(password, 99, stdin);
	if (password[strlen(password)-1] == '\n')
		password[strlen(password)-1] = 0;

#ifndef ASYNC

	if (!(foo = gg_register(email, password, 0)))
		return 1;

#else

	signal(SIGCHLD, sigchld);

	if (!(foo = gg_register(email, password, 1)))
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
			gg_free_register(foo);
			perror("select");
			return 1;
		}

                if (FD_ISSET(foo->fd, &rd) || FD_ISSET(foo->fd, &wr)) {
			if (gg_register_watch_fd(foo) == -1) {
				gg_free_register(foo);
				fprintf(stderr, "no b³±d jak b³±d\n");
				return 1;
			}
			if (foo->state == GG_STATE_IDLE) {
				gg_free_register(foo);
				fprintf(stderr, "jaki¶tam b³±d\n");
				return 1;
			}
			if (foo->state == GG_STATE_FINISHED) {
				printf("uin=%u\n", foo->uin);
				gg_free_register(foo);
				break;
			}
		}
        }

#endif

	return 0;
}

