/*
 * natmenu.c - forms driver program
 *
 * This program is designed to be used from within the Bourne
 * shell:
 * - parameters
 *   - none
 * - file descriptors 0,1,2
 *   - the terminal
 * - file descriptor 3
 *   - input forms to process
 * - file descriptor 4
 *   - option selected
 *
 * In order to achieve this effect in line, the syntax must follow this
 * example:
 opt=`natmenu 3<<EOF 1>&4 </dev/tty >/dev/tty
 HEAD=EXAMPLE: Form to fill in
 PROMPT=Make your selection, and hit RETURN
 SCROLL
 First selection/1
 Second selection/2
 Third selection/3`
 *
 * This will:
 *  - clear the screen
 *  - Display the header on the top line
 *  - Display three lines of options
 *  - Display the prompt at the bottom
 *
 * When the user hits return, the Shell variable opt will contain
 * the selected value
 *
 * Note that shell variable substitution will be applied to the form text,
 * so that date, user id etc. can be readily incorporated in the boiler
 * plate, and indeed the whole form can be generated dynamically.
 */ 
static char * sccs_id = "@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1992";
#include <limits.h>
#include <ctype.h>
#include <sys/types.h>
#ifndef V32
#include <sys/wait.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include "ansi.h"
#include "natmenu.h"
/*
 * Routine to process an operating system command
 *
 * Assemble the command with reference to the arguments
 * Calls the get_os_out() routine to execute it
 */

void catch_signal();
struct template * menu_tree[FORMS_MAX];
char null_select[133];
bool scroll_done;
static struct form_out_line shape;

/************************************************************************
 * Main program starts here
 */
int main(argc,argv,envp)
int argc;
char ** argv;
char ** envp;
{
    struct screen screen;
    FILE * input_form;
    natmenu = 1;
    sigset(SIGHUP,catch_signal);
    sigset(SIGQUIT,catch_signal);
    sigset(SIGTERM,catch_signal);
    sigset(SIGINT,catch_signal);
/*
 * Initialise the screen
 */
    screen = setup_form();
#ifdef OSF
    set_curses_modes();
#endif
/*
 * Process User Requests
 */
    if ((input_form = fdopen(3,"r")) != (FILE *) NULL)
    {
        read_form(input_form, &shape);
        if (*menu_tree != (struct template *) NULL)
            do_form(&screen,*menu_tree);   /* DOES NOT RETURN */
    }
#ifdef OSF
    unset_curses_modes();
#endif
    endwin();
    exit(1);
}
/***************************************************************************
 * Major Functions needed:
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
struct screen setup_form()
{
struct screen screen;
int pad_mode;
int e2lines;
int e2cols;
    WINDOW * full_scr;
    char *x;
    pad_mode = TRUE;
    if ((x = getenv("LINES")) == (char *) NULL)
        e2lines = DEF_LINES;
    else
        e2lines = atoi(x);
    if ((x = getenv("COLUMNS")) == (char *) NULL)
        e2cols = DEF_COLUMNS;
    else
        e2cols = atoi(x);
    setup_shape(&shape, e2lines,e2cols,0,0);
    full_scr = initscr();
    screen = setup_wins(&shape);
    screen.full_scr = full_scr;
    keypad(screen.full_scr,pad_mode);
    crmode();
    nonl();
    noecho();
    return screen;
}
