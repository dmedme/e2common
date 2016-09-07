/************************************************************************
 * unquote.c - Convert a file with escaped things into a plain separated
 * files, or vice versa.
 */
static char * sccs_id =  "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
#include <stdio.h>
#include <stdlib.h>
#ifndef LCC
#include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "e2conv.h"

#ifndef LINUX
char * strdup();
#endif
extern int optind;
extern char * optarg;
int strcasecmp();
enum look_status {CLEAR, PRESENT};
enum tok_id {SQL, COMMAND, FIELD, FNUMBER, FRAW, FLONG, FDATE, NOTNUM, EOR, PEOF};
static int sql_flag;
static char * tbuf;
static char * tlook;
int tlen;
enum tok_id look_tok;
enum look_status look_status;
int char_long;
char term_char;
static char * work_directory;
static char * id;
#define WORKSPACE 65536
static void scarper();
/*****************************************************************************
 * awk-inspired input line processing
 *
 * Read in a line and return an array of pointers
 */
struct in_rec {
   int fcnt;
   char buf[16384];
   char * fptr[1024];
};
static struct in_rec in_rec;
static struct in_rec * cur_rec;
static char * FS_ro = "\n\r\t ";
static char * FS;
/*
 * awk-like input line read routine. If the caller free()s the strdup()'ed
 * line buffer, the pointer to it must be set to NULL. 
 *
 * If the separator is only one character, consecutive delimiters delineate
 * multiple null fields.
 */
static struct in_rec * get_next(in_rec, fp)
struct in_rec * in_rec;
FILE * fp;
{
int i, l;

    if (fgets(in_rec->buf,sizeof(in_rec->buf), fp) == (char *) NULL)
        return (struct in_rec *) NULL;
    in_rec->fcnt = 0;
    if (in_rec->fptr[0] != (char *) NULL)
        free(in_rec->fptr[0]);
    in_rec->fptr[0] = strdup(in_rec->buf);
    if (in_rec->fptr[0][0] == '\0')
        return in_rec;
    if (FS[1] == '\0')
    {
        in_rec->fptr[1] = strtok(in_rec->buf,FS);
        if (in_rec->fptr[1] == (char *) NULL)
            return in_rec;
        if (strlen(in_rec->fptr[1]) == 0)
            i = 1;
        else
            i = 2;
        while ((in_rec->fptr[i] = strtok(NULL, FS)) != (char *) NULL)
            i++;
    }
    else
    {
        in_rec->fptr[1] = in_rec->buf;
        for (i = 1; in_rec->fptr[i][0] != '\0';)
        {
            l = strcspn(in_rec->fptr[i],FS);
            in_rec->fptr[i + 1] = in_rec->fptr[i] + l;
            i++;
            if (*(in_rec->fptr[i]) != '\0')
            {
                *(in_rec->fptr[i]) = '\0';
                in_rec->fptr[i]++;
            }
        }
    }

    in_rec->fcnt = i - 1;
    return in_rec;
}
struct in_rec * get_next_nomult(in_rec, fp)
struct in_rec * in_rec;
FILE * fp;
{
int i, l;
char *x;
unsigned char * got_to;

    if (fgets(in_rec->buf,sizeof(in_rec->buf), fp) == (char *) NULL)
        return (struct in_rec *) NULL;
    in_rec->fcnt = 0;
    if (in_rec->fptr[0] != (char *) NULL)
        free(in_rec->fptr[0]);
    in_rec->fptr[0] = strdup(in_rec->buf);
    if (in_rec->fptr[0][0] == '\0')
        return in_rec;
    if (FS[1] == '\0')
    {
    char * src = strdup(in_rec->buf);

        x = &in_rec->buf[0];
        in_rec->fptr[1] = nextasc_r(src, *FS, 0, &got_to, x,
                 &in_rec->buf[sizeof(in_rec->buf)]);
        if (in_rec->fptr[1] == (char *) NULL)
            return in_rec;
        i = 2;
        x += strlen(x);
        *x = '\0';
        x++;
        while ((in_rec->fptr[i] = nextasc_r(NULL, *FS, 0, &got_to, x,
                 &in_rec->buf[sizeof(in_rec->buf)])) != (char *) NULL)
        {
            x += strlen(x);
            *x = '\0';
            i++;
            x++;
        }
        free(src);
    }
    else
    {
        in_rec->fptr[1] = in_rec->buf;
        for (i = 1; in_rec->fptr[i][0] != '\0';)
        {
            l = strcspn(in_rec->fptr[i],FS);
            in_rec->fptr[i + 1] = in_rec->fptr[i] + l;
            i++;
            if (*(in_rec->fptr[i]) != '\0')
            {
                *(in_rec->fptr[i]) = '\0';
                in_rec->fptr[i]++;
            }
        }
    }
    in_rec->fcnt = i - 1;
    return in_rec;
}
/*******************************************************************
 * Routine to stuff embedded quotes in output strings
 */
void quote_stuff(fp, s)
FILE * fp;
char * s;
{
register char *sp;

    for (sp = s;  *sp != '\0'; sp++)
    {
        putc(*sp, fp);
        if (*sp == '\'')
            putc(*sp, fp);
    }
    return;
}
/**********************************************************************
 * Handle premature EOF
 */
enum tok_id prem_eof(cur_pos, cur_tok)
char * cur_pos;
enum tok_id cur_tok;
{            /* Allow the last string to be unterminated */
    *tlook = '\0';
    *cur_pos = '\0';
    look_status = PRESENT;
    switch (cur_tok)
    {
    case FIELD:
        look_tok = EOR; 
        break;
    case SQL:
    case EOR:
        look_tok = PEOF;
        break;
    default:
        break;
    }
    return cur_tok;
}
/**********************************************************************
 * Unstuff a field into tbuf
 */
static enum tok_id unstuff_field(fp, cur_pos, qtmark)
FILE * fp;
char * cur_pos;
int qtmark;
{
int p;

    for(;;)
    {
        p = getc(fp);
        if (p == EOF)
            return (prem_eof(cur_pos,FIELD));
        else
        if (p == (int) '\r')
            continue;
        else
        if (p == (int) qtmark)
        {            /* Have we come to the end? */
        int pn;

             while((pn = getc(fp)) == '\r');
             if (pn != qtmark)
             {   /* End of String */
                *cur_pos = '\0';
                if (pn != (int) ',')
                {    /* End of record */
                    *tlook = '\0';
                    look_status = PRESENT;
                    look_tok = EOR; 
                }
                return FIELD;
            }
        }
        *cur_pos++ = (char) p;
    }
}
/**********************************************************************
 * Unstuff a field into tbuf
 */
static enum tok_id simple_copy(fp, cur_pos)
FILE * fp;
char * cur_pos;
{
int p;

    for(;;)
    {
        p = getc(fp);
        if (p == EOF)
            return (prem_eof(cur_pos,FIELD));
        else
        if (p == (int) '\r')
            continue;
        if (p == (int) ',' || p == (int) '\n' )
        {            /* Have we come to the end? */
            *cur_pos = '\0';
            if (p != (int) ',')
            {    /* End of record */
                *tlook = '\0';
                look_status = PRESENT;
                look_tok = EOR; 
            }
            return FIELD;
        }
        *cur_pos++ = (char) p;
    }
}
static enum tok_id get_number(fp, cur_pos)
FILE * fp;
char * cur_pos;
{
int p;
char * x;                      /* for where strtod() gets to */
char * save_pos = cur_pos;
double ret_num;
int int_flag;
/*
 * First pass; collect likely numeric characters
 */
    int_flag = 1;
    for (save_pos = cur_pos,
         cur_pos++,
         *cur_pos =  getc(fp);
             *cur_pos == '.' || ( *cur_pos >='0' && *cur_pos <= '9');
                 cur_pos++,
                 *cur_pos = getc(fp))
         if (*cur_pos == '.')
             int_flag = 0;
    p = (int) *cur_pos;
    (void) ungetc(*cur_pos,fp);
    *cur_pos = '\0';
/*
 * Now apply a numeric validation to it
 */
    ret_num = strtod(save_pos, &x);
/*
 * Push back any over-shoot
 */
    while (cur_pos > x)
    {
        cur_pos--;
        (void) ungetc(*cur_pos,fp);
    }
    if ((*cur_pos = (char) getc(fp)) != (int) ',')
    {    /* End of record */
        if (*cur_pos == '\r')
            *cur_pos = (char) getc(fp);
        if (*cur_pos == '\n')
        {
            *tlook = '\0';
            look_status = PRESENT;
            look_tok = EOR; 
        }
        else /* A string starting with a digit */
        {
            cur_pos++;
            return simple_copy(fp, cur_pos);
        }
    }
    *cur_pos = '\0';
#ifdef DEBUG
    (void) fprintf(stderr,"Found NUMBER %s\n", save_pos);
    (void) fflush(stderr);
#endif   
    if (int_flag)
        return FLONG;
    else
        return FNUMBER;
}
/**********************************************************************
 * Read the next token
 */
static enum tok_id get_tok(fp)
FILE * fp;
{
int p;
char * cur_pos;
/*
 * If look-ahead present, return it
 */
#ifdef DEBUG
   fprintf(stdout, "get_tok() called, look_status %d\n", (int) look_status);
   fflush(stdout);
#endif
restart:
    if (look_status == PRESENT)
    {
        strcpy(tbuf,tlook);
        look_status = CLEAR;
        return look_tok;
    }
    else
        cur_pos = tbuf; 
    while ((p = getc(fp)) == (int) '\r'); /* Ignore CR */
/*
 * Scarper if all done
 */
    if ( p == EOF )
        return PEOF;
/*
 * Pick up the next token, stripping off the wrapper for fields.
 */
    else
    {
        switch(p)
        {
        case '-':
        case '.':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':                         /* apparently a number */
            *cur_pos = p;
            return get_number(fp, cur_pos);
        case (int) '\'':
        case (int) '"':
              /* This is a FIELD; search for the end of the string
               * Strings are returned unstuffed, and without wrapping quotes
               */
            return unstuff_field(fp, cur_pos, p);
        case (int) '\n':
            look_status = PRESENT;
            *tlook = '\0';
            look_tok = EOR;
        case (int) ',':
            *cur_pos = '\0';
            return FIELD;
        default:   /* an SQL statement? */
        {      /* Search for \n/\n */
        enum match_state { NOTHING, NEW_LINE, SLASH };
        enum match_state match_state;
        match_state = NEW_LINE;

            if (!sql_flag)
            {
                *cur_pos++ = p;
                return simple_copy(fp, cur_pos);
            }
            else
            for(;;)
            {
                if (p == EOF)
                    return (prem_eof(cur_pos,SQL));
                switch (match_state)
                {
                case NOTHING:
                    if (p == (int) '\n')
                        match_state = NEW_LINE;
                    else
                        *cur_pos++ = (char) p;
                    break;
                case NEW_LINE:
                    if (p == (int) '/')
                        match_state = SLASH;
                    else
                    {
                        *cur_pos++ = (char) '\n';
                        if (p != (int) '\n')
                        {
                            match_state = NOTHING;
                            *cur_pos++ = (char) p;
                        }
                    }
                    break;
                case SLASH:
                    if (p == (int) '\n')
                    {
                        *cur_pos = '\0';
                        return SQL;
                    }
                    else
                    {
                        match_state = NOTHING;
                        *cur_pos++ = (char) '\n';
                        *cur_pos++ = (char) '/';
                        *cur_pos++ = (char) p;
                    }
                    break;
                }
                if (cur_pos > tbuf + WORKSPACE - 4)
                {
                    fprintf(stderr, "WORKSPACE (%d) too small!?\n", WORKSPACE);
                    scarper(__FILE__,__LINE__,"Giving up");
                    return SQL;
                }
                p = getc(fp);
#ifdef DEBUG
                putchar(p);
                fflush(stdout);
#endif
            }
        }
        }
    }
}
/*****************************************************************************
 * Handle unexpected errors
 */
static void scarper(file_name,line,message)
char * file_name;
int line;
char * message;
{
static int recursed;

    if (recursed)
        return;
    else
        recursed = 1;
    (void) fprintf(stderr,"Unexpected Error %s,line %d\n",
                   file_name,line);
    perror(message);
    (void) fprintf(stderr,"UNIX Error Code %d\n", errno);
    recursed = 0;
    return;
}
/****************************************************************************
 * Implement the unquote capability
 */
static void unquote(tab)
char * tab;
{
enum tok_id tok_id;
int i,l;
int col_cnt = 0;
int arr_len = 256;
int fld_ind;
int row_cnt = 0;
char * U;
char * R;
char * fname;
FILE * fpr;
FILE * fpc;
FILE * fpd;
/*
 * Open the input file
 */
    if (!strcmp(tab, "-"))
        fpr = stdin;
    else
    if ((fpr = fopen(tab, "rb")) == (FILE *) NULL)
    {
        (void) fputs(tab,stderr);
        putc(':',stderr);
        scarper(__FILE__,__LINE__,"Failed to open input file");
        return;
    }
/*
 * First pass.
 *************
 * Process the file of statements and data with a simple parser. We are
 * looking to find out how many columns that we need, and for any that are
 * always NULL
 */
    tbuf = malloc(WORKSPACE);
    tlook = malloc(WORKSPACE);
    look_status = CLEAR;
    while ((tok_id = get_tok(fpr)) == SQL);
                  /* Skip any SQL statements at the start */
/*
 * Use the nullability flag to see if we want the column
 */
    fld_ind = 0;
    U = (char *) calloc(arr_len, sizeof(char *));
    while (tok_id != PEOF)
    {
        switch(tok_id)
        {
        case FNUMBER:
        case FIELD:
        case FLONG:
        case FRAW:
        case FDATE:
            l = strlen(tbuf);   /* Field Length */
            if (l > 0)
            {
                U[fld_ind] = 1;
                if (strchr(tbuf, term_char) != (char *) NULL)
                {
                    fprintf(stderr,
                    "Output row %d - field '%s' contains field separator %c\n",
                             row_cnt + 1, tbuf, term_char);
                     scarper(__FILE__, __LINE__,  "Syntax error in file");
                }
            }
            fld_ind++;
            if (fld_ind > col_cnt)
            {
                col_cnt++;
                if (col_cnt >= arr_len)
                {
                    R = (char *) calloc(arr_len + arr_len, sizeof(char *));
                    for (i = 0; i < arr_len; i++)
                         R[i] = U[i];
                    free(U);
                    U = R;
                }
            }
            break;
        case EOR:
            fld_ind = 0;
            row_cnt++;
            break;
        default:
            fprintf(stderr,"Warning at output line:%d - %s - unexpected\n",
                                 row_cnt + 1, tbuf);
            scarper(__FILE__, __LINE__,  "Syntax error in file");
            break;
        }
        tok_id = get_tok(fpr);
    }
    if (!col_cnt)
    {
        fprintf(stderr,"File %s contains no valid data\n", tab);
        scarper(__FILE__, __LINE__,  "Syntax error in file?");
        free(tlook);
        free(tbuf);
        free(U);
        return;
    }
/*
 * Open the Log file: work_directory/id_table.log
 */
    fname = (char *) malloc(strlen(work_directory) +
                            strlen(id) +
                            strlen(tab) + 7);
    if (!strcmp(id, "-"))
    {
        fpc = stderr;
        fpd = stdout;
    }
    else
    {
        (void) sprintf(fname,"%s/%s_%s.log",work_directory,id,tab);
        if ((fpc = fopen(fname,"wb")) == (FILE *) NULL)
        {
            (void) fputs(fname,stderr);
            putc(':', stderr);
            scarper(__FILE__,__LINE__,"Failed to open conversion log file");
            free(fname);
            free(U);
            free(tlook);
            free(tbuf);
            if (fpr != stdin)
                fclose(fpr);
            return;
        }
    }
/*
 * Output summary details to the log file
 */
    fprintf(fpc, "File %s output rows: %d maximum columns: %d\n", tab,
                        row_cnt, col_cnt);
    for (i = 0, l = 0; i < col_cnt; i++)
    {
        if (U[i] == 1)
            fprintf(fpc, "Output: %d\n", i + 1);
    }
/*
 * Open the output file: work_directory/id_table.dat
 */
    if (strcmp(id, "-"))
    {
        (void) sprintf(fname,"%s/%s_%s.dat",work_directory,id,tab);
        if ((fpd = fopen(fname,"wb")) == (FILE *) NULL)
        {
            (void) fputs(fname,stderr);
            putc(':', stderr);
            scarper(__FILE__,__LINE__,"Failed to open output data file");
            free(tlook);
            free(tbuf);
            free(fname);
            free(U);
            if (fpr != stdin)
                fclose(fpr);
            if (fpc != stderr)
                fclose(fpc);
            return;
        }
    }
/*
 * Re-write the data in separator format, omitting columns that are always
 * NULL.
 */
    (void) fseek(fpr,0,0);
    look_status = CLEAR;
    while ((tok_id = get_tok(fpr)) == SQL);
                  /* Skip any SQL statements at the start */
    fld_ind = 0;
    row_cnt = 0;
    while (tok_id != PEOF)
    {
        switch(tok_id)
        {
        case FNUMBER:
        case FIELD:
        case FLONG:
        case FRAW:
        case FDATE:
            l = strlen(tbuf);   /* Field Length */
            if ( U[fld_ind] == 1)
            {
                fputs(tbuf, fpd);
                putc(term_char, fpd);
            }
            if (l > 0)
            {
                if (strchr(tbuf, term_char) != (char *) NULL)
                {
                    fprintf(fpc,
                    "Output row %d - field '%s' contains field separator %c\n",
                             row_cnt + 1, tbuf, term_char);
                }
            }
            fld_ind++;
            break;
        case EOR:
            fld_ind = 0;
            putc('\n', fpd);
            row_cnt++;
            break;
        default:
            fprintf(fpc,"Warning at output line:%d - %s - unexpected\n",
                                 row_cnt + 1, tbuf);
            break;
        }
        tok_id = get_tok(fpr);
    }
    free(tbuf);
    free(tlook);
    free(U);
    free(fname);
/*
 * Close the files
 */
    if (fpr != stdin)
        fclose(fpr);
    if (fpc != stderr)
        fclose(fpc);
    if (fpd != stdout)
        fclose(fpd);
    return;
}
/****************************************************************************
 * Implement the quote capability
 */
static void quote(tab, mult_flag)
char * tab;
int mult_flag;
{
int i;
char * fname;
FILE * fpr;
FILE * fpd;
struct in_rec * (*rdr)();
/*
 * Open the input file
 */
    if (mult_flag)
        rdr = get_next_nomult;
    else
        rdr = get_next;
    if (!strcmp(tab, "-"))
        fpr = stdin;
    else
    if ((fpr = fopen(tab, "rb")) == (FILE *) NULL)
    {
        (void) fputs(tab,stderr);
        putc(':',stderr);
        scarper(__FILE__,__LINE__,"Failed to open input file");
        return;
    }
    fname = (char *) malloc(strlen(work_directory) +
                            strlen(id) +
                            strlen(tab) + 7);
/*
 * Open the output file: work_directory/id_table.dat
 */
    if (!strcmp(id, "-"))
        fpd = stdout;
    else
    {
        (void) sprintf(fname,"%s/%s_%s.dat",work_directory,id,tab);
        if ((fpd = fopen(fname,"wb")) == (FILE *) NULL)
        {
            (void) fputs(fname,stderr);
            putc(':', stderr);
            scarper(__FILE__,__LINE__,"Failed to open output data file");
            free(fname);
            if (fpr != stdin)
                fclose(fpr);
            return;
        }
    }
/*
 * Re-write the data in separator format, omitting columns that are always
 * NULL.
 */
    while ((cur_rec = rdr(&in_rec, fpr)) != (struct in_rec *) NULL)
    {
        if (cur_rec->fcnt < 1)
            continue;
        putc('\'', fpd);
        quote_stuff(fpd, cur_rec->fptr[1]);
        putc('\'', fpd);
        for (i = 2; i <= cur_rec->fcnt; i++)
        {
            putc(',', fpd);
            putc('\'', fpd);
            quote_stuff(fpd, cur_rec->fptr[i]);
            putc('\'', fpd);
        }
        putc('\n', fpd);
    }
    free(fname);
/*
 * Close the files
 */
    if (fpr != stdin)
        fclose(fpr);
    if (fpd != stdout)
        fclose(fpd);
    return;
}
/**************************************************************************
 * Output help, then exit
 */
static void opt_print(sts)
int sts;
{
    (void) puts("unquote: convert quoted/separated to pure separated\n\
  You can specify:\n\
  Options:\n\
  -  -i (Inverse mode)\n\
  -  -s (SQL statements need to be skipped (tabdiff input))\n\
  -  -d (Base directory; default .)\n\
  -  -t (terminator character)\n\
  -  -m (treat multiple terminator characters as separate fields)\n\
  -  -h (Output a help message)\n\
 \n\
  Arguments (after the options):\n\
  1 - ID (becomes part of the output filenames)\n\
  2 - n Files to work on.\n\
\n");
    exit(sts);
}
/***********************************************************************
 * Main Program Starts Here
 * VVVVVVVVVVVVVVVVVVVVVVVV
 */
int main(argc, argv)
int    argc;
char      *argv[];
{
int i, j;
char * x;
int ch;
int opt_cnt;
int inverse_flag = 0;
int mult_flag = 0;

    FS =strdup(FS_ro);
    work_directory = ".";
    term_char = '}';
    char_long = 240;
    while ( ( ch = getopt ( argc, argv, "ist:hd:m" ) ) != EOF )
    {
        switch ( ch )
        {
        case 'i' :
/*
 * Inverse flag; get the data ready for sqldrive or similar
 */
            inverse_flag = 1;
            break;
        case 's' :
/*
 * SQL Flag - SQL statements should be ignored (the file is tabdiff output)
 */
            sql_flag = 1;
            break;
        case 'd' :
/*
 * Directory to look for files/create files
 */
            work_directory = optarg;
        case 'm' :
/*
 * Directory to look for files/create files
 */
            mult_flag = 1;
            break;
        case 't' :
/*
 * Termination character to use
 */
            term_char = *optarg;
            FS[2] = term_char;
            FS[3] = '\0';
            break;

        case 'h' :
            opt_print(0);    /* Does not return */
        default:
        case '?' : /* Default - invalid opt.*/
            (void) fprintf(stderr,"Invalid option; try -h\n");
            opt_print(1);    /* Does not return */
        }
    }
    if (argc < (optind + 2))
    {
        (void) fprintf(stderr,"Insufficient Arguments Supplied\n");
        opt_print(1);    /* Does not return */
    }
    id = argv[optind++];
/*************************************************************************
 * Main processing : Loop through all the files for each option
 */
    for (i = optind; i < argc; i++)
    {
        if (inverse_flag)
            quote(argv[i], mult_flag);
        else
            unquote(argv[i]);
    }

/************************************************************************
 * Closedown
 */
    exit(0);
}
