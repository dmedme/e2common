/*
 * Support for an implemention of a Boyes-Moore scan of buffer data
 * @(#) $Name$ $Id$ Copyright (c) E2 Systems 2009
 */
#ifndef BMMATCH_H
#define BMMATCH_H
struct bm_table {
    int match_len;
    unsigned char * match_word;
    unsigned char * altern;
    int forw[256];
};
/*
 * Structure for supporting multiple concurrent buffered use of the Boyes-Moore
 * Scan
 */
struct bm_frag {
    struct bm_table *bp;
    unsigned char * (*bm_routine)();
    int tail_len;
    unsigned char * tail;
};
struct bm_frag * bm_frag_init();
struct bm_table * bm_compile();
struct bm_table * bm_compile_bin();
unsigned char * bm_match();
unsigned char * bm_match_new();
struct bm_table * bm_casecompile();
struct bm_table * bm_casecompile_bin();
#define bm_casematch bm_match
#define bm_casematch_new bm_match_new
unsigned char * bm_match_frag();
void bm_zap();
void bm_frag_zap();
#endif
