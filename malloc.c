/*
 * malloc.c - Simple-minded nestable memory allocator.
 *
 * Goals
 * =====
 * Contention-free memory allocation under most circumstances, achieved by
 * nesting.
 *
 * The application is a situation where the execution threads are essentially
 * anonymous, and are despatched to perform tasks on demand. Thus, thread-local
 * storage is not appropriate.
 *
 * When an allocator is created, the parent allocation structure would be
 * recorded.
 *
 * Calls to the parent allocator must be protected by mutexes, but calls to
 * the child allocator would not be.
 *
 * Compared with the standard LINUX malloc():
 * -    Allocation is quite a bit slower
 * -    Freeing is MUCH slower
 * -    In programs that do huge numbers of allocations and de-allocations,
 *      and use most of memory, it seems to resist memory exhaustion slightly
 *      better (at least if the tuning parameters are tweaked in a manner
 *      sympathetic to the particular workload). This is chiefly of benefit
 *      when processing huge network traces.
 * -    The e2malloc_base checks slow it down measurably, but they are so useful
 *      for troubleshooting they are activated by default
 *
 * The parameters here allocate about the same amount of space to the index and
 * the unallocated backlog, and search for best-fit in the backlog before going
 * down the free chain. The result is probably 10 times slower than the default.
 *
 * It would be more efficient, I think, if the headers were separate. Then
 * searching for free items would touch fewer pages. We do this with the OS
 * allocation/parent tracking.
 *******************************************************************************
 * You get thread-safe operation if:
 * -    MINGW32 is defined (all our Windows programs are multi-threaded, since
 *      the e2compat.c alarm() implementation uses a second thread).
 * OR
 * -    E2_THREAD_SAFE is defined (in which case we use two mutexes rather than
 *      a single Critical Section) 
 */
static char * sccs_id= "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
#define E2MALLOC_BUILD
#include "submalloc.h"
#ifdef MINGW32
#ifndef InterlockedAnd
#define InterlockedAnd InterlockedAnd_Inline
static __inline LONG InterlockedAnd_Inline(LONG volatile *Target,LONG Set)
{
LONG i;
LONG j;

    j = *Target;
    do
    {
        i = j;
        j = InterlockedCompareExchange(Target,i & Set,i);
    }
    while(i != j);
    return j;
}
#endif
#endif
#ifdef FRAG_TRACKING
static struct b_node * b_node_add();
static struct b_node * b_node_init();
static struct b_node * b_node_balance();
static struct b_node * b_node_remove();
static struct b_node * b_node_pop();
#endif
__inline static char * pick_mid();
static struct e2malloc_base e2malloc_base;
/*
 * Account for parent allocations
 */
static char * add_grab(bite, size)
char * bite;
unsigned long int size;
{
    if (e2malloc_base.last_buc->buc_cnt > 0)
    {
    struct pbite * pgrab;

        pgrab = & e2malloc_base.last_buc->grab[
                    e2malloc_base.last_buc->buc_cnt - 1];
        if (bite == pgrab->pchunk + pgrab->psize)
            pgrab->psize += size;
        else
        if ( e2malloc_base.last_buc->buc_cnt > 31 )
        {
/*
 * Put a HEADER on the piece to keep the diagnostics happy. The caller
 * may have to adjust their size accordingly
 */
        HEADER * hp = (HEADER *) bite;

            hp->h_size = 1 + sizeof(struct pbite)/sizeof(HEADER);
            hp->h_next = (HEADER *) &e2malloc_base;
            e2malloc_base.last_buc->next_buc = (struct pbuc *) (hp + 1);

            e2malloc_base.last_buc = e2malloc_base.last_buc->next_buc;
            e2malloc_base.last_buc->next_buc = NULL;
            e2malloc_base.last_buc->grab[0].pchunk = bite;
            e2malloc_base.last_buc->grab[0].psize = size;
            e2malloc_base.last_buc->buc_cnt = 1;
            bite = (char *) (hp + 1 + hp->h_size);
        }
        else
        {
            pgrab++;
            pgrab->pchunk = bite;
            pgrab->psize = size;
            e2malloc_base.last_buc->buc_cnt++;
        }
    }
    else
    {
        e2malloc_base.root_buc.grab[0].pchunk = bite;
        e2malloc_base.root_buc.grab[0].psize = size;
        e2malloc_base.root_buc.buc_cnt = 1;
    }
    return bite;
}
#ifdef QUICK_REPLACE
/******************************************************************************
 * Performance Enhancement - Remember locations of blocks of various sizes.
 *
 * The first-fit algorithms in this file are OK if all they have to do is
 * allocate, or if there is not much allocated, but they perform poorly if
 * there is a lot of free space, and it becomes fragmented.
 *
 * In the windows version, we take a huge initial piece. This provides
 * everything to start with. Keeping an index of pieces of various sizes turns
 * out not to be worth the candle; it doesn't take long to find space for
 * a new thing anyway. However, finding where to put things back in the free
 * list can be slow, since it involves a sequential scan. In this variant, we
 * keep a hash table of free items that should be spread evenly over the arena,
 * so the sequential scan time should be reduced.
 */
                                      /* Note that the pointer is to the PRIOR
                                       * free item, because of the maintenance
                                       * logic.                              */
#ifdef SOLAR
/*
 * Actually the assumption is 32 or 64 bit big-endian, really
 */
static char barr[] = { 0, 0, 1, 1, 2, 2, 2, 2, 
3,3,3,3,3,3,3,3,
4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
__inline int qpick_ind32(n)
unsigned int n;
{
union {
    unsigned int n;
    unsigned short int s[2];
    unsigned char b[4];
} l;

    l.n = n;
    if (l.s[0])
    {
        if (l.b[0])
            return 24 + barr[l.b[0]];
        else
            return 16 + barr[l.b[1]];
    }
    else
    if (l.b[2])
        return 8 + barr[l.b[2]];
    else
        return barr[l.b[3]];
}
__inline int qpick_ind64(n)
unsigned long int n;
{
union {
    unsigned long n;
    unsigned int w[2];
} l;

    l.n = n;
    if (l.w[0])
        return 32 + qpick_ind32(l.w[0]);
    else
        return qpick_ind32(l.w[1]);
}
#else
#ifdef LINUX
/*
 * Assume AMD64 or x86
 */
__inline long bsr(long x)
{
long r;

/* #ifdef _XBS5_LP64_OFF64 */
#if __WORDSIZE == 64
__asm__(
    "bsrq %1,%0\n\t"
    "jnz 1f\n\t"
    "movq $-1, %0\n\t"
    "1:"
    : "=r" (r)
    : "rm" (x)
); 
#else
__asm__(
    "bsrl %1,%0\n\t"
    "jnz 1f\n\t"
    "movl $-1,%0\n\t"
    "1:"        /* The local symbol branched forwards to above */
    : "=r" (r)  /* Output operands */
    : "rm" (x)  /* Input Operands */
                /* : Clobber list is empty */
);
#endif
#ifdef DEBUG
    if (r < 0 || r > (sizeof(long) << 3)) 
        fprintf(stderr,
#ifdef _XBS5_LP64_OFF64
         "LOGIC ERROR: %#lx:Set bit %#x is outside sane range\n",
#else
         "LOGIC ERROR: %#lx:Set bit %#x is outside sane range\n",
#endif
                     (long) &e2malloc_base, r);
#endif
    return r;
}
#endif
#endif
__inline static void quick_maint();
__inline static HEADER * quick_min();
/*
 * Routine to quickly find a nearby free item. The idea is that these things
 * will be spread over the full range, automatically, regardless of the size
 * of the arena.
 */
__inline static unsigned int quick_find(p)
HEADER * p;
{
long int n = (unsigned long) (((char *) p) - e2malloc_base.alloced_base);
unsigned int i;

    if (n < 0)
        return 0;
    i = qpick_ind(n);                /* Find the top bit                      */
    if (i <= e2malloc_base.quick_bits)
        return n;
    else
        return (n >> (i - e2malloc_base.quick_bits - 1 ))
               &~(1<<(e2malloc_base.quick_bits + 1));
                                     /* Get the top bits into the range */
}
/*
 * Routine to remove ourselves from the table of blocks if we are there.
 */
__inline static void quick_remove(p)
HEADER *p;
{
HEADER **x; 

    e2malloc_base.free_cnt--;
    if (e2malloc_base.quick_table != (HEADER **) NULL)
    {
        x = &(e2malloc_base.quick_table[quick_find(p)]);
        if (*x == p)
        {
#ifdef DEBUG
            fprintf(stderr, "%#lx:quick_remove(%#lx) matched %#lx\n",
                     (long) &e2malloc_base, (long) p, (long) x);
#endif
            *x = (HEADER *) NULL;
        }
#ifdef DEBUG
        else
            fprintf(stderr, "%#lx:quick_remove(%#lx) didn't find target\n",
                     (long) &e2malloc_base, (long) p);
#endif
        x = &(e2malloc_base.quick_table[quick_find(e2malloc_base.freep)]);
        if (*x == (HEADER *) NULL)
        {
            *x = e2malloc_base.freep;
#ifdef DEBUG
            fprintf(stderr, "%#lx:quick_remove() added %#lx at %#lx\n",
                     (long) &e2malloc_base,
                     (long) e2malloc_base.freep, (long) x);
#endif
        }
    }
    return;
}
/*
 * Adjust the size of the hash table
 */
static void quick_rehash()
{
HEADER ** old_base;
HEADER ** old_bound;
HEADER ** x;

    if (e2malloc_base.quick_table == (HEADER **) NULL)
    {
        e2malloc_base.quick_table = &(e2malloc_base.quick_table_1[0]);
        e2malloc_base.quick_bound = &(e2malloc_base.quick_table_1[1 << (START_HASH_BITS + 1)]);
        e2malloc_base.hash_step = (1 << (START_HASH_BITS + 6));
        e2malloc_base.quick_bits =  START_HASH_BITS;
    }
    else
    if ((e2malloc_base.quick_bound - e2malloc_base.quick_table) < 
           (1 << (FINAL_HASH_BITS + 1)))
    {
        old_base = e2malloc_base.quick_table;
        old_bound = e2malloc_base.quick_bound;
        if ( e2malloc_base.quick_table == &(e2malloc_base.quick_table_1[0]))
            e2malloc_base.quick_table = &(e2malloc_base.quick_table_2[0]);
        else
            e2malloc_base.quick_table = &(e2malloc_base.quick_table_1[0]);
        e2malloc_base.quick_bound = e2malloc_base.quick_table +
                                   4 * (old_bound - old_base);
        e2malloc_base.quick_bits += 2;
        e2malloc_base.hash_step <<= 2;
        if (e2malloc_base.hash_step > (1 << (FINAL_HASH_BITS + 5)))
            e2malloc_base.hash_step = 65536L*32768L;
        memset((char *) (e2malloc_base.quick_table), 0, 
                        (((char *) e2malloc_base.quick_bound) -
                          ((char *) e2malloc_base.quick_table)));
        for (x = old_base; x < old_bound; x++)
            if (*x != (HEADER *) NULL)
            {
#ifdef DEBUG
                if (((*x)->h_size == &e2malloc_base)
                  ||  ((*x)->h_next == NULL))
                {
                    (void) fprintf(stderr,
        "LOGIC ERROR: %#lx:Index corrupt; address %#lx is not in free chain\n",
                     (long) &e2malloc_base, (long) (*x));
                    return;
                }
#endif
                e2malloc_base.free_cnt--;
                quick_maint(*x);
            }
    }
    return;
}
/*
 * Routine to add ourselves to the table of free blocks if our slot is free.
 */
__inline static void quick_maint(p)
HEADER *p;
{
HEADER **x; 

    e2malloc_base.free_cnt++;
    if (e2malloc_base.free_cnt > e2malloc_base.hash_step)
        quick_rehash();
    if (e2malloc_base.quick_table != (HEADER **) NULL)
    {
        x = &(e2malloc_base.quick_table[quick_find(p)]);
/*
 * We favour larger addresses over smaller ones because the search will
 * take longer if the addresses are sparser over the larger range
 */
        if (*x + (1 << e2malloc_base.quick_bits) < p)
        {
#ifdef DEBUG
            fprintf(stderr, "%#lx:quick_maint(%#lx) adding at %#lx\n",
                     (long) &e2malloc_base,
                     (long) p, (long) x);
            if (((p)->h_size == &e2malloc_base)
               ||  ((p)->h_next == (HEADER *) &e2malloc_base))
            {
                    (void) fprintf(stderr,
 "LOGIC ERROR: %#lx:Attempted corruption; address %#lx is not in free chain\n",
                     (long) &e2malloc_base, (long) (*x));
                return;
            }
#endif
            *x = p;
        }
    }
    return;
}
/*
 * Routine to find a close-ish start point to a block to be free()'ed
 */
__inline static HEADER * quick_start(p)
HEADER *p;
{
HEADER **x, **lim;

    if (e2malloc_base.quick_table == (HEADER **) NULL)
        return &e2malloc_base.base;
/*
 *  x = &(e2malloc_base.quick_table[quick_find(p)]);
 *  if (*x > p || *x == (HEADER *) NULL)
 *      x--;
 *  else
 *      return *x;
 *  if (x < e2malloc_base.quick_table || *x > p || *x == (HEADER *) NULL)
 *      return &e2malloc_base.base;
 *  else
 *      return *x;
 */
    else
    {
        x = &(e2malloc_base.quick_table[quick_find(p)]);
/*
 *      fprintf(stderr, "Hash: %#x Slot: %d Contents: %#x\n",
 *               (unsigned long int) p,
 *               (unsigned long int)(x - e2malloc_base.quick_table),
 *               (unsigned long int) *x);
 */
        if (*x != (HEADER *) NULL && *x < p)
        {
#ifdef DEBUG
            if ((*x)->h_next == (HEADER *) &e2malloc_base)
                fprintf(stderr,"%#lx:quick_start(): Used space pointer %#lx found at index %u\n",
                     (long) &e2malloc_base,
                    (long) (*x)->h_next,
                   (x - e2malloc_base.quick_table));
#endif
            return *x;
        }
        return quick_min(p, (x - 1));
    }
}
/*
 * Routine to find the highest known free address less than passed value.
 */
__inline static HEADER * quick_min(p, bound)
HEADER *p;
HEADER **bound;
{
HEADER *min_p;

    while (bound >= e2malloc_base.quick_table)
    {
        if (*bound != (HEADER *) NULL)
        {
#ifdef DEBUG
            if ((*bound)->h_next == (HEADER *) &e2malloc_base)
                fprintf(stderr,"%#lx:quick_min(): Used space pointer %#lx found at index %u\n",
                     (long) &e2malloc_base,
                           (*bound)->h_next,
                           (bound - e2malloc_base.quick_table));
#endif
            if ( *bound < p)
                return *bound;
        }
        bound--;
    }
    return &e2malloc_base.base;
}
#endif
#ifdef MINGW32
/***********************************************************************
 * Home grown space management, to ensure maximum compatibility between
 * the UNIX and Windows versions of the E2 Systems code.
 */
#define MEM_COMMIT	4096
#define MEM_DECOMMIT	16384
#define PAGE_READWRITE	4

char * __stdcall VirtualAlloc(
  char * lpAddress,             /* Address of region to reserve or commit */
  int dwSize,                   /* Size of region */
  int flAllocationType,         /* Type of allocation */
  int flProtect                 /* Type of access protection */
);
int __stdcall GetLastError(void);
int __stdcall VirtualFree(
  char * lpAddress,             /* Address of region of committed pages */
  int dwSize,                   /* Size of region                       */
  int dwFreeType                /* Type of free operation               */
);
/*
 * For Windows
 */
static char * sbrk(space_size)
long int space_size;
{
unsigned long real_size;
char * ret;

    if (space_size == 0)
    {
/*
 * Return the top of the last allocation
 */
       if (e2malloc_base.last_buc->buc_cnt < 1)
           return 0;
       else
           return e2malloc_base.last_buc->grab[
                e2malloc_base.last_buc->buc_cnt - 1].pchunk
              + e2malloc_base.last_buc->grab[
                e2malloc_base.last_buc->buc_cnt - 1].psize - 1;
    }
    ret = sbrk(0);
#ifdef POWER_OF_TWO
    if (space_size > 65536L)
    {
        for (real_size = 131072L;
            real_size < space_size;
                real_size = real_size + real_size);
        space_size = real_size; 
    }
#endif
    if ((ret = (char *) VirtualAlloc( ret, space_size,
                          MEM_COMMIT, PAGE_READWRITE)) == (char *) NULL
      && (ret = (char *) VirtualAlloc( NULL, space_size,
                          MEM_COMMIT, PAGE_READWRITE)) == (char *) NULL)
    {
        fprintf(stderr,"VirtualAlloc() failed, error %u\n",GetLastError());
        return (char *) -1;
    }
    if (e2malloc_base.alloced_base == (char *) NULL)
        e2malloc_base.alloced_base = ret;
    return ret;
}
/*
 * The following returns all the space we have taken with VirtualAlloc() in
 * one hit. The complication is that we need to take care not to shoot ourselves
 * from under ourselves, since after the first one the structure is in one
 * of the allocated pieces ... 
 */
void e2osfree()
{   /* return space to OS */
struct pbuc * pb, *pbn;
int i;

     for (pb = &e2malloc_base.root_buc; pb != NULL; pb = pbn)
     {
         pbn = pb->next_buc;
         for (i = pb->buc_cnt - 1; i >= 0; i--)
              (void) VirtualFree(pb->grab[i].pchunk,
                        pb->grab[i].psize,
                        MEM_DECOMMIT);
     }
     e2malloc_base.alloced_base = (char *) NULL;
     e2malloc_base.root_buc.buc_cnt = 0;
     e2malloc_base.root_buc.next_buc = NULL;
     e2malloc_base.last_buc = & e2malloc_base.root_buc;
     return;
}
#else
extern char *sbrk();
#endif
/*
 * Routines to validate the arena. If the dump_flag is OUTPUT, list
 * the state of the arena; what's free, what's in use, and what seems
 * to be present in the in use-bits.
 */
void in_use_check(e2mbp, lower,bound,dump_flag)
struct e2malloc_base * e2mbp;
HEADER * lower;
HEADER * bound;
int dump_flag;
{
HEADER * in_use;
int byte_len;

    if (e2mbp == NULL)
        e2mbp = &e2malloc_base;
    for (in_use = lower;
             in_use < bound;
                   in_use = in_use + 1 + in_use->h_size)
    {
        if (in_use->h_next != (HEADER *) e2mbp)
        {
            (void) fprintf(stderr,
                    "%#lx:Corrupted occupied block at %#lx between %#lx and %#lx\n",
                     (long) e2mbp,
                    (long) in_use, (long) lower, (long) bound);
            return;
        }
        if (in_use->h_size & 0xfffffffff0000000UL)
        {
            (void) fprintf(stderr,
"%#lx:Corrupt occupied block at %#lx between %#lx and %#lx has nonsensical size %d\n",
                     (long) e2mbp,
               (long) in_use, (long) lower, (long) bound, in_use->h_size);
            return;
        }
        byte_len = in_use->h_size * sizeof(HEADER);
        if (byte_len == 0)
        {
            (void) fprintf(stderr,
                    "%#lx:Zero length occupied at %#lx between %#lx and %#lx\n",
                     (long) e2mbp,
                    (long) in_use, (long) lower, (long) bound);
            dump_flag = OUTPUT;
        }
        else
        {
            if ((in_use + in_use->h_size + 1) > bound)
            {
                (void) fprintf(stderr, "%#lx:Malloc arena corruption.\n\
Overlapping malloc items. Occupied at %#lx size %#x overlaps free at %#lx\n",
                     (long) e2mbp,
                         (long) in_use, byte_len, (long) bound);
                (void) fprintf(stderr, "Sample contents: %.50s\n",
                         (char *)(in_use+1));
                dump_flag = OUTPUT;
            }
#ifdef DEBUG_FULL
            if (dump_flag == OUTPUT)
            (void) fprintf(stderr, "%#lx:Occupied at %#lx size %#x Contents: %.*s\n",
                     (long) e2mbp,
                     (long) in_use, byte_len,byte_len,(char *) (in_use + 1));
#endif
        }
        e2mbp->tot_used += in_use->h_size + 1; 
    }
    return;
}
/*
 * Validate, and optionally dump out, the region between two free list
 * elements
 */
void region_check(e2mbp, bot_free,top_free,dump_flag)
struct e2malloc_base * e2mbp;
HEADER * bot_free;
HEADER * top_free;
int dump_flag;
{
int byte_len;

    if (e2mbp == NULL)
        e2mbp = &e2malloc_base;
    byte_len = bot_free->h_size * sizeof(HEADER);
    if (bot_free->h_size & 0xfffffffff0000000 
      && (bot_free + bot_free->h_size + 1) > top_free)
    {
        (void) fprintf(stderr, "%#lx:Malloc arena corruption.\n\
Overlapping free items. Free at %#lx size %#x overlaps free at %#lx\n",
                     (long) e2mbp,
                 (long) bot_free, byte_len, (long) top_free);
    }
    e2mbp->tot_free += bot_free->h_size + 1; 
#ifdef DEBUG_FULL
    if (dump_flag == OUTPUT)
        (void) fprintf(stderr, "%#lx:Free at %#lx size %#x\n",
                     (long) e2mbp,
                 (long) bot_free, bot_free->h_size * sizeof(HEADER));
#endif
    in_use_check(e2mbp, bot_free + bot_free->h_size + 1, top_free, dump_flag);
    return;
}
/*
 * Validate the entire malloc() arena, and tot up the free space
 */
void domain_check(e2mbp, dump_flag)
struct e2malloc_base * e2mbp;
int dump_flag;
{
HEADER * bot_free, *top_free;
struct pbuc * pb;
int i;
/*
 * From wherever base points to the highest free address
 */
#ifdef DEBUG
    fflush(stdout);
    fflush(stderr);
#endif
    if (e2mbp == NULL)
        e2mbp = &e2malloc_base;
    e2mbp->tot_used = 0;
    e2mbp->tot_free = 0;
    for (pb = &e2mbp->root_buc; pb != NULL; pb = pb->next_buc)
    {
        for (i = 0; i < pb->buc_cnt; i++)
        {
            for (bot_free = (HEADER *) pb->grab[i].pchunk,
                 top_free =  (HEADER *) (pb->grab[i].pchunk +
                                          pb->grab[i].psize);
                    bot_free < top_free;)
            {
                while( bot_free < top_free
                 && bot_free->h_next == (HEADER *) e2mbp)
                {
                    if (bot_free->h_size & 0xfffffffff0000000)
                    {
        (void) fprintf(stderr, "%#lx:Malloc arena corruption.\n\
Overlapping free items. Free at %#lx size %#x overlaps free at %#lx\n",
                     (long) e2mbp,
                 (long) bot_free, bot_free->h_size * sizeof(HEADER),
                       (long) top_free);
                    }
                    e2mbp->tot_used += bot_free->h_size + 1; 
                    bot_free += 1 + bot_free->h_size; /* Skip allocated */
                }
                if (bot_free->h_next > bot_free
                  && bot_free->h_next <= top_free)
                {
                    region_check(e2mbp, bot_free, bot_free->h_next,dump_flag);
                    bot_free = bot_free->h_next;
                }
                else
                {
                    region_check(e2mbp, bot_free, top_free, dump_flag);
                    bot_free = top_free;
                }
            }
        }
    }
    return;
}
void default_domain_check()
{
    domain_check(&e2malloc_base, QUIET);
}
/*
 * Free the block pointed at by 'cp'. Put the block into the correct place in
 * the free list, coalescing if possible.
 */
DONT_NEED_TO_SEE void real_free(cp)
char    *cp;
{ HEADER    *hp;
HEADER    *p;
HEADER    *prevp, *nextp;
/* int lcnt=0; */

    if (cp == (char *) NULL)
    {
        fputs("real_free() called with NULL pointer\n", stderr);
        return;
    }

#ifdef DEBUG
    fflush(stdout);
    (void) fprintf(stderr, "%#lx:real_free(%#lx)\n",
                (long) &e2malloc_base, (unsigned long) cp);
/*
 *  if (cp == 0x8146760)
 *      fputs("0x8146760 seen\n", stderr);
 */
    fflush(stderr);
#endif
    hp = ((HEADER *) cp) - 1;
    if (hp->h_next != (HEADER *) &e2malloc_base)
    {
        fprintf(stderr,
"%#lx:real_free(%#lx) but not one of ours or already freed; h_next(%#lx) != &e2malloc_base\n",
          (long) &e2malloc_base,
          (long) (hp->h_next),
          (unsigned long) cp);
        return;               /* Ameliorates corruption also ... */
    }
#ifdef DEBUG_TRACK
    fprintf(stderr,"R:%#lx:%u:%#lx\n", (long) &e2malloc_base, hp->h_size, (unsigned long) cp);
    if (hp->h_size > HIGHWATER || hp->h_size <= 0)
    {
        (void) fprintf(stderr,
       "LOGIC ERROR: %#lx:Unusual address in free %#lx, size: %d, next %#lx\n",
                     (long) &e2malloc_base,
                     (long) cp, hp->h_size, (long) hp->h_next);
        return;
    }
#endif
#ifdef DEBUG
    e2malloc_base.tot_free += hp->h_size + 1;
#endif
#ifdef DEBUG_FULL
    fflush(stdout);
    fprintf(stderr,"%#lx:real_free() returning %d at %#lx, Sample: %20.s\n",
      (long) &e2malloc_base,hp->h_size * sizeof(HEADER),(unsigned long)(cp),cp); 
    fflush(stderr);
#endif
#ifdef QUICK_REPLACE
    if (hp < e2malloc_base.freep)
        e2malloc_base.freep = quick_start(hp);
    else
    if (hp > e2malloc_base.freep->h_next + 512)
    {
/*
 * If we are not already in the right place, use the quick-start list in an
 * attempt to find a better start point.
 */
        p = quick_start(hp);
        if (p > e2malloc_base.freep)
        {
#ifdef DEBUG_FULL
            fprintf(stderr, "%#lxCloser - Wanted: %#lx Had: %#lx Got: %#lx\n",
                     (long) &e2malloc_base,
                        (unsigned long) hp,
                        (unsigned long) e2malloc_base.freep,
                        (unsigned long) p);
#endif
            e2malloc_base.freep = p;
        }
    }
#endif
    prevp = e2malloc_base.freep;
/*
 *  (void) fprintf(stderr, "real_free(%#x, prevp %#x, prevp->h_next %#x)\n",
 *              (unsigned long) hp,
 *              (unsigned long) prevp,
 *              (unsigned long) prevp->h_next);
 */
    for (p = prevp, nextp = p->h_next ;
             hp >= nextp || p >= hp ;
                 prevp=p, p = nextp, nextp = nextp->h_next) 
    {
/*        lcnt++; */
#ifdef DEBUG
        if (p == hp
          || (hp > p && hp < (p + p->h_size + 1))
          ||  (p->h_size == 0 && p != &e2malloc_base.base))
        {
            (void) fprintf(stderr, "LOGIC ERROR. %#lx:free() item already free().\n\
Free at %#lx size %#x contents %.80s within %#lx size %#x\n",
                     (long) &e2malloc_base,
                 (long) hp, hp->h_size * sizeof(HEADER),
                      (long) cp,
                 (long) p, p->h_size * sizeof(HEADER));
            domain_check(&e2malloc_base, OUTPUT);
            return;
        }
        else
        if (p == ( HEADER *) NULL
         ||  (p->h_size == &e2malloc_base)
         ||  (p->h_next == (HEADER *) &e2malloc_base))
        {
            (void) fprintf(stderr, "LOGIC ERROR. %#lx:free block chain corrupt\n",
                     (long) &e2malloc_base);
            domain_check(&e2malloc_base, OUTPUT);
            return;
        }
        else
#endif
        if (p >= nextp && (hp > p || hp < nextp))
            break;
#ifdef DEBUG
        if (((p + p->h_size + 1) > p->h_next) && ( p->h_next > p))
        {
            fflush(stdout);
            (void) fprintf(stderr, "%#lx:Malloc arena corruption.\n\
Overlapping free items. Free at %#lx size %#x overlaps free at %#lx\n",
                     (long) &e2malloc_base,
                 (long) p, p->h_size * sizeof(HEADER),
                 (long) p->h_next);
            domain_check(&e2malloc_base, OUTPUT);
            fflush(stderr);
            return;
        }
#endif
    }
/*
 *  (void) fprintf(stderr, "lcnt: %d\n", lcnt);
 *
 * At this point, the hp is either sandwiched between p and p->h_next,
 * or it is outside the zone (above or below).
 *
 * Can we coalesce with the following block?
 */
    if ((hp + hp->h_size + 1) == nextp)
    {
        hp->h_size += (nextp->h_size + 1);
        hp->h_next = nextp->h_next;
        e2malloc_base.freep = prevp;
#ifdef QUICK_REPLACE
        quick_remove(nextp);
#endif
#ifdef DEBUG
        nextp->h_next = &e2malloc_base;
        nextp->h_size = (HEADER *) &e2malloc_base;
#endif
    }
    else
        hp->h_next = nextp;
/*
 * Can we coalesce with the preceding block?
 */
    if ((p + p->h_size + 1) == hp)
    {
        e2malloc_base.freep = prevp;
        p->h_size += (hp->h_size + 1);
        p->h_next = hp->h_next;
#ifdef DEBUG
        hp->h_next = (HEADER *) &e2malloc_base;
        hp->h_size = &e2malloc_base;
#endif
    }
    else
    {
        e2malloc_base.freep = p;
        p->h_next = hp;
#ifdef QUICK_REPLACE
        quick_maint(hp);
#endif
    }
    return;
}
/*
 * Grab more core (nu * sizeof(HEADER) bytes of it), and return the
 * new address to the caller. If DEBUG, validate the arena
 * On Linux, must be protected by the rf mutex.
 */
static HEADER * morecore(nu)
unsigned int    nu;
{
HEADER    *hp;
char    *cp;
char * ret;
#ifdef DEBUG
    fflush(stdout);
    fprintf(stderr,"%#lx:Asking for more core: %u\n",(long) &e2malloc_base, nu);
    domain_check(&e2malloc_base, QUIET);
    fflush(stderr);
#endif
#ifdef MINGW32
    nu = ((nu - 1)/8192 + 1)*8192;
                         /* Must allocate in units of 64 KBytes */
#endif
    if (nu < e2malloc_base.os_alloc)
    {
        nu = e2malloc_base.os_alloc;
        if (e2malloc_base.os_alloc < HIGHWATER)
            e2malloc_base.os_alloc += e2malloc_base.os_alloc;
    }
    if (((cp = sbrk(nu * sizeof(HEADER))) == (char *) 0) || (cp == (char *) -1))
    {
        fflush(stdout);
        fprintf(stderr,"%#lx:Unable to obtain nu:%u (%#x bytes)\n",
                    (long) &e2malloc_base,nu, nu * sizeof(HEADER));
#ifdef DEBUG
        domain_check(&e2malloc_base, OUTPUT);
        fflush(stderr);
        abort();
#endif
        return((HEADER *) NULL);
    }
    if (e2malloc_base.alloced_base == (char *) NULL)
        e2malloc_base.alloced_base = cp;
    ret = add_grab(cp, nu * sizeof(HEADER));
    if (ret != cp)
    {     /* Happens if we needed more space for book-keeping */
        nu -= ((HEADER *) ret - (HEADER *) cp);
        ret = cp;
    }
    hp = (HEADER *) cp;
    hp->h_size = nu - 1;
    hp->h_next = (HEADER *) &e2malloc_base;       /* Marker for our blocks */
    real_free((char *)(hp + 1));
    return(e2malloc_base.freep);
}
__inline static HEADER * ini_malloc_arena()
{
static long interlock;
static long done;
HEADER * prevp;

#ifdef MINGW32
    if (InterlockedIncrement(&interlock) == 1)
    {
        InitializeCriticalSection(&(e2malloc_base.cs));
        InterlockedIncrement(&done);
    }
    else
    while(InterlockedAnd(&done, 1) != 1)
        Sleep(0);
    EnterCriticalSection(&(e2malloc_base.cs));
    if ((prevp = e2malloc_base.freep) == (HEADER *) NULL)
    {
#else
#ifdef E2_THREAD_SAFE
    if (__sync_fetch_and_add(&interlock, 1) == 0)
    {
        pthread_mutex_init(&(e2malloc_base.rf_mutex), NULL);
        pthread_mutex_init(&(e2malloc_base.lz_mutex), NULL);
        __sync_fetch_and_add(&done, 1);
    }
    else
    while(__sync_fetch_and_add(&done, 0) == 0)
        pthread_yield();
    pthread_mutex_lock(&(e2malloc_base.rf_mutex));
    if ((prevp = e2malloc_base.freep) == (HEADER *) NULL)
    {
#else
    {
#endif
#endif
        e2malloc_base.base.h_next = e2malloc_base.freep = prevp
                              = &e2malloc_base.base;
        e2malloc_base.os_alloc = NALLOC;
        e2malloc_base.last_buc = &e2malloc_base.root_buc;
#ifdef QUICK_REPLACE
        e2malloc_base.base.h_size = 0;
        e2malloc_base.hash_step = 32;
#endif
#ifdef LAZY_FREE
        e2malloc_base.unfreed_next = e2malloc_base.unfreed;
        e2malloc_base.unfreed_waiting = 0;
#endif
    }
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
#endif
#ifdef MINGW32
    LeaveCriticalSection(&(e2malloc_base.cs));
#endif
    return prevp;
}
/*
 *    Find the block, and take it off free list. If there's not enough
 *    space on the free list, then grow it.
 */
char * e2malloc(nbytes)
unsigned int    nbytes;
{
unsigned int    nunits;
HEADER        *prevp;
HEADER        *p;

    if (nbytes == 0)
        nunits = 0;
    else
    if ((nunits = (nbytes - 1)/sizeof(HEADER) + 1) >= HIGHWATER)
    {
        (void) fprintf(stderr, "%#lx:malloc: %d bytes refused\n",
              (long) &e2malloc_base, nbytes);
        fflush(stderr);
        return((char *) NULL);
    }
/*
 * Assumes just the one thread at the beginning ...
 */
    if ((prevp = e2malloc_base.freep) == (HEADER *) NULL)
        (void) ini_malloc_arena();
#ifdef DEBUG_FULL
    else
        domain_check(&e2malloc_base, QUIET);
#endif
#ifdef MINGW32
    EnterCriticalSection(&(e2malloc_base.cs));
#endif
#ifdef LAZY_FREE
/*
 * Begin with a look at the top of the unfreed list, since it takes less
 * maintenance
 */
#ifdef SEARCH_BACKLOG
#ifdef E2_THREAD_SAFE
    pthread_mutex_lock(&(e2malloc_base.lz_mutex));
#endif
    if ( e2malloc_base.unfreed_waiting
#if 0
      && nunits < REUSE_THRESHOLD
#endif
      && (p = reuse_unfreed(nunits)) != NULL)
    {
#ifdef DEBUG_TRACK
        fprintf(stderr,"M:%#lx:%u:%#lx:0\n", (long) &e2malloc_base, nunits,
               (long) (p + 1));
        e2malloc_base.tot_used += p->h_size + 1;
#endif
#ifdef MINGW32
        LeaveCriticalSection(&(e2malloc_base.cs));
#endif
        return ((char *) (p + 1));
    }
#ifdef E2_THREAD_SAFE
    else
    if (!e2malloc_base.unfreed_waiting)
        pthread_mutex_unlock(&(e2malloc_base.lz_mutex));
#endif
#endif
#endif
/*
 * Search the free list for a suitable piece
 */
retry:
#ifdef E2_THREAD_SAFE
    pthread_mutex_lock(&(e2malloc_base.rf_mutex));
#endif
    prevp = e2malloc_base.freep;
    for ( p = prevp->h_next; ; prevp = p, p = p->h_next)
    {
#ifdef DEBUG
        if (p == ( HEADER *) NULL || p == (HEADER *) &e2malloc_base)
        {
            (void) fprintf(stderr, "LOGIC ERROR. %#lx:free block chain corrupt\n\
     Pointer %#lx Size %#lx\n", (long) &e2malloc_base, (long) prevp,prevp->h_size);
            domain_check(&e2malloc_base, OUTPUT);
#ifdef E2_THREAD_SAFE
            pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
#else
#ifdef MINGW32
            LeaveCriticalSection(&(e2malloc_base.cs));
#endif
#endif
            return (char *) NULL;
        }
        if (p->h_size == 0 && p != &e2malloc_base.base)
        {
            (void) fprintf(stderr, "LOGIC ERROR. %#lx:write outside bounds?\n\
     Pointer %#lx Size Zero Previous %#lx\n", (long) &e2malloc_base, (long) p, (long) prevp);
            domain_check(&e2malloc_base, OUTPUT);
#ifdef E2_THREAD_SAFE
            pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
#else
#ifdef MINGW32
            LeaveCriticalSection(&(e2malloc_base.cs));
#endif
#endif
            return (char *) NULL;
        }
        if ((p > prevp) && (prevp + prevp->h_size) >= p)
        {
            (void) fprintf(stderr, "LOGIC ERROR. %#lx:free space chain corrupt.\n\
Space for allocation (%#x) at %#lx includes next header %#lx\n",
                     (long) &e2malloc_base,
                    (prevp->h_size + 1) * sizeof( *prevp),
                    (unsigned long) prevp,
                    (unsigned long) p);
            domain_check(&e2malloc_base, OUTPUT);
#ifdef E2_THREAD_SAFE
            pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
#else
#ifdef MINGW32
            LeaveCriticalSection(&(e2malloc_base.cs));
#endif
#endif
            return (char *) NULL;
        }
#endif
        if (p->h_size >= nunits)
        {
/*
 * A 'better fit' step
 */
            if (p->h_size > nunits + 1
             && p->h_next->h_size >= nunits
             && p->h_next->h_size < p->h_size)
            {
                prevp = p;
                p = p->h_next;
            }
            e2malloc_base.freep = prevp;
            if (p->h_size < nunits + 2)
            {
                prevp->h_next = p->h_next;
#ifdef QUICK_REPLACE
                quick_remove(p);
#endif
            }
            else
            {
                p->h_size -= (nunits + 1);
                p += p->h_size + 1;
                p->h_size = nunits;
            }
#ifdef DEBUG
            e2malloc_base.tot_used += p->h_size + 1;
            fflush(stdout);
            fprintf(stderr,"%#lx:malloc() returned %d at %#lx\n",
                     (long) &e2malloc_base, nbytes,(long)(p + 1)); 
            fflush(stderr);
#endif
#ifdef MINGW32
            LeaveCriticalSection(&(e2malloc_base.cs));
#else
#ifdef E2_THREAD_SAFE
            pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
#endif
#endif
            p->h_next = (HEADER *) &e2malloc_base;
#ifdef DEBUG_TRACK
            fprintf(stderr,"M:%#lx:%u:%#lx:0\n", (long) &e2malloc_base, nunits,
                       (long) (p + 1));
#endif
            if (((long) (p+1)) & 0xffffffff80000000LL)
                fprintf(stderr,"e2malloc() (%lx)Sign bit for 32 bit set?\n",
                             ((unsigned long)(p+1)));
            return((char *)(p + 1));
        }
#ifdef DEBUG
        if (((p + p->h_size + 1) > p->h_next) && (p->h_next > p))
        {
            fflush(stdout);
            (void) fprintf(stderr, "%#lx:Malloc arena corruption.\n\
Overlapping free items. Free at %#lx size %#x overlaps free at %#lx\n",
                 (long) &e2malloc_base,
                 (long) p, p->h_size * sizeof(HEADER),
                 (long) p->h_next);
            domain_check(&e2malloc_base, OUTPUT);
            fflush(stderr);
        }
#endif
        if (p == e2malloc_base.freep)
        {
#ifdef LAZY_FREE
#ifdef DEBUG
            fprintf(stderr,
     "\n%#lx:free_cnt=%d space=%lu unfreed_waiting=%lu but could not find %u\n",
                     (long) &e2malloc_base,
                e2malloc_base.free_cnt, 
                e2malloc_base.tot_free*sizeof(HEADER),
                e2malloc_base.unfreed_waiting*sizeof(HEADER), nbytes);
#endif
#endif
            if ((p = morecore(nunits+1)) == (HEADER *) NULL)
            {
#ifdef LAZY_FREE
#ifdef E2_THREAD_SAFE
                pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
                pthread_mutex_lock(&(e2malloc_base.lz_mutex));
#endif
                if (e2malloc_base.unfreed_waiting > 0)
                {
#ifdef DEBUG
                    fprintf(stderr, "%#lx:Last chance looking for %lu\n",
                            (long) &e2malloc_base, nbytes);
#endif
                    free_unfreed(); /* releases the mutex */
                    goto retry;
                }
#ifdef E2_THREAD_SAFE
                else
                    pthread_mutex_unlock(&(e2malloc_base.lz_mutex));
#endif
#endif
#ifdef MINGW32
                LeaveCriticalSection(&(e2malloc_base.cs));
#endif
#ifdef LAZY_FREE
                fprintf(stderr,
"\n%#lx:NO SPACE!? free_cnt=%d free=%lu used=%lu but could not find %lu\n",
                     (long) &e2malloc_base,
                e2malloc_base.free_cnt,
                e2malloc_base.tot_free*sizeof(HEADER),
                e2malloc_base.tot_used*sizeof(HEADER),
                  nbytes);
                fflush(stderr);
                domain_check(&e2malloc_base, OUTPUT);
                fflush(stderr);
#endif
                return((char *) NULL);
            }
        }
    }
}
/*
 * calloc - grab the memory via malloc, and zero it.
 */
char * e2calloc(n, siz)
unsigned int n;
unsigned int siz;
{
unsigned int    i;
char  *cp;

    i = n * siz;
    if ((cp = e2malloc(i)) != (char *) NULL)
        memset(cp, 0, i);
    return(cp);
}
/*
 *    Check the size of the new area. If it's less than
 *    or equal to the old area, then just use the old area. If not, then
 *    use malloc to get a block, copy the original data from old, and then
 *    free the old block. Return the address of the new block.
 *
 * This version guarantees not to move a thing that is shrinking. Years ago,
 * realloc() used to behave like that, but it was never part of the contract.
 * However, we have code (shrinking objects with a lot of self references) that
 * relies on it not moving.
 */
char * e2realloc_e2(old, siz)
char        *old;
unsigned int    siz;
{
HEADER    *hp, *hp_new;
char    *cp;
int nu;
int nnu;

    if (old == (char *) NULL)
        return e2malloc(siz);
    hp = (HEADER *)old - 1;            /* The size of the current block    */
    if (siz == 0)
        nu = 0;
    else
        nu = (siz - 1)/sizeof(HEADER) + 1;
                                       /* The size of the new block needed */
    nnu = hp->h_size - nu - 1;         /* The size left over after shrink  */
#ifdef DEBUG_TRACK
    fprintf(stderr,"A:%#lx:%u:%u:%#lx\n", 
                     (long) &e2malloc_base, hp->h_size, nu, (long) old);
    fflush(stdout);
    fprintf(stderr, "%#lx:called e2realloc_e2(%#lx,%d)\n",
                     (long) &e2malloc_base, (unsigned long) old, siz);
    fflush(stderr);
#endif
    if (nnu >= -1)
    {
#ifdef DEBUG
        fprintf(stderr, "%#lx:Reclaiming space - Current:%d Wanted:%d\n",
                     (long) &e2malloc_base,
            (hp->h_size + 1) * sizeof(HEADER), (nu + 1) * sizeof(HEADER));
        fflush(stderr);
#endif
        if (nnu > 0)
        {
            hp_new = hp + nu + 1;
            hp_new->h_size = nnu;
            hp_new->h_next = (HEADER *) &e2malloc_base;
            hp->h_size = nu;
            e2free((char *)(hp_new + 1));
                                      /* Should coalesce if possible      */
        } 
        return(old);
    }
#ifdef DEBUG
    fprintf(stderr, "%#lx:Does not fit, was %d now %d\n",
                     (long) &e2malloc_base, (hp->h_size) * sizeof(HEADER), siz);
    fflush(stderr);
#endif
    if ((cp = e2malloc(siz)) != (char *) NULL)
    {
        memcpy(cp, old, hp->h_size*sizeof(HEADER));
        e2free(old);
    }
#ifdef DEBUG
    fprintf(stderr, "%#lx:returned %#lx\n",
                     (long) &e2malloc_base, (unsigned long) cp);
    fflush(stderr);
#endif
    return(cp);
}
/*
 * Resize. Do not reuse an old area, even if it is shrinking, if we are using
 * less than half of it, and it is a big piece. This is an attempt to combat 
 * fragmentation problem.
 *
 * If we are not re-using, use malloc to get a block, copy the original data
 * from old, and then free the old block. Return the address of the new block.
 */
char * e2realloc(old, siz)
char        *old;
unsigned int    siz;
{
HEADER    *hp, *hp_new;
char    *cp;
int nu;
int nnu;

    if (old == (char *) NULL)
        return e2malloc(siz);
    hp = (HEADER *)old - 1;            /* The size of the current block    */
    if (siz == 0)
        nu = 0;
    else
        nu = (siz - 1)/sizeof(HEADER) + 1;
                                       /* The size of the new block needed */
    nnu = hp->h_size - nu - 1;         /* The size left over after shrink  */
#ifdef DEBUG_TRACK
    fprintf(stderr,"A:%#lx:%u:%u:%#lx\n", 
                     (long) &e2malloc_base, hp->h_size, nu, (long) old);
    fflush(stdout);
    fprintf(stderr, "%#lx:called e2realloc(%#lx,%d)\n",
                     (long) &e2malloc_base, (unsigned long) old, siz);
    fflush(stderr);
#endif
    if (nnu >= -1 && nnu < (nu + nu))
    {
#ifdef DEBUG
        fprintf(stderr, "%#lx:Reclaiming space - Current:%d Wanted:%d\n",
                     (long) &e2malloc_base,
            (hp->h_size + 1) * sizeof(HEADER), (nu + 1) * sizeof(HEADER));
        fflush(stderr);
#endif
        if (nnu > 0)
        {
            hp_new = hp + nu + 1;
            hp_new->h_size = nnu;
            hp_new->h_next = (HEADER *) &e2malloc_base;
            hp->h_size = nu;
            e2free((char *)(hp_new + 1));
                                      /* Should coalesce if possible      */
        } 
        return(old);
    }
#ifdef DEBUG
    fprintf(stderr, "%#lx:Does not fit, was %d now %d\n",
                     (long) &e2malloc_base, (hp->h_size) * sizeof(HEADER), siz);
    fflush(stderr);
#endif
    if ((cp = e2malloc(siz)) != (char *) NULL)
    {
        memcpy(cp, old, (hp->h_size > nu) ? siz : hp->h_size*sizeof(HEADER));
        e2free(old);
    }
#ifdef DEBUG
    fprintf(stderr, "%#lx:returned %#lx\n",
                     (long) &e2malloc_base, (unsigned long) cp);
    fflush(stderr);
#endif
    return(cp);
}
/*
 *    Add 'n' bytes at 'cp' into the free list.
 */
void bfree(cp, n)
char        *cp;
unsigned int    n;
{
HEADER    *hp;
HEADER    *p;
hp = (HEADER *) cp;

    if (n == 0)
        return;
    n = (n - 1)/sizeof(HEADER);
    if (n > HIGHWATER)
    {
        (void) fprintf(stderr, "%#lx:unusual address in bfree %#lx, size: %d\n",
                     (long) &e2malloc_base, (long) cp, n);
        return;
    }
    if (e2malloc_base.freep == (HEADER *) NULL)
        (void) ini_malloc_arena();
    hp->h_size = n;
    hp->h_next = (HEADER *) &e2malloc_base;
#ifdef DEBUG_TRACK
    fprintf(stderr,"B:%#lx:%u:%#lx\n", (long) &e2malloc_base, n,
                       (long) (hp + 1));
#endif
#ifdef DEBUG_FULL
    domain_check(&e2malloc_base, QUIET);
#endif
#ifdef MINGW32
    EnterCriticalSection(&(e2malloc_base.cs));
#else
#ifdef E2_THREAD_SAFE
    pthread_mutex_lock(&(e2malloc_base.rf_mutex));
#endif
#endif
    for (p = e2malloc_base.freep ; !(hp > p && hp < p->h_next) ; p = p->h_next) 
        if (p >= p->h_next && (hp > p || hp < p->h_next))
            break;
    hp->h_next = p->h_next;
    p->h_next = hp;
    e2malloc_base.freep = p;
#ifdef MINGW32
    LeaveCriticalSection(&(e2malloc_base.cs));
#else
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
#endif
#endif
    return;
}
/* #ifdef MINGW32 */
/*
 * Create a copy of a string in malloc()'ed space
 */
/* __inline */ char * e2strdup(s)
char    *s;                     /* Null-terminated input string */
{
int n = strlen(s) + 1;
char *cp;

    if ((cp = (char *) e2malloc(n)) == (char *) NULL)
        abort();
    (void) memcpy(cp, s, n);
    return(cp);
}
/* #endif */
#ifdef QUICK_REPLACE
#define NEED_QSORT
#endif
#ifdef LAZY_FREE
#define NEED_QSORT
#endif
#ifdef NEED_QSORT
#ifdef USE_RECURSE
/******************************************************************************
 * Quicksort implementation
 ******************************************************************************
 * Function to choose the mid value for Quick Sort. Because it is so beneficial
 * to get a good value, the number of values checked depends on the number of
 * elements to be sorted.
 */
__inline static char * pick_mid(a1, cnt)
char **a1;                             /* Array of pointers to be sorted      */
int cnt;                               /* Number of pointers to be sorted     */
{
char **top; 
char **bot; 
char **cur; 
char * samples[31];          /* Where we are going to put samples */
int i, j;

    bot = a1;
    cur = a1 + 1;
    top = a1 + cnt - 1;
/*
 * Check for all in order
 */
    while (bot < top && *bot < *cur)
    {
        bot++;
        cur++;
    }
    if (cur > top)
        return (char *) NULL;
                       /* They are all the same, or they are in order */
/*
 * Decide how many we are going to sample.
 */
    if (cnt < 8)
        return *cur;
    samples[0] = *cur;   /* Ensures that we do not pick the highest value */
    if (cnt < (2 << 5))
        i = 3;
    else 
    if (cnt < (2 << 7))
        i = 5;
    else 
    if (cnt < (2 << 9))
        i = 7;
    else 
    if (cnt < (2 << 11))
        i = 11;
    else 
    if (cnt < (2 << 15))
        i = 19;
    else 
        i = 31;
    j = cnt/i;
    cnt = i;
    for (bot = &samples[cnt - 1]; bot > &samples[0]; top -= j)
        *bot-- = *top;
    mqarrsort(bot, cnt);
    top = &samples[cnt - 1];
    cur = &samples[cnt/2];
/*
 * Make sure we do not return the highest value
 */
    while(cur > bot && *cur == *top)
        cur--;
    return *cur;
}
/*
 * Use QuickSort to sort an array of character pointers.
 */
__inline void mqarrsort(a1, cnt)
char **a1;                             /* Array of pointers to be sorted      */
int cnt;                               /* Number of pointers to be sorted     */
{
char * mid;
char * swp;
char **top; 
char **bot; 

    if ((mid = pick_mid(a1, cnt)) == (char *) NULL)
        return;
    bot = a1;
    top = a1 + cnt - 1;
    while (bot < top)
    {
        while (bot < top && *bot < mid)
            bot++;
        if (bot >= top)
            break;
        while (bot < top && *top > mid)
            top--;
        if (bot >= top)
            break;
        swp = *bot;
        *bot++ = *top;
        *top-- = swp;
    }
    if (bot == top && *bot < mid)
        bot++;
    mqarrsort(a1,bot - a1);
    mqarrsort(bot, cnt - (bot - a1));
    return;
}
#else
/*****************************************************************************
 * Alternative Quick Sort routines. More inline.
 *****************************************************************************
 */
static __inline char ** find_any(low, high, match_word)
char ** low;
char ** high;
char * match_word;
{
char **guess;

    while (low <= high)
    {
        guess = low + ((high - low) >> 1);
        if (*guess ==  match_word)
            return guess + 1; 
        else
        if (*guess < match_word)
            low = guess + 1;
        else
            high = guess - 1;
    }
    return low;
}
void mqarrsort(a1, cnt)
char **a1;                             /* Array of pointers to be sorted      */
int cnt;                               /* Number of pointers to be sorted     */
{
char * mid;
char * swp;
char **top; 
char **bot; 
char **cur; 
char **ptr_stack[128];
int cnt_stack[128];
int stack_level = 0;
int i, j, l;

    ptr_stack[0] = a1;
    cnt_stack[0] = cnt;
    stack_level = 1;
/*
 * Now Quick Sort the stuff
 */
    while (stack_level > 0)
    {
        stack_level--;
        cnt = cnt_stack[stack_level];
        a1 = ptr_stack[stack_level];
        top = a1 + cnt - 1;
        if (cnt < 5)
        {
            for (bot = a1; bot < top; bot++)
                 for (cur = top ; cur > bot; cur--)
                 {
                     if (*bot > *cur)
                     {
                         mid = *bot;
                         *bot = *cur;
                         *cur = mid;
                     }
                 }
            continue;
        }
        bot = a1;
        cur = a1 + 1;
/*
 * Check for all in order
 */
        while (bot < top && (*bot <= *cur))
        {
            bot++;
            cur++;
        }
        if (cur > top)
            continue;
                       /* They are all the same, or they are in order */
        mid = *cur;
        if (cur == top)
        {              /* One needs to be swapped, then we are done? */
            *cur = *bot;
            *bot = mid;
            cur = bot - 1;
            top = find_any(a1, cur, mid);
            if (top == bot)
                continue;
            while (cur >= top)
                *bot-- = *cur--;
            *bot = mid;
            continue;
        }
        if (cnt > 18)
        {
        unsigned char * samples[31];  /* Where we are going to put samples */

            samples[0] = mid; /* Ensure that we do not pick the highest value */
            l = bot - a1;
            if (cnt < (2 << 5))
                i = 3;
            else 
            if (cnt < (2 << 7))
                i = 5;
            else 
            if (cnt < (2 << 9))
                i = 7;
            else 
            if (cnt < (2 << 11))
                i = 11;
            else 
            if (cnt < (2 << 15))
                i = 19;
            else 
                i = 31;
            if (l >= i && (a1[l >> 1] < a1[l]))
            {
                mid = a1[l >> 1];
                bot = a1;
            }
            else
            {
                j = cnt/i;
                for (bot = &samples[i - 1]; bot > &samples[0]; top -= j)
                    *bot-- = *top;
                mqarrsort(&samples[0], i);
                cur = &samples[(i >> 1)];
                top = &samples[i - 1];
                while (cur > &samples[0] && (*cur == *top))
                    cur--;
                mid = *cur;
                top = a1 + cnt - 1;
                if (*(a1 + l) <= mid)
                    bot = a1 + l + 2;
                else
                    bot = a1;
            }
        }
        else
            bot = a1;
        while (bot < top)
        {
            while (bot < top && *bot < mid)
                bot++;
            if (bot >= top)
                break;
            while (bot < top && (*top > mid))
                top--;
            if (bot >= top)
                break;
            swp = *bot;
            *bot++ = *top;
            *top-- = swp;
        }
        if (bot == top && (*bot <= mid))
            bot++;
        if (stack_level > 125)
        {
            mqarrsort(a1, bot - a1);
            mqarrsort(bot, cnt - (bot - a1));
        }
        else
        {
            ptr_stack[stack_level] = a1;
            cnt_stack[stack_level] = bot - a1;
            stack_level++;
            ptr_stack[stack_level] = bot;
            cnt_stack[stack_level] = cnt - (bot - a1);
            stack_level++;
        }
    }
    return;
}
#endif
#endif
#ifdef LAZY_FREE
/***********************************************************************
 * Lazy space management attempting to reduce the cost of free() operations
 */
static void free_unfreed()
{
HEADER * hp;
HEADER * hp1;
char ** fpp;
char ** fpp1;
int i;
int j;
int k;

#ifdef DEBUG
    (void) fprintf(stderr,
            "%#lx:free_unfreed() %u entries totalling %u\n",
                     (long) &e2malloc_base,
                e2malloc_base.unfreed_next - e2malloc_base.unfreed,
                e2malloc_base.unfreed_waiting);
#endif
#ifdef FRAG_TRACKING
/*
 * Get the unfreed in a list
 */
    for (i = 0; i < B_NODE_ALLOC/32; i++)
    {
        if (e2malloc_base.alloc_map[i] != 0x0)
        {
            for (j = 0, k = 1; j < 32; j++, k<<=1)
            {
                if ((e2malloc_base.alloc_map[i] & k))
                {
                    *e2malloc_base.unfreed_next
                         = e2malloc_base.alloc_nodes[ (i << 5) + j].key;
                    e2malloc_base.unfreed_next++;
                }
            } 
        }
    }
/*
 * Re-set the unfreed list
 */
    memset(e2malloc_base.alloc_map, 0, sizeof(e2malloc_base.alloc_map));
    e2malloc_base.node_cnt = 0;
    e2malloc_base.root_node = NULL;
    e2malloc_base.try_from = 0;
#endif
/*
 * Sort them by address
 */
    mqarrsort(e2malloc_base.unfreed,
                e2malloc_base.unfreed_next - e2malloc_base.unfreed);
/*
 * Attempt to pre-coalesce
 */
    for (
#ifdef DEBUG
         i = 0,
#endif
         fpp = e2malloc_base.unfreed;
             fpp < e2malloc_base.unfreed_next;
                 fpp = fpp1)
    {
        hp = (HEADER *) *fpp;
#ifdef DEBUG
        i += (hp->h_size + 1);
        if (hp->h_next != (HEADER *) &e2malloc_base)
        {
            (void) fprintf(stderr,
     "LOGIC ERROR: %#lx:Write outside bounds at(%#lx), size: %#x, next %#lx\n",
                     (long) &e2malloc_base,
                    (long) hp, hp->h_size, (long) hp->h_next);
            fpp1 = fpp + 1;
            continue;
        }
#endif
        for (fpp1 = fpp + 1; 
                fpp1 < e2malloc_base.unfreed_next;
                    fpp1++)
        {
            if (*fpp1 == *fpp)
            {
                fprintf(stderr, "%#lx:Free(%#lx)ing twice\n",
                     (long) &e2malloc_base, (long) *fpp);
                continue;
            }
            hp1 = (HEADER *) *fpp1;
#ifdef DEBUG
            i += (hp1->h_size + 1);
            if (hp1->h_next != (HEADER *) &e2malloc_base)
            {
                (void) fprintf(stderr,
      "LOGIC ERROR: %#lx:Write outside bounds at(%#lx), size: %#x, next %#lx\n",
                     (long) &e2malloc_base,
                     (long) hp1, hp1->h_size, (long) hp1->h_next);
                break;
            }
#endif
            if (hp1 == (hp + hp->h_size + 1))
            {
                hp->h_size += (hp1->h_size + 1);
#ifdef DEBUG
                hp1->h_next = (HEADER *) &e2malloc_base;
                hp1->h_size = &e2malloc_base;
#endif
            }
            else
                break;
        }
#ifdef DEBUG_TRACK
        if (fpp1 != fpp + 1)
            fprintf(stderr, "C:%#lx:%u:%u\n",  (long) &e2malloc_base,
                                  (fpp1 - fpp) -1, hp->h_size + 1);
        else
            fprintf(stderr, "U:%#lx:%u\n", (long) &e2malloc_base,
                                   (hp->h_size + 1));
#endif
#ifdef E2_THREAD_SAFE
        pthread_mutex_lock(&(e2malloc_base.rf_mutex));
#endif
        real_free(hp + 1); 
#ifdef E2_THREAD_SAFE
        pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
#endif
    }
#ifdef DEBUG
    if (e2malloc_base.unfreed_waiting != i)
        fprintf(stderr, "%#lx:Mis-count? unfreed_waiting=%u but returned %d\n",
                     (long) &e2malloc_base, e2malloc_base.unfreed_waiting,i);
#endif
    e2malloc_base.unfreed_waiting = 0;
    e2malloc_base.unfreed_next = e2malloc_base.unfreed;
#ifdef DEBUG
    domain_check(&e2malloc_base, QUIET);
#endif
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2malloc_base.lz_mutex));
#endif
    return;
}
#ifdef SEARCH_BACKLOG
/***********************************************************************
 * Lazy space management attempting to reduce the cost of free() operations
 */
static HEADER * reuse_unfreed(nunits)
int nunits;
{
HEADER * p;
int ret;
#ifdef FRAG_TRACKING
struct b_node * root;
#endif
#ifdef FRAG_TRACKING
    p = NULL;
    e2malloc_base.root_node = b_node_pop(e2malloc_base.root_node, nunits, &p);
#else
char **fpp, **fpp1;

    for (p = NULL, fpp = &e2malloc_base.unfreed[0];
             fpp < e2malloc_base.unfreed_next;
                   fpp++)
        if (p == NULL)
        {
            if (((HEADER *) *fpp)->h_size == nunits
             || ((HEADER *) *fpp)->h_size == nunits + 1)
            {
                p = (HEADER *) *fpp;
                fpp1 = fpp;
            }
        }
        else
        {
            *fpp1 = *fpp;
            if (*fpp1 == p)
                fprintf(stderr, "%#lx:Freed(%#lx) twice\n",
                    (long) &e2malloc_base, (long) p);
            else
                fpp1++;
        }
    if (p != NULL)
        e2malloc_base.unfreed_next = fpp1;
#endif
    if (p != NULL)
    {
        e2malloc_base.unfreed_waiting -=  (p->h_size + 1);
#ifdef FRAG_TRACKING
#ifdef DEBUG_TRACK
        fprintf(stderr, "G:%#lx:%u:%#lx:%u\n",
                     (long) &e2malloc_base,
                nunits, (long) p, e2malloc_base.node_cnt);
#endif
#endif
    }
#ifdef FRAG_TRACKING
#ifdef DEBUG_TRACK
    else
        fprintf(stderr, "L:%#lx:%u:%u\n",
                     (long) &e2malloc_base,
                nunits, e2malloc_base.node_cnt);
#endif
#endif
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2malloc_base.lz_mutex));
#endif
    return p;
}
#endif
/*
 * First stage free() - depending on size, may opt to hang on to it for a bit
 */
void e2free(cp)
char *cp;
{
char ** fpp;
HEADER * hp;

    if (cp <= sizeof(HEADER))
        return;       /* Test for call in error. Perhaps should report this. */
    hp = ((HEADER *) cp) - 1;
    if (hp->h_next != (HEADER *) &e2malloc_base)
    {
        sub_free(hp->h_next, cp);
        return;
    }
#ifdef DEBUG_TRACK
    e2malloc_base.tot_used -= hp->h_size + 1;
    fprintf(stderr,"F:%#lx:%u:%#lx\n", (long) &e2malloc_base, hp->h_size,
                     (long) cp);
    fprintf(stderr,"%#lx:free() returned %d at %#lx\n", (long) &e2malloc_base,
                     hp->h_size * sizeof(HEADER),
              (long)(hp + 1)); 
    fflush(stderr);
    if (hp->h_next != (HEADER *) &e2malloc_base)
    {
        (void) fprintf(stderr,
    "LOGIC ERROR: %#lx:Write outside bounds free(%#lx), size: %#x, next %#lx\n",
                     (long) &e2malloc_base,
            (long) cp, hp->h_size, (long) hp->h_next);
        return;
    }
    if (hp->h_size > HIGHWATER || hp->h_size <= 0)
    {
        (void) fprintf(stderr,
        "LOGIC ERROR: %#lx:Unusual address in free %#lx, size: %d, next %#lx\n",
           (long) &e2malloc_base, (long) cp, hp->h_size, (long) hp->h_next);
        return;
    }
#endif
#ifdef MINGW32
    EnterCriticalSection(&(e2malloc_base.cs));
#endif
#if 0
    if (hp->h_size >= REUSE_THRESHOLD)
    {
#ifdef E2_THREAD_SAFE
        pthread_mutex_lock(&(e2malloc_base.rf_mutex));
#endif
        real_free(cp);
#ifdef E2_THREAD_SAFE
        pthread_mutex_unlock(&(e2malloc_base.rf_mutex));
#endif
    }
    else
#endif
    {
#ifdef E2_THREAD_SAFE
        pthread_mutex_lock(&(e2malloc_base.lz_mutex));
#endif
#ifdef FRAG_TRACKING 
        e2malloc_base.root_node = b_node_add(e2malloc_base.root_node, hp, cmp_size);
#else
        *e2malloc_base.unfreed_next = hp;
        e2malloc_base.unfreed_next++;
#endif
        e2malloc_base.unfreed_waiting +=  hp->h_size + 1;
        if (
#ifdef FRAG_TRACKING
            e2malloc_base.node_cnt >= BACKLOG ||
#else
            e2malloc_base.unfreed_next >= &e2malloc_base.unfreed[BACKLOG] ||
#endif
            e2malloc_base.unfreed_waiting >= e2malloc_base.os_alloc)
            free_unfreed();
#ifdef E2_THREAD_SAFE
        else
            pthread_mutex_unlock(&(e2malloc_base.lz_mutex));
#endif
    }
#ifdef MINGW32
    LeaveCriticalSection(&(e2malloc_base.cs));
#endif
    return;
}
#ifdef FRAG_TRACKING
/*
 * Compare two keys which are actually pointers to HEADERS to see if the
 * sizes match.
 */
int cmp_size(a, b)
long int a;
long int b;
{
HEADER * hp1 = (HEADER *) a;
HEADER * hp2 = (HEADER *) b;

    if (hp1->h_size < hp2->h_size)
        return -1;
    else
    if (hp1->h_size == hp2->h_size)
        return 0;
    else
        return 1;
}
/*
 * Allocate from the pool of nodes
 */
static struct b_node * b_node_alloc()
{
unsigned int i;
unsigned int j;
unsigned int k;
unsigned int top;
struct b_node * ret;

    i = e2malloc_base.try_from;
    top = B_NODE_ALLOC/32;
   
repeat:
    for (; i < top; i++)
    {
        if (e2malloc_base.alloc_map[i] != 0xffffffff)
        {
            for (j = 0, k = 1; j < 32; j++, k<<=1)
            {
                if (!(e2malloc_base.alloc_map[i] & k))
                {
                    e2malloc_base.alloc_map[i] |= k;
                    ret = &e2malloc_base.alloc_nodes[ (i << 5) + j];
                    memset(ret, 0, sizeof(struct b_node));
                    e2malloc_base.node_cnt++;
                    e2malloc_base.try_from++;
                    if ( e2malloc_base.try_from >= B_NODE_ALLOC/32)
                        e2malloc_base.try_from = 0;
                    return ret;
                }
            } 
        }
    }
    if (i == e2malloc_base.try_from)
        return NULL;
    i = 0;
    top = e2malloc_base.try_from;
    goto repeat;
}
/*
 * Return to pool
 */
static void b_node_free(bnp)
struct b_node * bnp;
{
    e2malloc_base.alloc_map[((bnp - e2malloc_base.alloc_nodes) >>5)] &=
                  ~(1 << ((bnp - e2malloc_base.alloc_nodes) & 31));
    e2malloc_base.node_cnt--;
    e2malloc_base.try_from = (bnp - e2malloc_base.alloc_nodes)>>5;
#ifdef DEBUG
    bnp->key = 23;
    bnp->child_left = 7;
    bnp->child_right = 11;
#endif
    return;
}

static struct b_node * b_node_add();
/*
 * Allocate a root
 */
static struct b_node * b_node_init()
{
    return b_node_alloc();
}
/*
 * Re-balance if out of kilter
 */
static struct b_node * b_node_balance(root, cmp)
struct b_node * root;
int (*cmp)();
{
struct b_node * child;
struct b_node * parent;
char * x;

    if (root->child_left != NULL && root->child_right != NULL)
    {
/*
 * Want to re-balance if:
 * |(r - l)| >|(lr - l -ll)|
 * or
 * |(r - l)| >|(ll - r -lr)|
 *
 * What we do instead is rebalance if the thing is way out of kilter, since
 * it seems much cheaper.
 *
 * Note that rebalancing is unnecessary if the stuff arrives in a random
 * sequence
 *
    int cur_bal;
    int left_rebal;
    int right_rebal;

        cur_bal = (root->cnt_left - root->cnt_right);
        if (cur_bal < 0)
            cur_bal = -cur_bal;
        left_rebal = (root->child_left->cnt_left - root->cnt_right -
                        root->child_left->cnt_right - 2);
        if (left_rebal < 0)
            left_rebal = -left_rebal; 
        right_rebal = (root->child_right->cnt_right - root->cnt_left -
                        root->child_right->cnt_left - 2);
        if (right_rebal < 0)
            right_rebal = -right_rebal; 
        if (right_rebal < cur_bal -16 && right_rebal <= left_rebal)
*/
        if (root->child_right->cnt_right >= 
                   (root->child_right->cnt_left + root->cnt_left + 2))
        {
            x = root->key;
            child = root->child_left;
            parent = root->child_right;
            b_node_free(root);
            for ( root = parent;
                    parent->child_left != NULL;
                        parent = parent->child_left)
                parent->cnt_left +=  child->cnt_left + child->cnt_right + 2;
            parent->cnt_left +=  child->cnt_left + child->cnt_right + 2;
            parent->child_left = b_node_add(child, x, cmp);
            root = b_node_balance(root, cmp);
        }
        else
/*        if (left_rebal < cur_bal -16 && left_rebal < right_rebal) */
        if (root->child_left->cnt_left >= 
            (root->child_left->cnt_right + root->cnt_right + 2))
        {
            x = root->key;
            child = root->child_right;
            parent = root->child_left;
            b_node_free(root);
            for (root = parent;
                    parent->child_right != NULL;
                        parent = parent->child_right)
                parent->cnt_right +=  child->cnt_left + child->cnt_right + 2;
            parent->cnt_right +=  child->cnt_left + child->cnt_right + 2;
            parent->child_right =  b_node_add(child, x, cmp);
            root = b_node_balance(root, cmp);
        }
    }
    return root;
}
/*
 * Add an entry - find out how balanced it stays
 *
 * The lame hand-optimisations actually make a big difference
 */
static struct b_node * b_node_add(root, key, cmp)
struct b_node * root;
unsigned char * key; 
int (*cmp)();
{
struct b_node * parent;
int ret;

    if (root == NULL)
        root = b_node_init();
    if (root->key == NULL)
        root->key = key;
    else
    if ((ret = cmp(key, root->key)) == 0)
    {
        if (key == root->key)
            fprintf(stderr, "%#lx:Freed(%#lx) twice\n",
                    (long) &e2malloc_base, (long) key);
        else
        {
            for (parent = root->child_middle;
                   parent != NULL
                && parent->child_middle != NULL ;
                    parent = parent->child_middle)
            {
                if ( parent->key == key)
                {
                    fprintf(stderr, "%#lx:Freed(%#lx) twice\n",
                        (long) &e2malloc_base, (long) key);
                    break;
                }
            }
            if (parent == NULL)
                root->child_middle = b_node_add(NULL, key, cmp);
            else
            if ( parent->key != key)
                parent->child_middle = b_node_add(NULL, key, cmp);
        }
        return root;   /* Don't rebalance if matched */
    }
    else
    if (ret < 0)
    {
        if (root->child_left == NULL)
        {
            root->child_left = b_node_add(NULL, key, cmp);
            root->cnt_left = 1;
        }
        else
        if (ret
           && root->child_right == NULL
           && root->cnt_left > 1)
        {
            parent = root;
            root = b_node_add(root->child_left, parent->key, cmp);
            b_node_free(parent);
            root = b_node_add(root, key, cmp);
        }
        else
        {
            root->child_left = b_node_add(root->child_left, key, cmp);
            root->cnt_left++;
        }
    }
    else
    {
        if (root->child_right == NULL)
        {
            root->child_right = b_node_add(NULL, key, cmp);
            root->cnt_right = 1;
        }
        else
        if (root->child_left == NULL && root->cnt_right > 1)
        {
            parent = root;
            root = b_node_add(root->child_right, parent->key, cmp);
            b_node_free(parent);
            root = b_node_add(root, key, cmp);
        }
        else
        {
            root->child_right = b_node_add(root->child_right, key, cmp);
            root->cnt_right++;
        }
    }
    return b_node_balance(root, cmp);
}
/*
 * Find an entry, and remove it. The logic knows about what we are looking
 * for, so provides cmp_size() to b_node_balance
 */
static struct b_node * b_node_pop(root, nunits, pop_key)
struct b_node * root;
unsigned int nunits;
unsigned char ** pop_key;
{
struct b_node * child;
struct b_node * parent;
HEADER * hp;

    if (root == NULL)
        return NULL;
    if ( root->key == NULL)
    {
        b_node_free(root);
        return NULL;
    }
    hp = (HEADER *) (root->key);
    if (hp->h_size == nunits || hp->h_size == nunits + 1)
    {
        *pop_key = (unsigned char *) hp;
        if (root->child_middle != NULL)
        {
            parent = root->child_middle;
            root->child_middle = parent->child_middle;
            root->key = parent->key;
            b_node_free(parent);
            return root;
        }
        else
        if (root->child_left == NULL && root->child_right == NULL)
        {
            b_node_free(root);
            return NULL;
        }
        else
        if (root->child_left == NULL)
        {
            child = root->child_right;
            b_node_free(root);
            return child;
        }
        else
        if (root->child_right == NULL)
        {
            child = root->child_left;
            b_node_free(root);
            return child;
        }
        else
        {
            child = root->child_left;
            parent = root->child_right;
            b_node_free(root);
            for (root = parent;
                    parent->child_left != NULL;
                        parent = parent->child_left)
                parent->cnt_left += child->cnt_left + child->cnt_right + 1; 
            parent->cnt_left +=  child->cnt_left + child->cnt_right + 1;
            parent->child_left = child;
        }
    }
    else
    if (hp->h_size > nunits)
    {
        if (root->child_left != NULL)
            root->child_left =
               b_node_pop(root->child_left, nunits, pop_key);
    }
    else
    if (root->child_right != NULL)
        root->child_right =
               b_node_pop(root->child_right, nunits, pop_key);
    if (root->child_left == NULL)
        root->cnt_left = 0;
    else
        root->cnt_left = root->child_left->cnt_left +
                                root->child_left->cnt_right + 1;
    if (root->child_right == NULL)
        root->cnt_right = 0;
    else
        root->cnt_right = root->child_right->cnt_left +
                                root->child_right->cnt_right + 1;
    return b_node_balance(root, cmp_size);
}
#ifdef TEST_BTREE
/*
 * Remove an entry - find out how balanced it stays
 */
static struct b_node * b_node_remove(root, key, cmp)
struct b_node * root;
unsigned char * key; 
int (*cmp)();
{
int ret;
struct b_node * child;
struct b_node * parent;

    if (root == NULL)
        return NULL;
    else
    if ( root->key == NULL)
    {
        b_node_free(root);
        return NULL;
    }
    else
    if ((ret = cmp(key, root->key)) == 0)
    {
        if (root->child_middle != NULL)
        {
            parent = root->child_middle;
            root->child_middle = parent->child_middle;
            root->key = parent->key;
            b_node_free(parent);
            return root;
        }
        if (root->child_left == NULL && root->child_right == NULL)
        {
            b_node_free(root);
            return NULL;
        }
        else
        if (root->child_left == NULL)
        {
            child = root->child_right;
            b_node_free(root);
            return child;
        }
        else
        if (root->child_right == NULL)
        {
            child = root->child_left;
            b_node_free(root);
            return child;
        }
        else
        {
            child = root->child_left;
            parent = root->child_right;
            b_node_free(root);
            for (root = parent;
                    parent->child_left != NULL;
                        parent = parent->child_left)
                parent->cnt_left += child->cnt_left + child->cnt_right + 1;
            parent->cnt_left += child->cnt_left + child->cnt_right + 1;
            parent->child_left = child;
        }
    }
    else
    if (ret < 0)
    {
        if (root->child_left != NULL)
            root->child_left =
               b_node_remove(root->child_left, key, cmp);
    }
    else
    if (root->child_right != NULL)
        root->child_right =
               b_node_remove(root->child_right, key, cmp);
    if (root->child_left == NULL)
        root->cnt_left = 0;
    else
        root->cnt_left = root->child_left->cnt_left +
                                root->child_left->cnt_right + 1;
    if (root->child_right == NULL)
        root->cnt_right = 0;
    else
        root->cnt_right = root->child_right->cnt_left +
                                root->child_right->cnt_right + 1;
    return b_node_balance(root, cmp);
}
/*
 * Find an entry
 */
static unsigned char * b_node_find(root, key, cmp)
struct b_node * root;
unsigned char * key; 
int (*cmp)();
{
int ret;

    if (root == NULL || root->key == NULL)
        return NULL;
    else
    if ((ret = cmp(key, root->key)) == 0)
        return root->key;
    else
    if (ret < 0)
        return b_node_find(root->child_left, key, cmp);
    else
        return b_node_find(root->child_right, key, cmp);
}
static void b_node_walk(root)
struct b_node * root;
{
static int lev;

    lev++;
    if (root->child_left != NULL)
        b_node_walk(root->child_left);
    if (root->key != NULL)
        printf("Value : %lu Level %d Left %d Right %d\n",
              (long int) root->key, lev, root->cnt_left, root->cnt_right);
    if (root->child_right != NULL)
        b_node_walk(root->child_right);
    lev--;
    return;
}
static int cmp_long(a, b)
long int a;
long int b;
{
    if (a < b)
        return -1;
    else
    if (a == b)
        return 0;
    else
        return 1;
}
/*
 * Test Bed
 */
int main(argc, argv)
int argc;
char ** argv;
{
struct b_node * root = NULL;
long int i;

    for (i = 1; i < 900000; i++)
    {
        root = b_node_add(root, i, cmp_long);
        if (i != node_cnt)
            printf("Should be: %d Actual %d\n", i, node_cnt);
    }
    puts("\nLoaded in order\n");
    b_node_walk(root);
    for (i = 3; i < 900000; i += 7)
        root = b_node_remove(root, i, cmp_long);
    puts("\nAfter some deletions\n");
    b_node_walk(root);
    for (i = 3; i < 900000; i += 7)
        root = b_node_add(root, i, cmp_long);
    if (node_cnt != 899999)
        printf("After put back should be: 899999 Actual %d\n",  node_cnt);
    puts("\nPut them back\n");
    b_node_walk(root);
/*
    srand(1);
    for (i = 1; i < 900000; i++)
        root = b_node_add(root, (unsigned long) rand(), cmp_long);
    puts("\nPut in random order\n");
    b_node_walk(root);
    srand(1);
    for (i = 1; i < 900000; i++)
    {
        if ((i % 7) == 3)
            root = b_node_remove(root, (unsigned long) rand(), cmp_long);
        else
            rand();
    }
    puts("\nDo some deletions\n");
    b_node_walk(root);
    srand(1);
    for (i = 1; i < 900000; i++)
    {
        if ((i % 7) == 3)
            root = b_node_add(root, (unsigned long) rand(), cmp_long);
        else
            rand();
    }
    puts("\nPut them back\n");
    b_node_walk(root);
*/
    exit(0);
}
#endif
#endif
#endif
