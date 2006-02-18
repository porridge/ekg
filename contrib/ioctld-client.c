/* $Id$ */

/*
 *  (C) Copyright 2002-2004 Pawe³ Maziarz <drg@hehe.pl>
 *                          Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#if defined (__FreeBSD__) || defined (__DragonFly__)
#  include <sys/kbio.h>			
#endif
#ifdef sun /* Solaris */
#  include <sys/kbd.h>
#  include <sys/kbio.h>
#endif 
#ifdef __linux__
#  include <linux/cdrom.h>		  
#  include <linux/kd.h>			 
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* from ictold.h stuff */
#define IOCTLD_MAX_ITEMS        50
#define IOCTLD_MAX_DELAY        2000000
#define IOCTLD_DEFAULT_DELAY    100000

#define IOCTLDNET_PORT          22004

struct action_data {
        int act;
        int value[IOCTLD_MAX_ITEMS];
        int delay[IOCTLD_MAX_ITEMS];
};

enum action_type {
        ACT_BLINK_LEDS = 1,
        ACT_BEEPS_SPK = 2
};

int blink_leds(int *flag, int *delay);
int beeps_spk(int *tone, int *delay);
/* ioctld.h */

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

char sock_path[PATH_MAX + 1] = "";

int blink_leds(int *flag, int *delay) 
{
    	int s, fd, restore_data;

	fprintf(stderr, "blink_leds");

#ifdef sun 
	if ((fd = open("/dev/kbd", O_RDONLY)) == -1)
		return -1;

	ioctl(fd, KIOCGLED, &restore_data);
#else
	if ((fd = open("/dev/console", O_WRONLY)) == -1)
		fd = STDOUT_FILENO;

	ioctl(fd, KDGETLED, &restore_data);
#endif

	for (s = 0; flag[s] >= 0 && s < IOCTLD_MAX_ITEMS; s++) {
#ifdef sun
		int leds = 0;
		/* tak.. na sunach jest to troszkê inaczej */
		if (flag[s] & 1) 
			leds |= LED_NUM_LOCK;
		if (flag[s] & 2) 
			leds |= LED_SCROLL_LOCK;
		if (flag[s] & 4) 
			leds |= LED_CAPS_LOCK; 

		ioctl(fd, KIOCSLED, &leds);
#else
	    	ioctl(fd, KDSETLED, flag[s]);
	
		fprintf(stderr, " %d/%d", flag[s], delay[s]);
#endif 
		if (delay[s]) {
			if (delay[s] <= IOCTLD_MAX_DELAY)
				usleep(delay[s]);
			else
				usleep(IOCTLD_MAX_DELAY);
		}
	}

#ifdef sun
	ioctl(fd, KIOCSLED, &restore_data);
#else
	ioctl(fd, KDSETLED, restore_data);
#endif
	
	if (fd != STDOUT_FILENO)
		close(fd);
	
	fprintf(stderr, "\n");
	
	return 0;
}

int beeps_spk(int *tone, int *delay)
{
    	int s;
#ifndef sun
	int fd;

	fprintf(stderr, "beeps_spk");

    	if ((fd = open("/dev/console", O_WRONLY)) == -1)
		fd = STDOUT_FILENO;
#endif
		
	for (s = 0; tone[s] >= 0 && s < IOCTLD_MAX_ITEMS; s++) {
		if (tone[s])
			fprintf(stderr, " %d/%d", tone[s], delay[s]);

#if defined (__FreeBSD__) || defined (__DragonFly__)
		ioctl(fd, KIOCSOUND, 0);
#endif

#ifndef sun
	    	ioctl(fd, KIOCSOUND, tone[s]);
#else
		/* ¿a³osna namiastka... */
		putchar('\a');
		fflush(stdout);
#endif
		if (delay[s]) {
			if (delay[s] <= IOCTLD_MAX_DELAY)
				usleep(delay[s]);
			else
				usleep(IOCTLD_MAX_DELAY);
		}
	}

#ifndef sun
	ioctl(fd, KIOCSOUND, 0);
	
	if (fd != STDOUT_FILENO)
		close(fd);
#endif

	fprintf(stderr, "\n");

	return 0;
}

int main(int argc, char **argv) 
{
    	int sock, port, ret;
	struct sockaddr_in addr;
	struct action_data data;
        struct hostent *h;
        in_addr_t host_addr, tmp_addr;
	
	if (argc < 2) {
		fprintf(stderr, "U¿ycie: %s <host> [port]\n", *argv);
		exit(1);
	}

	if ((tmp_addr = inet_addr(argv[1])) == INADDR_NONE) {
		if ((h = gethostbyname(argv[1])) == NULL) {
			fprintf(stderr, "Host %s nie znaleziony.\n", argv[1]);
			exit(1);
		}

		bcopy(*h->h_addr_list, &host_addr, sizeof(in_addr_t));
		host_addr = ntohl(host_addr);
	} else
		host_addr = ntohl(tmp_addr);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Nie mogê utworzyæ gniazda: %s\n", strerror(errno));
		exit(1);
	}
	
	if (argv[2])
		port = atoi(argv[2]);
	else
		port = IOCTLDNET_PORT;

	addr.sin_addr.s_addr = htonl(host_addr);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;

	if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
		fprintf(stderr, "Nie mogê po³±czyæ siê z %s:%d (%s)\n", argv[1], port, strerror(errno));
		close(sock);
		exit(1);
	}

	fprintf(stderr, "Po³±czono.\n");

	while (1) {
		if ((ret = read(sock, &data, sizeof(struct action_data))) == 0 || ret == -1) {
			if (ret == -1)
				perror("read()");
			close(sock);
			exit(1);
		}
		
		if (data.act == ACT_BLINK_LEDS)  
		    	blink_leds(data.value, data.delay);

		else if (data.act == ACT_BEEPS_SPK) 
		    	beeps_spk(data.value, data.delay);
	}

	close(sock);
	
	exit(0);
}
