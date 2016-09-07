/**************************************************************************
 * This code is derived from a differential file comparator I originally
 * wrote in REXX on VM/CMS on an IBM 4381.
 *************************************************************************
 *  USAGE: DIFFX    FIRST-FN FT FM  SECOND-FILE FT FM
 *  options (DIFF FN FT FM )   : FILE FOR DIFFERENCE OUTPUT
 * 
 *  AUTHOR : DME;
 *  DATE   : 13 JAN 1989
 *************************************************************************
 * I translated it into C, for use in the E2 ORACLE Schema Comparator, but
 * it retains much of its original flavour, in particular its use of simple
 * arrays for almost everything.
 *
 * Performance-wise, it compares rather poorly with the Linux (presumably GNU)
 * diff; it runs hundreds of times slower when comparing large files.
 *
 * But it is of course E2 Systems Intellectual Property.
 ************************************************************************
 * It takes two 'things', which are notionally lists of something, and returns
 * -  Counts of the list elements in each list
 * -  A count of the number of common elements between the lists
 * -  Which elements in each list correspond to each other
 * It can thus be used to compare a pair of PATH scripts, which both represent
 * captures of the same transaction, in order to:
 * -  Firstly, work out which messages correspond to which other messages
 * -  Then, to work out what changes from script incarnation to script
 *    incarnation
 * -  Finally, to identify changes to what is submitted from client to server,
 *    that are in essence reflections of data sent earlier from server to
 *    client.
 * -  To use it, you have to provide the following things
 *    -  Something akin to a pair of stdio channels, that will provide the
 *       sources for the elements
 *    -  The maximum size of an element in either list
 *    -  The name of a function that takes parameters like fgets() (that is,
 *       buffer, size of buffer, and channel) and that 'reads' an element
 *       into a buffer from a channel, and which returns NULL at logical EOF
 *    -  The name of a function that can return the length of an element
 *    -  The name of a function that can compare two elements, and which
 *       returns -1, 0 or +1, depending on whether the first element is less
 *       than, equal to or greater than the second. 
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1999";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "difflib.h"
/*
 * Functions in this file
 */
#define ABS(x) (((x)>0)?(x):-(x))
static void rankinit();
static void shuffle();
static int discard();
static void getworst();
#ifdef DEBUG
static int compute_disc();
static void dumpout();
#endif
static void execio_diskr();
#ifdef STAND
static void help();
/***********************************************************************
 * Getopt support
 */
extern int optind;           /* Current Argument counter.      */
extern char *optarg;         /* Current Argument pointer.      */
extern int opterr;           /* getopt() err print flag.       */
/***********************************************************/
int main(argc, argv)
int argc;
char ** argv;
{
struct diffx_con * dp;
FILE * ofp = stdout;
FILE * ifp1;
FILE * ifp2;
int ch;
enum output_option output_option = XEDIT;

    while ( ( ch = getopt ( argc, argv, "xdvo:" ) ) != EOF )
    {
        switch ( ch )
        {
        case 'x' :
            output_option = XEDIT;
            break;
        case 'd' :
            output_option = DIFFERENCES;
            break;
        case 'v' :
            output_option = FULL_LIST;
            break;
        case 'o' :
            if ((ofp = fopen(optarg, "wb")) == (FILE *) NULL)
            {
                perror("fopen() failed");
                fprintf(stderr, "Could not open %s; will use stdout\n",
                         optarg);
                ofp = stdout;
            }
            break;
        case 'h' :
        case '?' :
        default:
            help();           /* Does not return */
        }
    }
    if (argc - optind < 2)
    {
        help();
    }
/*
 * Check the files are not the same
 */
    if (!strcmp(argv[optind], argv[optind+1]))
    {
        puts( "Do not compare a file with itself");
        help();
    }
    if ((ifp1 = fopen(argv[optind], "rb")) == (FILE *) NULL)
    {
        perror("fopen() failed");
        fprintf(stderr, "Could not read %s, error %d\n", argv[optind], errno);
        exit(1);
    }
    if ((ifp2 = fopen(argv[optind + 1], "rb")) == (FILE *) NULL)
    {
        perror("fopen() failed");
        fprintf(stderr, "Could not read %s, error %d\n", argv[optind + 1],
                        errno);
        exit(1);
    }
    if ((dp = diffx_alloc((char *) ifp1, (char *) ifp2, fgets, 16384, strcmp,
              strlen)) == (struct diffx_con *) NULL)
         help();
                             /* Process the file arguments                 */
    fclose(ifp1);
    fclose(ifp2);
    diffrep(ofp, dp, output_option, fputs);
                             /* Report the differences, in the form of an
                              * xedit command file that converts file 1 into
                              * file 2                                     */
    diffx_cleanup(dp);
    exit(0);
}
/***********************************************************/
static void help()
{
    puts( "DIFFX compares two files, and outputs the differences");
    puts( "as a file that can be fed to XEDIT to change file 1 to file 2");
    puts( "Usage: DIFFX file1 file2");
    puts( "Options: -o Output filename");
    puts( "         -d (Differences)");
    puts( "         -v (Full File Output)");
    puts( "         -x (XEDIT Format Output)");
    exit(0);
}
#endif
static char * el_clone(thing, len)
char * thing;
int len;
{
char * buf;

    if ((buf = (char *) malloc(len + 1)) == NULL)
    {
        perror("malloc() failed");
        return NULL;
    }
    memcpy(buf, thing, len);
    buf[len] = '\0';
    return buf;
}
/*
 * Originally a Work-alike for the VM/CMS EXECIO DISKR facility. It read a
 * file into a dynamically allocated array. The zero'th element is a pointer
 * to an integer line count. The other elements were the file lines.
 *
 * Generalised by passing a 'channel' rather than a file name, and
 * parameterising the routines that handle elements from the channel.
 */
struct ln_con {
    char * ln;
    struct ln_con *nxt;
};
static void execio_diskr(fp, buf_len, el_read, el_len, ln)
char * fp;
int buf_len;
char * (el_read)();
int (el_len)();
char *** ln;
{
int n;
struct ln_con * anchor, * tail;
char * buf;
char **lp;

    if ((buf = (char *) malloc(buf_len)) == NULL)
    {
        perror("malloc() failed");
        *ln = (char **) NULL;
        return;
    }
    n = 0;
    anchor = (struct ln_con *) NULL;
    while ((el_read(buf, buf_len, fp)) != NULL)
    {
        n++;
        if (anchor == (struct ln_con *) NULL)
        {
            anchor = (struct ln_con *) malloc(sizeof(struct ln_con));
            tail = anchor;
        }
        else
        {
            tail->nxt = (struct ln_con *) malloc(sizeof(struct ln_con));
            tail = tail->nxt;
        }
        tail->nxt = (struct ln_con *) NULL;
        tail->ln = el_clone(buf, el_len(buf));
    }
    *ln = (char **) malloc(sizeof(char *)*(n+1));
    **ln = (char *) malloc( sizeof(long int));
    *((long int *) (**ln)) = n;
    for (tail = anchor, lp = *ln + 1;
             tail != (struct ln_con *) NULL;
                 lp++)
    {
        *lp = tail->ln;
        anchor = tail;
        tail = tail->nxt;
        free((char *) anchor);
    }
    free(buf);
    return;
}
/*****************************************************************************
 * Quick Sort routine for numbers or pointers
 *****************************************************************************
 * This stuff is here because I wanted to be rid of the Heapsort routine.
 * The change resulted in a significant speed-up.
 */
static __inline long int * find_any(low, high, match_word)
long int * low;
long int * high;
long int  match_word;
{
long int* guess;
long int i;

    while (low <= high)
    {
        guess = low + ((high - low) >> 1);
        if (*guess == match_word)
            return guess + 1; 
        else
        if (*guess < match_word)
            low = guess + 1;
        else
            high = guess - 1;
    }
    return low;
}
/*
 * Quick sort an array of pointers or long integers based on their values
 */
static __inline void qnums(a1, cnt)
long int *a1;                /* Array of numbers or pointers to be sorted */
int cnt;                     /* Number of numbers to be sorted            */
{
long int mid;
long int*top; 
long int*bot; 
long int*cur; 
long int*ptr_stack[128];
long int cnt_stack[128];
long int stack_level = 0;
long int i, j, l;

    ptr_stack[0] = a1;
    cnt_stack[0] = cnt;
    stack_level = 1;
/*
 * Now Quick Sort the stuff
 */
    while (stack_level > 0)
    {
        stack_level--;
        cnt = cnt_stack[stack_level];
        a1 = ptr_stack[stack_level];
        top = a1 + cnt - 1;
        if (cnt < 4)
        {
            for (bot = a1; bot < top; bot++)
                 for (cur = top ; cur > bot; cur--)
                 {
                     if (*bot > *cur)
                     {
                         *bot ^= *cur;
                         *cur ^= *bot;
                         *bot ^= *cur;
                     }
                 }
            continue;
        }
        bot = a1;
        cur = a1 + 1;
/*
 * Check for all in order
 */
        while (bot < top && *bot <= *cur)
        {
            bot++;
            cur++;
        }
        if (cur > top)
            continue;
                       /* They are all the same, or they are in order */
        mid = *cur;
        if (cur == top)
        {              /* One needs to be swapped, then we are done? */
            *cur = *bot;
            *bot = mid;
            cur = bot - 1;
            top = find_any(a1, cur, mid);
            if (top == bot)
                continue;
            while (cur >= top)
                *bot-- = *cur--;
            *bot = mid;
            continue;
        }
        if (cnt > 18)
        {
        long int samples[31];  /* Where we are going to put samples */

            samples[0] = mid; /* Ensure that we do not pick the highest value */
            l = bot - a1;
            if (cnt < (2 << 5))
                i = 3;
            else 
            if (cnt < (2 << 7))
                i = 5;
            else 
            if (cnt < (2 << 9))
                i = 7;
            else 
            if (cnt < (2 << 11))
                i = 11;
            else 
            if (cnt < (2 << 15))
                i = 19;
            else 
                i = 31;
            if (l >= i && a1[l >> 1] < a1[l])
            {
                mid = a1[l >> 1];
                bot = a1;
            }
            else
            {
                j = cnt/i;
                for (bot = &samples[i - 1]; bot > &samples[0]; top -= j)
                    *bot-- = *top;
                qnums(&samples[0], i);
                cur = &samples[(i >> 1)];
                top = &samples[i - 1];
                while (cur > &samples[0] && *cur == *top)
                    cur--;
                mid = *cur;
                top = a1 + cnt - 1;
                if (*(a1 + l) <= mid)
                    bot = a1 + l + 2;
                else
                    bot = a1;
            }
        }
        else
            bot = a1;
        while (bot < top)
        {
            while (bot < top && *bot < mid)
                bot++;
            if (bot >= top)
                break;
            while (bot < top && *top > mid)
                top--;
            if (bot >= top)
                break;
            *bot ^= *top;
            *top ^= *bot;
            *bot++ ^= *top--;
        }
        if (bot == top && *bot < mid)
            bot++;
        if (stack_level > 125)
        {
            qnums(a1, bot - a1);
            qnums(bot, cnt - (bot - a1));
        }
        else
        {
            ptr_stack[stack_level] = a1;
            cnt_stack[stack_level] = bot - a1;
            stack_level++;
            ptr_stack[stack_level] = bot;
            cnt_stack[stack_level] = cnt - (bot - a1);
            stack_level++;
        }
    }
    return;
}
#ifndef GENUINE_HEAPSORT
/*
 * Quick sort an array of pointers or long integers based on their values
 */
static __inline void qnums_slv(a1, a2, cnt)
long int *a1;                /* Array of numbers or pointers to be sorted */
long int *a2;                /* Second array to parallel the first        */
int cnt;                     /* Number of numbers to be sorted            */
{
long int mid;
long int slv; 
long int*top; 
long int*bot; 
long int*cur; 
long int*ptr_stack[128];
long int*slv_stack[128];
long int cnt_stack[128];
long int stack_level = 0;
long int i, j, l;

    ptr_stack[0] = a1;
    slv_stack[0] = a2;
    cnt_stack[0] = cnt;
    stack_level = 1;
/*
 * Now Quick Sort the stuff
 */
    while (stack_level > 0)
    {
        stack_level--;
        cnt = cnt_stack[stack_level];
        a1 = ptr_stack[stack_level];
        a2 = slv_stack[stack_level];
        top = a1 + cnt - 1;
        if (cnt < 4)
        {
            for (bot = a1; bot < top; bot++)
                 for (cur = top ; cur > bot; cur--)
                 {
                     if (*bot > *cur)
                     {
                         *bot ^= *cur;
                         *cur ^= *bot;
                         *bot ^= *cur;
                         a2[bot - a1] ^= a2[cur - a1];
                         a2[cur - a1] ^= a2[bot - a1];
                         a2[bot - a1] ^= a2[cur - a1];
                     }
                 }
            continue;
        }
        bot = a1;
        cur = a1 + 1;
/*
 * Check for all in order
 */
        while (bot < top && *bot <= *cur)
        {
            bot++;
            cur++;
        }
        if (cur > top)
            continue;
                       /* They are all the same, or they are in order */
        mid = *cur;
        slv = a2[cur - a1];
        if (cur == top)
        {              /* One needs to be swapped, then we are done? */
            *cur = *bot;    /* The last but one needs to be top */
            a2[cur - a1] = a2[bot - a1];
            *bot = mid;     /* Put the top one in the last-but-one */
            a2[bot - a1] = slv;
            cur = bot - 1;
            top = find_any(a1, cur, mid); /* See where it should go */
            if (top == bot)
                continue;                 /* It is right */
            while (cur >= top)
            {              /* Shuffle up from where it should go */
                a2[bot - a1] = a2[cur - a1];
                *bot-- = *cur--;
            }
            *bot = mid;   /* Now put it in the correct location */
            a2[bot - a1] = slv;
            continue;
        }
        if (cnt > 18)
        {
        long int samples[31];  /* Where we are going to put samples */

            samples[0] = mid; /* Ensure that we do not pick the highest value */
            l = bot - a1;
            if (cnt < (2 << 5))
                i = 3;
            else 
            if (cnt < (2 << 7))
                i = 5;
            else 
            if (cnt < (2 << 9))
                i = 7;
            else 
            if (cnt < (2 << 11))
                i = 11;
            else 
            if (cnt < (2 << 15))
                i = 19;
            else 
                i = 31;
            if (l >= i && a1[l >> 1] < a1[l])
            {
                mid = a1[l >> 1];
                bot = a1;
            }
            else
            {
                j = cnt/i;
                for (bot = &samples[i - 1]; bot > &samples[0]; top -= j)
                    *bot-- = *top;
                qnums(&samples[0], i);
                cur = &samples[(i >> 1)];
                top = &samples[i - 1];
                while (cur > &samples[0] && *cur == *top)
                    cur--;
                mid = *cur;
                top = a1 + cnt - 1;
                if (*(a1 + l) <= mid)
                    bot = a1 + l + 2;
                else
                    bot = a1;
            }
        }
        else
            bot = a1;
        while (bot < top)
        {
            while (bot < top && *bot < mid)
                bot++;
            if (bot >= top)
                break;
            while (bot < top && *top > mid)
                top--;
            if (bot >= top)
                break;
            *bot ^= *top;
            *top ^= *bot;
            a2[bot - a1] ^= a2[top - a1];
            a2[top - a1] ^= a2[bot - a1];
            a2[bot - a1] ^= a2[top - a1];
            *bot++ ^= *top--;
        }
        if (bot == top && *bot < mid)
            bot++;
        if (stack_level > 125)
        {
            qnums_slv(a1, a2, bot - a1);
            qnums_slv(bot, &a2[bot - a1], cnt - (bot - a1));
        }
        else
        {
            ptr_stack[stack_level] = a1;
            slv_stack[stack_level] = a2;
            cnt_stack[stack_level] = bot - a1;
            stack_level++;
            ptr_stack[stack_level] = bot;
            slv_stack[stack_level] = &a2[bot - a1];
            cnt_stack[stack_level] = cnt - (bot - a1);
            stack_level++;
        }
    }
    return;
}
#endif
/*
 * Release resources allocated with diffx_alloc()
 */
void diffx_cleanup(dp)
struct diffx_con * dp;
{
int i, j;
long int l1_cnt;
long int l2_cnt;
long int tot;
unsigned char ** ul;
unsigned char ** ul1;
unsigned char ** ul2;

    free((char *) (dp->match1));
    free((char *) (dp->match2));
    free((char *) (dp->forwptr));
    free((char *) (dp->backptr));
    free((char *) (dp->rank));
/*
 * Currently this will multiple-free() any matching elements in either list
 */
    l1_cnt = dp->l1cnt;
    l2_cnt = dp->l2cnt;
    tot = l1_cnt + l2_cnt;
    ul = (unsigned char **) malloc( tot  * sizeof(unsigned char *));
    for (i = 1, ul1 = ul; i <= l1_cnt; i++, ul1++)
        *ul1 = dp->ln1[i];
    for (i = 1; i <= l2_cnt; i++, ul1++)
        *ul1 = dp->ln2[i];
    qnums(ul, tot);
    for (ul1 = ul, i = 1; ul1 < ul + tot; ul1 = ul2, i++)
    {
        free(*ul1);
        ul2 = ul1 + 1;
        while (ul2 < ul + tot && *ul1 == *ul2)
            ul2++;
    }
    free((unsigned char *) ul);
    free((char *) dp->ln1[0]);
    free((char *) dp->ln1);
    free((char *) dp->ln2[0]);
    free((char *) (dp->ln2));
    free((char *) dp);
    return;
}
/****************************************************************
 * Sort two arrays into ascending order of the first array.
 *
 * This is Heapsort, after Kneuth.
 *
 * Note that the indexes really run from 1 rather than 0
 ****************************************************************
 * Getting rid of Heapsort code is the last significant improvement
 * that I can think of. However, for the first attempt it made
 * scarcely any difference.
 ****************************************************************
 */
static void heapsort(rrank, rord, nextr)
long int * rrank;
long int * rord;
int nextr;
{
int rightedge, nextsorted, leftedge, currecv, curreco, parent, child, i;

    rord[0] = 0;                                   /* Sentinel */
#ifndef GENUINE_HEAPSORT
    qnums_slv(&rrank[1], &rord[1], nextr - 1);
#else
    rightedge = nextr - 1;
    nextsorted = rightedge;
    leftedge = 2 + ((nextr-1)/2);
    for (;;)
    {
        if ( leftedge > 1 )
        {
            leftedge--;
            currecv = rrank[leftedge];
            curreco = rord[leftedge];
        }
        else
        {
            curreco = rord[rightedge];
            currecv = rrank[rightedge];
            rord[nextsorted] = rord[1];
            rrank[nextsorted] = rrank[1];
            nextsorted--;
            if ( rightedge <= 1 )
                break;
            rightedge--;
        }
        child = leftedge;
        for (;;)       /* The establish heap/siftup loop */
        {
            parent = child;
            child = child + child;
            if ( child > rightedge )
                break;
/*
 * Pick between child and sibling
 */
            if ( child < rightedge )
            {
                i = child + 1;
                if ( rrank[child] < rrank[i] )
                    child=i;
            }
            if ( currecv < rrank[child] )    /* now child and parent */
            {
                rord[parent] = rord[child];
                rrank[parent] = rrank[child];
            }
            else
                break;
        }
        rord[parent] = curreco;
        rrank[parent] = currecv;
    }
#endif
    return;
}
/***********************************************************************;
 * Before calling this, we have all the possible matches, sorted in
 * ascending order of file 1 line number. Our goal is to find
 * the maximum ordered set of matches. This routine initialises for
 * that search.
 *
 * Sort the rankings.
 */
static void rankinit(dp, rord)
struct diffx_con * dp;
long int * rord;
{
int nextr = dp->nextr;
long int * rrank = dp->rank;
long int * rforwptr = dp->forwptr;
int i, j;
int worsti, worstv;
/*
 * Sort rank, ord into ascending order of line 2 match.
 * - ord[] provides a pair index.
 * - rank[] records the line that matches in file 2; the sort key.
 */
    heapsort(rrank, rord, nextr);
#ifdef DEBUG_FULL
    for (i = 1; i < nextr; i++)
        printf("rank: %d ord: %d\n", rrank[i], rord[i]);
    dumpout("After sort", dp, 0, nextr, 0, 0);
#endif
    for (i = 1; i < nextr; i++)
        rforwptr[rrank[i]] = rord[i];
                              /* The Matching Pair index for a file 2 line */
/*
 * Replace the second file line numbers in rank with their relative
 * positions, and re-write in the order in which the first file line
 * numbers appear in the first file.
 */
    for (i = 1; i < nextr; i++)
    {
        j = rord[i];     /* Matching Pair Index */
        rrank[j] = i;    /* Matching pair index, but in file 2 line order */ 
    }
#ifdef DEBUG
    dumpout("After re-order", dp, 0, nextr, 0, 0);
    printf("Discrepancies: %d\n", compute_disc(dp, nextr));
#endif
    free((char *) rord);
/*
 * At this point:
 * - rank[] is a list of matches
 * - i - rank[i] is a measure of the disorder of the matches
 * - backptr[] lists the line 1 lines that match
 * - match2[] then lists the corresponding line 2 match.
 */ 
    shuffle(dp);             /* Find the maximal ordered common set */
    return;
}
/************************************************************************
 * Calculate what is happening to the disorder measure as the position
 * of a match is moved.
 *
 * The change must depend on where the stepped-over matches are in relation
 * to the pair we are getting rid of.
 *
 * If we get rid of a cross-over, we reduce the dis-order by 2.
 *
 * If we create a cross-over, we increase the disorder by 2.
 *
 * If we eliminate a line, we save twice its cost.
 *
 * The function returns  -1, 0 or +1 dependent on whether:
 * - They cross, and the first has a lower file 1 line number
 * - They do not cross
 * - They cross, and the second has a lower file 1 line number
 *
 * This will never be called with matches between the lines.
 */ 
#ifdef CROSSINC_FUN
static int crossinc(f11,f12,f21,f22)
int f11;
int f12;
int f21;
int f22;
{
int j;
    if (f11 > f21 && f12 < f22)
        j = 1;
    else
    if (f11 < f21 && f12 > f22)
        j = -1;
    else
        j = 0;
#ifdef DEBUG_FULL
    printf("crossinc(%d,%d,%d,%d) = %d\n", f11,f12,f21,f22,j);
#endif
    return j;
}
#else
#define crossinc(f11,f12,f21,f22) ((((f11)>(f21))&&((f12)<(f22)))?1:((((f11)<(f21))&&((f12)>(f22)))?-1:0))
#endif
/************************************************************************
 * Throw away and re-arrange until the list is in the right order.
 * All rank elements will equal their array sub-script, and worsti will be
 * zero as well.
 *
 * The backptr array will hold the line numbers of the maximum set of
 * of matching lines in the first file that whose matches in the second
 * file occur in the same sequence in the second file as these lines do
 * in the first file.
 *
 * The loop has two flavours, doing-worst and ripple. It is doing-worst
 * when it is processing a matching pair of lines; it is rippling when
 * it is processing a line displaced from its match by a line that
 * appears to give a match that is closer to home. Doing worst is
 * signalled with ripflag == 0; ripple is signalled with ripflag == 1.
 */
static void shuffle(dp)
struct diffx_con * dp;
{
int ripflag=0;
int j, poschange, posinc, savrank, sav1, sav2, i, m, k, l;
long int rln1cnt = dp->l1cnt;
long int rln2cnt = dp->l2cnt;
char ** rln1 = dp->ln1;
char ** rln2 = dp->ln2;
long int * rmatch1 = dp->match1;
long int * rmatch2 = dp->match2;
long int * rrank = dp->rank;
long int * rbackptr = dp->backptr;
long int * rforwptr = dp->forwptr;
long int nextr = dp->nextr;
int worsti;
int worstv;

/*
 * Find the line that is furthest from home
 */
    getworst(dp, nextr, &worsti, &worstv);
    while (worsti != 0)
    {
/*
 * This loop hunts through the entries in rank until all its entries are
 * equal in value to their array subscripts. When this happens, the
 * backptr[] array points to the maximum subset of lines in file1 that have
 * corresponding entries in the same order in file 2.
 *
 * The processing proceeds one entry at a time. The entry that is apparently
 * furthest from where it ought to be is considered. It consists of a pair of
 * lines. Because of the way the initial list of pairs was set up, and the way
 * that this algorithm works, one or other of these lines does not correspond
 * to a line in the other file; possibly both. If worsti is negative, the
 * line in the first file must be discarded; if worsti is positive, the
 * line in the second file must be discarded. However, the other line may
 * correspond to a duplicate further on in the file whose value is being
 * dropped, so a search takes place.
 *
 * If no match is found (the easy case), this line is discarded.
 *
 * If a match is found that is not currently associated with a line in the
 * other file, and the i - rank[i] value will be less than worstv, then this
 * line effectively replaces the line that was dropped.
 *
 * If a match is found, and the line is already associated with another line,
 * and replacing the associated line gives an improvement in the ord[] value,
 * this line is replaced. However, the displaced line may itself have a
 * further corresponding line; the search must continue.
 *
 * The outer loop is once per scan through either file, rather than once per
 * initial candidate.
 *
 * For negative ones, we want to search the first file for a candidate to
 * supersede.
 *
 * For positive ones, we want to search the second file for a candidate to
 * supersede.
 *
 * Begin by looking for a duplicate occurrence in the appropriate file.
 */
        j = rbackptr[worsti]; /* Position in file 1 */
        poschange = 0;
        if ( worstv < 0 )
        {
            sav1 = j;
            if ( ripflag == 0 )
            {
                savrank = rrank[worsti]; /* This rank is invariant, because
                                             * we are not moving the line in
                                             * file 2 at this point
                                             */
                sav2 = rmatch2[j];       /* Position in file 2 */
                rmatch2[j] = 0;          /* The file 1 line does not match */
            }
            posinc = 0;
            if ( j  <  rln1cnt )
            {
                for (i = j + 1; i <= rln1cnt && rln1[i] != rln1[j]; i++)
                {
                    if ( rmatch2[i] != 0 )
                    {
                        posinc++;
                        if (!ripflag)
                        {
                            poschange += (crossinc(sav1, sav2, i, rmatch2[i])
                                       - crossinc(i+1, sav2, i, rmatch2[i]));
/*
 * The following test seems to help
 */
                            if ((ABS((worsti - rrank[worsti])) <
                              ABS( worsti - rrank[worsti] - poschange)))
                                break;
                        }
                    }
                }
            }
            else
                i = j;
            if (i <= rln1cnt
               && (rln1[i] == rln1[j])
               && (ripflag
                || (rmatch2[i] == 0 && ABS((worsti - rrank[worsti])) >=
                                ABS(worsti - rrank[worsti] - poschange))
                || (rmatch2[i] != 0
                  && ABS((worsti - rrank[worsti]) - poschange) <=
                    ABS((worsti + posinc + 1 - rrank[worsti + posinc + 1])))))
            {
/*
 * There is a match, and we gain by swapping the value in;
 */
#ifdef DEBUG
          printf("ripflag %d worsti %d Swapping file 1 %d for %d file 2 %d poschange %d\n",
                       ripflag, worsti, sav1, i, sav2, poschange);
#endif
/*
 * - Shift the intervening elements down
 */
                for (k = worsti + posinc, m = worsti + 1;
                         m <= k;
                              m++)
                {
                    l = m - 1;
                    rbackptr[l] = rbackptr[m];
                    rforwptr[rmatch2[rbackptr[l]]] = l;
                    rrank[l] = rrank[m];
                }
                rbackptr[k] = i;      /* Where it now points */
                rforwptr[sav2] = k;
                rrank[k] = savrank;
#ifdef DEBUG
                printf("rank[%d] %d sav2 %d back[mat2] %d back[mat2[forw] %d\n",
                          k, rrank[k], sav2,
                          rbackptr[sav2],
                          rforwptr[rbackptr[sav2]]);
                printf("Post-pos adjust: %d\n", compute_disc(dp, nextr));
#endif
                if (rmatch2[i] != 0 )
                {
                    ripflag = 1;
                    k = rmatch2[i];
                    worsti = rforwptr[k];
                                       /* Advance to the pair's rank[] entry */
                                       /* This should be m */
#ifdef DEBUG
                    if (worsti != m)
                        printf("worsti; expected %d found %d\n", m, worsti);
#endif
                    savrank = rrank[worsti];
                    rmatch2[i] =sav2;
                    rmatch1[sav2] = i;
                    sav2 = k;
                }
                else
                {
                    if (i == j)
                        break;
                    ripflag=0;
                    rmatch1[sav2] = i;
                    rmatch2[i] = sav2;
                    getworst( dp, nextr, &worsti, &worstv);
                }
            }
            else         /* There is no gain from swapping in; */
            {
                rmatch1[sav2] = 0;
                rforwptr[sav2] = 0;
                ripflag = 0;
                nextr = discard(dp, nextr, worsti, &worsti, &worstv);
            }
        }
        else
        {
/*
 * Search the second file for an alternative
 */
            j = rmatch2[j];                 /* Position in file 2 */
            sav2 = j;
            if ( ripflag == 0 )
            {
                sav1 = rmatch1[j];          /* sav1 is position in file 1 */
                rmatch1[j] = 0;             /* File 2 line matches nothing */
                rforwptr[j] = 0;
            }
            if (j < rln2cnt)
            {
                for (i = j + 1; i <= rln2cnt && (rln2[i] != rln2[j]); i++)
                {
                    if ( rmatch1[i] != 0 )
                    {
                        poschange += (crossinc(sav1, sav2, rmatch1[i], i)
                                 - crossinc(sav1, i+1, rmatch1[i], i));
                        if (!ripflag && (ABS((worsti - rrank[worsti])) <
                               ABS(worsti - rrank[worsti] - poschange)))
                            break;
                    }
                }
            }
            else
                i = j;
#ifdef DEBUG
            printf("Pre-pos adjust: %d\n", compute_disc(dp, nextr));
#endif
            if (i <= rln2cnt
               && (rln2[i] == rln2[j])
               && (ripflag
                || (rmatch1[i] == 0 && ABS((worsti - rrank[worsti])) >=
                         ABS(worsti - rrank[worsti] - poschange))
                || (rmatch1[i] != 0
                     && ABS((worsti - rrank[worsti]) - poschange )
                       <= ABS((rforwptr[i] - rrank[rforwptr[i]]))) ))
            {                             /* We gain by swapping the value in */
/*
 * There are rank elements that need updating
 */
#ifdef DEBUG
          printf("ripflag %d worsti %d file 1 %d Swapping file 2 %d for %d poschange %d\n",
                       ripflag, worsti, sav1, sav2, i, poschange);
#endif
                poschange = 0;
                posinc = 0;
                for ( l = j + 1; l < i; l++)
                {
                    if (rmatch1[l] != 0)
                    {
                        k = crossinc(sav1, sav2, rmatch1[l], l)
                                 - crossinc(sav1, i, rmatch1[l], l);
                        if (k)
                        {
                            posinc += k;
                            rrank[rforwptr[l]] -= k;
#ifdef DEBUG
                  printf("line:%d run: %d Downdating file 2: rank[%d] = %d\n", 
                                 l, posinc, rforwptr[l], 
                            rrank[rforwptr[l]]);
#endif
                        }
                    }
                }
/*
 * Update the crossing count
 */
                rrank[worsti] = rrank[worsti] + posinc;
#ifdef DEBUG
                  printf("worsti rank[%d] = %d\n", 
                             worsti, rrank[worsti]);
#endif
                if ( rmatch1[i] != 0 )
                {                         /* It matched already */
                    ripflag = 1;
                    l = rforwptr[i];
                    rforwptr[i] = worsti;
                    worsti = l;
                    l = rmatch1[i];
                    rmatch1[i] = sav1;
                    rmatch2[sav1] = i;
                    sav1 = l;
                }
                else
                {
                    if (i == j)
                        break;
                    ripflag = 0;
                    rmatch2[sav1] = i;
                    rmatch1[i] = sav1;
                    rforwptr[i] = worsti;
                    rbackptr[worsti] = sav1;
                    getworst(dp, nextr, &worsti, &worstv);
                }
            }
            else
            {         /* There is no gain from swapping in. */
                ripflag = 0;
                rmatch2[sav1] = 0;
                nextr = discard(dp, nextr, worsti, &worsti, &worstv);
            }
        }
#ifdef DEBUG
        dumpout("After shuffle iteration", dp, ripflag, nextr, worsti, worstv);
#endif
    }
    dp->nextr = nextr;
    return;
}
/*
 * Structure used for sorting to find the possible matching lines.
 */
struct sort_el {
    unsigned char * in_el;
    int lisn;
    int linn;
};
/*
 * Not thread-safe; gl_el_cmp would have to be protected
 */
static int (*gl_el_cmp)();
/*
 * Comparison for the lists of elements
 */
int cmp_sort_el(s1, s2)
unsigned char * s1;
unsigned char * s2;
{
struct sort_el * se1 = (struct sort_el *) s1;
struct sort_el * se2 = (struct sort_el *) s2;
int ret;

    if (!(ret = gl_el_cmp(se1->in_el,se2->in_el)))
        if (!(ret = (se1->lisn - se2->lisn)))
            ret = (se1->linn - se2->linn);
    return ret;
}
/***************************************************************************
 * Allocate the differential comparator control structure, read in the
 * elements from the channels provided, and then identify the maximal set
 * of common lines.
 */
struct diffx_con * diffx_alloc(fp1, fp2, el_read, max_len, el_cmp, el_len)
char * fp1;
char * fp2;
void (*el_read)();
int max_len;
int (*el_cmp)();
int (*el_len)();
{
long int i;
long int l1_cnt;
long int l2_cnt;
long int tot;
long int nextr;
long int * rmatch1;
long int * rmatch2;
long int * rrank;
long int * rbackptr;
long int * rord;
struct diffx_con * dp;
struct sort_el * se, * xse, *xsen, *xse1, *xse2;

    dp = (struct diffx_con *) calloc(sizeof(struct diffx_con), 1);
    execio_diskr(fp1, max_len, el_read, el_len, &(dp->ln1));
    execio_diskr(fp2, max_len, el_read, el_len, &(dp->ln2));
    if (dp->ln1 == (char **) NULL || dp->ln2 == (char **) NULL)
        return (struct diffx_con *) NULL;
    l1_cnt = (*((long int *) (dp->ln1[0])));
    l2_cnt = (*((long int *) (dp->ln2[0])));
    tot = l1_cnt + l2_cnt;
/*
 * Now allocate the integer arrays, based on the line counts
 */
    dp->arrdim = (l1_cnt > l2_cnt )? l1_cnt : l2_cnt;
    i = (dp->arrdim + 2)* sizeof(long int);
    rmatch1 = (long int *) calloc(i, 1);
    rmatch2 = (long int *) calloc(i, 1);
    rbackptr = (long int *) calloc(i, 1);
    dp->forwptr = (long int *) calloc(i, 1);
    rrank = (long int *) calloc(i, 1);
    rord = (long int *) calloc(i, 1);
/*
 * Now produce a compressed, pre-compared representation of
 * the two inputs. First phase, replace things that are the same with pointers
 * to same. We give ourselves a problem freeing the stuff up, since we
 * need to take care to avoid multiple free()'s.
 */
    se = (struct sort_el *) malloc( tot  * sizeof(struct sort_el));
    for (i = 1, xse = se; i <= l1_cnt; i++, xse++)
    {
        xse->in_el = dp->ln1[i];
        xse->lisn = 0;
        xse->linn = i;
    }
    for (i = 1; i <= l2_cnt; i++, xse++)
    {
        xse->in_el = dp->ln2[i];
        xse->lisn = 1;
        xse->linn = i;
    }
    gl_el_cmp = el_cmp;
    qsort(se, tot, sizeof(struct sort_el), cmp_sort_el);
    for (xse = se, nextr = 1; xse < se + tot; xse = xsen)
    {
        xsen = xse + 1;
        xse2 = NULL;
        while (xsen < se + tot && !el_cmp(xse->in_el, xsen->in_el))
        {
            free(xsen->in_el);
            if (xsen->lisn == 0)
                dp->ln1[xsen->linn] = xse->in_el;
            else
            {
                dp->ln2[xsen->linn] = xse->in_el;
                if (xse2 == NULL)
                    xse2 = xsen;
            }
            xsen++;
        }
/*
 * Match up lines if possible.
 */
        if (xse->lisn == 0 && xse2 != NULL)
        {
            for (xse1 = xse, xse = xse2; xse1 < xse2 && xse < xsen; xse1++, xse++)
            {
                rmatch2[xse1->linn] = xse->linn; /* Line in 2 corresponding to 1 */
                rbackptr[nextr] = xse1->linn;    /* Match count -> file position */
                rmatch1[xse->linn] = xse1->linn; /* Line in 1 corresponding to 2 */
                rrank[nextr] = xse->linn;        /* Set up for sort in rankinit() */
                rord[nextr] = nextr;             /* Set up for sort in rankinit() */
                nextr++;
            }
        }
    }
/*
 * At this point:
 * - nextr is a count of the pairs seen (+1)
 * - ord[] provides a pair index.
 * - backptr[] records the line that matches in file 1
 * - rank[] records the line that matches in file 2
 * - match1[] records the file 1 line that matches the corresponding line in 2
 * - match2[] records the file 2 line that matches the corresponding line in 1
 * rrank and backptr need to be in file 1 line order.
 */
    heapsort(rbackptr, rrank, nextr); 
    free((unsigned char *) se);
    dp->nextr = nextr;
    dp->l1cnt = l1_cnt;
    dp->l2cnt = l2_cnt;
    dp->match1 = rmatch1;
    dp->match2 = rmatch2;
    dp->backptr = rbackptr;
    dp->rank = rrank;
    rankinit(dp, rord);     /* Find the maximal ordered list of matches  */
    return dp;
}
/*****************************************************************
 * Discard gets rid of an unwanted match;
 * - ind is the one to drop
 */
static int discard(dp, nextr, ind, worsti, worstv)
struct diffx_con * dp;
int nextr;
int ind;
int * worsti;
int * worstv;
{
int spltrank;
int j, k, l;
int rworsti = 0;
int rworstv = 0;
long int * rrank = dp->rank;
long int * rbackptr = dp->backptr;
long int * rmatch2 = dp->match2;
long int * rforwptr = dp->forwptr;

    spltrank = rrank[ind];
#ifdef DEBUG
    printf("Discarding(%d of %d): spltrank %d disc %d (File 1 %d,File 2 %d)\n",
             ind, nextr, spltrank, compute_disc(dp, nextr), rbackptr[ind], rmatch2[rbackptr[ind]]);
#ifdef DEBUG_FULL
        for (k = 1; k < nextr; k++ )
        {
            if (k != rrank[k])
                printf("l: %d rank: %d\n", k, rrank[k]);
        }
#endif
#endif
    for (k = 1; k < ind; k++)
    {
        if ( rrank[k] > spltrank )
            rrank[k]--;
        j = k - rrank[k];
        if ( ABS((j)) > ABS(rworstv) )
        {
            rworsti = k;
            rworstv = j;
        }
    }
    for (k = ind+1; k < nextr; k++ )
    {
        l = k - 1;
        if (  rrank[k] > spltrank )
             rrank[k]--;
        rrank[l] =  rrank[k];
        rbackptr[l] =  rbackptr[k];
        rforwptr[rmatch2[rbackptr[l]]] = l;
        j = l - rrank[l];
        if ( ABS(j) > ABS(rworstv) )
        {
            rworsti = l;
            rworstv = j;
        }
    }
    nextr--;
#ifdef DEBUG
    if (k = compute_disc(dp, nextr))
    {
        printf("Discrepancy after discard: %d\n", k);
#ifdef DEBUG_FULL
        for (k = 1; k < nextr; k++ )
        {
            if (k != dp->rank[k])
                printf("l: %d rank: %d\n", k, dp->rank[k]);
        }
#endif
        exit(0);
    }
    else
        printf("worsti: %d worstv: %d (File 1, %d, File 2, %d) %s\n", rworsti, rworstv,
          rbackptr[rworsti], rmatch2[rbackptr[rworsti]], dp->ln1[rbackptr[rworsti]]);
#endif
    *worsti = rworsti;
    *worstv = rworstv;
    return nextr;
}
/***********************************************************************
 * GETWORST: find the line furthest from home, and compute the distances
 * from home for each item.
 */
static void getworst(dp, nextr, worsti, worstv)
struct diffx_con * dp;
int nextr;
int * worsti;
int * worstv;
{
int i;
int j;
int rworsti = 0;
int rworstv = 0;
long int * rrank = dp->rank;

    for (i = 1; i < nextr ; i++)
    {
        j = i - rrank[i];
        if (ABS(rworstv) < ABS((j)))
        {
            rworsti = i;
            rworstv = j;
        }
    }
#ifdef DEBUG
    printf("getworst(%d,%d,%d): %d (File 1 %d File 2 %d) %s\n", nextr, rworsti, rworstv,
            compute_disc(dp, nextr), dp->backptr[rworsti],dp->match2[dp->backptr[rworsti]],
            dp->ln1[dp->backptr[rworsti]]);
#endif
    *worsti = rworsti;
    *worstv = rworstv;
    return;
}
#ifdef DEBUG
/*
 * Check the integrity of the control structures
 */
static int compute_disc(dp, nextr)
struct diffx_con * dp;
int nextr;
{
int i, j;

    for (i=1, j =0; i < dp->arrdim; i++)
    {
        if (i < nextr)
        {
            j += (i - dp->rank[i]);
            if (i + 1 != nextr && dp->rank[i] == dp->rank[i+1])
                printf("rank %d and %d = %d\n", i, i+1, dp->rank[i+1]);
/*
 * Check that the forwptr and backptr entries are consistent
 */
            if (dp->forwptr[dp->match2[dp->backptr[i]]] != i)
                printf("back-forw mismatch %d back %d match2 %d forw %d\n", i,
                       dp->backptr[i],
                       dp->match2[dp->backptr[i]],
                       dp->forwptr[dp->match2[dp->backptr[i]]]);
        }
/*
 * Check that the match1 and match2 entries are consistent
 */
        if (dp->match1[i] != 0
         && (dp->match2[dp->match1[i]] != i))
            printf("match1-2 mismatch %d match1 %d match2 %d\n", i,
                dp->match1[i], dp->match2[dp->match1[i]]);
        if (dp->match2[i] != 0
         && (dp->match1[dp->match2[i]] != i))
            printf("match2-1 mismatch %d match2 %d match1 %d\n", i,
                dp->match2[i], dp->match1[dp->match2[i]]);
   
    }
    return j;
}
/*********************************************************************
 * Useful diagnostic printout
 */
static void dumpout(prompt, dp, ripflag, nextr, worsti, worstv)
char * prompt;
struct diffx_con * dp;
int ripflag;
int nextr;
int worsti;
int worstv;
{
int i, j;

    printf( "%s ripflag %d nextr %d worsti %d worstv %d\n", prompt,
                ripflag, nextr, worsti, worstv);
    for (i=1, j =0; i <= dp->arrdim; i++)
    {
        printf("Line: %d VVVVVVVVVVVVVVV\n", i);
        if (i <= (*((int *) (dp->ln1[0]))))
            printf("1=>%s<= mat2 %d", (dp->ln1[i]), dp->match2[i]); 
        if (i <= (*((int *) (dp->ln2[0]))))
            printf("=>2=>%s<= mat1 %d forwptr %d =>", (dp->ln2[i]), dp->match1[i], dp->forwptr[i]); 
        if ( i < nextr )
        {
            printf( " rank %d back1 %d", dp->rank[i], dp->backptr[i]);
            j += (i - dp->rank[i]);
        }
        putchar('\n');
    }
    if (j != 0)
        printf( "Logic error: Sigma (i - rank[i]) is %d, should be 0\n", j);
    fflush(stdout);
    return;
}
#endif
/******************************************************************************
 * Report on the findings; output a report indicating which elements are in
 * file 1 and which are in file 2.
 */
void diffrep(ofp, dp, output_option, out_fun)
FILE * ofp;
struct diffx_con * dp;
enum output_option output_option;
int (*out_fun)();
{
int k;
int upto1, upto2, dcnt, iflag;
long int i = dp->l1cnt + 1;            /* Establish sentinel */
long int j = dp->l2cnt + 1;            /* Establish sentinel */

    dp->match2[i] = j;
    dp->match1[j] = i;
    upto1 = 1;
    upto2 = 1;
    dcnt = 0;
    iflag = 1;                /* Switch to cope with xedit i/d funny */
    for (;;)
    {
        while ( i > upto1 && j > upto2 && dp->match2[upto1] == upto2)
        {
#ifdef DEBUG
            if (dp->match1[upto2] != upto1)
                fprintf(stderr,
"Logic Error: File 1 line %d matche File 2 line %d but File 2 line %d matches File 1 line %d\n",
                   upto1, upto2, upto2, dp->match1[upto2]);
#endif
            if (output_option == FULL_LIST )
            {
                fputc(' ', ofp);
                fputc(' ', ofp);
                out_fun(dp->ln2[upto2], ofp);
            }
            else
                dcnt++;
            upto1++;    /* Advance over matching pairs of lines */
            upto2++;
        }
        if ( upto1 >= i && upto2 >= j )
            break;
        if (dp->match1[upto2] == 0)
        {     /* Insertions from the second file */
            if ( iflag == 0 )
                dcnt--;     /* Adjust on swap from D to I */
            iflag = 1;
            if ( dcnt > 0 )
            {
                if ( output_option != FULL_LIST )
                    fprintf(ofp, "down %d\n", dcnt);
                dcnt = 0;
            }
            while (dp->match1[upto2] == 0)
            {
                fputc('i', ofp);
                fputc(' ', ofp);
                out_fun(dp->ln2[upto2], ofp);
                upto2++;
            }
        }
        if (dp->match2[upto1] == 0)
        {           /* Deletions from the first file */
            k = 0;
            if ( iflag == 1 )
                dcnt++;               /* Adjust for swap from I to D */
            iflag = 0;
            if ( dcnt > 0 )
            {
                if ( output_option != FULL_LIST )
                    fprintf(ofp, "down %d\n", dcnt);
                dcnt = 0;
            }
            while (dp->match2[upto1] == 0)
            {
                k++;
                if (output_option != XEDIT)
                {
                    fputc('d', ofp);
                    fputc(' ', ofp);
                    out_fun(dp->ln1[upto1], ofp);
                }
                upto1++;
            }
            if (output_option == XEDIT)
                fprintf(ofp, "del %d\n", k);
        }
    }
    if (output_option != FULL_LIST)
        fputs("ff\n", ofp);
    return;                                          /* to main */
}
