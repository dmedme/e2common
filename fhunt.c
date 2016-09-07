/************************************************************************
 * fhunt.c - Search for a string in a list of files quickly.
 ***********************************************************************
 *This file is a Trade Secret of E2 Systems. It may not be executed,
 *copied or distributed, except under the terms of an E2 Systems
 *UNIX Instrumentation for CFACS licence. Presence of this file on your
 *system cannot be construed as evidence that you have such a licence.
 ***********************************************************************/
static unsigned char * sccs_id =
    (unsigned char *) "Copyright (c) E2 Systems Limited 1994\n\
@(#) $Name$ $Id$";
#ifdef LINUX
#define O_BINARY 0
#define setmode(x,y)
#include <sys/types.h>
#include <unistd.h>
#define tell(fd) lseek64(fd, (off64_t)0, SEEK_CUR)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include "bmmatch.h"
/**************************************************************************
 * Render hexadecimal characters as a binary stream.
 */
static unsigned char * hexout(out, in, len)
unsigned char * out;
unsigned char *in;
int len;
{
unsigned char * top = out + len;
/*
 * Build up half-byte at a time, subtracting 48 initially, and subtracting
 * another 7 (to get the range from A-F) if > (char) 9. The input must be
 * valid.
 */
register unsigned char * x = out,  * x1 = in;

    while (x < top)
    {
    register unsigned char x2;

        x2 = *x1 - (char) 48;
        if (x2 > (char) 48)
           x2 -= (char) 32;    /* Handle lower case */
        if (x2 > (char) 9)
           x2 -= (char) 7; 
        *x = (unsigned char) (((int ) x2) << 4);
        x1++;
        if (*x1 == '\0')
            break;
        x2 = *x1++ - (char) 48;
        if (x2 > (char) 48)
           x2 -= (char) 32;    /* Handle lower case */
        if (x2 > (char) 9)
           x2 -= (char) 7; 
        *x++ |= x2;
    }
    return x;
}
/*
 * Compress a buffer by removing all the white space
 *
 * Return the new length
 */
static int strip_white(wrd,i)
unsigned char * wrd;
int i;
{
unsigned register char * x = wrd;
unsigned register char * y = x;
    while (i > 0)
    {
        if (isspace(*x))
            x++;
        else
            *y++ = *x++;
        i--;
    }
    return y - wrd;
}
extern int getopt();
extern char *  optarg;
extern int optind;
/*
 * The first argument is the string, the others are the files to search
 */
int main(argc, argv)
int argc;
unsigned char ** argv;
{
int i;
int fd;
int len;
int eof_flag;
unsigned char *wrd;
int ch;
struct bm_table * bp;
struct bm_frag * bfp;
static unsigned char buf[65536];
int buf_cnt;              /* How many bytes are occupied in it        */
int case_insense = 0;
int white_space = 0;
int unicode = 0;
int all_flag = 0;
unsigned char * insp;
unsigned char * x;
unsigned char * y;

    while ((ch = getopt(argc, argv, "h?iwuax")) != EOF)
    {
        switch (ch)
        {
        case 'x':
            unicode = -1;
            case_insense = 0;
            white_space = 0;
            break;
        case 'u':
            unicode = 1;
            break;
        case 'a':
            all_flag = 1;
            break;
        case 'i':
            case_insense = 1;
            break;
        case 'w':
            white_space = 1;
            break;
        case 'h':
        default:
            fputs( "Provide a match string and files to search\n\
Options (provide first) -i (case insensitive) -u (Unicode) -w (ignore white space) -x (Hexadecimal) -a (All matches rather than first)\n",
                      stderr);
            exit(0);
        }
    }
    if (argc - optind < 1 )
        exit(1);
    len = strlen(argv[optind]);
    if (!white_space)
        wrd = argv[optind];
    else
    {
/*
 * Take a private copy of the match arguments
 */
        wrd = (unsigned char *) strdup(argv[optind]);
/*
 * Normalise by stripping out the white space if required.
 */
        if (white_space)
        {
            y = wrd + strip_white(wrd,len);
            *y = (unsigned char) '\0';
            len = y - wrd;
        }
    }
    if (len <= 0)
        exit(1);
    if (unicode == 1)
    {
        y = (unsigned char *) calloc(1, len + len + 2);
        for (x = wrd, wrd = y; *x != 0;)
        {
            *y++ = *x++;
            *y++ = '\0';
        }
        *y++ = '\0';
        *y++ = '\0';
        len += len;
    }
    else
    if (unicode == -1)
    {
        if ((len = strlen(wrd)) == 0
              || (len & 1) != 0)
        {
            fprintf(stderr,
              "Hexadecimal string %s has non-even length (%u)\n",
                        optarg, len);
                    exit(1);
        }
        for ( y = wrd; *y != 0; y++)
            if (!isxdigit(*y))
            {
                fprintf(stderr,
              "Invalid hex digit (%c); %s is not a valid hexadecimal string.\n",
                        *y, wrd);
                exit(1);
            }
        y = (unsigned char *) calloc(1, len/2);
        hexout(y, wrd, len);
        len = len/2;
        wrd = y;
    }
/*
 * Establish the match pattern
 */
    if (case_insense)
        bp = bm_casecompile_bin(wrd, len);
    else
        bp = bm_compile_bin(wrd, len);
    bfp = bm_frag_init(bp, bm_match_new);
    for (i = optind + 1; i < argc; i++)
    {    
        if (!strcmp(argv[i],"-"))
        {
            setmode(0, O_BINARY);
            fd = 0;
        }
        else
            fd = open(argv[i],O_RDONLY|O_BINARY);
        if (fd > -1)
        {
            for (eof_flag = 0; !eof_flag;)
            {
                buf_cnt = read(fd, &buf[0], sizeof(buf));
                if (buf_cnt < sizeof(buf))
                    eof_flag = 1;
                if (white_space)
                {
                    len = strip_white(&buf[0],buf_cnt);
                    if (!len)
                        continue;
                }
                else
                    len = buf_cnt;
                for (x = &buf[0];
                         x < &buf[len]
                         && (x = bm_match_frag(bfp, x, &buf[len])) != NULL;
                              x += bfp->bp->match_len) 
                {
/*                     printf("tell() = %ld\n", tell(fd));
                     printf("buffer offset = %d\n", (buf_cnt - (x - &buf[0]))); */
                     printf("Found %s in %s at %ld\n", argv[optind],
                                argv[i],((long signed int) tell(fd) - (buf_cnt - (x - &buf[0]))));
                     if (!all_flag)
                         exit(0);
                }
            }
            (void) close(fd);
        }
        else
        {
            perror("open()");
            fprintf(stderr, "Could not open %s for reading, error %d\n",
                          argv[i], errno);
        }
    }
    exit(0);
}
