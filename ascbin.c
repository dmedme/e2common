/*
 * ascbin; Convert and ASCI file to a binary file, or vice versa.
 *
 * This program is used to read and write various proprietary file formats
 * and convert them to and from line-feed-delimited text files.
 *
 * There isn't actually an ASCII/EBCDIC conversion option because we haven't
 * yet had to deal with an unconverted binary file with EBCDIC text, but the
 * support routines in e2conv.c exist; they had to be used on an EBCDIC binary
 * file that had been converted, corrupting all the packed and binary fields in
 * it.
 *
 * Copyright (c) E2 Systems, 1994
 *
 * This program expects the following input parameters
 *
 * -a (ASCII input) or not (Binary input)
 * -v variable length (binary) input
 * -p preamble length
 * -b blocksize
 * -t block trail length
 * -s field separator (default :)
 * -n non-spanned (default is spanned)
 * -l line-control
 * -f input file (default is ifp)
 * -o output file (default is ofp)
 * -d Dump format
 * -e Encode dumped
 * -r Dump in PATH script (re-usable) format
 *
 * Format Specifiers, which consist of:
 * - Occurrence Count (default 1)
 * - Type letter
 *   - X, characters
 *   - R, repeat count (for multiply-occurring fields)
 *   - {, group start
 *   - }, group start
 *   - I, integers
 *   - H, binary stream, rendered in Hexadecimal
 *   - Z, zoned (i.e. numeric charcters with a trailing sign ID)
 *   - P, packed
 *   - D, Double
 *   - I, Integer
 *   - T, Time or date
 * (- More specification (for dates))
 * - Length
 *
 * For ASCII input, we use:
 * - fgets() to read the record
 * - nextfield() to split it up (because it recognises an escape character)
 *
 * For binary input, we:
 * - fread() over the preamble (in case the device doesn't support seeking)
 * - read a buffer's worth (to the end of the debris; see below)
 * - process the whole records
 *   -  Loop; process the fields; build up an output record
 * - shift the debris back to the beginning (if spanning)
 * - repeat until EOF
 *   
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems 1994";
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifdef MINGW32
#include <fcntl.h>
#endif
#include "e2conv.h"
extern char * fgets();
extern double strtod();
static struct var_integra_header {
    unsigned char XAB_D_B_COD;
    unsigned char XAB_D_B_BLN;
    unsigned char XAB_D_B_HDR1;
    unsigned char XAB_D_B_HDR2;
    unsigned char XAB_D_L_NXT[4];
    unsigned char XAB_D_B_RFO;
    unsigned char XAB_D_B_ATR;
    unsigned char XAB_D_W_LRL[2];
    unsigned char XAB_D_L_HBK[4];
    unsigned char XAB_D_L_EBK[4];
    unsigned char XAB_D_W_FFB[2];
    unsigned char XAB_D_B_BKZ;
    unsigned char XAB_D_B_HSZ;
    unsigned char XAB_D_W_MRZ[2];
    unsigned char XAB_D_W_DXQ[2];
    unsigned char XAB_D_W_GBC[2];
    unsigned char XAB_D_B_XXX[8];
    unsigned char XAB_D_W_VERLIMIT[2];
    unsigned char XAB_D_L_SBN[4];
} var_integra_header = { 0x1d, 0x2c, 0xfe, 0xdc, 0, 0, 0, 0,
2, /* Variable - 1 would be fixed */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0 /* x40 */, /* (Maximum) record size, Low Byte/High Byte */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 1, /* Word */
0, 0, 0, 0
};
/***********************************************************************
 * Getopt support
 */
extern int optind;           /* Current Argument counter.      */
extern char *optarg;         /* Current Argument pointer.      */
extern int opterr;           /* getopt() err print flag.       */
/***********************************************************************
 * Functions in this file
 */
static int parse_formats();
/***********************************************************************
 * Main program starts here
 * VVVVVVVVVVVVVVVVVVVVVVVV
 */
int main(argc,argv,envp)
int argc;
char ** argv;
char ** envp;
{
int flag = 0;
int lflag = 0;
struct iocon * anchor = (struct iocon *) NULL;
int aflag = 0;
int dflag = 0;
int iflag = 0;
int vflag = 0;
int rflag = 0;
int plen = 0;
int blen = 0;
int tlen =0;
int rlen;
char sepchar = ':';
int nflag = 0;
int i;
unsigned char * ibuf;
unsigned char * obuf;
unsigned char * cp;
unsigned char *x;
int c;
FILE *ifp;
FILE *ofp;
/*
 * Validate the arguments
 *
 * -a (ASCII input) or not (Binary input)
 * -d Dump in driver-compatible format (clear text plus hexadecimal)
 * -e Re-encode from driver-compatible format (clear text plus hexadecimal)
 * -p preamble length
 * -v variable length binary input with a record separator
 * -w variable length binary input with a record count
 * -b blocksize
 * -t block trail length
 * -s field separator (default :)
 * -n non-spanned (default is spanned)
 * -l line feed delimited output
 * -i integra header
 */
#ifdef MINGW32
    _setmode(0,  O_BINARY);
    _setmode(1,  O_BINARY);
#endif
    ifp = stdin;
    ofp = stdout;
    while ( ( c = getopt ( argc, argv, "f:o:dervwab:p:t:s:nhli" ) ) != EOF )
    {
        switch ( c )
        {
        case 'i':
            iflag = 1;
            break;
        case 'd':
            dflag = 1;
            break;
        case 'r':
            rflag = 1;
            break;
        case 'e':
            dflag = -1;
            break;
        case 'v':
            vflag = 1;
            break;
        case 'f':
            if ((ifp= fopen(optarg, "rb")) == (FILE *) NULL)
            {
                perror("fopen() failed");
                fprintf(stderr, "Failed to open %s for input\n", optarg);
                exit(1);
            }
            break;
        case 'o':
            if ((ofp = fopen(optarg, "wb")) == (FILE *) NULL)
            {
                perror("fopen() failed");
                fprintf(stderr, "Failed to open %s for output\n", optarg);
                exit(1);
            }
            break;
        case 'w':
            vflag = 2;
            break;
        case 'a':
            aflag = 1;
            break;
        case 'n':
            nflag = 1;
            break;
        case 'l':
            lflag = 1;
            break;
        case 'b':
            blen = atoi(optarg);
            break;
        case 'p' :
            plen = atoi(optarg);
            break;
        case 't' :
            tlen = atoi(optarg);
            break;
        case 's' :
            sepchar = *optarg;
            break;
        default:
        case '?' : /* Default - invalid opt.*/
               (void) fprintf(stderr, "ascbin - Convert file layouts\n\
Options:\n\
 -a (ASCII input (default is binary))\n\
 -s Separator character (default is :)\n\
 -d dump format output\n\
 -r dump format without Unicode Strings\n\
 -e dump format input, binary output\
 -f input file (default is stdin))\n\
 -o output file (default is stdout))\n\
 -b Block Size\n\
 -p Preamble Size\n\
 -t Block Trailer size\n\
 -v Variable Length Binary records with line feed separators!?\n\
 -w Variable Length Binary records preceded by lengths\n\
 -h (Output this message)\n\
 -l (Output line feeds after every record)\n\
 -n (Not spanned (Default is spanned))\n\
Followed by a conversion specification; a valid series of format specifiers\n\
each of which consists of:\n\
 - Occurrence Count (default 1)\n\
 - Type letter or grouping directive\n\
 - A Field Length; necessary if the length cannot be inferred from the type\n\
The valid Type Letters are\n\
 - X, Character data in fixed length fields\n\
 - I, Integers\n\
 - H, Binary stream, rendered in Hexadecimal\n\
 - Z, Zoned (i.e. numeric characters with a trailing sign ID)\n\
 - P, Packed\n\
 - S, Null terminated strings\n\
 - U, Unicode strings\n\
 - D, Double Precision Floating Point\n\
 - F, Single Precision Floating Point\n\
 - I, Integer\n\
 - Q, Characters to skip\n\
 - T, Time or date\n\
Group control characters are\n\
 - R, Repeat count (for multiply-occurring groups)\n\
 - {, Group start\n\
 - }, Group start\n");
               exit(1);
            break;
       }
    }
    if (plen < 0)
    {
        fprintf(stderr,"Preamble length must be greater or equal to 0\n");
        exit(1);
    }
    if (tlen < 0)
    {
        fprintf(stderr,"Trailer length must be greater or equal to 0\n");
        exit(1);
    }
    if (tlen > blen)
    {
        fprintf(stderr,"Trailer length must be less than block length\n");
        exit(1);
    }
    if (!dflag)
    {
        rlen = parse_formats(&anchor, argc, argv);
        fprintf(stderr,"Record Length Calculated as %d\n",rlen);
        if (blen < 1 && !aflag)
            blen = rlen;
        else
        if (!aflag && rlen > 2 * blen)
        {
            fprintf(stderr,"Record length greater than twice block length!?\n");
            exit(1);
        }
    }
/*
 * Process the Input Stream.
 */
    if (aflag || dflag)
    {
        ibuf = (char *) malloc(65536);
        if (dflag != 1)
            obuf = (char *) malloc(65536);
        if (iflag) /* If integra, output the VAX-like control information */
            fwrite((char *) & var_integra_header, sizeof(char),
                    sizeof(struct var_integra_header), ofp);
    }
    else
    {
    char *pbf = (char *) malloc(plen);

        fread(pbf, sizeof(char), plen, ifp); /* Skip preamble */
        free(pbf);
        if (vflag != 2)
            ibuf = (char *) malloc(2 * blen); 
        else
            ibuf = (char *) malloc(8 * rlen); 
        obuf = (char *) malloc(65536);
    }
/*
 * Go through the records
 */
    for(cp = ibuf;;)
    {
    unsigned char * op;
/*
 * Process an ASCII to Binary conversion
 */
        if (aflag)
        {
            if (fgets(ibuf,65536,ifp) == (char *) NULL)
                break;
            x = ibuf + strlen(ibuf) - 1;
            if (x >= ibuf && *x == '\n')
                *x = '\0';
            x = nextasc(ibuf,sepchar,0);
            op = obuf;
            one_rec_ab(anchor, &x, &op, sepchar);
            if (iflag)
            {      /* If integra, output the VAX-like control information */
#ifdef VAX_CLONE
/*
 * NB. If this was really VAX-like these would be this way round
 */
                putc(rlen & 0xff,ofp);        /* Low byte of length */
                putc((rlen >> 8) & 0xff,ofp); /* High byte of length */
#else
                putc((rlen >> 8) & 0xff,ofp); /* High byte of length */
                putc(rlen & 0xff,ofp);        /* Low byte of length */
#endif
            }
            if (vflag == 2)
            {
                putc(((op - obuf)) & 0xff,ofp);        /* Low byte of length */
                putc(((op - obuf) >> 8) & 0xff,ofp); /* High byte of length */
            }
            fwrite(obuf,op - obuf, sizeof(char),ofp); 
            if (lflag)
                putc((int) '\n',ofp);
/*
 * NB. If this was really VAX-like the following would be necessary
 */
#ifdef VAX_CLONE
            else
            if (iflag && (rlen % 2))
                putc(0,ofp);
#endif
        }
        else
        if (dflag == -1)
        {
            if (fgets(ibuf,65536,ifp) == (char *) NULL)
                break;
            cp = ibuf + strlen(ibuf) - 1;
            if (cp > ibuf &&   *cp == '\n' && *(cp - 1) == '\\')
                cp--;
            else
                cp++;
            tlen = get_bin(obuf, ibuf, (cp - ibuf));
            fwrite(obuf, sizeof(char), tlen, ofp);
        }
        else
        if (dflag == 1)
        {
            while ((i = fread(ibuf,sizeof(char),65536, ifp)) > 0)
                if (rflag == 0)
                    gen_handle_nolf(ofp, ibuf, ibuf + i, 1);
                else
                    gen_handle_no_uni_nolf(ofp, ibuf, ibuf + i, 1);
            break;
        }
        else
        {
        unsigned char * ip;
        unsigned char * ve;
/*
 * Process a Binary to ASCII conversion
 */
            if (vflag == 2)
            {
                if ((i = fread(cp,sizeof(char),2, ifp)) < 1)
                    break;
                blen = *cp * 256 + *(cp + 1);
                if (blen > 8 * rlen)
                {
                    ibuf = (unsigned char *) realloc(ibuf, blen *2);
                    rlen = blen/4 + 1;
                }
            }
            if (vflag == 1)
            {
                if ((i = fread(cp,sizeof(char),blen - tlen,ifp)) < 1)
                    break;
                 ve = cp+blen - tlen - 1;
                 while (ve > ibuf && *ve != (unsigned char) '\n')
                     ve--;
                 if (ve > ibuf && *ve == (unsigned char) '\n')
                 {
                     tlen = blen - (ve - ibuf) -1;
                     if (tlen < 0)
                        abort();
                     memcpy(ibuf + blen, ve+1, tlen);
                     memset(ve,' ',tlen);
                 }
                 else
                     tlen = 0;
            }
            else
            {
                if ((i = fread(cp,sizeof(char),blen,ifp)) < 1)
                    break;
                if (i != blen)
                {   /* An incomplete last block */
                    blen = i;
                    tlen = 0;
                }
            }
            for (ip = ibuf, op = obuf;
                   (vflag == 1 && ip < ve )
                 || (vflag != 1 && ip  <= cp - rlen + (blen - tlen));)
            {
                one_rec_ba(anchor, &ip, &op, sepchar);
                *op++ = '\n'; 
            }
            fwrite(obuf,op - obuf, sizeof(char),ofp); 
            if (vflag == 1)
            {
                if (tlen != 0)
                {
                    memcpy(ibuf,ip,tlen);
                    cp = ibuf + tlen;
                }
                else
                    cp = ibuf;
            }
            else
            if (!nflag && (i = (blen - tlen) - (ip - cp)) > 0)
            {   /* Spanned, save the junk at the end */
                memcpy(ibuf,ip,i);
                cp = ibuf + i;
            }
            else
                cp = ibuf;
        }
    }
/*
 * Closedown
 */
    fclose(ifp);
    fclose(ofp);
    exit(0);
}
/*
 * Recognise the input format specifications.
 */
static int parse_formats(anchor, argc, argv)
struct iocon ** anchor;
int argc;
char ** argv;
{
char * x;
char buf[16384];
/*
 * Format Specifiers, which consist of:
 * - Occurrence Count (default 1)
 * - Type letter
 *   - R, occurrence count for following multiply occurring field
 *   - X, characters
 *   - Q, discard
 *   - I, integers
 *   - H, hexadecimal
 *   - Z, zoned (i.e. numeric characters with a trailing sign ID)
 *   - P, packed
 *   - D, Double
 *   - I, Integer
 *   - T, Time or date
 * (- More specification (for dates))
 * - Length
 */
    x = buf;
    (*anchor) = (struct iocon *) NULL;
    while (optind < argc)
    {
        strcpy(x, argv[optind]);
        x += strlen(x);
        *x = ' ';
        x++;
        optind++;
    }
    x--;
    *x = '\0';
    return e2rec_comp(anchor, buf);
}
