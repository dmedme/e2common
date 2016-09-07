/*
 * natmenu.h - forms driver program include file
 * - drives simple forms
 * - fixed format screen
 * - readily adapted
 * - uses AT&T curses (which don't work very well on Ultrix...)
 * @(#) $Name$ $Id$
 * Copyright (c) E2 Systems 1993
 */
#ifndef NATMENU_H
#define NATMENU_H
#define CHTYPE unsigned long
/*
 * Want to use AT&T curses()
 */
#ifdef OSF
#include <curses.h>
#else
#ifdef AIX
#undef HZ
#include <cur00.h>
#include <cur01.h>
#include <cur02.h>
#include <cur03.h>
#else
#ifdef ULTRIX
#include <cursesX.h>
#else
#ifdef MOD
#include <curses.h>
#include <term.h>
#else
#include <curses.h>
#endif
#endif
#endif /* AIX */
#endif /* OSF */
#ifdef AIX
#ifndef OSF
#define bool int
#define chtype int
#define attron colorout
#define wattron wcolorout
#define wnoutrefresh wrefresh
#define doupdate()
#define touchwin(x)
#endif
#endif

/*
 * Whether or not a menu-form allows multiple selections.
 */
enum multi_sel {MULTI_NO,MULTI_YES};

#define DEF_LINES 24
#define DEF_COLUMNS 80
#define MAX_COLUMNS 132
#define FORMS_MAX 100
#define HEAD_LINES 1
#ifdef AIX
#define HEAD_ATTR 0
#define FIELD_ATTR 0
/* #define SEL_ATTR _dREVERSE */
#define SEL_ATTR 0
#define COMM_ATTR 0
#define MESS_ATTR 0
#else
#define HEAD_ATTR A_BOLD
#define SEL_ATTR A_REVERSE
#define FIELD_ATTR A_UNDERLINE
#define COMM_ATTR A_UNDERLINE
#define MESS_ATTR A_BOLD
#endif
#define MEN_ATTR 0
#define PROMPT_LINES 1
#define PROMPT_ATTR 0
#define MESS_LINES 1
/*
 * Size and shape of the form
 */
struct form_out_line {
    int head_lines;
    int head_cols;
    int head_x;
    int head_y;
unsigned    int head_attr;
    int sel_lines;
    int sel_cols;
    int sel_x;
    int sel_y;
unsigned    int sel_attr;
    int men_lines;
    int men_cols;
    int men_x;
    int men_y;
unsigned    int men_attr;
    int field_lines;
    int field_cols;
    int field_x;
    int field_y;
unsigned    int field_attr;
    int prompt_lines;
    int prompt_cols;
    int prompt_x;
    int prompt_y;
unsigned    int prompt_attr;
    int comm_lines;
    int comm_cols;
    int comm_x;
    int comm_y;
unsigned    int comm_attr;
    int mess_lines;
    int mess_cols;
    int mess_x;
    int mess_y;
unsigned    int mess_attr;
};
struct screen {
    WINDOW * full_scr;
    WINDOW * head_zn;
    WINDOW * sel_zn;
    WINDOW * men_zn;
    WINDOW * field_zn;
    WINDOW * prompt_zn;
    WINDOW * comm_zn;
    WINDOW * mess_zn;
    struct form_out_line shape;
};                     /* window organisation for the form system */
/********************************************************************
 * A scrolling block control structure
 * - space is allocated in a 3 level structure, with 16 pointers in
 *   each block at each level, allowing 4096 lines in total
 * - strings are in dynamically allocated space
 */
struct scr_str {
short int top_off;
short int mid_off;
short int bot_off;
};
enum selective {DO_ALL, DO_SOME};
/*
 * - The control structure 
 *   - the top level block 
 *   - last, current and first displayed elements
 *   - count of members
 *   - window size
 * Currency means 'Current Displayed'. This is changed by the
 * Display routines, but not the routines that find items, or add
 * them.
 */
struct scr_cntrl {
char ***top_blk[16];
struct scr_str last;
struct scr_str curr;
struct scr_str first_disp;
short int member_cnt;  /* the number of members */ 
short int window_size; /* the number of members displayable */
};
/*
 * Create a new scrolling list
 */
struct scr_cntrl * make_scr ANSIARGS((void));
/*
 * Find the nth element
 */
char * find_member ANSIARGS((struct scr_cntrl * scr,int n));
/*
 * Add a new element to a scrolling list
 */
char * add_member ANSIARGS((struct scr_cntrl * scr,char * str));
char * add_padded_member ANSIARGS((struct scr_cntrl * scr,char * str,
                  int pad_length));
/*
 * Change an element in a scrolling list
 * String must be the same size
 */
void chg_member ANSIARGS((struct scr_cntrl * scr, int memb, char *new_str ));
/*
 * Scroll display
 * - Redraw takes as its parameters:
 *   - window in question
 *   - scroll area in question
 *   - new first to be displayed
 *   - new current item
 *   Both these are passed as numbers.
 */
#define SEQ_NUM(x) (((x).top_off<<8)+((x).mid_off<<4)+(x).bot_off)
/*
 * Paint some rows on the screen
 */
void scr_paint ANSIARGS((WINDOW * win,struct scr_cntrl * scr,int first_no,int first_pos,int count));
enum new_flag {OLD,NEW};
void scr_display ANSIARGS((WINDOW * win,struct scr_cntrl * scr,int new_first,int new_curr,enum new_flag new_flag));
/****************************************
 * Forms level stuff
 */
enum i_state {NOT_USED,NO_DATA,YES_DATA};  /* state of this template */
enum sel_allow {SEL_NO, SEL_YES};
                                           /* whether the form allows
					    * selections, or field input */
enum comm_allow {COMM_NO, COMM_YES};       /* whether the form allows command
					    * input, or not */
enum comm_type {MENU, SYSTEM};             /* whether the form is a menu or
					    * a packaged OS command */
struct template {
    enum i_state i_state;               /* How far set-up has got */
    enum sel_allow sel_allow;
    enum comm_allow comm_allow;
    enum comm_type comm_type;
    struct template * parent_form;      /* To support a menu tree */
    char head_txt[MAX_COLUMNS+1];
    struct scr_cntrl sel_cntrl;
    struct scr_cntrl men_cntrl;
    struct scr_cntrl field_cntrl;
    char prompt_txt[MAX_COLUMNS+1];
    char * comm_txt;
    char mess_txt[MAX_COLUMNS+1];
    char *trigger_code;                 /* Hook for trigger code */
    void (*trigger_fun)();
};
/*
 * Major Functions:
 * - setup system
 * - read form definition - read_form()
 * - define form
 *     - def_form()
 *     - get_os_out()
 * - re-display form
 *   - main body
 *     - dis_form()
 *       - scrolling region
 *         - get_fix_scroll()
 * - allow valid actions:
 *   - selections - inp_sel()
 *   - field inputs - inp_field()
 *   - command inputs - inp_comm()
 * - execute command
 *   - menu - do_menu()
 *   - operating system command - do_os()
 * - exit screen
 * - display message
 */
/*
 * Initialise the screen
 */
struct screen setup_form ANSIARGS((void));
struct screen setup_wins ANSIARGS((struct form_out_line * shape));
/*
 * Define a form
 */
struct template * def_form ANSIARGS((struct form_out_line * shape,
struct template * parent,enum sel_allow sel_allow,enum comm_allow comm_allow,
enum comm_type comm_type,char * head, char * prompt, char * comm, char * mess,
char * trigger_code,
void (*trigger_fun)()));
/*
 * Destroy a scrolling region that has been malloc'ed
 */
void scr_destroy ANSIARGS((struct scr_cntrl * scr));
/*
 * Destroy the form
 */
void destroy_form ANSIARGS((struct template * plate));
/*
 * inp_sel() - process menu selections
 *   - Two flavours
 *     - multi_sel == MULTI_YES;
 *       - allows multiple selections; selected values are flagged
 *       - selected items will be arguments to the OS command
 *     - multi_sel == MULTI_NO;
 *       - the selected item is executed on the spot
 *
 * routine for displaying the scrolling areas of the form
 */
char * inp_sel ANSIARGS((struct screen * screen,
                         struct template * plate,enum multi_sel multi_sel));
char * inp_field ANSIARGS((struct screen * screen,
                         struct template * plate));
void inp_comm ANSIARGS((struct screen * screen,
                         struct template * plate));
char * do_form ANSIARGS((struct screen * screen,
                         struct template * plate));
void get_fix_scroll ANSIARGS((struct screen * screen,struct template * plate));
/*
 * Print the scrolling areas to a file descriptor
 */
void scr_print ANSIARGS((FILE * fp,struct template * plate));
/*
 * Routine to find a form in the form cache by name
 */
struct template ** find_form ANSIARGS((char * form_name));
/*
 * Display a form
 */
void dis_form ANSIARGS((struct screen * screen,struct template * plate));
#ifndef NT4
extern char * getenv ANSIARGS((char * envname));
#endif
/*
 * Print a form
 */
void print_form ANSIARGS((struct screen * screen,struct template * plate));
/*
 * Routine to copy a string leaving exactly one space on the end of it
 * Returns where it got to.
 */
char * norm_str ANSIARGS((char * start,char * target));
/*************************************************************************
 * Routines to process a form
 * Forms are of two main types:
 *   - Menus (comm_type == MENU)
 *   - Commands (comm_type == SYSTEM)
 *
 * A Menu allows selection of a single item, which is then actioned.
 *
 * Commands come in two flavours:
 *   - those that allow multiple items to be selected,
 *     and then a command executed against them
 *     (sel_allow == SEL_YES)
 *   - those that allow fields to be entered, and then a command executed
 *     with the input fields as arguments.
 *     (sel_allow == SEL_NO)
 *
 * Read the form definition file descriptor
 */
void read_form ANSIARGS((FILE * fp, struct form_out_line * shape));
/*
 * Output a message
 */
void mess_write ANSIARGS((struct screen * s,struct template * p,char * message));
void   setup_shape ANSIARGS(( struct form_out_line * shape, int e2lines,
        int e2cols,int top_x, int top_y));
void   blank_shape ANSIARGS(( struct form_out_line * shape));

/*
 * Handle the output from an OS command executed by the form
 */
void get_os_out ANSIARGS((struct screen * screen,struct template * plate,char * comm,char * prompt));
/*
 * Feed back the input from the user to invoking shell
 */
void feed_back ANSIARGS((char * out));
void catch_signal ANSIARGS((void));
void help_init ANSIARGS((struct form_out_line * shape));

FILE * npopen ANSIARGS((char * fname,char * fmode));
#define TRAIL_SPACE_STRIP(x)\
{int _i; char *_y; for (_i=strlen(x),_y=(x)+_i;\
 _i>=0 &&(*_y=='\0'||*_y==' ');*_y-- ='\0',_i--);}

int edit_str ANSIARGS((WINDOW * win, char * str, int y, int len));
void proc_search ANSIARGS(( struct screen * screen, struct template * plate,
enum multi_sel multi_sel,
short * cur_line,
short * top_line,
short * cur_x));
int natmenu;   /* if 1, standalone behaviour; if 0, embedded */

char * do_os ANSIARGS((struct screen * screen,struct template * plate,char * comm,char * prompt,enum selective selective));
#ifndef A_CHARTEXT
#define A_CHARTEXT 0x7f
#endif
#ifdef OSF
void set_curses_modes();
void unset_curses_modes();
#endif
#endif      /* !NATMENU_H */
