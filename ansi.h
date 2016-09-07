/************************************************************
 * ansi.h - macros to get ansi-standard source through various
 * non-ansi compilers
 * @(#) $Name$ $Id$
 */
#ifndef ANSI_H
#define ANSI_H
#ifndef ANSIARGS
#ifdef __STDC__
#define ANSIARGS(x)	x
#define VOID		void
#endif /* __STDC__ */

#ifdef __ZTC__
#define ANSIARGS(x)	x
#define VOID		void
#endif /* __ZTC__ */

#ifdef __MSC__
#define ANSIARGS(x)	x
#define VOID		void
#endif /* __MSC__ */

#ifndef ANSIARGS
#define ANSIARGS(x)	()
#define VOID		char
#endif /* !ANSIARGS */
#endif /* !ANSIARGS */
#endif /* !ANSI_H */
