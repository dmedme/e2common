/*****************************************************************************
 * e2sort.c - Sort/Join utilities
 *
 * This file contains:
 * - Implementations of standard routines that are missing, or inadequate, on
 *   some supported operating systems (eg. strcasecmp(), fgets())
 * - Sort/Merge/Join code lifted and extended from the index building routines
 *   of E2 Systems's SQL Analysis product.
 *
 * These are completely general purpose, but their facilities are tuned to the
 * needs of McKeowns View Creation.
 *
 * If one of the alternate main routines (e2_sort() or e2_join()) is defined
 * to be main(), the source can be compiled and linked standalone.
 */
static char * sccs_id = "%E% %D% %T% %E% %U%\n\
Copyright (c) E2 Systems Limited 1999";
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "e2sort.h"
#ifndef WINNT
char * strdup();
char * getenv();
#endif
#if UNIX_WARE | INTEL_DGUX | WINNT
#ifndef E2
#define NEED_STRCASECMP
#endif
#endif
int main();
/*
 * Getopt support
 */
extern int optind;
extern int opterr;
extern char * optarg;
extern int getopt(); 
/*
 * Functions in this file
 */
static void qwork();
static int multi_read();
static int sort_merge_join();
static int arrstrcmp();
/*************************************************************************
 * Static data
 *
 * The low number is for the benefit of NT
 */
#define NO_OF_BUFFERS 12
/*
 * Buffers used to avoid possible memory leakage from the standard I/O library.
 */
static char buf[BUFSIZ];
static char sbuf[BUFSIZ];
static char io_bufs[NO_OF_BUFFERS][BUFSIZ];
static FILE * buffer_channel[NO_OF_BUFFERS];
/******************************************************************************
 * Sort/Merge/Join control structures
 *
 * Includes provision for record structure information, for sort and join
 * - No escape handling (ie. the separator character cannot be escaped)
 * - No output specification (output is all input, less subsequent join fields)
 */
struct buffer_structure {
    char sort_key[BUFSIZ];     /* Enough of a record to include the sort key
                                * when merging disk files                     */
    char sep;                  /* The field separator character               */
    int srt_cnt;               /* The number of sort sub-keys                 */
    int srt_key[10];           /* sort sub-key field numbers (eg. 0, 1, 2)    */
    int max_len;               /* The longest record in the input chain       */
    struct ln_con * anchor;    /* Chain of input records with the same sort
                                * key (used when joining)                     */
    struct ln_con * nxt;       /* Next input record (used when joining)       */
};
static struct buffer_structure buffers[NO_OF_BUFFERS];
/******************************************************************************
 * Windows-specific support.
 * 
 * The compiler flags used below are:
 * - UNIX, really meaning anything except Microsoft Windows (various flavours)
 * - MINGW32, meaning Windows, without the GNU C Library, ie. just CRTDLL.DLL
 * - WINNT, meaning Microsoft C++ Version 4 (ie. McKeowns build environment)
 * - LCC, indicating the lcc free C compiler. 
 * - LCC_DEBUG, indicating the lcc free C compiler, and that super-accurate
 *   gettimeofday() is required.
 * - WIN95, indicating the run-time target is Windows 95 (with its naff clock
 *   granularity)
 */
#ifdef UNIX
#define SLEEP_FACTOR 1
#else
#define SLEEP_FACTOR 1000
#endif
#if MINGW32 | WINNT
#ifdef LCC
#ifndef E2
#define NEED_STRCASECMP
#endif
#define STDCALL __stdcall
/*
 * Pick up _rdtsc(), which returns the clock cycles since boot off a Pentium
 * or higher class processor
 */
#include <intrinsics.h>
#else
#if _MSC_VER
#define STDCALL __stdcall
#else
#define STDCALL __attribute__ ((stdcall))
#endif
#endif
#ifdef DEBUG
/*
 * Stuff lifted from the WINDOWS header files, to avoid including <windows.h> 
 */
typedef long LONG;
typedef long DWORD;
typedef long WORD;
typedef short WCHAR;
typedef struct _FILETIME {
    DWORD dwLowDateTime;   /* low 32 bits  */
    DWORD dwHighDateTime;  /* high 32 bits */
} FILETIME, *PFILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;
typedef struct _TIME_ZONE_INFORMATION {
    LONG       Bias; 
    WCHAR      StandardName[ 32 ]; 
    SYSTEMTIME StandardDate; 
    LONG       StandardBias; 
    WCHAR      DaylightName[ 32 ]; 
    SYSTEMTIME DaylightDate; 
    LONG       DaylightBias; 
} TIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION; 
int STDCALL GetLastError( void);
int STDCALL GetSystemTime( LPSYSTEMTIME lpSystemTime);
int STDCALL SystemTimeToFileTime( const SYSTEMTIME *lpSystemTime,
	LPFILETIME lpFileTime);
DWORD STDCALL
GetTimeZoneInformation( LPTIME_ZONE_INFORMATION lpTimeZoneInformation);
#define TIME_ZONE_ID_DAYLIGHT      2
/*
 * Used for clock tick measurement. A long long is a 64 bit integer.
 */
#ifdef LCC
static long long scale_factor; 
#endif
#ifdef LCC_DEBUG
/*
 * The routines below have the task of improving the granularity of the clock.
 *
 * We have the system clock, which is only updated every half second or so
 * on Windows 95/98, and we have a cycle count since boot time.
 *
 * Over a period of seconds, the two do not agree at all well (or putting it
 * another way, the estimate of the ticks per second is not that good, since
 * we only count 10 seconds worth. The error will be 1/10 of the number of
 * system clock changes per second; on NT, 1/10000; on Windows 95, 1/20!).
 *
 * Thus, the approach is to use the definitive system clock, but to use
 * the tick count to distinguish close (and otherwise indistinguishable)
 * readings.
 *
 * Clock tick calibration
 *
 * Timeval arithmetic functions
 *
 * t3 = t1 - t2
 */
static void tvdiff(t1, m1, t2, m2, t3, m3)
long int * t1;                /* Later timeval seconds          */
long int * m1;                /* Later timeval microseconds     */
long int * t2;                /* Earlier timeval seconds        */
long int * m2;                /* Earlier timeval microseconds   */
long int * t3;                /* Output timeval seconds         */
long int * m3;                /* Output timeval microseconds    */
{
    *t3 = *t1 - *t2; 
    *m3 = *m1 - *m2; 
    if (*m3 < 0)
    {
        *m3 = (*m3) + 1000000;
        (*t3)--;
    }
    return;
}
/*
 * Routine to find out how many machine cycles take place in a 10 second
 * interval. LCC compiler only.
 */
void tick_calibrate()
{
static int done;
long long bef;
long long aft;
struct timeval t1, t2;
struct timezone z;
char * x;
    if (done == 1)
        return;
    done = 1;
    x = getenv("E2TRAF_SCALE");
    if (x != (char *) NULL)
        scale_factor = (long long) atoi(x);
    else
    {
        gettimeofday(&t1, &z);
        bef = _rdtsc();
        sleep( 10 * SLEEP_FACTOR);
        gettimeofday(&t2, &z);
        aft = _rdtsc();
        tvdiff(&(t2.tv_sec),
               &(t2.tv_usec),
               &(t1.tv_sec),
               &(t1.tv_usec),
               &(t1.tv_sec),
               &(t1.tv_usec));
        scale_factor = ((aft - bef) * ((long long) 1000000))/
                        ((((long long) 1000000) * ((long long) t1.tv_sec)) +
                           ((long long) t1.tv_usec)) ;
        x = (char *) malloc(40);
        sprintf(x, "E2TRAF_SCALE=%.23g", (double) scale_factor);
        (void) putenv(x);
    }
    puts(x);
    return;
}
#endif
double floor();
/*
 * Windows implementation of UNIX (BSD originally) gettimeofday()
 */
int gettimeofday(tv,tz)
struct timeval * tv;
struct timezone *tz;
{
static double jan1601 = (double) -11644473600.0;
double secs;
double musec_10;
FILETIME ft;
SYSTEMTIME st;
static TIME_ZONE_INFORMATION tzi;
static int ret_code = -501;
#ifdef LCC
long long bef;
long long aft;
static long long first_long;
static struct timeval first_time;
static struct timeval last_time;
#endif
    if (ret_code == -501)
        ret_code = GetTimeZoneInformation(&tzi);    
    GetSystemTime(&st);
#ifdef LCC
    aft = _rdtsc();
#endif
    if (!SystemTimeToFileTime(&st,&ft))
    {
        tv->tv_sec = GetLastError();
        tv->tv_usec = 0;
        return 0;
    }
#ifdef DEBUG
    printf("%x:%x\n",  ft.dwHighDateTime, ft.dwLowDateTime);
#endif
    musec_10 =(((double) ft.dwHighDateTime) * ((double) 65536.0) *
           ((double) 65536.0) + ((double)((unsigned long) ft.dwLowDateTime))) +
                        jan1601*((double) 10000000.0);
    secs =  floor(musec_10/((double) 10000000.0));
    tv->tv_sec = (int) secs;
    tv->tv_usec = (int) floor((musec_10 -secs*((double) 10000000.0))/
                                 ((double) 10.0));
#ifdef LCC
/*
 * Make a fine grained adjustment to the clock
 */
    if (scale_factor != 0)
    {
#ifdef WIN95
        if (tv->tv_sec - last_time.tv_sec > 1)
#else
        if (tv->tv_sec - last_time.tv_sec > 600)
#endif
        {
            first_time = *tv;
            last_time = first_time;
            first_long = aft;
        }
        else
        {
#ifdef DEBUG
            printf("1 gettimeofday() = %u.%06u\n",  tv->tv_sec, tv->tv_usec);
#endif
            bef = scale_factor * ((long long) (tv->tv_sec - first_time.tv_sec))
           + (scale_factor * ((long long) (tv->tv_usec - first_time.tv_usec)))/
                       ((long long) 1000000);
#ifdef DEBUG
            printf(" gettimeofday() aft %.23g bef %.23g\n",
                     (double) (aft - first_long), (double) bef);
#endif
            bef = aft - first_long - bef;
/*
 * Re-synchronise with the current reading, so that systematic errors do not
 * build up. The ticks are in nanoseconds, but the tick count rate measurement
 * is good to 1 in 10,000 on an NT box, where the clock granularity is about 1
 * millisecond. On a 95/98 box, the clock granularity is about 1/2 a second.
 * So, on NT, we will resynchronise after 2 seconds, and on NT, after 1200
 * seconds. 
 */
            tv->tv_usec = ((long) ((((long long) ( bef % scale_factor))*
                          ((long long) 1000000))/ scale_factor)) + tv->tv_usec;
            tv->tv_sec = tv->tv_sec + ((long) ( bef/scale_factor));
#ifdef DEBUG
            printf("2 gettimeofday() = %u.%06d\n",  tv->tv_sec, tv->tv_usec);
#endif
            if (tv->tv_usec < 0)
            {
                tv->tv_sec--;
                tv->tv_usec = 1000000 + tv->tv_usec;
            }
            else
            if (tv->tv_usec >= 1000000)
            {
                tv->tv_sec++;
                tv->tv_usec =  tv->tv_usec - 1000000;
            }
#ifdef DEBUG
            printf("3 gettimeofday() = %u.%06u\n",  tv->tv_sec, tv->tv_usec);
#endif
            last_time = *tv;
        }
    }
#endif
    tz->tz_minuteswest = tzi.Bias;
    if (ret_code == TIME_ZONE_ID_DAYLIGHT)
        return 1;
    else
        return 0;
}
/*
 * Create a copy of a string in malloc()'ed space
 */
__inline char * strdup(s)
char    *s;                     /* Null-terminated input string */
{
int n = strlen(s) + 1;
char *cp;

    if ((cp = (char *) malloc(n)) == (char *) NULL)
        abort();
    (void) memcpy(cp, s, n);
    return(cp);
}
#endif
#ifndef VCC2003
/****************************************************************************
 * Routines provided because the Microsoft CRTDLL library versions are so slow.
 *
 * Can't redefine these with VCC 2003
 *
 * Length of a string
 */
__inline static unsigned int strlen(x1_ptr)
unsigned char * x1_ptr;                     /* Null-terminated input string */
{
unsigned char * x2_ptr = x1_ptr;

    while (*x2_ptr != '\0')
        x2_ptr++;
    return x2_ptr - x1_ptr;
}
/*
 * Compare two null-terminated strings
 */
__inline static int strcmp(x1_ptr, x2_ptr)
unsigned char * x1_ptr;                     /* Null-terminated input string */
unsigned char * x2_ptr;                     /* Null-terminated input string */
{
    for (; *x1_ptr == *x2_ptr && *x1_ptr != '\0'; x1_ptr++, x2_ptr++);
    if (*x1_ptr > *x2_ptr)
        return 1;
    else
    if (*x1_ptr < *x2_ptr)
        return -1;
    else
        return 0;
}
#endif
/*
 * Output a null-terminated string to a file.
 */
int e2_fputs(x1_ptr, ofp)
const char * x1_ptr;                     /* Null-terminated input string */
FILE * ofp;                              /* Output FILE                  */
{
    if (fwrite(x1_ptr, strlen(x1_ptr), sizeof(char), ofp) > 0)
        return 1;
    else
        return 0;
}
/*
 * Macro to get the next character from a stream. This really is quicker.
 *
 * BUT ONLY IF WE USE MS stdio
 */

#define GETC(fp) ((fp->_cnt-- > 0) ?((int) *(fp->_ptr++)): getc(fp))
/* #define GETC(fp)  getc(fp) */
/*
 * Read a line from a file, or up to the size of the buffer. Null-terminate the
 * result.
 */
__inline char * e2_fgets(buffer, len, ifp)
char * buffer;
int len;
FILE * ifp;
{
char * x1_ptr = buffer;
char * top = buffer + len - 1;
int c;
    while (x1_ptr < top)
    {
        c = GETC(ifp);
        switch(c)
        {
        case EOF:
            if (x1_ptr == buffer)
                return (char *) NULL;
        case '\0':
            goto no_more;
        case '\n':
            *x1_ptr++ = (char) c;
            goto no_more;
        default:
            *x1_ptr++ = (char) c;
            break;
        }
    }
no_more:
    *x1_ptr = '\0';
    return buffer;
}
#endif
#ifdef NEED_STRCASECMP
int strcasecmp(x1_ptr, x2_ptr)
char * x1_ptr;                  /* First null-terminated string to compare  */
char * x2_ptr;                  /* Second null-terminated string to compare */
{
int i;
int j;

    for (;(*x1_ptr != '\0' && *x2_ptr != '\0');
                  x1_ptr++, x2_ptr++)
    {
                              /* Find where they differ */
        if (islower(*x1_ptr))
            i = toupper(*x1_ptr);
        else
            i = *x1_ptr;
        if (islower(*x2_ptr))
            j = toupper(*x2_ptr);
        else
            j = *x2_ptr;
        if (i > j)
            return 1;
        else
        if (j > i)
            return -1;
    }
    if (*x1_ptr != '\0')
        return 1;
    else
    if (*x2_ptr != '\0')
        return -1;
    else
        return 0;
}
#endif
/******************************************************************************
 * fputs() variation that does not output duplicate records.
 *
 * This function supports the sort unique output option.
 *
 * Use a NULL x1_ptr to flag reset.
 *
 * There is only ever one output file open at a time, so the static below
 * is not a problem.
 */
static int fputs_unique(x1_ptr, ofp)
const char * x1_ptr;                     /* Null-terminated input string */
FILE * ofp;                              /* Output FILE                  */
{
static struct ln_con lc;
int i;
    if (x1_ptr == (char *) NULL)
    {
        if (lc.ln != (char *) NULL)
        {
            e2_fputs(lc.ln, ofp);
            free(lc.ln);
            lc.ln = (char *) NULL;
            return 1;
        }
        return 0;
    }
    if (lc.ln == (char *) NULL)
    {
        lc.len = strlen(x1_ptr) + 4096;  /* Usually large enough */
        lc.ln  = (char *) malloc(lc.len);
        strcpy(lc.ln, x1_ptr);
        return 0;
    }
    else
    if (strcmp(x1_ptr, lc.ln))
    {
        e2_fputs(lc.ln, ofp);
        i = strlen(x1_ptr);
        if (i > lc.len)
        {
            free(lc.ln);
            lc.len = i + 4096;  /* Usually large enough */
            lc.ln  = (char *) malloc(lc.len);
        }
        strcpy(lc.ln, x1_ptr);
        return 1;
    }
    return 0;
}
/*
 * Identify the end of a field in the input string.
 * (A doctored strchr() that treats end markers as found rather than not found).
 */
__inline static unsigned char * e2strsep(x1_ptr, sep)
unsigned char * x1_ptr;             /* Input string                       */
unsigned char sep;                  /* End of field marker; line feed, carriage
                                       return and the terminating NUL also
                                       terminate */ 
{
    for (; *x1_ptr != '\0'
       && *x1_ptr != sep
       && *x1_ptr != '\n'
       && *x1_ptr != '\r';
            x1_ptr++);
    return x1_ptr;
}
/*
 * String compare delimited by string, separator or end of line stuff
 */
__inline int strsepcmp(x1_ptr, x2_ptr, sep)
unsigned char * x1_ptr;             /* Input null-terminated string 1    */
unsigned char * x2_ptr;             /* Input null-terminated string 1    */
unsigned char sep;                  /* End of field marker; line feed, carriage
                                       return and the terminating NUL also
                                       terminate */ 
{
    for (; *x1_ptr == *x2_ptr
        && *x1_ptr != '\0'
        && *x1_ptr != sep
        && *x1_ptr != '\n'
        && *x1_ptr != '\r';
            x1_ptr++, x2_ptr++);
    if (*x1_ptr == *x2_ptr)
        return 0;
    switch(*x1_ptr)
    {
    case '\0':
    case '\r':
    case '\n':
do_sep_1:
        switch(*x2_ptr)
        {
        case '\0':
        case '\r':
        case '\n':
do_sep_2:
            return 0;
        default:
            if (*x2_ptr == sep)
                goto do_sep_2;
            return -1;
        }
    default:
        if (*x1_ptr == sep)
            goto do_sep_1;
        switch(*x2_ptr)
        {
        case '\0':
        case '\r':
        case '\n':
do_sep_3:
            return 1;
        default:
            if (*x2_ptr == sep)
                goto do_sep_3;
            if (*x1_ptr > *x2_ptr)
                return 1;
            else
                return -1;
        }
    }
}
/*
 * Read and store an input line. Very long lines are read in pieces, and
 * reassembled. This is used in many places in the code in preference to
 * fgets(). The caller must clean up the returned ln_con.
 *
 * Note that the first part of the input always ends up in buf.
 */
struct ln_con * chain_read( buf, len, fp)
char * buf;                                 /* Initial work buffer to use */
int len;                                    /* Size of work buffer        */
FILE * fp;                                  /* Input FILE                 */
{
struct ln_con * tail;
int i;
char *dbuf;

    if ((e2_fgets(buf, len, fp) == (char *) NULL)
    || ((tail = (struct ln_con *) malloc(sizeof(struct ln_con))) ==
        (struct ln_con *) NULL))
        return (struct ln_con *) NULL;
    tail->nxt = (struct ln_con *) NULL;
    i = strlen(buf);
    if (buf[i - 1] != '\n') 
    {
        len *= 5;
        dbuf = (char *) malloc(len);
        memcpy(dbuf, buf, i);
        for (;;)
        {
            if (e2_fgets(dbuf + i, len - i, fp) == (char *) NULL)
                break;
            i += strlen(dbuf + i);
            if (*(dbuf + i - 1) == '\n')
                break;
            len *= 5;
            dbuf = realloc(dbuf, len); 
        }
        tail->ln = realloc(dbuf, i + 1); 
    }
    else
    {
        tail->ln = (char *) malloc(i + 1);
        memcpy(tail->ln, buf, i);
    }
    tail->len = i + 1;
    tail->ln[i] = '\0';
    return tail;
}
/*
 * Discard an input line set (used when joining).
 */
static void chain_clear(anchor)
struct ln_con * anchor;                     /* Anchor to list of input lines */
{
struct ln_con * tail;
    while ( anchor != (struct ln_con *) NULL)
    {
        tail = anchor;
        anchor = tail->nxt;
        free(tail->ln);
        free((char *) tail);
    }
    return;
}
/*
 * Create an array of pointers for sort purposes, discarding the input read
 * control structures as we go.
 */
static char ** make_array(anchor,n)
struct ln_con * anchor;                     /* Anchor to list of input lines */
int n;                                      /* Number of lines to process    */
{
char **ln = (char **) malloc(sizeof(char *)*n);
struct ln_con * tail;
char **lp;

    if (ln == (char **) NULL)
        return (char **) NULL;
    for (tail = anchor, lp = ln;
             tail != (struct ln_con *) NULL;
                 lp++)
    {
        *lp = tail->ln;
        anchor = tail;
        tail = tail->nxt;
        free((char *) anchor);
    }
    return ln;
}
/*
 * Sort a disk file (fn1) to give a disk file (fn2)
 *
 * Return a row count.
 */
static int sort_file(fn1, fn2, mem, id, unique_flag)
char * fn1;                   /* Input filename                               */
char * fn2;                   /* Output filename                              */
long mem;                     /* Amount of memory that can be used for sort   */
int id;                       /* ID (used to give unique names to work files) */
int unique_flag;              /* If set, discard lines with duplicate keys    */
{
char ** ln;
FILE * fp;
int n;
long so_far;
FILE *ofp;
struct ln_con * anchor, * tail, *nlp;
char **lp;
int f_cnt;
int o_cnt;
char tempname[24]; 

    if (!strcmp(fn1,"-"))
        fp = stdin;
    else
    if ((fp = fopen(fn1, "rb")) == (FILE *) NULL)
    {
        fprintf(stderr, "fopen(%s, rb) failed error %d\n", fn1, errno);
        return 0;
    }
    setvbuf(fp, sbuf, _IOFBF, BUFSIZ);  /* Avoid possible stdio memory leaks */
    n = 0;
    so_far = 0;
    f_cnt = 0;
    o_cnt = 0;
    anchor = (struct ln_con *) NULL;
    while ((nlp = chain_read(buf, sizeof(buf), fp)) != (struct ln_con *) NULL)
    {
        n++;
        so_far += 8 + sizeof(*nlp) + nlp->len; 
        if (anchor == (struct ln_con *) NULL)
        {
            anchor = nlp;
            tail = anchor;
        }
        else
        {
            tail->nxt = nlp;
            tail = tail->nxt;
        }
        if (so_far > mem)
        {
            ln = make_array(anchor, n);
            qwork(ln, n, arrstrcmp);
            sprintf (tempname, E2_SORT_TEMPLATE, id, f_cnt++);
            if ((ofp = fopen(tempname, "wb")) == (FILE *) NULL)
            {
                perror("fopen()");
                e2_fputs("Failed to open ", stderr);
                e2_fputs(tempname, stderr);
                fputc('\n', stderr);
                return 0;
            }
            setvbuf(ofp, buf, _IOFBF, BUFSIZ);
            for (lp = ln; n > 0; lp++, n--)
            {
                if (unique_flag)
                    o_cnt += fputs_unique(*lp, ofp);
                else
                {
                    e2_fputs(*lp, ofp);
                    o_cnt++;
                }
                free(*lp);
            }
            free((char *) ln);
            if (unique_flag)
                o_cnt += fputs_unique((char *) NULL, ofp);
            (void) fclose(ofp);
            so_far = 0;
            anchor = (struct ln_con *) NULL;
        }
    }
    (void) fclose(fp);
    if (n > 0)
    {
        if ((ln = make_array(anchor, n)) == NULL)
            return -1;
        qwork(ln, n, arrstrcmp);
        if (f_cnt == 0)
        {
            if (!strcmp(fn2,"-"))
                ofp = stdout;
            else
                ofp = fopen(fn2, "wb");
        }
        else
        {
            sprintf (tempname, E2_SORT_TEMPLATE, id, f_cnt++);
            ofp = fopen(tempname, "wb");
        }
        if (ofp == (FILE *) NULL)
        {
            perror("sort_file() fopen()");
            return 0;
        }
        setvbuf(ofp, buf, _IOFBF, BUFSIZ);
        for (lp = ln; n > 0; lp++, n--)
        {
            if (unique_flag)
                o_cnt += fputs_unique(*lp, ofp);
            else
            {
                e2_fputs(*lp, ofp);
                o_cnt++;
            }
            free(*lp);
        }
        free((char *) ln);
        if (unique_flag)
            o_cnt += fputs_unique((char *) NULL, ofp);
        (void) fclose(ofp);
    }
/*
 * If there are multiple files, merge them
 */
    if (f_cnt)
        return sort_merge_join(fn2, id, f_cnt, 0, unique_flag);
    return o_cnt;
}
/*
 * Undocumented externally accessible entry point
 */
void e2swork(a1, cnt, cmpfn)
char **a1;                             /* Array of pointers to be sorted      */
int cnt;                               /* Number of pointers to be sorted     */
int (*cmpfn)();                        /* Comparison function (eg. strcmp())  */
{
    qwork(a1, cnt, cmpfn);
    return;
}
#ifdef USE_RECURSE
/******************************************************************************
 * Quicksort implementation
 ******************************************************************************
 * Function to choose the mid value for Quick Sort. Because it is so beneficial
 * to get a good value, the number of values checked depends on the number of
 * elements to be sorted. If the values are already ordered, we do not return a
 * mid value.
 */
static char * pick_mid(a1, cnt, cmpfn)
char **a1;                             /* Array of pointers to be sorted      */
int cnt;                               /* Number of pointers to be sorted     */
int (*cmpfn)();                        /* Comparison function (eg. strcmp())  */
{
char **top; 
char **bot; 
char **cur; 
char * samples[31];          /* Where we are going to put samples */
int i, j;

    bot = a1;
    cur = a1 + 1;
    top = a1 + cnt - 1;
/*
 * Check for all in order
 */
    while (bot < top && cmpfn(*bot, *cur) < 1)
    {
        bot++;
        cur++;
    }
    if (cur > top)
        return (char *) NULL;
                       /* They are all the same, or they are in order */
/*
 * Decide how many we are going to sample.
 */
    if (cnt < 8)
        return *cur;
    samples[0] = *cur;   /* Ensures that we do not pick the highest value */
    if (cnt < (2 << 5))
        i = 3;
    else 
    if (cnt < (2 << 7))
        i = 5;
    else 
    if (cnt < (2 << 9))
        i = 7;
    else 
    if (cnt < (2 << 11))
        i = 11;
    else 
    if (cnt < (2 << 15))
        i = 19;
    else 
        i = 31;
    j = cnt/i;
    cnt = i;
    for (bot = &samples[cnt - 1]; bot > &samples[0]; top -= j)
        *bot-- = *top;
    qwork(bot, cnt, cmpfn);
    top = &samples[cnt - 1];
    cur = &samples[cnt >> 1];
/*
 * Make sure we do not return the highest value
 */
    while(cur > bot && !cmpfn(*cur, *top))
        cur--;
    return *cur;
}
/*
 * Use QuickSort to sort an array of character pointers. This implementation
 * is supposed to be very quick.
 */
static void qwork(a1, cnt, cmpfn)
char **a1;                             /* Array of pointers to be sorted      */
int cnt;                               /* Number of pointers to be sorted     */
int (*cmpfn)();                        /* Comparison function (eg. strcmp())  */
{
char * mid;
char * swp;
char **top; 
char **bot; 

    if ((mid = pick_mid(a1, cnt, cmpfn)) == (char *) NULL)
        return;
    bot = a1;
    top = a1 + cnt - 1;
    while (bot < top)
    {
        while (bot < top && cmpfn(*bot, mid) < 1)
            bot++;
        if (bot >= top)
            break;
        while (bot < top && cmpfn(*top, mid) > 0)
            top--;
        if (bot >= top)
            break;
        swp = *bot;
        *bot++ = *top;
        *top-- = swp;
    }
    if (bot == top && cmpfn(*bot, mid) < 1)
        bot++;
    qwork(a1,bot - a1, cmpfn);
    qwork(bot, cnt - (bot - a1), cmpfn);
    return;
}
#else
/*****************************************************************************
 * Alternative Quick Sort routines. More inline.
 *****************************************************************************
 */
static __inline char ** find_any(low, high, match_word, cmpfn)
char ** low;
char ** high;
char * match_word;
int (*cmpfn)();                        /* Comparison function (eg. strcmp())  */
{
char **guess;
int i;

    while (low <= high)
    {
        guess = low + ((high - low) >> 1);
        i = cmpfn(*guess, match_word);
        if (i == 0)
            return guess + 1; 
        else
        if (i < 0)
            low = guess + 1;
        else
            high = guess - 1;
    }
    return low;
}
static void qwork(a1, cnt, cmpfn)
char **a1;                             /* Array of pointers to be sorted      */
int cnt;                               /* Number of pointers to be sorted     */
int (*cmpfn)();                        /* Comparison function (eg. strcmp())  */
{
char * mid;
char * swp;
char **top; 
char **bot; 
char **cur; 
char **ptr_stack[128];
int cnt_stack[128];
int stack_level = 0;
int i, j, l;

    ptr_stack[0] = a1;
    cnt_stack[0] = cnt;
    stack_level = 1;
/*
 * Now Quick Sort the stuff
 */
    while (stack_level > 0)
    {
        stack_level--;
        cnt = cnt_stack[stack_level];
        a1 = ptr_stack[stack_level];
        top = a1 + cnt - 1;
        if (cnt < 4)
        {
            for (bot = a1; bot < top; bot++)
                 for (cur = top ; cur > bot; cur--)
                 {
                     if (cmpfn(*bot, *cur) > 0)
                     {
                         mid = *bot;
                         *bot = *cur;
                         *cur = mid;
                     }
                 }
            continue;
        }
        bot = a1;
        cur = a1 + 1;
/*
 * Check for all in order
 */
        while (bot < top && cmpfn(*bot, *cur) <= 0)
        {
            bot++;
            cur++;
        }
        if (cur > top)
            continue;
                       /* They are all the same, or they are in order */
        mid = *cur;    /* First out of order value */
        if (cur == top)
        {              /* One needs to be swapped, then we are done? */
            *cur = *bot;   /* This is the highest value */
            *bot = mid;    /* Put the top one back one below */
            cur = bot - 1;
            top = find_any(a1, cur, mid, cmpfn);
            if (top == bot)
                continue;  /* We put it in the right place already */
            while (cur >= top)
                *bot-- = *cur--; /* Shuffle up from where it should be */
            *bot = mid;    /* Now it is going in the right place */
            continue;
        }
        if (cnt > 18)
        {
        unsigned char * samples[31];  /* Where we are going to put samples */

            samples[0] = mid; /* Ensure that we do not pick the highest value */
            l = bot - a1;
            if (cnt < (2 << 5))
                i = 3;
            else 
            if (cnt < (2 << 7))
                i = 5;
            else 
            if (cnt < (2 << 9))
                i = 7;
            else 
            if (cnt < (2 << 11))
                i = 11;
            else 
            if (cnt < (2 << 15))
                i = 19;
            else 
                i = 31;
            if (l >= i && cmpfn(a1[l >> 1], a1[l]) < 0)
            {
                mid = a1[l >> 1];
                bot = a1;
            }
            else
            {
                j = cnt/i;
                for (bot = &samples[i - 1]; bot > &samples[0]; top -= j)
                    *bot-- = *top;
                qwork(&samples[0], i, cmpfn);
                cur = &samples[(i >> 1)];
                top = &samples[i - 1];
                while (cur > &samples[0] && !cmpfn(*cur, *top))
                    cur--;
                mid = *cur;
                top = a1 + cnt - 1;
                if (cmpfn(*(a1 + l), mid) <= 0)
                    bot = a1 + l + 2;
                else
                    bot = a1;
            }
        }
        else
            bot = a1;
        while (bot < top)
        {
            while (bot < top && cmpfn(*bot, mid) < 1)
                bot++;
            if (bot >= top)
                break;
            while (bot < top && cmpfn(*top, mid) > 0)
                top--;
            if (bot >= top)
                break;
            swp = *bot;
            *bot++ = *top;
            *top-- = swp;
        }
        if (bot == top && cmpfn(*bot, mid) < 1)
            bot++;
        if (stack_level > 125)
        {
            qwork(a1, bot - a1, cmpfn);
            qwork(bot, cnt - (bot - a1), cmpfn);
        }
        else
        {
            ptr_stack[stack_level] = a1;
            cnt_stack[stack_level] = bot - a1;
            stack_level++;
            ptr_stack[stack_level] = bot;
            cnt_stack[stack_level] = cnt - (bot - a1);
            stack_level++;
        }
    }
    return;
}
#endif
/*****************************************************************************
 * Alternative main routines.
 *****************************************************************************
 * Sort.
 */
int e2_sort(argc, argv)
int argc;
char ** argv;
{
int ch;
int j;
int id = 0;
int ret_val = 0;
int merge_cnt = 0;
int unique_flag = 0;
static int avail_mem;
char *x;
static char * hlp = "Provide an optional separator (-t)\n\
an optional list of sort keys (-0, -1, -2 ...)\n\
an optional id (-i number)\n\
an optional uniqueness flag (-u). This does not work for files that can't be\n\
sorted in memory with records wider than 16384 bytes.\n\
an optional list of sort keys (-0, -1, -2 ...)\n\
an optional merge flag and file count (-m count) or an input (text) file\n\
and an output file. For merge, the files must be named e2_id_wnn.\n";

     if (avail_mem == 0 &&
       (((x = getenv("E2_MEMORY")) == (char *) NULL) 
      || ((avail_mem = atoi(x)*1024*1024) <= 0)))
        avail_mem = E2_DEFAULT_MEM;

    for (ch = 0; ch < NO_OF_BUFFERS; ch++)
    {
         buffers[ch].sep = '|';
         buffers[ch].srt_cnt = 0;
    }
    optind = 1;
    while ((ch = getopt(argc, argv, "ui:m:ht:0123456789"))
                 != EOF)
    {
        switch (ch)
        {
        case 'm':
            merge_cnt = atoi(optarg);
            break;
        case 'i':
            id = atoi(optarg);
            break;
        case 'u':
            unique_flag = 1;
            break;
        case 't':
            buffers[0].sep = *optarg;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            buffers[0].srt_key[buffers[0].srt_cnt] = ch - 48;
            buffers[0].srt_cnt++;
            break;
        case 'h':
        default:
            e2_fputs( hlp, stderr);
            if (main == e2_sort)
                exit(1);
            else
                return 0;
        }
    }
    for (ch = 1; ch < NO_OF_BUFFERS; ch++)
    {
         buffers[ch].sep = buffers[0].sep;
         buffers[ch].srt_cnt = buffers[0].srt_cnt;
         for (j = 0; j < 10; j++)
             buffers[ch].srt_key[j] = buffers[0].srt_key[j];
    }
    if (merge_cnt == 0 && argc - optind > 1)
        ret_val = sort_file(argv[optind], argv[optind+1], avail_mem, getpid(),
                 unique_flag);
    else
    if (merge_cnt != 0 && argc - optind > 0)
        ret_val = sort_merge_join(argv[optind], id, merge_cnt, 0, unique_flag);
    if (main == e2_sort)
    {
        if (!ret_val)
            exit(1);
        else
            exit(0);
    }
    else
        return ret_val;
}
/******************************************************************************
 * Join
 */
int e2_join(argc, argv)
int argc;
char ** argv;
{
int ch;
int j;
int i;
int ret_val = 0;
int outer_flag;
static char * hlp = "Provide an optional separator (-t), an optional all\n\
indicator (-a) and a list of join keys (eg. -0 1 -1 0 ...). The number of\n\
files to join is inferred from the number of join specs. We can handle up to\n\
10! Finally, an output file\n";

    for (ch = 0; ch < NO_OF_BUFFERS; ch++)
    {
         buffers[ch].sep = '|';
         buffers[ch].srt_cnt = 0;
         buffers[ch].anchor = (struct ln_con *) NULL;
         buffers[ch].nxt = (struct ln_con *) NULL;
    }
    j = 0;
    outer_flag = 0;
    optind = 1;
    while ((ch = getopt(argc, argv, "hat:0:1:2:3:4:5:6:7:8:9:"))
                 != EOF)
    {
        switch (ch)
        {
        case 't':
            buffers[0].sep = *optarg;
            break;
        case 'a':
            outer_flag = 1;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ch -= 48;
            buffers[ch].srt_key[0] = atoi(optarg);
            buffers[ch].srt_cnt = 1;
            if (ch >= j)
                j = ch + 1;
            break;
        case 'h':
        default:
            e2_fputs( hlp, stderr);
            if (main == e2_join)
                exit(1);
            else
                return 0;
        }
    }
    for (ch = 1; ch < j; ch++)
         buffers[ch].sep = buffers[0].sep;
/*
 * Open the files to be joined
 */
    for (ch = 0; ch < j; ch++, optind++)
    {
        if (!strcmp(argv[optind],"-"))
            buffer_channel[ch] = stdin;
        else
        if ((buffer_channel[ch] = fopen(argv[optind], "rb"))
                     == (FILE *) NULL)
        {
            perror("Opening join input");
            e2_fputs("Failed to open ", stderr);
            e2_fputs(argv[optind], stderr);
            fputc('\n', stderr);
            if (main == e2_join)
                exit(1);
            else
                return 0;
        }
        setvbuf(buffer_channel[ch], io_bufs[ch], _IOFBF, BUFSIZ);
        if (!multi_read(buffer_channel[ch], &buffers[ch]))
        {
            if (!outer_flag)
            {           
                for (i = 0; i <= ch; i++)
                {
                     if (i != ch)
                     {
                         chain_clear(buffers[i].anchor);
                         if ((long int) buffers[i].nxt != EOF)
                             chain_clear(buffers[i].nxt);
                     }
                     fclose(buffer_channel[i]);
                }
                if (main == e2_join)
                    exit(1);
                else
                    return 0;        /* No matches possible! */
            }
            else
            {
                fclose(buffer_channel[ch]);
                j--;
                for (i = ch; i < j; i++)
                {
                    buffers[i].srt_key[0] = buffers[i+1].srt_key[0];
                    buffer_channel[i] = buffer_channel[i+1];
                }
                ch--;
            }
        }
    }
    if (j)
        ret_val = sort_merge_join(argv[optind], getpid(), j, 1, outer_flag);
    else
        ret_val = -1;
    if (main == e2_join)
    {
        if (!ret_val)
            exit(1);
        else
            exit(0);
    }
    else
        return ret_val;
}
/******************************************************************************
 * Code to support the merge or join of lists of files.
 ******************************************************************************
 * Routines to compare two input strings using the buffer_structure sort key
 * definitions.
 *
 * Returns: 1, first greater than second
 *          0, the two are equal
 *          -1, the first is less than the second
 */
static int arrstrcmp(x1, x2)
unsigned char * x1;                 /* First null-terminated input string  */
unsigned char * x2;                 /* Second null-terminated input string */
{
int i, j, k;
unsigned char * x1_ptr;
unsigned char * x2_ptr;

    if (!(buffers[0].srt_cnt))
        return strcmp(x1, x2);
    for (k = 0; k < buffers[0].srt_cnt; k++)
    {
        if (!k || (buffers[0].srt_key[k] < buffers[0].srt_key[k-1]))
        {
            j = 0;
            x1_ptr = x1;
            x2_ptr = x2;
        }
        else
            j = buffers[0].srt_key[k-1];
        for (i=j; i < buffers[0].srt_key[k]; i++)
        {
            x1_ptr = e2strsep(x1_ptr,buffers[0].sep);
            if (*x1_ptr)
                x1_ptr++;
            x2_ptr = e2strsep(x2_ptr,buffers[0].sep);
            if (*x2_ptr)
                x2_ptr++;
        }
        if (j = strsepcmp(x1_ptr, x2_ptr, buffers[0].sep))
            return j;
    }
    return 0;
}
/*
 * Compare two buffer structures.
 */
static int bufbufcmp(b1, b2)
struct buffer_structure * b1;           /* First buffer structure  */
struct buffer_structure * b2;           /* Second buffer structure */
{
int i, j, k;
unsigned char * x1_ptr;
unsigned char * x2_ptr;

    if (!(b1->srt_cnt))
        return strcmp((unsigned char *) &(b1->sort_key[0]), 
                  (unsigned char *) &(b2->sort_key[0]));
    for (k = 0; k < b1->srt_cnt; k++)
    {
        if (!k || (b1->srt_key[k] < b1->srt_key[k-1]))
        {
            j = 0;
            x1_ptr = (unsigned char *) &(b1->sort_key[0]);
        }
        else
            j = b1->srt_key[k-1];
        for (i=j; i < b1->srt_key[k]; i++)
        {
            x1_ptr = e2strsep(x1_ptr,b1->sep);
            if (*x1_ptr)
                x1_ptr++;
        }
        if (!k || (b2->srt_key[k] < b2->srt_key[k-1]))
        {
            j = 0;
            x2_ptr = (unsigned char *) &(b2->sort_key[0]);
        }
        else
            j = b2->srt_key[k-1];
        for (i=j; i < b2->srt_key[k]; i++)
        {
            x2_ptr = e2strsep(x2_ptr,b1->sep);
            if (*x2_ptr)
                x2_ptr++;
        }
        if (j = strsepcmp(x1_ptr, x2_ptr, b1->sep))
            return j;
    }
    return 0;
}
/*
 * Compare a buffer structure to another string, formatted the same way.
 * This is used to identify duplicate keys on a channel.
 */
static int bufstrcmp(b1, b2_ptr)
struct buffer_structure * b1;           /* Buffer structure       */
unsigned char * b2_ptr;                 /* Null-terminated string */
{
int i, j, k;
unsigned char * x1_ptr, *x2_ptr;

    if (!(b1->srt_cnt))
        return strcmp((unsigned char *) &(b1->sort_key[0]), b2_ptr);
    for (k = 0; k < b1->srt_cnt; k++)
    {
        if (!k || (b1->srt_key[k] < b1->srt_key[k-1]))
        {
            j = 0;
            x1_ptr = (unsigned char *) &(b1->sort_key[0]);
            x2_ptr = b2_ptr;
        }
        else
            j = b1->srt_key[k-1];
        for (i=j; i < b1->srt_key[k]; i++)
        {
            x1_ptr = e2strsep(x1_ptr,b1->sep);
            if (*x1_ptr)
                x1_ptr++;
            x2_ptr = e2strsep(x2_ptr,b1->sep);
            if (*x2_ptr)
                x2_ptr++;
        }
        if (j = strsepcmp(x1_ptr, x2_ptr, b1->sep))
            return j;
    }
    return 0;
}
/****************************************************************************
 * lconcat - routine to join two lines and insert a separator, or to strip
 * a line of trailing white space.
 *
 * We only allow one match key for joins.
 *
 * If we are joining, the match-key from the second string is discarded.
 */
void lconcat(x,y,z)
unsigned char * x;                  /* Input/output null-terminated string    */
struct buffer_structure * y;        /* Buffer structure that controls joining */
unsigned char *z;                   /* Null-terminated string to be joined    */
{
unsigned char * x1 = x + strlen(x) - 1;
unsigned char * x2;
int i;
    for (;x1 >= x && (*x1 == ' ' || *x1 == '\n' || *x1 == '\r' || *x == '\t');
                x1--);
    x1++;
    if (y != (struct buffer_structure *) NULL)
    {
        x2 = e2strsep(z, y->sep);
        if (y->srt_key[0] != 0)
        {
            for (i = 1; i < y->srt_key[0]; i++)
            {
                if (*x2)
                    x2++;
                x2 = e2strsep(x2, y->sep);
            }
            if (x2 != z)
            {
                *x1++ = y->sep;
                memcpy(x1, z, x2 - z);
                x1 += (x2 - z);
            }
            if (*x2)
                x2++;
            x2 = e2strsep(x2, y->sep);
        }
        strcpy(x1,x2);
    }
    else
        *x1 = '\0';
    return;
}
/*
 * Function to read all the lines from a file that have the same key, and
 * chain them to the buffer structure. Used for joins.
 */
static int multi_read(fp, bp)
FILE * fp;                                  /* Input FILE                   */
struct buffer_structure * bp;               /* Associated buffer_structure  */
{
struct ln_con * tail;
/*
 * Clean up the previous chain
 */
    chain_clear(bp->anchor);
    bp->max_len = 0;
    if ((long int) (bp->nxt) == EOF)
    {
        bp->anchor = (struct ln_con *) NULL;
        return 0;
    }
    else
    if (bp->nxt == (struct ln_con *) NULL)
    {
        if ((bp->nxt = chain_read(bp->sort_key, sizeof(bp->sort_key), fp))
            == (struct ln_con *) NULL)
        {
             bp->anchor = (struct ln_con *) NULL;
             return 0;
        }
    }
    bp->anchor = bp->nxt;
    tail = bp->anchor;
    bp->max_len = tail->len;
    for (;;)
    {
        bp->nxt = chain_read(bp->sort_key, sizeof(bp->sort_key), fp);
        if (bp->nxt == (struct ln_con *) NULL)
        {
            strncpy(bp->sort_key, bp->anchor->ln, sizeof(bp->sort_key) - 1);
            bp->sort_key[sizeof(bp->sort_key) - 1] = '\0';
            bp->nxt = (struct ln_con *) EOF;
            return 1;
        } 
        if (bufstrcmp(bp, bp->anchor->ln))
        {
            strncpy(bp->sort_key, bp->anchor->ln, sizeof(bp->sort_key) - 1);
            bp->sort_key[sizeof(bp->sort_key) - 1] = '\0';
            return 1;
        }
        tail->nxt = bp->nxt;
        tail = bp->nxt;
        if (tail->len > bp->max_len)
            bp->max_len = tail->len;
    }
}
/*
 * Do a join write; all the matching records are joined with each other and
 * written out. Up to NO_OF_BUFFERS items can be joined and written out at once.
 *
 * The logic can actually do a multi-way simultaneously outer join, but for
 * our purposes we only want the outer join if the first file in the list has
 * a matching value.
 */
static int multi_write(fp, no_of_files, bpp)
FILE *fp;                                   /* Output FILE                   */
int no_of_files;                            /* Number of input streams       */
struct buffer_structure **bpp;              /* Array of input stream control 
                                             * structures   */
{
struct ln_con * lp[NO_OF_BUFFERS];
int i, bs, max_len;
int o_cnt = 0;
char * x = (char *) NULL;
/*
 * For our purposes, we do not want any output if the first file hasn't got
 * a value.
 */
    if (bpp[0] == (struct buffer_structure *) NULL)
        return 0;
    for (i = 0, max_len = 0, bs = 0; i < no_of_files; i++)
    {
        if (bpp[i] != (struct buffer_structure *) NULL)
        {
            lp[i] = bpp[i]->anchor;
            if (lp[i] == (struct ln_con *) NULL)
                 fprintf(stderr,
                      "Logic Error: buffer %d key %s is closed!\n",
                                i,
                             bpp[i]->srt_key);
            max_len += bpp[i]->max_len;
        }
        else
        if (bs == i)
            bs++;
    }
    if (bs >= no_of_files)
        return o_cnt;
    if (max_len > sizeof(bpp[0]->sort_key))
    {
        x = (char *) malloc(max_len + 1);
        strcpy(x, lp[bs]->ln);
    }
    else
        x = bpp[bs]->sort_key;
    for (;;)
    {
        for ( i = bs + 1 ; i < no_of_files; i++)
            if (bpp[i] != (struct buffer_structure *) NULL
              && lp[i] != (struct ln_con *) NULL)
                lconcat(x, bpp[i], lp[i]->ln);
        e2_fputs(x, fp);
        o_cnt++;
        for (i = bs;;)
        {
            if (lp[i] != (struct ln_con *) NULL) 
                lp[i] = lp[i]->nxt;
            if (lp[i] == (struct ln_con *) NULL) 
            {
                if (i == no_of_files - 1)
                {
                    if (max_len > sizeof(bpp[bs]->sort_key))
                        free(x);
                    return o_cnt;
                }
                lp[i] = bpp[i]->anchor;
                for (i++;
                         i < no_of_files
                      && bpp[i] == (struct buffer_structure *) NULL;
                           i++);
                if (i == no_of_files)
                {
                    if (max_len > sizeof(bpp[bs]->sort_key))
                        free(x);
                    return o_cnt;
                }
            }
            else
                break;
        }
        strcpy(x, lp[bs]->ln);
    }
}
/*
 * Routine to do the sort/merge. Return rows written to the final output file
 * if successful, -1 otherwise.
 */
static int sort_merge_join(ofn, id, no_of_files, join_flag, outer_flag)
char * ofn;                  /* File name for final output                   */
int id;                      /* Needed to distinguish sort file families     */
int no_of_files;             /* Number of temporary files to merge           */
int join_flag;               /* zero, do a sort; non-zero, do a join         */ 
int outer_flag;              /* zero, do an inner join; non-zero, an outer   */
                             /* OR (sort) zero, non-unique; non-zero, unique */
{
int o_cnt;
int i;
struct ln_con * extra;
char  tempname[24];   /* name of file to create */
int next_buffer;
FILE * output_channel;

unsigned char eof_buffers;
char    rewrite_offset;

struct buffer_structure * work_buffer, *buffer_order[NO_OF_BUFFERS],
      *join_order[NO_OF_BUFFERS],
      **sort_area = buffer_order, **curr_sort;

unsigned int  next_id, new_no_of_files;

/*
 * Loop - process files to be merged until they have all been dealt with
 */
    new_no_of_files = 0;
    next_id = 0;
    for (;;)
    {
/*******************************************************************************
 * An output file, named by the number of such merge files that have been
 * created, is opened for output.
 *
 * The merge works as follows.
 *
 *     -   Each buffer has associated with it a control structure
 *         which has space for a single key (16384 characters),
 *         and the input channel.
 *
 * To initialise, a line is read from each of the input streams.
 *
 * The words are then sorted.
 *
 * The main loop works as follows.
 *
 * The output record is written out.
 *
 * If we are doing a join, depending on the outer flag, either nothing is
 * written out, or all matching elements are concatenated and written out.
 *
 * The next line is read on each of the channels which matched.
 *
 * The merge has finished when all the channels have returned EOF.
 *
 * The lines are once more re-ordered.
 *
 * - Obtain as many input files as possible, up to the maximum number of input
 *   files allowed.
 * - For Join, files are already open
 */
        if (!join_flag)
        {
/*
 * Sort
 */
            for (next_buffer = 0,
                 curr_sort = sort_area,
                 work_buffer = buffers;
    
                    (next_buffer < NO_OF_BUFFERS);
    
                        next_buffer++, work_buffer++, curr_sort++)
            {
/*
 * The logic is:
 *  -    Work out what the next input filename is.
 *  -    Open it.
 *  -    Attempt to read the first line on the input file.
 *  -    Decrement the next_buffer pointer if it is empty
 *       (should never happen).
 *  -    Break from the loop when all done
 */
                if (next_id == no_of_files)
                {    /* if we have exhausted the current list of input files,
                       start a new sequence of input files */
                    no_of_files = new_no_of_files;
                    new_no_of_files = 0;
                    next_id = 0;
                } 
                if (next_id == no_of_files)
                    break;
                sprintf (tempname, E2_SORT_TEMPLATE, id, next_id++);
                if ((buffer_channel[next_buffer] = fopen(tempname, "rb"))
                             == (FILE *) NULL)
                {
                    perror("Re-opening official output");
                    e2_fputs("Failed to open ", stderr);
                    e2_fputs(tempname, stderr);
                    fputc('\n', stderr);
                    return 0;
                }
#ifdef UNIX
                unlink(tempname);
#endif
                setvbuf(buffer_channel[next_buffer], io_bufs[next_buffer],
                         _IOFBF, BUFSIZ);
#ifdef DEBUG
                fprintf(stderr, "fopen %x : %d\n", buffer_channel[next_buffer],
                     fileno( buffer_channel[next_buffer]));
#endif
                if (e2_fgets(buffers[next_buffer].sort_key,
                     sizeof(buffers[next_buffer].sort_key),
                     buffer_channel[next_buffer]) == (char *) NULL)
                {
#ifdef DEBUG
                    fprintf(stderr, "fclose %x : %d\n",
                            buffer_channel[next_buffer],
                            fileno( buffer_channel[next_buffer]));
#endif
                    fclose(buffer_channel[next_buffer]);
                    next_buffer--;
                    work_buffer--;
                    curr_sort--;
                }
                else
                    *curr_sort = work_buffer;
            }
        }
        else
        {
/*
 * Join
 */
            for (next_buffer = 0,
                 curr_sort = sort_area,
                 work_buffer = buffers;
    
                    (next_buffer < no_of_files);
    
                        next_buffer++, work_buffer++, curr_sort++)
                *curr_sort = work_buffer;
        }
/*
 * At this point, we have got our channels allocated, and each buffer has one
 * word in it. Now complete the merge for these files.
 */
        if (!strcmp(ofn,"-"))
            output_channel = stdout;
        else
        if ((output_channel = fopen (ofn, "wb")) == (FILE *) NULL)
        {                                   /* Open the output file       */
            perror("Creating new merge output");
            fprintf(stderr, "Failed to open %s error: %d\n", ofn, errno);
            return 0;
        }
        setvbuf(output_channel, buf, _IOFBF, BUFSIZ);
        o_cnt = 0;                          /* Count of records written   */
#ifdef DEBUG
        fprintf(stderr, "fopen %x : %d\n", output_channel,
                     fileno( output_channel));
#endif
        eof_buffers = next_buffer;          /* Non-exhausted buffer count */

        while (eof_buffers > 1)
        {    /* loop; until all the input files are exhausted */
/*******************************************************************************
 * Sort the lines in the buffers.
 */
            qwork(sort_area, eof_buffers, bufbufcmp);
/*
 *  At this point, the sort area is occupied by
 *  -    The lowest word, which is ready to be written out.
 *  -    A number of possibly matching elements
 *
 * What happens next depends on whether we are doing a sort or a join.
 *
 * If we are doing a sort, all matching lines are written out.
 * If we are doing a join, all matching lines are concatenated and written
 * out. If it is not an outer join, only lines for which all match, are written
 * out.
 */
            rewrite_offset = 0;
/*
 *  Now we come to the item that is to be written out
 */
            join_order[*sort_area - buffers] =*sort_area;
            for (curr_sort = sort_area + 1;
                    curr_sort < sort_area + eof_buffers
                 && !bufbufcmp(*curr_sort, *sort_area);
                         curr_sort++)
            {
                next_buffer = *curr_sort - buffers;
                if (!join_flag)
                {
/*
 * Sort
 * ====
 * This algorithm doesn't work for very wide lines, because fputs_unique()
 * does not make provision for extra bits. It does not matter, since 16384 is
 * plenty for our purposes. Re-working to use chain_read() instead of fgets()
 * throughout will slow the program down to no useful purpose.
 */
                    if (outer_flag)
                        o_cnt += fputs_unique((*curr_sort)->sort_key,
                                                   output_channel);
                    else
                    {
                        o_cnt++;
                        i = strlen((*curr_sort)->sort_key);
                        fwrite((*curr_sort)->sort_key, i, sizeof(char),
                              output_channel);
/*
 * For very long lines, read and write the rest of the line
 */
                        if ((*curr_sort)->sort_key[i - 1] != '\n')
                        {
                            if ((extra = chain_read((*curr_sort)->sort_key,
                                        sizeof((*curr_sort)->sort_key),
                                        buffer_channel[next_buffer]))
                                  != (struct ln_con *) NULL)
                            {
                                fwrite(extra->ln, extra->len - 1, sizeof(char),
                                       output_channel);
                                free(extra->ln);
                                free((char *) extra);
                            }
                        }
                    }
                }
                else
                    join_order[next_buffer] =*curr_sort;
                if (!join_flag && e2_fgets(buffers[next_buffer].sort_key,
                     sizeof(buffers[next_buffer].sort_key),
                     buffer_channel[next_buffer]) == (char *) NULL)
                {
/*
 * Sort - read next line.
 */
#ifdef DEBUG
                    fprintf(stderr, "fclose %x : %d\n",
                         buffer_channel[next_buffer],
                         fileno( buffer_channel[next_buffer]));
#endif
                    fclose(buffer_channel[next_buffer]);
                    rewrite_offset++;
                }
                else
                if (rewrite_offset)
                    *(curr_sort - rewrite_offset) = *curr_sort;
            }
/*
 * If we have exhausted an input file, shuffle the remainder down.
 */
            if (rewrite_offset)
                while( curr_sort < sort_area + eof_buffers)
                {
                    *(curr_sort - rewrite_offset) = *curr_sort;
                    curr_sort++;
                }
            if (!join_flag)
            {
/*
 * Sort
 */
                next_buffer = *sort_area - buffers; /* units of struct
                                                           size */
                if (outer_flag)
                    o_cnt += fputs_unique((*sort_area)->sort_key,
                                            output_channel);
                else
                {
                    o_cnt++;
                    i = strlen((*sort_area)->sort_key);
                    fwrite((*sort_area)->sort_key, i, sizeof(char),
                                  output_channel);
/*
 * Handle very wide records; read in the rest of the record.
 */
                    if ((*sort_area)->sort_key[i - 1] != '\n')
                    {
                        if ((extra = chain_read((*sort_area)->sort_key,
                                        sizeof((*sort_area)->sort_key),
                                        buffer_channel[next_buffer]))
                                  != (struct ln_con *) NULL)
                        {
                            fwrite(extra->ln, extra->len - 1, sizeof(char),
                                       output_channel);
                            free(extra->ln);
                            free(extra);
                        }
                    }
                }
                eof_buffers -= rewrite_offset;
                if ( e2_fgets(buffers[next_buffer].sort_key,
                         sizeof(buffers[next_buffer].sort_key),
                         buffer_channel[next_buffer]) == (char *) NULL)
                {
#ifdef DEBUG
                    fprintf(stderr, "fclose %x : %d\n",
                         buffer_channel[next_buffer],
                         fileno( buffer_channel[next_buffer]));
#endif
                    fclose(buffer_channel[next_buffer]);
                    eof_buffers--;
                    for (rewrite_offset = 0;
                           rewrite_offset < eof_buffers;
                               rewrite_offset++)
                        buffer_order[rewrite_offset] =
                                  buffer_order[rewrite_offset + 1];
                }
            }
            else
            {
/*
 * Join
 */
                if (outer_flag)
                {
                    while (curr_sort < sort_area + eof_buffers)
                    {
                        join_order[*curr_sort - buffers] =
                                (struct buffer_structure *) NULL;
                        curr_sort++;
                    }
                    o_cnt += multi_write(output_channel,
                                         no_of_files, &join_order[0]);
                }
                else
                {
                    if ( curr_sort == sort_area + eof_buffers)
                         o_cnt += multi_write(output_channel,
                                         no_of_files, &join_order[0]);
                    else
                    {
                        while (curr_sort < sort_area + eof_buffers)
                        {
                            join_order[*curr_sort - buffers] =
                                    (struct buffer_structure *) NULL;
                            curr_sort++;
                        }
                    }
                }
                for (curr_sort = sort_area;
                        curr_sort < sort_area + eof_buffers;
                            curr_sort++)
                {
                    next_buffer = *curr_sort - buffers;
                    if (join_order[next_buffer]
                              != (struct buffer_structure *) NULL)
                    {
                        if (!multi_read(buffer_channel[next_buffer],
                                             *curr_sort))
                        {
                            if (!outer_flag)
                                break;           /* No more matches possible */
                            fclose(buffer_channel[next_buffer]);
                            join_order[next_buffer] =
                                          (struct buffer_structure *) NULL;
                            rewrite_offset++;
                        }
                        else
                        if (rewrite_offset)
                            *(curr_sort - rewrite_offset) = *curr_sort;
                    }
                    else
                    if (rewrite_offset)
                        *(curr_sort - rewrite_offset) = *curr_sort;
                }
                eof_buffers -= rewrite_offset;
/*
 * Handle EOF in the non-outer-join case
 */
                if (curr_sort < (sort_area + eof_buffers))
                {
                    fclose(output_channel);
/*
 * We make use of the fact that this must be the first file to become exhausted.
 */
                    for (next_buffer = 0;
                            next_buffer < eof_buffers;
                                next_buffer++)
                    {
                         chain_clear(buffers[next_buffer].anchor);
                         if ((long int) buffers[next_buffer].nxt != EOF)
                             chain_clear(buffers[next_buffer].nxt);
                         fclose(buffer_channel[next_buffer]);
                    }
                    return o_cnt;
                }
            }
        }                /* End-loop for Merge */
/*
 * If only one buffer left, and sort, or outer join and the first file is the
 * one that is left, copy across. This 'optimisation' was inherited from the 
 * the E2 Systems SQL Analysis Module Index Management code. It should speed
 * things up, but if the lines are > 16384 characters wide, the returned sort
 * line count is over-stated.
 */
        if (eof_buffers == 1 &&
         (!join_flag || ( outer_flag && *sort_area == &buffers[0])))
        {
        FILE * temp_channel;

            next_buffer = *sort_area - buffers;
            temp_channel = buffer_channel[next_buffer];
            if (join_flag)
            {
            struct ln_con * tail;
/*
 * Join
 */
                while ( buffers[next_buffer].anchor != (struct ln_con *) NULL)
                {
                    tail = buffers[next_buffer].anchor;
                    buffers[next_buffer].anchor = tail->nxt;
                    o_cnt++;
                    e2_fputs(tail->ln, output_channel);
                    free(tail->ln);
                    free((char *) tail);
                }
                tail = buffers[next_buffer].nxt;
                if ((tail  != (struct ln_con *) NULL)
                 && (((long int) tail) != EOF))
                {
                    o_cnt++;
                    e2_fputs(tail->ln, output_channel);
                    free(tail->ln);
                    free((char *) tail);
                }
            }
            else
            {
/*
 * Sort
 */
                if (outer_flag)
                    o_cnt += fputs_unique((*sort_area)->sort_key,
                                           output_channel);
                else
                {
                    o_cnt++;
                    e2_fputs((*sort_area)->sort_key, output_channel);
                }
            }
            while (e2_fgets((*sort_area)->sort_key,
                     sizeof((*sort_area)->sort_key),
                     temp_channel) != (char *) NULL)
            {
                if (!join_flag && outer_flag)
                    o_cnt += fputs_unique((*sort_area)->sort_key,
                                           output_channel);
                else
                {
                    o_cnt++;
                    e2_fputs((*sort_area)->sort_key, output_channel);
                }
            }
#ifdef DEBUG
            fprintf(stderr, "fclose %x : %d\n", temp_channel,
                     fileno( temp_channel));
#endif
            fclose (temp_channel);
        }    /* End of single buffer processing */
#ifdef DEBUG
        fprintf(stderr, "fclose %x : %d\n", output_channel,
                     fileno( output_channel));
#endif
        if (!join_flag && outer_flag)
            o_cnt += fputs_unique((char *) NULL, output_channel);
        fclose (output_channel);    /* close the output file */
/*
 * Join only makes one pass. Exit the Sort when all the temporary files have
 * been done.
 */
        if (join_flag
         || (no_of_files <= NO_OF_BUFFERS
           && next_id >= no_of_files
           && new_no_of_files == 0))
            break;
        sprintf (tempname, E2_SORT_TEMPLATE, id, new_no_of_files++);
        unlink(tempname);
        rename (ofn,tempname);
    }    /* End of loop to create one file from all of the existing files */
    return o_cnt;
}
