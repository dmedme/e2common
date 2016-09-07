/*
 *    natsock.c - Routines to get round the broken SOLARIS named pipes,
 *                and to use Microsoft Windows Named Pipes.
 *
 *    Copyright (C) E2 Systems 1995
 *
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (C) E2 Systems Limited 1995";
#ifndef VCC2003
#include <sys/param.h>
#endif
#include <sys/types.h>
#ifndef VCC2003
#include <sys/file.h>
#include <sys/time.h>
#endif
#include <fcntl.h>
#include <time.h>
#ifdef ULTRIX
#include <sys/fcntl.h>
#else
#ifdef AIX
#ifndef LINUX
#include <sys/fcntl.h>
#endif
#endif
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#ifdef MINGW32
#include <windows.h>
/************************************************************************
 * Windows Named Pipe Set-up; return a file descriptor to use.
 * This isn't really necessary, but it maintains the fiction that Windows
 * Named Pipes are like Solaris UNIX-domain sockets.
 */
int fifo_listen(link)
char * link;
{
HANDLE pipe_hand;
/*
 *    Create the named pipe.
 */
    if ((pipe_hand = CreateNamedPipe(link, 
             PIPE_ACCESS_DUPLEX | FILE_FLAG_WRITE_THROUGH,
             PIPE_TYPE_BYTE | PIPE_WAIT, 
             1,
             16384, 16384, 100,
            (LPSECURITY_ATTRIBUTES) NULL)) == INVALID_HANDLE_VALUE)
    { 
        (void) fprintf(stderr,
              "%s line %d: Windows CreateNamedPipe %s create failed error %d\n",
                       __FILE__, __LINE__, link, GetLastError()) ;
        return -1;
    }
    return _open_osfhandle(pipe_hand, O_RDWR);
}
/************************************************************************
 * Establish a connexion
 */
int fifo_connect(link, fifo_name)
char * link;       /* Not used */
char * fifo_name;
{
/*
 * Connect to the destination.
 */
HANDLE pipe_hand;

restart:
    while (!WaitNamedPipe(fifo_name, NMPWAIT_WAIT_FOREVER))
    {
        fprintf(stderr,
               "FIFO WaitNamedPipe() Failed error: %u\n", GetLastError());
        if (GetLastError() == ERROR_PIPE_BUSY
         || GetLastError() == ERROR_FILE_NOT_FOUND)
            _sleep(1000);        /* Wait for thing to be available */
        else
            return -1;
    }
    if ((pipe_hand = CreateFile(fifo_name,
         GENERIC_READ|GENERIC_WRITE,
         FILE_SHARE_READ|FILE_SHARE_WRITE,
         NULL, /* Security Attributes */
         OPEN_EXISTING,
         0,
         NULL)) == INVALID_HANDLE_VALUE)
    {
/*
 * There seem to be any number of undocumented 'Expected' errors here; try
 * again and it works ...
 */
        fprintf(stderr,
               "FIFO CreateFile() Failed error: %u\n", GetLastError());
        goto restart;
    }
    return _open_osfhandle(pipe_hand, O_RDWR);
}
/************************************************************************
 * Establish a connexion as a server. work_dir is actually the name in this
 * case
 */
int fifo_accept(work_dir,listen_fd)
char * work_dir;
int listen_fd;
{
HANDLE pipe_hand;

    if (listen_fd != -1)
        pipe_hand = _get_osfhandle(listen_fd);
    else
    {
restart:
        if ((listen_fd = fifo_listen(work_dir)) == -1
         || (pipe_hand = _get_osfhandle(listen_fd)) == INVALID_HANDLE_VALUE)
             return -1;
    }
/*
 * The step analogous to accept() ...
 */
    if (!(ConnectNamedPipe(pipe_hand, NULL))
      && GetLastError() != ERROR_PIPE_CONNECTED)
    {
        (void) fprintf(stderr,
         "%s line %d: Windows Named Pipe %s ConnectNamedPipe failed error %d\n",
              __FILE__, __LINE__, work_dir, GetLastError()) ;
        (void) DisconnectNamedPipe(pipe_hand);
        close(listen_fd);
        goto restart;
    }
    return listen_fd;
}
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/un.h>
static struct sockaddr_un one_sock = { AF_UNIX };
static struct sockaddr_un other_sock = { AF_UNIX };
char * filename();
/************************************************************************
 * UNIX Socket set up; return the file descriptor.
 */
int fifo_listen(link)
char * link;
{
int listen_fd;
/*
 *    Now create the socket to listen on
 */
    if ((listen_fd = socket(AF_UNIX,SOCK_STREAM,0))<0)
    { 
        perror("Communication socket create failed"); 
        (void) fprintf(stderr,
              "%s line %d: STREAM listen socket %s create failed\n",
                       __FILE__, __LINE__, link) ;
        fflush(stderr);
        return -1;
    }
/*
 * Bind its name to it
 */
    strcpy(&(one_sock.sun_path[0]), link);
    if (bind(listen_fd , (struct sockaddr *) (&one_sock),
             sizeof(one_sock.sun_family) + strlen(one_sock.sun_path)))
    { 
        perror("Communication bind failed"); 
        (void) fprintf(stderr,"%s line %d: STREAM listen bind %s failed\n",
                       __FILE__, __LINE__, link) ;
        close(listen_fd);
        fflush(stderr);
        return -1;
    }
/*
 * Listen on it
 */
    if (listen(listen_fd , 5))
    { 
        perror("Communication listen failed"); 
        (void) fprintf(stderr,"%s line %d: STREAM listen %s failed\n",
                       __FILE__, __LINE__, link) ;
        fflush(stderr);
        close(listen_fd);
        return -1;
    }
    return listen_fd ;
}
/************************************************************************
 * Establish a connexion
 * - Fills in the socket stuff.
 * - Sets up a calling socket; this is validated at the other end.
 */
int fifo_connect(link, fifo_name)
char * link;
char * fifo_name;
{
int fifo_fd;
/*
 * Connect to the destination.
 */
    strcpy(&(one_sock.sun_path[0]), link);
    if ((fifo_fd = socket(AF_UNIX,SOCK_STREAM,0))<0)
    { 
        perror("STREAM socket create failed"); 
        (void) fprintf(stderr,
           "%s line %d: STREAM connect socket %s create failed\n",
                       __FILE__, __LINE__, link) ;
        fflush(stderr);
        return -1;
    }
/*
 * Bind its name to it
 */
    strcpy(&(one_sock.sun_path[0]), link);
    if (bind(fifo_fd , (struct sockaddr *) (&one_sock),
           sizeof(one_sock.sun_family) + strlen(one_sock.sun_path)))
    { 
        perror("Communication bind failed"); 
        (void) fprintf(stderr,
           "%s line %d: STREAM connect socket bind %s failed\n",
                       __FILE__, __LINE__, link) ;
/*
 * Why should we care?
 *
 *      close(fifo_fd);
 *      fflush(stderr);
 *      return -1;
 */
    }
    strcpy(&(other_sock.sun_path[0]), fifo_name);
    if (connect(fifo_fd , (struct sockaddr *) (&other_sock),
           sizeof(other_sock.sun_family) + strlen(other_sock.sun_path)))
    {
        perror("connect() failed");
        (void) fprintf(stderr,
               "%s line %d: STREAM socket connect %s to %s failed\n",
                       __FILE__, __LINE__, link, fifo_name) ;
        close(fifo_fd);
        fflush(stderr);
        return -1;
    }
    else
        return fifo_fd ;
}
/************************************************************************
 * Establish a connexion as a server.
 * - Fills in the socket stuff.
 * - Sets up a calling socket; this is validated at the other end.
 * - The socket must be unlinked by the user process.
 */
int fifo_accept(work_dir,listen_fd)
char * work_dir;
int listen_fd;
{
int fifo_fd;
/*
 * Connect to the destination.
 */
char * send_check = filename("%s/%s",work_dir, "tmp");
int ret_len = sizeof(other_sock);

    if ((fifo_fd = accept(listen_fd , (struct sockaddr *) (&other_sock),
              &ret_len))< 0)
    {
static char * x = "Designation accept() failure\n";

        if (errno != EINTR)
        {
            perror("accept() failed");
            (void) fprintf(stderr,
               "%s line %d: STREAM socket accept %s from %d failed\n",
                       __FILE__, __LINE__,  send_check, listen_fd) ;
            fflush(stderr);
        }
        free(send_check);
        return -1;
    }
    else if (strncmp(other_sock.sun_path,send_check,strlen(send_check)))
    {
        (void) fprintf(stderr,
         "%s line %d: STREAM socket unexpected accept from %s rather than %s\n",
                       __FILE__, __LINE__, other_sock.sun_path,  send_check) ;
        fflush(stderr);
        free(send_check);
        (void) close(fifo_fd);
        return -1;
    }
    else
    {
        free(send_check);
        return fifo_fd;
    }
}
#endif
