/*********************************************************************
 *
 * datlib.c - a collection of data-manipulation sub-routines, in facilities
 *            similar to ORACLE's.
 *
 Copyright (c) E2 Systems. All rights reserved.
                 : This software is furnished under a licence. It may not be
                   copied or further distributed without permission in writing
                   from the copyright holder. The copyright holder makes no
                   warranty, express or implied, as to the usefulness or
                   functionality of this software. The licencee accepts
                   full responsibility for any applications built with it,
                   unless that software be covered by a separate agreement
                   between the licencee and the copyright holder.
 * 
 * datlib.c; collection of date validation and conversion routines
 * Compile with -DAT for at(1) compatible am/pm processing
 *
 * 17 January 1992 - Added Day of Week (D) support; 0 = Monday,
 *                   6, Sunday.
 */
#ifdef SOLAR
/*
 * Funny for the GNU compiler using the Solaris include files
 */
#ifdef __STDC__
#undef __STDC__
#define __STDC__ 0
#endif
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif
static char * sccs_id = "@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1993";
#include <stdio.h>
#include <sys/types.h>
#ifdef PTX
#define NOTIMEB
#endif
#ifdef NT4
#define NOTIMEB
#ifdef VCC2003
#include <WinSock2.h>
#else
#include <sys/time.h>
#endif
#endif
#ifdef V4
#define NOTIMEB
#endif
#ifdef LINUX
#include <sys/time.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif
#ifndef NOTIMEB
#include <sys/timeb.h>
#endif
#include <time.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
/*******************************************************************
 *
 * Functions called by the date routines
 *
 */
extern double atof();
extern double strtod();
extern long strtol();
extern double floor();
extern double fmod();

/*********************************************************************
 *
 * The implementation of date manipulation below
 *
 *   -  supports sub-set of ORACLE SQL functions
 *   -  does arithmetic on floating point (double) seconds difference
 *      from 1970 (sort of for compatibility with time(2)).
 *
 * The following two functions adjust gm seconds since 1970 to local seconds
 * since 1970, and vice versa.
 */
#ifdef MINGW32
#ifdef LCC 
#ifdef timezone
#undef timezone
#endif
struct timeval {
	unsigned int	tv_sec;		/* seconds */
	unsigned int	tv_usec;	/* and microseconds */
};
#endif
#ifdef VCC2003
struct timezone {
int tz_minuteswest;
};
#endif
#ifdef LCC
struct timezone {
int tz_minuteswest;
};
#endif
#endif
static double secs_from_to(t,d)
time_t t;
int d;    /* From GMT, = -1; to GMT, = +1 */
{
    double ret_secs;
    struct tm * tm;
#ifdef LINUX
struct timezone {
int tz_minuteswest;
};
static int check_flag;
static struct timezone tz;
static struct timeval tv;
#else
#ifndef NOTIMEB
    struct timeb current_time;
#else
#ifdef NT4
static int check_flag;
static struct timezone tz;
static struct timeval tv;
#endif
#endif
#endif
    tm = localtime(&t);
#ifdef LINUX
    if (!check_flag)
    {
        check_flag = 1;
        (void) gettimeofday(&tv, &tz);
    }
    ret_secs = (double) ( t + d * tz.tz_minuteswest*60);
/*
 * Whether or not it is DST is now already factored in
 *  if (tm->tm_isdst)
 *      ret_secs -= ((double) (d))*3600.0;
 */
#else
#ifdef NOTIMEB
#ifdef NT4
    if (!check_flag)
    {
        check_flag = 1;
        (void) gettimeofday(&tv, &tz);
    }
    ret_secs = (double) ( t + d * tz.tz_minuteswest*60);
    if (tm->tm_isdst)
        ret_secs -= ((double) (d))*3600.0;
#else
    if (tm->tm_isdst)
        ret_secs = (double) (t + d* altzone);
    else
        ret_secs = (double) (t + d* timezone);
#endif
#else
/**********************************************************************
 * ftime() is a Xenix-ism. Not sure if timezone and altzone will be
 * defined on Ultrix, at the time of writing.
 */
    (void) ftime(&current_time);
    ret_secs = (double) (t + d* 60*current_time.timezone);
    if (tm->tm_isdst)
        ret_secs -=  d * 3600.0;
#endif
#endif
    return ret_secs;
}
double local_secs(t)
time_t t;
{
    return secs_from_to(t,-1);
}
double gm_secs(t)
time_t t;
{
    return secs_from_to(t,1);
}
static struct {
               short int name_len;
               char * name_ptr;
              } day_names_mc[]={
6,   "Monday",
7,   "Tuesday",
9,   "Wednesday",
8,   "Thursday",
6,   "Friday",
8,   "Saturday",
6,   "Sunday"};
static struct {
               short int name_len;
               char * name_ptr;
              } day_names_uc[]={
6,   "MONDAY",
7,   "TUESDAY",
9,   "WEDNESDAY",
8,   "THURSDAY",
6,   "FRIDAY",
8,   "SATURDAY",
6,   "SUNDAY"};
static struct {
               short int name_len;
               char * name_ptr;
              } day_names_lc[]={
6,   "monday",
7,   "tuesday",
9,   "wednesday",
8,   "thursday",
6,   "friday",
8,   "saturday",
6,   "sunday"};
static struct {
               short int name_len;
               char * name_ptr;
              } month_names_uc[]={
7,   "JANUARY",
8,  "FEBRUARY",
5,   "MARCH",
5,   "APRIL",
3,   "MAY",
4,   "JUNE",
4,   "JULY",
6,   "AUGUST",
9,   "SEPTEMBER",
7,   "OCTOBER",
8,   "NOVEMBER",
8,   "DECEMBER"};
static struct {
               short int name_len;
               char * name_ptr;
              } month_names_mc[]={
7,   "January",
8,  "February",
5,   "March",
5,   "April",
3,   "May",
4,   "June",
4,   "July",
6,   "August",
9,   "September",
7,   "October",
8,   "November",
8,   "December"};
static struct {
               short int name_len;
               char * name_ptr;
              } month_names_lc[]={
7,   "january",
8,  "february",
5,   "march",
5,   "april",
3,   "may",
4,   "june",
4,   "july",
6,   "august",
9,   "september",
7,   "october",
8,   "november",
8,   "december"};
static long valid_31=0x0ad5; /* months that can have 31 days */

short int date_val(); /* validates/converts date to double */
char * to_char();     /* converts double to formatted date */
static short int dd_val();   /* validates dd */
static short int day_val();  /* validates the full day of the week */
static short int dy_val();   /* validates a truncated day of the week */
static short int mm_val();
static short int hh12_val(); /* 12 hour clock */
static short int hh24_val();
static short int mi_val();
static short int ss_val();
static short int ampm_val();
static short int month_val();
static short int mon_val();
static short int yy_val();
static short int rr_val();
short int yyyy_val();
static char * skip_white(); /* ignores white space (broadly interpreted) */ 
static char * copy_white(); /* copies white space (broadly interpreted) */

#define DAY_SEEN 1
#define MONTH_SEEN 2
#define YEAR_SEEN 4
#define LEAP_YEAR 8
#define HOURS_SEEN 16
#define MINUTES_SEEN 32
#define SECONDS_SEEN 64
#define AMPM_SEEN 128
#define WEEKDAY_SEEN 256
/********************************************************************
 *
 * ORACLE-like date validation
 *  - knows nothing about things like  'st'
 *  - less choosy (not case sensitive, ignores odd spaces and punctuation)
 *  - hidden option format string SYSDATE gives you the current system date
 * returns 
 *   0 for something invalid
 *   1 for a valid year/time
 *   -1 for a valid time only
 */
short int date_val(date_loc,form_loc,where_went,secs_since)
char * date_loc;               /* location of date to be validated */
char * form_loc;               /* location of format string */
char ** where_went;            /* place got to in the date when finished */
double * secs_since;           /* seconds difference from 01 Jan 1970 */
{
double secs;
short int days;
short int months;
short int years;
short int day_of_week;
short int hours;
short int minutes;
short int seconds;
short cur_state;
register char * x = form_loc;

    cur_state = 0;
    *secs_since=0;
    seconds = 0;
    hours = 0;
    minutes = 0;
    months = 0;
    if (!(stricmp(form_loc,"SYSDATE")))
    {
        *secs_since = local_secs(time(0));
        return 1;
    } 
#ifdef DEBUG
(void) fprintf(stderr,"date_val:  Input %30s,%s\n",
               date_loc,form_loc);
#endif
    while (*(form_loc = skip_white(form_loc)) != '\0')
    {
        date_loc = skip_white(date_loc);
        if (strnicmp("SS",form_loc,2) == 0 &&
            (cur_state & SECONDS_SEEN) == 0 &&
            (seconds=(ss_val(date_loc,&date_loc,&secs))) != -1)
            {   
               *secs_since += secs;
               cur_state |= SECONDS_SEEN;
               form_loc +=2;
               continue;
            }
        else if  (!(strnicmp("MI",form_loc,2)) &&
                !(cur_state & MINUTES_SEEN) &&
                ((minutes=(mi_val(date_loc,&date_loc,&secs)))+1))
            {   
               *secs_since += secs;
               cur_state |= MINUTES_SEEN;
               form_loc +=2;
               continue;
            }
        else if (!(strnicmp("HH24",form_loc,4)) &&
                !(cur_state & (HOURS_SEEN | AMPM_SEEN)) &&
                ((hours=(hh24_val(date_loc,&date_loc,&secs)))+1))
            {   
               *secs_since += secs;
               cur_state |= (HOURS_SEEN | AMPM_SEEN);
               form_loc +=4;
               continue;
            }
        else if ((!(strnicmp("AM",form_loc,2)) ||
                 !(strnicmp("PM",form_loc,2))) &&
                !(cur_state & AMPM_SEEN) &&
                 (ampm_val(date_loc,&date_loc,&secs)))
            {   
               *secs_since += secs;
               cur_state |= AMPM_SEEN;
               form_loc +=2;
               continue;
            }
        else if (!(strnicmp("HH12",form_loc,4)) &&
                !(cur_state & HOURS_SEEN ) &&
                ((hours=(hh12_val(date_loc,&date_loc,&secs)))+1))
            {   
               *secs_since += secs;
               cur_state |= HOURS_SEEN;
               form_loc +=4;
               continue;
            }
        else if (!(strnicmp("HH",form_loc,2)) &&
                !(cur_state & HOURS_SEEN ) &&
                ((hours=(hh12_val(date_loc,&date_loc,&secs)))+1))
            {   
               *secs_since += secs;
               cur_state |= HOURS_SEEN;
               form_loc +=2;
               continue;
            }
        else if (!(strnicmp("DD",form_loc,2)) &&
                !(cur_state & DAY_SEEN) &&
                (days=(dd_val(date_loc,&date_loc,&secs))))
            {   
               *secs_since += secs;
               cur_state |= DAY_SEEN;
               form_loc +=2;
               continue;
            }
        else if (!(strnicmp("MM",form_loc,2)) &&
                !(cur_state & MONTH_SEEN) &&
                (months=(mm_val(date_loc,&date_loc,&secs))))
            {   
               *secs_since += secs;
               cur_state |= MONTH_SEEN;
               form_loc +=2;
               continue;
            }
        else if (!(strnicmp("MONTH",form_loc,5)) &&
                !(cur_state & MONTH_SEEN) &&
                (months=(month_val(date_loc,&date_loc,&secs))))
            {   
               *secs_since += secs;
               cur_state |= MONTH_SEEN;
               form_loc +=5;
               continue;
            }
        else if (!(strnicmp("MON",form_loc,3)) &&
                !(cur_state & MONTH_SEEN) &&
                (months=(mon_val(date_loc,&date_loc,&secs))))
            {   
               *secs_since += secs;
               cur_state |= MONTH_SEEN;
               form_loc +=3;
               continue;
            }
        else if (!(strnicmp("DAY",form_loc,3)) &&
                !(cur_state & WEEKDAY_SEEN) &&
                (day_of_week=(day_val(date_loc,&date_loc))))
            {   
               cur_state |= WEEKDAY_SEEN;
               form_loc +=3;
               continue;
            }
        else if (!(strnicmp("DY",form_loc,2)) &&
                !(cur_state & WEEKDAY_SEEN) &&
                (day_of_week=(dy_val(date_loc,&date_loc))))
            {   
               cur_state |= WEEKDAY_SEEN;
               form_loc +=2;
               continue;
            }
        else if ('D' == *form_loc &&
                !(cur_state & WEEKDAY_SEEN) &&
                ((day_of_week= (int) ((int) *date_loc++ + 48)) > -1) &&
                 (day_of_week < 7))
            {   
               cur_state |= WEEKDAY_SEEN;
               form_loc +=1;
               continue;
            }
        else if (!(strnicmp("YYYY",form_loc,4)) &&
                !(cur_state & YEAR_SEEN) &&
                (years=(yyyy_val(date_loc,&date_loc,&secs))))
            {   
               *secs_since += secs;
               cur_state |= YEAR_SEEN;
               if (!(years % 400) || ((years % 100) && !(years % 4)))
                       cur_state |= LEAP_YEAR;
               form_loc +=4;
               continue;
            }
        else if (!(strnicmp("YY",form_loc,2)) &&
                !(cur_state & YEAR_SEEN) &&
                (years=(yy_val(date_loc,&date_loc,&secs))))
            {   
               *secs_since += secs;
               cur_state |= YEAR_SEEN;
               if (!(years % 400) || ((years % 100) && !(years % 4)))
                   cur_state |= LEAP_YEAR;
               form_loc +=2;
               continue;
            }
        else if (!(strnicmp("RR",form_loc,2)) &&
                !(cur_state & YEAR_SEEN) &&
                (years=(rr_val(date_loc,&date_loc,&secs))))
            {   
               *secs_since += secs;
               cur_state |= YEAR_SEEN;
               if (!(years % 400) || ((years % 100) && !(years % 4)))
                   cur_state |= LEAP_YEAR;
               form_loc +=2;
               continue;
            }
/*
 * Invalid; either unrecognised format sub-string or wrong
 */
       if (where_went != (char **) NULL)
           *where_went = date_loc;
       return 0;
    }
#ifdef DEBUG
(void) fprintf(stderr,
"date_val:  Output state %d,yyyy %d,mm %d dd %d day %d\n",
               cur_state,years,months,days,day_of_week);
#endif
    if (where_went != (char **) NULL)
        *where_went = date_loc;
    if ((cur_state & (YEAR_SEEN | MONTH_SEEN | DAY_SEEN)) != 0)
    if (   !(cur_state & YEAR_SEEN) 
        || (   (cur_state & DAY_SEEN)
            && (   !(cur_state & MONTH_SEEN) 
                || (   (months == 2) 
                    && (   (days > 29) 
                        || (   !(cur_state & LEAP_YEAR)
                            && (days == 29)
                           )
                       )
                   )
                || (   (days==31)
                    && !((1 << (months-1)) & valid_31)
                   )
               )
           )
        )
        return 0;         /* invalid */
    else
        if ((cur_state & LEAP_YEAR) && (months > 2))
            *secs_since += (double) 86400.0;
/*
 * Check up the day of the week
 */
   if (cur_state & WEEKDAY_SEEN)
       if ((cur_state & (YEAR_SEEN | MONTH_SEEN | DAY_SEEN)) == 0)
                                      /* day of the week only */
       {
           int days_since_dot;
           int cur_day;
           double week_secs;
           week_secs =  floor(local_secs(time(0)) / (double)
                                 86400.0) * (double) 86400.0;
           days_since_dot = (int) (week_secs / (double)
                                           86400.0);
           cur_day = ((days_since_dot - 4) % 7) + 1;
           if (cur_day == day_of_week)
               week_secs += (double) 7 * (double) 86400.0;
           else
           if (cur_day < day_of_week)
               week_secs += (double) (day_of_week - cur_day)
                                 * (double) 86400.0;
           else
               week_secs += (double) (7 + day_of_week - cur_day)
                                 * (double) 86400.0;
           *secs_since += week_secs;
           cur_state |= YEAR_SEEN;
       }
       else
       {
           int days_since_dot;
           int cur_day;
           days_since_dot = (int) floor(*secs_since / (double)
                                           86400.0);
           cur_day = ((days_since_dot - 4) % 7) + 1;
           if (cur_day != day_of_week) return 0;
       }
   if (cur_state & YEAR_SEEN)
       return 1; 
   else
       return -1;
}
static int year_from_secs(secs_since)
double * secs_since;
{
int neg_flag = (*secs_since < 0.0) ? 1: 0;
double ss = (neg_flag) ? -*secs_since : *secs_since;
int singles;
int fours;
int hundreds;
int four_hundreds = (int) floor(ss/(400.0*365.0 + 97.0)/86400.00);
int year;

    ss -= ((double) four_hundreds) * (400.0*365.0 + 97.0)*86400.00;
    hundreds = (int) floor(ss/(100.0*365.0 + 24.0)/86400.00);
    ss -= ((double) hundreds) * (100.0*365.0 + 24.0)*86400.00;
    fours = (int) floor(ss/(4.0*365.0 + 1.0)/86400.00);
    ss -= ((double) fours) * (4.0*365.0 + 1.0)*86400.00;
    if (((neg_flag) && (fours > 17 && hundreds != 3))
      || (!neg_flag && (fours > 7 && hundreds != 0)))
            ss += 86400.0;  /* We have treated a century as a leap year */
    singles = (int) floor(ss/365.00/86400.0);
    ss -= ((double) singles) * 365.0*86400.00;
    year = four_hundreds * 400 + hundreds * 100 + fours * 4 + singles;
    if (neg_flag)
    { /* We are at the end of the year; we are aiming for the beginning */
        if ((singles != 0) && (fours != 17 || hundreds == 3))
        { /* One of the years included is going to be a leap year */
            if (ss < 86400.0 && singles != 1)
            { /* We've over-counted the years because the leap year is longer */
                ss += 365.0 * 86400.0; /* Add back the year */
                year--;
            }
            ss -= 86400.0; /* Account for the leap year */
        }
        year++;
        ss = 365.0 * 86400.0 - ss;
        year = 1970 - year;
    }
    else
    {
        if ((singles > 2) && (fours != 7 || hundreds == 0))
        {
            if (ss < 86400.0)
            {
                ss += 365.0 * 86400.0;
                year--;
                singles--;
                if (singles > 2)
                    ss -= 86400.0;
            }
            else
                ss -= 86400.0;
        }
        year = 1970 + year;
    }
    *secs_since = ss;
    return year;
}
static double secs_from_year(year)
int year;
{
int neg_flag = (year < 1970) ? 1 : 0;
int four_hundreds;
int hundreds;
int fours;
double secs_since;

    year = year - 1970;
    if (neg_flag)
        year = -year;
    four_hundreds = year / 400;
    year = year % 400;
    hundreds = year / 100;
    year = year % 100;
    fours = year/4; 
    year = year % 4; 
    secs_since = 86400.0 *(((double) four_hundreds) * (400.0*365.0 + 97.0) 
                 +  (((double) hundreds) * (100.0*365.0 + 24.0))
                 +  (((double) fours) * (4.0*365.0 + 1.0))
                 + ((double) year) * 365.0);
    if (neg_flag)
    {
        if (fours > 17 && hundreds != 3)
            secs_since -= 86400.0;
        if ((year > 1) && (fours != 17 || hundreds == 3))
            secs_since += 86400.0;
        return -secs_since;
    }
    else
    {
        if (fours > 7 && hundreds != 0)
            secs_since -= 86400.0;
        if ((year > 2) && (fours != 7 || hundreds == 0))
            secs_since += 86400.0;
        return secs_since;
    }
}
/********************************************************************
 * ORACLE-like date to character conversion 
 *  - knows nothing about 'nd', 'st'
 *  - copies across odd spaces and punctuation in the format string
 *  - always returns a pointer to static (so must be copied on return)
 *  - gives up in the event of garbage in the format, returning as much
 *    as it managed to convert
 */
char * to_char(form_loc,secs_since)
char * form_loc;                          /* format string */
double secs_since;                        /* seconds difference from 1 Jan 70 */
{
    short int days;
    short int months;
    short int years;
    short int day_of_week;
    short int hours;
    short int minutes;
    short int seconds;
    double month_secs;
    double days_since_dot;
    static char ret_date[80];
    short cur_state;
    char * out_ptr;
    int leap_adjust;
/*
 * Begin; decompose the seconds into constituent parts
 */
    out_ptr = ret_date;
    cur_state = 0;
    days_since_dot = floor(secs_since / (double) 86400.0);
    day_of_week = (int) (days_since_dot
                       - 7.0 * floor((days_since_dot - 4.0) / 7.0));
                                     /* run from 0-6 */
    years = year_from_secs(&secs_since);
    if ((years % 400) == 0
       || ((years % 4) == 0 && (years % 100) != 0))
    for (months = 0;
           secs_since >=
           (month_secs=((double)("313232332323"[months]-20))*(double) 86400.0);
                    months++)
        secs_since -= month_secs;
    else
    for (months = 0;
           secs_since >=
           (month_secs=((double)("303232332323"[months]-20))*(double) 86400.0);
                    months++)
        secs_since -= month_secs;
    days = ((short int) floor(secs_since/((double) 86400.0))) + 1;
    secs_since -= ((double) days - (double) 1.0)*86400.0;
    hours = (short int) floor(secs_since/3600.0);
    secs_since -= ((double) hours * (double) 3600.0);
    minutes = (short int) floor(secs_since/60.0);
    secs_since -= ((double) minutes * (double) 60.0);
    seconds = secs_since;

    while (*(form_loc = copy_white(form_loc,&out_ptr)) != '\0')
    {
        if ((!(strnicmp("DD",form_loc,2))) &&
             !(cur_state & DAY_SEEN))
        {
           cur_state |= DAY_SEEN;
           sprintf(out_ptr,"%2.2d",days);
           form_loc +=2;
           out_ptr += 2;
        }
        else if ((!(strnicmp("MM",form_loc,2)) ) &&
             !(cur_state & MONTH_SEEN))
        {
           cur_state |= MONTH_SEEN;
           sprintf(out_ptr,"%2.2d",months+1);
           form_loc +=2;
           out_ptr += 2;
        }
        else if (!(strncmp("DAY",form_loc,3))
             && !(cur_state & WEEKDAY_SEEN))
        {
           cur_state |= WEEKDAY_SEEN;
           sprintf(out_ptr,"%s",day_names_uc[day_of_week].name_ptr);
           form_loc +=  3;
           out_ptr += day_names_uc[days].name_len;
        }
        else if (!(strncmp("DY",form_loc,2))
             && !(cur_state & WEEKDAY_SEEN))
        {
           cur_state |= WEEKDAY_SEEN;
           sprintf(out_ptr,"%3.3s",day_names_uc[day_of_week].name_ptr);
           form_loc +=  2;
           out_ptr += 3;
        }
        else if (!(strncmp("Day",form_loc,3))
             && !(cur_state & WEEKDAY_SEEN))
        {
           cur_state |= WEEKDAY_SEEN;
           sprintf(out_ptr,"%s",day_names_mc[day_of_week].name_ptr);
           form_loc +=  3;
           out_ptr += day_names_mc[days].name_len;
        }
        else if (!(strncmp("Dy",form_loc,2))
             && !(cur_state & WEEKDAY_SEEN))
        {
           cur_state |= WEEKDAY_SEEN;
           sprintf(out_ptr,"%3.3s",day_names_mc[day_of_week].name_ptr);
           form_loc +=  2;
           out_ptr += 3;
        }
        else if (!(strncmp("day",form_loc,3))
             && !(cur_state & WEEKDAY_SEEN))
        {
           cur_state |= WEEKDAY_SEEN;
           sprintf(out_ptr,"%s",day_names_lc[day_of_week].name_ptr);
           form_loc +=  3;
           out_ptr += day_names_mc[days].name_len;
        }
        else if (!(strncmp("dy",form_loc,2))
             && !(cur_state & WEEKDAY_SEEN))
        {
           cur_state |= WEEKDAY_SEEN;
           sprintf(out_ptr,"%3.3s",day_names_lc[day_of_week].name_ptr);
           form_loc +=  2;
           out_ptr += 3;
        }
        else if ((*form_loc == 'D' || *form_loc == 'd')
             && !(cur_state & WEEKDAY_SEEN))
        {
           cur_state |= WEEKDAY_SEEN;
           *out_ptr ++ = (char) (48 + day_of_week);
           form_loc +=  1;
           out_ptr += 1;
        }
        else if (!(strncmp("MONTH",form_loc,5))
             && !(cur_state & MONTH_SEEN))
        {
           cur_state |= MONTH_SEEN;
           sprintf(out_ptr,"%s",month_names_uc[months].name_ptr);
           form_loc +=  5;
           out_ptr += month_names_uc[months].name_len;
        }
        else if (!(strncmp("MON",form_loc,3))
             && !(cur_state & MONTH_SEEN))
        {
           cur_state |= MONTH_SEEN;
           sprintf(out_ptr,"%3.3s",month_names_uc[months].name_ptr);
           form_loc +=  3;
           out_ptr += 3;
        }
        else if (!(strncmp("Month",form_loc,5))
             && !(cur_state & MONTH_SEEN))
        {
           cur_state |= MONTH_SEEN;
           sprintf(out_ptr,"%s",month_names_mc[months].name_ptr);
           form_loc +=  5;
           out_ptr += month_names_mc[months].name_len;
        }
        else if (!(strncmp("Mon",form_loc,3))
             && !(cur_state & MONTH_SEEN))
        {
           cur_state |= MONTH_SEEN;
           sprintf(out_ptr,"%3.3s",month_names_mc[months].name_ptr);
           form_loc +=  3;
           out_ptr += 3;
        }
        else if (!(strncmp("month",form_loc,5))
             && !(cur_state & MONTH_SEEN))
        {
           cur_state |= MONTH_SEEN;
           sprintf(out_ptr,"%s",month_names_lc[months].name_ptr);
           form_loc +=  5;
           out_ptr += month_names_mc[months].name_len;
        }
        else if (!(strncmp("mon",form_loc,3))
             && !(cur_state & MONTH_SEEN))
        {
           cur_state |= MONTH_SEEN;
           sprintf(out_ptr,"%3.3s",month_names_lc[months].name_ptr);
           form_loc +=  3;
           out_ptr += 3;
        }
        else if ((!(strnicmp("YYYY",form_loc,4))) &&
             !(cur_state & YEAR_SEEN))
        {
           cur_state |= YEAR_SEEN;
           sprintf(out_ptr,"%4.4d",years);
           form_loc +=  4;
           out_ptr += 4;
        }
        else if ((!(strnicmp("YY",form_loc,2))) &&
             !(cur_state & YEAR_SEEN))
        {
           cur_state |= YEAR_SEEN;
           sprintf(out_ptr,"%2.2d",(years % 100));
           form_loc +=  2;
           out_ptr += 2;
        }
        else if ((!(strnicmp("HH24",form_loc,4))) &&
             !(cur_state & HOURS_SEEN))
        {
           cur_state |= HOURS_SEEN;
           sprintf(out_ptr,"%2.2d",hours);
           form_loc +=4;
           out_ptr += 2;
        }
        else if ((!(strnicmp("HH",form_loc,4))) &&
             !(cur_state & HOURS_SEEN))
        {
           short int temp_hours;
           cur_state |= HOURS_SEEN;
           temp_hours = (hours % 12);
           temp_hours = (temp_hours == 0) ? 12 : temp_hours;
           sprintf(out_ptr,"%2.2d",temp_hours);
           form_loc +=2;
           out_ptr += 2;
        }
        else if (((!(strncmp("AM",form_loc,2))
          || !(strncmp("PM",form_loc,2)))) &&
             !(cur_state & AMPM_SEEN))
        {
           cur_state |= AMPM_SEEN;
           if (hours > 11)
               sprintf(out_ptr,"P.M.");
           else
               sprintf(out_ptr,"A.M.");
           form_loc +=2;
           out_ptr += 4;
        }
        else if ((!(strncmp("am",form_loc,2))
          || !(strncmp("pm",form_loc,2))) &&
             !(cur_state & AMPM_SEEN))
        {
           cur_state |= AMPM_SEEN;
           if (hours > 11)
               sprintf(out_ptr,"p.m.");
           else
               sprintf(out_ptr,"a.m.");
           form_loc +=2;
           out_ptr += 4;
        }
        else if ((!(strnicmp("MI",form_loc,2)) ) &&
             !(cur_state & MINUTES_SEEN))
        {
           cur_state |= MINUTES_SEEN;
           sprintf(out_ptr,"%2.2d",minutes);
           form_loc +=2;
           out_ptr += 2;
        }
        else if ((!(strnicmp("SS",form_loc,2))) &&
             !(cur_state & SECONDS_SEEN))
        {
           cur_state |= SECONDS_SEEN;
           sprintf(out_ptr,"%2.2d",seconds);
           form_loc +=2;
           out_ptr += 2;
        }
        else break;
    }
    *out_ptr = '\0';
    return ret_date; 
}
/**********************************************************************
 *
 * REALLY private routines.....
 *
 */
static char * skip_white(x)
char * x;
{
   register char * rx = x;
    while (!(isalnum(*rx)) && *rx !='\0') rx++;
    return rx;
}
static char * copy_white(x,x1)
char * x;
char ** x1;
{
   register char * rx = x;
   register char * rx1 = *x1;
    while (!isalnum(*rx) && *rx !='\0') *rx1++ = *rx++;
    *x1 = rx1;
    return rx;
}
static short int ss_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int seconds;
   register char * x = date_loc;

   if (*x == '0') x++;
   if (*x < '0' || *x > '9')
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return -1;
   }
   seconds =  *x - 48;
   if (x == date_loc)
       if (*x < '6')
       {
           x++;
           if (*x >= '0' && *x <= '9')
               seconds = seconds * 10 + *x - 48;
           else x--;
       }
   *secs = (double) seconds;
   *date_ret = ++x;
   return seconds;
}
static short int mi_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int minutes;
   register char * x = date_loc;

   if (*x == '0') x++;
   if (*x < '0' || *x > '9')
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return -1;
   }
   minutes =  *x - 48;
   if (x == date_loc)
       if (*x < '6')
       {
           x++;
           if (*x >= '0' && *x <= '9')
               minutes = minutes * 10 + *x - 48;
           else x--;
       }
   *secs = ((double) minutes) * 60.0;
   *date_ret = ++x;
   return minutes;
}
static short int hh12_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int hours;
   register char * x = date_loc;

   if (*x == '0') x++;
   if (*x < '0' || *x > '9')
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return 0;
   }
   hours =  *x - 48;
   if (*x == '1' && x == date_loc)
   {
       x++;
       if (*x == '0')
           hours = 10;
       else if (*x == '1')
           hours = 11;
       else if (*x == '2')
           hours = 12;
       else x--;
   }
   *secs = ((double) hours) * 3600.0;
   *date_ret = ++x;
   return hours;
}
static short int ampm_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   register char * x = date_loc;

   if (*x == 'A' || *x == 'a')
       *secs = (double) 0.0;
   else if ( *x == 'P' || *x == 'p')
       *secs = 43200.0;
   else
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return 0;
   }
   ++x;
   x = skip_white(x);

   if ( *x != 'M' && *x != 'm')
   {
       *date_ret = x;
#ifdef AT
       return 1;
#else
       *secs = (double) 0.0;
       return 0;
#endif
   }
   *date_ret = ++x;
   return 1;
}
static short int hh24_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int hours;
   register char * x = date_loc;

   if (*x == '0') x++;
   if (*x < '0' || *x > '9')
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return -1;
   }
   hours =  *x - 48;
   if (x == date_loc)
       if (*x == '1')
       {
           x++;
           if (*x >= '0' && *x <= '9')
               hours = hours * 10 + *x - 48;
           else x--;
       }
       else if (*x == '2')
       {
           x++;
           if (*x >= '0' && *x <= '3')
               hours = hours * 10 + *x - 48;
           else x--;
       }
   *secs = ((double) (hours)) * 3600.0;
   *date_ret = ++x;
   return hours;
}
static short int dd_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int days;
   register char * x = date_loc;

   if (*x == '0') x++;
   if (*x < '0' || *x > '9')
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return 0;
   }
   days =  *x - 48;
   if (x == date_loc)
       if (*x < '3')
       {
           x++;
           if (*x >= '0' && *x <= '9')
               days = days * 10 + *x - 48;
           else x--;
       }
       else if (*x == '3')
       {
           x++;
           if (*x == '0')
              days = 30;
           else if(*x == '1')
              days = 31;
           else x--;
       }
   *secs = ((double) (days-1)) * 86400.0;
   *date_ret = ++x;
   return days;
}
static short int mm_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int months;
   register char * x = date_loc;

   if (*x == '0') x++;
   if (*x < '0' || *x > '9')
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return 0;
   }
   months =  *x - 48;
   if (*x == '1' && x == date_loc)
   {
       x++;
       if (*x == '0')
           months = 10;
       else if (*x == '1')
           months = 11;
       else if (*x == '2')
           months = 12;
       else x--;
   }
   *secs = ((double) ((months-1) * 31 - ("003344555667"[(months -1)]-48)))
                * 86400.0;
   *date_ret = ++x;
   return months;
}
static short int month_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int months;

   for (months=0; months < 12; months++)
       if (!(strncmp(month_names_uc[months].name_ptr,
                     date_loc,
                     month_names_uc[months].name_len)) ||
           !(strncmp(month_names_mc[months].name_ptr,
                     date_loc,
                     month_names_mc[months].name_len)) ||
           !(strncmp(month_names_lc[months].name_ptr,
                     date_loc,
                     month_names_lc[months].name_len))) break;
   if (months == 12)
   {
       *secs = (double) 0.0;
       *date_ret = date_loc;
       return 0;
   }
   *secs = ((double) ((months) * 31 - ("003344555667"[months]-48)))
                * 86400.0;
   *date_ret = date_loc + month_names_uc[months].name_len;
   return ++months;
}
static short int mon_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int months;

   for (months=0; months < 12; months++)
       if (!(strncmp(month_names_uc[months].name_ptr, date_loc, 3)) ||
           !(strncmp(month_names_mc[months].name_ptr, date_loc, 3)) ||
           !(strncmp(month_names_lc[months].name_ptr, date_loc, 3))) break;
   if (months == 12)
   {
       *secs = (double) 0.0;
       *date_ret = date_loc;
       return 0;
   }
   *secs = ((double) ((months) * 31 - ("003344555667"[months]-48)))
                * 86400.0;
   *date_ret = date_loc + 3;
   return ++months;
}
static short int day_val(date_loc,date_ret)
char * date_loc;
char ** date_ret;
{
   short int day_of_week;

   for (day_of_week=0; day_of_week < 7; day_of_week++)
       if (!(strncmp(day_names_uc[day_of_week].name_ptr,
                     date_loc,
                     day_names_uc[day_of_week].name_len)) ||
           !(strncmp(month_names_mc[day_of_week].name_ptr,
                     date_loc,
                     day_names_mc[day_of_week].name_len)) ||
           !(strncmp(month_names_lc[day_of_week].name_ptr,
                     date_loc,
                     day_names_lc[day_of_week].name_len))) break;
   if (day_of_week == 7)
   {
       *date_ret = date_loc;
       return 0;
   }
   *date_ret = date_loc + month_names_uc[day_of_week].name_len;
   return ++day_of_week;
}
static short int dy_val(date_loc,date_ret)
char * date_loc;
char ** date_ret;
{
   short int day_of_week;

   for (day_of_week=0; day_of_week < 7; day_of_week++)
       if (!(strncmp(day_names_uc[day_of_week].name_ptr, date_loc, 3)) ||
           !(strncmp(day_names_mc[day_of_week].name_ptr, date_loc, 3)) ||
           !(strncmp(day_names_lc[day_of_week].name_ptr, date_loc, 3))) break;
   if (day_of_week == 7)
   {
       *date_ret = date_loc;
       return 0;
   }
   *date_ret = date_loc + 3;
   return ++day_of_week;
}
static short int yy_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int years;
   register char * x = date_loc;

   if (*x == '0') x++;
   if (*x < '0' || *x > '9')
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return 0;
   }
   years =  *x - 48;
   if ( x == date_loc)
   {
       x++;
       if (*x >= '0' || *x <= '9')
           years = 10*years + *x -48;
       else x--;
   }
   years += 2000;
   *secs = secs_from_year(years);
   *date_ret = ++x;
   return years;
}
static short int rr_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int years;
   register char * x = date_loc;

   if (*x == '0') x++;
   if (*x < '0' || *x > '9')
   {
       *secs = (double) 0.0;
       *date_ret = x;
       return 0;
   }
   years =  *x - 48;
   if ( x == date_loc)
   {
       x++;
       if (*x >= '0' || *x <= '9')
           years = 10*years + *x -48;
       else x--;
   }
   if (years > 50)
       years += 1900;
   else
       years += 2000;
   *secs = secs_from_year(years);
   *date_ret = x;
   return years;
}
short int yyyy_val(date_loc,date_ret,secs)
char * date_loc;
char ** date_ret;
double * secs;
{
   short int years;
   char * x;

   years = (short int) strtol(date_loc,&x,10);
   if (x == date_loc)
   {
       *secs = (double) 0.0;
       *date_ret = date_loc;
       return 0;
   }
   *secs = secs_from_year(years);
   return years;
}
