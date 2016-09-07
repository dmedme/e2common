/*******************************************************************************
 * You get thread-safe operation if:
 * -    MINGW32 is defined (all our Windows programs are multi-threaded, since
 *      the e2compat.c alarm() implementation uses a second thread).
 * OR
 * -    E2_THREAD_SAFE is defined (in which case we use two mutexes rather than
 *      a single Critical Section) 
 "@(#) $Name$ $Id$ Copyright (c) E2 Systems Limited 2012
 */
#ifndef SUBMALLOC_H
#define SUBMALLOC_H
#include <stdio.h>
#ifndef MINGW32
#ifdef E2_THREAD_SAFE
#include <pthread.h>
#endif
#endif
/* amount to allocate at one time via sbrk */
#ifndef NALLOC
#ifdef MINGW32
#define NALLOC    8*1024*1024
#else
#define NALLOC    65536
#endif
#endif /* !NALLOC */

/* high water mark for allocation - any more than this is an error */
#ifndef HIGHWATER
#define HIGHWATER    33554432
#endif /* !HIGHWATER */
#define OUTPUT 1
#define QUIET 0
/*
 * Each block of memory allocated has a header
 */
typedef struct header {
    struct header   *h_next;    /* start of next header */
    unsigned int    h_size;     /* size of block (excl. header),
                                   in units of sizeof(HEADER) */
} HEADER;
/*
 * Defines that affect the functionality
 * =====================================
 * No defines; vanilla behaviour. Just manages a singly linked list of items.
 * DEBUG - debugging code
 * DEBUG_FULL - debugging code plus frequent arena validation
 * QUICK_REPLACE - hash entries from the free list to speed up finding where
 *                 to put things back
 * LAZY_FREE - hang on to free for a bit before returning it to the free list
 * FRAG_TRACKING - complex management of free stuff. LAZY_FREE must also be
 * defined to use this.
 * Each option has configuration sub-options, discussed below.
 *
 * Quick replace in particular benefits from aggressive compiler optimisation.
 */
#define QUICK_REPLACE
#define START_HASH_BITS 2
#define FINAL_HASH_BITS 10
/*
 * Lazy free. Backlog is the number of unrecovered elements we tolerate.
 * There is no point in defining FRAG_TRACKING if we are not interested in
 * searching the backlog, since it just adds extra overhead.
 *
 * The following settings worked well on a huge network capture.
 * BACKLOG = 2048
 * START_HASH_BITS 6
 * END_HASH_BITS 14
 */
#define LAZY_FREE
#define BACKLOG 128
#define SEARCH_BACKLOG

#define REUSE_THRESHOLD 8192
#define FRAG_TRACKING
/*
 * The globals are collected together for ease of recognition, and for future
 * multi-thread enhancements.
 */
#ifdef E2MALLOC_BUILD
#ifdef MINGW32
/*
 * Windows stuff, so that we don't have to include Windows headers, which
 * give us grief over function signature discrepancies.
 */
#define WINAPI __stdcall
typedef unsigned short WORD;
typedef unsigned long LONG;
typedef unsigned long DWORD;
typedef void * HANDLE;
typedef struct _LIST_ENTRY {
	struct _LIST_ENTRY *Flink;
	struct _LIST_ENTRY *Blink;
} LIST_ENTRY,*PLIST_ENTRY;
typedef struct _CRITICAL_SECTION_DEBUG {
	WORD Type;
	WORD CreatorBackTraceIndex;
	struct _CRITICAL_SECTION *CriticalSection;
	LIST_ENTRY ProcessLocksList;
	DWORD EntryCount;
	DWORD ContentionCount;
	DWORD Spare [2];
} CRITICAL_SECTION_DEBUG,*PCRITICAL_SECTION_DEBUG;
typedef struct _CRITICAL_SECTION {
	PCRITICAL_SECTION_DEBUG DebugInfo;
	LONG LockCount;
	LONG RecursionCount;
	HANDLE OwningThread;
	HANDLE LockSemaphore;
	DWORD SpinCount;
} CRITICAL_SECTION,*PCRITICAL_SECTION,*LPCRITICAL_SECTION;
#ifndef WINBASEAPI
#ifdef __W32API_USE_DLLIMPORT__
#define WINBASEAPI DECLSPEC_IMPORT
#else
#define WINBASEAPI
#endif
#endif
WINBASEAPI void WINAPI InitializeCriticalSection(LPCRITICAL_SECTION);
WINBASEAPI void WINAPI EnterCriticalSection(LPCRITICAL_SECTION);
WINBASEAPI void WINAPI LeaveCriticalSection(LPCRITICAL_SECTION);
WINBASEAPI void WINAPI Sleep(DWORD);
WINBASEAPI LONG WINAPI InterlockedIncrement(LONG volatile *);
WINBASEAPI LONG WINAPI InterlockedCompareExchange(LONG volatile *, LONG, LONG);
#endif
#endif
#define B_NODE_ALLOC (BACKLOG+32)
#ifdef FRAG_TRACKING
struct b_node {
    struct b_node * child_left;
    struct b_node * child_middle;
    struct b_node * child_right;
    unsigned char * key;
    int cnt_left;
    int cnt_right;
};
#endif
struct pbite {
    char * pchunk;
    unsigned long psize;
};
/*
 * Structure for tracking parent allocator grabs.
 */
struct pbuc {
    int buc_cnt;
    struct pbite grab[32];
    struct pbuc * next_buc;
};
struct e2malloc_base {
    HEADER    *freep;
    HEADER    base;  /* Mustn't be the first or it shares its address ... */
    char * alloced_base;    /* Space to remember allocations            */
    int os_alloc;                     /* Piece to ask for from the OS   */
    int free_cnt;                     /* The number of free items       */
    struct pbuc root_buc; 
    struct pbuc * last_buc;
#ifdef MINGW32
    CRITICAL_SECTION cs;
#else
#ifdef E2_THREAD_SAFE
    pthread_mutex_t lz_mutex;
    pthread_mutex_t rf_mutex;
#endif
#endif
#ifdef QUICK_REPLACE
/*
 * You need to be careful changing START_HASH_BITS and FINAL_HASH_BITS, in the
 * light of the fact it starts with quick_table_1, and finishes with it.
 * FINAL_HASH_BITS - START_HASH_BITS must be an even number.
 * If the number of increments (it goes up 2 at a time) was odd rather than
 * even, quick_table_2 would have to be 4 times bigger than quick_table_1
 * rather than the way round that it is
 */
    HEADER *quick_table_1[1 << (FINAL_HASH_BITS + 1)];
    HEADER *quick_table_2[1 << (FINAL_HASH_BITS - 1)];
    HEADER **quick_table;             /* The one we are currently using */
    HEADER **quick_bound;             /* Hash table bound               */
    long hash_step;                    /* The hash table grow trigger    */
    int quick_bits;                   /* How much we take for the hash  */
#endif
    unsigned long tot_free;           /* Total space free               */
    unsigned long tot_used;           /* Total space free               */
    unsigned long unfreed_waiting;    /* Total space waiting            */
#ifdef LAZY_FREE
    char ** unfreed_next;             /* Next free slot                 */
    char * unfreed[B_NODE_ALLOC];     /* Waiting to be freed            */
/*
 * Map of fragments being tracked
 */
#ifdef FRAG_TRACKING
    int node_cnt;
    int try_from;
    struct b_node * root_node;
    unsigned int alloc_map[B_NODE_ALLOC/32];
    struct b_node alloc_nodes[B_NODE_ALLOC];
#endif
#endif
};
#ifdef LAZY_FREE
#define DONT_NEED_TO_SEE static
static void free_unfreed();
#ifdef SEARCH_BACKLOG
static HEADER * reuse_unfreed();
#endif
__inline void mqarrsort();
#else
#define real_free free
#define DONT_NEED_TO_SEE
#endif
void free();
DONT_NEED_TO_SEE void real_free();
int cmp_size();
/*
 * Pick our array point. There may be an intrinsic for this. The following
 * legible code represents the macro below (in 32 bit form)
 *
    if (n < (1 << 16))
    {
        if (n < (1 << 8))
        {
            if (n < (1 << 4))
            {
                if (n < (2 << 2))
                {
                    if (n < 2)
                        i = 0;
                    else
                        i = 1;
                }
                else
                {
                    if (n < (1 << 3))
                        i = 2;
                    else
                        i = 3;
                }
            }
            else
            {
                if (n < (1 << 6))
                {
                    if (n < (1 << 5))
                        i = 4;
                    else
                        i = 5;
                }
                else
                {
                    if (1 < (2 << 7))
                        i = 6;
                    else
                        i = 7;
                }
            }
        }
        else
        {
            if (n < (1 << 12))
            {
                if (n < (1 << 10))
                {
                    if (n < (1 << 9))
                        i = 8;
                    else
                        i = 9;
                }
                else
                {
                    if (n < (1 << 11))
                        i = 10;
                    else
                        i = 11;
                }
            }
            else
            {
                if (n < (1 << 14))
                {
                    if (n < (1 << 13))
                        i = 12;
                    else
                        i = 13;
                }
                else
                {
                    if (n < (1 << 15))
                        i = 14;
                    else
                        i = 15;
                }
            }
        }
    }
    else
    {
        if (n < (1 << 24))
        {
            if (n < (1 << 20))
            {
                if (n < (1 << 18))
                {
                    if (n < (1 << 17))
                        i = 16;
                    else
                        i = 17;
                }
                else
                {
                    if (n < (1 << 19))
                        i = 18;
                    else
                        i = 19;
                }
            }
            else
            {
                if (n < (1 << 22))
                {
                    if (n < (1 << 21))
                        i = 20;
                    else
                        i = 21;
                }
                else
                {
                    if (n < (1 << 23))
                        i = 22;
                    else
                        i = 23;
                }
            }
        }
        else
        {
            if (n < (1 << 28))
            {
                if (n < (1 << 26))
                {
                    if (n < (1 << 25))
                        i = 24;
                    else
                        i = 25;
                }
                else
                {
                    if (n < (1 << 27))
                        i = 26;
                    else
                        i = 27;
                }
            }
            else
            {
                if (n < (1 << 30))
                {
                    if (n < (1 << 29))
                        i = 28;
                    else
                        i = 29;
                }
                else
                {
                    if (n < (1 << 31))
                        i = 30;
                    else
                        i = 31;
                }
            }
        }
    }
 */
#ifdef SOLAR
#ifdef _XBS5_LP64_OFF64
#define qpick_ind qpick_ind64
#else
#define qpick_ind qpick_ind32
#endif
#else
#ifdef LCC
#include <intrinsics.h>
#define qpick_ind(n) ((n < 2)?0:(_bsr(n)))
#else
#ifdef LINUX
/*
 * Assume AMD64 or x86
 */
long bsr();
#define qpick_ind(n) ((n < 2)?0:(bsr(n)))
#else
#define qpick_ind64(n) ((n)>(1<<32))?(32 + qpick_ind32((n)>>32)):(qpick_ind32((n)))
#define qpick_ind32(n) (((n)<(1<<16))?(((n)<(1<<8))?(((n)<(1<<4))?(((n)<(1<<2))?(((n)<2)?0:1):(((n)<(1<<3))?2:3)):\
(((n)<(1<<6))?(((n)<(1<<5))?4:5):(((n)<(1<<7))?6:7))):((((n)<(1<<12))?((((n)<(1<<10))?(((n)<(1<<9))?8:9)\
:(((n)<(1<<11))?10:11))):(((n)<(1<<14))?(((n)<(1<<13))?12:13):(((n)<(1<<15))?14:15))))):(\
((n)<(1<<24))?(((n)<(1<<20))?(((n)<(1<<18))?(((n)<(1<<17))?16:17):(((n)<(1<<19))?18:19)):(\
((n)<(1<<22))?(((n)<(1<<21))?20:21):(((n)<(1<<23))?22:23))):(((n)<(1<<28))?(((n)<(1<<26))?((((n)<(1<<25))?24:25)):(\
(((n)<(1<<27)?26:27)))):(((n)<(1<<30))?(((n)<(1<<29))?28:29):(((n)<(1<<31))?30:31)))))
#ifdef _XBS5_LP64_OFF64
#define qpick_ind qpick_ind64
#else
#define qpick_ind qpick_ind32
#endif
#endif
#endif
#endif
void sub_e2free();
void sub_free();
char * sub_malloc();
char * sub_calloc();
char * sub_realloc();
void sub_bfree();
#ifdef USE_E2_CODE
#ifdef E2_THREAD_SAFE 
#ifndef THREADED_MALLOC
#define THREADED_MALLOC
#endif
#endif
#endif
#ifdef THREADED_MALLOC
void malloc_thread_setup( struct e2malloc_base * ip);
void *malloc (size_t __size);
/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *calloc (size_t __nmemb, size_t __size);
/* Re-allocate the previously allocated block in __ptr, making the new
   block SIZE bytes long.  */
void *realloc (void *__ptr, size_t __size);
/* Free a block allocated by `malloc', `realloc' or `calloc'.  */
void free (void *__ptr);
#ifdef strdup
#undef strdup
#endif
#ifdef MINGW32
#define free e2free
#define malloc e2malloc
#define realloc e2realloc
#define calloc e2calloc
#define strdup e2strdup
#endif
char * strdup ();
char * e2strdup ();
#else
#define e2malloc malloc
#define e2calloc calloc
#define e2realloc realloc
#define e2free free
#define e2strdup strdup
#endif
void e2free();
char * e2malloc();
char * e2calloc();
char * e2realloc();
void bfree();
#endif
