/*
 * difflib.h - Comparator logic for the E2 System Schema Comparator
 *         @(#) $Name$ $Id$
 *         Copyright (c) E2 Systems Limited 1999, 2009
 */
/*
 * Control structure
 */
struct diffx_con {
    int arrdim;       /* The array sizes                                      */
    long int *match1; /* Matching elements in list 1 for a given list 2 index */
    long int *match2; /* Matching elements in list 2 for a given list 1 index */
    long int *backptr;/* List 1-2 match index for a given element 1 index     */
    long int *forwptr;/* List 2 index for a given list 1-2 match              */
    long int *rank;   /* The list 2 rank for a given list 1-2 match           */
    long int l1cnt;   /* Count of list 1 elements                             */
    long int l2cnt;   /* Count of list 2 elements                             */
    long int nextr;   /* Count of matching entries in rank array              */
    char **ln1;       /* List of pointers to list 1 elements (0th is count)   */
    char **ln2;       /* List of pointers to list 2 elements (0th is count)   */
};
/*
 * Functions that must be provided as arguments to diffx_alloc()
 * VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 * el_read  -  The name of a function that takes parameters like fgets()
 *             (that is, buffer, size of buffer, and channel) and that
 *             'reads' an element into a buffer from a channel, and which
 *             returns NULL at logical EOF
 * el_len   -  The name of a function that can return the length of an element
 * el_cmp   -  The name of a function that can compare two elements, and which
 *             returns -1, 0 or +1, depending on whether the first element is
 *             less than, equal to or greater than the second. 
 */
struct diffx_con * diffx_alloc(/* char * fp1, char * fp2, char * (*el_read)(char *, int, char *), int max_len, int (* el_cmp)( char *, char *), int (*el_len)(char *) */ );
void diffx_cleanup(/* struct diffx_con * */ );
void diff_disp_rep( /* FILE *, struct diffx_con *, enum output_option, void (*)() */ );
enum output_option { XEDIT, FULL_LIST, DIFFERENCES };
void diffrep( /* FILE *, struct diffx_con *, enum output_option, int (*)( char *, FILE * ) */);
