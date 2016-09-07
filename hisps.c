/*********************************************************************/

static char * helptext = "hisps.c - dump accounting records that apply to\n\
processes that executed during a time interval.\n\n\
  - if the number of arguments is 1, print all records up to this point\n\
  - if the option d (-d) is supplied, use this date format (the default is\n\
    for the times to be seconds since 1970)\n\
  - if the option s (-s) is supplied, print all records after that time.\n\
  - if the option e (-e) is provided print all records before that time.\n\
  - if the option p (-p) is supplied, stop after designated items output\n\
  - if the option h (-h) is supplied, print out a heading line\n\
  - if the option n (-n) is supplied, output the time in seconds since 1970\n\
  - In all other cases, and in the event of errors, give this help\n\n\
Dates are YYMMDDHH24MISS, in Local Time. Punctuation characters and space are allowed.\n\
Missing seconds give to the minute, minutes to the hour, hours to the day etc..\n\
stdin has the account file (or something of the same format)\n\n\
See /usr/include/sys/acct.h and ucb man 5 acct for an explanation of the fields.\n\
stdout has the fields separated by pipe symbols (|), with the control terminal\n\
device represented as major,minor, and with the accounting flags decoded:\n\
 - F Fork\n\
 - C Core dump\n\
 - X Killed by a signal\n\
 - S Acquired Super User status.\n";

/*********************************************************************/
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (C) 1992 E2 Systems Limited";
#include <sys/param.h>
#include <stdio.h>
#ifndef OSF
#include <sys/sysmacros.h>
#endif
#include <sys/types.h>
#ifndef SPARC_SOL_ON_LINUX
#ifdef LINUX
#include <linux/acct.h>
#else
#include <sys/acct.h>
#endif
#endif
#ifndef V32
#include <sys/time.h>
#endif
#include <time.h>
#include <math.h>
#include <string.h>
#ifndef AHZ
#ifdef HP7
#define AHZ 100.0
#else
#ifdef V4
#define AHZ 100.0
#else
#ifdef POSIX
#define AHZ 100.0
#else
#define AHZ 60.0
#endif
#endif
#endif
#endif
                     /* units of time */
static struct tm *cur_tm;
extern char * ctime();
/*
 * convert the peculiar and historical comp_t time representation into something
 * for the 1980s...
 *
 */
#define comp_t_double(x) ((double) ((((double)(((short int) (x)) & 0x1fff)) *\
 ((double) ((((((short int) (x))&0x2000)?8:1))))* \
 ((double) ((((((short int) (x))&0x4000)?64:1))))* \
 ((double) ((((((short int) (x))&0x8000)?4096:1)))))))

extern double atof();
#ifdef SPARC_SOL_ON_LINUX
typedef unsigned short int comp_t;   /* pseudo "floating point" representation */
                            /* 3 bit base-8 exponent in the high */
                            /* order bits, and a 13-bit fraction */
                            /* in the low order bits. */
#pragma pack(1)
struct	acct
{
    char   ac_flag;    /* Accounting flag */
    char align1;
    char   ac_stat;    /* Exit status */
    char align2;
    int  ac_uid;     /* Accounting user ID */
    int  ac_gid;     /* Accounting group ID */
    int  ac_tty;     /* control tty */
    int  ac_btime;   /* Beginning time */
    comp_t ac_utime;   /* accounting user time in clock ticks */
    comp_t ac_stime;   /* accounting system time in clock ticks */
    comp_t ac_etime;   /* accounting total elapsed time in clock ticks */
    comp_t ac_mem;     /* memory usage in clicks (pages) */
    comp_t ac_io;      /* chars transferred by read/write */
    comp_t ac_rw;      /* number of block reads/writes */
    char   ac_comm[8]; /* command name */
};

/*
 * Accounting Flags
 */

#define AFORK   01    /* has executed fork, but no exec */
#define ASU     02    /* used super-user privileges */
#define ACCTF   0300  /* record type */
#define AEXPND  040   /* Expanded Record Type - default */

/******************************************************************************
 * Reverse bytes in situ
 */
void other_end(inp, cnt)
char * inp;
int cnt;
{
char * outp = inp + cnt -1;
     for (cnt = cnt/2; cnt > 0; cnt--)
     {
         *outp = *outp ^ *inp;
         *inp = *inp ^ *outp;
         *outp = *inp ^ *outp;
         outp--;
         inp++;
     }
     return; 
}
/*
 * Covnvert from SPARC to x86 byte order
 */
void endify_record(inp)
char * inp;
{
    other_end(inp + 4,4);
    other_end(inp + 8,4);
    other_end(inp + 12,4);
    other_end(inp + 16,4);
    other_end(inp + 20,2);
    other_end(inp + 22,2);
    other_end(inp + 24,2);
    other_end(inp + 26,2);
    other_end(inp + 28,2);
    other_end(inp + 30,2);
    return;
}
#endif
/***********************************************************************
 *
 * Getopt support
 */
extern int optind;           /* Current Argument counter.      */
extern char *optarg;         /* Current Argument pointer.      */
extern int opterr;           /* getopt() err print flag.       */
extern int errno;


/*********************************************************************
 *
 * Main program starts here
 *
 */
static char * date_format=(char *) NULL;
                         /* date format expected of data arguments */
main(argc,argv,envp)
int argc;
char ** argv;
char ** envp;
{
#ifdef LINUX
struct acct_v3 acct_stuff;
#else
struct acct acct_stuff;
#endif
unsigned int first_time;
time_t last_time;
double valid_time;       /* needed for the date check */
char * x;                /* needed for the date check */
int c;
int to_do;               /* number to be printed */
enum time_check {DO,DONT};
enum time_check time_check;
int nout_flag = 0;

/*
 * Validate the arguments
 *
 */
    time_check = DONT;
    to_do = 0;
    first_time = 0;
    last_time = time(0);
    cur_tm = gmtime(&last_time);
    if (cur_tm->tm_isdst)
        last_time += 3600;
    while ( ( c = getopt ( argc, argv, "hd:s:e:p:n" ) ) != EOF )
    {
        switch ( c )
        {
        case 'd':
             date_format = optarg;
             break;
        case 'n' :
             nout_flag = 1;
             break;
        case 'e' :
        case 's' :
            time_check = DO;
            if ( date_format != (char *) NULL)
            {
                if ( !date_val(optarg,date_format,&x,&valid_time))
    /*
     * Time argument is not a valid date
     */
                {
                   (void) fwrite(helptext,strlen(helptext),1,stdout);
                   exit(0);
                }
                if (c == 's')
                    first_time = (unsigned int)
                        (valid_time - ((cur_tm->tm_isdst)?3600:0));
                else
                    last_time = (unsigned int)
                        (valid_time - ((cur_tm->tm_isdst)?3600:0));
            }
            else
            {
                if (c == 's')
                    first_time = atoi(optarg);
                else
                    last_time = atoi(optarg);
            }
            break;

        case 'p' :
             to_do = atoi(optarg); 
            break;
        case 'h' :
printf("%-8.8s|%8.8s|%8.8s|%8.8s|%-15.15s|%3.3s|%3.3s|%6.6s|%7.7s|%3.3s,%3.3s|%s\n",
    "Command",         /* Accounting command name */
    "User",  /* Accounting user time */                                    
    "System",  /* Accounting system time */
    "Elapsed",     /* Accounting elapsed time */
    "Start",
    "UID",        /* Accounting user ID */
    "GID",        /* Accounting group ID */
    "Memory",        /* average memory usage/k */
    "IO",         /* disk IO blocks or bytes xferred */
    "Maj",        /* control typewriter (major)*/
    "Min",        /* control typewriter (minor)*/
    "Flag");      /* Accounting flags */
            break;
        default:
        case '?' : /* Default - invalid opt.*/
               (void) fwrite(helptext,strlen(helptext),1,stdout);
               exit(0);
            break;
       }
    }
    while (fread (&acct_stuff, sizeof(acct_stuff), 1, stdin) != 0)
    {
    /*
     * if the time started is less than the target time, and the
     * time finished (= start time plus elapsed time) is greater than the
     * time of interest, print the record
     * 
     * Break out if count limit is reached
     */
#ifdef SPARC_SOL_ON_LINUX
         endify_record((char *) &acct_stuff);
#endif
         switch (time_check)
         {
         char *x, buf[12];
         case DO:
           if (last_time < acct_stuff.ac_btime ||
               first_time > acct_stuff.ac_btime +
#ifdef AIX4
          (int) (comp_t_double(acct_stuff.ac_etime)/(double) AHZ/100.0)
#else
          (int) (comp_t_double(acct_stuff.ac_etime)/(double) AHZ)
#endif
)
                continue;
         default:
             if (!nout_flag)
             {
             time_t t = (time_t) (acct_stuff.ac_btime);
                 x = ctime(&t)+4;   /* Beginning time */
             }
             else
             {
                 sprintf(buf,"%u",acct_stuff.ac_btime); /* Beginning time */
                 x = buf;
             }
             
printf("%-8.8s|%8.2f|%8.2f|%8.2f|%-15.15s|%3.1d|%3.1d|%6.0f|%7.0f|%3.1d,%3.1d|%c%c%c%c\n",
    acct_stuff.ac_comm,                            /* Accounting command name */
    comp_t_double(acct_stuff.ac_utime)/((double) AHZ),  /* User CPU time   */                                    
    comp_t_double(acct_stuff.ac_stime)/((double) AHZ),  /* System CPU time */
#ifdef SPARC_SOL_ON_LINUX
    comp_t_double(acct_stuff.ac_etime)/((double) AHZ),  /* Elapsed time */
#else
#ifdef LINUX
    (double) acct_stuff.ac_etime/100.0, /* Elapsed time */
#else
#ifdef AIX4
    comp_t_double(acct_stuff.ac_etime)/((double) AHZ)/100.0, /* Elapsed time */
#else
    comp_t_double(acct_stuff.ac_etime)/((double) AHZ),  /* Elapsed time */
#endif
#endif
#endif
    x,                                  /* Beginning time      */
    acct_stuff.ac_uid,                  /* Accounting user ID  */
    acct_stuff.ac_gid,                  /* Accounting group ID */
#ifdef SPARC_SOL_ON_LINUX
    ((double) acct_stuff.ac_mem)/(
    comp_t_double(acct_stuff.ac_utime) +
    comp_t_double(acct_stuff.ac_stime) + .000001),
                                        /* average memory usage/k */
    comp_t_double(acct_stuff.ac_io),    /* disk IO blocks or bytes xferred */
#else
#ifdef LINUX
    comp_t_double(acct_stuff.ac_mem),   /* average memory usage */
    comp_t_double(acct_stuff.ac_io),    /* disk IO blocks or bytes xferred */
#else
#ifdef AIX
    ((double) acct_stuff.ac_mem)/((double) AHZ),        /* average memory usage/k */
#else
#ifdef ULTRIX
    ((double) acct_stuff.ac_mem)*4.0,   /* average memory usage/k */
#else
#ifdef V4
    ((double) acct_stuff.ac_mem)/(
    comp_t_double(acct_stuff.ac_utime) +
    comp_t_double(acct_stuff.ac_stime) + .000001),
                                    /* average memory usage/k */
#else
    ((double) acct_stuff.ac_mem)/20.0,  /* average memory usage/k */
#endif
#endif
#endif
    comp_t_double(acct_stuff.ac_io),    /* disk IO blocks or bytes xferred */
#endif
#endif
    major(acct_stuff.ac_tty),           /* control typewriter (major)*/
    minor(acct_stuff.ac_tty),           /* control typewriter (minor)*/
#ifdef LINUX
/*
 * The flags are now an enum, so the ifdef's don't work.
 */
    (char) ((acct_stuff.ac_flag & AFORK) ? 'F' : ' '),  /* Fork flag */
    (char) ((acct_stuff.ac_flag & ASU) ? 'S' : ' ')    /* Super User flag */
#ifdef WHATEVER
    ,(char) ((acct_stuff.ac_flag & ACORE) ? 'C' : ' '),  /* Core dump flag */
    (char) ((acct_stuff.ac_flag & AXSIG) ? 'X' : ' ') /* Killed by signal */
#else
      ,' ', ' '
#endif
           );
#else
#ifdef AFORK
    (char) ((acct_stuff.ac_flag & AFORK) ? 'F' : ' '),  /* Accounting flag */
#else
    (char) ' ',  
#endif   
#ifdef ASU
    (char) ((acct_stuff.ac_flag & ASU) ? 'S' : ' '),    /* Accounting flag */
#else
    (char) ' ',
#endif
#ifdef ACORE
    (char) ((acct_stuff.ac_flag & ACORE) ? 'C' : ' '),  /* Accounting flag */
#else
    (char) ' ',
#endif
#ifdef AXSIG
    (char) ((acct_stuff.ac_flag & AXSIG) ? 'X' : ' ')); /* Accounting flag */
#else
    (char) ' '); 
#endif
#endif

/****** the following are peculiar to Sequent machines ************************/
/*  (short int) acct_stuff.ac_scgacct,        /o account identifier o/
    comp_t_double(acct_stuff.ac_rw),          /o number of disk IO blocks o/
    (short int) acct_stuff.ac_stat);          /o Exit status */

            if (to_do)
            {
                to_do--;
                if (to_do == 0) exit(0);                /* All done */
            }
            break;
        }
    }
    exit(0);
}
