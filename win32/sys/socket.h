/*
 * win32/sys/socket.h
 *
 * (C) Copyright 2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                    Hao <hao@astercity.net>
 * 
 * wrapper uniksowych funkcji do windzianych.
 */

#ifndef COMPAT_SYS_SOCKET_H
#define COMPAT_SYS_SOCKET_H

#include <winsock2.h>
#pragma link "Ws2_32.lib"  /* W Borlandzie linkuje biblioteke z socketami */

#define ASSIGN_SOCKETS_TO_THREADS /* gg_connect bedzie zapisywal nr socketa na liscie,
				     tak zeby mozna go bylo zamknac w polaczeniach synchronicznych (z innego watku) */

#define socket(af,type,protocol) WSASocket(af,type,protocol,0,0,WSA_FLAG_OVERLAPPED)
#define write(handle,buf,len) send(handle,(const char*)buf,len,0)
#define read(handle,buf,len) recv(handle,(const char*)buf,len,0)

#ifdef ASSIGN_SOCKETS_TO_THREADS
#  define close(handle) gg_thread_close(-1,handle)
#else
#  define close(handle) closesocket(handle)
#endif

#define EINPROGRESS WSAEINPROGRESS
#define ENOTCONN WSAENOTCONN
#define vsnprintf _vsnprintf
#define snprintf _snprintf

#define socket(x,y,z) 

static int fork()
{
	return -1;
}

static int pipe(int filedes[])
{
	return -1;
}

#endif /* COMPAT_SYS_SOCKET_H */
