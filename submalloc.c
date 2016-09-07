/*
 * submalloc.c - Simple-minded nestable memory allocator - nested routines.
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
 * -    The base pointer checks slow it down measurably, but they are so useful
 *      for troubleshooting they are activated by default
 *
 * The parameters here allocate about the same amount of space to the index and
 * the unallocated backlog, and search for best-fit in the backlog before going
 * down the free chain. The result is probably 10 times slower than the default.
 *
 * It would be more efficient, I think, if the headers were separate. Then
 * searching for free items would touch fewer pages. We do this with the OS
 * allocation/parent tracking.
 */
static char * sccs_id= "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
#define E2MALLOC_BUILD
#include "submalloc.h"
#ifdef FRAG_TRACKING
static struct b_node * sub_b_node_add();
static struct b_node * sub_b_node_init();
static struct b_node * sub_b_node_balance();
static struct b_node * sub_b_node_remove();
static struct b_node * sub_b_node_pop();
#endif
DONT_NEED_TO_SEE void sub_real_free();
#ifdef LAZY_FREE
static HEADER * sub_reuse_unfreed();
static void sub_free_unfreed();
#endif
/*
 * Account for parent allocations
 */
static char * sub_add_grab(e2mbp, bite, size)
struct e2malloc_base * e2mbp;
char * bite;
unsigned long int size;
{
    if (e2mbp->last_buc->buc_cnt > 0)
    {
    struct pbite * pgrab;

        pgrab = & e2mbp->last_buc->grab[
                    e2mbp->last_buc->buc_cnt - 1];
        if (bite == pgrab->pchunk + pgrab->psize)
            pgrab->psize += size;
        else
        if ( e2mbp->last_buc->buc_cnt > 31 )
        {
/*
 * Put a HEADER on the piece to keep the diagnostics happy. The caller
 * may have to adjust their size accordingly
 */
        HEADER * hp = (HEADER *) bite;

            hp->h_size = 1 + sizeof(struct pbite)/sizeof(HEADER);
            hp->h_next = (HEADER *) e2mbp;
            e2mbp->last_buc->next_buc = (struct pbuc *) (hp + 1);

            e2mbp->last_buc = e2mbp->last_buc->next_buc;
            e2mbp->last_buc->next_buc = NULL;
            e2mbp->last_buc->grab[0].pchunk = bite;
            e2mbp->last_buc->grab[0].psize = size;
            e2mbp->last_buc->buc_cnt = 1;
            bite = (char *) (hp + 1 + hp->h_size);
        }
        else
        {
            pgrab++;
            pgrab->pchunk = bite;
            pgrab->psize = size;
            e2mbp->last_buc->buc_cnt++;
        }
    }
    else
    {
        e2mbp->root_buc.grab[0].pchunk = bite;
        e2mbp->root_buc.grab[0].psize = size;
        e2mbp->root_buc.buc_cnt = 1;
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
__inline static void sub_quick_maint();
__inline static HEADER * sub_quick_min();
/*
 * Routine to quickly find a nearby free item. The idea is that these things
 * will be spread over the full range, automatically, regardless of the size
 * of the arena.
 */
__inline static unsigned int sub_quick_find(e2mbp, p)
struct e2malloc_base * e2mbp;
HEADER * p;
{
long int n = (unsigned long) (((char *) p) - e2mbp->alloced_base);
unsigned int i;

    if (n < 0)
        return 0;
    i = qpick_ind(n);                /* Find the top bit                      */
    if (i <= e2mbp->quick_bits)
        return n;
    else
        return (n >> (i - e2mbp->quick_bits - 1 ))
               &~(1<<(e2mbp->quick_bits + 1));
                                     /* Get the top bits into the range */
}
/*
 * Routine to remove ourselves from the table of blocks if we are there.
 */
__inline static void sub_quick_remove(e2mbp, p)
struct e2malloc_base * e2mbp;
HEADER *p;
{
HEADER **x; 

    e2mbp->free_cnt--;
    if (e2mbp->quick_table != (HEADER **) NULL)
    {
        x = &(e2mbp->quick_table[sub_quick_find(e2mbp, p)]);
        if (*x == p)
        {
#ifdef DEBUG
            fprintf(stderr, "%#lx:sub_quick_remove(%#lx) matched %#lx\n",
                    (long) e2mbp, (long) p, (long) x);
#endif
            *x = (HEADER *) NULL;
        }
#ifdef DEBUG
        else
            fprintf(stderr, "%#lx:sub_quick_remove(%#lx) didn't find target\n",
                    (long) e2mbp, (long) p);
#endif
        x = &(e2mbp->quick_table[sub_quick_find(e2mbp, e2mbp->freep)]);
        if (*x == (HEADER *) NULL)
        {
            *x = e2mbp->freep;
#ifdef DEBUG
            fprintf(stderr, "%#lx:sub_quick_remove() added %#lx at %#lx\n",
                    (long) e2mbp, (long) e2mbp->freep, (long) x);
#endif
        }
    }
    return;
}
/*
 * Adjust the size of the hash table
 */
static void sub_quick_rehash(e2mbp )
struct e2malloc_base * e2mbp;
{
HEADER ** old_base;
HEADER ** old_bound;
HEADER ** x;

    if (e2mbp->quick_table == (HEADER **) NULL)
    {
        e2mbp->quick_table = &(e2mbp->quick_table_1[0]);
        e2mbp->quick_bound = &(e2mbp->quick_table_1[1 << (START_HASH_BITS + 1)]);
        e2mbp->hash_step = (1 << (START_HASH_BITS + 6));
        e2mbp->quick_bits =  START_HASH_BITS;
    }
    else
    if ((e2mbp->quick_bound - e2mbp->quick_table) < 
           (1 << (FINAL_HASH_BITS + 1)))
    {
        old_base = e2mbp->quick_table;
        old_bound = e2mbp->quick_bound;
        if ( e2mbp->quick_table == &(e2mbp->quick_table_1[0]))
            e2mbp->quick_table = &(e2mbp->quick_table_2[0]);
        else
            e2mbp->quick_table = &(e2mbp->quick_table_1[0]);
        e2mbp->quick_bound = e2mbp->quick_table +
                                   4 * (old_bound - old_base);
        e2mbp->quick_bits += 2;
        e2mbp->hash_step <<= 2;
        if (e2mbp->hash_step > (1 << (FINAL_HASH_BITS + 5)))
            e2mbp->hash_step = 65536L*32768L;
        memset((char *) (e2mbp->quick_table), 0, 
                        (((char *) e2mbp->quick_bound) -
                          ((char *) e2mbp->quick_table)));
        for (x = old_base; x < old_bound; x++)
            if (*x != (HEADER *) NULL)
            {
#ifdef DEBUG
                if (((*x)->h_size == e2mbp)
                  ||  ((*x)->h_next == (HEADER *) e2mbp))
                {
                    (void) fprintf(stderr,
          "LOGIC ERROR: %#lx:Index corrupt; address %#lx is not in free chain\n",
                    (long) e2mbp, (long) (*x));
                    return;
                }
#endif
                e2mbp->free_cnt--;
                sub_quick_maint(e2mbp, *x);
            }
    }
    return;
}
/*
 * Routine to add ourselves to the table of free blocks if our slot is free.
 */
__inline static void sub_quick_maint(e2mbp, p)
struct e2malloc_base * e2mbp;
HEADER *p;
{
HEADER **x; 

    e2mbp->free_cnt++;
    if (e2mbp->free_cnt > e2mbp->hash_step)
        sub_quick_rehash(e2mbp);
    if (e2mbp->quick_table != (HEADER **) NULL)
    {
        x = &(e2mbp->quick_table[sub_quick_find(e2mbp, p)]);
/*
 * We favour larger addresses over smaller ones because the search will
 * take longer if the addresses are sparser over the larger range
 */
        if (*x + (1 << e2mbp->quick_bits) < p)
        {
#ifdef DEBUG
            fprintf(stderr, "%#lx:sub_quick_maint(%#lx) adding at %#lx\n",
                    (long) e2mbp, (long) p, (long) x);
            if (((p)->h_size == e2mbp)
               ||  ((p)->h_next == (HEADER *) e2mbp))
            {
                    (void) fprintf(stderr,
   "LOGIC ERROR: %#lx:Attempted corruption; address %#lx is not in free chain\n",
                    (long) e2mbp, (long) (*x));
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
__inline static HEADER * sub_quick_start(e2mbp, p)
struct e2malloc_base * e2mbp;
HEADER *p;
{
HEADER **x, **lim;

    if (e2mbp->quick_table == (HEADER **) NULL)
        return &e2mbp->base;
/*
 *  x = &(e2mbp->quick_table[sub_quick_find(e2mbp, p)]);
 *  if (*x > p || *x == (HEADER *) NULL)
 *      x--;
 *  else
 *      return *x;
 *  if (x < e2mbp->quick_table || *x > p || *x == (HEADER *) NULL)
 *      return &e2mbp->base;
 *  else
 *      return *x;
 */
    else
    {
        x = &(e2mbp->quick_table[sub_quick_find(e2mbp, p)]);
/*
 *      fprintf(stderr, "Hash: %#x Slot: %d Contents: %#x\n",
 *               (unsigned long int) p,
 *               (unsigned long int)(x - e2mbp->quick_table),
 *               (unsigned long int) *x);
 */
        if (*x != (HEADER *) NULL && *x < p)
        {
#ifdef DEBUG
            if ((*x)->h_next == (HEADER *) e2mbp)
                fprintf(stderr,"%#lx:sub_quick_start(): Used space pointer %#lx found at index %u\n",
                    (long) e2mbp, (*x)->h_next, (x - e2mbp->quick_table));
#endif
            return *x;
        }
        return sub_quick_min(e2mbp, p, (x - 1));
    }
}
/*
 * Routine to find the highest known free address less than passed value.
 */
__inline static HEADER * sub_quick_min(e2mbp, p, bound)
struct e2malloc_base * e2mbp;
HEADER *p;
HEADER **bound;
{
HEADER *min_p;

    while (bound >= e2mbp->quick_table)
    {
        if (*bound != (HEADER *) NULL)
        {
#ifdef DEBUG
            if ((*bound)->h_next == (HEADER *) e2mbp)
                fprintf(stderr,"%#lx:sub_quick_min(): Used space pointer %#lx found at index %u\n",
                    (long) e2mbp, (*bound)->h_next,
                           (bound - e2mbp->quick_table));
#endif
            if ( *bound < p)
                return *bound;
        }
        bound--;
    }
    return &e2mbp->base;
}
#endif
/*
 * For Windows
 */
static char * sub_sbrk(e2mbp, space_size)
struct e2malloc_base * e2mbp;
long int space_size;
{
unsigned long real_size;
char * ret;

    if (space_size == 0)
    {
/*
 * Return the top of the last allocation
 */
       if (e2mbp->last_buc->buc_cnt < 1)
           return 0;
       else
           return e2mbp->last_buc->grab[
                e2mbp->last_buc->buc_cnt - 1].pchunk
              + e2mbp->last_buc->grab[
                e2mbp->last_buc->buc_cnt - 1].psize - 1;
    }
    if ((ret = (char *) e2malloc( space_size)) == (char *) NULL)
    {
        fputs("malloc() failed, bad things are about to happen ...\n", stderr);
        return (char *) -1;
    }
    if (e2mbp->alloced_base == (char *) NULL)
        e2mbp->alloced_base = ret;
    return ret;
}
/*
 * The following returns all the space we have taken from the parent in
 * one hit. The complication is that we need to take care not to shoot ourselves
 * from under ourselves, since after the first one the structure is in one
 * of the allocated pieces ... 
 */
void sub_e2osfree(e2mbp)
struct e2malloc_base * e2mbp;
{   /* return space to OS */
struct pbuc * pb, *pbn;
int i;

     for (pb = &e2mbp->root_buc; pb != NULL; pb = pbn)
     {
         pbn = pb->next_buc;
         for (i = pb->buc_cnt - 1; i >= 0; i--)
             free(pb->grab[i].pchunk);
     }
     e2mbp->alloced_base = (char *) NULL;
     e2mbp->root_buc.buc_cnt = 0;
     e2mbp->root_buc.next_buc = NULL;
     e2mbp->last_buc = & e2mbp->root_buc;
     return;
}
/*
 * Grab more core (nu * sizeof(HEADER) bytes of it), and return the
 * new address to the caller. If DEBUG, validate the arena
 */
static HEADER * sub_morecore(e2mbp, nu)
struct e2malloc_base * e2mbp;
unsigned int    nu;
{
HEADER    *hp;
char    *cp;
char * ret;
#ifdef DEBUG
    fflush(stdout);
    fprintf(stderr,"%#lx:Asking for more core: %u\n", (long) e2mbp, nu);
    domain_check(e2mbp, QUIET);
    fflush(stderr);
#endif
#ifdef MINGW32
    if (nu == 0)
        return NULL;
    nu = ((nu - 1)/8192 + 1)*8192;
                         /* Must allocate in units of 64 KBytes */
#endif
    if (nu < e2mbp->os_alloc)
    {
        nu = e2mbp->os_alloc;
        if (e2mbp->os_alloc < HIGHWATER)
            e2mbp->os_alloc += e2mbp->os_alloc;
    }
    if (((cp = sub_sbrk(e2mbp, nu * sizeof(HEADER))) == (char *) 0) || (cp == (char *) -1))
    {
#ifdef DEBUG
    fflush(stdout);
    fprintf(stderr,"%#lx:Unable to obtain nu:%u (%#x bytes)\n",
                    (long) e2mbp,nu, nu * sizeof(HEADER));
    domain_check(e2mbp, OUTPUT);
    fflush(stderr);
            abort();
#endif
        return((HEADER *) NULL);
    }
    if (e2mbp->alloced_base == (char *) NULL)
        e2mbp->alloced_base = cp;
    ret = sub_add_grab(e2mbp, cp, nu * sizeof(HEADER));
    if (ret != cp)
    {     /* Happens if we needed more space for book-keeping */
        nu -= ((HEADER *) ret - (HEADER *) cp);
        ret = cp;
    }
    hp = (HEADER *) cp;
    hp->h_size = nu - 1;
    hp->h_next = (HEADER *) e2mbp; /* Marker for our blocks */
    sub_real_free(e2mbp, (char *)(hp + 1));
    return(e2mbp->freep);
}
/*
 * Free the block pointed at by 'cp'. Put the block into the correct place in
 * the free list, coalescing if possible.
 */
DONT_NEED_TO_SEE void sub_real_free(e2mbp, cp)
struct e2malloc_base * e2mbp;
char    *cp;
{
HEADER    *hp;
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
    (void) fprintf(stderr, "%#lx:sub_real_free(%#lx)\n",
                    (long) e2mbp, (unsigned long) cp);
/*
 *  if (cp == 0x8146760)
 *      fputs("0x8146760 seen\n", stderr);
 */
    fflush(stderr);
#endif
    hp = ((HEADER *) cp) - 1;
    if (hp->h_next != (HEADER *) e2mbp)
    {
        fprintf(stderr,
"%#lx:sub_real_free(%#lx) but not one of ours or already freed; h_next(%#lx) != e2mbp\n",
                    (long) e2mbp, (unsigned long) cp, (long) hp->h_next);
        e2free((char *) (hp + 1)); /* See if it is owned by the parent */
        return;               /* Ameliorates corruption also ... */
    }
#ifdef DEBUG_TRACK
    fprintf(stderr,"R:%#lx:%u:%#lx\n", (long) e2mbp,
               hp->h_size, (unsigned long) cp);
    if (hp->h_size > HIGHWATER || hp->h_size <= 0)
    {
        (void) fprintf(stderr,
        "LOGIC ERROR: %#lx:Unusual address in free %#lx, size: %d, next %#lx\n",
                    (long) e2mbp,(long ) cp, hp->h_size, (long) hp->h_next);
        return;
    }
#endif
#ifdef DEBUG
    e2mbp->tot_free += hp->h_size + 1;
#endif
#ifdef DEBUG_FULL
    fflush(stdout);
    fprintf(stderr,"%#lx:sub_real_free() returning %d at %#lx, Sample: %20.s\n",
              (long) e2mbp, hp->h_size * sizeof(HEADER),(unsigned long)(cp),cp);
    fflush(stderr);
#endif
#ifdef QUICK_REPLACE
    if (hp < e2mbp->freep)
        e2mbp->freep = sub_quick_start(e2mbp, hp);
    else
    if (hp > e2mbp->freep->h_next + 512)
    {
/*
 * If we are not already in the right place, use the quick-start list in an
 * attempt to find a better start point.
 */
        p = sub_quick_start(e2mbp, hp);
        if (p > e2mbp->freep)
        {
#ifdef DEBUG_FULL
            fprintf(stderr, "%#lx:Closer - Wanted: %#lx Had: %#lx Got: %#lx\n",
                    (long) e2mbp, (unsigned long) hp,
                        (unsigned long) e2mbp->freep,
                        (unsigned long) p);
#endif
            e2mbp->freep = p;
        }
    }
#endif
    prevp = e2mbp->freep;
/*
 *  (void) fprintf(stderr, "sub_real_free(%#x, prevp %#x, prevp->h_next %#x)\n",
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
          ||  (p->h_size == 0 && p != &e2mbp->base))
        {
            (void) fprintf(stderr, "LOGIC ERROR. %#lx:free() item already free().\n\
Free at %#lx size %#x contents %.80s within %#lx size %#x\n",
                    (long) e2mbp, (long) hp, hp->h_size * sizeof(HEADER), cp,
                 (long) p, p->h_size * sizeof(HEADER));
            domain_check(e2mbp, OUTPUT);
            return;
        }
        else
        if (p == ( HEADER *) NULL
         ||  (p->h_size == e2mbp)
         ||  (p->h_next == (HEADER *) e2mbp))
        {
            (void) fprintf(stderr,"LOGIC ERROR. %#lx:free block chain corrupt\n",
                    (long) e2mbp);
            domain_check(e2mbp, OUTPUT);
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
                    (long) e2mbp,
                 (long) p, p->h_size * sizeof(HEADER),
                 (long) p->h_next);
            domain_check(e2mbp, OUTPUT);
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
        e2mbp->freep = prevp;
#ifdef QUICK_REPLACE
        sub_quick_remove(e2mbp, nextp);
#endif
#ifdef DEBUG
        nextp->h_next = (HEADER *) e2mbp;
        nextp->h_size = e2mbp;
#endif
    }
    else
        hp->h_next = nextp;
/*
 * Can we coalesce with the preceding block?
 */
    if ((p + p->h_size + 1) == hp)
    {
        e2mbp->freep = prevp;
        p->h_size += (hp->h_size + 1);
        p->h_next = hp->h_next;
#ifdef DEBUG
        hp->h_next = (HEADER *) e2mbp;
        hp->h_size = e2mbp;
#endif
    }
    else
    {
        e2mbp->freep = p;
        p->h_next = hp;
#ifdef QUICK_REPLACE
        sub_quick_maint(e2mbp, hp);
#endif
    }
    return;
}
__inline static HEADER * sub_ini_malloc_arena(e2mbp)
struct e2malloc_base * e2mbp;
{
HEADER    *prevp;

#ifdef MINGW32
#ifdef NESTED_CONCURRENT
    InitializeCriticalSection(&(e2mbp->cs));
    EnterCriticalSection(&(e2mbp->cs));
    if ((prevp = e2mbp->freep) == (HEADER *) NULL)
#endif
    {
#else
#ifdef E2_THREAD_SAFE
#ifdef NESTED_CONCURRENT
    pthread_mutex_init(&(e2mbp->rf_mutex), NULL);
    pthread_mutex_lock(&(e2mbp->rf_mutex));
    if ((prevp = e2mbp->freep) == (HEADER *) NULL)
#endif
    {
#ifdef NESTED_CONCURRENT
        pthread_mutex_init(&(e2mbp->lz_mutex), NULL);
#endif
#else
    {
#endif
#endif
        e2mbp->base.h_next = e2mbp->freep = prevp
                  = &e2mbp->base;
        e2mbp->os_alloc = NALLOC;
        e2mbp->last_buc = &e2mbp->root_buc;
#ifdef QUICK_REPLACE
        e2mbp->base.h_size = 0;
        e2mbp->hash_step = 32;
#endif
#ifdef LAZY_FREE
        e2mbp->unfreed_next = e2mbp->unfreed;
        e2mbp->unfreed_waiting = 0;
#endif
    }
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#endif
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
    LeaveCriticalSection(&(e2mbp->cs));
#endif
#endif
    return prevp;
}
/*
 *    Find the block, and take it off free list. If there's not enough
 *    space on the free list, then grow it.
 */
char * sub_malloc(e2mbp, nbytes)
struct e2malloc_base * e2mbp;
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
        (void) fprintf(stderr, "%#lx:sub_malloc: %d bytes refused\n",
                    (long) e2mbp, nbytes);
        fflush(stderr);
        return((char *) NULL);
    }
    if (e2mbp == NULL)
        return malloc(nbytes);
    if ((prevp = e2mbp->freep) == (HEADER *) NULL)
        prevp = sub_ini_malloc_arena(e2mbp);
#ifdef DEBUG_FULL
    else
        domain_check(e2mbp, QUIET);
#endif
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
    EnterCriticalSection(&(e2mbp->cs));
#endif
#endif
#ifdef LAZY_FREE
/*
 * Begin with a look at the top of the unfreed list, since it takes less
 * maintenance
 */
#ifdef SEARCH_BACKLOG
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
    pthread_mutex_lock(&(e2mbp->lz_mutex));
#endif
#endif
    if ( e2mbp->unfreed_waiting
#if 0
      && nunits < REUSE_THRESHOLD
#endif
      && (p = sub_reuse_unfreed(e2mbp, nunits)) != NULL)
    {
#ifdef DEBUG_TRACK
         fprintf(stderr,"M:%#lx:%u:%#lx:0\n", (long) e2mbp, nunits,
               (long) (p + 1));
        e2mbp->tot_used += p->h_size + 1;
#endif
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
        LeaveCriticalSection(&(e2mbp->cs));
#endif
#endif
        return ((char *) (p + 1));
    }
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
    else
    if (!e2mbp->unfreed_waiting)
        pthread_mutex_unlock(&(e2mbp->lz_mutex));
#endif
#endif
#endif
#endif
/*
 * Search the free list for a suitable piece
 */
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
    pthread_mutex_lock(&(e2mbp->rf_mutex));
#endif
#endif
    for ( p = prevp->h_next; ; prevp = p, p = p->h_next)
    {
#ifdef DEBUG
        if (p == ( HEADER *) NULL || p == (HEADER *) e2mbp)
        {
            (void) fprintf(stderr,
            "LOGIC ERROR. %#lx:free block chain corrupt\nPointer %#lx Size %#lx\n",
                    (long) e2mbp, (long) prevp,prevp->h_size);
            domain_check(e2mbp, OUTPUT);
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
            pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#endif
            return (char *) NULL;
        }
        if (p->h_size == 0 && p != &e2mbp->base)
        {
            (void) fprintf(stderr, "LOGIC ERROR. %#lx:write outside bounds?\n\
     Pointer %#lx Size Zero Previous %#lx\n",
                    (long) e2mbp, (long) p, (long) prevp);
            domain_check(e2mbp, OUTPUT);
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
            pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#endif
            return (char *) NULL;
        }
        if ((p > prevp) && (prevp + prevp->h_size) >= p)
        {
            (void) fprintf(stderr,
"LOGIC ERROR. %#lx:free space chain corrupt.\n\
Space for allocation (%#x) at %#lx includes next header %#lx\n",
                    (long) e2mbp, (prevp->h_size + 1) * sizeof( *prevp),
                    (unsigned long) prevp, (unsigned long) p);
            domain_check(e2mbp, OUTPUT);
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
            pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#ifdef MINGW32
            LeaveCriticalSection(&(e2mbp->cs));
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
            e2mbp->freep = prevp;
            if (p->h_size < nunits + 2)
            {
                prevp->h_next = p->h_next;
#ifdef QUICK_REPLACE
                sub_quick_remove(e2mbp, p);
#endif
            }
            else
            {
                p->h_size -= (nunits + 1);
                p += p->h_size + 1;
                p->h_size = nunits;
            }
#ifdef DEBUG
            e2mbp->tot_used += p->h_size + 1;
            fflush(stdout);
            fprintf(stderr,"%#lx:sub_malloc() returned %d at %#lx\n",
                    (long) e2mbp, nbytes,(long)(p + 1)); 
            fflush(stderr);
#endif
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
            LeaveCriticalSection(&(e2mbp->cs));
#else
#ifdef E2_THREAD_SAFE
            pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#endif
#endif
            p->h_next = (HEADER *) e2mbp;
#ifdef DEBUG_TRACK
            fprintf(stderr,"M:%#lx:%u:%#lx:0\n", (long) e2mbp, nunits,
                        (long) (p + 1));
#endif
            return((char *)(p + 1));
        }
#ifdef DEBUG
        if (((p + p->h_size + 1) > p->h_next) && (p->h_next > p))
        {
            fflush(stdout);
            (void) fprintf(stderr, "%#lx:Malloc arena corruption.\n\
Overlapping free items. Free at %#lx size %#x overlaps free at %#lx\n",
                    (long) e2mbp, (long) p, p->h_size * sizeof(HEADER),
                 (long) p->h_next);
            domain_check(e2mbp, OUTPUT);
            fflush(stderr);
        }
#endif
        if (p == e2mbp->freep)
        {
#ifdef LAZY_FREE
#ifdef DEBUG
            fprintf(stderr,
     "\n%#lx:free_cnt=%d space=%lu unfreed_waiting=%lu but could not find %u\n",
                    (long) e2mbp, e2mbp->free_cnt, 
                e2mbp->tot_free*sizeof(HEADER),
                e2mbp->unfreed_waiting*sizeof(HEADER),  nbytes);
#endif
#endif
            if ((p = sub_morecore(e2mbp, nunits+1)) == (HEADER *) NULL)
            {
#ifdef LAZY_FREE
                if (e2mbp->unfreed_waiting > 0)
                {
#ifdef DEBUG
                    fprintf(stderr, "%#lx:Last chance looking for %lu\n",
                    (long) e2mbp, nbytes);
#endif
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
                    pthread_mutex_lock(&(e2mbp->lz_mutex));
#endif
#endif
                    sub_free_unfreed(e2mbp);
                    p = e2mbp->freep;
                    continue;
                }
#endif
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
                LeaveCriticalSection(&(e2mbp->cs));
#else
#ifdef E2_THREAD_SAFE
                pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#endif
#endif
#ifdef LAZY_FREE
                fprintf(stderr,
"\n%#lx:NO SPACE!? free_cnt=%d free=%lu used=%lu but could not find %lu\n",
                    (long) e2mbp, e2mbp->free_cnt,
                e2mbp->tot_free*sizeof(HEADER),
                e2mbp->tot_used*sizeof(HEADER),
                  nbytes);
                fflush(stderr);
                domain_check(e2mbp, OUTPUT);
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
char * sub_calloc(e2mbp, n, siz)
struct e2malloc_base * e2mbp;
unsigned int n;
unsigned int siz;
{
unsigned int    i;
char  *cp;

    i = n * siz;
    if ((cp = sub_malloc(e2mbp, i)) != (char *) NULL)
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
char * sub_realloc_e2(e2mbp, old, siz)
struct e2malloc_base * e2mbp;
char        *old;
unsigned int    siz;
{
HEADER    *hp, *hp_new;
char    *cp;
int nu;
int nnu;

    if (old == (char *) NULL)
        return sub_malloc(e2mbp, siz);
    else
    if (e2mbp == NULL)
        return realloc(old, siz);

    hp = (HEADER *)old - 1;            /* The size of the current block    */
    if (siz == 0)
        nu = 0;
    else
        nu = (siz - 1)/sizeof(HEADER) + 1;
                                       /* The size of the new block needed */
    nnu = hp->h_size - nu - 1;         /* The size left over after shrink  */
#ifdef DEBUG_TRACK
    fprintf(stderr,"A:%#lx:%u:%u:%#lx\n", (long) e2mbp, hp->h_size, nu,
                   (long) old);
    fflush(stdout);
    fprintf(stderr, "%#lx:called realloc(%#lx,%d)\n",
                    (long) e2mbp, (unsigned long) old, siz);
    fflush(stderr);
#endif
    if (nnu >= -1)
    {
#ifdef DEBUG
        fprintf(stderr, "%#lx:Reclaiming space - Current:%d Wanted:%d\n",
                    (long) e2mbp,
            (hp->h_size + 1) * sizeof(HEADER), (nu + 1) * sizeof(HEADER));
        fflush(stderr);
#endif
        if (nnu > 0)
        {
            hp_new = hp + nu + 1;
            hp_new->h_size = nnu;
            hp_new->h_next = (HEADER *) e2mbp;
            hp->h_size = nu;
            sub_free(e2mbp, (char *)(hp_new + 1));
                                      /* Should coalesce if possible      */
        } 
        return(old);
    }
#ifdef DEBUG
    fprintf(stderr, "%#lx:Does not fit, was %d now %d\n",
                    (long) e2mbp, (hp->h_size) * sizeof(HEADER), siz);
    fflush(stderr);
#endif
    if ((cp = sub_malloc(e2mbp, siz)) != (char *) NULL)
    {
        memcpy(cp, old, hp->h_size*sizeof(HEADER));
        sub_free(e2mbp, old);
    }
#ifdef DEBUG
    fprintf(stderr, "%#lx:returned %#lx\n", (long) e2mbp, (unsigned long) cp);
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
char * sub_realloc(e2mbp, old, siz)
struct e2malloc_base * e2mbp;
char        *old;
unsigned int    siz;
{
HEADER    *hp, *hp_new;
char    *cp;
int nu;
int nnu;

    if (old == (char *) NULL)
        return sub_malloc(e2mbp, siz);
    else
    if (e2mbp == NULL)
        return realloc(old, siz);

    hp = (HEADER *)old - 1;            /* The size of the current block    */
    if (siz == 0)
        nu = 0;
    else
        nu = (siz - 1)/sizeof(HEADER) + 1;
                                       /* The size of the new block needed */
    nnu = hp->h_size - nu - 1;         /* The size left over after shrink  */
#ifdef DEBUG_TRACK
    fprintf(stderr,"A:%#lx:%u:%u:%#lx\n", (long) e2mbp, hp->h_size, nu,
                   (long) old);
    fflush(stdout);
    fprintf(stderr, "%#lx:called realloc(%#lx,%d)\n",
                    (long) e2mbp, (unsigned long) old, siz);
    fflush(stderr);
#endif
    if (nnu >= -1 && nnu < (nu + nu))
    {
#ifdef DEBUG
        fprintf(stderr, "%#lx:Reclaiming space - Current:%d Wanted:%d\n",
                    (long) e2mbp,
            (hp->h_size + 1) * sizeof(HEADER), (nu + 1) * sizeof(HEADER));
        fflush(stderr);
#endif
        if (nnu > 0)
        {
            hp_new = hp + nu + 1;
            hp_new->h_size = nnu;
            hp_new->h_next = (HEADER *) e2mbp;
            hp->h_size = nu;
            sub_free(e2mbp, (char *)(hp_new + 1));
                                      /* Should coalesce if possible      */
        } 
        return(old);
    }
#ifdef DEBUG
    fprintf(stderr, "%#lx:Does not fit, was %d now %d\n",
                    (long) e2mbp, (hp->h_size) * sizeof(HEADER), siz);
    fflush(stderr);
#endif
    if ((cp = sub_malloc(e2mbp, siz)) != (char *) NULL)
    {
        memcpy(cp, old, (hp->h_size > nu) ? siz : hp->h_size*sizeof(HEADER));
        sub_free(e2mbp, old);
    }
#ifdef DEBUG
    fprintf(stderr, "%#lx:returned %#lx\n", (long) e2mbp, (unsigned long) cp);
    fflush(stderr);
#endif
    return(cp);
}
/*
 *    Add 'n' bytes at 'cp' into the free list.
 */
void sub_bfree(e2mbp, cp, n)
struct e2malloc_base * e2mbp;
char        *cp;
unsigned int    n;
{
HEADER    *hp;
HEADER    *p;
hp = (HEADER *) cp;

    if (e2mbp == NULL)
    {
        bfree(cp,n);
        return;
    }
    if (n == 0)
        return;
    n = (n - 1)/sizeof(HEADER);
    if (n > HIGHWATER)
    {
        (void) fprintf(stderr,
            "%#lx:unusual address in sub_bfree %#lx, size: %d\n",
                    (long) e2mbp, (long) cp, n);
        return;
    }
    if (e2mbp->freep == (HEADER *) NULL)
        (void) sub_ini_malloc_arena(e2mbp);
#ifdef DEBUG_FULL
    else
        domain_check(e2mbp, QUIET);
#endif
    hp->h_size = n;
    hp->h_next = (HEADER *) e2mbp;
#ifdef DEBUG_TRACK
    fprintf(stderr,"B:%#lx:%u:%#lx\n", (long) e2mbp, n, (long) (hp + 1));
#endif
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
    EnterCriticalSection(&(e2mbp->cs));
#else
#ifdef E2_THREAD_SAFE
    pthread_mutex_lock(&(e2mbp->rf_mutex));
#endif
#endif
#endif
    for (p = e2mbp->freep ; !(hp > p && hp < p->h_next) ; p = p->h_next) 
        if (p >= p->h_next && (hp > p || hp < p->h_next))
            break;
    hp->h_next = p->h_next;
    p->h_next = hp;
    e2mbp->freep = p;
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
    LeaveCriticalSection(&(e2mbp->cs));
#else
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#endif
#endif
    return;
}
/* #ifdef MINGW32 */
/*
 * Create a copy of a string in malloc()'ed space
 */
/* __inline */ char * sub_strdup(e2mbp, s)
struct e2malloc_base * e2mbp;
char    *s;                     /* Null-terminated input string */
{
int n = strlen(s) + 1;
char *cp;

    if ((cp = (char *) sub_malloc(e2mbp, n)) == (char *) NULL)
        abort();
    (void) memcpy(cp, s, n);
    return(cp);
}
/* #endif */
/*
 * Output a summary of memory usage (and validate the arena)
 */
void print_managed(e2mbp)
struct e2malloc_base * e2mbp;
{
#ifdef LAZY_FREE
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
    pthread_mutex_lock(&(e2mbp->lz_mutex));
#endif
#endif
    sub_free_unfreed(e2mbp);
#endif
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
    EnterCriticalSection(&(e2mbp->cs));
#else
#ifdef E2_THREAD_SAFE
    pthread_mutex_lock(&(e2mbp->rf_mutex));
#endif
#endif
#endif
    domain_check(e2mbp, 0);
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
    LeaveCriticalSection(&(e2mbp->cs));
#else
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#endif
#endif
    fflush(stdout);
    fprintf(stderr, "\n%#lx:Malloc() arena - Used: %u Unfreed: %u Freed: %u\n",
                    (long) e2mbp,
        sizeof(HEADER) * (e2mbp->tot_used
                            - e2mbp->unfreed_waiting),
        sizeof(HEADER) *e2mbp->unfreed_waiting,
        sizeof(HEADER) *e2mbp->tot_free);
    fflush(stderr);
    return;
}
#ifdef LAZY_FREE
/***********************************************************************
 * Lazy space management attempting to reduce the cost of free() operations
 * The appropriate mutex must be held before calling.
 */
static void sub_free_unfreed(e2mbp)
struct e2malloc_base * e2mbp;
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
            "%#lx:sub_free_unfreed() %u entries totalling %u\n",
                    (long) e2mbp,
                e2mbp->unfreed_next - e2mbp->unfreed,
                e2mbp->unfreed_waiting);
#endif
#ifdef FRAG_TRACKING
/*
 * Get the unfreed in a list
 */
    for (i = 0; i < B_NODE_ALLOC/32; i++)
    {
        if (e2mbp->alloc_map[i] != 0x0)
        {
            for (j = 0, k = 1; j < 32; j++, k<<=1)
            {
                if ((e2mbp->alloc_map[i] & k))
                {
                    *e2mbp->unfreed_next
                         = e2mbp->alloc_nodes[ (i << 5) + j].key;
                    e2mbp->unfreed_next++;
                }
            } 
        }
    }
/*
 * Re-set the unfreed list
 */
    memset(e2mbp->alloc_map, 0, sizeof(e2mbp->alloc_map));
    e2mbp->node_cnt = 0;
    e2mbp->root_node = NULL;
    e2mbp->try_from = 0;
#endif
/*
 * Sort them by address
 */
    mqarrsort(e2mbp->unfreed,
                e2mbp->unfreed_next - e2mbp->unfreed);
/*
 * Attempt to pre-coalesce
 */
    for (
#ifdef DEBUG
         i = 0,
#endif
         fpp = e2mbp->unfreed;
             fpp < e2mbp->unfreed_next;
                 fpp = fpp1)
    {
        hp = (HEADER *) *fpp;
#ifdef DEBUG
        i += (hp->h_size + 1);
        if (hp->h_next != (HEADER *) e2mbp)
        {
            (void) fprintf(stderr,
     "LOGIC ERROR: %#lx:Write outside bounds at(%#lx), size: %#x, next %#lx\n",
                    (long) e2mbp, (long) hp, hp->h_size, (long) hp->h_next);
            fpp1 = fpp + 1;
            continue;
        }
#endif
        for (fpp1 = fpp + 1; 
                fpp1 < e2mbp->unfreed_next;
                    fpp1++)
        {
            if (*fpp1 == *fpp)
            {
                fprintf(stderr, "%#lx:Free(%#lx)ing twice\n",
                    (long) e2mbp, (long) *fpp);
                continue;
            }
            hp1 = (HEADER *) *fpp1;
#ifdef DEBUG
            i += (hp1->h_size + 1);
            if (hp1->h_next != (HEADER *) e2mbp)
            {
                (void) fprintf(stderr,
      "LOGIC ERROR: %#lx:Write outside bounds at(%#lx), size: %#x, next %#lx\n",
                    (long) e2mbp, (long) hp1, hp1->h_size, (long) hp1->h_next);
                break;
            }
#endif
            if (hp1 == (hp + hp->h_size + 1))
            {
                hp->h_size += (hp1->h_size + 1);
#ifdef DEBUG
                hp1->h_next = (HEADER *) e2mbp;
                hp1->h_size = e2mbp;
#endif
            }
            else
                break;
        }
#ifdef DEBUG_TRACK
        if (fpp1 != fpp + 1)
            fprintf(stderr, "C:%#lx:%u:%u\n", (long) e2mbp, (fpp1 - fpp) -1, hp->h_size + 1);
        else
            fprintf(stderr, "U:%#lx:%u\n", (long) e2mbp, (hp->h_size + 1));
#endif
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
        pthread_mutex_lock(&(e2mbp->rf_mutex));
#endif
#endif
        sub_real_free(e2mbp, hp + 1); 
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
        pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
#endif
    }
#ifdef DEBUG
    if (e2mbp->unfreed_waiting != i)
        fprintf(stderr, "%#lx:Mis-count? unfreed_waiting=%u but returned %d\n",
                    (long) e2mbp, e2mbp->unfreed_waiting,i);
#endif
    e2mbp->unfreed_waiting = 0;
    e2mbp->unfreed_next = e2mbp->unfreed;
#ifdef DEBUG
    domain_check(e2mbp, QUIET);
#endif
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2mbp->lz_mutex));
#endif
#endif
    return;
}
#ifdef SEARCH_BACKLOG
/***********************************************************************
 * Lazy space management attempting to reduce the cost of free() operations
 */
static HEADER * sub_reuse_unfreed(e2mbp, nunits)
struct e2malloc_base * e2mbp;
int nunits;
{
HEADER * p;
int ret;
#ifdef FRAG_TRACKING
struct b_node * root;
#endif
#ifdef FRAG_TRACKING
    p = NULL;
    e2mbp->root_node = sub_b_node_pop(e2mbp,e2mbp->root_node, nunits, &p);
#else
char **fpp, **fpp1;

    for (p = NULL, fpp = &e2mbp->unfreed[0];
             fpp < e2mbp->unfreed_next;
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
                    (long) e2mbp, (long) p);
            else
                fpp1++;
        }
    if (p != NULL)
        e2mbp->unfreed_next = fpp1;
#endif
    if (p != NULL)
    {
        e2mbp->unfreed_waiting -=  (p->h_size + 1);
#ifdef FRAG_TRACKING
#ifdef DEBUG_TRACK
        fprintf(stderr, "G:%#lx:%u:%#lx:%u\n", (long) e2mbp,
                nunits, (long) p, e2mbp->node_cnt);
#endif
#endif
    }
#ifdef FRAG_TRACKING
#ifdef DEBUG_TRACK
    else
        fprintf(stderr, "L:%#lx:%u:%u\n", (long) e2mbp,
                nunits, e2mbp->node_cnt);
#endif
#endif
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
    pthread_mutex_unlock(&(e2mbp->lz_mutex));
#endif
#endif
    return p;
}
#endif
/*
 * First stage free() - depending on size, may opt to hang on to it for a bit
 */
void sub_free(e2mbp, cp)
struct e2malloc_base * e2mbp;
char *cp;
{
char ** fpp;
HEADER * hp;

    if (cp <= sizeof(HEADER))
        return;       /* Test for call in error. Perhaps should report this. */
    hp = ((HEADER *) cp) - 1;
    if (e2mbp == NULL && hp->h_next == NULL)
    {
        e2free(cp);    /* Local de-allocator called with global memory; O.K. */
        return;
    }
    if (hp->h_next != (HEADER *) e2mbp)
    {
        if (((((unsigned long int) (hp->h_next)) & 0xff00000000000000L) >>56) >
               30)
            (void) fprintf(stderr,
                 "ERROR: %#lx:Looks like write outside bounds\n\
sub_free(%#lx), size: %#x, next %#lx\n",
                    (long) e2mbp, (long) cp, hp->h_size, (long) hp->h_next);
        else
            sub_free(hp->h_next, cp);
        return;
    }
#ifdef DEBUG_TRACK
    e2mbp->tot_used -= hp->h_size + 1;
    fprintf(stderr,"F:%#lx:%u:%#lx\n", (long) e2mbp, hp->h_size, (long) cp);
                    (long) e2mbp,
    fprintf(stderr,"%#lx:sub_free() returned %d at %#lx\n",
                    (long) e2mbp,hp->h_size * sizeof(HEADER), (long)(hp + 1)); 
    fflush(stderr);
    if (hp->h_next != (HEADER *) e2mbp)
    {
        (void) fprintf(stderr,
 "LOGIC ERROR: %#lx:Write outside bounds sub_free(%#lx), size: %#x, next %#lx\n",
                    (long) e2mbp, (long) cp, hp->h_size, (long) hp->h_next);
        return;
    }
    if (hp->h_size > HIGHWATER || hp->h_size <= 0)
    {
        (void) fprintf(stderr,
   "LOGIC ERROR: %#lx:Unusual address in sub_free %#lx, size: %d, next %#lx\n",
                    (long) e2mbp, (long) cp, hp->h_size, (long) hp->h_next);
        return;
    }
#endif
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
    EnterCriticalSection(&(e2mbp->cs));
#endif
#endif
#if 0
    if (hp->h_size >= REUSE_THRESHOLD)
    {
#ifdef E2_THREAD_SAFE
        pthread_mutex_lock(&(e2mbp->rf_mutex));
#endif
        real_free(cp);
#ifdef E2_THREAD_SAFE
        pthread_mutex_unlock(&(e2mbp->rf_mutex));
#endif
    }
    else
#endif
    {
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
        pthread_mutex_lock(&(e2mbp->lz_mutex));
#endif
#endif
#ifdef FRAG_TRACKING 
        e2mbp->root_node = sub_b_node_add(e2mbp,e2mbp->root_node, hp, cmp_size);
#else
        *e2mbp->unfreed_next = hp;
        e2mbp->unfreed_next++;
#endif
        e2mbp->unfreed_waiting +=  hp->h_size + 1;
        if (
#ifdef FRAG_TRACKING
            e2mbp->node_cnt >= BACKLOG ||
#else
            e2mbp->unfreed_next >= &e2mbp->unfreed[BACKLOG] ||
#endif
            e2mbp->unfreed_waiting >= e2mbp->os_alloc)
            sub_free_unfreed(e2mbp);
#ifdef NESTED_CONCURRENT
#ifdef E2_THREAD_SAFE
        else
            pthread_mutex_unlock(&(e2mbp->lz_mutex));
#endif
#endif
    }
#ifdef NESTED_CONCURRENT
#ifdef MINGW32
    LeaveCriticalSection(&(e2mbp->cs));
#endif
#endif
    return;
}
#ifdef FRAG_TRACKING
/*
 * Allocate from the pool of nodes
 */
static struct b_node * sub_b_node_alloc(e2mbp)
struct e2malloc_base * e2mbp;
{
unsigned int i;
unsigned int j;
unsigned int k;
unsigned int top;
struct b_node * ret;

    i = e2mbp->try_from;
    top = B_NODE_ALLOC/32;
   
repeat:
    for (; i < top; i++)
    {
        if (e2mbp->alloc_map[i] != 0xffffffff)
        {
            for (j = 0, k = 1; j < 32; j++, k<<=1)
            {
                if (!(e2mbp->alloc_map[i] & k))
                {
                    e2mbp->alloc_map[i] |= k;
                    ret = &e2mbp->alloc_nodes[ (i << 5) + j];
                    memset(ret, 0, sizeof(struct b_node));
                    e2mbp->node_cnt++;
                    e2mbp->try_from++;
                    if ( e2mbp->try_from >= B_NODE_ALLOC/32)
                        e2mbp->try_from = 0;
                    return ret;
                }
            } 
        }
    }
    if (i == e2mbp->try_from)
        return NULL;
    i = 0;
    top = e2mbp->try_from;
    goto repeat;
}
/*
 * Return to pool
 */
static void sub_b_node_free(e2mbp, bnp)
struct e2malloc_base * e2mbp;
struct b_node * bnp;
{
    e2mbp->alloc_map[((bnp - e2mbp->alloc_nodes) >>5)] &=
                  ~(1 << ((bnp - e2mbp->alloc_nodes) & 31));
    e2mbp->node_cnt--;
    e2mbp->try_from = (bnp - e2mbp->alloc_nodes)>>5;
#ifdef DEBUG
    bnp->key = 23;
    bnp->child_left = 7;
    bnp->child_right = 11;
#endif
    return;
}

static struct b_node * sub_b_node_add();
/*
 * Allocate a root
 */
static struct b_node * sub_b_node_init(e2mbp)
struct e2malloc_base * e2mbp;
{
    return sub_b_node_alloc(e2mbp);
}
/*
 * Re-balance if out of kilter
 */
static struct b_node * sub_b_node_balance(e2mbp, root, cmp)
struct e2malloc_base * e2mbp;
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
        if (root == root->child_left || root == root->child_right)
            abort();
        if (root->child_right->cnt_right >= 
                   (root->child_right->cnt_left + root->cnt_left + 2))
        {
            x = root->key;
            child = root->child_left;
            parent = root->child_right;
            sub_b_node_free(e2mbp, root);
            for ( root = parent;
                    parent->child_left != NULL;
                        parent = parent->child_left)
                parent->cnt_left +=  child->cnt_left + child->cnt_right + 2;
            parent->cnt_left +=  child->cnt_left + child->cnt_right + 2;
            parent->child_left = sub_b_node_add(e2mbp, child, x, cmp);
            root = sub_b_node_balance(e2mbp, root, cmp);
        }
        else
/*        if (left_rebal < cur_bal -16 && left_rebal < right_rebal) */
        if (root->child_left->cnt_left >= 
            (root->child_left->cnt_right + root->cnt_right + 2))
        {
            x = root->key;
            child = root->child_right;
            parent = root->child_left;
            sub_b_node_free(e2mbp, root);
            for (root = parent;
                    parent->child_right != NULL;
                        parent = parent->child_right)
                parent->cnt_right +=  child->cnt_left + child->cnt_right + 2;
            parent->cnt_right +=  child->cnt_left + child->cnt_right + 2;
            parent->child_right =  sub_b_node_add(e2mbp, child, x, cmp);
            root = sub_b_node_balance(e2mbp, root, cmp);
        }
    }
    return root;
}
/*
 * Add an entry - find out how balanced it stays
 *
 * The lame hand-optimisations actually make a big difference
 */
static struct b_node * sub_b_node_add(e2mbp, root, key, cmp)
struct e2malloc_base * e2mbp;
struct b_node * root;
unsigned char * key; 
int (*cmp)();
{
struct b_node * parent;
int ret;

    if (root == NULL)
        root = sub_b_node_init(e2mbp);
    if (root->key == NULL)
        root->key = key;
    else
    if ((ret = cmp(key, root->key)) == 0)
    {
        if (key == root->key)
            fprintf(stderr, "%#lx:Freed(%#lx) twice\n",
                    (long) e2mbp, (long) key);
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
                        (long) e2mbp, (long) key);
                    break;
                }
            }
            if (parent == NULL)
                root->child_middle = sub_b_node_add(e2mbp, NULL, key, cmp);
            else
            if ( parent->key != key)
                parent->child_middle = sub_b_node_add(e2mbp, NULL, key, cmp);
        }
        return root;   /* Don't rebalance if matched */
    }
    else
    if (ret < 0)
    {
        if (root->child_left == NULL)
        {
            root->child_left = sub_b_node_add(e2mbp, NULL, key, cmp);
            root->cnt_left = 1;
        }
        else
        if (ret
           && root->child_right == NULL
           && root->cnt_left > 1)
        {
            parent = root;
            root = sub_b_node_add(e2mbp, root->child_left, parent->key, cmp);
            sub_b_node_free(e2mbp, parent);
            root = sub_b_node_add(e2mbp, root, key, cmp);
        }
        else
        {
            root->child_left = sub_b_node_add(e2mbp, root->child_left, key, cmp);
            root->cnt_left++;
        }
    }
    else
    {
        if (root->child_right == NULL)
        {
            root->child_right = sub_b_node_add(e2mbp, NULL, key, cmp);
            root->cnt_right = 1;
        }
        else
        if (root->child_left == NULL && root->cnt_right > 1)
        {
            parent = root;
            root = sub_b_node_add(e2mbp, root->child_right, parent->key, cmp);
            sub_b_node_free(e2mbp, parent);
            root = sub_b_node_add(e2mbp, root, key, cmp);
        }
        else
        {
            root->child_right = sub_b_node_add(e2mbp, root->child_right, key, cmp);
            root->cnt_right++;
        }
    }
    return sub_b_node_balance(e2mbp, root, cmp);
}
/*
 * Find an entry, and remove it. The logic knows about what we are looking
 * for, so provides cmp_size() to b_node_balance
 */
static struct b_node * sub_b_node_pop(e2mbp, root, nunits, pop_key)
struct e2malloc_base * e2mbp;
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
        sub_b_node_free(e2mbp, root);
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
            sub_b_node_free(e2mbp, parent);
            return root;
        }
        else
        if (root->child_left == NULL && root->child_right == NULL)
        {
            sub_b_node_free(e2mbp, root);
            return NULL;
        }
        else
        if (root->child_left == NULL)
        {
            child = root->child_right;
            sub_b_node_free(e2mbp, root);
            return child;
        }
        else
        if (root->child_right == NULL)
        {
            child = root->child_left;
            sub_b_node_free(e2mbp, root);
            return child;
        }
        else
        {
            child = root->child_left;
            parent = root->child_right;
            sub_b_node_free(e2mbp, root);
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
               sub_b_node_pop(e2mbp, root->child_left, nunits, pop_key);
    }
    else
    if (root->child_right != NULL)
        root->child_right =
               sub_b_node_pop(e2mbp, root->child_right, nunits, pop_key);
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
    return sub_b_node_balance(e2mbp, root, cmp_size);
}
#endif
#endif
#ifdef USE_E2_CODE
#ifdef THREADED_MALLOC
/*
 * Using thread-local variable
 */
static __thread struct e2malloc_base * e2mbp;

void malloc_thread_setup( struct e2malloc_base * ip)
{
    e2mbp = ip;
    return;
}
void *malloc (size_t __size)
{
char *chk;

    if (e2mbp == NULL)
    {
        chk = e2malloc(__size);
        return chk;
    }
    else
        return sub_malloc(e2mbp, __size);
}
char * strdup(s1)
char * s1;
{
    if (e2mbp == NULL)
        return e2strdup(s1);
    else
        return sub_strdup(e2mbp, s1);
}
/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *calloc (size_t __nmemb, size_t __size)
{
    if (e2mbp == NULL)
        return e2calloc(__nmemb, __size);
    else
        return sub_calloc(e2mbp, __nmemb, __size);
}
/* Re-allocate the previously allocated block in __ptr, making the new
   block SIZE bytes long.  */
void *realloc (void *__ptr, size_t __size)
{
    if (e2mbp == NULL)
        return e2realloc(__ptr, __size);
    else
        return sub_realloc(e2mbp, __ptr, __size);
}
/* Free a block allocated by `malloc', `realloc' or `calloc'.  */
void free (void *__ptr)
{
    if (e2mbp == NULL)
        e2free(__ptr);
    else
        sub_free(e2mbp, __ptr);
    return;
}
#endif
#else
void malloc_thread_setup( struct e2malloc_base * ip)
{
}
#endif
