#ifndef CALENDAR_H
#define CALENDAR_H
/* The calendar of the game (calendar-and-turn.md, "The turn"): one turn is
   10 days, a month has 3 turns, a year has 12 months and four seasons.
   Spring is months 1 to 3, summer 4 to 6, autumn 7 to 9 and winter 10 to
   12. [documented, manual p.7, p.9]

   A date is a year, a month and the turn of the month (0, 1 or 2). The
   turns of a month start on day 1, 11 and 21. Pure functions: no state,
   no events. */

#include "base.h"

#define CALENDAR_DAYS_PER_TURN     10
#define CALENDAR_TURNS_PER_MONTH   3
#define CALENDAR_MONTHS_PER_YEAR   12
#define CALENDAR_MONTHS_PER_SEASON 3

enum calendar_season
{
    CALENDAR_SPRING,
    CALENDAR_SUMMER,
    CALENDAR_AUTUMN,
    CALENDAR_WINTER,
    CALENDAR_SEASON_COUNT
};

struct calendar_date
{
    u16 year;
    u8  month;          /* 1..12 */
    u8  turn_of_month;  /* 0..2: day 1, 11 or 21 */
};

/* A date from a year, a month and a day of the month. The day picks the
   turn that contains it: days 1-10 are turn 0, 11-20 turn 1, 21-30 turn 2. */
internal calendar_date
calendar_make(u16 year, u8 month, u8 day)
{
    calendar_date date;
    date.year  = year;
    date.month = (u8)(month < 1 ? 1 : (month > CALENDAR_MONTHS_PER_YEAR ? CALENDAR_MONTHS_PER_YEAR : month));
    u32 turn   = day > 0 ? (u32)(day - 1) / CALENDAR_DAYS_PER_TURN : 0;
    date.turn_of_month = (u8)(turn < CALENDAR_TURNS_PER_MONTH ? turn : CALENDAR_TURNS_PER_MONTH - 1);
    return date;
}

/* The first day of the turn: 1, 11 or 21. */
internal u8
calendar_day(calendar_date date)
{
    return (u8)(1 + date.turn_of_month * CALENDAR_DAYS_PER_TURN);
}

internal calendar_season
calendar_season_of(calendar_date date)
{
    return (calendar_season)((date.month - 1) / CALENDAR_MONTHS_PER_SEASON);
}

/* The date one turn later. Day 21 of month 12 goes to day 1 of month 1 of
   the next year. */
internal calendar_date
calendar_next_turn(calendar_date date)
{
    calendar_date next = date;
    next.turn_of_month++;
    if (next.turn_of_month == CALENDAR_TURNS_PER_MONTH)
    {
        next.turn_of_month = 0;
        next.month++;
        if (next.month > CALENDAR_MONTHS_PER_YEAR)
        {
            next.month = 1;
            next.year++;
        }
    }
    return next;
}

/* The number of turns from year 0, month 1, day 1 to the date. */
internal s64
calendar_turn_number(calendar_date date)
{
    return ((s64)date.year * CALENDAR_MONTHS_PER_YEAR + (date.month - 1)) * CALENDAR_TURNS_PER_MONTH +
           date.turn_of_month;
}

/* The days from one date to a later date (negative if "to" is earlier). */
internal s64
calendar_days_between(calendar_date from, calendar_date to)
{
    return (calendar_turn_number(to) - calendar_turn_number(from)) * CALENDAR_DAYS_PER_TURN;
}

internal b32 calendar_starts_month(calendar_date date)  { return date.turn_of_month == 0; }
internal b32 calendar_starts_season(calendar_date date) { return date.turn_of_month == 0 && (date.month - 1) % CALENDAR_MONTHS_PER_SEASON == 0; }
internal b32 calendar_starts_year(calendar_date date)   { return date.turn_of_month == 0 && date.month == 1; }

/* The turns that a job of some days takes: a 20-day job takes 2 turns
   (cities-and-facilities.md, Edge cases). A part of a turn counts as a
   whole turn. */
internal u32
calendar_turns_for_days(u32 days)
{
    return (days + CALENDAR_DAYS_PER_TURN - 1) / CALENDAR_DAYS_PER_TURN;
}

internal const char *
calendar_season_name(calendar_season season)
{
    switch (season)
    {
        case CALENDAR_SPRING: return "spring";
        case CALENDAR_SUMMER: return "summer";
        case CALENDAR_AUTUMN: return "autumn";
        case CALENDAR_WINTER: return "winter";
        default:              return "?";
    }
}

#endif /* CALENDAR_H */
