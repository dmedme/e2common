/*
 * oneproc.c - Program to dump out a vaguely ps-like listing for the children
 *             of a given process
 *
 * Used to:
 * - Find the root process (sicomms, natrun, ptydrive, whatever)
 * - Thereafter, to report on its process hierarchy, easily.
 *
 * Universal version.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1992";
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <errno.h>
#define FRAC_USECS 100000000.0
#ifndef GENSHIFT
#define GENSHIFT PGENSHIFT
#define u_comm U_comm
#define u_start U_start
#define u_ru U_ru
#define u_cru U_cru
#endif
#include <sys/time.h>
#include <sys/dir.h>
#include <sys/var.h>
#define p_osptr p_siblings
#define p_cptr p_child
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/acct.h>
#ifndef AHZ
#define AHZ 64
#endif
#include <stdio.h>
#include <nlist.h>
struct	nlist nl[] = {
{"proc"},
{"v"},
{"u"},
{NULL}
};
static long find_thing();      /* routine to position in the image */
void dump_nl();
void do_one_proc();
void print_ru();
void print_proc();
unsigned long proc_search();
long int int_dump();
static int root_pid;     /* Control output of sibling processes */
/*****************************************************************************
 * The units in the rusage structure. On some systems, they are in pages
 * (clicks). On others, they are in K.
 */
#define MEMTOK ctob(1)/1024
static int fdk;
/**************************************
 *
 * Main process starts here
 *
 *VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 */
main(argc,argv)
int argc;
char ** argv;
{
int fdm;
int i;
/*
 * Get the positions of the interesting things from vmunix
 */
    if (argc < 2)
    {
        (void) fprintf(stderr,"Must provide a list of PIDS\n");
        exit(1);
    }
    if (knlist(nl, sizeof(nl)/sizeof(struct nlist), sizeof(struct nlist))
                   < 0)
    {
        perror("Failed to look up the /vmunix name list");
        exit(1);
    }
    if ((fdk = open("/dev/kmem",O_RDONLY)) < 0)
    {
        perror("Failed to open the kernel memory");
        exit(1);
    }
    if ((fdm = open("/dev/mem",O_RDONLY))< 0)
    {
        perror("Failed to open real memory");
        exit(1);
    }
/*
 * The Process table in all its glory
 */
    for (i = 1; i < argc; i++)
    {
        do_one_proc(fdm,atoi(argv[i]));
    }
    (void) close(fdm);
    (void) close(fdk);
    exit(0);
}
/*
 * Routine to limit the variants for reading kernel memory on different
 * systems
 */
static void kvm_read(fd,loc,addr,len)
int fd;
unsigned long loc;
char * addr;
int len;
{
    lseek(fd,(loc & 0x7fffffff),0);
    if (loc & 0x80000000)
        readx(fd,addr,len,TRUE);
    else
        read(fd,addr,len);
    return;
}
/*
 * Routine to position us to the interesting place in the UNIX kernel
 * The code copes with the peculiar way in which the different UNIX
 * flavours handle the initial underscore.
 */
static long find_thing(thing)
char * thing;
{
int i;
    for(i = sizeof(nl)/sizeof(struct nlist) - 1; i > -1; i--)
    {
        if (nl[i].n_name != (char *) NULL)
        {
        char * x, *x1;
            if (*thing == '_')
            {
                x = nl[i].n_name;
                if (nl[i].n_name[0] == '_')
                    x1 = thing;
                else
                    x1 = thing + 1;
            }
            else
            {
                x1 = thing;
                if (nl[i].n_name[0] == '_')
                    x = nl[i].n_name +1;
                else
                    x = nl[i].n_name;
            }
            if ( !strcmp(x,x1))
            {
                return nl[i].n_value;
            } 
        } 
    }
    (void) fprintf(stderr,"Failed to find the %s\n",thing); 
    return 0;
}
static void dump_nl()
{
int i;
    (void) printf("Name List Offsets\n");
    (void) printf("=================\n");
    for(i = sizeof(nl)/sizeof(struct nlist) - 1; i > -1; i--)
    if (nl[i].n_name != (char *) NULL)
    {
        printf("%-20.20s %-20.1u\n",
           nl[i].n_name, nl[i].n_value);
    } 
    return;
}
/*
 * Routine to find a single integer value
 */
static long int_dump(thing)
char * thing;
{
    int value;
    if (!(value = find_thing(thing)))
        return -1;
    kvm_read(fdk,value,&value,sizeof(value));
    return value;
}
/*
 * The proc table in all its glory
 */
void do_one_proc(fdm,pid)
int fdm;
int pid;
{
unsigned long loc;
int i;
unsigned long procNPROC;
unsigned long proc;
    struct var v;
    proc = (unsigned long) find_thing("proc");
    procNPROC = (unsigned long) find_thing("v");
    kvm_read(fdk,procNPROC, &v,sizeof(v));
    procNPROC = (unsigned long) v.ve_proc;
    root_pid = pid;     /* Stop it outputting siblings for root process */
printf("%-6.6s%-7.7s%-7.7s%-6.6s%-8.8s%-8.8s%-8.8s%-9.9s%-9.9s%-9.9s\n",
    "p_pid",
    "p_ppid",
    "p_pgrp",
    "p_sid",
    "p_tsize",
    "p_dsize",
    "p_ssize",
    "p_rssize",
    "Command",
    "Started");
    printf("%-22.22s %7.7s %7.7s %10.10s %10.10s %7.7s %7.7s\n",
                "Description",
                "User",
                "System",
                "ISMS",
                "IUDMS",
                "Blk.In",
                "Blk.Out");
/*
 * We know where init is
 */
    if (pid == 1)
        loc = proc + sizeof(struct proc);
    else
    {
        loc = proc + (sizeof(struct proc)) * ((pid != 1) ?
    	((root_pid & 0x7ffff00) >> GENSHIFT) : 1);
    }
    loc = proc_search((long) loc,(long) proc, (long) procNPROC,pid);
    if (loc == 0)
       (void) fprintf(stderr,"Pid %d does not exist\n",pid);
    else
        print_proc(fdm,(long) loc,(long) proc, (long) procNPROC);
    return;
}
/*
 * Recursively print out the proc structures starting from the
 * given one.
 */
void print_proc(fdm,loc,bot,top)
int fdm;
unsigned long loc;
unsigned long bot;
unsigned long top;
{
struct proc proc_buf;
int i;
struct user * up;
struct user u;
long u_addr;
    kvm_read(fdk,loc,&proc_buf,sizeof(proc_buf));
printf("%-6.1u%-7.1u%-7.1u%-6.1u%-8.1u%-8.1u%-8.1u%-9.1u",
        (int) proc_buf.p_pid,
        (int) proc_buf.p_ppid,
        (int) proc_buf.p_pgrp,
        (int) proc_buf.p_sid,
        (long) proc_buf.p_size * MEMTOK, 0, 0, 0
        );
    if (proc_buf.p_stat == SZOMB)
    {
        struct rusage res_stuff;
        memset((char *) &res_stuff,0,sizeof(res_stuff));
        printf("\n");
        res_stuff = proc_buf.xp_ru;
        print_ru(res_stuff,"Zombie");
    }
    up = (struct user *) NULL;
    u_addr = find_thing("u");
    if ( lseek(fdm,u_addr,0) < 0)
    {
        perror("/dev/mem lseek() failed");
    }
    readx(fdm,&u, sizeof(u),proc_buf.p_adspace);
    up = &u;
    if (up != (struct user *) NULL
    )
    {
        printf("%-8.8s %-8.8s\n", up->u_comm, ctime(&(up->u_start))+11);
        print_ru(up->u_ru,"This Process Resource");
        print_ru(up->u_cru,"Dead Children Resource");
    }
    else
        printf("U lookup failed: p_addr: %x u_addr: %x\n",
                               (long ) (proc_buf.p_adspace),
                               u_addr
              );
    if ((unsigned long) proc_buf.p_cptr >= bot &&
             (unsigned long) proc_buf.p_cptr < top)
        print_proc(fdm,(unsigned long) proc_buf.p_cptr, bot,top);
    if ( proc_buf.p_pid != root_pid &&
        (unsigned long) proc_buf.p_osptr >= bot &&
                   (unsigned long) proc_buf.p_osptr < top)
        print_proc(fdm,(unsigned long) proc_buf.p_osptr, bot,top);
    return;
}
/*
 * Find a PID
 */
unsigned long proc_search(loc,bot,top,pid)
unsigned long loc;
unsigned long bot;
unsigned long top;
int pid;
{
struct proc proc_buf;
unsigned long start_loc;
    start_loc = loc;
    for (;;)
    {
        if (loc < bot || loc >=top)
            return 0;
        kvm_read(fdk, loc, &proc_buf,sizeof(proc_buf));
        if (proc_buf.p_pid == pid)
            return loc;
        loc += sizeof(proc_buf);
        if (loc >= top)
            loc = bot;
        else
        if (loc == start_loc)
            break;
    }
    return 0;
}
void print_ru (res_stuff,desc)
struct rusage res_stuff;
char * desc;
{
    if ( (double)((double) res_stuff.ru_utime.tv_sec
                + ((double) res_stuff.ru_utime.tv_usec)/FRAC_USECS) != 0.0 ||
                 (double)((double) res_stuff.ru_stime.tv_sec 
                + ((double) res_stuff.ru_stime.tv_usec)/FRAC_USECS) != 0.0 ||
           res_stuff.ru_ixrss != 0 ||
           res_stuff.ru_idrss != 0 ||
           res_stuff.ru_inblock != 0 ||
           res_stuff.ru_oublock != 0)
    {
        double user_time;
        double sys_time;
        double floor();
        int comb_ticks;
        user_time = (double)((double) res_stuff.ru_utime.tv_sec
                           + ((double) res_stuff.ru_utime.tv_usec)/FRAC_USECS);
                                       /* user time used */
        sys_time = (double)((double) res_stuff.ru_stime.tv_sec 
                           + ((double) res_stuff.ru_stime.tv_usec)/FRAC_USECS);
                                       /* system time used */
        comb_ticks = (int) floor(((double) AHZ) * (user_time + sys_time));
        if (comb_ticks == 0)
            comb_ticks = 1;
        printf("%-22.22s %7.6g %7.6g %10.1u %10.1u %7.1u %7.1u\n",
                     desc,
                     user_time,
                     sys_time,
                     res_stuff.ru_ixrss * MEMTOK/comb_ticks,
                                         /* integral shared memory size */
                     res_stuff.ru_idrss * MEMTOK/comb_ticks,
                                        /* integral unshared data " */
                     res_stuff.ru_inblock, /* block input operations */
                     res_stuff.ru_oublock); /* block output operations */
   }
    return;
}
