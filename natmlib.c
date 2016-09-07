/*
 * natmlib.c - forms driver service routines
 * - drives simple forms
 * - fixed format screen
 * - readily adapted
 * - uses AT&T curses (which don't work very well on Ultrix...)
 */
static char * sccs_id = "@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1992";
#include <limits.h>
#ifndef ARG_MAX
#define ARG_MAX 4096
#endif
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#ifdef PYR
#define WEXITSTATUS(x) x.w_T.w_Retcode
#endif
#include <stdio.h>
#include <string.h>
#include "ansi.h"
#include "natmenu.h"
#include "natregex.h"
#ifdef LINUX
void * calloc();
void * malloc();
#endif
#ifdef OSF
/*
 * curses does not seem to set the terminal modes
 */
#include <termios.h>
static struct termios save_tm;
void set_curses_modes()
{
    struct termios t;
    if (tcgetattr(0,&save_tm) < 0)
        perror("Getting Terminal Attributes");
    t = save_tm;
    t.c_cflag |= CS7 ; /* 7 bit characters with no parity */
    t.c_cflag &= ~PARENB;
    t.c_cc[VINTR] = (cc_t) 3;
    t.c_cc[VQUIT] = (cc_t) 28;
    t.c_cc[VERASE] = (cc_t) 8;
    t.c_cc[VKILL] = (cc_t) 21;
    t.c_cc[VREPRINT] = (cc_t) 18;
    t.c_iflag = ISTRIP | IXOFF | IGNPAR;
    t.c_oflag = 0;
    t.c_lflag = ISIG ;
    if (tcsetattr(0,TCSANOW,&t) < 0)
        perror("Error Setting Terminal Attributes");
    return;
}
void unset_curses_modes()
{
    if (tcsetattr(0,TCSANOW,&save_tm) < 0)
        perror("Setting Terminal Attributes");
    return;
}
#endif

extern struct template * menu_tree[FORMS_MAX];
extern char null_select[133];
static struct help_skel {
    char * head;
    char * sel_lines[17];
    char * prompt;
} help_skel[] = {
{
    "MULTI_SEARCH: Look for List Items",
    {
         "Search forwards                         ",
         "Search backwards                        ",
         "Select all matching                     ",
         "Select all not matching                 ",
    },
    "Select Option and Press RETURN               "
},
{
    "SINGLE_SEARCH: Look for List Item",
    {
         "Search forwards                         ",
         "Search backwards                        ",
    },
    "Select Option and Press RETURN               "
},
{
    "MULTI_HELP: Multiple Item Selection    ",
    {
         "Select Items for processing.            ",
         "You select items as follows             ",
         "- PRINT,^P            - Print the screen",
         "- TAB, +, DOWN ARROW  - To item below   ",
         "- UP ARROW, -         - To item above   ",
         "- SELECT              - Mark selected   ",
         "- SPACE               - Clear selection ",
         "- HELP (F15)          - Display this    ",
         "- /                   - Search/Select   ",
         "- RETURN, ^J          - Return selection"
    },
    "Any Command to Return                  "
},
{
    "SINGLE_HELP: Menu Option Selection     ",
    {
         "Choose a Menu Option.",
         "You select items as follows",
         "- PRINT,^P            - Print the pop-up",
         "- TAB, +, DOWN ARROW  - To item below   ",
         "- UP ARROW, -         - To item above   ",
         "- SELECT, RETURN, ^J  - Action          ",
         "- HELP (F15)          - Display this    ",
         "- /                   - Search          ",
         "- ^Z                  - Quit the screen "
    },
    "Any Command to Return                  "
},
{
    "FIELD_HELP: Filling in fields          ",
    {
        "Fill in the fields, then process.        ",
        "You can edit the fields as follows       ",
        "- ^U          - Clear the field          ",
        "- ^Z          - Quit the pop-up          ",
        "- PRINT,^P    - Print the screen         ",
        "- TAB         - Move to field below      ",
        "- DOWN ARROW  - Move to field below      ",
        "- UP ARROW    - Move to field above      ",
        "- LEFT ARROW  - Move left one character  ",
        "- RIGHT ARROW - Move right one           ",
        "- INSERT HERE - Insert a space           ",
        "- HELP (F15)  - Display this             ",
        "- REMOVE      - Delete this character    ",
        "- Delete      - Delete and move left     ",
        "- RETURN, ^J  - Pass on for execution    ",
        "- Otherwise   - Overtype the current     ",
        "                and move right           "
    },
    "Any Command to Return                  "
}};

/****************************************************************************
 * Populate the form cache with help prompt strings
 */
void help_init(shape)
struct form_out_line * shape;
{
char line_buf[4096];
static char spare_buf[133];
struct template * new;
struct template ** par;
int i;
int ret;
register struct help_skel * hsp;
/*
 * Loop through the templates in the skeleton array, and initialise forms
 * for them.
 */
    memset (spare_buf,' ',sizeof(spare_buf) - 1);
    for (i = 0,
         hsp = help_skel;
             i < sizeof(help_skel)/sizeof(struct help_skel);
                  i++,
                  hsp++)
    {
    int j;
        new = (struct template *) NULL;
        par = &new;
        if ((new =  def_form(shape, *par,
                                SEL_YES,
                                COMM_YES,
                                MENU,
                                hsp->head,hsp->prompt,spare_buf,spare_buf,
                                (char *) NULL,
                                (void (*)()) NULL))
                           == (struct template *) NULL)
        {
#ifdef DEBUG
     printf("Cannot create help form!?\n");
#endif
            return;
        }
        for (j = 0; j < sizeof(hsp->sel_lines)/
                       sizeof(hsp->sel_lines[0]) &&
                    hsp->sel_lines[j] != (char *) NULL; j++)
        {
             add_member(&(new->sel_cntrl)," ");
             add_member(&(new->men_cntrl),hsp->sel_lines[j]);
             add_member(&(new->field_cntrl),"EXIT:");
        }
/*
 * Hard code the search capability here. The alternative is worse.
 */
        if (!strncmp(hsp->head,"MULTI_SEARCH:",13)
            ||  !strncmp(hsp->head,"SINGLE_SEARCH:",14))
        {
            chg_member(&(new->field_cntrl),(int) 0,"FORW:");
            chg_member(&(new->field_cntrl),(int) 1,"BACK:");
            if (!strncmp(hsp->head,"MULTI_SEARCH:",13))
            {
                chg_member(&(new->field_cntrl),(int) 2,"MALL:");
                chg_member(&(new->field_cntrl),(int) 3,"MOTH:");
            }
        }
    }
    return;
}
/***************************************************************************
 * Create a new scrolling list
 */
struct scr_cntrl * make_scr()
{
   register struct scr_cntrl * x;
   x = (struct scr_cntrl *) calloc(1,sizeof(struct scr_cntrl));
                                /* The main structure */

   *(x->top_blk) =  (char ***) calloc(16,sizeof(char **));
                                /* The Second level pointers */
   *(*(x->top_blk)) =  (char **) calloc(16,sizeof(char *));
                                /* The Third level pointers */
   return x;
}
/*
 * Find the nth element
 */
char * find_member(scr,n)
struct scr_cntrl * scr;
int n;
{
    short int top_off, mid_off, bot_off, rn;
    char ***mid_ptr;
    char **bot_ptr;
    rn  = (short int) n;
    if (rn >= scr->member_cnt || rn < 0)
        return (char *) 0;
    top_off= ( rn >> 8);
    mid_off= ((rn & 0xf0) >> 4);
    bot_off= (rn & 0xf);
    mid_ptr = *((scr->top_blk) + top_off);
    if (mid_ptr == (char ***) 0)
        return (char *) 0;
    else
        mid_ptr += mid_off;
    if (*mid_ptr == (char **) 0)
        return (char *) 0;
    else
    bot_ptr = *mid_ptr + bot_off;
    if (*bot_ptr == (char *) 0)
        return (char *) 0;
    else
        return *bot_ptr;
}
/*
 * Add a new element to a scrolling list
 */
char * add_member(scr,str)
struct scr_cntrl * scr;
char * str;
{
    short int top_off, mid_off, bot_off, len;
    char ****top_blk;
    char ***top_ptr;
    char **mid_ptr;
    char *bot_ptr;
    len = strlen(str);
    if (scr->member_cnt == 4096)
        return (char *) 0;       /* maximum capacity is 4096 */
    if ((bot_ptr = (char *) malloc(len+1)) == (char *) 0)
        return (char *) 0;
    (void) strcpy(bot_ptr,str);
    top_blk= scr->top_blk;
    if (scr->member_cnt == 0 && top_blk[0] == (char ***) 0)
    {
        top_off = -1;
        mid_off = 15;
        bot_off = 15;
    }
    else
    {
        top_off= scr->last.top_off;
        mid_off= scr->last.mid_off;
        bot_off= scr->last.bot_off;
    }
/*
 * Check if can put in at the bottom level:
 * - No, allocate a new bottom level; add to next level up
 * - If can't add at this level, add another mid-level block
 * - Can't fill top level, because have checked that less than 4096
 */
    bot_off++;
    if (bot_off == 16)
    {
        bot_off = 0;
        if ((mid_ptr = (char **) calloc(16,sizeof(char *)))
                     == (char **) 0)
            return (char *) 0;
        mid_off++;
        if (mid_off == 16)
        {
            mid_off = 0;
            if ((top_ptr = (char ***) calloc(16,sizeof(char **)))
                     == (char ***) 0)
            return (char *) 0;
            top_off++;
            *(top_blk+top_off) = top_ptr;
            scr->last.top_off = top_off;
        }
        *(*(top_blk + top_off) + mid_off) = mid_ptr;
        scr->last.mid_off = mid_off;
    }
    *(*(*(top_blk + top_off) + mid_off)+bot_off) = bot_ptr;
    scr->member_cnt++;
    scr->last.bot_off = bot_off;
    return bot_ptr;
}
/*
 * Add a new element to a scrolling list in a space-padded field
 */
char * add_padded_member(scr,str,pad_length)
struct scr_cntrl * scr;
char * str;
int pad_length;
{
    char * x, *x1;
    if ((x = (char *) malloc(pad_length+1)) == (char *) 0)
        return (char *) 0;
    (void) sprintf(x,"%-*.*s",pad_length,pad_length,str);
    x1 = add_member(scr,x);
    free(x);
    return x1;
}
/*
 * Change an element in a scrolling list
 * String must be the same size
 */
void chg_member(scr,memb,str)
struct scr_cntrl * scr;
int memb;
char * str;
{
    int patch;
    char * upd = find_member(scr,memb);
    if (upd == (char *) NULL)
        return;
    patch=strlen(upd);
    strncpy(upd,str,patch);
    return;
}
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
 * Output a string, stuffing control characters and respecting the width
 */
static void str_paint(win,wid,x)
WINDOW *win;
int wid;
char * x;
{
#ifdef DEBUG
    int junk;
#endif
    *x &= 0x7f;
    while ((wid-- > 0) && *x != '\0')
    {
/*
 * Output control characters as ^X
 */
        if (((int) *x) < 32 && ((int) *x) > 0)
        {
            wid--;
            waddch(win, (chtype) '^');
            if (wid)
                waddch(win, (chtype) (*x++ + 64));
        }
        else
            waddch(win, (chtype) *x++);
#ifdef DEBUG
        wrefresh(win);
        read(0,&junk,1);
#endif
    }
    if (wid > 0)
        wclrtoeol(win);
    return;
}
/*
 * Paint some rows on the screen
 */
void scr_paint(win,scr,first_no,first_pos,count)
WINDOW * win;
struct scr_cntrl * scr;
short first_no;            /* index in list of first item to display */
short first_pos;           /* line on screen of first to display     */
short count;               /* number of lines to display             */
{
register short i,j,l;
    for (i=first_pos, j=first_no, l=first_pos + count ; i < l; i++,j++)
    {
        char * x;
        wmove(win,i,0);
        if ((x = find_member(scr,(int) j)) != (char *) 0)
        {
/*
             win->_maxx - win->_begx + 1
 */
            str_paint(win, ((win->_maxy == i) ?
                           (win->_maxx - 1) :
                           (win->_maxx)), x);
        }
        else
        {
            wclrtobot(win);
            break;
        }
    }
    return;
}
extern bool scroll_done;
void scr_display(win,scr,new_first,new_curr,new_flag)
WINDOW * win;
struct scr_cntrl * scr;
short new_first;          /* index in list of first item on screen */
short new_curr;           /* index in list of current item */
enum new_flag new_flag;
{
/*
 * The interesting cases are:
 * - Nothing here, or nothing currently displayed, in which case,
 *   need to do the whole thing
 * - Can do some scrolling
 */
    int i,j;
    int old_first;
    old_first = SEQ_NUM(scr->first_disp);
    if (old_first != new_first || new_flag == NEW)
/*
 * if new first is the same, don't have to do anything: just position
 * cursor
 */
    if (new_flag == NEW
       || new_flag == OLD          /* Ultrix special - scrolling doesn't work */
       || (new_first < old_first - scr->window_size+1)
       || (new_first > old_first + scr->window_size))
                               /* none of the current lines are displayed */
        scr_paint(win,scr,new_first,0,scr->window_size);
    else if (new_first > old_first)
    {
/*
 * New first line is going to be a line below the current
 */
        if (scroll_done == FALSE)
        {
            for (i= new_first - old_first; i; i--)
            {
                scroll(win);  /* rack it up */
            }
            scroll_done = TRUE;
        }
        scr_paint(win,scr,scr->window_size+old_first, 
                  scr->window_size - new_first + old_first,
                   new_first - old_first);
                                                 /* paint lines on the bottom */
    }
    else /* if (new_first < old_first) */
    {
/*
 * New first line is going to be a line above the current
 */
        wmove(win,0,0);
        if (scroll_done == FALSE)
        {
            for (i= old_first - new_first; i; i--)
                winsertln(win);             /* put in some blank lines */
            scroll_done = TRUE;
        }
        scr_paint(win,scr,new_first, 0,
                  old_first - new_first); /* paint them */
    }
    if (find_member(scr, (int) new_curr) == (char *) 0)
       new_curr = scr->member_cnt - 1;
    wmove(win,new_curr - new_first,0);
    scr->first_disp.top_off= (new_first >> 8);
    scr->first_disp.mid_off= ((new_first & 0xf0) >> 4);
    scr->first_disp.bot_off= (new_first & 0xf);
    scr->curr.top_off= (new_curr >> 8);
    scr->curr.mid_off= ((new_curr & 0xf0) >> 4);
    scr->curr.bot_off= (new_curr & 0xf);
    return;
}
/*
 * Initialise the shape of a screen
 */
void setup_shape (shape,e2lines,e2cols,top_x,top_y)
struct form_out_line * shape;
int e2lines;
int e2cols;
int top_x;
int top_y;
{
    shape->head_attr = HEAD_ATTR;
    shape->prompt_attr = PROMPT_ATTR;
    shape->comm_attr = COMM_ATTR;
    shape->mess_attr = MESS_ATTR;
    shape->sel_attr = SEL_ATTR;
    shape->men_attr = MEN_ATTR;
    shape->field_attr = FIELD_ATTR;
    shape->head_lines = HEAD_LINES;
    shape->prompt_lines = PROMPT_LINES;
    shape->mess_lines = MESS_LINES;
    shape->sel_lines = e2lines - shape->prompt_lines -
                           shape->head_lines - shape->mess_lines;
    shape->head_cols = e2cols;
    shape->head_x = top_x;
    shape->head_y = top_y;
    shape->comm_lines = shape->prompt_lines;
    shape->men_lines = shape->sel_lines;
    shape->field_lines = shape->sel_lines;
    shape->sel_y = shape->head_y + shape->head_lines;
    shape->men_y = shape->sel_y;
    shape->field_y = shape->sel_y;
    shape->prompt_y = shape->sel_y + shape->sel_lines;
    shape->comm_y = shape->prompt_y;
    shape->mess_y = shape->prompt_y + shape->prompt_lines;
    shape->sel_x = shape->head_x;
    shape->prompt_x = shape->head_x;
    shape->mess_x = shape->head_x;
    shape->prompt_cols = shape->head_cols;
    shape->comm_cols = 3 * shape->prompt_cols/4;
    shape->comm_x = shape->prompt_x + shape->prompt_cols -
                        shape->comm_cols;
    shape->mess_cols = shape->head_cols - 1;
    shape->sel_cols = 1;
    shape->men_cols = shape->head_cols - shape->sel_cols;
    shape->men_x = shape->sel_x + shape->sel_cols;
    if (shape->head_cols > 40)
        shape->field_cols = shape->men_cols/2;
    else
        shape->field_cols = 3 * shape->men_cols/4;
    shape->field_x = shape->men_x + shape->men_cols
                          - shape->field_cols;
    return;
}
/*
 * Initialise the screen
 */
struct screen setup_wins(shape)
struct form_out_line * shape;
{
    struct screen screen;
    int pad_mode;
    int scroll_mode;

    pad_mode = TRUE;
    scroll_mode = FALSE;
    screen.shape = *shape;
    screen.field_zn = newwin(screen.shape.field_lines,
                               screen.shape.field_cols,
                               screen.shape.field_y,screen.shape.field_x);
    wattron(screen.field_zn,screen.shape.field_attr);
    scrollok(screen.field_zn,scroll_mode);
    keypad(screen.field_zn,pad_mode);
    screen.comm_zn = newwin(screen.shape.comm_lines,
                               screen.shape.comm_cols,
                               screen.shape.comm_y,screen.shape.comm_x);
    wattron(screen.comm_zn,screen.shape.comm_attr);
#ifdef AIX
    scrollok(screen.comm_zn,FALSE);
#else
    scrollok(screen.comm_zn,scroll_mode);
#endif
    keypad(screen.comm_zn,pad_mode);
    screen.prompt_zn = newwin(screen.shape.prompt_lines,
                          screen.shape.prompt_cols,
                               screen.shape.prompt_y,screen.shape.prompt_x);
    wattron(screen.prompt_zn,screen.shape.prompt_attr);
#ifdef AIX
    scrollok(screen.prompt_zn,FALSE);
#else
    scrollok(screen.prompt_zn,scroll_mode);
#endif
    screen.mess_zn = newwin(screen.shape.mess_lines,
                          screen.shape.mess_cols,
                               screen.shape.mess_y,screen.shape.mess_x);
    wattron(screen.mess_zn,screen.shape.mess_attr);
#ifdef AIX
    scrollok(screen.mess_zn,FALSE);
#else
    scrollok(screen.mess_zn,scroll_mode);
#endif
    screen.men_zn = newwin(screen.shape.men_lines,
                          screen.shape.men_cols,
                               screen.shape.men_y,screen.shape.men_x);
    wattron(screen.men_zn,screen.shape.men_attr);
    scrollok(screen.men_zn,scroll_mode);
    screen.head_zn = newwin(screen.shape.head_lines,
                          screen.shape.head_cols,
                               screen.shape.head_y,screen.shape.head_x);
    wattron(screen.head_zn,screen.shape.head_attr);
#ifdef AIX
    scrollok(screen.head_zn,FALSE);
#else
    scrollok(screen.head_zn,scroll_mode);
#endif
    screen.sel_zn = newwin(screen.shape.sel_lines,
                          screen.shape.sel_cols,
                               screen.shape.sel_y,screen.shape.sel_x);
    wattron(screen.sel_zn,screen.shape.sel_attr);
    keypad(screen.sel_zn,pad_mode);
    scrollok(screen.sel_zn,scroll_mode);
    if (screen.men_zn == NULL
     || screen.sel_zn == NULL
     || screen.comm_zn == NULL
     || screen.prompt_zn == NULL
     || screen.mess_zn == NULL
     || screen.head_zn == NULL)
        abort();
    return screen;
}
/*
 * Clear a shape, so that the refresh works properly
 */
void blank_shape(shape)
struct form_out_line * shape;
{
    WINDOW * blank;
    if ((blank = newwin(shape->head_lines +
                   shape->sel_lines +
                   shape->prompt_lines +
                   shape->mess_lines,
                   shape->head_cols,
                   shape->head_y,
                   shape->head_x)) == (WINDOW *) NULL)
        abort();
    wclrtobot(blank);
    wrefresh(blank);
    delwin(blank);
    return ;
}
/*
 * Destroy a scrolling region that has been malloc'ed
 */
void scr_destroy(scr)
struct scr_cntrl * scr;
{
    short int i, j, k;
    for (i = 0; i < 16; i++)
    {
        char *** mid_ptr;
        mid_ptr = scr->top_blk[i];
        if (mid_ptr == (char ***) NULL)
            return;
        for (j = 0; j < 16; j++)
        {
            char ** bot_ptr;
            bot_ptr = *(mid_ptr + j);
            if (bot_ptr == (char **) NULL)
                break;
            for (k = 0; k < 16; k++)
            {
                char * str_ptr;
                str_ptr = *(bot_ptr + k);
                if (str_ptr == (char *) NULL)
                    break;
                free(str_ptr);
            }
            free(bot_ptr);
        }
        free(mid_ptr);
    }
}
/*
 * Destroy the form
 */
void destroy_form(plate)
struct template * plate;
{
    struct template ** x;
    scr_destroy(&(plate->sel_cntrl));
    scr_destroy(&(plate->men_cntrl));
    scr_destroy(&(plate->field_cntrl));
    if ((x = find_form(plate->head_txt)) != (struct template **) NULL)
        *x = (struct template *) NULL;
    free (plate);
    return;
}
/*
 * routine for displaying the scrolling areas of the form
 */
void get_fix_scroll(screen,plate)
struct screen * screen;
struct template * plate;
{
    scroll_done = FALSE;
    scr_display(screen->men_zn, &(plate->men_cntrl), 0, 0, NEW);
    scr_display(screen->sel_zn, &(plate->sel_cntrl), 0, 0, NEW);
    if (plate->comm_type == SYSTEM && plate->sel_allow == SEL_NO )
        scr_display(screen->field_zn, &(plate->field_cntrl), 0, 0, NEW);
    return;
}
/*
 * Print the scrolling areas to a file descriptor
 */
void scr_print(fp,plate)
FILE * fp;
struct template  * plate;
{
    short int j;
    for (j = 0; ;j++)
    {
        char * x, * x1, *x2;
        if ((x = find_member(&(plate->sel_cntrl),(int) j)) == (char *) 0)
            return;
        if ((x1 = find_member(&(plate->men_cntrl),(int) j)) == (char *) 0)
            return;
        if (plate->sel_allow == SEL_YES)
            x2 = "";
        else if ((x2 = find_member(&(plate->field_cntrl),(int) j))
                                                            == (char *) 0)
            return;
        (void) fprintf(fp,"%-1.1s %-77.77s %s\n",x,x1,x2);
    }
}
/*
 * Routine to find a form in the form cache by name
 * Null name, returns the first free
 */
struct template ** find_form(form_name)
char * form_name;
{
   register struct template ** r = menu_tree;
   register char * x = form_name;
   register short int l;
   register short int i = sizeof(menu_tree)/sizeof(struct template *);

   if (x != (char *) NULL)
      l = strlen(x);
   while (i-- &&
             (x != (char *) NULL && (*r == (struct template *) NULL ||
                   strncmp(x,(*r)->head_txt,l))) ||
             (x == (char *) NULL && *r != (struct template *) NULL))
       r++;
   if (i > -1)
       return r;
   else
       return (struct template **) NULL;
}
/*
 * Display a form
 */
void dis_form(screen,plate)
struct screen * screen;
struct template * plate;
{
    register struct screen * rs = screen;
    touchwin(rs->men_zn);
    touchwin(rs->sel_zn);
    touchwin(rs->comm_zn);
    touchwin(rs->prompt_zn);
    touchwin(rs->mess_zn);
    touchwin(rs->head_zn);
    wmove(rs->head_zn,0,0);
    if (rs->men_zn == NULL
     || rs->sel_zn == NULL
     || rs->comm_zn == NULL
     || rs->prompt_zn == NULL
     || rs->mess_zn == NULL
     || rs->head_zn == NULL)
        abort();

    wprintw(rs->head_zn,"%-*.*s",rs->shape.head_cols -1,
                                 rs->shape.head_cols - 1,
                         plate->head_txt);
    if (rs->men_zn == NULL
     || rs->sel_zn == NULL
     || rs->comm_zn == NULL
     || rs->prompt_zn == NULL
     || rs->mess_zn == NULL
     || rs->head_zn == NULL)
        abort();
    wclrtoeol(rs->head_zn);
    if (rs->men_zn == NULL
     || rs->sel_zn == NULL
     || rs->comm_zn == NULL
     || rs->prompt_zn == NULL
     || rs->mess_zn == NULL
     || rs->head_zn == NULL)
        abort();
    wmove(rs->prompt_zn,0,0);
    if (rs->men_zn == NULL
     || rs->sel_zn == NULL
     || rs->comm_zn == NULL
     || rs->prompt_zn == NULL
     || rs->mess_zn == NULL
     || rs->head_zn == NULL)
        abort();
    wprintw(rs->prompt_zn,"%-*.*s",rs->shape.prompt_cols -1,
                                 rs->shape.prompt_cols - 1,
                         plate->prompt_txt);
    if (rs->men_zn == NULL
     || rs->sel_zn == NULL
     || rs->comm_zn == NULL
     || rs->prompt_zn == NULL
     || rs->mess_zn == NULL
     || rs->head_zn == NULL)
        abort();
    wclrtoeol(rs->prompt_zn);
    if (rs->men_zn == NULL
     || rs->sel_zn == NULL
     || rs->comm_zn == NULL
     || rs->prompt_zn == NULL
     || rs->mess_zn == NULL
     || rs->head_zn == NULL)
        abort();
    wmove(rs->mess_zn,0,0);
    if (rs->men_zn == NULL
     || rs->sel_zn == NULL
     || rs->comm_zn == NULL
     || rs->prompt_zn == NULL
     || rs->mess_zn == NULL
     || rs->head_zn == NULL)
        abort();
    wprintw(rs->mess_zn,"%-*.*s",rs->shape.mess_cols -1,
                               rs->shape.mess_cols - 1,
                         plate->mess_txt);
    if (rs->men_zn == NULL
     || rs->sel_zn == NULL
     || rs->comm_zn == NULL
     || rs->prompt_zn == NULL
     || rs->mess_zn == NULL
     || rs->head_zn == NULL)
        abort();
    wclrtoeol(rs->mess_zn);
    plate->i_state = NO_DATA;
    get_fix_scroll(rs,plate);
    wnoutrefresh(rs->head_zn);
    wnoutrefresh(rs->sel_zn);
    wnoutrefresh(rs->men_zn);
    wnoutrefresh(rs->prompt_zn);
    wnoutrefresh(rs->mess_zn);
    doupdate();
    if (plate->comm_type == SYSTEM && plate->sel_allow == SEL_NO)
    {
        touchwin(rs->field_zn);
        wrefresh(rs->field_zn);
    }
    return;
}
/*************************************************************************
 * Routine to process a form
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
 */
char * do_form(screen,plate)
struct screen * screen;
struct template * plate;
{
    dis_form(screen,plate);
    if (plate->comm_type == MENU)
        return (inp_sel(screen,plate,MULTI_NO));
    else if (plate->sel_allow == SEL_YES)
        return (inp_sel(screen,plate,MULTI_YES));
    else if (plate->sel_allow == SEL_NO)
        return (inp_field(screen,plate));
    return (char *) NULL;
}
/*
 * Print a form
 */
char * getenv();
void print_form(screen,plate)
struct screen * screen;
struct template * plate;
{
    FILE * fp;
    char * print_comm;
    int ret_code;
    char line_buf[80];
    if ((print_comm=getenv("VCSPRINT")) == (char *) NULL)
         print_comm="lpr";
    if (screen != (struct screen *) NULL)
    {
        if ((fp = npopen(print_comm,"w")) == (FILE *) NULL)
        {
             mess_write(screen,plate,"Cannot Access Print Command");
             return;
        }
    }
    else
    {
        if ((fp = fopen("/tmp/formdump","a")) == (FILE *) NULL)
        {
             perror("Cannot open form dump file");
            return;
        }
    }
    (void) fprintf(fp,"%s\n\n",plate->head_txt);
    scr_print(fp,plate);
    (void) fprintf(fp,"\n%s\n",plate->prompt_txt);
    if (plate->mess_txt != (char *) NULL)
        (void) fprintf(fp,"%s\n",plate->mess_txt);
    if (screen != (struct screen *) NULL)
    {
        (void) sprintf(line_buf,"Print Command Return Status %d", npclose(fp));
        mess_write(screen,plate,line_buf);
    }
    else (void) fclose(fp);
    return;
}
/*
 * Routine to copy a string leaving exactly one space on the end of it
 * Returns where it got to.
 */
char *
norm_str(start,target)
char * start;
char * target;
{
    register char * x, *x1, *x2;
    for(x=start,x2=start,x1=target;
           *x1 != '\0';
               x1++, x++)
    {
      *x = *x1;
      if (*x != ' ' && *x != '\t' && *x != '\n' && *x != '\r')
      {
          x2 = x + 1;   
      }
    }
    *x2++ = ' ';
    *x2 = '\0';    /* strip off trailing white space from the command */
    return x2;
}
#ifdef DEBUG
/*
 * Routine to dump out the form cache
 */
void dump_tree()
{
    FILE * fp;
    struct template ** r;
    if ((fp = fopen("/tmp/formdump","a")) == (FILE *) NULL)
    {
        perror("Cannot open form dump file");
        return;
    }
    (void) fprintf(fp,"%s\n\n","Dumping the Menu tree....");
    for (r = menu_tree;
             r < menu_tree + sizeof(menu_tree)/sizeof(struct template *);
                 r++)
    {
        (void) fprintf(fp,"\nPosition %d\n",
                          (r - menu_tree));
        if (*r != (struct template *) NULL)
        {
             (void) fclose(fp);
             print_form((struct screen *) NULL,*r);
             if ((fp = fopen("/tmp/formdump","a")) == (FILE *) NULL)
             return;
        }
        else (void) fprintf(fp,"is empty\n");
    }
    (void) fclose(fp);
}
#endif
/*
 * Just to tidy things up; long shell commands can be part of a single
 * menu option, if a trailing \ is provided
 */
static char * egets ( ptr, len, f)
char * ptr;
int len;
FILE * f;
{
char * w;
char * r;
int to_do;
int this;
    for (w = ptr, r = w, to_do = len;
            to_do > 0 && r != (char *) NULL;)
    {
        r = fgets ( w, to_do, f);
        if (r == (char *) NULL)
            if (w == ptr)
                return r;
            else
                return ptr;
        this = strlen(r);
        w = w + this - 2;
        if (*w == '\\')
        {
            *w++ = '\n';
            to_do = to_do - this - 1;
        }
        else
        {
            w++;
            *w = '\0';
            break;
        }
    }
    return ptr;
}
/*
 * Populate the form cache by reading a FILE descriptor until EOF
 */
void read_form(fp,shape)
FILE * fp;
struct form_out_line * shape;
{
struct screen * screen;
struct template work_plate;
                      /* home for various working variables and defaults */
char * comm;          /* command to execute */
char * prompt;        /* header string if wanted */
char line_buf[4096];
static char spare_buf[133]; /* 133 blanks */
static char spare_comm[133];
struct template * new;
int ret;
/*
 * Loop through the strings that are fed on the stream until
 * no more, stuffing them in a control structure
 */
    memset (spare_buf,' ',sizeof(spare_buf) - 1);
    memset (spare_comm,' ',sizeof(spare_comm) - 1);
    spare_buf[sizeof(spare_buf) - 1] = '\0';
    spare_comm[sizeof(spare_buf) - 1] = '\0';
    for (work_plate.i_state = NOT_USED,
         work_plate.sel_allow = SEL_YES,
         work_plate.comm_allow = COMM_NO,
         work_plate.comm_type = MENU;
            egets(line_buf,sizeof(line_buf),fp) != (char *) NULL;
                 ) 
    {
        char * x;
        x = strtok(line_buf,"=/\n");
        if ( x == (char *) NULL && work_plate.i_state == YES_DATA)
            work_plate.i_state = NOT_USED;
        else for (;
                 x != (char *) NULL ;
                      x = strtok(NULL,"=/\n"))
         {
            switch (work_plate.i_state)
            {
            case NOT_USED:
                if ((new =  def_form(shape, (struct template *) NULL,
                                work_plate.sel_allow,
                                work_plate.comm_allow,
                                work_plate.comm_type,
                                   spare_buf,spare_buf,spare_comm,spare_buf,
                                (char *) NULL,
                                (void (*)()) NULL))
                           == (struct template *) NULL)
                {
#ifdef DEBUG
     printf("Cannot create form!?\n");
#endif
                return;
                }
                work_plate.i_state = NO_DATA;
            case NO_DATA:
                if (!strcmp(x,"MENU"))
                    new->comm_type = MENU;
                else if (!strcmp(x,"SYSTEM"))
                    new->comm_type = SYSTEM;
                else if (!strcmp(x,"COMM_NO"))
                    new->comm_allow = COMM_NO;
                else if (!strcmp(x,"COMM_YES"))
                    new->comm_allow = COMM_YES;
                else if (!strcmp(x,"SEL_NO"))
                    new->sel_allow = SEL_NO;
                else if (!strcmp(x,"SEL_YES"))
                    new->sel_allow = SEL_YES;
                else if (!strcmp(x,"HEAD"))
                {
                    if ((x = strtok(NULL,"\n")) != (char *) NULL);
                        strncpy(new->head_txt,x,sizeof(new->head_txt));
                }
                else if (!strcmp(x,"PROMPT"))
                {
                    if ((x = strtok(NULL,"\n")) != (char *) NULL);
                        strncpy(new->prompt_txt,x,sizeof(new->prompt_txt));
                }
                else if (!strcmp(x,"MESSAGE"))
                {
                    if ((x = strtok(NULL,"\n")) != (char *) NULL);
                        strncpy(new->mess_txt,x,sizeof(new->mess_txt));
                }
                else if (!strcmp(x,"COMMAND"))
                {
                    if ((x = strtok(NULL,"\n")) != (char *) NULL)
                    {
                        char * x1 = (char *) malloc(strlen(x)+1);
                        if (x1 != (char *) NULL)
                        {
                            new->comm_txt = x1;
                            strcpy(x1,x);
                        }
                    }
                }
                else if (!strcmp(x,"TRIGGER"))
                {
                    char * x1 = (char *) malloc(strlen(x)+1);
                    if (x1 != (char *) NULL)
                    {
                        new->trigger_code = x1;
                        strcpy(x1,x);
                    }
                }
                else if (!strcmp(x,"PARENT"))
                {
                    if ((x = strtok(NULL,"\n")) != (char *) NULL);
                    {
                        struct template ** x1 = find_form(x);
                        if (x1 != (struct template **) NULL)
                            new->parent_form = *x1;
                    }
                }
                else if (!strcmp(x,"SCROLL"))
                    work_plate.i_state = YES_DATA;
                break;
            case YES_DATA:
                add_padded_member(&(new->men_cntrl),x, shape->men_cols);
                if ((x = strtok(NULL,"")) != (char *) NULL)
                {
                    if ( new->sel_allow == SEL_NO && new->comm_type == SYSTEM)
                    {
                        add_member(&(new->sel_cntrl)," ");
                        add_padded_member(&(new->field_cntrl),x,
                                            shape->field_cols+1);
                    }
                    else
                    {
                        if (*x == '*')
                        {               /* Pre-selected menu item */
                            add_member(&(new->sel_cntrl),"*");
                            add_member(&(new->field_cntrl),x+1);
                        }
                        else
                        {
                            add_member(&(new->sel_cntrl)," ");
                            add_member(&(new->field_cntrl),x);
                        }
                    }
                }
                else 
                {
                    add_member(&(new->sel_cntrl)," ");
                    add_member(&(new->field_cntrl),spare_buf);
                }
                break;
            }
        }
    }
    (void) fclose(fp);
    help_init(shape);
#ifdef DEBUG
    dump_tree();
#endif
    return;
}
/*
 * Output a message
 */
void mess_write(s,p,message)
struct screen * s;
struct template *p;
char * message;
{
#ifndef MINGW32
    beep();
#endif
    wmove(s->mess_zn,0,0);
    wclrtoeol(s->mess_zn);
    wprintw(s->mess_zn,"%-.*s", s->shape.mess_cols -1, message);
    (void) strncpy(p->mess_txt,message, s->shape.mess_cols -1);
    wrefresh(s->mess_zn);
    return;
}
/*
 * Define a form
 */
struct template * def_form(shape, parent,sel_allow,comm_allow,comm_type,
head,prompt,comm,mess,trigger_code,trigger_fun)
struct form_out_line * shape;
struct template * parent;
enum sel_allow sel_allow;
enum comm_allow comm_allow;
enum comm_type comm_type;
char * head;
char * prompt;
char * comm;
char * mess;
char * trigger_code;
void (*trigger_fun)();
{
 /*
  * allocate a template
  * Fill in the static bits
  */
 
struct template * plate, **x;
    if ((plate = (struct template *) calloc(1, sizeof(struct template)))
        == (struct template *) NULL)
        return (struct template *) NULL;
    plate-> i_state =  NO_DATA;               /* How far set-up has got */
    plate-> trigger_code =  trigger_code;
    plate-> trigger_fun =  trigger_fun;
    plate-> sel_allow =  sel_allow;
    plate-> comm_allow =  comm_allow;
    plate-> comm_type =  comm_type;
    plate-> parent_form =  parent;      /* To support a menu tree */
    plate->head_txt[sizeof(plate->head_txt) - 1] = '\0';
    plate->prompt_txt[sizeof(plate->prompt_txt) - 1] = '\0';
    plate->mess_txt[sizeof(plate->mess_txt) - 1] = '\0';
    memset(plate->head_txt,' ',sizeof(plate->head_txt) - 1);
    memset(plate->prompt_txt,' ',sizeof(plate->prompt_txt) - 1);
    memset(plate->mess_txt,' ',sizeof(plate->mess_txt) - 1);
    memcpy(plate->head_txt,head,strlen(head));
    memcpy(plate->prompt_txt,prompt,strlen(prompt));
    memcpy(plate->mess_txt,mess,strlen(mess));
    plate->comm_txt = comm;
    plate->men_cntrl.window_size = shape->men_lines;
    plate->sel_cntrl.window_size = shape->sel_lines;
    plate->field_cntrl.window_size = shape->field_lines;
    if ((x = find_form((char *) NULL)) == (struct template **) NULL)
        return (struct template *) NULL;
    *x = plate;
    return plate;
}
void get_os_out(screen,plate,comm,prompt)
struct screen * screen;
struct template * plate;
char * comm;          /* command to execute */
char * prompt;        /* header string if wanted */
{
char line_buf[80];
char next_buf[80];
    int ret;
    FILE * fp;
/*
 * Loop through the strings that are fed on the stream until
 * no more, stuffing them in a control structure
 */
    if ((fp = npopen(comm, "r")) == (FILE *) NULL)
        (void) strcpy(line_buf,"Couldn't execute command");
    else
    {
        struct template * new;
        if (fgets(line_buf,sizeof(line_buf),fp) != (char *) 0)
        {
            int i;
            if (fgets(next_buf,sizeof(line_buf),fp) == (char *) 0)
            {
                i = strlen(line_buf);
                if (*(line_buf+i-1) == '\n')
                    *(line_buf+i-1) = '\0';
                ret = npclose(fp);
            }
            else
            {
/*
 * Use def_form() to get a new form
 *
 * Use add_member() to add line_buf and next_buf to the scrolling region,
 * and loop
 * through picking up the others
 */
                new =  def_form(&screen->shape,plate,SEL_YES,COMM_YES,MENU,
             prompt,"Use ARROW keys to inspect, ^P to print, ^Z to exit",
"                                                                              \
 ","",(char *)NULL, (void (*)()) NULL);
                if (new == (struct template *) NULL)
                {
                    mess_write(screen,plate,"Cannot create new form");
                    return;
                }
                i = strlen(line_buf);
                if (*(line_buf+i-1) == '\n')
                    *(line_buf+i-1) = '\0';
                add_padded_member(&(new->men_cntrl),line_buf,
                                    screen->shape.men_cols);
                add_member(&(new->field_cntrl),"EXIT:");
                add_member(&(new->sel_cntrl)," ");
                i = strlen(next_buf);
                if (*(next_buf+i-1) == '\n')
                    *(next_buf+i-1) = '\0';
                add_padded_member(&(new->men_cntrl),next_buf,
                                    screen->shape.men_cols);
                add_member(&(new->field_cntrl),"EXIT:");
                add_member(&(new->sel_cntrl)," ");
                while (fgets(line_buf,sizeof(line_buf),fp) != (char *) NULL)
                {
                    i = strlen(line_buf);
                    if (*(line_buf+i-1) == '\n')
                        *(line_buf+i-1) = '\0';
                    add_padded_member(&(new->men_cntrl),line_buf,
                                        screen->shape.men_cols);
                    add_member(&(new->field_cntrl),"EXIT:");
                    add_member(&(new->sel_cntrl)," ");
                }
                ret = npclose(fp);
                (void) do_form(screen,new);
                destroy_form(new);
                dis_form(screen,plate);
                (void) sprintf(line_buf,"Returned Status: %d",ret);
            }
        }
        else
        {
            ret = npclose(fp);
            (void) sprintf(line_buf,"Returned Status: %d",ret);
        }
    }
    mess_write(screen,plate,line_buf);
    return;
}
/************************************************************************
 * Field edit routine
 * - Returns the function key pressed.
 */
int edit_str(win,str,y,len)
WINDOW * win;
char * str;
int y;
int len;
{
    int ch;
    int esc_flag = 0;   /* Are we inputting an absolute character? */
    int cur_x = 0;      /* Position in string */
    int scur_x = 0;     /* Position on screen allowing for control characters */
    char * x;
    int maxx;
    for (x=str, maxx = 0; *x != '\0' && maxx < len; maxx++, x++)
         if (((int) (*x & 0x7f)) < 32 && ((int) (*x & 0x7f)) > 0)
             maxx++;   /* How much are we using? */
    for (x=str + maxx; (*x == '\0' || *x == ' ') && maxx > 0; maxx--, x--);
    for (;;)
    {
        if (!esc_flag)
        {
            ch = wgetch(win);
#ifdef ULTRIX
            if (ch == 0)
            {    /* What is going on here ????? */
                ch = (int) 'p';
            }
#endif
            if ((ch > 127 && ch < 255) || ch < 0)
                ch &= 0x7f;
            switch (ch)
            {
            case 0x1f & 'V' :
                esc_flag = 1;
                break;
/*
 * Clear a field
 */
            case 0x1f & 'U' :
                {
                    short int i;
                    *(str) = '\0';
                    cur_x = 0;
                    scur_x = 0;
                    maxx = 0;
                    wmove(win,y,scur_x);
                    wclrtoeol(win);
                    for (i=cur_x; i < len; i++)
                        *(str + i) = '\0';
                }
                break;
            case 0x7f :
            case KEY_DC :
                if (cur_x > 0)
                {
                    if (*(str + cur_x) < 32 && *(str + cur_x) != 0)
                        scur_x--;
                    scur_x--;
                    cur_x--;
                    wmove(win,y,scur_x);
                }
                if (maxx > 0)
                    maxx--;
                wdelch(win);
                {
                    short int i;
                    if (*(str + cur_x) < 32 && *(str + cur_x) != 0)
                    {
                        wdelch(win);
                        if (maxx > 0)
                            maxx--;
                    }
                    for (i=cur_x;
                                 *(str+i) != '\0';
                                      i++)
                        *(str + i) = *(str + i + 1);
                }
                break;
            case KEY_LEFT :
                if (cur_x > 0)
                {
                    cur_x--;
                    if (*(str + cur_x) < 32 && *(str + cur_x) != 0)
                        scur_x--;
                    scur_x--;
                    wmove(win,y,scur_x);
                }
                break;
            case KEY_IC  :
                if (maxx < len -1 )
                {
                    char * x =  str + strlen(str);
                    char * x1 = str + cur_x;
                    while (x != x1)
                    {
                        *(x+1) = *x;
                        x++;
                    }
                    winsch(win,(chtype) ' '); 
                    *(str + cur_x) = ' ';
                    maxx++;
                }
                break;
            default :
                if (ch > 31 && ch < 128)
                {
                    if (*(str + cur_x) < 32 && *(str + cur_x) != 0)
                    {
                        wdelch(win);
                        if (maxx > 0)
                            maxx--;
                    }
                    *(str + cur_x) = ch;
                    waddch(win,(chtype) ch);
                }
                else
                    return ch;
            case KEY_RIGHT :
                if ( scur_x < len - ((*(str + cur_x) < 32) ? 2 : 1) )
                {
                    if (*(str + cur_x) < 32)
                        scur_x++;
                    cur_x++;
                    if (*(str + cur_x) == '\0')
                        *(str + cur_x) = ' ';
                    scur_x++;
                }
                wmove(win,y,scur_x);
                break;
            }
        }
        else
        {
            char c;
            read(0,&c,1);
            ch = (int) c;
            esc_flag = 0;
            ch &= 0x7f;
            if (ch > 31 || maxx < len - 1)
            {
                if (ch < 32)
                {
                    if (*(str + cur_x) > 31 || *(str + cur_x) == 0)
                    {
                        winsch(win, (chtype) ' ');
                        maxx++;
                    }
                    waddch(win, (chtype) '^');
                    waddch(win,(chtype) (ch + 64));
                }
                else
                    waddch(win,(chtype) ch);
                *(str + cur_x) = ch;
                if ( scur_x < len - 1)
                {
                    cur_x++;
                    if (*(str + cur_x) == '\0')
                        *(str + cur_x) = ' ';
                    if (ch < 32)
                        scur_x++;
                    scur_x++;
                    if (scur_x > maxx)
                        maxx = scur_x;
                }
                else
                    wmove(win,y,scur_x);
            }
        }
        wrefresh(win);
    }
}
/*******************************************************************
 * routine to handle the input of a search string on a form
 */
static char * inp_search(screen, plate, multi_sel, search_disp)
struct screen * screen;
struct template * plate;
enum multi_sel multi_sel;
char ** search_disp;
{
    int ch;
    int cur_dis = 0;  /* ranges from 0 to plate->comm_lines -1 */
    int cur_line = 0; /* ranges from 0 to
                              plate->sel_cntrl.member_cnt -1        */
    int top_line = 0;
    for(;;)
    {
static char * cur_comm="                                                                                     ";
        static char expbuf[ESTARTSIZE];
        wmove(screen->comm_zn,cur_dis,0);
        wrefresh(screen->comm_zn);
        ch = edit_str(screen->comm_zn,cur_comm,cur_dis,
                          screen->shape.comm_cols -1);
 
        switch(ch)
        {
            case EOF :
                if (natmenu)
                    catch_signal();   /* Does not return */
                else
                {
                    *search_disp = "EXIT:";
                    return (char *) NULL;
                }
#ifdef ATT
            case KEY_HELP :
#else
#ifdef AIX
            case KEY_HLP :
#else
            case KEY_F(1) :
#endif
#endif
            {
                struct template ** new_form =
                             find_form("COMM_HELP:"); 
                if (new_form != (struct template **) NULL)
                {
                    do_form(screen,*new_form);
                    dis_form(screen,plate);
                }
                else
                    break;
            }
            case 'P' & 0x1f :
            case KEY_PRINT :
                print_form(screen,plate);
                break;
            case 'R' & 0x1f :
                wrefresh(curscr);
                break;
            case 'Z' & 0x1f :
                    *search_disp = "EXIT:";
                    return (char *) 0;
            case KEY_HOME :
#ifdef AIX
        case KEY_NEWL :
        case KEY_DO :
#endif
#ifdef OSF
        case KEY_ENTER :
#endif
            case '\r' :
            case '\n' :
/*
 * Do this action
 */
                TRAIL_SPACE_STRIP(cur_comm);
                if (re_comp(cur_comm,expbuf,(long int) sizeof(expbuf)) >
                          (char *) sizeof(expbuf))
                {
                    mess_write(screen,plate,"Invalid Regular Expression");
                    continue;
                }
                else
                {
                struct template ** new_form;
                    if (multi_sel == MULTI_YES)
                        new_form = find_form("MULTI_SEARCH:"); 
                    else
                        new_form = find_form("SINGLE_SEARCH:"); 
                    if (new_form != (struct template **) NULL)
                    {
                        *search_disp = do_form(screen,*new_form);
                        dis_form(screen,plate);
                    }
                    if (*search_disp == (char *) NULL)
                        *search_disp = "EXIT:";
                    return expbuf;
                }
            case 0x1f & 'H' :
            case '\t' :
#ifdef AIX
         case KEY_TAB :
#endif
            case KEY_UP :
                *search_disp = "EXIT:";
                return (char *) 0;
        }
    }
}
/************************************************************
 * Process a search request
 */
void proc_search(screen,plate, multi_sel, cur_line, top_line, cur_dis)
struct screen * screen;
struct template * plate;
enum multi_sel multi_sel;
short * cur_line;
short * top_line;
short * cur_dis;
{
    char * search_buf;
    char * match_text;
    short int last_line = (plate->sel_cntrl.member_cnt - 1);
    short int search_line;
    char *search_disp;
    search_buf = inp_search(screen,plate,multi_sel, &search_disp);
    
    if (!strcmp(search_disp,"FORW:"))
    {
        for (search_line = *cur_line;
                search_line <= last_line;
                    search_line++)
        {
            match_text =
              find_member( &(plate->men_cntrl),search_line);
             
            if (re_exec(match_text,search_buf) > 0)
            {
                *cur_line = search_line;
                *top_line = search_line;
            }
        }
    }
    else
    if (!strcmp(search_disp,"BACK:"))
    {
        for (search_line = *cur_line;
                search_line >= 0;
                    search_line--)
        {
            match_text =
              find_member( &(plate->men_cntrl),search_line);
             
            if (re_exec(match_text,search_buf) > 0)
            {
                *cur_line = search_line;
                *top_line = search_line;
            }

        }
    }
    else
    if (multi_sel == MULTI_YES && (
        !strcmp(search_disp,"MALL:")
      ||!strcmp(search_disp,"MOTH:")))
    {
        int wanted_rv;
        *cur_line = 0;         /* Wrap at bottom */
        *top_line = 0;
        *cur_dis = 0;
        if (!strcmp(search_disp,"MALL:"))
        {
            wanted_rv = 1;
        }
        else
            wanted_rv = 0;
        for (search_line = 0;
                search_line <= last_line;
                    search_line++)
        {
            match_text =
              find_member( &(plate->men_cntrl),search_line);
             
            if ( re_exec(match_text,search_buf) == wanted_rv)
                chg_member(&(plate->sel_cntrl),search_line,"*");
            else
                chg_member(&(plate->sel_cntrl),search_line," ");
        }
        dis_form(screen,plate);
    }
    return;
}
/*********************************************************************
 * Menu Selection
 */
char * inp_sel(screen,plate,multi_sel)
struct screen * screen;
struct template * plate;
enum multi_sel multi_sel;
{
    short int menu_size;
/*
 * On entry to this routine, the lists of options
 * are assumed to have been initialised.
 * Loop to process all characters.
 */
    menu_size = (screen->shape.sel_lines >
                                  plate->sel_cntrl.member_cnt) ?
                               plate->sel_cntrl.member_cnt :
                               screen->shape.sel_lines;
    for(;;)
    {
        int ch;
        short int cur_dis = 0; /* ranges from 0 to screen->shape.sel_lines -1 */
        short int cur_line = 0;  /* ranges from 0 to
                              plate->sel_cntrl.member_cnt -1        */
        short int top_line = 0;
        wmove(screen->sel_zn,cur_dis,0);
        for(;;)
        {
            wnoutrefresh(screen->men_zn);
            wnoutrefresh(screen->sel_zn);
            doupdate();
            ch = wgetch(screen->sel_zn);
#ifdef AIX
            if (ch > 127 && ch < 255)
                ch &= 0x7f;
#endif
            switch(ch)
            {
            case EOF :
                if (natmenu)
                    catch_signal();   /* Does not return */
                else
                    return "EXIT:";
            case 'R' & 0x1f :
                wrefresh(curscr);
                break;
            case 'P' & 0x1f :
            case KEY_PRINT :
                print_form(screen,plate);
                break;
/* Go to top of menu */
            case KEY_HOME :
            case 0x1f & 'B' :
                cur_line = 0;
                top_line = 0;
                cur_dis  = 0;
                break;
/* Go to bottom of menu */
            case KEY_EOS :
            case 0x1f & 'F' :

                cur_line = (plate->sel_cntrl.member_cnt - 1);
                if (plate->sel_cntrl.member_cnt <= screen->shape.sel_lines)
                    top_line = 0;
                else
                    top_line = plate->sel_cntrl.member_cnt - menu_size;
                cur_dis  = menu_size - 1;
                break;
/* Backward page */
            case KEY_PPAGE :
            case 0x1f & 'U' :
                if (cur_line == top_line)
                {
                   if (top_line > screen->shape.sel_lines)
                       top_line -= screen->shape.sel_lines;
                   else
                       top_line = 0;
                }
                cur_line = top_line;
                cur_dis  = top_line;
                break;
/* Forward page */
            case KEY_NPAGE :
            case 0x1f & 'D' :
                if (cur_dis == menu_size - 1)
                {
                    if (top_line + menu_size * 2 > plate->sel_cntrl.member_cnt)
                        top_line = plate->sel_cntrl.member_cnt - menu_size;
                    else 
                    if (cur_line != plate->sel_cntrl.member_cnt - 1)
                        top_line += menu_size;
                }
                cur_line = top_line + menu_size - 1;
                cur_dis  = menu_size - 1;
                break;
            case KEY_F(16) :
#ifdef AIX
        case KEY_NEWL :
        case KEY_DO :
#endif
#ifdef OSF
        case KEY_ENTER :
#endif
            case '\r' :
                if (multi_sel == MULTI_YES)
                {
/*
 * Accept the action
 */
                    if (natmenu)
                        inp_comm(screen,plate);
                    else
                        return do_os(screen,plate,plate->comm_txt,
                                           plate->prompt_txt,MULTI_YES);
                }
/*
 * Come on here if a multi-select has not been done
 */
#ifdef ATT
            case KEY_SELECT :
#else
#ifdef AIX
            case KEY_SEL :
#endif
#endif
            case '*' :
            case '\n' :
                if (multi_sel == MULTI_YES)
                {
/*
 * Mark the item as selected
 */
                    wdelch(screen->sel_zn);
                    winsch(screen->sel_zn, (chtype) '*');
                    chg_member(&(plate->sel_cntrl),cur_line,"*");
                    cur_dis++;
                    scroll_done = FALSE;
                    scr_display(screen->men_zn,
                                    &(plate->sel_cntrl),
                                    top_line,cur_line,OLD);
                    scr_display(screen->sel_zn,
                                    &(plate->sel_cntrl),
                                    top_line,cur_line,OLD);
                }
                else
                {
/*
 * Do this action
 * MULTI_NO; must be a Menu in the current version
 */
                    if (plate->comm_type == MENU)
                    {
                        char * comm;
                        comm = find_member(&(plate->field_cntrl),cur_line);
                        if (comm == (char *) NULL)
                            if (natmenu)
                                comm = "NYI:";
                            else
                                comm = "EXIT:";
                        if (!strcmp("EXIT:",comm))
                        {
                            if (!natmenu)
                                return comm;
                            if (plate->parent_form == (struct template *) NULL)
                                 feed_back(comm);
                            else return;
                        }
                        else if (!strcmp("NULL:",comm))
                        {
                            char * x;
                            if (natmenu)
                                return (char *) NULL;
                            else
                            if ((x = find_member(&(plate->men_cntrl),
                                                 cur_line)) !=
                                 (char *) NULL)
                                 strcpy(null_select, x);
                            else
                                 null_select[0] = '\0';
                        }
                        else if (!strcmp("FORW:",comm))
                        {
                            return comm;
                        }
                        else if (!strcmp("BACK:",comm))
                        {
                            return comm;
                        }
                        else if (!strcmp("MALL:",comm))
                        {
                            return comm;
                        }
                        else if (!strcmp("MOTH:",comm))
                        {
                            return comm;
                        }
                        else if (!strcmp("NYI:",comm))
                            mess_write(screen,plate,"Unsupported Feature");
                        else
                        {
                            struct template ** new_form = find_form(comm); 
                            if (new_form != (struct template **) NULL)
                                do_form(screen,*new_form);
                            else
                            if (plate->comm_allow == COMM_YES &&
                                 *(comm +strlen(comm) - 1) != ':')
                            {
                                char * prompt;
                                if ((prompt =
                                   find_member(&(plate->men_cntrl),cur_line)) ==
                                           (char *) NULL)
                                    prompt = comm;
                                mess_write(screen,plate,
                                           "Executing Command....");
                                if (natmenu)
                                    do_os(screen,plate,comm,prompt,DO_SOME);
                                else
                                    return comm;
                            }
                            else
                            if (natmenu)
                                feed_back (comm);
                            else
                            {
                                if (plate->trigger_fun != (void (*)()) NULL)
                                {
                                    (*plate->trigger_fun)(screen,plate);
                                    return "EXIT:";
                                }
                                else
                                    return comm;
                            }
                            goto breakout;
                        }
                    }
                    else
                    {
                        mess_write(screen,plate,"Executing Command....");
                        return do_os(screen,plate,plate->comm_txt,
                                     plate->prompt_txt, DO_SOME);
                    }
                }
                break;
#ifdef ATT
            case KEY_HELP :
#else
#ifdef AIX
            case KEY_HLP :
#endif
#endif
            case '?' :
            {
                struct template ** new_form;
                if (multi_sel == MULTI_YES)
                    new_form = find_form("MULTI_HELP:"); 
                else
                    new_form = find_form("SINGLE_HELP:"); 
                if (new_form != (struct template **) NULL)
                {
                    do_form(screen,*new_form);
                    goto breakout;
                }
                else
                    break;
            }
/*
 * Searching for items
 */
            case '/' :
#ifdef ATT
#ifndef OSF
            case KEY_FIND :
#endif
#endif
                proc_search(screen, plate, multi_sel, &cur_line,
                                 &top_line, &cur_dis);
                break;
            case '\t' :
#ifdef AIX
         case KEY_TAB :
#endif
            case KEY_DOWN :
            case 'j' :
            case '+' :
                if (cur_line == (plate->sel_cntrl.member_cnt - 1))
                {
                    cur_line = 0;         /* Wrap at bottom */
                    top_line = 0;
                    cur_dis = 0;
                }
                else
                {
                    cur_line ++;
                    if (cur_line == top_line + screen->shape.sel_lines)
                        top_line++;
                    else cur_dis++;
                }
                break;
            case 0x1f & 'H' :
#ifdef AIX
        case KEY_BTAB :
#endif
            case KEY_UP :
            case 'k' :
            case '-' :
                if (cur_line == 0)                /* Wrap at top */
                {
                    cur_line = plate->sel_cntrl.member_cnt - 1;
                    if (cur_line > top_line + screen->shape.sel_lines)
                    {
                        cur_dis = menu_size -1;
                        top_line = cur_line - menu_size + 1;
                    }
                    else
                        cur_dis = cur_line - top_line;
                }
                else
                {
                    cur_line --;
                    if (cur_line < top_line)
                        top_line--;
                    else cur_dis --;
                }
                break;
            case ' ' :
            case KEY_DC :
                if (multi_sel == MULTI_YES)
                {
                    wdelch(screen->sel_zn);
                    winsch(screen->sel_zn,' ');
                    chg_member(&(plate->sel_cntrl),cur_line," ");
                }
                break;
            case 0x1f & 'Z' :
                if (plate->parent_form == (struct template *) NULL)
                              feed_back("EXIT:");
                    else return;
            default :
#ifdef DEBUG
            {   /* Used to investigate puzzling arrow keys */
                char buf[20];
                sprintf(buf,"Key = %d", ch);
                mess_write(screen,plate,buf);
            }
#endif
                break;
            }
            scroll_done = FALSE;
            scr_display(screen->men_zn,
                                    &(plate->men_cntrl),
                                    top_line,cur_line,OLD);
            scr_display(screen->sel_zn,
                                    &(plate->sel_cntrl),
                                    top_line,cur_line,OLD);
        }
breakout:
        dis_form(screen,plate);
    }
}
/*
 * routine to handle the input of fields on a form
 */
char * inp_field(screen,plate)
struct screen * screen;
struct template * plate;
{
/*
 * On entry to this routine, the lists of options
 * are assumed to have been initialised.
 */
    enum new_flag new_flag;
    int ch;
    int cur_dis = 0;  /* ranges from 0 to screen->shape.sel_lines -1 */
    int cur_line = 0; /* ranges from 0 to
                              plate->sel_cntrl.member_cnt -1        */
    int top_line = 0;
    wmove(screen->sel_zn,0,0);
    wclrtobot(screen->sel_zn);
    wnoutrefresh(screen->sel_zn);
    for(;;)
    {
#ifdef DEBUG
        char buf[20];
#endif
        char * cur_field;
        new_flag = OLD;
        wmove(screen->field_zn,cur_dis,0);
        wnoutrefresh(screen->men_zn);
        wnoutrefresh(screen->field_zn);
        doupdate();
        cur_field = find_member(&(plate->field_cntrl),cur_line);
        if (cur_field == (char *) NULL)
        {
            mess_write(screen,plate,"Logic Error: No FIELD");
            if (natmenu)
                catch_signal();   /* Does not return */
            else
                return "EXIT:";
        }
        ch = edit_str(screen->field_zn, cur_field, cur_dis,
                                                   screen->shape.field_cols -1);
#ifdef DEBUG
                    sprintf(buf,"Key No: %d",ch);
                    mess_write(screen,plate,buf);
#endif
        switch(ch)
        {
        case EOF :
            if (natmenu)
                catch_signal();   /* Does not return */
            else
                return "EXIT:";
        case 'R' & 0x1f :
            wrefresh(curscr);
            touchwin(screen->field_zn);
            wrefresh(screen->field_zn);
            break;
        case 'P' & 0x1f :
        case KEY_PRINT :
            print_form(screen,plate);
            break;
        case KEY_HOME :
#ifdef AIX
        case KEY_NEWL :
        case KEY_DO :
#endif
#ifdef OSF
        case KEY_ENTER :
#endif
        case '\r' :
        case '\n' :
/*
 * Do this action
 */
            if (natmenu)
            {
                mess_write(screen,plate,"Executing Command....");
                do_os(screen,plate,plate->comm_txt,plate->prompt_txt,
                        DO_ALL);
            }
            else
            if (plate->trigger_fun != (void(*)()) NULL)
                (*plate->trigger_fun)(screen,plate);
            return "EXIT:";
#ifdef ATT
        case KEY_HELP :
#else
#ifdef AIX
        case KEY_HLP :
#else
        case KEY_F(1) :
#endif
#endif
        {
            struct template ** new_form = find_form("FIELD_HELP:"); 
            if (new_form != (struct template **) NULL)
            {
                do_form(screen,*new_form);
                dis_form(screen,plate);
            }
            else
                break;
        }
        case '\t' :
#ifdef AIX
         case KEY_TAB :
#endif
        case KEY_DOWN :
            if (cur_line == (plate->field_cntrl.member_cnt - 1))
            {
                char * x;
                if (plate->comm_allow == COMM_NO)
                    break;
                if ((x = find_member(&(plate->men_cntrl),cur_line))
                            == (char *) NULL) break;
                add_member(&(plate->men_cntrl),x);
                add_padded_member(&(plate->field_cntrl), " ",
                      screen->shape.field_cols);
                add_member(&(plate->sel_cntrl)," ");
                new_flag = NEW;
            }
            cur_line ++;
            if (cur_line == top_line + screen->shape.sel_lines)
            {
                top_line++;
/*
 *                      touchwin(screen->field_zn);
 */
            }
            else
                cur_dis++;
            break;
        case 0x1f & 'H' :
#ifdef AIX
        case KEY_BTAB :
#endif
        case KEY_UP :
            if (cur_line == 0)
                    break;         /* ignore at top */
            cur_line --;
            if (cur_line < top_line)
            {
                    top_line--;
/*
 *                      touchwin(screen->field_zn);
 */
            }
            else cur_dis --;
            break;
        }
        scroll_done = FALSE;
        scr_display(screen->men_zn, &(plate->men_cntrl),
                                    top_line,cur_line,new_flag);
        if (new_flag == NEW)
            touchwin(screen->field_zn);
        scr_display(screen->field_zn, &(plate->field_cntrl),
                                    top_line,cur_line,new_flag);
    }
    return;
}
/**********************************************************************
 * Execute an operating system command
 *
 * Assemble the command with reference to the arguments
 * If COMM_YES, Call the get_os_out() routine to execute it
 * Else Feed the data back
 * 
 */
char * do_os(screen,plate,comm,prompt,selective)
struct screen * screen;
struct template * plate;
char * comm;
char * prompt;
enum selective selective;
{
static char buf[ARG_MAX+1];
    char * got_to;
    short int i;
    got_to = norm_str(buf, comm);
    for (i = 0;
             i < plate->field_cntrl.member_cnt;
                 i++)
    {
        if (selective == DO_ALL ||
                 strcmp("*",find_member( &(plate->sel_cntrl),i)) == 0)
        {
            char * x;
            x = find_member(&(plate->field_cntrl),i);
            if (x != (char *) NULL &&
                  strlen(x) + (got_to - buf - 1) < sizeof(buf))
                got_to = norm_str(got_to, x);
        }
    }
    if (!strlen(buf))
        return buf;
    if (plate->comm_allow == COMM_YES)
    {   /* execute the command from within the menu system */
        get_os_out(screen,plate,buf,prompt);
        return buf;
    }
    else if (natmenu)
        feed_back(buf);
    else
    {
        buf[0] = '\0';
        return buf;
    }
}
/*
 * routine to action commands on a form
 */
void inp_comm(screen,plate)
struct screen * screen;
struct template * plate;
{
/*
 * On entry to this routine, the lists of options
 * are assumed to have been initialised.
 */
    int ch;
    int cur_dis = 0;  /* ranges from 0 to plate->comm_lines -1 */
    int cur_line = 0; /* ranges from 0 to
                              plate->sel_cntrl.member_cnt -1        */
    int cur_x = 0;
    int top_line = 0;
    char * cur_field;
static char * cur_comm="                                                                                     ";
/*
 * Do this action
 */
    mess_write(screen,plate,"Executing Command....");
    plate->comm_txt = cur_comm;
    do_os(screen,plate,plate->comm_txt,plate->prompt_txt,
                            DO_SOME);
    return;
}
/*
 * Feed back the output to the invoking shell, using file descriptor 4
 * Unless we are searching.....
 */
void feed_back(comm)
char * comm; /* DOES NOT RETURN */
{
    FILE * output_fp = fdopen(4,"w");
    if (output_fp != (FILE *) NULL)
        (void) fwrite(comm,sizeof(char),strlen(comm),output_fp);
    (void) fclose(output_fp);
#ifdef OSF
    unset_curses_modes();
#endif
    endwin();
    exit(0);
}
/*
 * Stop users killing the thing
 */
void catch_signal()
{
    feed_back("EXIT:"); /* Does not return */
}
