/******************************************************************************
 * mempat.c - Routines to manipulate things that are best handled with mmap(),
 * when dd doesn't work.
 */
static char * sccs_id = "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems 2008\n";
#include <sys/types.h>
#ifndef MINGW32
#include <sys/mman.h>
#endif
#ifdef LINUX
#define O_BINARY 0
#define setmode(x,y)
#include <sys/types.h>
#include <unistd.h>
#define tell(fd) lseek64(fd, (off64_t)0, SEEK_CUR)
#endif
#include <sys/fcntl.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#ifndef WIN32
#define O_BINARY 0
#define setmode(x,y)
#define MAP_SIZE 4096
#else
#define MAP_SHARED 0
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SIZE 65536
#ifdef errno
#undef errno
#endif
#define errno GetLastError()
#endif
#include "bmmatch.h"
extern int optind;
extern int opterr;
extern char * optarg;
extern int getopt();
static char * hlp =
"mempat: Read or write from a location in a file accessed via mmap().\n\
Copyright (c) E2 Systems Limited 2008. All rights reserved.\n\
However, this is Free Software (as in Beer).\n\
Options\n\
-f filename, input for write input or output for read output\n\
-h output this string\n\
-l length to read (implies -r)\n\
-r (default) read\n\
-n add a NUL to the input string\n\
-s write input is string (implies -w)\n\
-w write\n\
-p target is a string to patch; it will be globally patched\n\
-x write input is hexadecimal string (implies -w)\n\
Arguments: Provide the target offset, and the name of a target file\n\
If neither -f, -x or -s is provided, input comes from stdin (-w) or output\n\
goes to stdout (-r, default). If arguments conflict, the last to appear prevail.\n";
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
 * Routine to extract or patch, assuming that the mapping must honour some
 * alignment constraint (set with MAP_SIZE)
 */
static void do_map(ofp, fd, target, length, buf)
FILE * ofp;            /* NULL means we are patching; otherwise dumping */ 
int fd;                /* file descriptor   */
off64_t target;          /* point of interest */
size_t length;         /* length to extract/patch */
unsigned char * buf;   /* source or destination */
{
size_t map_length;
off64_t offset;
int map_flags;
unsigned char *map_base;
 
    offset = (target/MAP_SIZE) *MAP_SIZE;
    map_length = (target + length) -offset;
/*
    map_length = ((((target + length) -offset) - 1)/MAP_SIZE + 1) *MAP_SIZE;
 *  fprintf(stderr, "Rounding to page size(%x), map length %lx at address %lx\n",
 *               MAP_SIZE, map_length, offset);
 */
    if (ofp == (FILE *) NULL)
        map_flags = PROT_READ|PROT_WRITE;
    else
        map_flags = PROT_READ; 
/*
 * Map the length page
 */
    map_base = mmap(0, map_length, map_flags, MAP_SHARED, fd, offset);
    if (map_base == (unsigned char *) -1)
    {
        perror("mmap() failed");
#if __WORDSIZE == 64
        fprintf(stderr, "Failed to map length %lx at address %lx error %d\n",
                length, (long unsigned int) target, errno);
#else
        fprintf(stderr, "Failed to map length %x at address %x error %d\n",
                length, (unsigned int) target, errno);
#endif
        exit(1);
    }
    if (ofp != (FILE *) NULL)
    {
        fwrite(map_base + (target - offset), sizeof(char), length, ofp);
        fclose(ofp);
    }
    else
        memcpy(map_base + (target - offset), buf, length);

    if (munmap(map_base, map_length) == -1)
    {
        perror("munmap() failed");
#if __WORDSIZE == 64
        fprintf(stderr, "Failed to unmap length %lx at address %lx error %d\n",
                map_length, (long unsigned int) map_base, errno);
#else
        fprintf(stderr, "Failed to unmap length %x at address %x error %d\n",
                map_length, (unsigned int) map_base, errno);
#endif
        printf("Memory unmap failed.\n");	
    }
    return;
}
static void global_edit(fname, map_fd, to_patch, newval, patch_len)
char * fname;
int map_fd;
char * to_patch;
char * newval;
size_t patch_len;
{
int fd;
int eof_flag;
unsigned char * wrd;
struct bm_table * bp;
struct bm_frag * bfp;
static unsigned char buf[65536];
int buf_cnt;              /* How many bytes are occupied in it        */
unsigned char * insp;
unsigned char * x;
unsigned char * y;
/*
 * Take a private copy of the match arguments
 */
    bp = bm_compile(to_patch);
    bfp = bm_frag_init(bp, bm_match_new);
    if ((fd = open(fname,O_RDONLY|O_BINARY)) < 0)
        exit(1);
    for (eof_flag = 0; !eof_flag;)
    {
        buf_cnt = read(fd, &buf[0], sizeof(buf));
        if (buf_cnt < sizeof(buf))
            eof_flag = 1;
        for (x = &buf[0];
               x < &buf[buf_cnt]
            && (x = bm_match_frag(bfp, x, &buf[buf_cnt])) != NULL;
                    x += bfp->bp->match_len) 
        {
            do_map(NULL, map_fd, 
         (off64_t)((long signed int) tell(fd) - (buf_cnt - (x - &buf[0]))),
              (size_t) patch_len, newval);
        }
    }
    (void) close(fd);
    return;
}
/*
 * Load up what can be read from the FILE pointer into a buffer
 */
static unsigned char * load_file(ifp, lenp)
FILE * ifp;
size_t * lenp;
{
unsigned char * buf;
int buflen;
int readlen;
int ret;
unsigned char * so_far;
unsigned char * top;

    buflen = 1048576;
    if ((buf = malloc( buflen )) == (unsigned char *) NULL)
    {
        fputs("Out of memory!?\n", stderr);
        exit(1);
    }
    readlen = 0;
    so_far = buf;
    top = buf + buflen;
    while ((ret = fread(so_far, sizeof(unsigned char),
                (buflen - readlen), ifp)) > 0)
    {
        so_far += ret;
        readlen += ret;
        if (so_far >= top)
        {
            buflen += buflen;
            if ((buf = (unsigned char *)
                  realloc(buf, buflen)) 
                    == (unsigned char *) NULL)
            {
                fputs("Out of memory!?\n", stderr);
                exit(1);
            }
            so_far = buf + readlen;
            top = buf + buflen;
        }
    }
    fclose(ifp);
    *lenp = readlen;
    return buf;
}
/*********************************************************************
 * Main program starts here
 * VVVVVVVVVVVVVVVVVVVVVVVV
 */
int main(argc, argv)
int argc;
char ** argv;
{
int fd;
int oflags;
char * foflags;
off64_t target = 0;
size_t length;
FILE *fp;
char * tfile;
char * insp;
unsigned char * buf;
int nul_flag = 0;
int write_flag = 0;
int hex_flag = 0;
int match_flag = 0;
int i, n;
char * match_str;

    tfile = (char *) NULL;
    insp = (char *) NULL;
/*
 * Examine the input options
 */
    while ((i = getopt(argc, argv, "pf:hl:ns:rwx:")) != EOF)
    {
        switch (i)
        {
        case 'f':
            tfile = optarg;
            break;
        case 'l':
/*
 * Try multiple radices
 */
            i = strlen(optarg);
#if __WORDSIZE == 64
            if ((
              (sscanf(optarg, "%lu%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "%lx%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "X%lx%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "x%lx%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "H%lx%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "h%lx%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "o%lo%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "O%lo%n", &length, &n) == 0
                || n != i)
             ) || length < 1)
#else
            if ((
              (sscanf(optarg, "%u%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "%x%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "X%x%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "x%x%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "H%x%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "h%x%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "o%o%n", &length, &n) == 0
                || n != i)
             && ( sscanf(optarg, "O%o%n", &length, &n) == 0
                || n != i)
             ) || length < 1)
#endif
            {
                fprintf(stderr, "Read length %s must be a number > 0\n",
                      optarg);
                fputs(hlp, stderr);
                exit(1);
            }
            write_flag = 0;
            break;
        case 'n':
            nul_flag = 1;
            break;
        case 's':
            insp = optarg;
            write_flag = 1;
            hex_flag = 0;
            break;
        case 'r':
            write_flag = 0;
            break;
        case 'w':
            write_flag = 1;
            break;
        case 'p':
            match_flag = 1;
            break;
        case 'x':
            for ( insp = optarg; *insp != 0; insp++)
                if (!isxdigit(*insp))
                {
                    fprintf(stderr,
              "Invalid hex digit (%c); %s is not a valid hexadecimal string.\n",
                        *insp, optarg);
                    exit(1);
                }
            insp = optarg;
            write_flag = 1;
            hex_flag = 1;
            break;
        case 'h':
        default:
            fputs( hlp, stderr);
            exit(1);
        }
    }
    if (argc - optind < 2)
    {
        fputs("Insufficient arguments\n", stderr);
        fputs(hlp, stderr);
        exit(1);
    }
    if (write_flag)
    {
        oflags = O_RDWR|O_BINARY;
        foflags = "rb";
    }
    else
    {
        oflags = O_RDONLY|O_BINARY;
        foflags = "wb";
    }
    if (tfile == (char *) NULL)
    {
        if (write_flag)
            fp = stdin;
        else
            fp = stdout;
        setmode(fileno(fp), O_BINARY); /* DOS-ism; NO-OP elsewhere */
    }
    else
    if ((fp = fopen(tfile, foflags)) == (FILE *) NULL)
    {
        perror("fopen() failed");
        fprintf (stderr, "Failed to open %s mode %s, error %d\n",
                tfile, foflags, errno);
        fputs(hlp, stderr);
        exit(1);
    }
/*
 * Try multiple radices for the offset
 */
    i = strlen(argv[optind]);
    if (match_flag)
        match_str = argv[optind];
    else
    if ( (sscanf(argv[optind], "%lu%n", (unsigned long int *) &target, &n) == 0
                || n != i)
      && (sscanf(argv[optind], "%lx%n", (unsigned long int *) &target, &n) == 0
                || n != i)
      && (sscanf(argv[optind], "x%lx%n", (unsigned long int *) &target, &n) == 0
                || n != i)
      && (sscanf(argv[optind], "X%lx%n", (unsigned long int *) &target, &n) == 0
                || n != i)
      && (sscanf(argv[optind], "h%lx%n", (unsigned long int *) &target, &n) == 0
                || n != i)
      && (sscanf(argv[optind], "H%lx%n", (unsigned long int *) &target, &n) == 0
                || n != i)
      && (sscanf(argv[optind], "o%lo%n", (unsigned long int *) &target, &n) == 0
                || n != i)
      && (sscanf(argv[optind], "O%lo%n", (unsigned long int *) &target, &n) == 0
                || n != i))
    {
        fprintf(stderr, "Target offset %s must be an unsigned number\n",
                      argv[optind]);
        fputs(hlp, stderr);
        exit(1);
    }
    optind++;
/*
 * Open the file that is to be subjected to memory mapping
 */
    if ((fd = open(argv[optind], oflags)) == -1)
    {
        perror("open() failed");
        fprintf (stderr, "Failed to open %s mode flags %d, error %d\n",
                 argv[optind], oflags, errno);
        fputs(hlp, stderr);
        exit(1);
    }
/*
 * Write flag set; we are going to patch; load a buffer with whatever
 */
    if (write_flag)
    {
        if (insp == (char *) NULL)
            buf = load_file(fp, &length);
        else
        {
            length = strlen(insp);
            if (hex_flag)
            {
                if (length < 1)
                {
                    fputs("Hex string requested but nothing supplied\n",
                                   stderr);
                    fputs(hlp, stderr);
                    exit(1);
                }
                length = (length - 1)/2 + 1;
                if ((buf = (unsigned char *) malloc(length)) == NULL)
                {
                    fputs("Out of memory!?\n", stderr);
                    exit(1);
                }
                hexout(buf, (unsigned char *) insp, length);
            }
            else
            {
                if (nul_flag)
                    length++;
                if (length < 1)
                {
                    fputs("String requested without NUL and nothing supplied\n",
                                stderr);
                    fputs(hlp, stderr);
                    exit(1);
                }
                buf = (unsigned char *) insp;
            }
        }
    }
/*
 * Patch or dump as requested
 */ 
    if (match_flag)
        global_edit(argv[optind], fd, match_str, buf, (size_t) length);
    else
        do_map((write_flag ? NULL : fp), fd, target, length, buf);
    close(fd);
    exit(0);
}
