/*
 * e2recs.c; Convert ASCI representations of records to binary, or vice versa.
 *
 * Copyright (c) E2 Systems, 1994
 *
 * There are two routines; a format compiler, and a converter.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems 1994";
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "e2conv.h"
extern char * fgets();
extern double strtod();
struct fld_descrip_arr {
    struct fld_descrip_arr * next_arr;
    int fld_cnt;
    struct fld_descrip desc_arr[128];
};
/*
 * e2rec_comp takes a string of format specifiers and returns a compiled
 * converter
 *
 * Format Specifiers consist of:
 * - Occurrence Count (default 1)
 * - Type letter
 *   - S, C Strings
 *   - X, characters
 *   - I, integers
 *   - Z, zoned (i.e. numeric charcters with a trailing sign ID)
 *   - P, packed
 *   - H, hexadecimal (i.e. raw binary data)
 *   - D, Double
 *   - I, Integer
 *   - T, Time or date
 *   - U, Unicode
 * (- More specification (for dates))
 * - Length
 *   If the length is zero, it means a 'C' string (ie. NULL terminated)
 *   if we are dealing with type S, otherwise it means 1.
 *   If the length is negative, it refers to the length of a counter.
 *   -1 = 1 byte
 *   -2 = 2 bytes Little-Endian
 *   -3 = 2 bytes Big-endian
 *   -4 = 4 bytes Little-endian
 *   -5 = 4 bytes Big-endian
 *   The length counts the elements, so if it is Unicode (double byte) the
 *   number of bytes is doubled.
 *
 * A NULL in_form signals a recursive call.
 *****************************************************************************
 * The use of strtok() ensures that this function is not thread-safe, although
 * the conversion code is supposed to be (bugs excepted, of course).
 *****************************************************************************
 */
int e2rec_comp(anchor, in_form)
struct iocon ** anchor;
char * in_form;
{
char * ip, *xp;
struct iocon * curi, *previ;
int rlen = 0;

    if (anchor == (struct iocon **) NULL)
        return -1;
    if (in_form != (char *) NULL)
    {
        ip = strdup(in_form);
        xp = strtok(ip, " \r\n\t");
    }
    else
        xp = strtok(NULL, " \r\n\t");
    for ( ; xp != (char *) NULL; xp = strtok(NULL, " \r\n\t"))
    {
    char * x;

        if (*anchor == (struct iocon *) NULL)
        {
            previ = *anchor;
            *anchor = (struct iocon *) malloc(sizeof(struct iocon));
            curi = *anchor;
        }
        else
        {
            curi->next_iocon = (struct iocon *) malloc(sizeof(struct iocon));
            previ = curi;
            curi = curi->next_iocon;
        }
        curi->next_iocon = (struct iocon *) NULL;
        curi->child_iocon = (struct iocon *) NULL;
        curi->occs = atoi(xp);
        if (curi->occs < 1)
            curi->occs  = 1;
        for (x = xp;
                *x != '\0' && *x != 'X' && *x != 'I' && *x != 'F' && *x != 'Q'
                && *x != '{' && *x != '}' && *x != 'H' && *x != 'U'
                && *x != 'Z' && *x != 'P' && *x !='D' && *x !='R' && *x != 'S';
                    x++);
        switch (*x)
        {
/*
 * It is worth noting:
 * - flen = Formatted (that is, ASCII) length
 * - alen = Actual Binary length
 * - The lengths provided should always be lengths in the binary file (alen)
 * However, this was not the case with P (Packed) or U (Unicode), causing
 * a certain amount of confusion to me, the programmer. This has now been fixed,
 * at the cost of breaking any ancient ascbin command files lying around with
 * P specifications in them.
 */
        case '\0':
            fprintf(stderr,"Invalid format %s\n", xp);
            if (in_form != (char *) NULL)
                free(ip);
            return -1;
        case 'X':
            curi->getfun = xin_r;
            curi->putfun = xout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen < 1)
                curi->alen = 1;
            curi->flen = curi->alen;
            break;
        case 'S':
            x++;
            curi->alen = atoi(x);
            if (curi->alen)
                curi->getfun = stin_r;
            else
                curi->getfun = cstin_r;
            curi->putfun = stout;
            curi->flen = curi->alen;
            break;
        case 'U':
            curi->getfun = unin_r;
            curi->putfun = unout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen == 0)
                curi->alen = 2;   /* No such thing as a 'C' unicode string */
            if (curi->alen > 0)
                curi->flen = curi->alen >> 1;
            else
                curi->flen = curi->alen; /* Flags variable length */
            break;
        case 'Q':
/*
 * Discard
 */
            curi->getfun = 0;
            curi->putfun = 0;
            x++;
            curi->alen = atoi(x);
            if (curi->alen < 1)
                curi->alen = 1;
            curi->flen = curi->alen;
            break;
        case 'Z':
            curi->getfun = zin_r;
            curi->putfun = zout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen < 1)
                curi->alen = 1;
            curi->flen = curi->alen;
            break;
        case 'D':
            curi->getfun = din_r;
            curi->putfun = dout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen > -1)
                curi->alen = 8;
            curi->flen = curi->alen;
            break;
        case 'F':
            curi->getfun = fin_r;
            curi->putfun = fout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen > -1)
                curi->alen = 4;
            curi->flen = curi->alen;
            break;
        case 'P':
            curi->getfun = pin_r;
            curi->putfun = pout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen < 1)
                curi->alen = 1;
            curi->flen = 2 * curi->alen;
            break;
        case 'I':
            curi->getfun = iin_r;
            curi->putfun = iout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen == 0
             || (curi->alen > 0 && curi->alen != 1 && curi->alen !=2  && curi->alen != 4 && curi->alen != 8 ))
                curi->alen = 2;
            curi->flen = curi->alen;
            break;
        case 'H':
            curi->getfun = hexin_r;
            curi->putfun = hexout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen == 0)
                curi->alen = 1;
            curi->flen = 2 * curi->alen;
            break;
        case '{':
/*
 * Start group
 */
            curi->getfun = 0;
            curi->putfun = 0;
            curi->alen = 0;
            curi->flen = 0;
            rlen += e2rec_comp(&(curi->child_iocon), (char *) NULL);
            break;
        case '}':
/*
 * End group
 */
            if (in_form != (char *) NULL)
                free(ip);
            if (previ == (struct iocon *) NULL)
                *anchor = (struct iocon *) NULL;
            else
                previ->next_iocon = (struct iocon *) NULL;
            free((char *) curi);
            return rlen;
        case 'R':
/*
 * Multiple occurrence count for the following field
 */
            curi->getfun = iin_r;
            curi->putfun = iout;
            x++;
            curi->alen = atoi(x);
            if (curi->alen != 1 && curi->alen !=2  && curi->alen != 4 && curi->alen != 8)
                curi->alen = 1;
            curi->flen = 0;   /* Flag that this is an occurrence count */ 
            break;
        }
        rlen += (curi->alen * curi->occs); /* Doesn't work with variable length
                                              fields */
    }
#ifdef DEBUG
    fprintf(stderr,"Record Length Calculated as %d but wrong if record includes variable elements\n",rlen);
#endif
    if (in_form != (char *) NULL)
        free(ip);
    return rlen;
}
/*
 * Field descriptor array processing
 */
static void free_desc_arr(this)
struct fld_descrip_arr * this;
{
struct fld_descrip_arr * t;

    while (this != (struct fld_descrip_arr *) NULL)
    {
        t = this->next_arr;
        free(this);
        this = t;
    }
    return;
}
static struct fld_descrip_arr * new_fld_descrip_arr(this)
struct fld_descrip_arr *this;
{
struct fld_descrip_arr * fdap;

    if ((fdap = (struct fld_descrip_arr *)
                                   malloc(sizeof(struct fld_descrip_arr)))
                     == (struct fld_descrip_arr *) NULL)
        return fdap;
    fdap->fld_cnt = 0;
    fdap->next_arr =  (struct fld_descrip_arr *) NULL;
    if (this != (struct fld_descrip_arr *) NULL)
         this->next_arr = fdap;
    return fdap;
}
static struct fld_descrip_arr * add_fld_descrip(this, fdp)
struct fld_descrip_arr * this;
struct fld_descrip *fdp;
{
/*
 * Add another bucket if needed
 */
    if (this == (struct fld_descrip_arr *) NULL || this->fld_cnt >= 128)
        this = new_fld_descrip_arr(this);
    if (fdp != (struct fld_descrip *) NULL)
        this->desc_arr[this->fld_cnt++] = *fdp;
    return this;
}
/*
 * Create a descriptor array
 */
int tidy_fld_desc(anchor, ret_arr)
struct fld_descrip_arr * anchor;
struct fld_descrip ** ret_arr;
{
int fld_cnt = 0;
struct fld_descrip_arr * fdap;
struct fld_descrip * fdp;
/*
 * Count the elements needed
 */
    for (fdap = anchor;
            fdap != (struct fld_descrip_arr *) NULL;
                fdap = fdap->next_arr)
        fld_cnt += fdap->fld_cnt;
/*
 * Allocate an array of the correct size
 */
    if ((*ret_arr = (struct fld_descrip *) malloc(sizeof(struct fld_descrip)
                              * fld_cnt)) == (struct fld_descrip *) NULL)
        return 0;
/*
 * Copy across the descriptors
 */
    for (fdap = anchor, fdp = *ret_arr;
            fdap != (struct fld_descrip_arr *) NULL;
                fdap = fdap->next_arr)
    {
        memcpy((char *) fdp, (char *) &(fdap->desc_arr[0]),
                fdap->fld_cnt * sizeof(struct fld_descrip));
        fdp += fdap->fld_cnt;
    }
/*
 * Release the original
 */
    free_desc_arr(anchor);
    return fld_cnt;
}
/*
 * Process a chain of iocon structures doing ASCII/Binary Conversion
 */
void one_rec_ab(curi, x, op, sepchar)
struct iocon * curi;
unsigned char ** x;
unsigned char ** op;
unsigned char sepchar;
{
int i;

    for (; curi != (struct iocon *) NULL; curi = curi->next_iocon)
    {
#ifdef DEBUG
         fprintf(stderr,"IOCON: this %x next %x child %x = %s\n\
 occs %d flen %d alen %d getfun %x putfun %x\n",
                curi,
                curi->next_iocon,
                curi->child_iocon,
                *x,
                curi->occs,
                curi->flen,
                curi->alen,
                curi->getfun,
                curi->putfun);
#endif
        for (i = curi->occs; i; i--)
        {
            if (*x == (char *) NULL)
            {
                fputs("Input corrupt: too few fields\n", stderr);
                return;
            }
            if (curi->child_iocon != (struct iocon *) NULL)
            {
                one_rec_ab(curi->child_iocon, x, op, sepchar);
                continue;
            }
            else
            if (curi->putfun != 0)
            {
                if (curi->flen == 0 && curi->getfun == iin_r)
                        /* Multiple occurrence count */
                    curi->next_iocon->occs = atoi(*x); 
                *op = (*(curi->putfun))(*op,*x,curi->alen);
            }
            *x = nextasc(NULL,sepchar,0);
        }
    }
    return;
}
/*
 * Convert a string of elements from binary to ASCII. No provision is made
 * for the case when the data runs out before the specification.
 */
void one_rec_ba(curi, ip, op, sepchar)
struct iocon * curi;
unsigned char ** ip;
unsigned char ** op;
unsigned char sepchar;
{
int i;
unsigned char * x;
char buf[65536];

    for (; curi != (struct iocon *) NULL; curi = curi->next_iocon)
    {
#ifdef DEBUG
        fprintf(stderr,"IOCON: this %x next %x child %x\n\
 occs %d flen %d alen %d getfun %x putfun %x\n",
                curi,
                curi->next_iocon,
                curi->child_iocon,
                curi->occs,
                curi->flen,
                curi->alen,
                curi->getfun,
                curi->putfun);
#endif
        for (i = curi->occs; i; i--)
        {
            if (curi->child_iocon != (struct iocon *) NULL)
                one_rec_ba(curi->child_iocon, ip, op, sepchar);
            else
            if (curi->getfun == 0)
            {
                *ip += curi->alen;
            }
            else
            {
/*
 * The multiple occurrence count for a following repeating group.
 */
                if (curi->flen == 0 && curi->getfun == iin_r)
                {
                    x = (unsigned char *)
                            (*(curi->getfun))(*ip,curi->alen, &buf[0], &buf[65535]);
                    curi->next_iocon->occs = atoi(x);
                }
                else
                    x = (unsigned char *)
                            (*(curi->getfun))(*ip, curi->alen,
                                                    &buf[0], &buf[65535]);
/***** This should alen, that is the Binary length **********/
#ifdef DEBUG
                fprintf(stderr, "ip: %x conv: %s\n", (long) *ip, x);
#endif
                if (curi->alen == 0)
                    *ip += strlen(*ip) + 1;
                else
                if (curi->alen < 0)
                {
                int skip_len;

                    x = get_cnt(*ip, curi->alen, &skip_len);
                    *ip = x + skip_len;
                    if (curi->getfun == unin_r)
                        *ip = *ip + skip_len;
                }
                else
                    *ip += curi->alen;
                while (*x != '\0')
                    *((*op)++) = *x++;
                *((*op)++) = sepchar;
            }
        }
    }
    **op = '\0';
    return;
}
/*
 * e2rec_conv converts an input to an output buffer according to the spec
 * given.
 *
 * It returns the length of the occupied output buffer
 */
int e2rec_conv(aflag, ibuf, obuf, anchor, sepchar)
int aflag;
char * ibuf;
char * obuf;
struct iocon * anchor;
char sepchar;
{
char * x;
char * op;
struct iocon * curi;
/*
 * Process an ASCII to Binary conversion
 */
    if (aflag)
    {
        x = nextasc(ibuf,sepchar,0);
        op = obuf;
        one_rec_ab(anchor, &x, &op, sepchar); 
    }
    else
    {
/*
 * Process a Binary to ASCII conversion
 */
        op = obuf;
        one_rec_ba(anchor, &ibuf, &op, sepchar); 
        *op++ = '\n'; 
        *op = '\0';
    }
    return (op - obuf);
}
/***************************************************************************
 * Map a delimited ASCII record, converting to binary format as we go.
 */
void map_one_rec(fdap, curi, x, op, sepchar, escchar)
struct fld_descrip_arr ** fdap;
struct iocon * curi;
unsigned char ** x;
unsigned char ** op;
unsigned char sepchar;
unsigned char escchar;
{
int i;
struct fld_descrip fd;

    for (; curi != (struct iocon *) NULL; curi = curi->next_iocon)
    {
#ifdef DEBUG
         fprintf(stderr,"IOCON: this %x next %x child %x = %s\n\
 occs %d flen %d alen %d getfun %x putfun %x\n",
                curi,
                curi->next_iocon,
                curi->child_iocon,
                *x,
                curi->occs,
                curi->flen,
                curi->alen,
                curi->getfun,
                curi->putfun);
#endif
        for (i = curi->occs; i; i--)
        {
            if (*x == (char *) NULL)
            {
                fputs("Input corrupt: too few fields\n", stderr);
                return;
            }
            if (curi->child_iocon != (struct iocon *) NULL)
            {
                map_one_rec(fdap, curi->child_iocon, x, op, sepchar, escchar);
                continue;
            }
            else
            if (curi->putfun != 0)
            {
                if (curi->flen == 0)
                        /* Multiple occurrence count */
                    curi->next_iocon->occs = atoi(*x); 
                fd.fld = *op;
                *op = (*(curi->putfun))(*op,*x,curi->alen);
                fd.len = *op - (unsigned char *) fd.fld;
                *fdap = add_fld_descrip(*fdap, &fd);
            }
            *x = nextasc(NULL,sepchar,escchar);
        }
    }
    return;
}
/***************************************************************************
 * Map a binary record, converting to delimited ASCII as we go.
 */
void map_one_rec_bin(fdap, curi, ip, op, sepchar, escchar)
struct fld_descrip_arr ** fdap;
struct iocon * curi;
unsigned char ** ip;
unsigned char ** op;
unsigned char sepchar;
unsigned char escchar;
{
int i;
struct fld_descrip fd;
unsigned char * x, *x1;
char buf[65536];

    for (; curi != (struct iocon *) NULL; curi = curi->next_iocon)
    {
#ifdef DEBUG
         fprintf(stderr,"IOCON: this %x next %x child %x = %s\n\
 occs %d flen %d alen %d getfun %x putfun %x\n",
                curi,
                curi->next_iocon,
                curi->child_iocon,
                *ip,
                curi->occs,
                curi->flen,
                curi->alen,
                curi->getfun,
                curi->putfun);
#endif
        for (i = curi->occs; i; i--)
        {
            if (curi->child_iocon != (struct iocon *) NULL)
                map_one_rec_bin(fdap, curi->child_iocon, ip, op,
                                      sepchar, escchar);
            else
            if (curi->getfun == 0)
            {
                *ip += curi->alen;
            }
            else
            {
/*
 * The multiple occurrence count for a following repeating group.
 */
                if (curi->flen == 0 && curi->getfun != cstin_r)
                {
                    x = (unsigned char *)
                            (*(curi->getfun))(*ip,curi->alen,
                                              &buf[0], &buf[65535]);
                    if (curi->next_iocon != NULL)
                        curi->next_iocon->occs = atoi(x);
                }
                else
                    x = (unsigned char *)
                            (*(curi->getfun))(*ip, curi->alen,
                                              &buf[0], &buf[65535]);
/***** This should be alen, the binary length **********/
#ifdef DEBUG
                fprintf(stderr, "ip: %x conv: %s\n", (long) *ip, x);
#endif
                fd.fld = *ip;
                if (curi->alen == 0)
                    *ip += strlen(*ip) + 1;
                else
                if (curi->alen < 0)
                {
                int skip_len;

                    x1 = get_cnt(*ip, curi->alen, &skip_len);
                    *ip = x1 + skip_len;
                    if (curi->getfun == unin_r)
                        *ip = *ip + skip_len;
                }
                else
                    *ip += curi->alen;
                fd.len = *ip - (unsigned char *) fd.fld;
                *fdap = add_fld_descrip(*fdap, &fd);
                while (*x != '\0')
                {
                    if (*x == sepchar || *x == escchar)
                        *((*op)++) = escchar;
                    *((*op)++) = *x++;
                }
                *((*op)++) = sepchar;
            }
        }
    }
    **op = '\0';    /* Make sure that anything returned is NUL terminated */
    return;
}
/*
 * e2rec_map either converts an input to an output buffer according to the spec
 * given, or it simply finds the positions of the input fields in the input
 * buffer. In either case it returns a count of fields found, and an array of
 * field descriptors.
 */
int e2rec_map(desc_arr, ibuf, obuf, anchor, sepchar, escchar)
struct fld_descrip ** desc_arr;
char * ibuf;
char * obuf;
struct iocon * anchor;
char sepchar;
char escchar;
{
struct fld_descrip_arr * root;
struct fld_descrip_arr * fdap;
struct fld_descrip fd;

    root = add_fld_descrip((struct fld_descrip_arr *) NULL,
                   (struct fld_descrip *) NULL);
    fdap = root;
    ibuf = nextasc(ibuf,sepchar,escchar);
    if (anchor != (struct iocon *) NULL && obuf != (char *) NULL)
        map_one_rec(&fdap, anchor, &ibuf, &obuf, sepchar, escchar); 
    else
        while(ibuf != (char *) NULL)
        {
            fd.fld = ibuf;
            fd.len = strlen(ibuf);
            fdap = add_fld_descrip(fdap, &fd);
            ibuf = nextasc(NULL,sepchar,escchar);
        }
    return tidy_fld_desc(root, desc_arr);
}
/*
 * e2rec_map_bin converts an input to an output buffer according to the spec
 * given, or it simply finds the positions of the input fields in the input
 * buffer. In either case it returns a count of fields found, and an array of
 * field descriptors.
 ****************************************************************************
 * Note that multiple sub-records are converted, but are not mapped but
 * not converted, as is also the case with the routine above.
 ****************************************************************************
 * ZZZZZZZZZZZZ What does that MEAN!? Possibly, it means that if you ask for
 * for the conversion, it happens, but if you only ask for the mapping, the
 * mapping can only map a single instance of a sub-record, because there is
 * only a single set of sub-descriptors without having found an actual count.
 ****************************************************************************
 */
int e2rec_map_bin(desc_arr, ibuf, obuf, anchor, sepchar, escchar)
struct fld_descrip ** desc_arr;
unsigned char * ibuf;
unsigned char * obuf;
struct iocon * anchor;
char sepchar;
char escchar;
{
struct fld_descrip_arr * root;
struct fld_descrip_arr * fdap;
struct fld_descrip fd;
struct iocon * curi;
int i;

    root = add_fld_descrip((struct fld_descrip_arr *) NULL,
                   (struct fld_descrip *) NULL);
    fdap = root;
    if (anchor != (struct iocon *) NULL && obuf != (char *) NULL)
        map_one_rec_bin(&fdap, anchor, &ibuf, &obuf, sepchar, escchar); 
    else
    {
        for (curi = anchor;
                 curi != (struct iocon *) NULL;
                     curi = curi->next_iocon)
        {
#ifdef DEBUG
            fprintf(stderr,"IOCON: this %x next %x child %x = %s\n\
 occs %d flen %d alen %d getfun %x putfun %x\n",
                curi,
                curi->next_iocon,
                curi->child_iocon,
                ibuf,
                curi->occs,
                curi->flen,
                curi->alen,
                curi->getfun,
                curi->putfun);
#endif
            for (i = curi->occs; i; i--)
            {
                if (curi->getfun == 0)
                {
                    ibuf += curi->alen;
                }
                else
                {
                    fd.fld = ibuf;
                    if (curi->alen == 0)
                        ibuf += strlen(ibuf) + 1;
                    else
                    if (curi->alen < 0)
                    {
                    int skip_len;

                        ibuf = get_cnt(ibuf, curi->alen, &skip_len);
                        ibuf = ibuf + skip_len;
                        if (curi->getfun == unin_r)
                            ibuf = ibuf + skip_len;
                    }
                    else
                        ibuf += curi->alen;
                    fd.len = ibuf - (unsigned char *) fd.fld;
                    fdap = add_fld_descrip(fdap, &fd);
                }
            }
        }
    }
    return tidy_fld_desc(root, desc_arr);
}
