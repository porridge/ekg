/* $Id$ */

/*
 *  (C) Copyright 2002  Pawel Maziarz <drg@go2.pl>
 *			Wojtek Kaniewski <wojtekka@irc.pl>
 *			Robert J. Wozny <speedy@ziew.org>
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
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>		// linux specific  
#include <linux/kd.h>			// -||- 
#include <ctype.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include "ioctld.h"

char sock_path[PATH_MAX] =  "";

int blink_leds(int *flag, int *delay) 
{
    	int s, fd;
        
	fd = open("/dev/console", O_WRONLY);

	for(s=0; flag[s] >= 0; s++) {
	    	ioctl(fd, KDSETLED, flag[s]);
		usleep(delay[s]);
	}

	ioctl(fd, KDSETLED, 8);
	
	close(fd);
	
	return 0;
}

int beeps_spk(int *tone, int *delay)
{
    	int s, fd;
       
    	fd = open("/dev/console", O_WRONLY);
		
	for(s=0; tone[s] >= 0; s++) {
	    	ioctl(fd, KIOCSOUND, tone[s]);
		usleep(delay[s]);
	}

	ioctl(fd, KIOCSOUND, 0);
	
	close(fd);

	return 0;
}

void quit() 
{
    	unlink(sock_path);
	exit(0);
}

int main(int argc, char **argv) 
{
    	int sock, length;
	struct sockaddr_un addr;
	struct action_data data;
	
	if (argc != 2) {
		printf("program ten nie jest przeznaczony do samodzielnego wykonywania! \n");
	    	exit(1);
	}
	
	if (strlen(argv[1]) >= PATH_MAX)
	    	exit(2);
	
	signal(SIGQUIT, quit);
	signal(SIGTERM, quit);
	signal(SIGINT, quit);
	
	umask(0177);

	close(1); 
	close(2);
	
	strcpy(sock_path, argv[1]);
	
	unlink(sock_path);

	if((sock = socket(AF_UNIX, SOCK_DGRAM, 0))==-1) 
		exit(1);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, sock_path);
	length = sizeof(addr);

	if(bind(sock, (struct sockaddr *)&addr, length)==-1) 
	    	exit(2);

	chown(sock_path, getuid(), -1);

	while(1) {
	    	if (recvfrom(sock, &data, sizeof(data), 0, (struct sockaddr *)&addr,&length)==-1) 
		    	continue;
		
		if (data.act == ACT_BLINK_LEDS)  
		    	blink_leds(data.value, data.delay);
		
		else if (data.act == ACT_BEEPS_SPK) 
		    	beeps_spk(data.value, data.delay);
	}
	
	exit(0);
}
