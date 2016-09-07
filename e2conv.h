/*
 * e2conv.h - Miscellaneous format conversion routines to deal with
 * odd formats, eg. ORACLE-specific, packed decimal
 * @(#) $Name$ $Id$
 * Copyright  ANSIARGS((c)); E2 Systems Limited 1994
 */
#ifndef E2CONV_H
#define E2CONV_H
struct iocon {
     struct iocon * next_iocon;
     struct iocon * child_iocon;
     int occs;   /* Number of occurrences */
     int flen;   /* Input format length */
     int alen;   /* Actual binary file length */
     char* (*getfun)(/* void *,int */);
     char* (*putfun)(/* void *, void*, int */);
};
struct fld_descrip {
    unsigned char * fld;
    int len;
};
/*
 *   - T, Time or date. For the moment, nothing. Need to decide on the
 *     internal representation of dates.
 */
/**********************************
 * Read a field
 *
 * Routine that is a cross between strtok ANSIARGS(() and strchr());
 * Splits string out into fields.
 * Differences from strtok ANSIARGS(());
 * - second parameter is a char, not a series of chars
 * - returns a zero length string for adjacent separator characters
 * - handles an escape character for the separators, '\'
 */
#include "ansi.h"
#ifdef LCC
unsigned char * nextasc();
unsigned char * nextasc_r();
#else
unsigned char * nextasc ANSIARGS((unsigned char * start_point,unsigned char sepchar,unsigned char escchar));
unsigned char * nextasc_r ANSIARGS((unsigned char * start_point,unsigned char sepchar,unsigned char escchar, unsigned char ** got_to, unsigned char * ret_buf, unsigned char * limt));
#endif
void one_rec_ab();
void one_rec_ba();
/*
 *   - X, characters
 */
unsigned char * xin ANSIARGS((unsigned char * buf_ptr,int in_len));
unsigned char * xin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * xout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 *   - I, integers
 */
unsigned char * iin ANSIARGS((unsigned char * buf_ptr, int in_len));
unsigned char * iin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * iout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 *   - H, hexadecimal string
 */
unsigned char * hexin ANSIARGS((unsigned char * buf_ptr, int in_len));
unsigned char * hexin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * hexout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 *   - Z, zoned  ANSIARGS((i.e. numeric characters with a trailing sign ID));
 *     There may be more to these than this.The final characters can be
 *     any of a number of alphabetic characters, I think; does it indicate
 *     the mantissa?
 */
unsigned char * zin ANSIARGS((unsigned char * buf_ptr, int in_len));
unsigned char * zin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * zout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 *   - S, C strings
 */
unsigned char * stin ANSIARGS((unsigned char * buf_ptr, int in_len));
unsigned char * stin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * cstin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * stout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 *   - U, Unicode strings
 */
unsigned char * unin ANSIARGS((unsigned char * buf_ptr, int in_len));
unsigned char * unin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * unout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 *   - P, packed
 */
unsigned char * pin ANSIARGS((unsigned char * buf_ptr, int in_len));
unsigned char * pin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * pout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 *   - D, Double
 */
unsigned char * din ANSIARGS((unsigned char * buf_ptr, int in_len));
unsigned char * din_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * dout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 *   - F, Float  ANSIARGS((single precision));
 */
unsigned char * fin ANSIARGS((unsigned char * buf_ptr, int in_len));
unsigned char * fin_r ANSIARGS((unsigned char * buf_ptr,int in_len, unsigned char * buf, unsigned char * top));
unsigned char * fout ANSIARGS((unsigned char * out_buf, unsigned char * in_buf, int out_len));
/*
 * Render an ORACLE-internal number in characters.
 *
 * The input argument is a pointer to a 1 byte length, which is followed
 * by the exponent byte and the mantissa.
 */
short int ora_num ANSIARGS((unsigned char *in,unsigned char *out,int len));
/*****************************************************************
 * Expand a modulus 100 representation of a number, most significant half-byte
 * first.
 */
void ordate ANSIARGS((unsigned char *t, unsigned char *f));
unsigned char * retordate ANSIARGS((double f));
/****************************************************************************/
unsigned char * gen_handle ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char * top, int write_flag));
unsigned char * gen_handle_nolf ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char * top, int write_flag));
unsigned char * gen_handle_no_uni ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char * top, int write_flag));
unsigned char * gen_handle_no_uni_nolf ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char * top, int write_flag));
unsigned char * hex_in_out ANSIARGS(( unsigned char * out, unsigned char * in));
void hex_out ANSIARGS((FILE * ofp, unsigned char *b, unsigned char * top));
void hex_line_out ANSIARGS((FILE * ofp, unsigned char *b, unsigned char * top));
unsigned char * bin_handle ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char *top, int write_flag));
unsigned char * bin_handle_no_uni ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char *top, int write_flag));
unsigned char * bin_handle_no_uni_cr ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char *top, int write_flag));
unsigned char * uni_handle ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char *top, int write_flag));
unsigned char * asc_handle ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char *top, int write_flag));
unsigned char * asc_handle_nocr ANSIARGS((FILE * ofp, unsigned char *p,
    unsigned char *top, int write_flag));
unsigned char * get_cnt ANSIARGS((unsigned char *in_buf, int len_flag,unsigned  int *act_len));
void asc2uni ANSIARGS((unsigned char *in_buf,unsigned char *out_buf, unsigned int len));
void uni2asc ANSIARGS((unsigned char *in_buf,unsigned char *out_buf, unsigned int len));
int b64enc ANSIARGS((int len, unsigned char *in_buf,unsigned char *out_buf));
int b64dec ANSIARGS((int len, unsigned char *in_buf,unsigned char *out_buf));
int url_escape ANSIARGS((unsigned char *, unsigned char *, int, int));
int xml_unescape ANSIARGS((unsigned char *,  int));
int url_unescape ANSIARGS((unsigned char *, int));
int get_bin ANSIARGS((unsigned char *, unsigned char *, int));
int hex_to_int ANSIARGS((char *,  int));
int memspn ANSIARGS((unsigned char*, unsigned char*, int,unsigned char *));
int memcspn ANSIARGS((unsigned char*, unsigned char*, int,unsigned char *));
void other_end ANSIARGS((unsigned char *, unsigned int));
/*
 * Output line length for line wrapping. For years this was 79, but now
 * increased.
 */
#define E2_OLEN 127
#endif
