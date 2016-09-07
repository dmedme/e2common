/**************************
 * hashlib.h - header for routines for maintaining hash tables
 * @(#) $Name$ $Id$
 * Copyright (c) E2 Systems 1993
 */
#ifndef HASHLIB_H
#define HASHLIB_H
#ifndef ANSIARGS
#define ANSIARGS(x) ()
#endif
typedef unsigned (* HASHFUNC) ANSIARGS((char*,int));
                                          /* routine to use for hashing */
typedef int (* COMPFUNC) ANSIARGS((void*,void*));
                                          /* comparison routine; returns
                                           -1 (first less than second)
                                           0 (match)
                                           +1 (second less than first)
                                         */
typedef struct _hipt {
char * name;
char * body;
struct _hipt * next;
short int in_use;
} HIPT;
typedef struct _hash_con {
    HIPT * table;
    int tabsize;
    HASHFUNC hashfunc;
    COMPFUNC compfunc;
} HASH_CON;
/*
 * Iterator that treats the hash table as a collection
 */
typedef struct _hiter {
    HASH_CON * hp;
    int i;
    HIPT * cp;
} HITER;
/*
 * Routines to maintain a hash table where the
 * key value is an integer, not a string
 */ 
HASH_CON * hash ANSIARGS((int sz, HASHFUNC hash_to_use, COMPFUNC comp_to_use));
/*
 * Add an entry to the hash table
 */
HIPT * insert ANSIARGS((HASH_CON * con, char * item, char * data));
/*
 * Function to delete element
 */

int hremove ANSIARGS((HASH_CON *con,char *item));
/*
 * Function to find element
 */

HIPT * lookup ANSIARGS((HASH_CON *con, char *p));
/*
 * Do something to everything
 */
void iterate ANSIARGS((HASH_CON * con, void(*)(), void(*)() ));
/*
 * Get rid of everything
 */
void cleanup ANSIARGS((HASH_CON * con));
/*
 * Hash function for string keys
 */

unsigned long int string_hh ANSIARGS(( char * w, int modulo));
unsigned long int stringi_hh ANSIARGS(( char * w, int modulo));
/*
 * Hash function for long keys
 */
unsigned long int long_hh ANSIARGS((char *w, int modulo));
/*
 * Comparison of two ints
 */
int icomp ANSIARGS(( int i1, int i2));
/*
 * Create an iterator
 */
HITER * hiter ANSIARGS((HASH_CON * hp));
/*
 * Next Value
 */
HIPT * hiter_next ANSIARGS((HITER * hp));
/*
 * Zap Value; clear entry and advances to the next one
 */
HIPT * hiter_zap ANSIARGS((HITER * hp));
/*
 * Go back to the beginning
 */
HIPT * hiter_reset ANSIARGS((HITER * hp));
/*
 * Empty the table
 */
void hempty ANSIARGS((HASH_CON * hp));
#endif
