/*
 * bmmatch.c - Boyes-Moore scanning routines
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright(c) E2 Systems 1993\n";
#include <stdio.h>
#include <stdlib.h>
#include "bmmatch.h"
/******************************************************************************
 * Routines to implement a Boyes-Moore scan of buffer data
 *
 * Routine to support scan where scan pattern can span multiple buffers. The
 * aim is to provide the same semantics as the usual bm_match* routines.
 *
 * The return is always relative to the base; beware that the returned value
 * may be LESS than base, indicating a match that started before the current
 * buffer. This could give a problem if the offset is actually negative ...
 *
 */
unsigned char * bm_match_frag(bfp, base, bound)
struct bm_frag * bfp;
unsigned char * base;
unsigned char * bound;
{
unsigned char * ret;
int borrow;

    if (bfp->tail_len > 0)
    {
/*
 * If there is a tail input, we need to see if there is a match starting within
 * it. Note that the tail_len should never be as great as match_len.
 */
        if (bfp->tail_len >= bfp->bp->match_len)
        {
            fprintf(stderr, "Logic Error: tail_len %d should be less than match_len %d; bad things would happen ...\n", bfp->tail_len, bfp->bp->match_len);
            return NULL;
        }
        if ((bound - base) < bfp->bp->match_len)
            borrow = bound - base;
        else
            borrow = bfp->bp->match_len - 1;
        memcpy(bfp->tail + bfp->tail_len, base, borrow);
        bfp->tail_len += borrow;
/*
 * Can't match if we don't have enough
 */
        if (bfp->tail_len < bfp->bp->match_len)
            return NULL;
/*
 * If we have a match in the overlap region, RETURN ITS POSITION RELATIVE TO
 * BASE.
 *
 * Note that the calling routine's life may become difficult if it next wants to
 * jump to an offset. If the offset is less than the match_len, we may be
 * stuffed. The caller must check that the computed address is within the
 * buffer being manipulated.
 *
 * It is certainly easier to accumulate the whole thing first ...
 */
        if ((ret = bfp->bm_routine(bfp->bp, bfp->tail,
                     bfp->tail + bfp->tail_len)) <=
                       bfp->tail + bfp->tail_len - bfp->bp->match_len)
        {
            ret = base - ((bfp->tail_len - borrow) - (ret - bfp->tail));
            bfp->tail_len = 0;
            return ret;
        }
/*
 * If the new base has not got beyond the tail, then shrink the
 * the tail. This can't happen unless there isn't enough in the main buffer for
 * a full match.
 */
        else
        if (borrow == (bound - base))
        {
            memmove(bfp->tail, ret, (((bfp->tail + bfp->tail_len) - ret)));
            bfp->tail_len = (((bfp->tail + bfp->tail_len) - ret));
            return NULL;
        }
/*
 * Otherwise, we must be in the new bit. Discard the tail and adjust the base.
 */
        else
            base += (borrow - ((bfp->tail + bfp->tail_len) - ret));
        bfp->tail_len = 0;
    }
/*
 * If we find it, return it
 */
    if ((ret = bfp->bm_routine(bfp->bp, base, bound)) <=(bound - bfp->bp->match_len))
        return ret;
/*
 * If we have not found it, save the tail and return NULL (not found)
 */
    bfp->tail_len = (bound - ret);
    if (bfp->tail_len > 0)
        memcpy( bfp->tail, ret, bfp->tail_len);
    return NULL;
}
/*
 * Set up a new bm_frag
 */
struct bm_frag * bm_frag_init(bp, bm_routine)
struct bm_table * bp;
unsigned char * (*bm_routine)();
{
struct bm_frag * bfp = (struct bm_frag *) malloc(sizeof(struct bm_frag));

    bfp->bp = bp;
    bfp->tail = (unsigned char *) malloc(2 * bp->match_len);
    bfp->tail_len = 0;
    bfp->bm_routine = bm_routine;
    return bfp;
}
/******************************************************************************
 * Create a Boyes-Moore control table
 */
struct bm_table * bm_compile_bin(wrd, len)
unsigned char * wrd;
int len;
{
struct bm_table * bp;
unsigned char * x;
int i;

    if ((bp = (struct bm_table *) malloc(sizeof(struct bm_table)))
                == (struct bm_table *) NULL)
        return (struct bm_table *) NULL;
/*
 * By default, skip forwards the length - 1
 */
    bp->match_len = len;
    bp->match_word = (unsigned char *) malloc(len);
    bp->altern = NULL;
    memcpy(bp->match_word, wrd, len);
    for (i = 0; i < 256; i++)
        bp->forw[i] = len;
/*
 * Adjust the skip length for the characters that are present. The skip length
 * takes you to the last character of the word if it is all there.
 */
    for (x = wrd, i = len; i > 0; x++, i--)
        bp->forw[*x] = len - ( x - wrd ) - 1;
    return bp;
}
void bm_zap(bp)
struct bm_table * bp;
{
    if (bp->altern != NULL)
        free(bp->altern);
    free(bp->match_word);
    free(bp);
    return;
}
void bm_frag_zap(bfp)
struct bm_frag * bfp;
{
    free(bfp->tail);
    free(bfp);
    return;
}
/******************************************************************************
 * Routines to implement a Boyes-Moore scan of buffer data
 *
 * Create a case-insensitive Boyes-Moore control table
 */
struct bm_table * bm_casecompile_bin(wrd, len)
unsigned char * wrd;
int len;
{
struct bm_table * bp;
unsigned char * x;
unsigned char * x1;
unsigned char * x2;
int i;

    if ((bp = (struct bm_table *) malloc(sizeof(struct bm_table)))
                == (struct bm_table *) NULL)
        return (struct bm_table *) NULL;
/*
 * By default, skip forwards the length - 1
 */
    bp->match_len = len;
    bp->match_word = (unsigned char *) malloc(len);
    bp->altern = (unsigned char *) malloc(len);
    for (i = 0; i < 256; i++)
        bp->forw[i] = len;
/*
 * Adjust the skip length for the characters that are present. The skip length
 * takes you to the last character of the word if it is all there.
 */
    for (x = wrd, x1 = bp->match_word, x2 = bp->altern, i = len;
             i > 0; x++, x1++, x2++, i--)
    {
        bp->forw[*x] = len - ( x - wrd ) - 1;
        if (islower(*x))
        {
            *x2 = toupper(*x);
            bp->forw[*x2] = len - ( x - wrd ) - 1;
            *x1 = *x;
        }
        else
        if (isupper(*x))
        {
            *x1 = tolower(*x);
            bp->forw[*x1] = len - ( x - wrd ) - 1;
            *x2 = *x;
        }
        else
        {
            *x1 = *x;
            *x2 = *x;
        }
    }
    if (!memcmp(bp->altern, bp->match_word, bp->match_len))
    {
        free(bp->altern);
        bp->altern = NULL;
    }
    return bp;
}
struct bm_table * bm_compile(wrd)
unsigned char * wrd;
{
    return bm_compile_bin(wrd, strlen(wrd));
}
struct bm_table * bm_casecompile(wrd)
unsigned char * wrd;
{
    return bm_casecompile_bin(wrd, strlen(wrd));
}
/*
 * For characters that are present;
 * - When first seen, skip back and start matching from the beginning
 * - When seen out of place in the match, skip ahead again
 * New return to support progressive scanning fragment by fragment.
 */ 
unsigned char * bm_match_new(bp, base, bound)
struct bm_table * bp;
unsigned char * base;
unsigned char * bound;
{
int i = bp->match_len - 1;
unsigned char * x = base + i;
unsigned char * x1;
unsigned char * x2;

    while (x < bound)
    {
        i = bp->forw[*x];
        if (i != 0)
            x += i;   /* Anywhere but a last character match */
        else
        {
            x -= (bp->match_len - 1);
            if ( x < base )
                x += bp->match_len;
            else
            {
                if (bp->altern == NULL)
                    for (x1 = bp->match_word, i = bp->match_len;
                        i > 0 && *x1 == *x;
                              x1++, x++, i--);
                else
                    for (x1 = bp->match_word, x2 = bp->altern, i = bp->match_len;
                        i > 0 && (*x1 == *x || *x2 == *x);
                              x1++, x2++, x++, i--);
                if (i == 0)
                    return (x - bp->match_len);
                x += i;
            }
        }
    }
    x -= ( bp->match_len - 1);
    if (x < base)
        x = base;
    return x;
}
/*
 * Original semantics
 */
unsigned char * bm_match(bp, base, bound)
struct bm_table * bp;
unsigned char * base;
unsigned char * bound;
{
unsigned char * ret;

    if (bp == NULL || bound < base + bp->match_len)
        return NULL;
    if ((ret = bm_match_new(bp, base, bound)) <=(bound - bp->match_len))
        return ret;          /* Found */
    else
        return NULL;         /* Not found */
}
#ifdef DEBUG_STAND
unsigned char * test_str[] = {
"ATSAAA4TA44TATSAMS   4NATATATAT",
"AMS   4NAT SAMS",
"AMS   4NAT SAMS   SAMS   4NAT",
"AMS   4NATATATS   SAMS   4NATSAMS   4NAT",
"AMS4N 4NATATATAT SAMS   4NATSAMS   4NAT",
"AMS   4NATATATATA SAMS   4NASAMS    4NAT",
"SAMS   4NAT",
"SAMS   4NAT SAMS",
NULL };
#define BITE 33
main()
{
struct bm_table * bp;
struct bm_frag * bfp;
unsigned char ** x;
unsigned char * y;
unsigned char * base;
unsigned char * bound;

/*    bp = bm_compile("SAMS   4NAT"); */
    bp = bm_compile("ATATAT");
    bfp = bm_frag_init(bp, bm_match_new);
    for (x = test_str; *x != (unsigned char *) NULL; x++)
    {
        puts(*x);
        y = bm_match_new(bp, *x, *x + strlen(*x));
        while (y <= (*x + strlen(*x) - bp->match_len))
        {
            printf("Match:%.*s\n", bp->match_len, y);
            y += bp->match_len;
        }
        if (y < *x + strlen(*x))
            puts("No Match");
    }
    for (base = test_str[0], bound = test_str[7] + strlen(test_str[7]);
            base < bound;
                base += BITE)
    {
    int xlen = bfp->tail_len;

        printf("Piece|%.*s|%.*s\n", bfp->tail_len, bfp->tail, BITE, base);
        y = bm_match_frag(bfp, base, base + BITE);
        if (y == bfp->tail && bfp->tail_len < bp->match_len)
            printf("No Match, now:%.*s\n", bfp->tail_len, bfp->tail);
        else
        {
            printf("Match:%.*s\n", bp->match_len, y);
            y += bp->match_len;
            if (bfp->tail_len > 0)
            {
/*
 * xlen is the amount of transfer. There may also be an amount of unscanned
 * buffer.
 */
                xlen = bfp->tail_len - xlen;
                bfp->tail_len = (bfp->tail + bfp->tail_len) - y;
                if (bfp->tail_len == 0 && xlen == 0)
                {
                    free(bfp->tail);
                    bfp->tail = NULL;
                }
                else
                {
                    if (bfp->tail_len > 0)
                        memmove(bfp->tail, y, bfp->tail_len);
                    if (BITE - xlen > 0)
                    {
                        bfp->tail = realloc(bfp->tail, bfp->tail_len +
                                           (BITE - xlen));
                        memcpy(bfp->tail + bfp->tail_len, base + xlen,
                                         (BITE - xlen));
                    }
                    bfp->tail_len += (BITE - xlen);
                }
                
            }
            else
            if (y < base + BITE)
            {
                bfp->tail_len = (base + BITE - y);
                bfp->tail = (unsigned char *) malloc( bfp->tail_len );
                memcpy(bfp->tail, y,  bfp->tail_len );
            }
        }
    }
    exit(0);
}
#endif
