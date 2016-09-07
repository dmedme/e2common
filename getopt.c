/**************************************************************************
 * getopt(ARGC, ARGV, OPTSTRING) - Scan elements of the array ARGV (whose
 * length is ARGC) for option characters given in OPTSTRING.
 *
 * This routine is needed on all flavours of Windows, and whenever the GNU
 * C Compiler is used. 
 *
 * If an element of ARGV starts with '-', and is not exactly "-" or "--",
 * then it is an option element.
 *
 * The characters of this element (aside from the initial '-' and a possible
 * trailing option argument) are option characters.
 *
 * If getopt()is called repeatedly, it returns successively each of the option
 * characters from each of the option elements, together with its associated
 * argument if appropriate.
 *
 * As getopt() finds each option character, it returns that character,
 * updating 'optind' and 'optcp' so that the next call to getopt()
 * resumes the scan with the following option character or ARGV-element.
 *
 * If there are no more option characters, getopt() returns 'EOF'.
 * At this point 'optind' is the index in ARGV of the first ARGV-element
 * that is not an option. Note that the '--' value is skipped if present.
 *
 * OPTSTRING is a string containing the legitimate option characters, which
 * cannot include ':'.
 *
 * If an option character is seen that is not listed in OPTSTRING,
 * getopt() returns '\0' after printing an error message.  If 'opterr' is 
 * zero, the error message is suppressed but we still return '\0'.
 *
 * If a char in OPTSTRING is followed by a colon, that means it wants an arg,
 * so the remaining text in the same ARGV-element, or (if there is none) the
 * whole of the following ARGV-element, is returned in 'optarg'.
 *
 * Note that in the latter case, whatever is there is returned; it may be a
 * valid option, or even the 'end of processing' option. Thus, the
 * 'option requires argument' error is only ever returned at the end of the
 * argument list.
 *
 * This routine can safely be called again with different arguments after
 * optind has been reset, PROVIDED THAT IT HAS COMPLETELY PROCESSED ITS CURRENT
 * ARGV ELEMENT. optind is global, and can freely be reset by the user, but
 * optcp, the current position in the current element, is static, and no 
 * standard means is allowed for to reset it. The code below endeavours to
 * ensure that optcp is not left pointing at what might be random addresses on
 * subsequent calls, but if there was a real option that is never processed,
 * the next call to getopt() will start from this address, whatever it now
 * happens to hold.
 *
 * We have never encountered problems with the UNIX implementation of
 * getopt(), but we do see something like this with the GNU version.
 *
 * As it happens, since encountering AIX 4, all E2 Systems
 * getopt() usage provides at a minimum a zero length argv[0] value and
 * requests argument processing by setting optind = 1. However, using
 * optind == 0 to reset would be counter-intuitive, and incompatible with the
 * older getopt implementations that cheerfully processed options from
 * optind == 0. Thus, we would be forced to use our own getopt implementation
 * everywhere.
 *
 * Leave it as it is for now.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1997\n";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef VCC2003
#include <strings.h>
#endif
#ifndef __STDC__
#define const
#endif
char *optarg = (char *) NULL;          /* Returned Argument String    */
int optind = 1;                        /* Position in argument vector */
static char *optcp = (char *) NULL;    /* Position within an argument */
int opterr = 1;                        /* Allow error text output     */
int optopt = '\0';                     /* Erroneous option return     */

int getopt(argc, argv, optstring)
int argc;
#ifdef LCC
char **argv;
#else
char *const *argv;
#endif
const char *optstring;
{
char c;
char *x;

    optarg = (char *) NULL;
    if (optcp == (char *) NULL || *optcp == '\0')
    {
        optcp = (char *) NULL;   /* Make sure optcp doesn't point at random */
/*
 * The special ARGV-element `--' means 'End of options'. Otherwise, if we have
 * processed all the ARGV-elements, or reached a non-option argument, end
 * the scan.
 */
        if (optind < argc && !strcmp(argv[optind], "--"))
        {
            optind++;
            return EOF;
        }
        if ((optind >= argc)
         || (argv[optind][0] != '-' || argv[optind][1] == '\0'))
            return EOF;
        optcp = argv[optind] + 1;
    }
/*
 * Look at and handle the next option-character, which should not be a ':'.
 */
    c = *optcp++;
    x = strchr(optstring, c);
    if (x == (char *) NULL || c == ':')
    {
        if (opterr)
            fprintf (stderr, "%s: illegal option -- %c\n", argv[0], c);
        optopt = c;
        optcp = (char *) NULL;   /* Do not leave optcp pointing at random */
        optind++;
        return '\0';
    }
/*
 * Increment `optind' when we do the last character.
 */ 
    if (*optcp == '\0')
    {
        optcp = (char *) NULL;
        optind++;
    }
    if (x[1] == ':')
    {
/*
 * This is an option that requires an argument.
 */
        if (optcp != (char *) NULL && *optcp != '\0')
        {
            optarg = optcp;
            optind++;
        }
        else
        if (optind >= argc)
        {
            if (opterr)
            {
                fprintf (stderr, "%s: option requires an argument -- %c\n",
                     argv[0], c);
            }
            optopt = c;
            c = '\0';
        }
        else
            optarg = argv[optind++];   /* Pick up next ARGV element */
        optcp = (char *) NULL;
    }
    return c;
}
#ifdef STANDALONE
/*
 * Compile with -DSTANDALONE to make an executable for testing. Provide an
 * option string, and a set of arguments to test against. This doesn't
 * provide for testing of repeat call behaviour, however.
 */
int main (argc, argv)
int argc;
char **argv;
{
int c;
char *opt_string;

    if (argc > 1)
    {
        opt_string = argv[1];
        optind = 2;
        while ((c = getopt(argc, argv, opt_string)) != EOF)
            printf("Option: %c Argument: %s\n",
                   c, (optarg == (char *) NULL) ? "(None)": optarg);
    }
    while (optind < argc)
    {
        printf("Argument: %d - %s\n", optind, argv[optind]);
        optind++;
    }
    exit(0);
}
#endif
