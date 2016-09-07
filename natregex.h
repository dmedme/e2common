/* natregex.h - typedef's for use with the Regular Expression stuff
 * @(#) $Name$ $Id$
 */

#ifndef NATREGEX_H
#define NATREGEX_H
/*
 * Linked list element for memory owned.
 */
struct mem_con {
    char * owned;
    struct mem_con * next;
    void (*special_routine)();
    int  extra_arg;
};
struct mem_con * mem_note();
struct mem_con * descr_note();
#ifdef OSF
#define re_comp e2re_comp
#define re_exec e2re_exec
#endif
typedef char * reg_comp;	/* typedef for pointer to compiled regular
				   expression
				 */

extern char * re_comp();	/* routine that compiles regular expressions
				 */

extern int re_exec();		/* routine that matches regular expressions
				 */


#define ESTARTSIZE 256		/* first try at buffer for a regular
				   expression */
#endif

