/*
 * e2conv.c - Miscellaneous format conversion routines to deal with
 * odd formats, eg. ORACLE-specific, packed decimal
 */
static char * sccs_id = "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1994";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ansi.h"
#include "e2conv.h"
char * to_char();
static char * months[]= {
"Jan",
"Feb",
"Mar",
"Apr",
"May",
"Jun",
"Jul",
"Aug",
"Sep",
"Oct",
"Nov",
"Dec"};
static void dec2byte();
#ifdef CORRUPT
/*
 * The following code was written to deal with binary files that had been
 * converted from EBCDIC to ASCII. It remains so that it can, as the
 * need arises, be used again in the future.
 */
static int asc_to_ebc[] = {
/* 0  */ 0, 1, 2, 3, 55, 45, 46, 47, 22, 5, 37, 11, 12, 13, 14, 15,
/* 16 */ 16, 17, 18, 19, 60, 61, 50, 38, 24, 25, 63, 39, 28, 29, 30, 31,
/* 32 */ 64, 90, 127, 123, 91, 108, 80, 125, 77, 93, 92, 78, 107, 96, 75, 97,
240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 122, 94, 76, 126, 110, 111,
124, 193, 194, 195, 196, 197, 198, 199, 200, 201, 209, 210, 211, 212, 213, 214,
215, 216, 217, 226, 227, 228, 229, 230, 231, 232, 233, 173, 224, 189, 95, 109,
121, 129, 130, 131, 132, 133, 134, 135, 136, 137, 145, 146, 147, 148, 149, 150,
151, 152, 153, 162, 163, 164, 165, 166, 167, 168, 169, 192, 79, 208, 161, 7,
/* 128 */ 32, 33, 34, 35, 36, 21, 6, 23, 40, 41, 42, 43, 44, 9, 10, 27,
/* 144 */ 48, 49, 26, 51, 52, 53, 54, 8, 56, 57, 58, 59, 4, 20, 62, 225,
/* 160 */ 65, 66, 67, 68, 69, 70, 71, 72, 73, 81, 82, 83, 84, 85, 86, 87,
88, 89, 98, 99, 100, 101, 102, 103, 104, 105, 112, 113, 114, 115, 116, 117,
118, 119, 120, 128, 138, 139, 140, 141, 142, 143, 144, 154, 155, 156, 157, 158,
159, 160, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
184, 185, 186, 187, 188, 189, 190, 191, 202, 203, 204, 205, 206, 207, 218, 219,
220, 221, 222, 223, 234, 235, 236, 237, 238, 239, 250, 251, 252, 253, 254, 255};
static void asctoebc(buf_ptr, in_len)
unsigned char * buf_ptr;
int in_len;
{
register unsigned char *x = buf_ptr;
register unsigned char * bound = x + in_len;
    for (x = buf_ptr; x < bound; x++)
    {
        *x = asc_to_ebc[*x];
    }
    fflush(stdout);
    return;
}
#endif
#ifndef E2BIT_TEST
/*
 * Bitmap macros
 */
#define E2BIT_TEST(bitmap,index)\
         (*((bitmap) + ((index)/(sizeof(int) << 3))) & (1 << ((index) % (sizeof(int) << 3))))
#define E2BIT_SET(bitmap,index)\
         (*((bitmap) + ((index)/(sizeof(int) << 3))) |= (1 << ((index) % (sizeof(int) << 3))))
#define E2BIT_CLEAR(bitmap,index)\
         (*((bitmap) + ((index)/(sizeof(int) << 3))) &= ~(1 << ((index) % (sizeof(int) << 3))))
#endif
/****************************************************************************
 * memspn() and memcspn() -  strspn() and strcspn() analogues with bounds
 * checking
 */
__inline int memspn(base, bound, pat_len, pstr)
unsigned char * base; 
unsigned char * bound; 
int pat_len;
unsigned char * pstr;
{
int bitmap[256 /(sizeof(int) << 3)]; 
unsigned char * xp;

    if (!pat_len || base >= bound)
        return 0;
    memset(bitmap, 0, sizeof(bitmap));
    for ( xp = pstr; pat_len > 0; xp++, pat_len--)
        E2BIT_SET(bitmap, (int) (*xp));
    for ( xp = base; xp < bound; xp++)
        if (!E2BIT_TEST(bitmap, (int) (*xp)))
            return (xp - base);
    return (bound - base);
}
__inline int memcspn(base, bound, pat_len, pstr)
unsigned char * base; 
unsigned char * bound; 
int pat_len;
unsigned char * pstr;
{
int bitmap[256 /(sizeof(int) << 3)]; 
unsigned char * xp;

    if (base >= bound)
        return 0;
    if (!pat_len)
        return (bound - base);
    memset(bitmap, 0, sizeof(bitmap));
    for ( xp = pstr; pat_len > 0; xp++, pat_len--)
        E2BIT_SET(bitmap, (int) (*xp));
    for ( xp = base; xp < bound; xp++)
        if (E2BIT_TEST(bitmap, (int) (*xp)))
            return (xp - base);
    return (bound - base);
}
unsigned char asc_ind();
/*****************************************************************************
 * EBCDIC - ASCII conversion
 */
static unsigned char ebc_to_asc[] = { 
/* 0 */ 0, 1, 2, 3, 156, 9, 134, 127, 151, 141, 142, 11, 12, 13, 14, 15,
/* 16 */ 16, 17, 18, 19, 157, 133, 8, 135, 24, 25, 146, 143, 28, 29, 30, 31,
/* 32 */ 128, 129, 130, 131, 132, 10, 23, 27, 136, 137, 138, 139, 140, 5, 6, 7,
/* 48 */ 144, 145, 22, 147, 148, 149, 150, 4, 152, 153, 154, 155, 20, 21, 158,
26,
/* 64 */ 32, 160, 161, 162, 163, 164, 165, 166, 167, 168, 213, 46, 60, 40, 43,
124,
/* 80 */ 38, 169, 170, 171, 172, 173, 174, 175, 176, 177, 33, 36, 42, 41, 59,
126,
/* 96 */ 45, 47, 178, 179, 180, 181, 182, 183, 184, 185, 203, 44, 37, 95, 62,
63,
/* 112 */ 186, 187, 188, 189, 190, 191, 192, 193, 194, 96, 58, 35, 64, 39, 61,
34,
/* 128 */ 195, 97, 98, 99, 100, 101, 102, 103, 104, 105, 196, 197, 198, 199,
200, 201,
/* 144 */ 202, 106, 107, 108, 109, 110, 111, 112, 113, 114, 94, 204, 205, 206,
207, 208,
/* 160 */ 209, 229, 115, 116, 117, 118, 119, 120, 121, 122, 210, 211, 212, 91,
214, 215,
/* 176 */ 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 93,
230, 231,
/* 192 */ 123, 65, 66, 67, 68, 69, 70, 71, 72, 73, 232, 233, 234, 235, 236, 237,
/* 208 */ 125, 74, 75, 76, 77, 78, 79, 80, 81, 82, 238, 239, 240, 241, 242,
243,
/* 224 */ 92, 159, 83, 84, 85, 86, 87, 88, 89, 90, 244, 245, 246, 247, 248, 249,
/* 240 */ 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 250, 251, 252, 253, 254, 255
};
static unsigned char ret_buf[BUFSIZ];
/***********************************************************************
 * Read a little-endian int field from a non-aligned buffer.
 */
int get_fld_int_le(fdp)
struct fld_descrip * fdp;
{
short int s;
int i;

    if (fdp->len == 1)
        return (int) (*(fdp->fld));
    else
    if (fdp->len == 2)
        return (int) (*(fdp->fld) + ((*(fdp->fld + 1)) << 8));
    else
    if (fdp->len == 4)
        return (int) (*(fdp->fld) + ((*(fdp->fld + 1)) << 8) +
                 ((*(fdp->fld + 2)) << 16) + ((*(fdp->fld + 3)) << 24));
    else
        return 0;
}
/***********************************************************************
 * Read a field
 *
 * Routine that is a cross between strtok() and strchr()
 * Splits string out into fields.
 * Differences from strtok()
 * - second parameter is a char, not a series of chars
 * - returns a zero length string for adjacent separator characters
 * - optionally handles an escape character for the separators; pass in
 *   zero if escape processing is not desired.
 */
unsigned char * nextasc_r(start_point,sepchar,escchar,got_to,ret_buf,limt)
unsigned char * start_point;
unsigned char sepchar;
unsigned char escchar;
unsigned char **got_to;
unsigned char * ret_buf;
unsigned char * limt;
{
register unsigned char * x;
register unsigned char * x1;
register unsigned char * top;

    if (got_to == (unsigned char **) NULL
     || (start_point == (unsigned char *) NULL
     && (*got_to == (unsigned char *) NULL
      || **got_to == (unsigned char) '\0')))
        return (unsigned char *) NULL;
    if (start_point != (unsigned char *) NULL)
        *got_to = start_point;
    else
/*    if (**got_to != sepchar)
        start_point = *got_to;
    else
  */       start_point = *got_to+1;
    for (x = start_point,
         x1 = ret_buf;
              x1 < limt && *x != (unsigned char) '\0' && *x != sepchar;
                  x++)
    {
        if (escchar && (*x == escchar))
        {
            x++;
            if (*x == (unsigned char) '\0')
                break;
        }
        *x1++ = *x;
    }
    *x1 = (unsigned char) '\0';
    *got_to = x;    
    return ret_buf;
}
unsigned char * nextasc(start_point,sepchar,escchar)
unsigned char * start_point;
unsigned char sepchar;
unsigned char escchar;
{
static unsigned char * got_to;

    return nextasc_r(start_point,sepchar,escchar, &got_to, ret_buf,
       ret_buf + sizeof(ret_buf) - 1);
}
/*
 * Function to write a count into the output stream, according to our rules
 */
static unsigned char * add_cnt(out_buf, len_flag, act_len)
unsigned char * out_buf;
int len_flag;
unsigned int act_len;
{
    switch(len_flag)
    {
    case -1:
        *out_buf++ = (unsigned char) act_len;
        break;
    case -2:
        *out_buf++ = (unsigned char) (act_len & 0xff);
        *out_buf++ = (unsigned char) ((act_len >>8) & 0xff);
        break;
    case -3:
        *out_buf++ = (unsigned char) ((act_len >>8) & 0xff);
        *out_buf++ = (unsigned char) (act_len & 0xff);
        break;
    case -4:
        *out_buf++ = (unsigned char) (act_len & 0xff);
        *out_buf++ = (unsigned char) ((act_len >>8) & 0xff);
        *out_buf++ = (unsigned char) ((act_len >>16) & 0xff);
        *out_buf++ = (unsigned char) ((act_len >>24) & 0xff);
        break;
    case -5:
        *out_buf++ = (unsigned char) ((act_len >>24) & 0xff);
        *out_buf++ = (unsigned char) ((act_len >>16) & 0xff);
        *out_buf++ = (unsigned char) ((act_len >>8) & 0xff);
        *out_buf++ = (unsigned char) (act_len & 0xff);
        break;
    default:
        fprintf(stderr, "Invalid length count marker %d\n", len_flag);
        break;
    }
    return out_buf;
}
/*
 * Function to get a count from the input stream, according to our rules
 */
unsigned char * get_cnt(in_buf, len_flag, act_len)
unsigned char * in_buf;
int len_flag;
unsigned int * act_len;
{
    switch(len_flag)
    {
    case -1:
        *act_len = (unsigned int) *in_buf++;
        break;
    case -2:
        *act_len = *in_buf++;
        *act_len += (*in_buf++ << 8);
        if (*act_len == 65535)
            *act_len = 0;
        break;
    case -3:
        *act_len = (*in_buf++ << 8);
        *act_len += *in_buf++;
        break;
    case -4:
        *act_len = *in_buf++;
        *act_len += (*in_buf++ << 8);
        *act_len += (*in_buf++ << 16);
        *act_len += (*in_buf++ << 24);
        break;
    case -5:
        *act_len = (*in_buf++ << 24);
        *act_len += (*in_buf++ << 16);
        *act_len += (*in_buf++ << 8);
        *act_len += *in_buf++;
        break;
    default:
        fprintf(stderr, "Invalid length count marker %d\n", len_flag);
        break;
    }
    return in_buf;
}
/*
 *   - X, characters
 */
unsigned char * xin_r(buf_ptr,in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
unsigned char * x;

    for (x =  buf_ptr + in_len - 1;
            x >= buf_ptr &&
        ( *x == '\0' || *x == ' ' || *x == '\t' || *x == '\r' || *x == '\n' );
                x--);    /* Strip trailing spaces */
    if ((limt - ret_buf) < (x - buf_ptr + 1))
        in_len = (limt - ret_buf);
    else
        in_len = (x - buf_ptr + 1) ;
    memcpy(ret_buf, buf_ptr, in_len);
    x = ret_buf + in_len;
    *x = '\0';
    x--;
    while (x > (char *) &ret_buf[0])
    {
       if (*x == '\0')
           *x=' ';
       x--;
    }
    return ret_buf;
}
/*
 *   - X, characters
 */
unsigned char * xin(buf_ptr,in_len)
unsigned char * buf_ptr;
int in_len;
{
    return xin_r(buf_ptr,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
unsigned char * xout(out_buf, in_buf, out_len)
unsigned char * out_buf;
unsigned char * in_buf;
int out_len;
{
unsigned char * x;
unsigned char * y;

    for ( x = out_buf, y = in_buf; out_len && (*y != '\0'); x++, y++, out_len--)
        *x = *y;
    while (out_len--)
        *x++ = ' ';
    return x;
}
/*
 *   - S, C strings
 */
unsigned char * stin_r(buf_ptr,in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
    if (in_len < 0)
    {
        buf_ptr = get_cnt(buf_ptr, in_len, &in_len);
        if (in_len == 0)
        {
            *ret_buf = '\0';
            return ret_buf;
        }
    }
    if ((limt - ret_buf) < in_len)
        in_len = (limt - ret_buf) - 1;
    strncpy((char *) ret_buf, buf_ptr, in_len);
    *(ret_buf + in_len) = '\0';
    return ret_buf;
}
/*
 *   - S, C strings with null terminator and no clue regarding the length
 */
unsigned char * cstin_r(buf_ptr,in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
    in_len = (limt - ret_buf) + 1;
    strncpy((char *) ret_buf, buf_ptr, in_len);
    *(ret_buf + in_len) = '\0';
    return ret_buf;
}
unsigned char * stin(buf_ptr,in_len)
unsigned char * buf_ptr;
int in_len;
{
    return stin_r(buf_ptr,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
unsigned char * stout(out_buf, in_buf, out_len)
unsigned char * out_buf;
unsigned char * in_buf;
int out_len;
{
unsigned char * x;
unsigned char * y;

    if (out_len == 0)
    {
        strcpy(out_buf, in_buf);
        return out_buf + strlen(out_buf) + 1;
    }
    else
    if (out_len < 0)
    {
    int act_len = strlen(in_buf);

       out_buf = add_cnt(out_buf, out_len, act_len);
       memcpy(out_buf,in_buf,act_len);
       return out_buf + act_len;
    }
    else
    {
        for (x = out_buf, y = in_buf;
                out_len > 0 && (*y != '\0');
                    x++, y++, out_len--)
            *x = *y;
        while(out_len-- > 0)
            *x++ = '\0';
        return x;
    }
}
/*
 *   - U, Unicode strings
 */
void uni2asc(op, ip, cnt)
unsigned char * op;
unsigned char * ip;
unsigned int cnt;         /* Count of UNICODE characters */
{
    while (cnt > 0)
    {
        *op++ = *ip;
        ip += 2;
        cnt--;
    }
    *op = '\0';
    return;
}
void asc2uni(op, ip, cnt)
unsigned char * op;
unsigned char * ip;
unsigned int cnt;         /* Count of UNICODE characters */
{
    while (cnt > 0)
    {
        *op++ = *ip++;
        *op++ = '\0';
        cnt--;
    }
    return;
}
unsigned char * unin_r(buf_ptr,in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;    /* Binary length; twice the character count */
unsigned char * ret_buf;
unsigned char * limt;
{
    if (in_len < 0)
    {
        buf_ptr = get_cnt(buf_ptr, in_len, &in_len);
        if (in_len == 0)
        {
            *ret_buf = '\0';
            return ret_buf;
        }
        else
            in_len += in_len;
    }
    if ((limt - ret_buf) < in_len)
        in_len = (limt - ret_buf) - 1;
    uni2asc((char *) ret_buf, buf_ptr, in_len/2); 
    return ret_buf;
}
unsigned char * unin(buf_ptr,in_len)
unsigned char * buf_ptr;
int in_len;
{
    return unin_r(buf_ptr,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
unsigned char * unout(out_buf, in_buf, out_len)
unsigned char * out_buf;
unsigned char * in_buf;
int out_len;    /* Binary length; twice the character count */
{
unsigned char * x;
unsigned char * y;

    if (out_len == 0)
    {
        strcpy(out_buf, in_buf);
        return out_buf + strlen(out_buf) + 1;
    }
    else
    if (out_len < 0)
    {
    int act_len = strlen(in_buf);

        out_buf = add_cnt(out_buf, out_len, act_len);
        asc2uni(out_buf,in_buf,act_len);
        return out_buf + act_len + act_len;
    }
    else
    {
        for (x = out_buf, y = in_buf;
                out_len > 0 && (*y != '\0');
                    x++, y++, out_len -= 2)
        {
            *x++ = *y;
            *x = '\0';
        }
        while(out_len-- > 0)
            *x++ = '\0';
        return x;
    }
}
/*
 *   - I, integers
 */
unsigned char * iin_r(buf_ptr, in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
union { unsigned long l; unsigned char c[sizeof(unsigned long)];} x;
#ifdef CORRUPT
    asctoebc(buf_ptr,in_len);
#endif
    if (in_len < 0)
    {
        buf_ptr = get_cnt(buf_ptr, in_len, &in_len);
        if (in_len == 0)
            return ret_buf;
    }
    if (in_len > sizeof(unsigned long))
        in_len = sizeof(unsigned long);
    x.l = 0;
    memcpy(&x.c[0],buf_ptr,in_len);/* Avoid possible alignment errors */
    buf_ptr = &x.c[0];
    if (in_len == 1)
        sprintf(ret_buf,"%u",(unsigned int) *buf_ptr);
    else
    if (in_len == 2)
        sprintf((char *) ret_buf,"%u",(int) *((unsigned short int *) buf_ptr));
    else /* (in_len == 4) */
        sprintf((char *) ret_buf,"%u",(int) *((unsigned int *) buf_ptr));
    return ret_buf;
}
unsigned char * iin(buf_ptr, in_len)
unsigned char * buf_ptr;
int in_len;
{
    return iin_r(buf_ptr,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
unsigned char * iout(out_buf, in_buf, out_len)
unsigned char * out_buf;
unsigned char * in_buf;
int out_len;
{
union { unsigned int l;  short int i;} x;

    if (out_len == 1)
    {
        *out_buf = (unsigned char) atoi(in_buf);
    }
    else
    if (out_len == 2)
    {
        x.i = (unsigned short int) atoi(in_buf);
        memcpy(out_buf, (char *) &(x.i),2);
                /* Avoid possible alignment errors */
    }
    else
    if (out_len < 0)
    {
       x.l = (unsigned int) atol(in_buf);
       out_buf = add_cnt(out_buf, out_len, 
            (x.l < 256) ? 1 : ((x.l < 65536) ? 2 : 4));
       if (x.l < 256)
       {
           *out_buf++ = (unsigned char) x.l;
           return out_buf;
       }
       else
       if (x.l < 65536)
       {
           x.i = x.l;
           memcpy(out_buf, (char *) &(x.i),2);
           return out_buf + 2;
       }
       else
       {
           memcpy(out_buf,(char *) &(x.l),4);
           return out_buf + 4;
       }
       
    } 
    else /* (in_len == 4) */
    {
        x.l = (unsigned int) atol(in_buf);
        memcpy(out_buf,(char *) &(x.l),4); /* Avoid possible alignment errors */
    }
    return out_buf + out_len;
}
/*
 *   - Z, zoned (i.e. numeric characters with a trailing sign ID)
 *     There may be more to these than this. The final characters can be
 *     any of a number of alphabetic characters, I think; does it indicate
 *     the mantissa?
 */
unsigned char * zin_r(buf_ptr, in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
unsigned char * x = buf_ptr + in_len -1;

    if (*x == '{')
    {
        ret_buf[0] = '-';
        *x = '\0';
        x = &ret_buf[1];
    }
    else
    {
        *x = '\0';
        x = ret_buf;
    }
    while (*buf_ptr == '0')
        buf_ptr++;
    if (*buf_ptr == '\0')
        *x++ = '0';
    else
    while (*buf_ptr != '\0')
        *x++ = *buf_ptr++;
    *x = '\0';
    if ((x - ret_buf) > in_len)
        abort();
       
    return (char *) ret_buf;
}
unsigned char * zin(buf_ptr, in_len)
unsigned char * buf_ptr;
int in_len;
{
    return zin_r(buf_ptr,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
unsigned char * zout(out_buf, in_buf, out_len)
unsigned char * out_buf;
unsigned char * in_buf;
int out_len;
{
unsigned char * x;
unsigned char * y;

    x = out_buf + out_len - 1;
    y = in_buf + strlen(in_buf) - 1;
    if (*in_buf == '-')
    {
        *x = '{';
        in_buf++;
    }
    else
    {
        *x = '0';
    }
    while ( x > out_buf && y > in_buf)
        *x-- = *y--;
    while(x > out_buf)
        *x-- = '0';
    return out_buf + out_len;
}
/*
 *   - P, packed
 */
static void perr(lab,base,bound)
int lab;
unsigned char *base;
unsigned char *bound;
{
    fprintf(stderr,"Label %d Attempt to convert invalid BCD: ", lab);
    for ( ; base < bound; base++)
          fprintf(stderr, "%02x",((unsigned int) (*base)) & 0xff);
    putc((int) '\n', stderr);
    return;
}
unsigned char * pin_r(buf_ptr, in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
unsigned char * x;
unsigned char c;
unsigned char h;
unsigned char l;
unsigned char *bound;
static int con_count;

    con_count++;
#ifdef DEBUG
    perr(con_count,buf_ptr,buf_ptr + in_len);
#endif
    x = buf_ptr + in_len -1; /* Last byte of input field */
    bound = x + 1;                  /* In case of garbage in the field */
#ifdef CORRUPT
    asctoebc(buf_ptr,in_len);
#endif
/*
 * Search for the sign in the field
 */
    for(;;)
    {
        c = *x;
        if ((l = (char) (c&((char) 0xf))) == (char) 0xd)
        {
            ret_buf[0] = '-';
            x = &ret_buf[1];
            break;
        }
        else
        if (l == (char) 0xc)
        {
            x = ret_buf;
            break;
        }
        else
        {
            x--;
            bound--;
            if (x < buf_ptr)
            {
                ret_buf[0] = '0';
                ret_buf[1] = '\0';
                perr(con_count,buf_ptr,buf_ptr + in_len);
                return ret_buf;
            }
        }
    }
/*
 * Now skip over any leading zeroes, stopping if we hit the last character
 */
    while (*buf_ptr != c && *buf_ptr == '\0')
        buf_ptr++;
    c = *buf_ptr++;
    l = (unsigned char) (c & (unsigned char) 0xf);
    h = (unsigned char) (48 + ((c>>4) & 0xf));    

/*
 * If there is only one digit, output it
 */
    if ( l == (unsigned char) 0xc || l == (unsigned char) 0xd)
       *x++ = h;
    else
    {
/*
 * Don't output a leading zero
 */
        if (h != '0')
            *x++ = h;
        do
        {
            *x++ = (unsigned char) (48 + l);
            c = *buf_ptr++;
            l = (unsigned char) (c & 0xf);
            h = (unsigned char) (48 + ((c>>4) & 0xf));    
            *x++ = h;
        }
        while  ( l != (unsigned char) 0xc
              && l != (unsigned char) 0xd
              && buf_ptr < bound);
        if ( l != (unsigned char) 0xc && l != (unsigned char) 0xd)
            perr(con_count,buf_ptr - 1, bound);
    }    
    *x = '\0';
    return (char *) ret_buf;
}
unsigned char * pin(buf_ptr, in_len)
unsigned char * buf_ptr;
int in_len;  /* The input length, alen */
{
    return pin_r(buf_ptr,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
unsigned char * pout(out_buf, in_buf, out_len)
unsigned char * out_buf;
unsigned char * in_buf;
int out_len;   /* The output field length, alen */
{
unsigned char * x;
unsigned char * y;
unsigned char c;

    x = out_buf + out_len - 1;
    y = in_buf + strlen(in_buf) - 1;
    if (*in_buf == '-')
    {
        c = (unsigned char) 0xd;
        in_buf++;
    }
    else
    {
        c = (unsigned char) 0xc;
    }
    while ( x > out_buf && y > in_buf)
    {
        c |= (unsigned char) (((*y - (unsigned char) 48) << 4));
        *x-- = c;
        y--;
        if (y < in_buf)
        {
            c = '\0';
            break;
        }
        c = (char) (*y - (unsigned char) 48);
        y--;
    }
    if ( c != '\0')
        *x-- = c;
    while(x > out_buf)
        *x-- = '\0';
    return out_buf + out_len;
}
/*
 *   - D, Double
 */
unsigned char * din_r(buf_ptr, in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
double x;
unsigned char * y;

    if (in_len < 0)
    {
        buf_ptr = get_cnt(buf_ptr, in_len, &in_len);
        if (in_len == 0)
        {
            *ret_buf = '\0';
            return ret_buf;
        }
    }
    if (in_len > sizeof(double))
        in_len = sizeof(double);
    memcpy((char *) &x,buf_ptr,in_len);/* Avoid possible alignment errors */
    (void)  sprintf(ret_buf,"%f",x);
    for (y = (char *) (ret_buf + strlen(ret_buf));
             y > ((char *) ret_buf) && (*y == '\0' || *y == '0');
                  y--)
              *y = '\0';
    if (*y == '.')
        *y = '\0'; 
    return ret_buf;
}
unsigned char * din(buf_ptr, in_len)
unsigned char * buf_ptr;
int in_len;
{
    return din_r(buf_ptr,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
unsigned char * dout(out_buf, in_buf, out_len)
unsigned char * out_buf;
unsigned char * in_buf;
int out_len;
{
double x;

    x =  strtod(in_buf, (char **) NULL);
    memcpy(out_buf, (char *) &x,sizeof(double)); /* Avoid possible alignment errors */
    return out_buf + out_len;
}
/*
 *   - F, Float (single precision)
 */
unsigned char * fin_r(buf_ptr, in_len,ret_buf,limt)
unsigned char * buf_ptr;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
float x;
unsigned char * y;

    if (in_len < 0)
    {
        buf_ptr = get_cnt(buf_ptr, in_len, &in_len);
        if (in_len == 0)
        {
            *ret_buf = '\0';
            return ret_buf;
        }
    }
    if (in_len > sizeof(float))
        in_len = sizeof(float);
    memcpy((char *) &x,buf_ptr,in_len);/* Avoid possible alignment errors */
    (void)  sprintf(ret_buf,"%f",(double)x);
    for (y = ((char *) ret_buf) + strlen(ret_buf);
             y > ((char *) ret_buf) && (*y == '\0' || *y == '0');
                  y--)
              *y = '\0';
    if (*y == '.')
        *y = '\0'; 
    return ret_buf;
}
unsigned char * fin(buf_ptr, in_len)
unsigned char * buf_ptr;
int in_len;
{
    return fin_r(buf_ptr,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
unsigned char * fout(out_buf, in_buf, out_len)
unsigned char * out_buf;
unsigned char * in_buf;
int out_len;
{
float x;

    x = (float) strtod(in_buf, (char **) NULL);
    memcpy(out_buf, (char *) &x,sizeof(float)); /* Avoid possible alignment errors */
    return out_buf + out_len;
}
/*
 * Bring in hexadecimal binary data, and output a stream of bytes.
 */
unsigned char * hexin_r(in, in_len,ret_buf,limt)
unsigned char * in;
int in_len;
unsigned char * ret_buf;
unsigned char * limt;
{
unsigned char * out = ret_buf;

    if (in_len < 0)
    {
        in = get_cnt(in, in_len, &in_len);
        if (in_len == 0)
        {
            *ret_buf = '\0';
            return ret_buf;
        }
    }
    if (in_len > (limt - ret_buf)/2)
        in_len = (limt - ret_buf)/2;
    for (; in_len; in_len--, in++)
    { 
        *out++ = "0123456789ABCDEF"[(((int) *in) & 0xf0) >> 4];
        *out++ = "0123456789ABCDEF"[(((int) *in) & 0xf)];
    }
    *out = '\0';
    return ret_buf;
}
unsigned char * hexin(in, in_len)
unsigned char * in;
int in_len;
{
    return hexin_r(in,in_len,ret_buf,ret_buf + sizeof(ret_buf) - 1);
}
/**************************************************************************
 * Render hexadecimal characters as a binary stream. Len is the input length
 */
unsigned char * hexout(out, in, len)
unsigned char * out;
unsigned char *in;
int len;
{
unsigned char * top = out + len;
/*
 * Build up half-byte at a time, subtracting 48 initially, and subtracting
 * another 7 (to get the range from A-F) if > (char) 9;
 */
register unsigned char * x = out,  * x1 = in;

    while (x < top)
    {
        register char x2;
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
static char high[101] = "0000000000111111111122222222223333333333444444444455555555556666666666777777777788888888889999999999";
static char low[101] = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
/*
 * Render an ORACLE-internal number in characters.
 *
 * The input argument is a pointer to a 1 byte length, which is followed
 * by the exponent byte and the mantissa.
 */
short int ora_num(in,out,len)
unsigned char *in;
unsigned char *out;
int len;
{
register int i;
register unsigned char *rx = in;
register char * x1 = (char *) out;
int expon;
register int r;

    if (len < 1)
        len = *rx++ - 1;
    else
        len--;
/*
 * Zero or illegal numbers
 */
    if (*rx == (unsigned char) 0x80 || len > 20 || len < 1)
       *x1++ = '0';
    else
/*
 * Positive number
 */
    if (*rx > (unsigned char ) 0x80)
    {
        expon = (int) ((unsigned int) *rx++) - 0xc1;
        if (expon < 0)
        {
        int i;
/*
 * Decimal; output the leading zeroes
 */
            *x1++ = '0';
            *x1++ = '.';
            for (i = expon + 1; i < 0;i++)
            {
                *x1++ = '0';
                *x1++ = '0';
            }
        }
/*
 * The first digit may have a leading zero; always ignore it.
 */
        r = (*rx++) -1;
        *x1 = high[r];
        if (*x1 != '0' || expon < 0)
            x1++;
        *x1++ = low[r];
        len--;                /* The number still left */
        if (expon < 0)
        {
            for (i = 0; i < len ;i++)
            {
                r = (*rx++) -1;
                *x1++ = high[r];
                *x1++ = low[r];
            }
            if (*(x1 - 1) == '0')
               x1--;
        }
        else
        {
            for (i = 0; i < expon && i < len ;i++)
            {
                r = (*rx++) -1;
                *x1++ = high[r];
                *x1++ = low[r];
            }
            if (expon < len)
            {
                *x1++ = '.';
                for (; i < len ;i++)
                {
                    r = (*rx++) -1;
                    *x1++ = high[r];
                    *x1++ = low[r];
                }
                if (*(x1 - 1) == '0')
                   x1--;
            }
            else
            for (; i < expon ;i++)
            {
                *x1++ = '0';
                *x1++ = '0';
            }
        }
    }
    else
    {
/*
 * Negative number
 */
        if (*(rx + len) == 'f')
            len--;
        *x1++ = '-';
        expon =  (~*rx++ &0xff) - 0xc1;
        if (expon < 0)
        {
        int i;
/*
 * Decimal; output the leading zeroes
 */
            *x1++ = '0';
            *x1++ = '.';
            for (i = expon + 1; i < 0; i++)
            {
                *x1++ = '0';
                *x1++ = '0';
            }
        }
/*
 * The first digit may have a leading zero; always ignore it.
 */
        r = 101 - (*rx++);
        *x1 = high[r];
        if (*x1 != '0' || expon < 0)
            x1++;
        *x1++ = (r % 10) + 48;
        len--;                 /* The number left */
        if (expon < 0)
        {
            for (i = 0; i < len ;i++)
            {
                r = 101 - (*rx++);
                *x1++ = high[r];
                *x1++ = low[r];
            }
            if (*(x1 - 1) == '0')
               x1--;
        }
        else
        {
            for (i = 0; i < expon && i < len ;i++)
            {
                r = 101 - (*rx++);
                *x1++ = high[r];
                *x1++ = low[r];
            }
            if (expon < len)
            {
                *x1++ = '.';
                for (; i < len ;i++)
                {
                    r = 101 - (*rx++) ;
                    *x1++ = high[r];
                    *x1++ = low[r];
                }
                if (*(x1 - 1) == '0')
                   x1--;
            }
            else
            for (; i < expon ;i++)
            {
                *x1++ = '0';
                *x1++ = '0';
            }
        }
    }
    *x1 = '\0';
    return (short int) (x1 - (char *) out);
}
/*
 * Render a character strings as an ORACLE-internal number
 *
 * The input is a numeric string. Invalid or empty strings return 0.
 *
 * The output is a 1 byte length, which is followed
 * by the exponent byte and the mantissa.
 */
int retora_num(out, in, out_len)
unsigned char *out;
unsigned char *in;
int out_len;
{
int bef_dec = 0;
int neg = 0;
int after_dec = 0;
unsigned char * dec = (char *) NULL;
unsigned char * num = (char *) NULL;
unsigned char * top = out + out_len - 2;
register unsigned char *rx = in;
register unsigned char *ry;
int expon;
char buf[2];
/*
 * Skip leading white space
 */
    for (rx = in; isspace(*rx) ; rx++);
/*
 * First check for possible negative number
 */
    if (*rx == '-')
    {
        neg = 1;
        rx++;
    }
/*
 * Skip leading zeroes
 */
    while(*rx == '0')
        rx++;
    if (isdigit(*rx))
    {
        num = rx;
        {
            bef_dec++;
            rx++;
        }
        while (isdigit(*rx));
    }
    if (*rx == '.')
    {
        rx++;
        dec = rx; 
        while(*rx == '0')
            rx++;
        if (isdigit(*rx))
        {
            do
            {
                rx++;
            }
            while (isdigit(*rx));
            after_dec = rx - dec;
        }
    }
/*
 * Zero - return this for things that are not valid numbers also
 */
    if (bef_dec == 0 && after_dec == 0)
    {
        *out = (unsigned char) 0x80;
        return 1;
    }
/*
 * Compute the exponent
 */
    if (bef_dec > 0)
        expon = bef_dec - 1;
    else
        expon = -after_dec;
/*
 * ORACLE Number - if negative, extra processing.
 */
    ry = out + 1;
    *out = (unsigned char) (expon + 0xc1);
    if (neg)
        *out = (~(*out)) & 0xff; 
    if (num != (unsigned char *) NULL)
    {
        if (bef_dec & 1)
        {
            buf[0] = '0';
            buf[1] = *num;
            bef_dec--;
            dec2byte(ry, &buf[0]);
            if (neg)
                *ry = 101 - *ry; 
            ry++;
            num++;
        }
        while (bef_dec > 0 && ry < top)
        {
            dec2byte(ry, num);
            if (neg)
                *ry = 101 - *ry; 
            ry++;
            num += 2;
            bef_dec -= 2;
        }
    }
    if (dec != (unsigned char *) NULL)
    {
        while (after_dec > 1 && ry < top)
        {
            dec2byte(ry, dec);
            if (neg)
                *ry = 101 - *ry; 
            ry++;
            dec += 2;
            after_dec -= 2;
        }
        if (after_dec)
        {
            buf[0] = *dec;
            buf[1] = '0';
            dec2byte(ry, &buf[0]);
            if (neg)
                *ry = 101 - *ry; 
            ry++;
        }
    }
    return ry - out;
}
/*****************************************************************
 * Expand a modulus 100 representation of a number, most significant half-byte
 * first.
 */
void ordate(t,f)
unsigned char * t;
unsigned char * f;
{
int i;

    if ((int) *(f+2) < 1 || (int) *(f+2) > 12)
    {
        i = 7;
        strcpy(t,"Wrong:");
        for (t = t +6 ; i; i--, f++)
        {
            *t = (((*f) >> 4) & 0xf) + 48;
            if (*t > 58)
               *t += 7;
            t++;
            *t = (((*f)) & 0xf) + 48;
            if (*t > 58)
               *t += 7;
            t++;
        }
        return;
    }
    *t++ = high[*(f+3)];
    *t++ = low[*(f+3)];
    *t++ = ' ';
    memcpy(t, months[(int) *(f+2) - 1],3);                       /* Month */
    t += 3;
    *t++ = ' ';
    i = ((int) (*f)) - 100;
    *t++ = high[i];
    *t++ = low[i];
    f++;
    i = ((int) (*f)) - 100;
    if (i < 0)
        i += 256;
    *t++ = high[i];
    *t++ = low[i];
    *t++ = ' ';
    f += 3;
    i = ((int) (*f)) - 1;
    *t++ = high[i];
    *t++ = low[i];
    *t++ = ':';
    f++;
    i = *f - 1;
    *t++ = high[i];
    *t++ = low[i];
    *t++ = ':';
    f++;
    i = *f - 1;
    *t++ = high[i];
    *t++ = low[i];
    *t = '\0';
    return;
}
/*
 * Bring in a pair of decimal bytes, and output a binary byte.
 */
static void dec2byte(out, in)
unsigned char * out;
unsigned char * in;
{
register char tmp;
/*
 * Build up half-byte at a time, subtracting 48 initially, and subtracting
 */
    tmp = *in - (unsigned char) 48;
    *out = (unsigned char) ((int ) tmp) * 10;
    in++;
    tmp = *in - (unsigned char) 48;
    *out += tmp;
    return;
}
/*****************************************************************
 * Return a pointer to a 7 byte ORACLE date, given seconds since 1970.
 */
unsigned char * retordate_r(f, ret_val)
double f;
unsigned char * ret_val;
{
char hex_val[15];
char * i, *o;

    strcpy(&hex_val[0], to_char("yyyymmddhh24miss",f));
    for (i = &hex_val[0], o = &ret_val[0]; *i != '\0'; i = i + 2, o++)
         dec2byte(o,i);
    ret_val[6]++;
    ret_val[5]++;
    ret_val[4]++;
    ret_val[0] += 100;
    ret_val[1] += 100;
    if (ret_val[1] < 0)
        ret_val[1] += 256;
    return &ret_val[0];
}
/*****************************************************************
 * Return a pointer to a 7 byte ORACLE date, given seconds since 1970.
 */
unsigned char * retordate(f)
double f;
{
static unsigned char ret_val[7];

    (void) retordate_r(f, ret_val);
}
/*
 * Recognise a counted integer
 */
unsigned int counted_int(p, unsigned_flag)
unsigned char * p;
int unsigned_flag;
{
int i, j;
unsigned int u;

    if (*p > 8)
        return 0;
    if (unsigned_flag)
        i = 0;
    else
    if (*(p+1) & 0x80)
        i = -1;
    else
        i = 1;
    for (j = (*p++), u = 0; j; j--,p++)
        u = (u << 8) + ((unsigned int) *p);
/*
 * Make sure that negative numbers are sign-extended
 */
    if (i == -1 && !(u & 0xff000000))
    {
        u |= 0xff000000;
        if (!(u & 0x00ff0000))
        {
            u |= 0x00ff0000;
            if (!(u & 0x0000ff00))
                u |= 0x0000ff00;
        }
    }
    return u;
}
static int ebcdic;
void set_ebcdic_flag()
{
    ebcdic = 1;
}
void clear_ebcdic_flag()
{
    ebcdic = 0;
}
/**************************************************************************
 * Output clear text when we encounter it, otherwise hexadecimal.
 */
unsigned char * gen_handle_no_uni_nolf(ofp, p, top, write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
    while ((p = bin_handle_no_uni(ofp, p,top,write_flag)) < top) 
        p = asc_handle(ofp, p,top,write_flag);
    return top;
}
/**************************************************************************
 * Output clear text when we encounter it, otherwise hexadecimal.
 */
unsigned char * gen_handle_no_uni(ofp, p, top, write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
    top = gen_handle_no_uni_nolf(ofp, p,top,write_flag);
    if (write_flag)
    {
        if (p < top && *(top - 1) == '\\')
        {
             fputc((int) '\\', ofp);
             fputc((int) '\n', ofp);
        }
        fputc((int) '\n', ofp);
    }
    return top;
}
/**************************************************************************
 * Output non-clear text as blocks of Hexadecimal.
 */
unsigned char * bin_handle_no_uni(ofp, p,top,write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
unsigned char tran;
unsigned     char *la;

    for (la = p; la < top; la++)
    {
        tran = asc_ind(*la);
        if ((tran == (unsigned char) '\t'
             || tran == (unsigned char) '\n'
             || tran ==  (unsigned char) '\r'
             || (tran > (unsigned char) 31 && tran < (unsigned char) 127))
             && ((asc_handle(ofp, la, top, 0) - la) > 3))
            break;
    }
    if (write_flag && (la - p))
        hex_line_out(ofp, p,la);
    return la;
}
/**************************************************************************
 * Output non-clear text as blocks of Hexadecimal.
 */
unsigned char * bin_handle_no_uni_cr(ofp, p,top,write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
unsigned char tran;
unsigned     char *la;

    for (la = p; la < top; la++)
    {
        tran = asc_ind(*la);
        if (( tran == (unsigned char) '\n'
             || (tran > (unsigned char) 31 && tran < (unsigned char) 127))
             && ((asc_handle_nocr(ofp, la, top, 0) - la) > 3))
            break;
    }
    if (write_flag && (la - p))
        hex_line_out(ofp, p,la);
    return la;
}
/**************************************************************************
 * Output Hexadecimal in Lines.
 */
void hex_line_out(ofp, b, top)
FILE *ofp;
unsigned char *b;
unsigned char * top;
{
int olen = 30;
int len = (top - b);
register unsigned char * x = b;

    do 
    {
        if (olen > len)
            olen = len;
        hex_out(ofp, x, x + olen);
        x += olen;
        fputc((int) '\\', ofp); 
        fputc((int) '\n', ofp); 
        len -= olen;
    }
    while (len > 0);
    return;
}
/**************************************************************************
 * Output Hexadecimal. Flag them with a leading and trailing single quote
 */
void hex_out(ofp, b,top)
FILE * ofp;
unsigned char *b;
unsigned char * top;
{
int len = (top - b);
register unsigned char * x = b;
register int i;

    fputc((int) '\'', ofp); 
    for (i = len; i > 0; i--, x++)
    { 
        fputc((int) "0123456789ABCDEF"[(((int) *x) & 0xf0) >> 4] , ofp); 
        fputc((int) "0123456789ABCDEF"[(((int) *x) & 0xf)] , ofp); 
    }
    fputc((int) '\'', ofp); 
    return;
}
/*
 * Bring in hexadecimal strings, and output a stream of bytes.
 * This should be able to update in place.
 */
unsigned char * hex_in_out( out, in)
unsigned char * out;
unsigned char * in;
{
register unsigned char * x = out,  * x1 = in;
/*
 * Build up half-byte at a time, subtracting 48 initially, and subtracting
 * another 7 (to get the range from A-F) if > (char) 9;
 */
    while (*x1 != '\0')
    {
        register char x2;
        x2 = *x1 - (char) 48;
        if (x2 > (char) 9)
           x2 -= (char) 7; 
        if (x2 > (char) 15)
           x2 -= (char) 32;    /* Handle lower case */
        *x = (unsigned char) (((int ) x2) << 4);
        x1++;
        if (*x1 == '\0')
            break;
        x2 = *x1++ - (char) 48;
        if (x2 > (char) 9)
           x2 -= (char) 7; 
        if (x2 > (char) 15)
           x2 -= (char) 32;    /* Handle lower case */
        *x++ |= x2;
    }
    return x;
}
/**************************************************************************
 * Output clear text when we encounter it, otherwise hexadecimal.
 */
unsigned char * gen_handle_nolf(ofp, p, top, write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
    while ((p = bin_handle(ofp, p,top,write_flag)) < top) 
    {
        if ((uni_handle(ofp, p, top, 0) - p) > 3)
            p = uni_handle(ofp, p,top,write_flag);
        else
            p = asc_handle(ofp, p,top,write_flag);
    }
    return top;
}
/**************************************************************************
 * Output clear text when we encounter it, otherwise hexadecimal.
 */
unsigned char * gen_handle(ofp, p, top, write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
    gen_handle_nolf(ofp, p,top,write_flag);
    if (write_flag)
        fputc((int) '\n', ofp);
    return top;
}
/**************************************************************************
 * Match clear text independent of ascii or ebcdic-ness
 */
unsigned char asc_ind(la)
unsigned char la;
{
    if (ebcdic)
        return ebc_to_asc[la];
    else
        return la;
}
/******************************************************************************
 * Validate a sequence of characters for hex-ness
 */
static int ishex(p, len)
unsigned char * p;
int len;
{
    if (len <= 0)
        return 0;
    else
    for (;len > 0; len--, p++)
        if (!isxdigit(*p))
            return 0;
    return 1;
}
/******************************************************************************
 * Routine to make sure that we do not write out something that will be
 * miss-read by driver programs because it has hexadecimal strings in it.
 *
 * Additionally, track the offset, so that the line breaks look neater.
 */
static void help_re_read(p, olen, ofp)
unsigned char * p;
int olen;
FILE * ofp;
{
int line_pos;
unsigned char * x;
unsigned char * bpos;
unsigned char * xpos;
unsigned char * top = p + olen;

    if (ebcdic)
        for (x = p; x < top ; x++)
            *x = asc_ind(*x);
    for (x = p, line_pos = 0, bpos = NULL; x < top; )
    {
        if (*x == '\n')
        {
            if (p != x && *(x - 1) == '\\')
            {
/*
 * We have an apparently-escaped line feed that will be mis-read. Ensure that
 * it can't be.
 */
                fwrite(p, sizeof(char), (x - p), ofp);
                fputs("'0A'\\\n", ofp);
            }
            else
                fwrite(p, sizeof(char), (x - p) + 1, ofp);
            line_pos = 0;
            x++;
            p = x;
            bpos = NULL;
            continue;
        }
        else
        if (*x == '\''
          && ((xpos = memchr(x + 1, '\'', (top - x -1))) != NULL || (xpos = top))
          && (xpos - x) > 2
          && ((xpos - x) & 1) == 1
          && (xpos == top || ishex(xpos + 1, (xpos - x) - 1)))
        {
/*
 * We have a length of hexadecimal that will be mis-read. Ensure that
 * it can't be.
 */
            fwrite(p, sizeof(char), xpos - p, ofp);
            fputs("\\\n", ofp);
            line_pos = 0;
            x = xpos;
            p = x;
            bpos = NULL;
            continue;
        }
        else
        if (isspace(*x) || ispunct(*x))
            bpos = x;
        if (line_pos >= E2_OLEN)
        {
            if (bpos != NULL)
            {
                fwrite(p, sizeof(char), (bpos - p) + 1, ofp);
                fputs("\\\n", ofp);
                p = bpos + 1;
                if (p > x)
                    x = p;
                line_pos = x - p;
                bpos = NULL;
            }
            else
            {
                fwrite(p, sizeof(char), (x - p) + 1, ofp);
                fputs("\\\n", ofp);
                line_pos = 0;
                p = x + 1;
                x = p;
            }
        }
        else
        {
            line_pos++;
            x++;
        }
    }
    if (p < top)
        fwrite(p, sizeof(char), (top - p), ofp);
    return;
}
/**************************************************************************
 * Output clear text when we encounter it.
 */
unsigned char * asc_handle(ofp, p, top, write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
unsigned char *la;
unsigned char tran;

    for (la = p; la < top; la++)
    {
        tran = asc_ind(*la);
        if (!(tran == (unsigned char) '\t'
             || tran == (unsigned char) '\n'
             || tran ==  (unsigned char) '\r'
             || tran ==  (unsigned char) '\f'
             || (tran > (unsigned char) 31 && tran < (unsigned char) 127)))
            break;
    }
    if (write_flag && (la - p))
        help_re_read(p, la - p, ofp);
    return la;
}
/**************************************************************************
 * Output clear text when we encounter it, excluding characters that give
 * problems with browser-based editors
 */
unsigned char * asc_handle_nocr(ofp, p, top, write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
unsigned char *la;
unsigned char tran;

    for (la = p; la < top; la++)
    {
        tran = asc_ind(*la);
        if (!(tran == (unsigned char) '\n'
             || (tran > (unsigned char) 31 && tran < (unsigned char) 127)))
            break;
    }
    if (write_flag && (la - p))
        help_re_read(p, la - p, ofp);
    return la;
}
/**************************************************************************
 * Output clear text when we encounter it.
 */
unsigned char * uni_handle(ofp, p, top, write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
unsigned char *la;
unsigned char tran;

    for (la = p; la < top; la++)
    {
        tran = asc_ind(*la);
        if (!(tran == (unsigned char) '\t'
             || tran == (unsigned char) '\n'
             || tran ==  (unsigned char) '\r'
             || tran ==  (unsigned char) '\f'
             || (tran > (unsigned char) 31 && tran < (unsigned char) 127)))
            break;
        la++;
        if (*la != '\0')
        {
            la--;
            break;
        }
    }
    if (write_flag && (la - p))
    {
    int len=(la -p)/2;
    int olen = E2_OLEN;
    char buf[E2_OLEN + 1];
    int i;
    unsigned char *p1, *p2;

        fwrite("\"U\"",sizeof(char), 3,ofp);
        while (len)
        {
            if (olen > len)
                olen = len;
            for (i = olen, p1 = p, p2 = buf; i ; i--, p2++, p1 += 2)
            {
                if (ebcdic)
                    *p2 = asc_ind(*p1);
                else
                    *p2 = *p1;
            }
            fwrite(buf,sizeof(char), olen,ofp);
            len -= olen;
            p += olen * 2;
        }
    }
    return la;
}
/**************************************************************************
 * Output non-clear text as blocks of Hexadecimal.
 */
unsigned char * bin_handle(ofp, p,top,write_flag)
FILE *ofp;
unsigned char *p;
unsigned char *top;
int write_flag;
{
unsigned char tran;
unsigned     char *la;

    for (la = p; la < top; la++)
    {
        tran = asc_ind(*la);
        if ((tran == (unsigned char) '\t'
             || tran == (unsigned char) '\n'
             || tran ==  (unsigned char) '\r'
             || (tran > (unsigned char) 31 && tran < (unsigned char) 127))
             && (((asc_handle(ofp, la, top, 0) - la) > 3)
              ||((uni_handle(ofp, la, top, 0) - la) > 3)))
            break;
    }
    if (write_flag && (la - p))
        hex_line_out(ofp, p,la);
    return la;
}
/*
 * Get rid of the XML escape characters
 * " = &quot;
 * ' = &apos;
 * & = &amp;
 * < = &lt;
 * > = &gt;
 * Do nothing if it doesn't appear to have escapes, or if there are
 * illegal ones.
 */
int xml_unescape(pin, in_len)
unsigned char * pin;
int in_len;
{
unsigned char * base = pin;
unsigned char * bound = pin + in_len;
unsigned char * outp;

    while(pin < bound - 5 && *pin != '&')
        pin++; 
    if (pin < bound - 3)
    {
        outp = pin;
        while (pin < bound - 3)
        {
            if (*pin == '&')
            {
                pin++;
                switch(*pin)
                {
                case 'a':
                    pin++;
                    if (pin < bound -2 
                       && *pin == 'm' && *(pin+1) == 'p' && *(pin + 2) == ';')
                    {
                        *outp++ = '&';
                        pin += 3;
                    }
                    else
                    if (pin < bound - 3
                        && *pin == 'p' && *(pin+1) == 'o'
                        && *(pin + 2) == 's' && *(pin + 3) == ';')
                    {
                        *outp++ = '\'';
                        pin += 4;
                    }
                    else
                    {
                       pin -= 2;
                       *outp++ = *pin++;
                    }
                    break;
                case 'g':
                    pin++;
                    if (pin < bound - 1
                     && *pin == 't' && *(pin+1) == ';')
                    {
                        *outp++ = '>';
                        pin += 2;
                    }
                    else
                    {
                       pin -= 2;
                       *outp++ = *pin++;
                    }
                    break;
                case 'l':
                    pin++;
                    if (pin < bound - 1
                     && *pin == 't' && *(pin+1) == ';')
                    {
                        *outp++ = '<';
                        pin += 2;
                    }
                    else
                    {
                       pin -= 2;
                       *outp++ = *pin++;
                    }
                    break;
                case 'q':
                    pin++;
                    if (pin < bound - 3
                     && *pin == 'u' && *(pin+1) == 'o'
                        && *(pin + 2) == 't' && *(pin + 3) == ';')
                    {
                        *outp++ = '"';
                        pin += 4;
                    }
                    else
                    {
                       pin -= 2;
                       *outp++ = *pin++;
                    }
                    break;
                default:
                    pin--;
                    *outp++ = *pin++;
                    break;
                }
            }
            else
                *outp++ = *pin++;
        }
        while (pin < bound)
            *outp++ = *pin++;
        in_len = outp - base;
    }
    return in_len;
}
static __inline unsigned char hexdig(hp)
unsigned char * hp;
{
unsigned char xh;
unsigned char xl;

    xh = *hp++ - (char) 48;
    if (xh > (unsigned char) 48)
       xh -= (unsigned char) 32;    /* Handle lower case */
    if (xh > (char) 9)
       xh -= (char) 7; 
    xh = (unsigned char) (xh << 4);
    xl = *hp - (char) 48;
    if (xl > (unsigned char) 48)
       xl -= (unsigned char) 32;    /* Handle lower case */
    if (xl > (char) 9)
       xl -= (char) 7; 
    return xh | xl;
}
/*
 * BEWARE: This table needs to be kept in ascending order of name, as per
 * LC_ALL=C. Make sure that any additional values are inserted in the right
 * place.
 *
 * This table only includes the codes with a single byte rendition. Multi-byte
 * escapes are therefore passed through unchanged.
 */
static struct esctab {
    int cd;
    int len;
    char * esc;
} esctab [] = {
{198, 5, "AElig"},
{193, 6, "Aacute"},
{194, 5, "Acirc"},
{192, 6, "Agrave"},
{197, 5, "Aring"},
{195, 6, "Atilde"},
{196, 4, "Auml"},
{199, 6, "Ccedil"},
{208, 3, "ETH"},
{201, 6, "Eacute"},
{202, 5, "Ecirc"},
{200, 6, "Egrave"},
{203, 4, "Euml"},
{205, 6, "Iacute"},
{206, 5, "Icirc"},
{204, 6, "Igrave"},
{207, 4, "Iuml"},
{209, 6, "Ntilde"},
{211, 6, "Oacute"},
{212, 5, "Ocirc"},
{210, 6, "Ograve"},
{216, 6, "Oslash"},
{213, 6, "Otilde"},
{214, 4, "Ouml"},
{222, 5, "THORN"},
{218, 6, "Uacute"},
{219, 5, "Ucirc"},
{217, 6, "Ugrave"},
{220, 4, "Uuml"},
{221, 6, "Yacute"},
{225, 6, "aacute"},
{226, 5, "acirc"},
{180, 5, "acute"},
{230, 5, "aelig"},
{224, 6, "agrave"},
{38, 3, "amp"},
{229, 5, "aring"},
{227, 6, "atilde"},
{228, 4, "auml"},
{166, 6, "brvbar"},
{231, 6, "ccedil"},
{184, 5, "cedil"},
{162, 4, "cent"},
{169, 4, "copy"},
{164, 6, "curren"},
{176, 3, "deg"},
{247, 6, "divide"},
{233, 6, "eacute"},
{234, 5, "ecirc"},
{232, 6, "egrave"},
{240, 3, "eth"},
{235, 4, "euml"},
{189, 6, "frac12"},
{188, 6, "frac14"},
{190, 6, "frac34"},
{62, 2, "gt"},
{237, 6, "iacute"},
{238, 5, "icirc"},
{161, 5, "iexcl"},
{236, 6, "igrave"},
{191, 6, "iquest"},
{239, 4, "iuml"},
{60, 2, "lt"},
{175, 4, "macr"},
{181, 5, "micro"},
{183, 6, "middot"},
{32, 4, "nbsp"},
{172, 3, "not"},
{241, 6, "ntilde"},
{243, 6, "oacute"},
{244, 5, "ocirc"},
{242, 6, "ograve"},
{170, 4, "ordf"},
{186, 4, "ordm"},
{248, 6, "oslash"},
{245, 6, "otilde"},
{246, 4, "ouml"},
{182, 4, "para"},
{177, 6, "plusmn"},
{163, 5, "pound"},
{34, 4, "quot"},
{187, 5, "raquo"},
{174, 3, "reg"},
{167, 4, "sect"},
{173, 3, "shy"},
{185, 4, "sup1"},
{178, 4, "sup2"},
{179, 4, "sup3"},
{223, 5, "szlig"},
{254, 5, "thorn"},
{215, 5, "times"},
{250, 6, "uacute"},
{251, 5, "ucirc"},
{249, 6, "ugrave"},
{168, 3, "uml"},
{252, 4, "uuml"},
{253, 6, "yacute"},
{165, 3, "yen"},
};
/*
 * Binary search for match in esctab
 */
static struct esctab* find_esc(match_word, match_len)
unsigned char * match_word;
int match_len;
{
struct esctab * low = &esctab[0];
struct esctab * high = &esctab[98];
struct esctab *guess;
register unsigned char*x1_ptr, *x2_ptr, *t_ptr;
int test_len;

    while (low <= high)
    {
        guess = low + ((high - low) >> 1);
        test_len = guess->len;
        if (test_len > match_len)
            test_len = match_len;
        for (x1_ptr = guess->esc,
             t_ptr = x1_ptr + test_len,
             x2_ptr = match_word;
                   (x1_ptr < t_ptr) && (*x1_ptr == *x2_ptr);
                         x1_ptr++,
                         x2_ptr++);
        if (x1_ptr >= t_ptr)
        {
            if ( test_len == match_len)
                return guess;
            else
                low = guess + 1;
        }
        else
        if (*x1_ptr > *x2_ptr)
            high = guess - 1;
        else
            low = guess + 1;
    }
    return (struct esctab *) NULL;
}
/*
 * This doesn't work with 2 byte chars.
 */
static int look_for_esc(pin, bound)
unsigned char * pin;
unsigned char * bound;
{
int len;
int chr;
struct esctab * etp;

    if ((len = memcspn(pin, bound, 1, ";")) == (bound - pin)
     || len < 2)
        return 0;
    if (*(pin + 1) == '#')
    {
        if (memspn(pin+2, pin + len, 10, "0123456789") < len -2
         || ((chr = atoi(pin+2)) < 32 || chr > 255)) 
            return 0;
        *pin = (unsigned char) chr;
        return len + 1;
    }
    else
    if ((etp = find_esc(pin + 1, len - 2)) == NULL)
        return 0;
    else
    {
        *pin = etp->cd;
        return len + 1;
    }
}
/*
 * Unescape a variable picked off a GET (or from a POST)
 */
int url_unescape(pin, in_len)
unsigned char * pin;
int in_len;
{
unsigned char * base = pin;
unsigned char * bound = pin + in_len;
unsigned char * outp;
int skip_len;

    while(pin < bound - 2 && *pin != '%' && *pin != '&')
        pin++; 
    if (pin < bound - 2)
    {
        outp = pin;
        while (pin < bound - 2)
        {
            if (*pin == '%')
            {
                pin++;
                *outp++ = hexdig(pin);
                pin += 2;
            }
            else
            if (*pin == '&'
             && (skip_len = look_for_esc(pin, bound)) != 0)
            {
                if (outp != pin)
                    *outp++ = *pin;
                else
                    outp++;
                pin += skip_len;
            }
            else
                *outp++ = *pin++;
        }
        while (pin < bound)
            *outp++ = *pin++;
        in_len = outp - base;
    }
    return in_len;
}
/*********************************************************************
 * Escape a string as would be needed within a URL
 *
 * pout      -> the escaped output string
 * pin       -> the unescaped input string
 * in_len    -> the input length
 * plus_flag -> whether or not space should change to a plus sign
 *
 * returns the length of the output
 *
 * The output space needs to be at least 3 times the size of the input.
 */
int url_escape(pout, pin, in_len, plus_flag)
unsigned char * pout;
unsigned char * pin;
int in_len;
int plus_flag;
{
register unsigned char * rpi = pin;
register unsigned char * rpo = pout;

/*
 * Not sure if this should be universal or what
 */
    in_len = xml_unescape(pin, in_len);
    while (in_len > 0)
    {
        if (*rpi > 0x7e
         || *rpi < 0x20
         || strchr(";/?:@&=+$, <>#%\"{}|\\^[]`", *rpi))
        {
            if (*rpi == 0x20 && plus_flag)
                *rpo++ = '+';
            else
            {
                *rpo++ = '%';
                *rpo++ = "0123456789ABCDEF"[(*rpi & 0xf0) >> 4];
                *rpo++ = "0123456789ABCDEF"[*rpi & 0xf];
            }
            rpi++;
        }
        else
            *rpo++ = *rpi++;
        in_len--;
    }
    return (rpo - pout);
}
/*
 * Convert a MSB/LSB ordered hexadecimal string into an integer
 */
int hex_to_int(x1, len)
char * x1;
int len;
{
long x;
int x2;

    for (x = (unsigned long) (*x1++ - (char) 48),
         x = (x > 9)?(x - 7):x, len -= 1;
             len;
                 len--, x1++)
    {
        x2 = *x1 - (char) 48;
        if (x2 >  9)
            x2 -=  7;
        x = x*16 + x2;
    }
    return x;
}
/*
 * Code to generate binary from a mixed buffer of ASCII and hexadecimal
 * Note that this is heuristic, not deterministic. We rely on the construction
 * of the input to have replaced things like '10306070' with '\
 * 10306070' so that we don't have ambiguous input. Thus, numeric and valid
 * hexadecimal strings are handled correctly.
 *
 * This is able to update in place.
 *
 * Dreadful. The logic depends on the thing being null-terminated, so we make
 * it null-terminated, but preserve whatever is in the null position in case
 * we might further process it. 
 */ 
int get_bin(tbuf, tlook, cnt)
unsigned char * tbuf;
unsigned char * tlook;
int cnt;
{
unsigned char * cur_pos;
unsigned char * sav_tbuf = tbuf;
int len;
unsigned char * bound = tlook + cnt;
unsigned char sav_char;

    sav_char = *bound;
    *bound = '\0';
    while (cnt > 0)
/*
 * Is this a length of hexadecimal?
 */
    {
        if (*tlook == '\''
          && (cur_pos = strchr(tlook+1,'\'')) > (tlook + 1)
          && (((cur_pos - tlook) % 2) ==  1)
          && strspn(tlook+1,"0123456789ABCDEFabcdef") ==
                          (len = (cur_pos - tlook - 1)))
        {
            cnt -= (2 + len);
            tlook++;
            *(tlook + len) = (unsigned char) 0;
            tbuf = hex_in_out(tbuf, tlook);
            tlook = cur_pos + 1;
        }
/*
 * Is this a run of characters?
 */
        else
        if ((len = strcspn(tlook,"'")))
        {
            if (tbuf != tlook)
                memmove(tbuf,tlook,len);
            tlook += len;
            tbuf += len;
            cnt -= len;
        }
/*
 * Otherwise, we have a stray "'"
 */
        else
        {
            if (tbuf != tlook)
                *tbuf = *tlook;
            tbuf++;
            tlook++;
            cnt--;
        }
    }
    *bound = sav_char;
    return tbuf - sav_tbuf;
}
/*********************************************************************
 * Base64 encode and decode
 *********************************************************************
 * Lookup table that translates 6-bit values index values into
 * 'Base64 Alphabet' equivalents as per Table 1 of RFC 2045.
 */
static unsigned char c2b64[] = {
'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};
/*
 * Reverse lookup table. Characters that fall within the valid range but which
 * aren't drawn from the 'Base64 Alphabet' are translated to 127.
 */
static unsigned char b642c[] = {
62, 127, 127, 127, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 127,
127, 127, 127, 127, 127, 127, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
127, 127, 127, 127, 127, 127, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
    };
/*
 * Encode a length
 */
int b64enc(len, ip, op)
int len;
unsigned char * ip;
unsigned char * op;
{
unsigned char * top = ip + len;
unsigned char * base = op;

   while (ip < top)
   {
      *op++ = c2b64[(*ip >> 2)];
      *op = ((*ip++ & 0x3) << 4);
      if (ip >= top)
      {
          *op++ = c2b64[*op];
          break;
      }
      *op |= (*ip >> 4);
      *op++ = c2b64[*op];
      *op = ((*ip++ & 0xf) << 2);
      if (ip >= top)
      {
          *op++ = c2b64[*op];
          break;
      }
      *op |= (*ip & 0xc0) >> 6;
      *op++ = c2b64[*op];
      *op++ = c2b64[(*ip++ & 0x3f)];
   }
   return op - base;
}
#define b64invalid(bp) ((*(bp)<0x2b)||(*(bp)>=sizeof(b642c)+0x2b)||(b642c[*(bp)-0x2b]==127))
/*
 * Decode one line
 */
int b64dec(len, ip, op)
int len;
unsigned char * ip;
unsigned char * op;
{
unsigned char * top = ip + len;
unsigned char * base = op;

    while (ip < top)
    {
        if (b64invalid(ip))
        {
            ip++;
            continue;
        }
        *op = b642c[*ip++ - 0x2b] << 2;
        if (ip >= top)
        {
            op++;
            break;
        }
        if (b64invalid(ip))
        {
            op++;
            ip++;
            continue;
        }
        *op++ |= (b642c[*ip - 0x2b] & 0x30) >> 4;
        *op = ((b642c[*ip++ - 0x2b] & 0x0f) << 4);
        if (ip >= top)
        {
            op++;
            break;
        }
        if (b64invalid(ip))
        {
            op++;
            ip++;
            continue;
        }
        *op++ |= (b642c[*ip - 0x2b] & 0x3c) >> 2;
        *op = ((b642c[*ip++ - 0x2b] & 0x03) << 6);
        if (ip >= top)
        {
            op++;
            break;
        }
        if (b64invalid(ip))
        {
            op++;
            ip++;
            continue;
        }
        *op++ |= (b642c[*ip++ - 0x2b] & 0x3f);
    }
    return (op - base);
}
/******************************************************************************
 * Reverse bytes in situ
 */
void other_end(inp, cnt)
unsigned char * inp;
unsigned int cnt;
{
char * outp = inp + cnt -1;

     for (cnt = cnt/2; cnt > 0; cnt--)
     {
         *outp = *outp ^ *inp;
         *inp = *inp ^ *outp;
         *outp = *inp ^ *outp;
         outp--;
         inp++;
     }
     return; 
}
