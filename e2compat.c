/*
 *    e2compat.c - implementations of otherwise standard UNIX features
 *                 that are missing or broken on some of the releases
 *                 we support.
 *
 *    Copyright (c) E2 Systems, UK, 1993. All rights reserved.
 *
 *    We have some universal routines, the cross-device rename and
 *    strnsave, in order to make sure that the object file is not empty!
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1993\n";
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include "natregex.h"
#ifdef MINGW32
#include <fcntl.h>
#define NEED_SUNDRIES
#ifdef VCC2003
#include <WinSock2.h>
int errno;
#endif
#ifdef LCC
#define NEED_SUNDRIES
#endif
#include <windows.h>
#include <io.h>
#define SLEEP_FACTOR 1000
#else
#define SLEEP_FACTOR 1
#include <sys/param.h>
#include <sys/file.h>
#ifdef AIX
#include <fcntl.h>
#else
#ifndef ULTRIX
#include <sys/fcntl.h>
#endif
#endif
#endif
#ifdef HP7
#define W3NEED
#endif
#ifndef OSF
#ifdef OSF
#define SELNEED
#endif
#endif
#ifdef SCO
#ifdef NOLS
#define GTNEED
#define W3NEED
#endif
#endif
#ifdef ICL
#define SELNEED
#define GTNEED
#endif
#ifdef PTX
#define SELNEED
#define GTNEED
#define W3NEED
#endif
#ifndef ICL
#define e2gettimeofday(x,y) gettimeofday(x,y)
#endif
#ifdef SOLAR
#undef SELNEED
#endif
/**********************************************************************
 * Implementations required for reliable System V.3 signal functions?
 */
#ifdef NT4
#define NO_SIGHOLD
#ifndef MINGW32
#define W3NEED
#include <sys/resource.h>
#else
#ifdef LCC
#define GTNEED
#endif
#ifdef VCC2003
#define GTNEED
#endif
#endif
#endif
#ifdef ULTRIX
#include "ansi.h"
#define NO_SIGHOLD
#endif
#ifdef ANDROID
#define NO_SIGHOLD
#endif
#ifdef NO_SIGHOLD
void (*sigset())();
void sighold();
void sigrelse();
#endif
#ifdef DEBUG
#ifndef DEBUG_ALARM
#define DEBUG_ALARM
#endif
#endif
/*
 * Allocate space for a string, and copy it in
 */
char *
strnsave(s, n)
char    *s;
int    n;
{
    char    *cp;

    if ((cp = (char *) malloc(n + 1)) == (char *) NULL)
        abort();
    (void) strncpy(cp, s, n);
    cp[n] = 0;
    return(cp);
}
/*
 * Generalised form of dyn_note in e2srclib.c
 */
struct mem_con * mem_note(anchor,p)
struct mem_con * anchor;
char * p;
{
struct mem_con * m = (struct mem_con *) malloc(sizeof(struct mem_con));
    m->next = anchor;
    m->owned = p;
    m->special_routine = NULL;
    m->extra_arg = 0;
#ifdef DEBUG
    fprintf(stderr, "anchor:%x note:%x owned:%x\n", anchor, m, p);
#endif
    return m;
}
/*
 * Generalised form of mem_note
 */
struct mem_con * descr_note(anchor,p, special_routine, extra_arg )
struct mem_con * anchor;
char * p;
void (* special_routine)();
int extra_arg;
{
struct mem_con * m = (struct mem_con *) malloc(sizeof(struct mem_con));
    m->next = anchor;
    m->owned = p;
    m->special_routine = special_routine;
    m->extra_arg = extra_arg;
#ifdef DEBUG
    fprintf(stderr, "anchor:%x note:%x owned:%x\n", anchor, m, p);
#endif
    return m;
}
/*
 * Zap the memory bound to a dynamic control block
 */
void mem_forget(anchor)
struct mem_con * anchor;
{
struct mem_con * n, *p;

#ifdef DEBUG
    fprintf(stderr, "mem_forget anchor:%x\n", anchor);
#endif
    for (n = anchor; n != (struct mem_con *) NULL; )
    {
        p = n;
        n = p->next;
#ifdef DEBUG
        fprintf(stderr, "mem_forget mem:%x owned:%x\n", p, p->owned);
#endif
        if (p->special_routine == NULL)
            free(p->owned);
        else
            p->special_routine(p->owned, p->extra_arg);
        free((char *) p);
    }
    return;
}
/*
 * Test for a file's existence, and return its length
 */
int e2getflen(s)
char * s;
{
struct stat stat_buf;

    if (stat(s, &stat_buf) < 0)
        return -1;
    else
        return stat_buf.st_size;
}
#ifdef ULTRIX
char * strdup(x)
char * x;
{
    return strnsave(x, strlen(x));
}
#endif
/*
 * Doctored popen that:
 *   -  Binds stderr as well as stdout on "r"
 *   -  Is able to handle multiple occurrences
 */
#ifndef V32
#ifndef MINGW32
#include <sys/wait.h>
#endif
#else
#define SIGCHLD SIGCLD
#define WEXITSTATUS(x) ((x) >> 8)
#endif
#define NPMAX 10
enum going {GO_NO,GO_YES};
static struct npcntrl {
   enum going going;
   FILE * fp;
   int pid;
#ifdef OSF
    int
#else
#ifdef SCO
    int
#else
#ifdef HP7
    int
#else
#ifdef PTX
    int
#else
#ifdef AIX
    int
#else
#ifdef MINGW32
    int
#else
#ifdef NT4
    int
#else
    union wait
#endif
#endif
#endif
#endif
#endif
#endif
#endif
stat_val;
} npcntrl[NPMAX];

FILE * npopen(comm,mode)
char * comm;
char * mode;
{
int fildes[2];
int child_pid;
short int i;
FILE * fp;
#ifdef MINGW32
int h0;
int h1;
int h2;
STARTUPINFO si;
PROCESS_INFORMATION pi;
char * prog_name;
char * command_line;

    if ((prog_name = getenv("COMSPEC")) == (char *) NULL)
    {
        if ( _osver & 0x8000 )
            prog_name = "C:\\WINDOWS\\COMMAND.COM";
        else
            prog_name = "C:\\WINNT\\SYSTEM32\\CMD.EXE";
    }
    if ((command_line = (char *) malloc(strlen(prog_name)+4+strlen(comm) + 1))
                  == (char *) NULL)
        return (FILE *) NULL;
    sprintf(command_line, "%s /c %s", prog_name, comm); 
    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwX = 0;
    si.dwY = 0;
    si.dwXSize = 0;
    si.dwYSize= 0;
    si.dwXCountChars = 0;
    si.dwYCountChars= 0;
    si.dwFillAttribute= 0;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.wShowWindow = 0;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;
    if (_pipe(&fildes[0], 16384, O_BINARY|O_NOINHERIT))
    {
        fprintf(stderr,"pipe() failed error:%d\n", GetLastError());
        return (FILE *) NULL;
    }
/*
 * Set up the pipe handles for inheritance
 */
    if (*mode == 'w')
    {
        if((h0 = _dup(fildes[0])) < 0)
        {
            fprintf(stderr,"h0 = dup(fildes[0]) failed error:%d\n",
                           GetLastError());
            return (FILE *) NULL;
        }
        close(fildes[0]);
        h1 = 1;
        h2 = 2;
    }
    else
    {
        if ((h1 = _dup(fildes[1])) < 0)
        {
            fprintf(stderr,"h1 = dup(fildes[1]) failed error:%d\n",
                           GetLastError());
            close(fildes[1]);
            close(fildes[0]);
            return (FILE *) NULL;
        }
        if ((h2 = _dup(fildes[1])) < 0)
        {
            fprintf(stderr,"h2 = dup(fildes[1]) failed error:%d\n",
                           GetLastError());
            _close(h1);
            _close(fildes[1]);
            _close(fildes[0]);
            return (FILE *) NULL;
        }
        close(fildes[1]);
        h0 = 0;
    }
/*
 * Restore the normal handles
 */
    si.hStdInput = (HANDLE) _get_osfhandle(h0);
    si.hStdOutput = (HANDLE) _get_osfhandle(h1);
    si.hStdError = (HANDLE) _get_osfhandle(h2);

    if (!CreateProcess(prog_name, command_line, NULL, NULL, TRUE,
                  0, NULL, NULL, &si, &pi))
    {
        fprintf(stderr,"Create process failed error %d\n",GetLastError());
        if (*mode == 'w')
        {
            (void) close(fildes[1]);
            close(h0);
        }
        else
        {
            (void) close(fildes[0]);
            close(h1);
            close(h2);
        }
        free(command_line);
        return (FILE *) NULL;
    }
    if (*mode == 'w')
        close(h0);
    else
    {
        close(h1);
        close(h2);
    }
    CloseHandle(pi.hThread);
    free(command_line);
    child_pid = (int) pi.hProcess;
#else
    if (pipe(fildes) < 0)
        return (FILE *) NULL;
    if ((child_pid=fork()) == 0)
    {     /* Child pid */
static char * args[4] = {"sh","-c"};
        (void) close(0);
        (void) close(1);
        (void) close(2);
        if (!strcmp("r",mode))
        {           /* Readable pipe */
            (void) close(fildes[0]);
            (void) dup2(fildes[1],1);
            (void) dup2(fildes[1],2);
        }
        else
        {
            (void) close(fildes[1]);
            (void) dup2(fildes[0],0);
        }
        args[2] = comm;
        if (execvp("/bin/sh",args) < 0)
        {
            if (!strcmp("r",mode))
            {                      /* possible to return error text */
                perror("Exec failed");
                (void) fprintf(stderr,
                               "Could not execute program %s\n", args[0]);
            }
            exit(1);
        }
    }
    else if (child_pid < 0)
        return (FILE *) NULL;
    else      /* Parent */
#endif
    if (*mode == 'r')
    {           /* Readable pipe */
        (void) close(fildes[1]);
        fp = fdopen(fildes[0],mode);
    }
    else
    {
        (void) close(fildes[0]);
        fp = fdopen(fildes[1],mode);
    }
    for (i = 0; i < NPMAX; i++)
         if (npcntrl[i].pid == 0)
         {
             npcntrl[i].going = GO_YES;
             npcntrl[i].pid = child_pid;
             npcntrl[i].fp = fp;
             break;
         }
    return fp;
}
#ifdef LINUX
typedef void (*sighandler_t)(int);
sighandler_t sigset(int sig, sighandler_t disp);
#endif
/*
 * Corresponding close routine; attempts to wait for the right one
 */
int npclose(fp)
FILE * fp;
{
#ifdef OSF
    int
#else
#ifdef SCO
    int
#else
#ifdef HP7
    int
#else
#ifdef PTX
    int
#else
#ifdef AIX
    int
#else
#ifdef NT4
    int
#else
#ifdef MINGW32
    int
#else
    union wait
#endif
#endif
#endif
#endif
#endif
#endif
#endif
stat_val;
int pid;
int i;
#ifdef MINGW32
    for (i = 0; i < NPMAX; i++)
    {
        if (npcntrl[i].fp == fp)
        {
            if ( (_cwait(&stat_val, npcntrl[i].pid, 0) != -1)
                 || errno == EINTR)
            {
                    npcntrl[i].pid = 0;
                    npcntrl[i].fp = (FILE *) NULL;
                    return stat_val;
            }
        }
    }
    return -1;
#else
int ret_code;
void (*sigfun)();

    sigfun = sigset(SIGCHLD,SIG_DFL);
    ret_code = fclose(fp);
    for (i = 0; i < NPMAX; i++)
    {
        if (npcntrl[i].fp == fp)
        {
            int j;
            if (npcntrl[i].going == GO_NO)
            {
                npcntrl[i].pid = 0;
                npcntrl[i].fp = (FILE *) NULL;
                return WEXITSTATUS(npcntrl[i].stat_val);
            }
            else
            {
                while (((pid = wait(&stat_val)) > 0)
                     || errno == EINTR)
                {
                    if (pid <= 0)
                        continue;
                    if (pid != npcntrl[i].pid)
                    {     /* the wrong one */
                        for (j = 0; j < NPMAX; j++)
                        {
                            if (npcntrl[j].pid == pid)
                            {
                                npcntrl[j].going = GO_NO;
                                npcntrl[j].stat_val = stat_val;
                                break;
                            }
                        }
                        if (j >= NPMAX && sigfun != SIG_DFL &&
                                          sigfun != SIG_IGN)
                            (*sigfun)();    /* Call it if necessary */
                    }
                    else
                    {
                        npcntrl[i].pid = 0;
                        npcntrl[i].fp = 0;
                        return WEXITSTATUS(stat_val);
                    }
                }
            }
        }
    }
    sigset(SIGCHLD,sigfun);    /* Restore original behaviour */
    return ret_code;
#endif
}
#ifdef NO_SIGHOLD
#ifdef MINGW32
static void (*prev_hdlr[NSIG])();
#endif
/*
 * These are implemented with POSIX standard facilities.
 * If these are not available, use sigblock(1 << sig) for sighold(),
 * and use sigsetmask(0) to read the existing mask value, followed by
 * sigsetmask( mask & ~(1 << sig)) to clear it, for sigrelse(), and
 * use sigvec() for sigaction() ( change the names from sa_ to sv_)
 * - sigset()
 * - sighold()
 * - sigrelse()
 */
#define DONTWANTARGS(x)
void (*sigset(sig,handler))()
int sig;
void (*handler)DONTWANTARGS((int sig, int code, struct sigcontext * scp));
{
#ifdef MINGW32
    return signal(sig,handler);
#else
    struct sigaction sv;
    struct sigaction osv;
    sv.sa_handler = handler;
    sv.sa_mask = 0;
#ifdef LINUX
    sv.sa_flags = SA_RESTART;
#else
    sv.sa_flags = 0;
#endif
    sigaction(sig,&sv,&osv);
    return osv.sa_handler;
#endif
}
int sigadjust(sig,how)
int sig;
int how;
{
#ifdef MINGW32
    return 0;
#else
sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask,sig);
    return sigprocmask(how,&mask,(sigset_t *) NULL);
#endif
}
void sighold(sig)
int sig;
{
#ifdef MINGW32
    prev_hdlr[sig] = signal(sig,SIG_IGN);
#else
    sigadjust(sig,SIG_BLOCK);
#endif
    return;
}
void sigrelse(sig)
{
#ifdef MINGW32
    if (prev_hdlr[sig] != NULL)
        signal(sig, prev_hdlr[sig]);
#else
    sigadjust(sig,SIG_UNBLOCK);
#endif
    return;
}
#endif
#ifdef MINGW32
void _invalid_parameter(
   const wchar_t * expression,
   const wchar_t * function, 
   const wchar_t * file, 
   unsigned int line,
   unsigned int * pReserved
)
{
    fputs("Microsoft CRT Invalid Parameter Handler Called\n", stderr);
    return;
}
void __stdcall _CxxThrowException(void * pObject,
    /* _s__ThrowInfo */void const * pObjectInfo)
{
    fputs("Microsoft Exception Handling Called - crashing out\n", stderr);
    exit(1);
}
struct timespec {
        time_t tv_sec;
        long tv_nsec;
};
/*
 * Converts to the millisecond resolution of WIN32 Sleep
 */
int nanosleep(req, rem)
struct timespec * req;
struct timespec * rem;
{
    Sleep(1000 * req->tv_sec + req->tv_nsec/1000000);
    if (rem != NULL)
    {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}
#ifndef SIGALRM
#define SIGALRM SIGBREAK
#endif
#ifdef DONT_RAISE
void read_timeout();
#endif
static long int cs_flag = -1;
static CRITICAL_SECTION cs;
static HANDLE coord_event;
static HANDLE current_timer = INVALID_HANDLE_VALUE;
static int alarm_thread_id;
static void timer_service(s)
void * s;
{
MSG msg;
MSG discard;
int ret;
int alarm_timer_id = 0;

    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
                    /* Make sure this thread has a message queue */
    SetEvent(coord_event);

    while ((ret = GetMessage (&msg, NULL, 0, 0)) != 0)
    {
        if (ret == -1)
        {
#ifdef DEBUG_ALARM
            fprintf(stderr,
               "timer_service() problem, error: %x\n",
                         GetLastError());
            fflush(stderr);
#endif
            msg.message = 0;
            Sleep(100);
            continue;
        }
        TranslateMessage (&msg);
        if (msg.message == WM_TIMER)
        {
#ifdef DEBUG_ALARM
            fputs("alarm fired\n", stderr);
            fflush(stderr);
#endif
            KillTimer(NULL, alarm_timer_id);
            alarm_timer_id = 0;
#ifdef DONT_RAISE
            read_timeout();
#else
            raise(SIGALRM);
#endif
        }
        else
        if (msg.message == WM_USER)
        {
            if (alarm_timer_id != 0)
            {
                KillTimer(NULL, alarm_timer_id);
                alarm_timer_id = 0;
#ifdef DEBUG_ALARM
                fputs("running alarm() cancelled\n", stderr);
                fflush(stderr);
#endif
            }
/*
 * Drain any timer messages
 */
            while (PeekMessage(&discard, NULL, WM_TIMER, WM_TIMER, PM_REMOVE));
/*
 * Get the message to go
 */
            while (PeekMessage(&discard, NULL, WM_USER, WM_USER, PM_REMOVE))
                msg = discard;
            if (msg.wParam != 0)
                alarm_timer_id =
                   SetTimer(NULL, 0, msg.wParam * SLEEP_FACTOR,NULL);
        }
        else
        {
#ifdef DEBUG_ALARM
            fprintf(stderr,
     "Unexpected message %u in timer_service() %s\n", msg.message,
(msg.hwnd == NULL) ? "for thread, so likely dropped on floor" : "for window!?");
            fflush(stderr);
#endif
            DispatchMessage (&msg);
        }
    }
    EnterCriticalSection(&cs);
    CloseHandle(current_timer);
    current_timer = INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&cs);
    return;
}
int alarm(s)
int s;
{

    if (!InterlockedIncrement(&cs_flag))
    {
        InitializeCriticalSection(&cs);
        coord_event = CreateEvent(NULL, 0, 0, NULL);
    }
    EnterCriticalSection(&cs);
    if (current_timer == INVALID_HANDLE_VALUE && s == 0)
    {
        LeaveCriticalSection(&cs);
        return 0;
    }
    else
    if (current_timer == INVALID_HANDLE_VALUE)
    {
        current_timer = CreateThread(NULL, 0,
                      (LPTHREAD_START_ROUTINE) timer_service, (LPVOID) s,
                      0, &alarm_thread_id);
        WaitForSingleObject(coord_event, INFINITE);
    }
    while (!PostThreadMessage(alarm_thread_id,WM_USER,s,0))
    {
#ifdef DEBUG_ALARM
        fprintf(stderr,
                 "alarm(%d) cannot post message to thread: %d error: %x\n",
                          s, alarm_thread_id, GetLastError());
        fflush(stderr);
#endif
        if (GetLastError() == ERROR_INVALID_THREAD_ID)
            break;   /* It will never succeed ... */
        Sleep(100);
    }
    LeaveCriticalSection(&cs);
    return 0;                     /* Does not get the remaining time */
}
/*
 * Re-entrant strtok()
 */
char * strtok_r(start_point, sepstr, got_to)
char * start_point;
char * sepstr;
char **got_to;
{
char * x;
char * x1;

    if (got_to == (char **) NULL
     || (start_point == (char *) NULL
     && (*got_to == (char *) NULL
      || **got_to == (char) '\0')))
        return (unsigned char *) NULL;
    if (start_point != (unsigned char *) NULL)
        start_point = *got_to;
    if (*start_point == '\0')
        return NULL;
    x = start_point + strcspn(start_point, sepstr);
    x1 = x + strspn(x, sepstr);
    while (x < x1)
        *x++ = '\0';
    *got_to = x;
    return start_point;
}
#endif
#ifndef SCO
#ifndef NT4
#ifndef AIX4
#ifndef OSF
/*
 * Home implementation of putenv(), because of interference with home
 * implementation of malloc?
 */
extern char ** environ;
void putenv(x)
char * x;
{
    char * x1, **e;
    int i;
    int lo, ln;
    if ((x1 = strchr(x,'=')) == (char *) NULL)
       return;
    ln = strlen(x);
    for (e = environ,
         i = x1 - x + 1;
             *e != (char *) NULL;
                 e++)
    {
        x1 = *e;
        if (!strncmp(x,x1,i))
        {
            lo = strlen(x1);
            if (ln <= lo)
                strcpy(x1,x);
            else
                *e = strnsave(x,ln);
            break;
        }
    }
    if (*e == (char *) NULL)
    {
        char ** new_env, **n_ptr;
        int n = e - environ +2;
        new_env = (char **) malloc( sizeof(char *) * n);
        for (e = environ,
             n_ptr = new_env;
                  *e != (char *) NULL;
                      *n_ptr ++ = *e ++);
        
        *n_ptr++ = strnsave(x,ln);
        *n_ptr = (char *) NULL;
        environ = new_env;
    }
    return;
}
char * getenv(x)
char * x;
{
    char * x1, *x2, **e;
    int ln;
    if ((x1 = strchr(x,'=')) != (char *) NULL)
        return (char *) NULL;
    ln = strlen(x);
    for (e = environ;
             *e != (char *) NULL;
                 e++)
    {
        x1 = *e;
        if ((x2 = strchr(x1,'=')) == (char *) NULL
          || (x2 -x1 != ln))
            continue;
        if (!strncmp(x,x1,ln))
            return x2+1;
    }
    return (char *) NULL;
}
#endif
#endif
#endif
#endif
#ifdef SELNEED
#include <time.h>
#ifndef NOLS
#include <sys/time.h>
#else
struct timeval {
    int    tv_sec;        /* seconds */
    int    tv_usec;    /* and microseconds */
};
#endif
#include <stropts.h>
#include <poll.h>
/*
 * Implementation of select() needed using poll()
 *
 * This implementation can only handle 32 descriptors at a time.
 *
 * Olivetti XP/9 ATT System V.3.2.2
 * ================================
 * A further problem is that poll() appears only to work on STREAMS.
 *
 * We do not have a solution to this problem, since no means is short of
 * poking around in the kernel of 'peeking' at a non-streams-driver terminal
 * input buffer to see if there is anything there. We cannot therfore easily
 * simulate the effect by looping to see if anything is present.
 */
int select(nfds,readmask,writemask,exceptmask,teout)
int nfds;
unsigned int *readmask;
unsigned int *writemask;
unsigned int *exceptmask;
struct timeval * teout;
{
struct pollfd fds[32];
int i;
unsigned int bit_mask;
unsigned int so_far;
int tout;
unsigned int irm, iwm, iem;
unsigned int rrm, rwm, rem;
int r;
    if (readmask != (unsigned int *) NULL)
        irm = *readmask;
    else
        irm = 0;
    if (writemask != (unsigned int *) NULL)
        iwm = *writemask;
    else
        iwm = 0;
    if (exceptmask != (unsigned int *) NULL)
        iem = *exceptmask;
    else
        iem = 0;
    if (teout == (struct timeval *) NULL)
        tout = -1;          /* Wait until event or interrupt */
    else
        tout  = 1000 * teout->tv_sec + teout->tv_usec/1000;
    if (nfds > 32)
        nfds = 32;
/*
 * Loop through the descriptors in the input masks
 */
    for (so_far = 0, i = 0, bit_mask = 1, fds[0].events = 0, fds[0].revents = 0;
            i < nfds;
                i++, bit_mask = bit_mask << 1)
    {
        if (bit_mask & irm)
            fds[so_far].events |= (POLLIN | POLLPRI); 
        if (bit_mask & iwm)
            fds[so_far].events |= (POLLOUT); 
        if ((bit_mask & irm)
         || (bit_mask & iwm)
         || (bit_mask & iem))
        {
            fds[so_far].fd = i;
#ifdef DEBUG
            fprintf(stderr,"In so_far %d fd %d events %d\n",
                    so_far,i,fds[so_far].events);
#endif
            so_far ++;
            fds[so_far].events = 0;
            fds[so_far].revents = 0;
        }
    }
/*
 * Now call poll() with the passed parameters
 */
    while ((i = poll(fds,so_far,tout) == -1) && errno == EAGAIN);
#ifdef DEBUG
       fprintf(stderr,"poll(fds,%d,%d) returned %d\n",so_far,tout,i);
       fflush(stderr);
#endif
    if (i < 0)
    {
#ifdef DEBUG
       perror("Unexpected poll() error");
#endif
        return i;
    }
    else
    {
        tout = i;
/*
 * Loop through the returned values, checking bits. Store the bits and return
 * the number of descriptors for which anything was set
 */
        for ( i = 0, rrm = 0, rwm = 0, rem = 0, r = 0;
                i < so_far;
                    i++)
        {
#ifdef DEBUG
            fprintf(stderr,"Out so_far %d fd %d events %d\n",
                    i,fds[i].fd,fds[i].revents);
            fprintf(stderr,
"POLLIN:%d POLLPRI:%d POLLHUP:%d POLLNVAL:%d POLLOUT:%d POLLERR:%d\n",
(fds[i].revents & POLLIN) ? 1 : 0,
(fds[i].revents & POLLPRI) ? 1 : 0,
(fds[i].revents & POLLHUP) ? 1 : 0,
(fds[i].revents & POLLNVAL) ? 1 : 0,
(fds[i].revents & POLLOUT) ? 1 : 0,
(fds[i].revents & POLLERR) ? 1 : 0);
#ifdef POLLMSG
            fprintf(stderr,
"POLLRDNORM:%d POLLRDBAND:%d POLLWRNORM:%d POLLWRBAND:%d POLLMSG:%d\n",
(fds[i].revents & POLLRDNORM) ? 1 : 0,
(fds[i].revents & POLLRDBAND) ? 1 : 0,
(fds[i].revents & POLLWRNORM) ? 1 : 0,
(fds[i].revents & POLLWRBAND) ? 1 : 0,
(fds[i].revents & POLLMSG) ? 1 :  0);
#endif
#endif
            bit_mask = 1 << fds[i].fd;
            if (bit_mask & irm)
            {
                if ( fds[i].revents & (POLLIN | POLLPRI| POLLNVAL| POLLHUP)) 
                {
#ifdef DEBUG
                    if ( fds[i].revents &  POLLNVAL) 
                        fprintf(stderr,"Read Not Streams on fd %d events %d\n",
                                i,fds[so_far].revents);
#endif
                    rrm |= bit_mask;
                }
            }
            if (bit_mask & iwm)
            {
                if ( fds[i].revents & (POLLOUT | POLLNVAL | POLLHUP)) 
                {
#ifdef DEBUG
                    if ( fds[i].revents &  POLLNVAL) 
                        fprintf(stderr,"Write Not Streams on fd %d events %d\n",
                                i,fds[so_far].revents);
#endif
                    rwm |= bit_mask;
                }
            }
            if (bit_mask & iem)
            {
                if ( fds[i].revents & (POLLERR | POLLHUP)) 
                    rem |= bit_mask;
            }
            if ((bit_mask & rrm)
             || (bit_mask & rwm)
             || (bit_mask & rem))
                r++;
        }
#ifdef DEBUG
       if (tout != r)
           fprintf(stderr,"Logic Error? poll() returned %d, but found %d\n",
                   tout,r);
       fflush(stderr);
#endif
        if (readmask != (unsigned int *) NULL)
            *readmask = rrm;
        if (writemask != (unsigned int *) NULL)
            *writemask = rwm;
        if (exceptmask != (unsigned int *) NULL)
            *exceptmask = rem;
        return r;
    }
}
#endif
#ifndef LCC
/*
 * extended rename to handle cross device renames
 */
int
lrename(f, t)
char    *f;
char    *t;
{
    char    buf[BUFSIZ];
    int    fout;
    int    fin;
    int    rc;
    int    r;

    if ((r = rename(f, t)) == 0)
            return 0;
    else if (errno == EXDEV)
    {
        (void) unlink(t);
        if ((fin = open(f, O_RDONLY, 0)) < 0)
            return(-1);
        if ((fout = open(t, O_WRONLY|O_CREAT, 0666)) < 0)
        {
            (void) close(fin);
            return(-1);
        }
        for (r = 0 ; (rc = read(fin, buf, sizeof(buf))) > 0 ; )
        {
            if ( write(fout, buf, rc) != rc)
            {
                r = -1;
                break;
            }
        }
        (void) close(fin);
        (void) close(fout);
        if (r < 0)
        {
            (void) unlink(t);
            return(r);
        }
        return(unlink(f));
    }
    else return r;
}
#endif
/*
 * Routine for assembling file names from directory/file sequences
 */
/* VARARGS */
char *
filename(fmt, s1, s2, s3, s4, s5)
char    *fmt, *s1, *s2, *s3, *s4, *s5;
{
    char    buf[BUFSIZ];

    (void) sprintf(buf, fmt, s1, s2, s3, s4, s5);
    return(strnsave(buf, strlen(buf)));
}
/*
 *    compute a random 16 bit value.
 */
unsigned short
rand16()
{
static int first = 0;

    if (!(first))
    {
        first = 1;
        srand(getpid());
    }
#ifndef AIX
    if (sizeof(int) == sizeof(long))
        return((unsigned short)((rand() & 0x00ffff00) >> 8));
#endif
    return( (unsigned short) rand());
}

/*
 *    make a unique file name, and return it.
 */
char *
mkuniq(template)
char    *template;
{
    char    buf[BUFSIZ];

    for (;;)
    {
        (void) sprintf(buf, template, rand16());
        if (access(buf, 0) < 0)
            return(strnsave(buf, strlen(buf)));
    }
}
/*
 *    make a unique file name, and return it.
 */
char *
getnewfile(directory,family)
char    *directory;
char    *family;
{
char    buf[BUFSIZ];

    (void) sprintf(buf, "%s/%s%%.05d", directory,family);
    return(mkuniq(buf));
}
/* return the part of the name after the last '/' in s */
char *
fileof(s)
char    *s;
{
    char    *cp;

    return(((cp = strrchr(s, '/')) == (char *) NULL) ? s : cp + 1);
}
#ifdef GTNEED
#ifndef SELNEED
#include <time.h>
#ifndef NOLS
#ifndef LCC
#ifndef VCC2003
#include <sys/time.h>
#endif
#endif
#else
struct timeval {
    int    tv_sec;        /* seconds */
    int    tv_usec;    /* and microseconds */
};
#endif
#endif
#ifdef MINGW32
#ifdef LCC
struct timeval {
    int    tv_sec;        /* seconds */
    int    tv_usec;    /* and microseconds */
};
struct timezone {
int tz_minuteswest;
int tz_dsttime;
};
#endif
#ifdef VCC2003
struct timezone {
int tz_minuteswest;
int tz_dsttime;
};
#endif
#else
#include <sys/times.h>
#endif
/*
 * Simulate BSD gettimeofday()
 * tz_minuteswest is the only field we ever reference from this structure.
 */
#ifdef PTX
#ifndef PTX4
struct timezone {
int tz_minuteswest;
int tz_dsttime;
};
#endif
#endif
#ifdef NOLS
struct timezone {
int tz_minuteswest;
int tz_dsttime;
};
#endif
#ifdef DO_CLOCK_ADJUST
#include <intrinsics.h>
/*
 * Clock tick calibration
 *
 * Timeval arithmetic functions
 *
 * t3 = t1 - t2
 */
static void tvdiff(t1, m1, t2, m2, t3, m3)
unsigned int * t1;
unsigned int * m1;
unsigned int * t2;
unsigned int * m2;
int * t3;
int * m3;
{
    *t3 = *t1 - *t2; 
    *m3 = *m1 - *m2; 
    if (*m3 < 0)
    {
        *m3 = (*m3) + 1000000;
        (*t3)--;
    }
    return;
}
/*
 * Used for clock tick measurement
 */
static long long scale_factor; 
void tick_calibrate()
{
static int done;
long long bef;
long long aft;
struct timeval t1, t2;
struct timezone z;
char * x;

    if (done == 1)
        return;
    done = 1;
    x = getenv("E2TRAF_SCALE");
    if (x != (char *) NULL)
        scale_factor = (long long) atoi(x);
    else
    {
        gettimeofday(&t1, &z);
        bef = _rdtsc();
        sleep( 10 * SLEEP_FACTOR);
        gettimeofday(&t2, &z);
        aft = _rdtsc();
        tvdiff(&(t2.tv_sec),
               &(t2.tv_usec),
               &(t1.tv_sec),
               &(t1.tv_usec),
               &(t1.tv_sec),
               &(t1.tv_usec));
        scale_factor = ((aft - bef) * ((long long) 1000000))/
                        ((((long long) 1000000) * ((long long) t1.tv_sec)) +
                           ((long long) t1.tv_usec)) ;
        x = (char *) malloc(40);
        sprintf(x, "E2TRAF_SCALE=%.23g", (double) scale_factor);
        (void) putenv(x);
    }
    return;
}
#else
void tick_calibrate() {}
#endif
extern double floor();
/*
 * This routine has the task of improving the granularity of the clock.
 * We have the system clock, which is only updated every half second or so
 * on Windows 95/98, and we have a cycle count since boot time.
 *
 * Over a period of seconds, the two do not agree at all well (or putting it
 * another way, the estimate of the ticks per second is not that good). Thus,
 * the objective is to generally use the definitive system clock, but to use
 * the tick count to distinguish close (and otherwise indistinguishable)
 * readings.
 */
int e2gettimeofday(tv,tz)
struct timeval * tv;
struct timezone *tz;
{
#ifdef MINGW32
static double jan1601 = (double) -11644473600.0;
double secs;
double musec_10;
FILETIME ft;
SYSTEMTIME st;
static TIME_ZONE_INFORMATION tzi;
static int ret_code = -501;
#ifdef LCC
long long bef;
long long aft;
static long long first_long;
static struct timeval first_time;
static struct timeval last_time;
#endif
    if (ret_code == -501)
        ret_code = GetTimeZoneInformation(&tzi);    
    GetSystemTime(&st);
#ifdef DO_CLOCK_ADJUST
    aft = _rdtsc();              /* Corresponding Machine Cycles since boot */ 
#endif
    SystemTimeToFileTime(&st,&ft);
#ifdef DEBUG
    printf("%x:%x\n",  ft.dwHighDateTime, ft.dwLowDateTime);
#endif
    musec_10 =(((double) ft.dwHighDateTime) * ((double) 65536.0) *
           ((double) 65536.0) + ((double)((unsigned long) ft.dwLowDateTime))) +
                        jan1601*((double) 10000000.0);
    secs =  floor(musec_10/((double) 10000000.0));
    tv->tv_sec = (int) secs;
    tv->tv_usec = (int) floor((musec_10 -secs*((double) 10000000.0))/
                                 ((double) 10.0));
#ifdef DO_CLOCK_ADJUST
/*
 * Make a fine grained adjustment to the clock
 */
    if (scale_factor != 0)
    {
/*
 * If we have gone a long time since the last timing point, remember
 * where we are and do not adjust the timing.
 *
 * On an NT box, the clock granularity is about 1 millisecond. On a 95/98 box,
 * the clock granularity is about 1/2 a second. So, on 95, we will
 * resynchronise after 600 seconds, and on NT, after 1 seconds. 
 */
#ifdef WIN95
        if (tv->tv_sec - last_time.tv_sec > 600)
#else
        if (tv->tv_sec - last_time.tv_sec > 1)
#endif
        {
            first_time = *tv;
            last_time = first_time;
            first_long = aft;
        }
        else
        {
#ifdef DEBUG
            printf("1 gettimeofday() = %u.%06u\n",  tv->tv_sec, tv->tv_usec);
#endif
/*
 * Otherwise, we prefer to use the last timing point, and adjust using the
 * cycle count. What if clock speed slows down as a power saving measure!?
 * bef starts as the number of ticks we should have had since the last
 * resynchronisation if the system's times were to be believed.
 */
            bef = scale_factor * ((long long) (tv->tv_sec - first_time.tv_sec))
           + (scale_factor * ((long long) (tv->tv_usec - first_time.tv_usec)))/
                       ((long long) 1000000);
#ifdef DEBUG
            printf(" gettimeofday() aft %ld bef %ld\n",
                     (aft - first_long), bef);
            if (bef > (long long) 900000000000)
            {
                printf("scale_factor:%.23g tv->tv_sec:%d first_time.tv_sec:%d tv->tv_usec:%d first_time.tv_usec%d\n",
      (double) scale_factor, tv->tv_sec, first_time.tv_sec,
                             tv->tv_usec, first_time.tv_usec);
            } 
#endif
/*
 * Now bef is the difference between the estimated and actual cycle counts
 */
            bef = aft - first_long - bef;
/*
 * Add this discrepancy to the recorded time as a microsecond component and a
 * whole second component.
 */
            tv->tv_usec = ((long) ((((long long) ( bef % scale_factor))*
                          ((long long) 1000000))/ scale_factor)) + tv->tv_usec;
            tv->tv_sec = tv->tv_sec + ((long) ( bef/scale_factor));
#ifdef DEBUG
            printf("2 gettimeofday() = %u.%06d\n",  tv->tv_sec, tv->tv_usec);
#endif
            if (tv->tv_usec < 0)
            {
                tv->tv_sec--;
                tv->tv_usec = 1000000 + tv->tv_usec;
            }
            else
            if (tv->tv_usec >= 1000000)
            {
                tv->tv_sec++;
                tv->tv_usec =  tv->tv_usec - 1000000;
            }
/*
 * Re-synchronise with the current reading, so that systematic errors do not
 * build up.
 */
            if (tv->tv_sec < last_time.tv_sec
              || (tv->tv_sec == last_time.tv_sec
               && tv->tv_usec < last_time.tv_usec))
            {
/*
 * Disaster. We are too far out. Must make this our new starting point.
 */
#ifdef DEBUG
               printf("Out of Order gettimeofday() = %u.%06u\n",
                          tv->tv_sec, tv->tv_usec);
#endif
                tv->tv_sec = last_time.tv_sec;
                tv->tv_usec = last_time.tv_usec;
                first_time.tv_sec = (int) secs;
                first_time.tv_usec =
                       (int) floor((musec_10 -secs*((double) 10000000.0))/
                                 ((double) 10.0));
                last_time = first_time;
                first_long = aft;
            }
            else
                last_time = *tv;
        }
    }
#ifdef DEBUG
    printf("3 gettimeofday() = %u.%06u\n",  tv->tv_sec, tv->tv_usec);
#endif
#endif
    tz->tz_minuteswest = tzi.Bias;
    if (ret_code == TIME_ZONE_ID_DAYLIGHT)
        tz->tz_dsttime = 1;
    else
        tz->tz_dsttime = 0;
    return tz->tz_dsttime;
#else
struct tms tms;
struct tm *tm;
static int dst_flag = 0;
static long first_call = 0;
static long first_time = 0;
long this_time;

    if (first_call == 0)
    {
        first_call = time ( (long *) 0);
        tm = gmtime(&first_call);  /* ensure timezone and altzone have
                                       values */
        dst_flag = tm->tm_isdst;
    }
    this_time = times(&tms);  
    if (first_time == 0)
        first_time = this_time;
    tv->tv_sec = first_call + (this_time - first_time) /100;
    tv->tv_usec = ((this_time - first_time) % 100) * 10000;
    tz->tz_dsttime = dst_flag;
    if (dst_flag)
    {
        tz->tz_minuteswest = altzone/60;
        return 1;
    }
    else
    {
        tz->tz_minuteswest = timezone/60;
        return 0;
    }
#endif
}
#endif
/*
 * Give milliseconds since 1969, as a double, sort of like M$ GetTickCount().
 */
double get_tick_count()
{
struct timeval tv;
/* struct timezone tz; */

    e2gettimeofday(&tv, NULL);
    return (double) ((tv.tv_sec * 1000 + (tv.tv_sec/1000)));
}
#ifdef W3NEED
#ifdef PTX
#define RUNEED
#endif
#ifdef NOLS
#define RUNEED
#endif
#ifdef RUNEED
struct  rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        unsigned int    ru_maxrss;
        unsigned int    ru_ixrss;       /* XXX: 0 */
        unsigned int    ru_idrss;       /* XXX: sum of rm_asrss */
        unsigned int    ru_isrss;       /* XXX: 0 */
        unsigned int    ru_minflt;      /* any page faults not requiring I/O */
        unsigned int    ru_majflt;      /* any page faults requiring I/O */
        unsigned int    ru_nswap;       /* swaps */
        unsigned int    ru_inblock;     /* block input operations */
        unsigned int    ru_oublock;     /* block output operations */
        unsigned int    ru_msgsnd;      /* messages sent */
        unsigned int    ru_msgrcv;      /* messages received */
        unsigned int    ru_nsignals;    /* signals received */
        unsigned int    ru_nvcsw;       /* voluntary context switches */
        unsigned int    ru_nivcsw;      /* involuntary " */
};
#endif
#ifdef PTX
#include <sys/procstats.h>
int wait3( pidstatus, hang_state,res_used)
int * pidstatus;
int hang_state;
struct rusage * res_used;
{
struct process_stats ps, cpsb, cpsa;
struct timeval tv;
int r;
    if (res_used != (struct rusage *) NULL)
    {
        get_process_stats(&tv,PS_SELF,&ps, &cpsb);
    }
    if ((r = waitpid(-1,pidstatus,hang_state)) > 0)
    {
        if (res_used != (struct rusage *) NULL)
        {
            get_process_stats(&tv,PS_SELF,&ps, &cpsa);
            res_used->ru_utime.tv_sec = cpsa.ps_utime.tv_sec -
                                         cpsb.ps_utime.tv_sec;
            res_used->ru_utime.tv_usec = cpsa.ps_utime.tv_usec -
                                         cpsb.ps_utime.tv_usec;
            res_used->ru_stime.tv_sec = cpsa.ps_stime.tv_sec -
                                         cpsb.ps_stime.tv_sec;
            res_used->ru_stime.tv_usec = cpsa.ps_stime.tv_usec -
                                         cpsb.ps_stime.tv_usec;
            res_used->ru_maxrss = cpsa.ps_maxrss;
            res_used->ru_ixrss = 0;
            res_used->ru_idrss = 0;
            res_used->ru_isrss = 0;
            res_used->ru_minflt = cpsa.ps_reclaim - cpsb.ps_reclaim;
            res_used->ru_majflt = cpsa.ps_pagein - cpsb.ps_pagein;
            res_used->ru_nswap = cpsa.ps_swap - cpsb.ps_swap; /* swaps */
            res_used->ru_inblock = cpsa.ps_bread - cpsb.ps_bread;
                                                 /* block input operations */
            res_used->ru_oublock = cpsa.ps_bwrite - cpsb.ps_bwrite;
                                                 /* block output operations */
            res_used->ru_msgsnd = 0;
                                                  /* messages sent */
            res_used->ru_msgrcv = 0;
                                                  /* messages received */
            res_used->ru_nsignals = cpsa.ps_signal - cpsb.ps_signal;
                                                /* signals received */
            res_used->ru_nvcsw = cpsa.ps_volcsw - cpsb.ps_volcsw;
                                         /* voluntary context switches */
            res_used->ru_nivcsw = cpsa.ps_involcsw - cpsb.ps_involcsw;
                                         /* involuntary " */
        }
    }
    return r;
}
#else
#ifdef HP7
#include <sys/resource.h>
#endif
#ifndef GTNEED
#ifndef MINGW32
#include <sys/times.h>
#endif
#endif
int wait3( pidstatus, hang_state,res_used)
int * pidstatus;
int hang_state;
struct rusage * res_used;
{
static struct tms tma,tmb;
struct tms ps;
struct timeval tv;
int r;
    if (res_used != (struct rusage *) NULL)
    {
        times(&tma);
    }
    if ((r = waitpid(-1,pidstatus,hang_state)) > 0)
    {
        times(&tmb);
        if (res_used != (struct rusage *) NULL)
        {
            res_used->ru_utime.tv_sec = tmb.tms_utime - tma.tms_utime;
            res_used->ru_utime.tv_usec = 0;
            res_used->ru_stime.tv_sec = tmb.tms_stime - tma.tms_stime;
            res_used->ru_stime.tv_usec = 0;
            res_used->ru_maxrss = 0;
            res_used->ru_ixrss = 0;
            res_used->ru_idrss = 0;
            res_used->ru_isrss = 0;
            res_used->ru_minflt = 0;
            res_used->ru_majflt = 0;
            res_used->ru_nswap = 0;
            res_used->ru_inblock = 0;
                                                 /* block input operations */
            res_used->ru_oublock = 0;
                                                 /* block output operations */
            res_used->ru_msgsnd = 0;
                                                  /* messages sent */
            res_used->ru_msgrcv = 0;
                                                  /* messages received */
            res_used->ru_nsignals = 0;
                                                /* signals received */
            res_used->ru_nvcsw = 0;
                                         /* voluntary context switches */
            res_used->ru_nivcsw = 0;
                                         /* involuntary " */
        }
    }
    return r;
}
#endif
#endif
#ifdef NT4
int mkfifo(pipe_name,access_mode)
char * pipe_name;
int access_mode;
{
    return _open_osfhandle((long int) CreateNamedPipe(pipe_name, 
             PIPE_ACCESS_DUPLEX | FILE_FLAG_WRITE_THROUGH,
             PIPE_TYPE_BYTE | PIPE_WAIT, 
             1,
             16384, 16384, 100,
            (LPSECURITY_ATTRIBUTES) NULL), access_mode);
}
#endif
#ifdef MINGW32
#ifdef UNTHREADSAFE
char * ctime_r(tp, bp)
time_t * tp;
char * bp;
{
    return strcpy(bp, ctime(tp));
}
#endif
#endif
#ifdef NEED_SUNDRIES
/*
 * POSIX memory mapping calls. These are loosely based on the pukka versions,
 * in order to minimise portation difficulties.
 *
 * In POSIX:
 *
 * access_flags has bits:
 * - PROT_EXEC
 * - PROT_READ
 * - PROT_WRITE
 * - PROT_NONE
 *
 * private_shared has bits
 * - MAP_FIXED  (this address only)
 * - MAP_SHARED
 * - MAP_PRIVATE
 *
 * In Windows:
 *
 * access_flags to CreateFileMapping() must be one of:
 * - PAGE_READONLY
 * - PAGE_READWRITE
 * - PAGE_WRITECOPY
 * optionally OR'ed with
 * - SEC_COMMIT
 * - SEC_IMAGE (use the properties of the executable mapped to set attributes)
 * - SEC_NOCACHE
 * - SEC_RESERVE
 *
 * access_flags to MapViewOfFile() must be one of
 * - FILE_MAP_READ
 * - FILE_MAP_WRITE
 * - FILE_MAP_ALL_ACCESS (same as write)
 * - FILE_MAP_COPY (copy-on-write)
 *
 * This implementation:
 * - ignores MAP_FIXED, and the desired start address
 * - will only reliably unmap a single address at a time
 */
#define PROT_READ 1
#define PROT_WRITE 2
static HANDLE hmap;
void * mmap(void * advise_start, int len, int access_flags,
           int private_shared, int fd, size_t offset)
{
void * ret;

    if (access_flags & PROT_WRITE)
        access_flags = PAGE_READWRITE;
    else
        access_flags = PAGE_READONLY;
    if ((hmap = CreateFileMapping((HANDLE) _get_osfhandle(fd),
            NULL, access_flags, 0, offset + len, NULL)) == NULL)
        return (void *) -1;
    if (access_flags == PAGE_READWRITE)
        access_flags = FILE_MAP_WRITE;
    else
        access_flags = FILE_MAP_READ;
    if ((ret = MapViewOfFile(hmap,
            access_flags, 0, offset, len)) == NULL)
        return (void *) -1;
    else
        return ret;
}
int munmap(void * map_base, int map_length)
{
    UnmapViewOfFile(map_base);
    CloseHandle(hmap);
    return 0;
} 
#ifdef VCC2003
/*
 * We could possibly use stricmp() as drop-in replacements?
 */
int strcasecmp(x1_ptr, x2_ptr)
char * x1_ptr;
char * x2_ptr;
{
int i;
int j;

    for (;(*x1_ptr != '\0' && *x2_ptr != '\0');
                  x1_ptr++, x2_ptr++)
    {
                              /* Find where they differ */
        if (islower(*x1_ptr))
            i = toupper(*x1_ptr);
        else
            i = *x1_ptr;
        if (islower(*x2_ptr))
            j = toupper(*x2_ptr);
        else
            j = *x2_ptr;
        if (i > j)
            return 1;
        else
        if (j > i)
            return -1;
    }
    if (*x1_ptr != '\0')
        return 1;
    else
    if (*x2_ptr != '\0')
        return -1;
    else
        return 0;
}
int strncasecmp(x1_ptr, x2_ptr, k)
char * x1_ptr;
char * x2_ptr;
int k;
{
int i;
int j;

    for (;(*x1_ptr != '\0' && *x2_ptr != '\0') && k > 0;
                  x1_ptr++, x2_ptr++, k--)
    {
                              /* Find where they differ */
        if (islower(*x1_ptr))
            i = toupper(*x1_ptr);
        else
            i = *x1_ptr;
        if (islower(*x2_ptr))
            j = toupper(*x2_ptr);
        else
            j = *x2_ptr;
        if (i > j)
            return 1;
        else
        if (j > i)
            return -1;
    }
    if (!k)
        return 0;
    else
    if (*x1_ptr != '\0')
        return 1;
    else
    if (*x2_ptr != '\0')
        return -1;
    else
        return 0;
}
#endif
#endif
#ifdef SOLAR
#ifndef SOL10
#include <math.h>
long powl(f, f1)
double f;
double f1;
{
    return (long) pow(f,f1);
}
long expl(f)
double f;
{
    return (long) exp(f);
}
long logl(f)
double f;
{
    return (long) log(f);
}
long log10l(f)
double f;
{
    return (long) log10(f);
}
long floorl(f)
double f;
{
    return (long) floor(f);
}
#endif
#endif
