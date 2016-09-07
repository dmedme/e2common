/*
 * matchlib.h
 * Set up of things to look for
 * ============================
 *
 * There are a series of 'slots' that can be allocated;
 * [A-Z][0-9A-Z]* with some gaps.
 * - A is reserved for a setup event.
 * - S is reserved for the start event
 * - F is reserved for the finish event
 * - Z is reserved for abort
 *
 * Dynamic allocation; need to know what each means. Therefore
 * use the A event to record the setting of an event to look for.
 *
 * Potential events have:
 * - a slot number
 * - delay factor (a time that the processing should wait for
 *   regardless of what happens next); set with the think time
 * - a timeout (a time after which the process will abort)
 * - a series of:
 *   - a string to match
 *   - what to do when the string is seen.
 * - comment (which is ignored)
 *
 * The timestamp event records the outcome of the event.
 * The outcome can be:
 * - one of the expected events happens (in which case it is recorded)
 * - the timeout expires
 *
 * If an expected event occurs, there may be optional actions. However,
 * we do not allow action recursion; the actions must simply restore
 * the main thread.
 *
 * If the timeout expires, the process exits.
 *
 * Data Structures
 * ===============
 * - A hash table indicating the slots in use
 * - Structures, dynamically allocated, to hold the information that
 *   relates to a single slot
 * - Structures, dynamically allocated, tracking the individual words
 *   being matched. 
 *
 * @(#) $Name$ $Id$
 * Copyright (c) E2 Systems 1993
 */
#ifndef MATCHLIB_H
#define MATCHLIB_H
#ifdef MINGW32
#ifdef LCC
#ifdef timezone
#undef timezone
#endif
struct timezone {
int tz_minuteswest;
int tz_dsttime;
};
#endif
#ifdef VCC2003
#ifdef timezone
#undef timezone
#endif
struct timezone {
int tz_minuteswest;
int tz_dsttime;
};
#endif
#ifndef _WINSOCK
#ifndef	__winsock2_h__
#ifndef	_WINSOCKAPI_
#ifndef	_WINSOCK2API_
#ifndef _TIMEVAL_DEFINED /* also in winsock[2].h */
#define _TIMEVAL_DEFINED
struct timeval {
	unsigned int	tv_sec;		/* seconds */
	unsigned int	tv_usec;	/* and microseconds */
};
#endif
#endif
#endif
#endif
#endif
struct timespec {
        time_t tv_sec;
        long tv_nsec;
};
#define PROT_READ 1
#define PROT_WRITE 2
#define SIGHUP 1
#define SIGQUIT 3
#define SIGIO 29
#define SIGBUS 10
#define SIGKILL 9
#define SIGPIPE 13
/*
 * SIGALRM should be 14, but use SIGBREAK to implement alarm()
 */
#define SIGALRM SIGBREAK
#define SIGUSR1 16
#define SIGUSR2 17
void (*sigset())();
#define MAXPATHLEN MAX_PATH
#ifndef MAX_PATH
#define MAX_PATH 256
#endif
#endif
#include "ansi.h"
#ifdef PATH_AT
#define BUF_CNT 5
#else
#define BUF_CNT 3
#endif
struct circ_buf {
    int buf_cnt;
    int buf_low;
    int buf_high;
    short int * head;
    short int * tail;
    short int * base;
    short int * top;
};
#define MAX_EVENT 128
#ifdef AIX
#define TGNEED
#endif
#ifdef ULTRIX
#define TGNEED
#endif
#ifdef HP7
#define TGNEED
#define signed
#endif
#ifdef SEQ
#define signed
#endif
#ifdef PYR
#define signed
#endif
#ifdef SUN
#define signed
#endif
#ifdef SCO
#define signed
#endif
#ifdef DGUX
#define signed
#endif
#include "e2conv.h"
#include "hashlib.h"
#ifdef V32
struct timeval {
unsigned int tv_sec;
unsigned int tv_usec;
};
#endif
struct word_con {
struct word_con * next_word;
short int * words;
short int * tail;
short int * head;
short int * state;
short int * action;
short int * comped;    /* If a MAGIC item, the compiled expression */
};
struct event_con {
char event_id[3];
double time_int;
short int min_delay;
struct timeval timeout;
struct word_con * first_word;
struct word_con * word_found;
short int * comment;
short int match_type;    /* MAGIC or NOMAGIC */
short int flag_cnt[4];
};
extern char * getenv();
extern char * strtok();
extern FILE * fopen();
extern void event_record();
extern double timestamp();
extern void alarm_catch();
#ifdef CANT_USE_OLD
signed char * nextfield ANSIARGS((signed char * start_point,
signed char sepchar));
#else
extern signed char * nextfield();
#endif
extern int ttlex();        /* Lexical Analyser */
extern void push_file();   /* Echo File Injector */
extern struct event_con * stamp_declare();
                                      /* Declare an event from string */
extern struct event_con * termload(); /* Read Terminal Definitions */
/**************
 * Undocumented internal curses function that looks up the attribute name
 * for a given terminal type, and returns a pointer to the string.
 */
extern char * tigetstr();
/************************
 * Global data
 */
struct ptydrive_glob {
FILE * cur_in_file; /* Where input is coming from at the moment */
FILE * script_file; /* Channel for script input */
FILE * log_output;  /* Channel for script output */
FILE * fo;          /* Channel for timestamp output */
struct circ_buf * circ_buf[BUF_CNT];
                    /* circular buffer pointers for input and output */
struct circ_buf term_buf;
int write_mask;     /* fd's to select for write */
int sav_write_mask;
int term_check;     /* select mask to check for pty */
int in_check;       /* select mask to check for script */
int seqX;           /* current event number */
short int cps;      /* current typing speed */
short int think_time; /* current think time */
                                            /* program parameters */
char * fdriver_seq;
char * bundle_seq;
char * rope_seq;
char * logfile;
short int see_through;                      /* flag for ttlex() processing */
short int esc_comm_flag;                      /* flag for ttlex() processing */
short int dumpin_flag;
short int dumpout_flag;
struct event_con * term_event; /* Pointer to terminal key definitions */
int child_pid;
struct event_con * curr_event; /* The event we are looking for; used as
                                * a flag to see if scanning or not */
struct event_con * abort_event; /* The event that failed; used to see
                                * to see what went wrong */
int cur_esc_char;              /* The escape character */
HASH_CON * poss_events;
int frag_size;                 /* Maximimum size of a single write to the
                                  pty */
int packet_mode;               /* True if packet mode flow control is enabled */
#ifdef PATH_AT
void (*cur_dia)();             /* The current dialogue, if it exists */
short int int_flag;            /* Flag saying that the user has requested
                                * a script interrupt
                                */
short int break_flag;          /* Flag saying that the user wants the script
                                * to pause after each function key.
                                */
short int force_flag;          /* Flag saying that the user has requested
                                * a forced match.
                                */
short int next_auto;           /* For auto-declared events */
struct event_con * auto_event; /* The event that we are automatically
                                * incrementing the ID for. */
#ifdef DEBUG_FULL
int alarm_flag;                /* Flag that an interrupt was deliberate */
#endif
short int * (*match_comp)();
void (*match_out)();
#endif
};
extern struct ptydrive_glob pg;
#ifdef PATH_AT
#define FORCE_REQUEST 1        /* force processing to be initiated */
#define FORCE_DUE 2            /* force processing due */
extern void do_redraw();              /* Do a screen redraw */
extern void do_pop_up();              /* Handle a pop-up screen */
extern void dia_man();
extern void fix_man();
#endif
void match_set_up ANSIARGS((struct circ_buf * buf, struct event_con * e));
/*
 * Matching algorithm
 */
#define NOMAGIC 0
#define MAGIC 1
short int * match_comp ANSIARGS(( short int * in_exp, int *len ));
void match_out ANSIARGS(( struct word_con * curr_word ));
/*
 * Routine for searching for matches
 */
int lanalyse ANSIARGS(( struct circ_buf * circular,
struct event_con * curr_event,
struct word_con ** match,
struct word_con ** disc));
/*
 * Force a match
 */
int match_force ANSIARGS(( struct circ_buf * circular,
struct event_con * curr_event,
struct word_con ** match,
struct word_con ** disc));
/*
 * Set up for the Forced Matching
 */
void ini_force ANSIARGS(( int timeout ));

/*
 * Screen Redraw String
 */
#define REDRAW_STRING ""
/*
 * Default Typing speed
 */
#define PATH_CPS 10
/*
 * Default Think time in seconds
 */
#define PATH_THINK 10
/*
 * Time before we give up, in seconds
 */
#define PATH_TIMEOUT 1200
/*
 * Direction indicators for message pair logging
 */
#define IN_DIRECTION 0
#define OUT_DIRECTION 1
void logadd ANSIARGS((int direction, char * ptr, int len));
#endif
