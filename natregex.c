/* natregex.c - regular expression matching code.
*/
static    char* sccs_id= "@(#) $Name$ $Id$";

/*
 * routines to do regular expression matching
 *
 * Entry points:
 *
 *    re_comp(s,c,reg_max)
 *        char *s;
 *        reg_comp c;
 *        long reg_max;
 *     ... returns the length of the expression if valid.
 *         reg_max + 1 if buffer too small;
 *         reg_max + 1 + pointer to error message otherwise.
 *
 *         If passed 0 or a null string returns without changing
 *           the buffer compiled re (see note 11 below).
 *         reg_max is the maximum size of compiled expression allowed;
 *         this starts as ESTARTSIZE.
 *
 *    Routines that start with ESTARTSIZE should double it if the
 *    (size too big) error is returned.
 *
 *    re_exec(s,c)
 *        char *s;
 *        reg_comp c;
 *     ... returns 1 if the string s matches the compiled regular
 *               expression c, 
 *             0 if the string s failed to match the compiled
 *               regular expression, c, and
 *            -1 if the compiled regular expression was invalid 
 *               (indicating an internal error).
 *
 * The strings passed to both re_comp and re_exec may have trailing or
 * embedded newline characters; they are terminated by nulls.
 *
 * The identity of the author of these routines is lost in antiquity;
 * this is essentially the same as the re code in the original V6 ed.
 *
 * The main change to it has been the replacement of the the static
 * regular expressions with dynamically added ones. This enables the
 * expressions to be managed by the calling routines (that wish to
 * keep lots of expressions together).
 *
 * The regular expressions recognized are described below. This description
 * is essentially the same as that for ed.
 *
 *    A regular expression specifies a set of strings of characters.
 *    A member of this set of strings is said to be matched by
 *    the regular expression.  In the following specification for
 *    regular expressions the word `character' means any character but NUL.
 *
 *    1.  Any character except a special character matches itself.
 *        Special characters are the regular expression delimiter plus
 *        \ [ . and sometimes ^ * $.
 *    2.  A . matches any character.
 *    3.  A \ followed by any character except a digit or ( )
 *        matches that character.
 *    4.  A nonempty string s bracketed [s] (or [^s]) matches any
 *        character in (or not in) s. In s, \ has no special meaning,
 *        and ] may only appear as the first letter. A substring 
 *        a-b, with a and b in ascending ASCII order, stands for
 *        the inclusive range of ASCII characters.
 *    5.  A regular expression of form 1-4 followed by * matches a
 *        sequence of 0 or more matches of the regular expression.
 *    6.  A regular expression, x, of form 1-8, bracketed \(x\)
 *        matches what x matches.
 *    8.  A regular expression of form 1-7, x, followed by a regular
 *        expression of form 1-7, y matches a match for x followed by
 *        a match for y, with the x match being as long as possible
 *        while still permitting a y match.
 *    9.  A regular expression of form 1-8 picks out the longest among
 *        the leftmost matches in a line.
 */
#include "natregex.h"

/*
 * constants for re's
 */
#define    CCHR    2
#define    CDOT    4
#define    CCL    6
#define    NCCL    8
#define    CEOF    11

#define    CSTAR    01

static int cclass(set, c, af)
register char  *set, c;
int af;
{
register int n;

    if (c == 0)
        return(0);
    n = *set++;
    while (--n)
        if (*set++ == c)
            return(af);
    return(! af);
}

/*
 * compile the regular expression argument into a dfa
 */
char *  re_comp(sp,expbuf,reg_max)
register char  *sp;
reg_comp expbuf;
long int reg_max;
{
register int c;
register char  *ep = expbuf;
char  * endptr = expbuf + reg_max;
int cclcnt;
char  *lastep = 0;

#define    comerr(msg) {expbuf = 0; return(msg + 1 + reg_max); }

    if (sp == 0 || *sp == '\0')
        return(0);
    for (;;)
    {
        if (ep >= endptr)
            return((char  *)(reg_max+1));
        if ((c = *sp++) == '\0')
        {
            *ep++ = CEOF;
            *ep++ = 0;
            return((char *)(ep - expbuf));
        }
        if (c != '*')
            lastep = ep;
        switch (c)
        {
        case '.':
            *ep++ = CDOT;
            continue;
        case '*':
            if (lastep == 0)
                goto defchar;
            *lastep |= CSTAR;
            continue;
        case '[':
            *ep++ = CCL;
            *ep++ = 0;
            cclcnt = 1;
            if ((c = *sp++) == '^')
            {
                c = *sp++;
                ep[-2] = NCCL;
            }
            do
            {
                if (c == '\0')
                    comerr("missing ]");
                if (c == '-' && ep [-1] != 0)
                {
                    if ((c = *sp++) == ']')
                    {
                        *ep++ = '-';
                        cclcnt++;
                        break;
                    }
                    while (ep[-1] < c)
                    {
                        *ep = ep[-1] + 1;
                        ep++;
                        cclcnt++;
                        if (ep >= endptr)
                            return(char *)(reg_max+1);
                    }
                }
                *ep++ = c;
                cclcnt++;
                if (ep >= endptr)
                    return(char *)(reg_max+1);
            }
            while ((c = *sp++) != ']');
            lastep[1] = cclcnt;
            continue;
        case '\\':
            if (!(c = *sp++))
            {
                *ep++ = CEOF;
                *ep++ = 0;
                return((char *)(ep - expbuf));
            }
            *ep++ = CCHR;
            *ep++ = c;
            continue;
        defchar:
        default:
            *ep++ = CCHR;
            *ep++ = c;
        }
    }
}

/* 
 * Match the argument string against the compiled re
 * An empty string never matches anything.
 */
int re_exec(p,expbuf)
char  *p;
reg_comp expbuf;
{
register char  *ep;
register char  *curlp;
register char  *lp;
int rv;
    while (*p != '\0')
    {
       lp = p;
       ep = expbuf;

        for (;;)
            switch (*ep++)
            {
            case CCHR:
                if (*ep++ == *lp++)
                    continue;
                goto another;
            case CDOT:
                if (*lp++)
                    continue;
                goto another;
            case CEOF:
                return(1);
            case CCL:
                if (cclass(ep, *lp++, 1))
                {
                    ep += *ep;
                    continue;
                }
                goto another;
            case NCCL:
                if (cclass(ep, *lp++, 0))
                {
                    ep += *ep;
                    continue;
                }
                goto another;
            case CDOT|CSTAR:
                curlp = lp;
                while (*lp++) ;
                goto star;
            case CCHR|CSTAR:
                curlp = lp;
                while (*lp++ == *ep);
                ep++;
                goto star;
            case CCL|CSTAR:
            case NCCL|CSTAR:
                curlp = lp;
                while (cclass(ep, *lp++, ep[-1] == (CCL|CSTAR)));
                ep += *ep;
                goto star;
            star:
                do
                {
                    lp--;
                    if (rv = re_exec(lp, ep))
                        return(rv);
                }
                while (lp > curlp);
                goto another;
            default:
/*
 * Error in compiled regular expression
 */
                return(-1);
            }
another:
        p++;
    }
    return 0;
}
/*
 * Version for the information retrieval matching
 *
 *  This code is derived from the BSD 4.2 regexp(3) sources. The
 *  differences are:
 *
 *  1.  The compiled regular expressions are no longer in
 *      static; this enables a  number to exist together.
 *
 *  2.  The original versions match whenever the source exists
 *      in the target; this version only matches when the target
 *      is matched in its entirety by the sources.
 *
 *  3.  Certain options (match at beginning of line, and at end
 *      of line, and the parenthesis feature) are not meaningful
 *      in the context of the IR system: these features have been
 *      removed.
 *
 *  4.  Because of the requirement for a complete (anchored) match,
 *      a level of function call indirection (exec and advance) has been
 *      eliminated. The IR_exec function is derived mainly from
 *      the regexp advance function.
 */
int
IR_exec(lp,expbuf)
register char    *lp;
reg_comp expbuf;
{
register char    *ep = expbuf;
register char    *curlp;
int    rv;

    for (;;)
        switch (*ep++)
        {
        case CCHR:
            if (*ep++ == *lp++)
                continue;
            return(0);

        case CDOT:
            if (*lp++)
                continue;
            return(0);

        case CEOF:
            if (*lp == '\0')
                return(1);

            return (0);

        case CCL:
            if (cclass(ep, *lp++, 1))
            {
                ep += *ep;
                continue;
            }
            return(0);

        case NCCL:
            if (cclass(ep, *lp++, 0))
            {
                ep += *ep;
                continue;
            }
            return(0);


        case CDOT|CSTAR:
            curlp = lp;
            while (*lp++);
            goto star;

        case CCHR|CSTAR:
            curlp = lp;
            while (*lp++ == *ep);
            ep++;
            goto star;

        case CCL|CSTAR:
        case NCCL|CSTAR:
            curlp = lp;
            while (cclass(ep, *lp++, ep[-1] == (CCL|CSTAR)));
            ep += *ep;
            goto star;

        star:
            do
            {
                lp--;
                if (rv = IR_exec(lp, ep))
                    return(rv);
            }
            while (lp > curlp);
            return(0);

        default:
            return(-1);
        }
}
/* 
 * Match the argument bytes against the compiled re
 *
 * This is different to the other, because it terminates on a length, rather
 * than a NUL.
 *
 * Also, the first parameter has a pointer to the last match on return.
 */
int
re_scan(rp,expbuf, len)
    char    **rp;
    reg_comp expbuf;
    int len;
{
char *p = *rp;
register char    *ep;
char    *curlp;
register char    *lp;
int    rv;
register char * top = p + len;

    while (p < top)
    {
       lp = p;
       ep = expbuf;

        while (lp < top)
            switch (*ep++)
            {

            case CCHR:
                if (*ep++ == *lp++)
                    continue;
                goto another;

            case CDOT:
                lp++;
                continue;

            case CEOF:
                *rp = p;
                return(1);

            case CCL:
                if (cclass(ep, *lp++, 1))
                {
                    ep += *ep;
                    continue;
                }
                goto another;

            case NCCL:
                if (cclass(ep, *lp++, 0))
                {
                    ep += *ep;
                    continue;
                }
                goto another;

            case CDOT|CSTAR:
                curlp = lp;
                lp = top;
                goto star;

            case CCHR|CSTAR:
                curlp = lp;
                while (lp < top && *lp++ == *ep);
                ep++;
                goto star;

            case CCL|CSTAR:
            case NCCL|CSTAR:
                curlp = lp;
                while (lp < top && cclass(ep, *lp++, ep[-1] == (CCL|CSTAR)));
                ep += *ep;
                goto star;

            star:
                do
                {
                char *nlp;
                    lp--;
                    nlp = lp;
                    if (rv = re_scan(&nlp, ep, len - (lp - p)))
                        return(rv);
                }
                while (lp > curlp);
                goto another;

            default:
/*
 * Error in compiled regular expression
 */
                return(-1);
            }
another:
        p++;
    }
    return 0;
}
