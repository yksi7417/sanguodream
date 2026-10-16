/* Unit tests of code/sim/calendar.h: the date arithmetic of
   calendar-and-turn.md ("The turn"). */

internal b32
calendar_is(calendar_date date, u16 year, u8 month, u8 day)
{
    return date.year == year && date.month == month && calendar_day(date) == day;
}

internal void
unit_calendar_tests(test_counts *counts)
{
    calendar_date start = calendar_make(190, 1, 1);
    TEST_CHECK(counts, calendar_is(start, 190, 1, 1), "a date starts on day 1, 11 or 21");
    TEST_CHECK(counts, calendar_day(calendar_make(190, 1, 15)) == 11 && calendar_day(calendar_make(190, 1, 30)) == 21,
               "a day inside a turn gives the first day of that turn");

    calendar_date date = start;
    date = calendar_next_turn(date);
    TEST_CHECK(counts, calendar_is(date, 190, 1, 11), "one turn is 10 days");
    date = calendar_next_turn(calendar_next_turn(date));
    TEST_CHECK(counts, calendar_is(date, 190, 2, 1), "a month has 3 turns");
    TEST_CHECK(counts, calendar_days_between(start, date) == 30, "a month is 30 days");

    calendar_date year_end = calendar_make(190, 12, 21);
    TEST_CHECK(counts, calendar_is(calendar_next_turn(year_end), 191, 1, 1), "day 21 of month 12 goes to 1 January of the next year");

    calendar_date later = start;
    for (u32 turn = 0; turn < 36; ++turn) later = calendar_next_turn(later);
    TEST_CHECK(counts, calendar_is(later, 191, 1, 1) && calendar_days_between(start, later) == 360,
               "a year is 36 turns and 360 days");
    TEST_CHECK(counts, calendar_days_between(later, start) == -360, "days to an earlier date are negative");

    TEST_CHECK(counts, calendar_season_of(calendar_make(190, 1, 1)) == CALENDAR_SPRING &&
                       calendar_season_of(calendar_make(190, 3, 21)) == CALENDAR_SPRING, "months 1 to 3 are spring");
    TEST_CHECK(counts, calendar_season_of(calendar_make(190, 4, 1)) == CALENDAR_SUMMER &&
                       calendar_season_of(calendar_make(190, 6, 21)) == CALENDAR_SUMMER, "months 4 to 6 are summer");
    TEST_CHECK(counts, calendar_season_of(calendar_make(190, 7, 1)) == CALENDAR_AUTUMN &&
                       calendar_season_of(calendar_make(190, 9, 21)) == CALENDAR_AUTUMN, "months 7 to 9 are autumn");
    TEST_CHECK(counts, calendar_season_of(calendar_make(190, 10, 1)) == CALENDAR_WINTER &&
                       calendar_season_of(calendar_make(190, 12, 21)) == CALENDAR_WINTER, "months 10 to 12 are winter");

    u32 month_starts = 0, season_starts = 0, year_starts = 0;
    calendar_date walk = start;
    for (u32 turn = 0; turn < 36; ++turn)
    {
        walk = calendar_next_turn(walk);
        if (calendar_starts_month(walk))  month_starts++;
        if (calendar_starts_season(walk)) season_starts++;
        if (calendar_starts_year(walk))   year_starts++;
    }
    TEST_CHECK(counts, month_starts == 12 && season_starts == 4 && year_starts == 1,
               "a year of turns starts 12 months, 4 seasons and 1 year");
    TEST_CHECK(counts, calendar_starts_season(calendar_make(190, 4, 1)) && calendar_starts_season(calendar_make(190, 7, 1)) &&
                       calendar_starts_season(calendar_make(190, 10, 1)) && !calendar_starts_season(calendar_make(190, 2, 1)),
               "the seasons start in months 1, 4, 7 and 10 (the grain income months)");

    TEST_CHECK(counts, calendar_turns_for_days(20) == 2 && calendar_turns_for_days(30) == 3 &&
                       calendar_turns_for_days(25) == 3 && calendar_turns_for_days(0) == 0,
               "a 20-day job takes 2 turns; a part of a turn counts as a turn");
}
