/*
 * e2file.c - vaguely stdio-compatible disk file management with:
 * - Correct behaviour with flushing when reads, writes and seeks
 *   are mixed
 * - Buffering happening dynamically
 * - Delta log
 * - Transaction markers in the log
 * - Chain of write edits? Updates to this? Idea would be to avoid
 *   writing an 8K block just because a small bit has changed.
 *   Keep a single 'mod' range for each block.
 * - We insist that a single file is only opened the once.
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1994\n";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef LCC
#ifndef VCC2003
#define __cdecl
#include <unistd.h>
#include <sys/param.h>
#endif
#endif
#include <limits.h>
#include <errno.h>
#ifdef AIX
#include <fcntl.h>
#else
#ifdef LCC
#include <fcntl.h>
#else
#ifdef VCC2003
#include <fcntl.h>
#else
#ifdef ULTRIX
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#endif
#endif
#endif
#include "hashlib.h"
#include "e2file.h"
extern char * filename();
extern char * getcwd();
/*
 * Private information to do with the replacement stdio package
 */
static E2_CON * anchor;
static E2_CON * cur_e2_con;
#ifdef MINGW32
#include <windows.h>
#ifdef BUFSIZ
#undef BUFSIZ
#endif
#define BUFSIZ 16384
/*
 * Provide more or less direct access to the WIN32 File handling functions,
 * in order to get round the ridiculous limits on the number of simultaneously
 * opened files using the industry standard interface. We limit ourselves to
 * 4 GByte files.
 */
static int e2open(nm,acc,modes)
char * nm;
int acc;
int modes;
{
HANDLE fh;
DWORD dwDesiredAccess;
DWORD dwShareMode;
DWORD dwCreationDisposition;
DWORD dwFlagsAndAttributes;
static SECURITY_ATTRIBUTES sa = {
    sizeof(sa),
    NULL,
1};
/*
 * Set up Win32 equivalents to the usual flags
 */
    dwDesiredAccess = 0;
    if (acc & O_WRONLY || acc & O_APPEND || acc & O_TRUNC || acc & O_RDWR)
        dwDesiredAccess |= GENERIC_WRITE;
/*
 * On Windows, O_RDONLY is ZERO, so how can it be tested!?
 * However, the buffer management scheme requires that we have read access
 * to everything anyway.
 *
    if (!(acc & O_WRONLY))
 */
        dwDesiredAccess |= GENERIC_READ;
#ifdef O_BLKANDSET
    if (acc & O_BLKANDSET)
        dwShareMode = 0;
    else
#endif
    {
        dwShareMode = FILE_SHARE_READ;
        if (acc & O_RDWR)
            dwShareMode = FILE_SHARE_WRITE;
    }
    if (acc & O_TRUNC)
        dwCreationDisposition = CREATE_ALWAYS;
    else
    if (acc & O_CREAT)
        dwCreationDisposition = CREATE_NEW;
    else
    if (acc == O_RDONLY)
        dwCreationDisposition = OPEN_EXISTING;
    else
        dwCreationDisposition = OPEN_ALWAYS;
    fh = CreateFile(nm, dwDesiredAccess, dwShareMode, &sa,
                       dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE)
    {
        errno = GetLastError();
        return -1;
    }
    if (acc & O_APPEND)
        SetFilePointer(fh, 0, NULL, FILE_END);
    return (int) fh;
}
static long e2lseek(fh, pos, whence)
int fh;
long pos;
int whence;
{
DWORD dwMoveMethod;

    if (whence == 0)
        dwMoveMethod = FILE_BEGIN;
    else
    if (whence == 1)
        dwMoveMethod = FILE_CURRENT;
    else
        dwMoveMethod = FILE_END;
    return SetFilePointer((HANDLE) fh, pos, NULL, dwMoveMethod);
}
static int e2read(fh,ptr,len)
int fh;
char * ptr;
int len;
{
int ret;
int cnt;

    ret = ReadFile((HANDLE) fh, ptr, len, &cnt, NULL);
    if (ret)
        return cnt;
    else
    {
        errno = GetLastError();
        return -1;
    }
}
static int e2write(fh,ptr,len)
int fh;
char * ptr;
int len;
{
int ret;
int cnt;

    ret = WriteFile((HANDLE) fh, ptr, len, &cnt, NULL);
    if (ret)
        return cnt;
    else
    {
        errno = GetLastError();
        return -1;
    }
}
static int e2close(fh)
int fh;
{
    return CloseHandle((HANDLE) fh);
}
#endif
#ifndef MINGW32
#define HMODULE void*
#include <dlfcn.h>
#define GetProcAddress dlsym
#endif
/****************************************************************************
 * Overcome limitations on the numbers of open files.
 *
 * Re-direct calls for a stdio library to the appropriate routines. Handles
 * stdin, stdout and stderr.
 */
static struct std_fun {
    HMODULE hcrtdll;
    FILE * (*fopen)(const char * fname, const char * access);
    int (*fclose)(FILE * fp);
    int (*fflush)(FILE * fp);
    int (*fputc)(char c, FILE * fp);
    int (*fputs)(const char *ptr, FILE * fp);
    char * (*fgets)(char * ptr, int maxlen, FILE * fp);
    size_t (*fread)(const void *, size_t sz, size_t n_els, FILE * fp);
    size_t (*fwrite)(const void *, size_t sz, size_t n_els, FILE * fp);
    long (*fseek)(FILE * fp, long whence, int where);
    long (*ftell)(FILE * fp);
    int (*vfprintf)(FILE * fp, const char * fmt, va_list arglist);
} std_fun;
static void ini_lib()
{
#ifdef MINGW32
#ifdef USE_CRTDLL
    if ((std_fun.hcrtdll = GetModuleHandle("msvcrt.dll")) == (HMODULE) NULL) 
         std_fun.hcrtdll = LoadLibrary("msvcrt.dll");
#else
    if ((std_fun.hcrtdll = GetModuleHandle("crtdll.dll")) == (HMODULE) NULL) 
         std_fun.hcrtdll = LoadLibrary("crtdll.dll");
#endif
#else
    std_fun.hcrtdll = dlopen("/usr/lib/libc.so.1",RTLD_LAZY);
#endif
    std_fun.fopen = (FILE *(__cdecl *)(const char *,const char *)) GetProcAddress(std_fun.hcrtdll, "fopen"); 
    std_fun.fclose = (int (__cdecl *)(FILE *)) GetProcAddress(std_fun.hcrtdll, "fclose"); 
    std_fun.fflush =  (int (__cdecl *)(FILE *)) GetProcAddress(std_fun.hcrtdll, "fflush"); 
    std_fun.fputc = (int (__cdecl *)(char,FILE *)) GetProcAddress(std_fun.hcrtdll, "fputc"); 
    std_fun.fputs = (int (__cdecl *)(const char *,FILE *)) GetProcAddress(std_fun.hcrtdll, "fputs"); 
    std_fun.fgets = (char *(__cdecl *)(char *,int,FILE *)) GetProcAddress(std_fun.hcrtdll, "fgets"); 
    std_fun.vfprintf =  (int (__cdecl *)(FILE *,const char *,va_list)) GetProcAddress(std_fun.hcrtdll, "vfprintf"); 
    std_fun.fwrite =  (size_t (__cdecl *)(const void *,size_t,size_t,FILE *)) GetProcAddress(std_fun.hcrtdll, "fwrite"); 
    std_fun.ftell =  (long (__cdecl *)(FILE *)) GetProcAddress(std_fun.hcrtdll, "ftell"); 
    std_fun.fseek =  (long (__cdecl *)(FILE *,long,int)) GetProcAddress(std_fun.hcrtdll, "fseek"); 
    std_fun.fread = (size_t (__cdecl *)(const void *,size_t,size_t,FILE *)) GetProcAddress(std_fun.hcrtdll, "fread"); 
    return;
}
#ifdef REPLACE_STDIO
/*
 * stdio library interface that directs key calls to the correct implementation
 */
FILE * fopen(fn,mode)
const char * fn;
const char * mode;
{
    if (cur_e2_con == NULL && e2init(NULL, 2048, 4096) == (E2_CON *) NULL)
        return (FILE *) NULL;
    return (FILE *) e2fopen(fn, mode);
}
void setbuf(FILE *fp, char *buf)
{
    return;
}
int setvbuf(FILE *fp, char *x, int i, size_t s)
{
    return 0;
}
int fclose(fp)
FILE *fp;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fclose(fp);
    }
    return e2fclose((E2_FILE *) fp);
}
int fflush(fp)
FILE *fp;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fflush(fp);
    }
    return e2fflush((E2_FILE *) fp);
}
long ftell(fp)
FILE *fp;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.ftell(fp);
    }
    return e2ftell(((E2_FILE *) fp));
}
int fputs(ptr, fp)
const char *ptr;
FILE *fp;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fputs(ptr, fp);
    }
    return e2fputs(ptr, (E2_FILE *) fp);
}
char * fgets(ptr, len, fp)
char * ptr;
int len;
FILE * fp;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fgets(ptr, len, fp);
    }
    return e2fgets(ptr, len, (E2_FILE *) fp);
}
int fputc(c, fp)
char c;
FILE *fp;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fputc(c, fp);
    }
    return e2fputc(c, (E2_FILE *) fp);
}
size_t fread(ptr,s, c, fp)
void * ptr;
size_t s;
size_t c;
FILE *fp;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fread(ptr,s,c, fp);
    }
    return e2fread(ptr,s,c, (E2_FILE *) fp);
}
size_t fwrite(ptr,s, c, fp)
const void * ptr;
size_t s;
size_t c;
FILE *fp;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fwrite(ptr,s,c, fp);
    }
    return e2fwrite(ptr,s,c, (E2_FILE *) fp);
}
int fseek(fp, s, c)
FILE *fp;
long s;
int c;
{
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fseek(fp,s,c);
    }
    return e2fseek((E2_FILE *) fp,s,c);
}
int fprintf( FILE *fp, const char * x, ...)
{
    va_list(arglist);
    va_start(arglist,x);
    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.vfprintf(fp,x,arglist);
    }
    return e2vfprintf((E2_FILE *) fp, x, arglist);
}
#endif
#ifndef MINGW32
#define e2open open
#define e2close close
#define e2read read
#define e2write write
#define e2lseek lseek
#define e2tell tell
#endif
#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX _POSIX_PATH_MAX
#endif
#endif
/*
 * Hash the key fields in the buffer header.
 */
static int uhash_buf (utp,modulo)
E2_BUF_HDR * utp;
int modulo;
{
#ifdef DEBUG
    if (utp == (E2_BUF_HDR *) NULL)
    {
        fputs("Hashed NULL buffer header\n", stderr);
        fflush(stderr);
        return 0;
    }
    else
    if (utp->ip == (E2_INO *) NULL)
    {
        fprintf("Hashed NULL file pointer\n", stderr);
        fflush(stderr);
        return 0;
    }
    else
        fprintf(stderr,"Key: %x %d\n",(unsigned long) utp->ip,utp->buf_no);
    fflush(stderr);
#endif
    return ( long_hh(((long)(utp->ip) ^ utp->buf_no),modulo) &(modulo - 1));

}
static int ucomp_buf(utp1,  utp2)
E2_BUF_HDR * utp1;
E2_BUF_HDR * utp2;
{
    return (utp1->ip == utp2->ip) ? (( utp1->buf_no == utp2->buf_no) ? 0 :
    (( utp1->buf_no > utp2->buf_no) ? 1 : -1)) :
    ((((long) (utp1->ip)) > ((long) utp2->ip)) ? 1 : -1);
}
/*
 * Dump out an E2_INO structure
 */
static void e2inodump(ip, label)
E2_INO * ip;
char * label;
{
    (void) fprintf(stderr,"%s - ", label);
    if (ip == (E2_INO *) NULL)
        (void) fputs("NULL e2ino\n", stderr);
    else
        fprintf(stderr,"(fname = %s, fd = %d, w_cnt = %d)\n",
               ip->fname, ip->fd, ip->w_cnt);
    return;
}
/*
 * Dump out a buffer.
 */
static void e2bufdump(bp, label)
E2_BUF_HDR * bp;
char * label;
{
    e2inodump(bp->ip,label);
    fprintf(stderr,
           "Buffer %d Found at %d, Used %d, Write Offset %d To Be Written %d\n",
            bp->buf_no, (bp - bp->conp->bhp), (bp->vp - bp->bp),
            (bp->wbp - bp->bp), (bp->wep - bp->wbp));
    fprintf(stderr,"bp->bp=%x bp->wbp=%x bp->wep=%x bp->vp=%x bp->ep=%x\n",
                  bp->bp, bp->wbp, bp->wep, bp->vp, bp->ep);
    if (bp->vp > bp->bp)
        fprintf(stderr,"Contents:\n%*.*s\n",(bp->vp - bp->bp),
                (bp->vp - bp->bp), bp->bp);
    if (bp->wbp != (char *) NULL)
        fprintf(stderr,"To be Written:\n%*.*s\n",
                  (bp->wep - bp->wbp),(bp->wep - bp->wbp), bp->wbp);
    return;
}
/*
 * Initialise an E2_CON ready for use
 */
E2_CON * e2init( logfile, no_of_files, no_of_buffers)
char * logfile;
int no_of_files;
int no_of_buffers;
{
E2_CON * prev_anchor = anchor;
E2_BUF_HDR * b;
E2_FILE * f;
E2_INO * i;
char *x;

    if ((anchor = (E2_CON *) malloc(sizeof(*anchor))) == (E2_CON *) NULL)
    {
        errno = ENOMEM;
        anchor = prev_anchor;
        return (E2_CON *) NULL;
    }
    if (prev_anchor == (E2_CON *) NULL)
        if (atexit(e2finish))
        {
            free(anchor);
            anchor = prev_anchor;
            return (E2_CON *) NULL;
        }
    if ((anchor->bp = (char *) malloc(BUFSIZ * no_of_buffers)) == (char *) NULL)
    {
        errno = ENOMEM;
        free(anchor);
        anchor = prev_anchor;
        return (E2_CON *) NULL;
    }
    if ((anchor->fp = (E2_FILE *) calloc(no_of_files, sizeof(*(anchor->fp))))
           == (E2_FILE *) NULL)
    {
        errno = ENOMEM;
        free(anchor->bp);
        free(anchor);
        anchor = prev_anchor;
        return (E2_CON *) NULL;
    }
    if ((anchor->ip = (E2_INO *) calloc(no_of_files, sizeof(*(anchor->ip))))
           == (E2_INO *) NULL)
    {
        errno = ENOMEM;
        free(anchor->bp);
        free(anchor->fp);
        free(anchor);
        anchor = prev_anchor;
        return (E2_CON *) NULL;
    }
    if ((anchor->bhp=(E2_BUF_HDR *)calloc(no_of_buffers,sizeof(*(anchor->bhp))))
           == (E2_BUF_HDR *) NULL)
    {
        errno = ENOMEM;
        free(anchor->bp);
        free(anchor->fp);
        free(anchor->ip);
        free(anchor);
        anchor = prev_anchor;
        return (E2_CON *) NULL;
    }
    if (logfile == (char *) NULL)
        anchor->log_fd = -1;
    else
    if ((anchor->log_fd = e2open(logfile,O_CREAT|O_WRONLY,0664)) < 0)
    {
        perror("logfile open failed");
        free(anchor->bp);
        free(anchor->fp);
        free(anchor->ip);
        free(anchor->bhp);
        free(anchor);
        anchor = prev_anchor;
        return (E2_CON *) NULL;
    }
    if ((anchor->buf_hash = hash(no_of_buffers,uhash_buf, ucomp_buf))
        == (HASH_CON *) NULL)
    {
        if (anchor->log_fd != -1)
            (void) e2close(anchor->log_fd);
        free(anchor->bp);
        free(anchor->fp);
        free(anchor->ip);
        free(anchor->bhp);
        free(anchor);
        anchor = prev_anchor;
        return (E2_CON *) NULL;
    }
    if ((anchor->fil_hash = hash(no_of_files,string_hh, strcmp))
        == (HASH_CON *) NULL)
    {
        cleanup(anchor->buf_hash);
        if (anchor->log_fd != -1)
            (void) e2close(anchor->log_fd);
        free(anchor->bp);
        free(anchor->fp);
        free(anchor->ip);
        free(anchor->bhp);
        free(anchor);
        anchor = prev_anchor;
        return (E2_CON *) NULL;
    }
    anchor->logfile = logfile;             /* Name of file to use for logging */
    anchor->f_cnt = no_of_files;           /* Number of files */
    anchor->buf_cnt = no_of_buffers;       /* Number of buffers    */
    anchor->log_where = &(anchor->log_buffer[0]);
                          /* Whereabouts in the log buffer we are */
    anchor->whp = (E2_BUF_HDR *) NULL;     /* Write list head pointer */
    anchor->wtp = (E2_BUF_HDR *) NULL;     /* Write list tail pointer */
    anchor->chp = anchor->bhp;             /* Clean list head pointer  */
/*
 * Initialise the buffers
 */
    x = anchor->bp;
    anchor->chp->next = anchor->bhp + 1;    /* Next pointer  */
    anchor->chp->prev = (E2_BUF_HDR *) NULL;/* Previous pointer */
    anchor->chp->conp = anchor;            /* Back Pointer */
    anchor->chp->bp   = x;
    anchor->chp->ep   = x + BUFSIZ;
    anchor->chp->vp   = x;
    anchor->ctp = anchor->bhp + no_of_buffers - 1;
                                           /* Clean list tail pointer  */
    for (b = anchor->bhp + 1; b < anchor->ctp; b++)
    {
        x += BUFSIZ;
        b->conp = anchor;            /* Back Pointer */
        b->next = b + 1;
        b->prev = b - 1;
        b->bp   = x;
        b->ep   = x + BUFSIZ;
        b->vp   = x;
    } 
    x += BUFSIZ;
    b->bp   = x;
    b->ep   = x + BUFSIZ;
    b->vp   = x;
    b->next = (E2_BUF_HDR *) NULL;         /* Next pointer */
    b->prev =  b - 1;                      /* Previous pointer */
    b->conp = anchor;                      /* Back Pointer */
    for ( f = anchor->fp, i = anchor->ip; no_of_files; no_of_files--, i++, f++)
    {
        i->conp = anchor;
        i->fd = -1;
        f->conp = anchor;
    }
/*
 * Chain this to the others, if there are any.
 */
    anchor->next_e2_con = prev_anchor;     /* Chain pointer           */
    cur_e2_con = anchor;
    return anchor;
}
/*
 * Flush out the log buffer
 */
static void e2log_flush(e2con)
E2_CON * e2con;
{
int i;

    if (e2con->log_fd != -1 
      && (i = (e2con->log_where - &(e2con->log_buffer[0]))))
    {
        char * x = &(e2con->log_buffer[0]);
        int j;
        do
        {
            if ((j = e2write(e2con->log_fd, x, i)) <= 0)
            {
                 perror("log write() failed");
                 return;
            }
            i -= j;
            x += j;
        } while ( i > 0);
    }
    e2con->log_where = &(e2con->log_buffer[0]);
    return;
}
/*
 * Get rid of an E2_CON, releasing all memory
 */
static void e2dismantle(e2con)
E2_CON * e2con;
{
int i;
E2_CON * w;
E2_CON * prev;
E2_FILE * ef;

    for (prev = (E2_CON *) NULL, w = anchor;
              w != e2con && w != (E2_CON *) NULL;
                  prev = w, w = w->next_e2_con);
    if (w == (E2_CON *) NULL)
        return;                      /* Not a valid pointer */
    e2commit(e2con);                 /* Flush out the outstanding writes */
    for (ef = e2con->fp, i  = e2con->f_cnt;
             i;
                 ef++, i--)
    {
        if (ef->ip != (E2_INO *) NULL)
            (void) e2fclose(ef);         /* Close all the files */
    }
    if (prev == (E2_CON *) NULL)
        anchor = e2con->next_e2_con;
    else
        prev->next_e2_con = e2con->next_e2_con;
    if (cur_e2_con == e2con)
        cur_e2_con = anchor;
    if (e2con->log_fd != -1)
        (void) e2close(e2con->log_fd);
    free(e2con->bp);
    free(e2con->fp);
    free(e2con->ip);
    free(e2con->bhp);
    cleanup(e2con->fil_hash);
    cleanup(e2con->buf_hash);
    return;
}
/*
 * Routine registered with atexit() to close all files etc.
 */
void e2finish()
{
E2_CON * e2con;

    for (e2con = anchor; e2con != (E2_CON *) NULL;)
    {
        anchor = e2con;
        e2con = e2con->next_e2_con;
        e2dismantle(anchor);
    } 
    return;
}
static struct f_mode_flags {
    char * modes;
    int flags;
} f_mode_flags[] =
{
{ "r", O_RDONLY },
{ "rb", O_RDONLY },
{"r+",  O_RDWR  },
{"rb+",  O_RDWR  },
{"r+b",  O_RDWR  },
#ifndef ULTRIX
{"w", O_CREAT | O_APPEND | O_TRUNC | O_WRONLY  },
{"wb", O_CREAT | O_APPEND | O_TRUNC | O_WRONLY  },
{"a", O_CREAT | O_WRONLY | O_APPEND  },
{"ab", O_CREAT | O_WRONLY | O_APPEND  },
{"w+", O_CREAT | O_TRUNC | O_RDWR  },
{"wb+", O_CREAT | O_TRUNC | O_RDWR  },
{"w+b", O_CREAT | O_TRUNC | O_RDWR  },
{"a+", O_CREAT | O_APPEND | O_RDWR  },
{"ab+", O_CREAT | O_APPEND | O_RDWR  },
{"a+b", O_CREAT | O_APPEND | O_RDWR  },
#else
{"w", O_CREAT | O_APPEND | O_TRUNC | O_WRONLY | O_BLKANDSET },
{"a", O_CREAT | O_WRONLY | O_APPEND | O_BLKANDSET },
{"A", O_CREAT | O_APPEND | O_WRONLY | O_BLKANDSET },
{"wb", O_CREAT | O_APPEND | O_TRUNC | O_WRONLY | O_BLKANDSET },
{"ab", O_CREAT | O_WRONLY | O_APPEND | O_BLKANDSET },
{"Ab", O_CREAT | O_APPEND | O_WRONLY | O_BLKANDSET },
{"wb+", O_CREAT | O_TRUNC | O_RDWR | O_BLKANDSET },
{"ab+", O_CREAT | O_APPEND | O_RDWR | O_BLKANDSET },
{"Ab+", O_CREAT | O_APPEND | O_RDWR | O_BLKANDSET },
{"w+b", O_CREAT | O_TRUNC | O_RDWR | O_BLKANDSET },
{"a+b", O_CREAT | O_APPEND | O_RDWR | O_BLKANDSET },
{"A+b", O_CREAT | O_APPEND | O_RDWR | O_BLKANDSET },
#endif
{0,0}
};
/*
 * Check the input mode flags for open
 */
static int check_mode(mode, flags)
char * mode;
int * flags;
{
register struct f_mode_flags * fm;
/*
 * Validate the mode flags
 */
    for (fm = f_mode_flags;
             fm->modes != (char *) NULL && strcmp(fm->modes,mode);
                 fm++);
    if (fm->modes == (char *) NULL)
    {
        fprintf(stderr, "%s:line %d: Invalid file mode %s",__FILE__, __LINE__,
                         mode);
        return 0;
    }
    else
    {
        *flags = fm->flags;
        return 1;
    }
}
/*
 * Check the input mode flags for open
 */
static int check_flags(mode, flags)
char ** mode;
int flags;
{
register struct f_mode_flags * fm;
/*
 * Validate the mode flags
 */
    for (fm = f_mode_flags;
             fm->modes != (char *) NULL && flags != fm->flags;
                 fm++);
    if (fm->modes == (char *) NULL)
    {
        fprintf(stderr, "%s:line %d: Invalid file mode %s",__FILE__, __LINE__,
                         mode);
        return 0;
    }
    else
    {
        *mode = fm->modes;
        return 1;
    }
}
/*
 * Find free file slot
 */
static E2_FILE * find_f_slot(e2con)
E2_CON * e2con;
{
int i;
E2_FILE * fp;

    for (fp = e2con->fp, i = e2con->f_cnt;
              i && (fp->ip != (E2_INO *) NULL);
                    i--, fp++);
    if (!i)
    {
        errno = ENOSPC;
        return (E2_FILE *) NULL;
    }
    else
        return fp;
}
/*
 * Find free ino slot
 */
static E2_INO * find_i_slot(e2con)
E2_CON * e2con;
{
int i;
E2_INO * ip;

    for (ip = e2con->ip, i = e2con->f_cnt;
              i && (ip->fname != (char *) NULL);
                    i--, ip++);
    if (!i)
    {
        errno = ENOSPC;
        return (E2_INO *) NULL;
    }
    else
        return ip;
}
/*
 * Add stuff to the edit log
 */
static void e2log_add(e2con, bp, len)
E2_CON * e2con;
char * bp;
int len;
{
int slen;

    if ((e2con->log_where + len) < &(e2con->log_buffer[65536]))
    {
        memcpy(e2con->log_where, bp, len);
        e2con->log_where += len;
    }
    else
    {
       int slen =  &(e2con->log_buffer[65536]) - e2con->log_where;
       memcpy(e2con->log_where, bp, slen);
       e2log_flush(e2con);
       e2con->log_where  =   &(e2con->log_buffer[0]) +
                (len - slen);
       if (slen != len)
           memcpy(&(e2con->log_buffer[0]), bp + slen, len - slen);
    }
    return;
}
/*
 * Add write stuff to the edit log
 */
static void e2log_wadd(bp)
E2_BUF_HDR * bp;
{
register E2_CON * e2con = bp->conp;
char tmp_buf[1 + sizeof(bp->ip)+sizeof(long) + sizeof(long) + BUFSIZ];
int len;

    if (bp->wbp == bp->wep)
    {
        e2bufdump(bp, "Logic Error: buffer on write chain with no write\n");
        abort();
    }
    tmp_buf[0] = 'W';
    memcpy((char *) &tmp_buf[1], &(bp->ip), sizeof(bp->ip)); /* Which file    */
    len = bp->buf_no * BUFSIZ + (bp->wbp - bp->bp);
    memcpy((char *) &tmp_buf[1] + sizeof(bp->ip),
            &len, sizeof(len));                              /* Seek Position */
    len = bp->wep - bp->wbp;
    memcpy((char *) &tmp_buf[1] + sizeof(bp->ip) + sizeof(len),
            &len, sizeof(len));                              /* Length        */
    memcpy((char *) &tmp_buf[1] + sizeof(bp->ip) + sizeof(len) + sizeof(len),
            bp->wbp, len);                                   /* Data          */
    len += ( 1 +  sizeof(bp->ip) + sizeof(len) + sizeof(len));
    e2log_add(e2con, &tmp_buf[0],len);
    return;
}
/*
 * Add file open stuff to the edit log
 */
static void e2log_oadd(ip)
E2_INO * ip;
{
register E2_CON * e2con = ip->conp;
char tmp_buf[1 + sizeof(ip)+sizeof(ip->flags) + sizeof(long) + PATH_MAX];
int len;

    tmp_buf[0] = 'O';
    memcpy((char *) &tmp_buf[1], &(ip), sizeof(ip));         /* Which file */
    memcpy((char *) &tmp_buf[1] + sizeof(ip),
            &(ip->flags), sizeof((ip->flags)));               /* Flags */
    len = strlen(ip->fname) + 1;
    memcpy((char *) &tmp_buf[1] + sizeof(ip) + sizeof(ip->flags),
            &len, sizeof(len));                              /* Path Length   */
    memcpy((char *) &tmp_buf[1] + sizeof(ip) + sizeof(ip->flags) + sizeof(len),
            ip->fname, len);                             /* Path Name     */
    len += ( 1 +  sizeof(ip) + sizeof(ip->flags) + sizeof(len));
    e2log_add(e2con, &tmp_buf[0], len);
    return;
}
/*
 * Function that takes a buffer off the clean chain, and leaves it
 * in limbo
 */
static void e2take_clean(bp)
E2_BUF_HDR * bp;
{
register E2_CON * e2con = bp->conp;

    if (bp->prev == (E2_BUF_HDR *) NULL)
    {
        if (bp != e2con->chp)
        {
            fprintf(stderr,
            "%s:line %d Logic Error: e2con and bp disagree about clean head?\n",
                __FILE__,__LINE__);
            fprintf(stderr,"e2con: %x bp: %x file: %s\n",
                        (long) e2con, (long) bp, bp->ip->fname);
            if (bp->next != (E2_BUF_HDR *) NULL)
                abort();
            else
                return;
        }
        e2con->chp = bp->next; 
        if (e2con->chp == (E2_BUF_HDR *) NULL)
        {
            if (bp != e2con->ctp)
            {
                fprintf(stderr,
            "%s:line %d Logic Error: e2con and bp disagree about clean tail?\n",
                        __FILE__,__LINE__);
                fprintf(stderr,"e2con: %x bp: %x file: %s\n",
                        (long) e2con, (long) bp, bp->ip->fname);
            }
            e2con->ctp = (E2_BUF_HDR *) NULL;
        }
        else
            e2con->chp->prev = (E2_BUF_HDR *) NULL;
    }
    else
    {
        bp->prev->next = bp->next;
        if (bp->next == (E2_BUF_HDR *) NULL)
        {
            if (bp != e2con->ctp)
            {
                fprintf(stderr,
          "%s:line %d Logic Error: e2con and bp disagree about clean tail?\n",
                            __FILE__, __LINE__);
                fprintf(stderr,"e2con: %x bp: %x file: %s\n",
                        (long) e2con, (long) bp, bp->ip->fname);
                abort();
            }
            e2con->ctp = bp->prev;
        }
        else
            bp->next->prev = bp->prev;
    }
    bp->next = (E2_BUF_HDR *) NULL;
    bp->prev = (E2_BUF_HDR *) NULL;
    return;
}
/*
 * Function that takes a buffer off the clean chain, and disassociates
 * it from everything.
 */
static void e2free_clean(bp)
E2_BUF_HDR * bp;
{
    e2take_clean(bp);
    if (bp->ip != (E2_INO *) NULL)
    {
        hremove(bp->conp->buf_hash, (char *) bp);
        bp->ip = (E2_INO *) NULL;       /* Break the association */
    }
    return;
}
/*
 * Function that associates a buffer with a file, and readies it for use.
 */
static void e2fresh(bp,ip,buf_no)
E2_BUF_HDR * bp;
E2_INO * ip;
int buf_no;
{
    bp->ip = ip;
    bp->buf_no = buf_no;
    if (bp->ip != (E2_INO *) NULL)
        insert(ip->conp->buf_hash, (char *) bp, (char *) bp);
    bp->vp = bp->bp;
    bp->wbp = (char *) NULL;
    bp->wep = (char *) NULL;
    return;
}
/*
 * Function that takes a buffer off the write chain, and leaves it
 * in limbo
 */
static void e2take_write(bp)
E2_BUF_HDR * bp;
{
register E2_CON * e2con = bp->conp;

    if (bp->prev == (E2_BUF_HDR *) NULL)
    {
        if (bp != e2con->whp)
        {
            fputs("Logic Error: e2con and bp disagree about write head\n",
                            stderr);
            fprintf(stderr,"e2con: %x bp: %x file: %s\n", (long) e2con,
                       (long) bp,
                       bp->ip->fname);
            abort();
        }
        e2con->whp = bp->next; 
        if (e2con->whp == (E2_BUF_HDR *) NULL)
        {
            if (bp != e2con->wtp)
            {
                fputs("Logic Error: e2con and bp disagree about write tail\n",
                            stderr);
                fprintf(stderr,"e2con: %x bp: %x file: %s\n", (long) e2con,
                       (long) bp,
                       bp->ip->fname);
                abort();
            }
            e2con->wtp = (E2_BUF_HDR *) NULL;
        }
        else
            e2con->whp->prev  = (E2_BUF_HDR *) NULL;
    }
    else
    {
        bp->prev->next = bp->next;
        if (bp->next == (E2_BUF_HDR *) NULL)
        {
            if (bp != e2con->wtp)
            {
                fputs("Logic Error: e2con and bp disagree about write tail\n",
                            stderr);
                fprintf(stderr,"e2con: %x bp: %x file: %s\n",
                        (long) e2con, (long) bp, bp->ip->fname);
                abort();
            }
            e2con->wtp = bp->prev;
        }
        else
            bp->next->prev = bp->prev;
    }
    bp->next = (E2_BUF_HDR *) NULL;
    bp->prev = (E2_BUF_HDR *) NULL;
    return;
}
/*
 * Add a floating buffer to the tail of the clean list. This will be the
 * first buffer that gets re-used.
 */
static void e2add_clean_tail(bp)
E2_BUF_HDR * bp;
{
register E2_CON * e2con = bp->conp;

    bp->next = (E2_BUF_HDR *) NULL;
    bp->prev = e2con->ctp;
    if (e2con->chp ==  (E2_BUF_HDR *) NULL)
        e2con->chp = bp;
    else
        bp->prev->next = bp;
    e2con->ctp = bp;
    return;
}
/*
 * Add a floating buffer to the head of the clean list. This will be the
 * first buffer that gets re-used.
 */
static void e2add_clean_head(bp)
E2_BUF_HDR * bp;
{
register E2_CON * e2con = bp->conp;

    bp->prev = (E2_BUF_HDR *) NULL;
    bp->next = e2con->chp;
    if (e2con->ctp ==  (E2_BUF_HDR *) NULL)
        e2con->ctp = bp;
    else
        bp->next->prev = bp;
    e2con->chp = bp;
    return;
}
/*
 * Add a floating buffer to the tail of the write list. This will be the
 * first buffer that gets re-used.
 */
static void e2add_write_tail(bp)
E2_BUF_HDR * bp;
{
register E2_CON * e2con = bp->conp;

    bp->next = (E2_BUF_HDR *) NULL;
    bp->prev = e2con->wtp;
    if (e2con->whp ==  (E2_BUF_HDR *) NULL)
        e2con->whp = bp;
    else
        bp->prev->next = bp;
    e2con->wtp = bp;
    return;
}
/*
 * Add a floating buffer to the head of the write list. This will be the
 * first buffer that gets written, usually.
 */
static void e2add_write_head(bp)
E2_BUF_HDR * bp;
{
register E2_CON * e2con = bp->conp;

    bp->prev = (E2_BUF_HDR *) NULL;
    bp->next = e2con->whp;
    if (e2con->wtp ==  (E2_BUF_HDR *) NULL)
        e2con->wtp = bp;
    else
        bp->next->prev = bp;
    e2con->whp = bp;
    return;
}
/*
 * Disassociate a buffer from a file, and move it to the clean list tail.
 * Carry out a sanity check as we go.
 */
static void e2release(bp)
E2_BUF_HDR * bp;
{
/*
 * Remove the buffer from the clean chain
 */
    e2free_clean(bp);
/*
 * Add it to the tail of the clean chain
 */
    e2add_clean_tail(bp);
    return;
}
/*
 * Write out a buffer, remove it from the write list, and add it to the head
 * of the clean list
 */
static void e2put(bp)
E2_BUF_HDR * bp;
{
register E2_CON * e2con = bp->conp;
char * x;

    if ((bp->wbp < bp->bp) || (bp->wep > bp->ep)
     || (bp->wep < bp->bp) || (bp->wbp > bp->ep)
     || (bp->vp < bp->wbp) || (bp->wep > bp->vp))
    {
        fputs("Logic Error: block administration corrupt\n", stderr);
        e2bufdump(bp, "e2put");
        abort();
    }
    e2log_wadd(bp);                           /* Log the write */
    while (e2lseek(bp->ip->fd, (bp->buf_no * BUFSIZ + (bp->wbp - bp->bp)),
                SEEK_SET) !=  (bp->buf_no * BUFSIZ + (bp->wbp - bp->bp)))
    {
        if (errno != EINTR)
        {
            perror("lseek(): unexpected failure");
            fprintf(stderr,"Error number: %d\n", errno);
            e2bufdump(bp, "e2put");
            abort();
        }
    }
    if (e2write(bp->ip->fd,bp->wbp, (bp->wep - bp->wbp)) != (bp->wep - bp->wbp))
    {
        perror("write(): unexpected failure");
        fprintf(stderr,"Error number: %d\n", errno);
        e2bufdump(bp, "e2put");
        abort();
    }
    bp->wbp = (char *) NULL;                 /* Mark it as gone */
    bp->wep = (char *) NULL;
    bp->ip->w_cnt--;
    if (bp->ip->w_cnt < 0)
    {
        fprintf(stderr,"Logic Error: write count negative for %s\n",
                       bp->ip->fname);
        e2bufdump(bp, "e2put");
        abort();
    }
    e2take_write(bp);
/*
 * Add it to the head of the clean chain
 */
    e2add_clean_head(bp);
    return;
}
/*
 * Wander down the write chain, writing out whatever needs doing,
 * logging it, and putting the buffers onto the clean chain.
 * If ip is set, only process buffers for that file.
 */
static void e2force(e2con, ip)
E2_CON * e2con;
E2_INO * ip;
{
E2_BUF_HDR *sav_bp, * bp = e2con->wtp;

    while (bp != (E2_BUF_HDR *) NULL)
    {
        sav_bp = bp;
        bp = bp->prev;
        if ((ip == (E2_INO *) NULL || sav_bp->ip == ip))
            e2put(sav_bp);
    }
    return;
}
/*
 * Release all, or only those blocks for a particular file.
 */
static void e2clean(ip)
E2_INO * ip;
{
register E2_CON * e2con = ip->conp;
E2_BUF_HDR *sav_bp, * bp = e2con->chp;

    while (bp != (E2_BUF_HDR *) NULL)
    {
        if (bp->wbp != (char *) NULL || bp->wep != (char *) NULL)
        {
            fprintf(stderr, "Logic Error: dirty block on clean list for %s\n",
                       bp->ip->fname);
            fprintf(stderr,"e2con: %x bp: %x\n",
                        (long) e2con, (long) bp);
            e2bufdump(bp, "Buffer");
            abort();
        }
        if (bp->ip != (E2_INO *) NULL
         && (ip == (E2_INO *) NULL  || bp->ip == ip))
        {
            sav_bp = bp;
            bp = bp->next;
            e2release(sav_bp);
        }
        else
            bp = bp->next;
    }
    return;
}
/*
 *    Open the file for filename, and return the fp.
 *    Lock the file to prevent modification by others.
 */
E2_FILE * e2fopen( f, mode)
char    *f;
char    *mode;
{
#ifndef ULTRIX
#ifndef MINGW32
static struct flock fl = {F_WRLCK,0,0,0,0};
#endif
#endif
static int cmode=0660;
E2_FILE    *fp;
E2_INO     *ip;
char    *name;
int fd, fm;
int i;
HIPT * h;
/*
 * Validate the mode flags
 */
    if (!check_mode(mode,&fm))
    {
        errno = EINVAL;
        return (E2_FILE *) NULL;
    }
/*
 * Search for a free E2_FILE slot
 */
    if ((fp = find_f_slot(cur_e2_con)) == (E2_FILE *) NULL)
        return (E2_FILE *) NULL;
/*
 * Construct the file name
 */
    if (*f != '/' && *(f + 1) != ':')
    {
        char d[PATH_MAX+1];
        errno = EPERM;
        if (getcwd(&d[0],sizeof(d)) == (char *) NULL)
            return (E2_FILE *) NULL;
        name = filename("%s/%s", d, f);
    }
    else
        name = strdup(f);
    if (name == (char *) NULL)
    {
        errno = ENOMEM;
        return (E2_FILE *) NULL;
    }
    while (fp->ip == (E2_INO *) NULL)
    {
/*
 * Have we already got this one?
 */
        if ((h = lookup(cur_e2_con->fil_hash, name)) == (HIPT *) NULL)
        {                      /* No */
            if ((fp->ip = find_i_slot(cur_e2_con)) == (E2_INO *) NULL)
                return (E2_FILE *) NULL;
            fp->ip->fname = name;
            insert(cur_e2_con->fil_hash,fp->ip->fname,fp->ip);
            fp->ip->fd = -1;
        }
        else
        {                      /* Yes */
            free(name);
            fp->ip = (E2_INO *) (h->body);
            if (fp->ip->flags != fm)
            {
                e2fclose(fp);
            }
        }
    }
    if (fp->ip->fd == -1)
    {          /* We actually need to open the file */
#ifndef ULTRIX
        while ( fp->ip->fd == -1 )
        {   /* until we have the file exclusively to ourselves */
            if ((fp->ip->fd = e2open(fp->ip->fname,fm,cmode)) < 0)
            {
                return (E2_FILE *) NULL;
            }
#ifndef AIX
#ifndef MINGW32
            if (fm != O_RDONLY && fcntl(fp->ip->fd,F_SETLK,&fl) == -1)
            {
                close(fp->ip->fd);
                fp->ip->fd = -1;
                sleep(1);
            }
#endif
#endif
        }
#else
        if ((fp->ip->fd = e2open(fp->ip->name,fm,cmode)) < 0)
            return (E2_FILE *) NULL;
#endif
        fp->ip->flags = fm;
        e2log_oadd(fp->ip);    /* Add details to the log file */
    }
    else
    {
        if (fm & O_APPEND)
            fp->fpos = e2lseek(fp->ip->fd,0,SEEK_END);
        else
            fp->fpos = e2lseek(fp->ip->fd,0,SEEK_SET);
                             /* Set to current position       */
    }
    fp->ip->f_cnt++;          /* Increment the reference count */
    fp->fpos = e2lseek(fp->ip->fd,0,SEEK_CUR);
                             /* Set to current position       */
    fp->cur_err = 0;
    fp->eof = 0;
    return(fp);
}
/*
 * Ugly, this. The approach is to fabricate a pseudo-filename that is open.
 */
E2_FILE * e2fdopen(fd,mode)
int fd;
char * mode;
{
E2_INO * ip;
char buf[20];
static int fdcnt;
int fm;

    if (!check_mode(mode,&fm))
    {
        errno = EINVAL;
        return (E2_FILE *) NULL;
    }
    if ((ip = find_i_slot(cur_e2_con)) == (E2_INO *) NULL)
        return (E2_FILE *) NULL;
    (void) sprintf(buf,"/!*e2dme!?!%d", fdcnt++);
    ip->fname = strdup(buf);
    ip->fd = fd;
    ip->flags = fm;
    return e2fopen(buf,mode);
}
/*
 * Search for a buffer.
 * If it is in the cache
 * - If it is in the clean chain, move it to the head.
 * - Return a pointer to it.
 * If the clean chain is empty, write the block on the tail of the write
 * chain; this should give a clean item. 
 * Now take the tail off the clean chain, assign it to this ip, read in
 * the block of data, put the block at the head of the clean chain, and
 * return the pointer.
 */
static E2_BUF_HDR * e2get(ip, buf_no)
E2_INO * ip;
int buf_no;
{
E2_BUF_HDR search_buf, *bp;
HIPT * h;
int i;

restart:
    search_buf.ip = ip;
    search_buf.buf_no = buf_no;
#ifdef DEBUG
    fprintf(stderr,"e2get(%s, fd = %d, buf_no = %d)\n",ip->fname,
         ip->fd,buf_no);
#endif
    if ((h = lookup(ip->conp->buf_hash,&search_buf)) != (HIPT *) NULL)
    {
        bp = (E2_BUF_HDR *) (h->body);
        if (bp->wbp == (char *) NULL && bp != ip->conp->chp)
        {
            e2take_clean(bp);
            e2add_clean_head(bp);
        }
#ifdef DEBUG
        e2bufdump(bp, "e2get");
#endif
        return bp;
    }
/*
 * No clean chain, write one; it goes onto the clean chain.
 */
    if (ip->conp->ctp == (E2_BUF_HDR *) NULL)
        e2put(ip->conp->wtp);
    bp = ip->conp->ctp;
    if (bp != ip->conp->chp)
    {
        e2take_clean(bp);             /* Detach it from the clean chain */
        e2add_clean_head(bp);
    }
    e2fresh(bp,ip,buf_no);            /* Mark it as in use */
    while (e2lseek(ip->fd,buf_no*BUFSIZ, SEEK_SET) != buf_no * BUFSIZ)
    {
        if (errno != EINTR)
        {
            perror("lseek():unexpected error");
            fprintf(stderr,"Error number: %d\n", errno);
            e2bufdump(bp, "e2get");
            abort();
        }
    }
    if ((i = e2read(ip->fd,bp->bp,BUFSIZ)) < 0)
    {
        e2release(bp);
        return (E2_BUF_HDR *) NULL;   /* Error           */
    }
    bp->vp = bp->bp + i;
    if ((i = e2lseek(ip->fd, 0, SEEK_CUR))
                 != buf_no * BUFSIZ + (bp->vp - bp->bp))
    {
        fprintf(stderr,"Warning: lseek() reports incorrect position\n\
 Error number: %d Reported Position: %d Correct Position: %d\n",
               errno, i, ((buf_no * BUFSIZ) + (bp->vp - bp->bp)));
        e2bufdump(bp, "e2get");
        e2release(bp);
        goto restart;
    }
#ifdef DEBUG
    e2bufdump(bp, "e2get data fetched");
#endif
    return bp;
}
/*
 * Read the specified amount of data into the buffer
 */
size_t e2fread(buffer, size, n_els, fp)
char * buffer;
size_t size;
size_t n_els;
E2_FILE * fp;
{
/*
 * Loop; whilst the amount of data read and copied is less than what we
 * want, and we have not had an error.
 * - Work out the block number that the data is in.
 * - Look to see if it is in memory.
 * - If it is not, get it.
 * - Copy the required data from the block to the address 
 * - If there is an error, set the error code
 * Return the number of elements read, or zero if EOF or error.
 */ 
char * out, *x;
int to_do;
int in;
int i;
E2_BUF_HDR * bp;
int buf_no;

    if (fp->ip == (E2_INO *) NULL || fp->ip->fd == -1 )
    {
        errno = EINVAL;
        fp->cur_err = errno;
        return 0;
    }
    for (buf_no = fp->fpos/BUFSIZ,
         to_do = n_els * size,
         out = buffer;
            to_do > 0;
                to_do -= in,
                fp->fpos += in,
                buf_no++)
    {
        if ((bp = e2get(fp->ip,buf_no)) == (E2_BUF_HDR *) NULL)
        {
            fp->cur_err = errno;
            return 0;
        }
        x = bp->bp + (fp->fpos % BUFSIZ);
        in = (bp->vp - x);
        if (in > to_do)
            in = to_do;
        if (in > 0)
            memcpy(out,x,in);
        if (bp->ep != bp->vp && (x + in) >= bp->vp)
        {
            to_do -= in;
            fp->cur_err = 0;
            fp->eof = 1;
            fp->fpos = buf_no * BUFSIZ + (bp->vp - bp->bp);
            return ((n_els * size) - to_do)/size;
        }
        out += in;
    }
    return n_els;
}
/*
 * Read in up to a new line, and terminate with a null character
 */
char * e2fgets ( buffer, size, fp)
char * buffer;
size_t size;
E2_FILE * fp;
{
char * out, *x;
char buf[80];
int to_do;
int i;
int piece;

    for (to_do = size - 1,
         piece = (to_do < sizeof(buf))? to_do : sizeof(buf),
         out = buffer;
             to_do;
                   to_do -= piece)
    {
        if ((i = e2fread(&buf[0],sizeof(char),piece,fp)) == 0)
        {
            if (to_do == (size - 1) || fp->cur_err)
                return (char *) NULL;
            else
            {
                *out = '\0';
                return buffer;
            }
        }
        for (x = &buf[0], out = buffer; i; i--, x++, out++) 
        {
            *out = *x;
            if (*x == '\n')
                break;
        }
        if (i)
        {
            out++;
            *out = '\0';
            fp->fpos -= i - 1;
            fp->eof = 0;
            return buffer;
        }
    }
    *out = '\0';
    return buffer;
}
/*
 * Write data
 */
size_t e2fwrite (buffer, size, n_els, fp)
char * buffer;
size_t size;
size_t n_els;
E2_FILE * fp;
{
register char * out, *x, *t1, *t2;
int to_do;
register int in, in1;
int i;
E2_BUF_HDR * bp;
int buf_no;

    if (fp->ip == (E2_INO *) NULL || fp->ip->fd == -1 )
    {
        errno = EINVAL;
        fp->cur_err = errno;
        return -1;
    }
    if (fp->conp != fp->ip->conp)
    {
        fprintf(stderr,
               "%s:line %d Logic Error: conp values don't match for %s\n",
                __FILE__, __LINE__, fp->ip->fname);
        fprintf(stderr,"cur_e2_con: %x fp->conp: %x fp->ip->conp: %x\n",
                      (long) cur_e2_con, (long) fp->conp, (long) fp->ip->conp);
        abort();
    }
/*
 * First find the appropriate buffer, reading it if necessary. An error
 * presumably means that the file is write-only
 */
    for (buf_no = fp->fpos/BUFSIZ,
         to_do = n_els * size,
         out = buffer;
            to_do > 0;
                to_do -= in,
                fp->fpos += in,
                buf_no++)
    {
        if ((bp = e2get(fp->ip,buf_no)) == (E2_BUF_HDR *) NULL)
        {
/*
 * There must be a clean chain after e2get.
 */
            bp = fp->conp->ctp;
            e2free_clean(bp);
            e2add_clean_head(bp);
            e2fresh(bp,fp->ip,buf_no); /* Set it up for this file and block */
            memset(bp->bp,0,BUFSIZ);   /* Fill it with zeroes */
        }
        x = bp->bp + (fp->fpos % BUFSIZ);
        if (x > bp->vp)
        {
            e2bufdump(bp, "e2fwrite: Writing beyond the end of the block?");
            abort();
        }
        in = (bp->ep - x);
        if (in > to_do)
            in = to_do;
        if (in <= 0)
        {
            e2bufdump(bp, "e2fwrite: Nothing to write?");
            abort();
        }
/*
 * in1 is the number we are going to copy. in is the number of further input
 * characters we have dismissed.
 */
        in1 = in;
/*
 * If the data is not extending the file, reduce the amount logged by not
 * re-logging data that has not changed. First, check the front bit.
 */
        for (; in > 0 && x < bp->vp && *x == *out; in--,  x++, out++);
        in1 = (in1 - in);
        to_do -= in1;
        fp->fpos += in1;
        if (in == 0)
            continue;
        in1 = in;
/*
 * Now, check the trailing bit. We know there must be at least one match.
 */
        t1 = x + in - 1;
        if (t1 < bp->vp)
            for ( t2 = out + in - 1;
                    in1 > 1 && *t1 == *t2;
                        in1--, t1--, t2--);
        if (bp->wbp == (char *) NULL)
        {
            bp->wbp = x;
            e2take_clean(bp);
            e2add_write_head(bp);
            bp->ip->w_cnt++;
        }
        else
        if ( bp->wbp > x)
            bp->wbp = x;
        memcpy(x, out,in1);
        out += in;
        if ((x + in1) > bp->wep)
            bp->wep = x + in1;
        if (bp->wep > bp->vp)
            bp->vp = bp->wep;
    }
    return n_els;
}
/*
 * Yuck; enough for our purposes, however
 */
size_t e2vfprintf(E2_FILE *fp, char *x, va_list arglist)
{
static char fbuf[BUFSIZ];

#ifdef VCC2003
    (void) _vsnprintf(fbuf, BUFSIZ, x, arglist);
#else
    (void) vsnprintf(fbuf, BUFSIZ, x, arglist);
#endif
    return e2fwrite(fbuf, sizeof(char), strlen(fbuf), fp);
}
int e2fprintf( E2_FILE *fp, const char * x, ...)
{
    va_list(arglist);
    va_start(arglist,x);
    return e2vfprintf(fp, x, arglist);
}
/*
 * fputs() analogue
 */
int e2fputs(ptr, fp)
char * ptr;
E2_FILE * fp;
{
    return e2fwrite(ptr, sizeof(char), strlen(ptr), fp);
}
/*
 * fputs() analogue
 */
int e2fputc(c, fp)
char c;
E2_FILE * fp;
{
    return e2fwrite(&c, sizeof(char), 1, fp);
}
/*
 * Seek to a point in the file. Do not need the system calls!?
 */
int e2fseek ( fp, offset, ptrname)
E2_FILE * fp;
long offset;
int ptrname;
{
    if (fp->ip == (E2_INO *) NULL || fp->ip->fd == -1 )
    {
        errno = EINVAL;
        fp->cur_err = errno;
        return -1;
    }
    fp->eof = 0;
    if (e2lseek(fp->ip->fd,offset,ptrname) < 0)
    {
        fp->cur_err = errno;
        return -1;
    }
    else
    {
        fp->fpos = e2lseek(fp->ip->fd,0,SEEK_CUR);
        return 0;
    }
}
/*
 * Seek to the beginning of the file
 */
int e2rewind ( fp)
E2_FILE * fp;
{
    if (fp->ip == (E2_INO *) NULL || fp->ip->fd == -1 )
    {
        errno = EINVAL;
        fp->cur_err = errno;
        return -1;
    }
    fp->eof = 0;
    fp->cur_err = 0;
    if (e2lseek(fp->ip->fd,0,SEEK_SET) < 0)
    {
        fp->cur_err = errno;
        return -1;
    }
    else
    {
        fp->fpos = 0;
        return 0;
    }
}
/*
 * Flush the data for a file.
 */
int e2fflush(fp)
E2_FILE * fp;
{
    if (fp->ip == (E2_INO *) NULL || fp->ip->fd == -1 )
    {
        errno = EINVAL;
        fp->cur_err = errno;
        return -1;
    }
    if (fp->ip->w_cnt > 0)
    {
        e2force(fp->conp, fp->ip);
    }
    if (fp->ip->w_cnt != 0)
    {
        e2inodump(fp->ip, "Logic Error: not all dirty blocks flushed\n");
        abort();
    }
    return 0;
}
int e2fclose(fp)
E2_FILE * fp;
{
    if (fp->ip == (E2_INO *) NULL || fp->ip->fd == -1 )
    {
        errno = EINVAL;
        fp->cur_err = errno;
        return -1;
    }
    e2fflush(fp);
    fp->ip->f_cnt--;          /* Decrement reference count */
    if ( fp->ip->f_cnt == 0)
    {
        e2clean(fp->ip);      /* Discard everything */
        e2close(fp->ip->fd);    /* Close the fd if no more references */
        fp->ip->fd = -1;      /* Mark it as closed           */
    }
    fp->ip = (E2_INO *) NULL; /* Disconnect the E2_FILE from the E2_INO */
    return 0;
}
/*
 * Switches to another control block
 */
void e2switch(ncon)
E2_CON * ncon;
{
    cur_e2_con = ncon;
    return;
}
/*
 * Forces everything to disk, and writes a commit marker in the log file.
 */
void e2commit()
{
    e2force(cur_e2_con,(E2_INO *) NULL);
    e2log_add(cur_e2_con, "C",1);
    e2log_flush(cur_e2_con);
    return;
}
struct log_element {
    char eltype;
    struct log_element * next;
    union eldata {
        struct odata { 
            char * fname;
            int len;
            int flags;
            long handle;
        } od;
        struct wdata { 
            long handle;
            long fpos;
            int wlen;
            char * wdata;
        } wd;
    } eld;
};
/*
 * Read a file open record off the log stream
 */
static struct log_element * e2oalloc(fp)
FILE * fp;
{
struct log_element * x;

    if ((x = (struct log_element *) malloc(sizeof(struct log_element)))
                   == (struct log_element *) NULL)
        return (struct log_element *) NULL;
    x->eltype = 'O';
    x->next = (struct log_element *) NULL;
    if (fread(&(x->eld.od.handle),sizeof(x->eld.od.handle),1,fp) < 1)
    {
        free(x);
        return (struct log_element *) NULL;
    }
    if (fread(&(x->eld.od.flags),sizeof(x->eld.od.flags),1,fp) < 1)
    {
        free(x);
        return (struct log_element *) NULL;
    }
    if (fread(&(x->eld.od.len),sizeof(x->eld.od.len),1,fp) < 1)
    {
        free(x);
        return (struct log_element *) NULL;
    }
    if ((x->eld.od.fname = (char *) malloc(x->eld.od.len)) == (char *) NULL)
    {
        free(x);
        return (struct log_element *) NULL;
    }
    if (fread((x->eld.od.fname),x->eld.od.len,1,fp) < 1)
    {
        free(x->eld.od.fname);
        free(x);
        return (struct log_element *) NULL;
    }
    return x;
}
/*
 * Read a write record off the log stream
 */
static struct log_element * e2walloc(fp)
FILE * fp;
{
struct log_element * x;

    if ((x = (struct log_element *) malloc(sizeof(struct log_element)))
                   == (struct log_element *) NULL)
        return (struct log_element *) NULL;
    x->eltype = 'W';
    x->next = (struct log_element *) NULL;
    if (fread(&(x->eld.wd.handle),sizeof(x->eld.wd.handle),1,fp) < 1)
    {
        free(x);
        return (struct log_element *) NULL;
    }
    if (fread(&(x->eld.wd.fpos),sizeof(x->eld.wd.fpos),1,fp) < 1)
    {
        free(x);
        return (struct log_element *) NULL;
    }
    if (fread(&(x->eld.wd.wlen),sizeof(x->eld.wd.wlen),1,fp) < 1)
    {
        free(x);
        return (struct log_element *) NULL;
    }
    if ((x->eld.wd.wdata = (char *) malloc(x->eld.wd.wlen)) == (char *) NULL)
    {
        free(x);
        return (struct log_element *) NULL;
    }
    if (fread((x->eld.wd.wdata),x->eld.wd.wlen,1,fp) < 1)
    {
        free(x->eld.wd.wdata);
        free(x);
        return (struct log_element *) NULL;
    }
    return x;
}
static void e2chain_discard( cp )
struct log_element * cp;
{
struct log_element * np;

    while (cp != (struct log_element *) NULL)
    {
        np = cp->next;
        if (cp->eltype == 'O')
            free(cp->eld.od.fname);
        else
        if (cp->eltype == 'W')
            free(cp->eld.wd.wdata);
        free(cp);
        cp = np;
    }
    return;
}
/*
 * Routine to roll forwards a log file.
 */
void e2restore(logfp)
FILE * logfp;
{
/*
 * Loop; until EOF
 * Process incoming records.
 * - If it is a C
 *   - Loop through the saved records
 *   - Process them
 * - If O or W,
 *   - Save them
 */
char option;
E2_FILE * e2fp;
HASH_CON * hi_hash;
char * mode;
struct log_element * anchor, * current, *x;

    if ((hi_hash = hash(100,long_hh,icomp)) == (HASH_CON *) NULL)
        return;
    anchor = (struct log_element *) NULL;
    while (fread(&option,sizeof(char),1,logfp) > 0)
    {
        switch (option)
        {
        case 'O':
            if ((x = e2oalloc(logfp)) == (struct log_element *) NULL
                || !check_flags(&mode, x->eld.od.flags) )
            {
                e2chain_discard(anchor);
                cleanup(hi_hash);
                return;
            }
            if (anchor == (struct log_element *) NULL)
                anchor = x;
            else
                current->next = x;
            current = x;
            break;
        case 'W':
            if ((x = e2walloc(logfp)) == (struct log_element *) NULL)
            {
                e2chain_discard(anchor);
                cleanup(hi_hash);
                return;
            }
            if (anchor == (struct log_element *) NULL)
                anchor = x;
            else
                current->next = x;
            current = x;
            break;
        case 'C':
            for (x = anchor; x != (struct log_element *) NULL; x = x->next)
            {
                if (x->eltype == 'O')
                {
                    (void) check_flags(&mode, x->eld.od.flags);
                    if ((e2fp = e2fopen(x->eld.od.fname,mode))
                               == (E2_FILE *) NULL)
                    {
                        perror("e2fopen() failed");
                        fprintf(stderr,
                            "Logic Error: open of %s failed in recovery\n",
                                 x->eld.od.fname);
                        abort();
                    }
                    else
                        insert(hi_hash,x->eld.od.handle,(char *) e2fp);
                }
                else
                if (x->eltype == 'W')
                {
                HIPT * h;
                    if ( (h = lookup(hi_hash,x->eld.wd.handle))
                             == (HIPT *) NULL)
                    {
                        fprintf(stderr,
       "Logic Error: write for unrecognised handle %x on recovery chain\n",
                           x->eld.wd.handle);
                        abort();
                    }
                    if (e2fwrite(x->eld.wd.wdata,sizeof(char),x->eld.wd.wlen,
                              (E2_FILE *) (&(h->body))) < x->eld.wd.wlen)
                    {
                        perror("e2fwrite() failed");
                        fprintf(stderr,
                            "Logic Error: write failed in recovery\n");
                        abort();
                    }
                }
                else   /* Unrecognised type */
                {
                    fprintf(stderr,
                        "Logic Error: unrecognised type %c on recovery chain\n",
                           x->eltype);
                    abort();
                }
            }
            e2commit();
            e2chain_discard(anchor);
            anchor = (struct log_element *) NULL;
        default:
            return;
        }
    }
    e2chain_discard(anchor);
    cleanup(hi_hash);
    return;
}
