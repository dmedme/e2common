/****************************************
 * Circular buffer manipulation functions
 *  - Simple add and remove macros
 *  - read into/write from the circular buffer
 *  - trade off elegance versus speed; with macros rather than
 *    functions, the buffers need to be pre-allocated.
 *  - Three buffers in total
 *  - Added extra bytes because of apparent over-run (top not right)
 * @(#) $Name$ $Id$
 * Copyright (c) 1993 E2 Systems
 */
#ifndef CIRCLIB_H
#define CIRCLIB_H
#ifdef TIMEOUT_HANDLER
#define BUFLEN 16384
#define BUF_LOW 20
#define BUF_HIGH 480
#else
#ifdef OSXDC
#define BUFLEN 2048
#define BUF_LOW 512
#define BUF_HIGH 1536
#else
#ifdef HP7
#define BUFLEN 16384
#define BUF_LOW 4096
#define BUF_HIGH 12288
#else
#ifdef OSF
#define BUFLEN 16384
#define BUF_LOW 4096
#define BUF_HIGH 12288
#else
#define BUFLEN 4096
#define BUF_LOW 1024
#define BUF_HIGH 3072
#endif
#endif
#endif
#endif
#define PTYOUT ((pg.circ_buf[0])) 
#define PTYCHAR ((pg.circ_buf[1])) 
#define PTYIN ((pg.circ_buf[2])) 
#include "ansi.h"
struct circ_buf * cre_circ_buf ANSIARGS((void));
void des_circ_buf ANSIARGS((struct circ_buf * buf));
int buftomem ANSIARGS((char * str, struct circ_buf * buf));
/***************************
 * Macros - No checks for wrap-around
 */
#define BUFADD(buf,x) ((*((*(buf)).head++)=(x)),((*(buf)).head=((*(buf)).head==(*(buf)).top)?(*(buf)).base:(*(buf)).head),(*(buf)).buf_cnt++)

#ifdef PATH_AT
int bufadd ANSIARGS(( struct circ_buf * buf, int x));
#else
#define bufadd BUFADD
#endif
int bufinc ANSIARGS(( struct circ_buf * buf, int x));
#define BUFTAKE(buf,x) ((*(x)=*((*(buf)).tail++)),((*(buf)).tail=((*(buf)).tail==(*(buf)).top)?(*(buf)).base:(*(buf)).tail),(*(buf)).buf_cnt--)

#ifdef PATH_AT
int buftake ANSIARGS((struct circ_buf * buf, short int * x));
#else
#define buftake BUFTAKE
#endif
#define BUFUNTAKE(buf) (((*(buf)).tail=((*(buf)).tail==(*(buf)).base)?((*(buf)).top-1):(*(buf)).tail--),(*(buf)).buf_cnt++)
#ifdef PATH_AT
int bufuntake ANSIARGS(( struct circ_buf * buf ));
#else
#define bufuntake BUFUNTAKE
#endif
/*
 * The following macros don't work any more, because the buffers are now
 * short ints
 */
#define BUFREAD(fd,buf,deb) (((*(buf)).head+=read(fd,(*(buf)).head,(int) ((*(buf)).head<(*(buf)).tail)?((*(buf)).head - (*(buf)).tail - 1):((*(buf)).top-(*(buf)).head))),((*(buf)).head=((*(buf)).head==(*(buf)).top)?(*(buf)).base:(*(buf)).head),(*(buf)).buf_cnt=(((*(buf)).head>=(*(buf)).tail)?((*(buf)).head-(*(buf)).tail):((*(buf)).head-(*(buf)).base+(*(buf)).top-(*(buf)).tail)))
#define BUFWRITE(fd,buf,n,deb) (((*(buf)).tail+=write(fd,(*(buf)).tail,(int) ((*(buf)).head>((*(buf)).tail+n))?(n):(((*(buf)).head>=(*(buf)).tail)?((*(buf)).head-(*(buf)).tail):((((*(buf)).tail+(n)+1)<(*(buf)).top)?(n):((*(buf)).top-(*(buf)).tail-1))))),((*(buf)).tail=((*(buf)).tail==(*(buf)).top)?(*(buf)).base:(*(buf)).tail),(*(buf)).buf_cnt=(((*(buf)).head>=(*(buf)).tail)?((*(buf)).head-(*(buf)).tail):((*(buf)).head-(*(buf)).base+(*(buf)).top-(*(buf)).tail)))
/*
 * These do
 */
#define BUFDUMP(buf) {short int *_x;for(_x=(*(buf)).tail;_x!=(*(buf)).head;_x++,_x=(_x==(*(buf)).top)?(*(buf)).base:_x)fprintf(stderr,"%c",(short int) (*_x));}
#ifdef PATH_AT
void bufdump ANSIARGS(( struct circ_buf * buf ));
#else
#define bufdump BUFDUMP
#endif
#define BUFNDUMP(buf) {short int *_x;for(_x=(*(buf)).tail;_x!=(*(buf)).head;_x++,_x=(_x==(*(buf)).top)?(*(buf)).base:_x)fprintf(stderr,"<%d>",(int) (*_x));fprintf(stderr,"\n");}
#ifdef PATH_AT
void bufndump ANSIARGS(( struct circ_buf * buf ));
void log_inject ANSIARGS(( char * str ));
void log_discard ANSIARGS(( struct circ_buf * buf, short int* head,
short int *tail));
#else
#define bufndump BUFDUMP
#endif
#endif
