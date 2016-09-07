/*
 * tosecs.c - get the time in seconds since 1 Jan 1970 GMT
 */
static char * sccs_id= "@(#) $Name$ $Id$\nCopyright (c) E2 Systems Limited, 1993";
#include <stdio.h>
long time();
#ifdef DEBUG
double local_secs();
char * to_char();
char * ctime();
#endif
main( argc, argv)
int argc;
char ** argv;
{
char *x;
char *y;
double ret_time;
long t;

    if (argc < 2)
        (void) printf("%lu\n",time(0));
    else
    {
        if (argc < 3)
            y = "DD Mon YYYY";
        else
            y = argv[2];
        (void) date_val(argv[1],y,&x,&ret_time);
#ifdef DEBUG
        t = (long) ret_time;
        (void) printf("%12.0f|%s|%s\n",ret_time, ctime(&t),
              to_char("dd mon yyyy hh24:mi:ss", local_secs(t)));
#else
        (void) printf("%12.0f\n",ret_time);
#endif
    }
    exit(0);
}
