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
#ifdef ICL
#define _KMEMUSER 1
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <errno.h>
#ifdef AIX
#define FRAC_USECS 100000000.0
#ifndef GENSHIFT
#define GENSHIFT PGENSHIFT
#define u_comm U_comm
#define u_start U_start
#define u_ru U_ru
#define u_cru U_cru
#endif
#else
#define FRAC_USECS 1000000.0
#endif
#ifdef V32
#include <time.h>
#else
#include <sys/time.h>
#endif /* V32 */
#ifndef V4
#include <sys/dir.h>
#endif
#ifdef PTX
#include <signal.h>
#include <sys/mc_vmmac.h>
#include <sys/procstats.h>
struct  rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        long    ru_maxrss;
        long    ru_ixrss;               /* XXX: 0 */
        long    ru_idrss;               /* XXX: sum of rm_asrss */
        long    ru_isrss;               /* XXX: 0 */
        long    ru_minflt;              /* any page faults not requiring I/O */
        long    ru_majflt;              /* any page faults requiring I/O */
        long    ru_nswap;               /* swaps */
        long    ru_inblock;             /* block input operations */
        long    ru_oublock;             /* block output operations */
        long    ru_msgsnd;              /* messages sent */
        long    ru_msgrcv;              /* messages received */
        long    ru_nsignals;            /* signals received */
        long    ru_nvcsw;               /* voluntary context switches */
        long    ru_nivcsw;              /* involuntary " */
};
#include <sys/fcntl.h>
#define PTE_PFNSHIFT PNUMSHFT
#define p_osptr p_sptr
#endif
#ifdef ULTRIX
#include <machine/pte.h>
#include <machine/cpu.h>
#define p_flag p_sched
#else
#ifdef SCO
/*************************************************************************
 * This is very different to the others, which have a BSD core in common
 */
#ifdef V4
#ifndef DGUX
#include <sys/vmparam.h>
#ifndef SOLAR
#ifndef OSF
#include <vm/pte.h>
#ifndef UTS
#include <vm/cpu.h>
#endif
#endif
#endif
#include <sys/vnode.h>
#endif
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/cred.h>
#ifdef DGUX
#include <sys/dg_process_info>
#else
#ifndef OSF
#include <sys/session.h>
#ifndef SOLAR
#include <sys/immu.h>
#endif
#endif
#ifndef UTS
#ifndef OSF
#define NBPPT NBPP
#endif
#endif
#include <sys/fault.h>
#include <sys/syscall.h>
#endif
#include <sys/procfs.h>
#include <sys/stat.h>
#else
#include <sys/var.h>
#include <sys/machdep.h>
#include <sys/mmu.h>
#include <sys/immu.h>
#include <sys/region.h>
#endif
#include <sys/fcntl.h>
/*************************************************************************
 * BSD features not normally present with SCO
 */
#ifdef ZILCH
struct timeval {
        long    tv_sec;         /* seconds */
        long    tv_usec;        /* and microseconds */
};
#endif
#ifdef NILE
#define NWIREDENTRIES 2
#else
#ifndef OSXDC
#ifndef OSF
#ifndef SOLAR
struct  rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        long    ru_maxrss;
        long    ru_ixrss;               /* XXX: 0 */
        long    ru_idrss;               /* XXX: sum of rm_asrss */
        long    ru_isrss;               /* XXX: 0 */
        long    ru_minflt;              /* any page faults not requiring I/O */
        long    ru_majflt;              /* any page faults requiring I/O */
        long    ru_nswap;               /* swaps */
        long    ru_inblock;             /* block input operations */
        long    ru_oublock;             /* block output operations */
        long    ru_msgsnd;              /* messages sent */
        long    ru_msgrcv;              /* messages received */
        long    ru_nsignals;            /* signals received */
        long    ru_nvcsw;               /* voluntary context switches */
        long    ru_nivcsw;              /* involuntary " */
};
#endif
#endif
#endif
#endif
#define p_cptr p_child
#define p_osptr p_sibling
#ifdef V4
#ifndef OSF
#define PGSHIFT PAGESHIFT
#endif
#define PTE_PFNSHIFT MMUSEGSHIFT
#else
#define PG_V PG_P
#define PG_PFNUM PG_ADDR
#define PGSHIFT BPTSHFT
#define PTE_PFNSHIFT PNUMSHFT
#endif
#else
#ifndef SUN
#ifndef AIX
#ifndef HP7
#include <sys/pte.h>
#else
#define PIDHASH(x) (x & PIDHMASK)
#endif
#else
#include <sys/var.h>
#define p_osptr p_siblings
#define p_cptr p_child
#endif
#endif
#endif
#endif
#include <sys/proc.h>
#ifndef SCO
#ifndef AIX
#include <sys/dk.h>
#endif
#endif
#include <sys/user.h>
#include <sys/file.h>
#include <sys/acct.h>
#ifndef AHZ
#ifdef V4
#define AHZ 100
#else
#define AHZ 64
#endif
#endif
#include <stdio.h>
#ifdef SUN
#include <kvm.h>
#else
#ifndef SCO
#ifndef AIX
struct user * getdisku(); /* Read the u area off the swap device */
#endif
#endif
#endif
#ifndef V4
#include <nlist.h>
struct	nlist nl[] = {
#ifdef SCO
{"proc"},
{"v"},
#else
#ifdef AIX
{"proc"},
{"v"},
{"u"},
#else
#ifdef PTX
{"proc"},
{"pidhash"},
{"procNPROC"},
#else
#ifdef HP7
{"proc"},
{"PIDHMASK"},
{"procNPROC"},
#else
{"_proc"},
{"_pidhash"},
{"_procNPROC"},
#endif
#endif
#endif
#endif
{NULL}
};
static long find_thing();      /* routine to position in the image */
#endif /* V4 */
void dump_nl();
void do_one_proc();
void print_ru();
void print_proc();
#ifndef V4
unsigned long proc_search();
#endif
long int int_dump();
static int root_pid;     /* Control output of sibling processes */
/*****************************************************************************
 * The units in the rusage structure. On some systems, they are in pages
 * (clicks). On others, they are in K.
 */
#ifdef PYR
#define MEMTOK 1
#else
#ifdef V4
#define MEMTOK 1
#else
#ifdef SCO
#define MEMTOK PTSIZE/1024
#else
#ifdef HP7
#define MEMTOK 1
#else
#define MEMTOK ctob(1)/1024
#endif
#endif
#endif
#endif
#ifdef SUN
static kvm_t * fdk;
#else
static int fdk;
#endif
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
#ifdef SUN
    if ((fdk = kvm_open("/vmunix",(char *) NULL, "/dev/drum",O_RDONLY,
          (char *) NULL)) < 0)
    {
        perror("Failed to open the kernel memory");
        exit(1);
    }
    if (kvm_nlist(fdk,nl) < 0)
    {
        perror("Failed to look up the /vmunix name list");
        exit(1);
    }
#else
#ifndef V4
#ifdef PTX
    if (nlist("/unix",nl) < 0)
#else
#ifdef HP7
    if (nlist("/hp-ux",nl) < 0)
#else
#ifdef SCO
    if (nlist("/unix",nl) < 0)
#else
#ifdef AIX
    if (nlist("/unix",nl) < 0)
#else
#ifdef AIX
    if (knlist(&nl[0], sizeof(nl)/sizeof(struct nlist), sizeof(struct nlist))
                   < 0)
#else
    if (nlist("/vmunix",nl) < 0)
#endif
#endif
#endif
#endif
#endif
    {
        perror("Failed to look up the /vmunix name list");
        exit(1);
    }
#ifdef DEBUG
    dump_nl();
#endif
#endif /* V4 */
    if ((fdk = open("/dev/kmem",O_RDONLY)) < 0)
    {
        perror("Failed to open the kernel memory");
        exit(1);
    }
#ifndef V4
    if ((fdm = open("/dev/mem",O_RDONLY))< 0)
    {
        perror("Failed to open real memory");
        exit(1);
    }
#endif
#endif /* SUN */
/*
 * The Process table in all its glory
 */
    for (i = 1; i < argc; i++)
    {
        do_one_proc(fdm,atoi(argv[i]));
    }
#ifdef SUN
    (void) kvm_close(fdk);
#else
    (void) close(fdm);
    (void) close(fdk);
#endif
    exit(0);
}
#ifndef SUN
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
#ifdef AIX
    lseek(fd,(loc & 0x7fffffff),0);
    if (loc & 0x80000000)
        readx(fd,addr,len,TRUE);
    else
        read(fd,addr,len);
#else
#ifdef ULTRIX
/*
 * Bizarre; the lseek() and read() can fail when the system is under
 * heavy stress, but succeed again in a moment
 */
    int err_loop = 60;
    do
    {
        if (lseek(fd,loc,0) < 0)
            continue;
        if (read(fd,addr,len) < 0)
            continue;
    }
    while (err_loop-- > 0 && errno != 0);
    if (errno)
    {
        perror("kvm_read error");
        fprintf(stderr,"loc=%u errno=%d\n",loc,errno);
    }
#else
    if (lseek(fd,loc,0) < 0 && errno != 0)
    {
       perror("kvm_read lseek() error");
       fprintf(stderr,"loc=%u errno=%d\n",loc,errno);
    }
    if (read(fd,addr,len) < 0 && errno != 0)
    {
       perror("kvm_read read() error");
       fprintf(stderr,"loc=%u errno=%d\n",loc,errno);
    }
#endif
#endif
    return;
}
#endif
#ifndef SUN
#ifndef SCO
#ifdef HP7
/*
 * Something of a misnomer, this. Finding the u area for HP-UX 9 seems
 * very complicated. Getting it off the disk will be amazing, since the
 * block number on the disk does not seem to be stored in an obvious
 * location. We get as far as the swap map, and then there is no variable
 * that looks like a block number. It must be the page number from the vfd?
 * When the device is just that, should be easy to open it as a character
 * device. But if there is not, how can I attach a file name to the vnode;
 * is there one?
 */
static void  getswapp(p,buf)
struct vfddbd *p;
char *buf;
{
/*
 * Strategy.
 * - Identify the swap devices. These are either file system mount
 *   points, or normal character devices.
 * - Open these devices, and hold them open.
 * - Find the swap tab entry, and hence the file.
 * - Read the block off the disk.
 * - The swap map holds reference count information, and does not seem able
 *   to affect the address on the device.
 * - return
 */
    return;
}
struct user * getdisku(fdm,p)
int fdm;
struct proc * p;
{
preg_t upreg;
reg_t ureg;
chunk_t uchunk;
struct vfddbd *vp;
union {
char d[(((sizeof(struct user) -1)/NBPG + 1) * NBPG)];
struct user uu;
} buf;
char *x;
    kvm_read(fdk, (long)p->p_upreg, &upreg, sizeof(upreg));
    kvm_read(fdk, (long)upreg.p_reg, &ureg, sizeof(ureg));
    kvm_read(fdk, (long)ureg.r_chunk, &uchunk, sizeof(uchunk));
    for (vp = (struct vfddbd *) &uchunk, x = buf.d;
            x < (&(buf.d[0]) + sizeof(buf.d)) &&
            (char *) vp < (char *) (((char *)( &uchunk)) + sizeof(uchunk));
                vp++,x += NBPG)
    {
        if (!(vp->c_vfd.pgm.pg_v))
            getswapp(vp,x);
        else
        {
            long u_addr;
            u_addr = (vp->c_vfd.pgm.pg_pfnum) << PGSHIFT;
            lseek(fdm,u_addr,0);
            read(fdm,x, NBPG);
        }
    }
    return &(buf.uu);
}
#else
#ifndef AIX
/****************************************************************************
 * Get the u area off disk, for those systems that ever put it there, and
 * allow it to be read from a swap pseudo-device.
 */
struct user * getdisku(p)
struct proc * p;
{
static int fds = -1;
#ifdef ULTRIX
struct dmap dmap;
#endif
int ret;
#ifdef PTX
char buf[ctob(UPAGES) + NBPG];
char * align =  (char *) ((((long ) &buf[0] + NBPG)/NBPG)*NBPG);
#endif
int ublk;
static struct user u;
#ifdef PTX
    if ( fds < 0 && ((fds = open("/dev/swap",O_RDONLY))< 0))
#else
    if ( fds < 0 && ((fds = open("/dev/drum",O_RDONLY))< 0))
#endif
    {
        perror("Failed to open swap");
        return (struct user *) NULL;
    }
#ifndef ULTRIX
#ifdef BSD42
    lseek( fds, (long)dtob( p->p_swaddr ), 0 ) ;
#else
#ifdef PTX
    if (lseek( fds, (long)dtob( p->p_swaddr ), 0 ) != (long)dtob(p->p_swaddr))
    {
        perror( "lseek() failed on swap");
        fprintf(stderr,"fds = %d swap address = %u\n",
                       fds,
                       (long) dtob(p->p_swaddr));
    }
#else
    lseek( fds, (long)ctob( p->p_swaddr ), 0 ) ;
#endif
#endif
#else
    kvm_read(fdk, (long)p->p_smap, &dmap, sizeof(struct dmap));
    kvm_read(fdk, (long)dmap.dm_ptdaddr,  &ublk, sizeof(ublk));
    lseek(fds, (long)dtob(ublk), 0);
#endif
#ifdef PTX
/*
 * Swap must be to a page-aligned buffer.
 */
    if ((ret = read( fds,  align, ctob(UPAGES))) != ctob(UPAGES) )
    {
        perror( "Cannot read u area ");
        fprintf(stderr,"read() returned %d, expected %d\n",
                ret, ctob(UPAGES) );
        return ( struct user *) NULL ;
    }
    memcpy((char *) &u, UBTOUSER(align),sizeof(u));
#else
    if ((ret = read( fds, (char*)&u, sizeof( struct user )) )
        != sizeof( struct user ) )
    {
        perror( "Cannot read u area ");
        fprintf(stderr,"read() returned %d, expected %d\n",
            ret, sizeof(struct user));
        return ( struct user *) NULL ;
    }
#endif
    return &u;
}
#endif
#endif
#else
#ifndef V4
static void getdiskp(pg,p)
long pg; 
char *p; 
{
static int fds = -1;
    if ( fds < 1 && ((fds = open("/dev/swap",O_RDONLY))< 0))
    {
        perror("Failed to open swap");
        return;  
    }
    lseek( fds, (long)ptod( pg ), 0 ) ;
    read(fdk, p, NBPP);
    return; 
}
#endif 
#endif
#endif /* SUN */
#ifndef V4
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
#ifdef DEBUG
    printf("%-15.15s = %-10.1u\n",
       thing, value);
#endif
    return value;
}
#endif /* V4 */
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
#ifdef V4
#ifdef DGUX
struct dg_process_info buf;
int dgpkey = DG_PROCESS_INFO_INITIAL_KEY;
    if (!dg_process_info(DG_PROCESS_INFO_SELECTOR_ALL_PROCESSES,
                        pid,
                        DG_PROCESS_INFO_CMD_NAME_ONLY,
                        &dgpkey,
                        &buf,
                        DG_PROCESS_INFO_CURRENT_VERSION))
        loc = 0;
    else
    {
        /* With DG, we need to read in all processes up front, 
           connect them together by messing around with the parent PID's,
           and then go through again looking for the ones of interest.
           struct proc and struct user u are useless, because they contain
           pointers to functions that are callable from within the kernel.
         */
        loc=buf.memory_address_of_process;
    }
#else
static char pid_buf[20];
    struct stat stat_buf;
    sprintf(pid_buf,"/proc/%05d",pid);
    if (stat(pid_buf,&stat_buf) < 0)
        loc = 0;
    else
        loc = pid;
#endif
#else
#ifdef SCO
    struct var v;
    proc = (unsigned long) find_thing("proc");
    procNPROC = (unsigned long) find_thing("v");
    kvm_read(fdk,procNPROC, &v,sizeof(v));
    procNPROC = (unsigned long) v.ve_proc;
#else
#ifdef AIX
    struct var v;
    proc = (unsigned long) find_thing("proc");
    procNPROC = (unsigned long) find_thing("v");
    kvm_read(fdk,procNPROC, &v,sizeof(v));
    procNPROC = (unsigned long) v.ve_proc;
#else
#ifndef HP7
short pidhash[PIDHSZ];
#else
int   PIDHMASK;
#endif
#ifdef HP7
    PIDHMASK =  int_dump("PIDHMASK");
#else
    if ((i = find_thing("pidhash")) == -1)
        return;
    kvm_read(fdk,(unsigned long) i,pidhash,sizeof(pidhash));
#endif 
    proc = (unsigned long) int_dump("proc");
    procNPROC =(unsigned long) int_dump("procNPROC");
#endif
#endif
#endif /* V4 */
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
#ifndef V4
/*
 * We know where init is
 */
    if (pid == 1)
        loc = proc + sizeof(struct proc);
    else
    {
#ifndef SCO
#ifdef AIX
        loc = proc + (sizeof(struct proc)) * ((pid != 1) ?
    	((root_pid & 0x7ffff00) >> GENSHIFT) : 1);
#else
#ifdef HP7
        loc = proc + (sizeof(struct proc)) * PIDHASH(pid);
#else
        loc = proc + pidhash[PIDHASH(pid)] *
                sizeof(struct proc); 
#endif
#endif
#else
        loc = proc;
#endif
    }
    loc = proc_search((long) loc,(long) proc, (long) procNPROC,pid);
#endif   /* V4 */
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
#ifdef SCO
#ifndef SOLAR
#ifdef OSF
   struct {char d[sizeof(struct user)]; struct user uu;} buf;
#else
union {
char d[(((sizeof(struct user) -1)/NBPPT + 1) * NBPPT)];
struct user uu;
} buf;
#endif
#endif
#else
struct user u;
#endif
long u_addr;
#ifdef V4
#ifndef OSF
    struct pid sid;
    struct pid pgrp;
    struct pid pid;
    struct sess sess;
#endif
    char pid_buf[20];
    if (loc == root_pid)
    {
#ifdef SOLAR
        struct prpsinfo prp;
#endif
        sprintf(pid_buf,"/proc/%05d",loc);
        if ((fdm = open(pid_buf,O_RDONLY)) < 0)
            return;
#ifdef SOLAR
        ioctl(fdm,PIOCPSINFO,&prp);
        loc = (unsigned long) prp.pr_addr;
#else
        ioctl(fdm,PIOCGETU,&(buf.uu));
        loc = (unsigned long) buf.uu.u_procp;
        up = &(buf.uu);
#endif
    }
#endif
#ifdef SUN
    struct sess sess;
#endif
    kvm_read(fdk,loc,&proc_buf,sizeof(proc_buf));
#ifdef SOLAR
    up = &(proc_buf.p_user);
#endif
#ifdef SUN
    if (proc_buf.p_sessp == (struct sess *) NULL)
        sess.s_sid = 0;
    else
        kvm_read(fdk,proc_buf.p_sessp,&sess,sizeof(sess));
#else
#ifdef V4
#ifdef OSF
    if (proc_buf.p_pid != root_pid)
    {
        sprintf(pid_buf,"/proc/%05d",pid.p_pid);
        if ((fdm = open(pid_buf,O_RDONLY)) < 0)
            return;
        if (ioctl(fdm,PIOCGETU,&(buf.uu)) < 0)
            up = (struct user *) NULL;
        else
            up = &(buf.uu);
    }
    (void) close(fdm);
#else
    if ((unsigned long) proc_buf.p_sessp != 0)
    {
        kvm_read(fdk,proc_buf.p_sessp,&sess,sizeof(sess));
        kvm_read(fdk,sess.s_sidp, &sid,sizeof(sid));
    }
    else
        sid.pid_id = 0;
    kvm_read(fdk,proc_buf.p_pidp, &pid,sizeof(pid));
    kvm_read(fdk,proc_buf.p_pgidp, &pgrp,sizeof(pgrp));
#ifndef SOLAR
#endif
#endif
#else
#ifdef HP7
    up = getdisku(fdm,&proc_buf);
#endif
#endif /* V4 */
#endif /* SUN */
printf("%-6.1u%-7.1u%-7.1u%-6.1u%-8.1u%-8.1u%-8.1u%-9.1u",
#ifdef OSF
        (int) proc_buf.p_pid,
        (int) proc_buf.p_ppid,
        (int) proc_buf.p_pgrp,
        0,
        0,
        (long) proc_buf.p_brksize * MEMTOK,
        (long) proc_buf.p_stksize * MEMTOK,
        (long) proc_buf.p_rssize * MEMTOK
#else
#ifdef V4
        (int) pid.pid_id,
        (int) proc_buf.p_ppid,
        (int) pgrp.pid_id,
        (int) sid.pid_id,
        0,
        (long) proc_buf.p_brksize * MEMTOK,
        (long) proc_buf.p_stksize * MEMTOK,
#ifdef SOLAR
        0
#else
        (long) proc_buf.p_swrss * MEMTOK
#endif
#else
        (int) proc_buf.p_pid,
        (int) proc_buf.p_ppid,
        (int) proc_buf.p_pgrp,
#ifdef SUN
        (int) sess.s_sid,
#else
#ifdef PYR
        (int) proc_buf.p_sessionid,
#else
        (int) proc_buf.p_sid,
#endif
#endif
#ifdef SCO
        (long) proc_buf.p_size * MEMTOK, 0, 0, 0
#else
#ifdef AIX
        (long) proc_buf.p_size * MEMTOK, 0, 0, 0
#else
#ifdef HP7
        (long) up->u_exdata.ux_tsize * MEMTOK,
        (long) up->u_exdata.ux_dsize * MEMTOK,
        (long) up->u_exdata.ux_bsize * MEMTOK,
        0
#else
#ifdef PTX
        0,
#else
        (long) proc_buf.p_tsize * MEMTOK,
#endif
        (long) proc_buf.p_dsize * MEMTOK,
        (long) proc_buf.p_ssize * MEMTOK,
        (long) proc_buf.p_rssize * MEMTOK
#endif
#endif
#endif
#endif   /* V4 */
#endif   /* OSF */
        );
#ifdef PTX
    if (proc_buf.p_stat == SZOMB)
#else
#ifdef HP7
    if (proc_buf.p_stat == SZOMB)
#else
#ifdef SCO
    if (proc_buf.p_stat == SZOMB)
#else
#ifdef AIX
    if (proc_buf.p_stat == SZOMB)
#else
    if ((long) proc_buf.p_ru != 0)
#endif
#endif
#endif
#endif /* PTX */
    {
        struct rusage res_stuff;
        memset((char *) &res_stuff,0,sizeof(res_stuff));
        printf("\n");
#ifdef PTX
        res_stuff.ru_utime = proc_buf.p_un.pu_pst.ps_utime;
        res_stuff.ru_stime = proc_buf.p_un.pu_pst.ps_stime;
        res_stuff.ru_minflt = proc_buf.p_un.pu_pst.ps_reclaim;
        res_stuff.ru_majflt = proc_buf.p_un.pu_pst.ps_pagein;
        res_stuff.ru_nswap = proc_buf.p_un.pu_pst.ps_swap;
        res_stuff.ru_inblock = proc_buf.p_un.pu_pst.ps_bread;
                                            /* block input operations */
        res_stuff.ru_oublock = proc_buf.p_un.pu_pst.ps_bwrite;
                                            /* block output operations */
        res_stuff.ru_nsignals = proc_buf.p_un.pu_pst.ps_signal;
                                            /* signals received */
        res_stuff.ru_nvcsw = proc_buf.p_un.pu_pst.ps_volcsw;
                                            /* voluntary context switches */
        res_stuff.ru_nivcsw = proc_buf.p_un.pu_pst.ps_involcsw;
                                            /* involuntary " */
#else
#ifndef SCO
#ifdef AIX
        res_stuff = proc_buf.xp_ru;
#else
#ifdef HP7
        kvm_read(fdk,(long) proc_buf.p_rusagep, &res_stuff,sizeof(res_stuff));
        res_stuff.ru_utime = proc_buf.p_utime;
        res_stuff.ru_stime = proc_buf.p_stime;
#else
        kvm_read(fdk,(long) proc_buf.p_ru, &res_stuff,sizeof(res_stuff));
#endif
#endif
#else
        res_stuff.ru_utime.tv_sec = proc_buf.p_utime/100;
        res_stuff.ru_utime.tv_usec =  (proc_buf.p_utime%100) *10000;
        res_stuff.ru_stime.tv_sec = proc_buf.p_stime/100;
        res_stuff.ru_stime.tv_usec =   (proc_buf.p_stime%100) *10000;
#ifndef V4
        res_stuff.ru_ixrss = proc_buf.p_size;
#endif
#endif
#endif   /* ndef SCO */
        print_ru(res_stuff,"Zombie");
    }
#ifndef V4
#ifndef HP7
    up = (struct user *) NULL;
#endif
#ifdef AIX
    u_addr = find_thing("u");
    if ( lseek(fdm,u_addr,0) < 0)
    {
        perror("/dev/mem lseek() failed");
    }
    if ( readx(fdm,&u, sizeof(u),proc_buf.p_adspace) <= 0)
        perror("u-block readx() failed");
    up = &u;
#else
#ifndef SUN
    if (proc_buf.p_flag & SLOAD)
#endif
    {
#ifdef SUN
/*
 * SUN - The routine kvm_getu() does not appear to work
    up = kvm_getu(fdk,&proc_buf);
 */
        kvm_read(fdk,proc_buf.p_uarea,&u, sizeof(u));
        up = &u;
#else
#ifdef PTX
        kvm_read(fdk,proc_buf.p_uarea,&u, sizeof(u));
        up = &u;
#else
#ifdef SCO
        for (i = 0; i < proc_buf.p_usize; i++)
        {
            u_addr = *((long *) &(proc_buf.p_ubptbl[i]));
            if (!(u_addr & PG_V))
                getdiskp(pfnum(u_addr),((char *) &(buf.uu)) + NBPP *i);
            else
            {
                u_addr = (u_addr & PG_PFNUM) << (PGSHIFT - PTE_PFNSHIFT);
                lseek(fdm,u_addr,0);
                read(fdm,((char *) &(buf.uu)) + NBPP *i, NBPP);
            }
            up = &buf.uu;
        }
#else
#ifndef HP7
/*
 * For ULTRIX,
 * p_addr is the address of the page table entries for the u area, unless
 * this is a system process. We just get these wrong.
 */
        kvm_read(fdk,(long) proc_buf.p_addr,&u_addr,sizeof(u_addr));
        if (u_addr & PG_V)
        {
#ifdef PYR
            u_addr = (u_addr & PG_PFNUM);
#else
            u_addr = (u_addr & PG_PFNUM) << (PGSHIFT - PTE_PFNSHIFT);
#endif
            lseek(fdm,u_addr,0);
            read(fdm,&u, sizeof(u)); /* Assumes interesting sizeof(u) < page */
            up = &u;
        }
#endif
#endif
#endif
#endif    /* SUN */
    }
#ifndef SUN
#ifndef SCO
#ifndef HP7
    else
        up = getdisku(&proc_buf);
#endif
#endif
#endif
#endif /* AIX */
#endif /* V4 */
    if (up != (struct user *) NULL
#ifdef PYR
        || u.u_magic != UMAGIC
#endif
    )
    {
#ifdef SCO
        struct rusage res_stuff;
        memset((char *) &res_stuff,0,sizeof(res_stuff));
#else
#ifdef HP7
        struct rusage res_stuff;
        memset((char *) &res_stuff,0,sizeof(res_stuff));
#endif
#endif
#ifdef PTX
        struct rusage res_stuff;
        memset((char *) &res_stuff,0,sizeof(res_stuff));
#endif
#ifdef OSXDC
        printf("%-8.8s %-8.8s\n", proc_buf.p_comm, ctime(&(proc_buf.p_start))+11);
#else
#ifdef HP7
        printf("%-8.8s %-8.8s\n", up->u_comm, ctime(&(proc_buf.p_start))+11);
#else
        printf("%-8.8s %-8.8s\n", up->u_comm, ctime(&(up->u_start))+11);
#endif
#endif
#ifdef SCO
#ifdef V4
        res_stuff.ru_utime.tv_sec = proc_buf.p_utime/100;
        res_stuff.ru_utime.tv_usec =    (proc_buf.p_utime%100) *10000;
        res_stuff.ru_stime.tv_sec = proc_buf.p_stime/100;
        res_stuff.ru_stime.tv_usec =    (proc_buf.p_stime%100) *10000;
#else
        res_stuff.ru_utime.tv_sec = up->u_utime/100;
        res_stuff.ru_utime.tv_usec =    (up->u_utime%100) *10000;
        res_stuff.ru_stime.tv_sec = up->u_stime/100;
        res_stuff.ru_stime.tv_usec =    (up->u_stime%100) *10000;
#endif
        res_stuff.ru_ixrss = up->u_tsize;
        res_stuff.ru_idrss = up->u_dsize;
#ifdef SOLAR
        res_stuff.ru_inblock = proc_buf.p_ru.inblock;
        res_stuff.ru_oublock = proc_buf.p_ru.oublock;
        res_stuff.ru_nswap = proc_buf.p_ru.nswap;
#else
        res_stuff.ru_inblock = up->u_ior;
        res_stuff.ru_oublock = up->u_iow;
        res_stuff.ru_nswap = up->u_iosw;
#endif
        print_ru(res_stuff, "This Process Resource");
        memset((char *) &res_stuff,0,sizeof(res_stuff));
#ifdef V4
        res_stuff.ru_utime.tv_sec = proc_buf.p_cutime/100;
        res_stuff.ru_utime.tv_usec = (proc_buf.p_cutime%100)*10000;
        res_stuff.ru_stime.tv_sec = proc_buf.p_cstime/100;
        res_stuff.ru_utime.tv_usec = (proc_buf.p_cstime%100)*10000;
#else
        res_stuff.ru_utime.tv_sec = up->u_cutime/100;
        res_stuff.ru_utime.tv_usec = (up->u_cutime%100)*10000;
        res_stuff.ru_stime.tv_sec = up->u_cstime/100;
        res_stuff.ru_utime.tv_usec = (up->u_cstime%100)*10000;
#endif
        print_ru(res_stuff, "Dead Children Resource");
#else
#ifdef PTX
        res_stuff.ru_utime = up->u_pst.ps_utime;
        res_stuff.ru_stime = up->u_pst.ps_stime;
        res_stuff.ru_minflt = up->u_pst.ps_reclaim;
        res_stuff.ru_majflt = up->u_pst.ps_pagein;
        res_stuff.ru_nswap = up->u_pst.ps_swap;
        res_stuff.ru_inblock = up->u_pst.ps_bread;
                                            /* block input operations */
        res_stuff.ru_oublock = up->u_pst.ps_bwrite;
                                            /* block output operations */
        res_stuff.ru_nsignals = up->u_pst.ps_signal;
                                            /* signals received */
        res_stuff.ru_nvcsw = up->u_pst.ps_volcsw;
                                            /* voluntary context switches */
        res_stuff.ru_nivcsw = up->u_pst.ps_involcsw;
                                            /* involuntary " */
        print_ru(res_stuff, "This Process Resource");
        memset((char *) &res_stuff,0,sizeof(res_stuff));
        res_stuff.ru_utime = up->u_cpst.ps_utime;
        res_stuff.ru_stime = up->u_cpst.ps_stime;
        res_stuff.ru_minflt = up->u_cpst.ps_reclaim;
        res_stuff.ru_majflt = up->u_cpst.ps_pagein;
        res_stuff.ru_nswap = up->u_cpst.ps_swap;
        res_stuff.ru_inblock = up->u_cpst.ps_bread;
                                            /* block input operations */
        res_stuff.ru_oublock = up->u_cpst.ps_bwrite;
                                            /* block output operations */
        res_stuff.ru_nsignals = up->u_cpst.ps_signal;
                                            /* signals received */
        res_stuff.ru_nvcsw = up->u_cpst.ps_volcsw;
                                            /* voluntary context switches */
        res_stuff.ru_nivcsw = up->u_cpst.ps_involcsw;
                                            /* involuntary " */
        print_ru(res_stuff, "Dead Children Resource");
#else
#ifdef HP7
        kvm_read(fdk,(long) proc_buf.p_rusagep, &res_stuff,sizeof(res_stuff));
        res_stuff.ru_utime = proc_buf.p_utime;
        res_stuff.ru_stime = proc_buf.p_stime;
        print_ru(res_stuff,"This Process Resource");
#else
        print_ru(up->u_ru,"This Process Resource");
#endif
        print_ru(up->u_cru,"Dead Children Resource");
#endif
#endif   /* SCO */
    }
#ifndef V4
#ifndef HP7
    else
        printf("U lookup failed: p_addr: %x u_addr: %x\n",
#ifdef SUN
                               (long ) (loc),
                               (long ) (proc_buf.p_uarea)
#else
#ifdef SCO
                               (long ) (*(long *) (&(proc_buf.p_ubptbl[0]))),
#else
#ifdef PTX
                               (long ) (loc),
#else
#ifdef AIX
                               (long ) (proc_buf.p_adspace),
#else
                               (long ) (proc_buf.p_addr),
#endif
#endif
#endif
                               u_addr
#endif
              );
#endif  /* HP7 */
    if ((unsigned long) proc_buf.p_cptr >= bot &&
             (unsigned long) proc_buf.p_cptr < top)
        print_proc(fdm,(unsigned long) proc_buf.p_cptr, bot,top);
    if ( proc_buf.p_pid != root_pid &&
        (unsigned long) proc_buf.p_osptr >= bot &&
                   (unsigned long) proc_buf.p_osptr < top)
        print_proc(fdm,(unsigned long) proc_buf.p_osptr, bot,top);
#else
#ifdef DEBUG
    printf("Chain details for: %d at %x\n",pid.pid_id, loc);
    printf("proc_buf.p_link : %x\n",(unsigned long) proc_buf.p_link);
    printf("proc_buf.p_child : %x\n",(unsigned long) proc_buf.p_child);
    printf("proc_buf.p_sibling : %x\n",(unsigned long) proc_buf.p_sibling);
    printf("proc_buf.p_rlink : %x\n",(unsigned long) proc_buf.p_rlink);
    printf("proc_buf.p_pglink : %x\n",(unsigned long) proc_buf.p_pglink);
    printf("proc_buf.p_next : %x\n",(unsigned long) proc_buf.p_next);
    printf("proc_buf.p_nextofkin : %x\n",(unsigned long) proc_buf.p_nextofkin);
    printf("proc_buf.p_orphan : %x\n",(unsigned long) proc_buf.p_orphan);
    printf("proc_buf.p_nextorph : %x\n",(unsigned long) proc_buf.p_nextorph);
#endif
    if ((unsigned long) proc_buf.p_cptr > 0)
        print_proc(fdm,(unsigned long) proc_buf.p_cptr, bot,top);
    if ( pid.pid_id != root_pid && (unsigned long) proc_buf.p_osptr > 0)
        print_proc(fdm,(unsigned long) proc_buf.p_osptr, bot,top);
#endif /* V4 */
    return;
}
#ifndef V4
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
#ifdef DEBUG
        else
            printf("stat: %x pid: %u ind: %u\n",
                 proc_buf.p_stat,proc_buf.p_pid, (loc - bot)/sizeof(proc_buf));
#endif
        loc += sizeof(proc_buf);
        if (loc >= top)
            loc = bot;
        else
        if (loc == start_loc)
            break;
    }
    return 0;
}
#endif
void print_ru (res_stuff,desc)
struct rusage res_stuff;
char * desc;
{
#ifdef DEBUG
    printf("User Time/Secs %d.%06d\n",res_stuff.ru_utime.tv_sec
                , res_stuff.ru_utime.tv_usec);
        /* user time used */
    printf("System Time/Secs %d.%06d\n", res_stuff.ru_stime.tv_sec ,
                  res_stuff.ru_stime.tv_usec);
        /* system time used */
    printf("Maximum Memory Used %lu\n",res_stuff.ru_maxrss);
    printf("Integral Shared Memory Used %lu\n",res_stuff.ru_ixrss);
        /* integral shared memory size */
    printf("Integral Data Segment %lu\n",res_stuff.ru_idrss);
        /* integral unshared data " */
    printf("Integral Stack Segment %lu\n",res_stuff.ru_isrss);
                /* integral unshared stack " */
    printf("Page Reclaims %lu\n",res_stuff.ru_minflt);
        /* page reclaims */
    printf("Page Faults %lu\n",res_stuff.ru_majflt);
        /* page faults */
    printf("Swaps %lu\n",res_stuff.ru_nswap);
        /* swaps */
    printf("Block Input Operations %lu\n",res_stuff.ru_inblock);
        /* block input operations */
    printf("Block Output Operations %lu\n",res_stuff.ru_oublock);
            /* block output operations */
    printf("Messages Sent %lu\n",res_stuff.ru_msgsnd);
            /* messages sent */
    printf("Messages Received %lu\n",res_stuff.ru_msgrcv);
        /* messages received */
    printf("Signals Received %lu\n",res_stuff.ru_nsignals);
        /* signals received */
    printf("Voluntary Context Switches %lu\n",res_stuff.ru_nvcsw);
        /* voluntary context switches */
    printf("Involuntary Context Switches %lu\n",res_stuff.ru_nivcsw);
        /* involuntary " */
    return;
#else
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
#endif
    return;
}
