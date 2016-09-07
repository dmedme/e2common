/*
 * which.c - find out where the command is. Do not bother about
 *           aliases.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems 1993";
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
char * getenv();
char * strtok();
#ifndef MAXPATHLEN
#define MAXPATHLEN PATHSIZE
#endif
int main (argc,argv)
int argc;
char ** argv;
{
    char * p, *x, *x1, **x2;
    int ret_code, path_len;
    
  
    if ((p = getenv("PATH")) == (char *) NULL)
        exit(1);
    if ((x = (char *) malloc(strlen(p) + 1)) == (char *) NULL)
        exit(1);
    for (ret_code = 0,x2 = &argv[1]; x2  < &argv[argc]; x2++)
    {
        strcpy(x,p);
        for ( x1 = strtok(x,":"); x1 != (char *) NULL;x1 = strtok(NULL,":"))
        {
            char buf[MAXPATHLEN];
            struct stat s;
            int i;
            sprintf(buf,"%s/%s", x1,*x2);
            if (stat(buf,&s) != -1)
            {
                if (S_ISREG(s.st_mode)
                  && (s.st_mode & (S_IXUSR
#ifndef MINGW32
| S_IXGRP | S_IXOTH
#endif
                    )))
                {
                    printf("%s\n",buf);
                    goto anoth;
                }
            }
        }
        if (x1 == (char *) NULL)
            ret_code = 1;
anoth:
        ;
    }
    exit(ret_code);
}
