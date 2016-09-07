/**************************
 * hashlib.c - routines for maintaining hash tables
 *
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright(c) E2 Systems 1993\n";
#include "hashlib.h"
#include <stdio.h>
#include <stdlib.h>
/*
 * Create a hash table
 */
HASH_CON * hash(sz, hash_to_use, comp_to_use)
int sz;
HASHFUNC hash_to_use;
COMPFUNC comp_to_use;
{
HASH_CON * ret;
HIPT * table;
int i;

    for (i = 2; i < sz; i = (i << 1));
    table = (HIPT *) calloc(i,sizeof(HIPT));
    if (!table)
        return (HASH_CON *) 0;
    ret = (HASH_CON *) malloc (sizeof(*ret));
    ret->table = table;
    ret->tabsize = i;
    ret->hashfunc = hash_to_use;
    ret->compfunc = comp_to_use;
    return ret;
}
/*
 * Double the size of the hash table. So how do we know to call it?
 */
static void rehash(con)
HASH_CON * con;
{
HASH_CON * ncon;
int i;
int used;
HIPT * p;
HIPT * q;

/*
 * Avoid trouble if the hash function is useless.
 */
    for (i = 0, used = 0;  i < con->tabsize; i++)
        if (con->table[i].in_use)
            used++;
    if (used + used < i)
        return;
    ncon = hash(con->tabsize + con->tabsize, con->hashfunc, con->compfunc);
/*
 * Re-create in the new structure, free malloc()ed space off old.
 */
    for (i = 0; i < con->tabsize; i++)
    {
        p = &con->table[i];
        do
        {
            if (p->in_use)
                insert(ncon, p->name, p->body);
            q = p;
            p = p->next;
            if (q !=  &con->table[i])
                free(q);
        }
        while(p);
    }
    p = con->table;
    *con = *ncon;
    free(p);
    free(ncon);
    return;
}
/*
 * Add an entry to the hash table
 */
HIPT * insert(con,item,data)
HASH_CON * con;
char *item;
char * data;
{
unsigned hv;              /* hash value */
HIPT * np;    

    hv = (*(con->hashfunc))(item,con->tabsize);     /* get hash # */
    np = &con->table[hv];
    hv = 0;
    if (np->in_use)
    {               /* chain exists */
        for (;;)
        {
            if (!((*(con->compfunc))(np->name,item)))
                return np;
            if (!np->next)
                break;
            np = np->next;              /* hop along chain */
            hv++;
        }
        np->next = (HIPT *) calloc(1,sizeof(HIPT));
                                        /* allocate new item */
        if (!np->next)
            return (HIPT *) 0;
        np = np->next;
    }
    np->name = item;          /* fill it in */
    np->body = data;
    np->in_use = 1;
    np->next = 0;
    if (hv > 8)
        rehash(con);
    return np;
}
/*
 * Function to delete element
 * There are dire consequences if more than one element matches, and
 * the wrong one is deleted; the insert() does not prevent this.
 */
int hremove(con, item)
HASH_CON * con;
char *item;
{
unsigned hv;
HIPT *np, *last;    

    if (!item)
        return -1;
    hv = (*(con->hashfunc))(item,con->tabsize);
    np = &con->table[hv];
    if (!np->in_use)                            /* no such item */
        return(-1);
    if (!np->next)
    {                    /* only item on this chain */
        if (!(*(con->compfunc))(item,np->name))
        {
            np->in_use = 0;
            return(0);
        }
        else
            return(-1);
    }
    else                              /* first item on chain matches */
    if (!(*(con->compfunc))(item, np->name))
    {
        last = np->next;
        *np = *last;
        free(last);
        return(0);
    }
    else
    {                            /* must look along chain */
        while (np)
        {
            last = np;
            if ((np = np->next) && (!(*(con->compfunc))(item, np->name)))
            {   /* match found */
                last->next = np->next;      /* link it out */
                free(np);
                return(0);
            }
        }
        return(-1);
    }
}
/*
 * Function to find element
 */
HIPT * lookup(con, p)
HASH_CON * con;
char * p;
{
unsigned hv;
HIPT * np;
#ifdef DEBUG
int chain_cnt = 0;
#endif

#ifdef DEBUG_FULL
    fprintf(stderr,"Before calculation\n");
    fflush(stderr);
    fprintf(stderr,"tabsize %d\n", con->tabsize);
    fflush(stderr);
    fprintf(stderr,"hashfunc %lx\n", (long) con->hashfunc);
    fflush(stderr);
#endif

    if ((hv = (*(con->hashfunc))(p,con->tabsize))
          < 0 || hv >= con->tabsize)
    {
        fprintf(stderr,"Hash out of range? tabsize %d, hash %d\n",
                        con->tabsize, hv);
#ifdef DEBUG
        fflush(stderr);
        abort();
#endif
    }
    np = &con->table[hv];
#ifdef DEBUG_FULL
    fprintf(stderr,"Starting point: %u %lx\n", hv, (long) np);
    fflush(stderr);
#endif
    if (np->in_use)
    for (; np; np = np->next)
    {
#ifdef DEBUG_FULL
        fprintf(stderr,"Searching ...\n");
        fflush(stderr);
#endif
#ifdef DEBUG
        chain_cnt++;
#endif
        if (!((*(con->compfunc))(np->name,p)))
        {
#ifdef DEBUG
            fprintf(stderr, "Found after %d\n", chain_cnt);
#endif
            return(np);
        }
    }
#ifdef DEBUG
    fprintf(stderr,"Not found after %d\n", chain_cnt);
#endif
#ifdef DEBUG_FULL
    fflush(stderr);
#endif
    return (HIPT *) 0;
}
/*
 * Do something to everything; the functions take a name pointer
 * and a body pointer as their arguments.
 *
 * Use eg. to reclaim space before destroying the table.
 */
void iterate(con,fun_name,fun_body)
HASH_CON * con;
void (*fun_name)();
void (*fun_body)();
{
int i;
HIPT * p;

    for (i = 0; i < con->tabsize; i++)
    {
        p = &con->table[i];
        do
        {
            if (p->in_use)
            {
               if (fun_name)
                   (*fun_name)(p->name);
               if (fun_body)
                   (*fun_body)(p->body);
            }
            p = p->next;
        }
        while(p);
    }
    return;
}
/*
 * Get rid of everything
 */
void cleanup(con)
HASH_CON * con;
{
    int i;
    HIPT * p, * q;

    for (i = 0; i < con->tabsize; i++)
    {
        p = &con->table[i];
        if (p->in_use)
        for (p= p->next; p;)
        {
            q = p;
            p = p->next;
            free(q);
        }
    }
    free(con->table);
    free(con);
    return;
}
/*
 * Hash function for string keys
 */
static unsigned long int crc64_table[256] = {
0,
0xb32e4cbe03a75f6fUL,
0xf4843657a840a05bUL,
0x47aa7ae9abe7ff34UL,
0x7bd0c384ff8f5e33UL,
0xc8fe8f3afc28015cUL,
0x8f54f5d357cffe68UL,
0x3c7ab96d5468a107UL,
0xf7a18709ff1ebc66UL,
0x448fcbb7fcb9e309UL,
0x325b15e575e1c3dUL,
0xb00bfde054f94352UL,
0x8c71448d0091e255UL,
0x3f5f08330336bd3aUL,
0x78f572daa8d1420eUL,
0xcbdb3e64ab761d61UL,
0x7d9ba13851336649UL,
0xceb5ed8652943926UL,
0x891f976ff973c612UL,
0x3a31dbd1fad4997dUL,
0x64b62bcaebc387aUL,
0xb5652e02ad1b6715UL,
0xf2cf54eb06fc9821UL,
0x41e11855055bc74eUL,
0x8a3a2631ae2dda2fUL,
0x39146a8fad8a8540UL,
0x7ebe1066066d7a74UL,
0xcd905cd805ca251bUL,
0xf1eae5b551a2841cUL,
0x42c4a90b5205db73UL,
0x56ed3e2f9e22447UL,
0xb6409f5cfa457b28UL,
0xfb374270a266cc92UL,
0x48190ecea1c193fdUL,
0xfb374270a266cc9UL,
0xbc9d3899098133a6UL,
0x80e781f45de992a1UL,
0x33c9cd4a5e4ecdceUL,
0x7463b7a3f5a932faUL,
0xc74dfb1df60e6d95UL,
0xc96c5795d7870f4UL,
0xbfb889c75edf2f9bUL,
0xf812f32ef538d0afUL,
0x4b3cbf90f69f8fc0UL,
0x774606fda2f72ec7UL,
0xc4684a43a15071a8UL,
0x83c230aa0ab78e9cUL,
0x30ec7c140910d1f3UL,
0x86ace348f355aadbUL,
0x3582aff6f0f2f5b4UL,
0x7228d51f5b150a80UL,
0xc10699a158b255efUL,
0xfd7c20cc0cdaf4e8UL,
0x4e526c720f7dab87UL,
0x9f8169ba49a54b3UL,
0xbad65a25a73d0bdcUL,
0x710d64410c4b16bdUL,
0xc22328ff0fec49d2UL,
0x85895216a40bb6e6UL,
0x36a71ea8a7ace989UL,
0xadda7c5f3c4488eUL,
0xb9f3eb7bf06317e1UL,
0xfe5991925b84e8d5UL,
0x4d77dd2c5823b7baUL,
0x64b62bcaebc387a1UL,
0xd7986774e864d8ceUL,
0x90321d9d438327faUL,
0x231c512340247895UL,
0x1f66e84e144cd992UL,
0xac48a4f017eb86fdUL,
0xebe2de19bc0c79c9UL,
0x58cc92a7bfab26a6UL,
0x9317acc314dd3bc7UL,
0x2039e07d177a64a8UL,
0x67939a94bc9d9b9cUL,
0xd4bdd62abf3ac4f3UL,
0xe8c76f47eb5265f4UL,
0x5be923f9e8f53a9bUL,
0x1c4359104312c5afUL,
0xaf6d15ae40b59ac0UL,
0x192d8af2baf0e1e8UL,
0xaa03c64cb957be87UL,
0xeda9bca512b041b3UL,
0x5e87f01b11171edcUL,
0x62fd4976457fbfdbUL,
0xd1d305c846d8e0b4UL,
0x96797f21ed3f1f80UL,
0x2557339fee9840efUL,
0xee8c0dfb45ee5d8eUL,
0x5da24145464902e1UL,
0x1a083bacedaefdd5UL,
0xa9267712ee09a2baUL,
0x955cce7fba6103bdUL,
0x267282c1b9c65cd2UL,
0x61d8f8281221a3e6UL,
0xd2f6b4961186fc89UL,
0x9f8169ba49a54b33UL,
0x2caf25044a02145cUL,
0x6b055fede1e5eb68UL,
0xd82b1353e242b407UL,
0xe451aa3eb62a1500UL,
0x577fe680b58d4a6fUL,
0x10d59c691e6ab55bUL,
0xa3fbd0d71dcdea34UL,
0x6820eeb3b6bbf755UL,
0xdb0ea20db51ca83aUL,
0x9ca4d8e41efb570eUL,
0x2f8a945a1d5c0861UL,
0x13f02d374934a966UL,
0xa0de61894a93f609UL,
0xe7741b60e174093dUL,
0x545a57dee2d35652UL,
0xe21ac88218962d7aUL,
0x5134843c1b317215UL,
0x169efed5b0d68d21UL,
0xa5b0b26bb371d24eUL,
0x99ca0b06e7197349UL,
0x2ae447b8e4be2c26UL,
0x6d4e3d514f59d312UL,
0xde6071ef4cfe8c7dUL,
0x15bb4f8be788911cUL,
0xa6950335e42fce73UL,
0xe13f79dc4fc83147UL,
0x521135624c6f6e28UL,
0x6e6b8c0f1807cf2fUL,
0xdd45c0b11ba09040UL,
0x9aefba58b0476f74UL,
0x29c1f6e6b3e0301bUL,
0xc96c5795d7870f42UL,
0x7a421b2bd420502dUL,
0x3de861c27fc7af19UL,
0x8ec62d7c7c60f076UL,
0xb2bc941128085171UL,
0x192d8af2baf0e1eUL,
0x4638a2468048f12aUL,
0xf516eef883efae45UL,
0x3ecdd09c2899b324UL,
0x8de39c222b3eec4bUL,
0xca49e6cb80d9137fUL,
0x7967aa75837e4c10UL,
0x451d1318d716ed17UL,
0xf6335fa6d4b1b278UL,
0xb199254f7f564d4cUL,
0x2b769f17cf11223UL,
0xb4f7f6ad86b4690bUL,
0x7d9ba1385133664UL,
0x4073c0fa2ef4c950UL,
0xf35d8c442d53963fUL,
0xcf273529793b3738UL,
0x7c0979977a9c6857UL,
0x3ba3037ed17b9763UL,
0x888d4fc0d2dcc80cUL,
0x435671a479aad56dUL,
0xf0783d1a7a0d8a02UL,
0xb7d247f3d1ea7536UL,
0x4fc0b4dd24d2a59UL,
0x3886b22086258b5eUL,
0x8ba8fe9e8582d431UL,
0xcc0284772e652b05UL,
0x7f2cc8c92dc2746aUL,
0x325b15e575e1c3d0UL,
0x8175595b76469cbfUL,
0xc6df23b2dda1638bUL,
0x75f16f0cde063ce4UL,
0x498bd6618a6e9de3UL,
0xfaa59adf89c9c28cUL,
0xbd0fe036222e3db8UL,
0xe21ac88218962d7UL,
0xc5fa92ec8aff7fb6UL,
0x76d4de52895820d9UL,
0x317ea4bb22bfdfedUL,
0x8250e80521188082UL,
0xbe2a516875702185UL,
0xd041dd676d77eeaUL,
0x4aae673fdd3081deUL,
0xf9802b81de97deb1UL,
0x4fc0b4dd24d2a599UL,
0xfceef8632775faf6UL,
0xbb44828a8c9205c2UL,
0x86ace348f355aadUL,
0x34107759db5dfbaaUL,
0x873e3be7d8faa4c5UL,
0xc094410e731d5bf1UL,
0x73ba0db070ba049eUL,
0xb86133d4dbcc19ffUL,
0xb4f7f6ad86b4690UL,
0x4ce50583738cb9a4UL,
0xffcb493d702be6cbUL,
0xc3b1f050244347ccUL,
0x709fbcee27e418a3UL,
0x3735c6078c03e797UL,
0x841b8ab98fa4b8f8UL,
0xadda7c5f3c4488e3UL,
0x1ef430e13fe3d78cUL,
0x595e4a08940428b8UL,
0xea7006b697a377d7UL,
0xd60abfdbc3cbd6d0UL,
0x6524f365c06c89bfUL,
0x228e898c6b8b768bUL,
0x91a0c532682c29e4UL,
0x5a7bfb56c35a3485UL,
0xe955b7e8c0fd6beaUL,
0xaeffcd016b1a94deUL,
0x1dd181bf68bdcbb1UL,
0x21ab38d23cd56ab6UL,
0x9285746c3f7235d9UL,
0xd52f0e859495caedUL,
0x6601423b97329582UL,
0xd041dd676d77eeaaUL,
0x636f91d96ed0b1c5UL,
0x24c5eb30c5374ef1UL,
0x97eba78ec690119eUL,
0xab911ee392f8b099UL,
0x18bf525d915feff6UL,
0x5f1528b43ab810c2UL,
0xec3b640a391f4fadUL,
0x27e05a6e926952ccUL,
0x94ce16d091ce0da3UL,
0xd3646c393a29f297UL,
0x604a2087398eadf8UL,
0x5c3099ea6de60cffUL,
0xef1ed5546e415390UL,
0xa8b4afbdc5a6aca4UL,
0x1b9ae303c601f3cbUL,
0x56ed3e2f9e224471UL,
0xe5c372919d851b1eUL,
0xa26908783662e42aUL,
0x114744c635c5bb45UL,
0x2d3dfdab61ad1a42UL,
0x9e13b115620a452dUL,
0xd9b9cbfcc9edba19UL,
0x6a978742ca4ae576UL,
0xa14cb926613cf817UL,
0x1262f598629ba778UL,
0x55c88f71c97c584cUL,
0xe6e6c3cfcadb0723UL,
0xda9c7aa29eb3a624UL,
0x69b2361c9d14f94bUL,
0x2e184cf536f3067fUL,
0x9d36004b35545910UL,
0x2b769f17cf112238UL,
0x9858d3a9ccb67d57UL,
0xdff2a94067518263UL,
0x6cdce5fe64f6dd0cUL,
0x50a65c93309e7c0bUL,
0xe388102d33392364UL,
0xa4226ac498dedc50UL,
0x170c267a9b79833fUL,
0xdcd7181e300f9e5eUL,
0x6ff954a033a8c131UL,
0x28532e49984f3e05UL,
0x9b7d62f79be8616aUL,
0xa707db9acf80c06dUL,
0x14299724cc279f02UL,
0x5383edcd67c06036UL,
0xe0ada17364673f59UL
};
void crc64_update(unsigned char * buf, int len, unsigned long *crcp)
{
int i;

    for (i = 0; i < len; i++)
        *crcp = crc64_table[(buf[i] ^ (int)*crcp) & 0xFF] ^ (*crcp >> 8);
    return;
}
unsigned long int string_hh(w,modulo)
unsigned char *w;
int modulo;
{
long int x = 0;
unsigned long int y = 0;
unsigned char *p;

    if (w == NULL || (x = strlen(w)) == 0)
        return 0;
#ifdef BAD
#ifdef DEBUG
    fprintf(stderr, "string_hh(%lx, %u)\n", (long) w, modulo);
#endif
#ifdef DEBUG_FULL
    fputs(w, stderr);
    fflush(stderr);
#endif
    for (p = w + x - 1, y=0;  p >= w;  x--, p--)
    {
        y += x;
        y *= (*p);
    }
#else
    y = 0xffffffffffffffffUL;
    crc64_update(w, strlen(w), &y);
    y = ((y & 0xffffffff00000000UL) >> 32) ^ y;
#endif
    x = y % modulo;
#ifdef DEBUG
    if (x < 0 || x > modulo)
    {
        fprintf(stderr, "string_hh(%lx, %u) gives %ld\n", (long) w, modulo, x);
        abort();
    }
#endif
    return(x);
}
unsigned long int stringi_hh(w,modulo)
unsigned char *w;
int modulo;
{
long int x = 0;
unsigned long int y = 0;
unsigned char *p, *p1;

    if (w == NULL || (x = strlen(w)) == 0)
        return 0;
#ifdef BAD
#ifdef DEBUG
    fprintf(stderr, "stringi_hh(%lx, %u)\n", (long) w, modulo);
#endif
#ifdef DEBUG_FULL
    fputs(w, stderr);
    fflush(stderr);
#endif
    for (p = w + x - 1, y=0;  p >= w;  x--, p--)
    {
        y += x;
        y *= (*p);
    }
#else
    y = 0xffffffffffffffffUL;
    p = strdup(w);
    for (p1 = p; *p1 != 0; p1++)
        if (isupper(*p1))
            *p1 = tolower(*p1);
    crc64_update(p, strlen(w), &y);
    free(p);
    y = ((y & 0xffffffff00000000UL) >> 32) ^ y;
#endif
    x = y % modulo;
#ifdef DEBUG
    if (x < 0 || x > modulo)
    {
        fprintf(stderr, "string_hh(%lx, %u) gives %ld\n", (long) w, modulo, x);
        abort();
    }
#endif
    return(x);
}
/*
 * Hash function for long keys
 */
unsigned long int long_hh(w,modulo)
char * w;
int modulo;
{
    return(((unsigned long int) (w) % modulo));
/*
 * Just use the lower bits?
unsigned short int * x = (short *) w;
 
    return(((*x & 65535)^(*(x+1) &65535)) & (modulo-1));
 */
}
/*
 * Comparison of two ints
 */
int icomp(i1,i2)
char * i1;
char * i2;
{
    if ((unsigned long) i1 == (unsigned long) i2)
        return 0;
    else if ((unsigned long) i1 < (unsigned long) i2)
        return -1;
    else return 1;
}
/*
 * Create an iterator. Return NULL if there aren't any. 
 */
HITER * hiter(hp)
HASH_CON * hp;
{
HITER * retp;
int i;
HIPT * p;

    if (hp == NULL)
        return NULL;
    for (i = 0, p = &(hp->table[0]); i < hp->tabsize; i++, p++)
    {
        if (p->in_use)
        {
            retp = (HITER *) malloc(sizeof(*retp));
            retp->hp = hp;
            retp->cp = NULL;
            retp->i = i;
            return retp;
        }
    }
    return NULL;
}
/*
 * Search the main table for the next occupied
 */
static HIPT * hiter_advance(hp)
HITER * hp;
{
HIPT * retp;
int i;

    for (i = hp->i, retp = &(hp->hp->table[i]);
            i < hp->hp->tabsize;
                i++, retp++)
    {
        if (retp->in_use)
        {
            hp->i = i;
            return retp;
        }
    }
    hp->i = -1;
    return NULL;
}
/*
 * Next Value
 */
HIPT * hiter_next(hp)
HITER * hp;
{
HIPT * retp;

    if (hp == NULL || hp->i == -1)
        return NULL;
    if (hp->cp != NULL)
        retp = hp->cp->next;
    else
        retp = hp->hp->table[hp->i].next;
    if (retp != NULL)
    {
        hp->cp = retp;
        return retp;
    }
    hp->cp = NULL;
    hp->i++;
    return hiter_advance(hp);
}
/*
 * Zap Value; clear entry and advances to the next one
 */
HIPT * hiter_zap(hp)
HITER * hp;
{
HIPT * retp;
HIPT *p;

    if (hp == NULL)
        return NULL;
/*
 * We are down a side chain
 */
    if (hp->cp != NULL)
    {
        for (p = &hp->hp->table[hp->i]; p->next != hp->cp; p = p->next);
        p->next = p->next->next;
        free(hp->cp);
        hp->cp = p->next;
        retp = hp->cp;
        if (retp != NULL)
            return retp;
    }
    else
    {
        retp = hp->hp->table[hp->i].next;
        if (retp != NULL)
        {
            hp->hp->table[hp->i] = *retp;
            free(retp);
            return &(hp->hp->table[hp->i]);
        }
        hp->hp->table[hp->i].in_use = 0;
    }
    hp->i++;
    return hiter_advance(hp);
}
/*
 * Go back to the beginning
 */
HIPT * hiter_reset(hp)
HITER * hp;
{
    if (hp == NULL)
        return NULL;
    hp->i = 0;
    hp->cp = NULL;
    return hiter_advance(hp);
}
/*
 * Get rid of the contents, but leave the table intact.
 */
void hempty(h)
HASH_CON * h;
{
HITER * hp = hiter(h);

    while(hiter_zap(hp) != NULL);
    return;
}
