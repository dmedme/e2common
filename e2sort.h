/*
 * e2sort.h - common definitions for the E2 sort/join routines
 * @(#) $Name$ $Id$
 * Copyright (c) E2 Systems Limited 1999
 */
#ifndef E2_SORT_H
#define E2_SORT_H
/*******************************************************************************
 * Routines and data structures used across source files
 */
void lconcat();           /* Strip trailing white space/join a record         */
int strsepcmp();          /* Compare two delimited input fields               */
int e2_sort();                    /* General purpose text file sort           */
int e2_join();                    /* General purpose text file join           */
int gettimeofday();               /* UNIX gettimeofday()                      */
/*
 * Structure to link together input lines
 */
struct ln_con {
    char * ln;                       /* The line itself (malloc()'ed space */
    int len;                         /* The line length                    */
    struct ln_con *nxt;              /* Pointer to the next input line     */
};
struct ln_con * chain_read();        /* Read an arbitrarily long text line */
/******************************************************************************
 * Build-environment specifics
 */
#ifdef WINNT
char *e2_fgets();
int   e2_fputs();
#else
#define __inline
#define e2_fgets fgets
#define e2_fputs fputs
#endif
#ifdef HP9000
#define const
#else
#ifndef AIX4
struct timezone {
int tz_minuteswest;
};
#ifndef SEQUENT_ELF
struct timeval {
long tv_sec;
long tv_usec;
};
#endif
#endif
#endif
#ifdef BUFSIZ
#undef BUFSIZ
#define BUFSIZ 16384
#endif
/******************************************************************************
 * Filenames and filename templates
 */
#define E2_SORT_TEMPLATE "e2_%x_w%d"
#define E2_DEFAULT_MEM 128*1024*1024
#endif
