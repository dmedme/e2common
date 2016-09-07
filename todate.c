/*
 * todate.c - Convert seconds into an ORACLE format date
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993";
#include <string.h>
#include <time.h>
#include <stdio.h>
char * conv_time();
char buf[BUFSIZ];
double strtod();
char * to_char();
/*
 * Main Program
 */
main(argc,argv)
int argc;
char **argv;
{

    if (argc == 2)
        puts(conv_time(argv[1]));
    else
    if (argc == 3)
        puts(to_char(argv[2],strtod(argv[1],NULL)));
    else
        exit(1);
    exit(0);
}
/*
 * Seconds since 1970 to ORACLE Date string
 */
char * conv_time(time_secs)
char * time_secs;
{
long time_long;
char * conv_time;
static char time_buf[80];
time_long = atoi(time_secs);
conv_time=ctime(&time_long);
(void) sprintf(time_buf,"%-2.2s-%-3.3s-%-4.4s %-2.2s:%-2.2s:%-2.2s",
conv_time+8,
conv_time+4,
conv_time+20,
conv_time+11,
conv_time+14,
conv_time+17);
return time_buf;
}
