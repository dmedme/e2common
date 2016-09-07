#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifdef DUP_NOT_BEHAVING
#ifdef _NHANDLE_
#undef _NHANDLE_
#endif
/*
 * Value hallowed by tradition (actually it is rather unfair; 2048 is the
 * real number ...)
 */
#endif
#ifndef _NHANDLE_
#define _NHANDLE_ 20
#endif
/*
 * Need to provide an implementation of opendir(), readdir(), closedir()
 */
#include <windows.h>
#include <io.h>
#ifndef S_ISDIR
#define S_ISDIR(x) (x & S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(x) (x & S_IFREG)
#endif
struct dirent {
    struct _finddata_t f;
    int read_cnt;
    long handle;
};
typedef struct dirent DIR;
#define d_name f.name

DIR * opendir(fname)
char * fname;
{
char buf[260];
DIR * d = (struct dirent *) malloc(sizeof(struct dirent));

    strcpy(buf, fname);
    strcat(buf, "/*");
    if ((d->handle = _findfirst(buf, &(d->f))) < 0)
    {
        fprintf(stderr,"Failed looking for %s error %d\n",
                          buf, GetLastError());
        fflush(stderr);
        free((char *) d);
        return (DIR *) NULL;
    }
    d->read_cnt = 0;
    return d;
}
struct dirent * readdir(d)
DIR * d;
{
    if (d->read_cnt == 0)
    {
        d->read_cnt = 1;
        return d;
    }
    if (_findnext(d->handle, &(d->f)) < 0)
        return (struct dirent *) NULL;
    return d;
}
void closedir(d)
DIR *d;
{
    _findclose(d->handle);
    free((char *) d);
    return;
}
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* We need to provide this, actually
#include <dirent.h>
*/
#include <errno.h>
/*
 * Activate this stuff
 */
#define open e2open
#define close e2close
#define read e2read
#define write e2write
#define lseek e2lseek

/*****************************************************************************
 * Provide more or less direct access to the WIN32 File handling functions,
 * in order to get round the ridiculous limits on the number of simultaneously
 * opened files using the industry standard interface.
 * - We limit ourselves to 4 GByte files.
 * - There is no buffering beyond any that win32 deigns to provide.
 * - Reported errors are likely to be win32 codes.
 */
int e2open(nm,acc,modes)
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
long e2lseek(fh, pos, whence)
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
int e2read(fh,ptr,len)
int fh;
void * ptr;
unsigned int len;
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
int e2write(fh, ptr, len)
int fh;
const void * ptr;
unsigned int len;
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
int e2close(fh)
int fh;
{
    return CloseHandle((HANDLE) fh);
}
/****************************************************************************
 * Overcome limitations on the numbers of open files.
 *
 * Re-direct calls to the stdio library to the appropriate routines. Leaves
 * stdin, stdout and stderr as they were (with Microsoft standard).
 *
 * Absolutely unbuffered outside win32.
 */
static struct std_fun {
    HMODULE hcrtdll;
    FILE * (*fopen)(const char * fname, const char * access);
    FILE * (*fdopen)(int fd, const char * access);
    int (*fclose)(FILE * fp);
    int (*fflush)(FILE * fp);
    int (*fputc)(char c, FILE * fp);
    int (*fputs)(const char *ptr, FILE * fp);
    char * (*fgets)(char * ptr, int maxlen, FILE * fp);
    int (*fgetc)(FILE * fp);
    long (*ftell)(FILE * fp);
    int (*ferror)(FILE * fp);
    size_t (*fread)(const void *, size_t sz, size_t n_els, FILE * fp);
    size_t (*fwrite)(const void *, size_t sz, size_t n_els, FILE * fp);
    long (*fseek)(FILE * fp, long whence, int where);
    int (*vfprintf)(FILE * fp, const char * fmt, va_list arglist);
    int (*dup)(int fd1);
    int (*dup2)(int fd1, int fd2);
} std_fun;
static void ini_lib()
{
#ifdef USE_CRTDLL
    if ((std_fun.hcrtdll = GetModuleHandle("crtdll.dll")) == (HMODULE) NULL) 
         std_fun.hcrtdll = LoadLibrary("crtdll.dll");
#else
    if ((std_fun.hcrtdll = GetModuleHandle("msvcrt.dll")) == (HMODULE) NULL) 
         std_fun.hcrtdll = LoadLibrary("msvcrt.dll");
#endif
    std_fun.fdopen =(FILE *(__cdecl *)(int,const char *))
                      GetProcAddress(std_fun.hcrtdll, "fdopen"); 
    std_fun.fgetc = (int (__cdecl *)(FILE *))
                      GetProcAddress(std_fun.hcrtdll, "fgetc"); 
    std_fun.ferror =  (int (__cdecl *)(FILE *))
                      GetProcAddress(std_fun.hcrtdll, "ferror"); 
    std_fun.fopen = (FILE *(__cdecl *)(const char *,const char *))
                      GetProcAddress(std_fun.hcrtdll, "fopen"); 
    std_fun.fclose = (int (__cdecl *)(FILE *))
                      GetProcAddress(std_fun.hcrtdll, "fclose"); 
    std_fun.fflush =  (int (__cdecl *)(FILE *))
                      GetProcAddress(std_fun.hcrtdll, "fflush"); 
    std_fun.fputc = (int (__cdecl *)(char,FILE *))
                      GetProcAddress(std_fun.hcrtdll, "fputc"); 
    std_fun.fputs = (int (__cdecl *)(const char *,FILE *))
                      GetProcAddress(std_fun.hcrtdll, "fputs"); 
    std_fun.fgets = (char *(__cdecl *)(char *,int,FILE *))
                      GetProcAddress(std_fun.hcrtdll, "fgets"); 
    std_fun.vfprintf =  (int (__cdecl *)(FILE *,const char *,va_list))
                      GetProcAddress(std_fun.hcrtdll, "vfprintf"); 
    std_fun.fwrite =  (size_t (__cdecl *)(const void *,size_t,size_t,FILE *))
                      GetProcAddress(std_fun.hcrtdll, "fwrite"); 
    std_fun.ftell =  (long (__cdecl *)(FILE *))
                      GetProcAddress(std_fun.hcrtdll, "ftell"); 
    std_fun.fseek =  (long (__cdecl *)(FILE *,long,int))
                      GetProcAddress(std_fun.hcrtdll, "fseek"); 
    std_fun.fread = (size_t (__cdecl *)(const void *,size_t,size_t,FILE *))
                      GetProcAddress(std_fun.hcrtdll, "fread"); 
    std_fun.dup =  (long (__cdecl *)(int ))
                      GetProcAddress(std_fun.hcrtdll, "_dup"); 
    std_fun.dup2 =  (long (__cdecl *)(int, int ))
                      GetProcAddress(std_fun.hcrtdll, "_dup2"); 
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
/******************************************************************************
 * stdio library interface that directs key calls to the correct implementation
 * and otherwise uses Win32 File Handles in place of FILE pointers.
 */
FILE * fopen(fn,mode)
const char * fn;
const char * mode;
{
int cmode=0660;
int fm;
int fd;

    if (!check_mode(mode,&fm))
    {
        errno = EINVAL;
        return (FILE *) NULL;
    }
    if ((fd = e2open(fn, fm, cmode)) < 0)
        return (FILE *) NULL;
    else
        return (FILE *) fd;
}
FILE * fdopen(fd,mode)
int fd;
const char * mode;
{
/*
 * If the input value looks like a CRT File Descriptor, return the
 * corresponding Win32 handle.
 */
    if (fd < _NHANDLE_)
        return _get_osfhandle(fd);
    else
        return (FILE *) fd;
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
    return e2close((int) fp);
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
    return 0;
}
#ifdef ferror
#undef ferror
#endif
int ferror(fp)
FILE *fp;
{
    return GetLastError();
}
size_t fread(ptr,s, c, fp)
void * ptr;
size_t s;
size_t c;
FILE *fp;
{
int ret;

    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fread(ptr,s,c, fp);
    }
    ret = e2read((int) fp, ptr,s * c);
    if (ret >= 0)
        return ret;
    else
        errno = GetLastError();
    return 0;
}
#ifdef fileno
#undef fileno
#endif
int fileno(fp)
FILE * fp;
{
    if (fp == stdin)
        return 0;
    else
    if (fp == stdout)
        return 1;
    else
    if (fp == stderr)
        return 2;
    else
        return (int) fp;
}
size_t fwrite(ptr,s, c, fp)
const void * ptr;
size_t s;
size_t c;
FILE *fp;
{
int ret;

    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fwrite(ptr,s,c, fp);
    }

    ret = e2write((int) fp, ptr,s * c);
    if (ret >= 0)
        return ret;
    else
        errno = GetLastError();
    return 0;
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
    return e2lseek((int) fp,s,c);
}
/*
 * Yuck; enough for our purposes, however
 */
size_t e2vfprintf(FILE *fp, char *x, va_list arglist)
{
char fbuf[BUFSIZ];

#ifdef VCC2003
    (void) _vsnprintf(fbuf, BUFSIZ, x, arglist);
#else
    (void) vsnprintf(fbuf, BUFSIZ, x, arglist);
#endif
    return fwrite(fbuf, sizeof(char), strlen(fbuf), fp);
}
int e2fprintf( FILE *fp, const char * x, ...)
{
    va_list(arglist);
    va_start(arglist,x);
    return e2vfprintf(fp, x, arglist);
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
    return e2vfprintf( fp, x, arglist);
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
    return fwrite(ptr, sizeof(char), strlen(ptr), fp);
}
char * fgets(ptr, len, fp)
char * ptr;
int len;
FILE * fp;
{
char *x;

    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fgets(ptr, len, fp);
    }
    x = ptr;
    len--;
    while(len > 0)
    {
        if (fread(x,1,1,fp) <= 0)
        {
            if (x == ptr)
                return (char *) NULL;
            else
            {
                *x = '\0';
                return ptr; 
            }
        }
        if (*x == '\n')
        {
            x++;
            break;
        }
        x++;
        len--;
    }
    *x = '\0';
    return ptr;
}
int fgetc(fp)
FILE * fp;
{
unsigned char x;

    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.fgetc(fp);
    }
    if (fread(&x,1,1,fp) <= 0)
        return EOF;
    else
        return (int) x;
}
long ftell(fp)
FILE * fp;
{
unsigned char x;

    if (fp == stdin || fp == stdout || fp == stderr)
    {
        if (std_fun.hcrtdll == 0)
            ini_lib();
        return std_fun.ftell(fp);
    }
    return e2lseek((int) fp, 0, 1);
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
    return fwrite(&c, 1,1, fp);
}
/*
 * Versions of dup() and dup2() that cope with the passed parameter being
 * a Windows HANDLE rather than a file descriptor
 *
 * If the passed value is not a file descriptor, the returned value isn't
 * a file descriptor either, but a handle.
 *
 * Are errors possible because handles are unsigned?
 */
int dup(fd)
int fd;
{
HANDLE h;

    if (fd < _NHANDLE_
    &&  _get_osfhandle(fd) != INVALID_HANDLE_VALUE)
        return std_fun.dup(fd);
    if ( !(DuplicateHandle(GetCurrentProcess(),
                               (HANDLE) fd,
                               GetCurrentProcess(),
                               (PHANDLE)&h,
                               0L,
                               TRUE,
                               DUPLICATE_SAME_ACCESS)) )
        return -1;
    return _open_osfhandle((long) h, _O_BINARY);
}
int dup2(fd1, fd2)
int fd1;
int fd2;
{
HANDLE h;

    if (fd1 < _NHANDLE_
    &&  _get_osfhandle(fd1) != INVALID_HANDLE_VALUE)
        return std_fun.dup2(fd1, fd2);
    (void) _close(fd2);
    if ( !(DuplicateHandle(GetCurrentProcess(),
                               (HANDLE) fd1,
                               GetCurrentProcess(),
                               (PHANDLE)&h,
                               0L,
                               TRUE,
                               DUPLICATE_SAME_ACCESS)) )
        return -1;
/*
 * The routine for setting these values in the CRT ioinfo array isn't exported,
 * rather annoyingly.
 *
 *  _set_osfhnd(fd2, h);
 */
    if ((fd1 = _open_osfhandle((long) h, _O_BINARY)) < 0)
        return -1;
    if (std_fun.dup2(fd1, fd2) < 0)
    {
        _close(fd1);
        return -1;
    }
    _close(fd1);
    switch (fd2)
    {
    case 0:
        SetStdHandle( STD_INPUT_HANDLE, h);
        break;
    case 1:
        SetStdHandle( STD_OUTPUT_HANDLE, h);
        break;
    case 2:
        SetStdHandle( STD_ERROR_HANDLE, h);
        break;
    }
    return 0;
}
