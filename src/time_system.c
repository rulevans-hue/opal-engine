#include "time_system.h"
#include <stdlib.h>

void timesystem_init(TimeSystem* ts) {
    ts->day        = 1;
    ts->day_of_week = DAY_MONDAY;
    ts->slot       = TIMESLOT_MORNING;
    ts->weather    = WEATHER_CLEAR;
    ts->month      = 4;  // April — like Summon 5
    ts->date       = 9;
}

static Weather random_weather() {
    int r = rand() % 10;
    if (r < 6) return WEATHER_CLEAR;
    if (r < 9) return WEATHER_CLOUDY;
    return WEATHER_RAIN;
}

void timesystem_advance(TimeSystem* ts) {
    ts->slot++;
    if (ts->slot >= TIMESLOT_COUNT) {
        timesystem_sleep(ts);
    }
}

void timesystem_sleep(TimeSystem* ts) {
    ts->slot       = TIMESLOT_MORNING;
    ts->day++;
    ts->date++;
    ts->day_of_week = (DayOfWeek)((ts->day_of_week + 1) % 7);
    ts->weather    = random_weather();

    // Simple month rollover
    int days_in_month = 30;
    if (ts->date > days_in_month) {
        ts->date = 1;
        ts->month++;
        if (ts->month > 12) ts->month = 1;
    }
}

int timesystem_is_school_day(TimeSystem* ts) {
    return ts->day_of_week != DAY_SATURDAY &&
           ts->day_of_week != DAY_SUNDAY;
}

int timesystem_is_last_slot(TimeSystem* ts) {
    return ts->slot == TIMESLOT_NIGHT;
}

const char* timeslot_name(TimeSlot slot) {
    switch (slot) {
        case TIMESLOT_MORNING:     return "Morning";
        case TIMESLOT_LUNCH:       return "Lunch";
        case TIMESLOT_AFTERNOON:   return "Afternoon";
        case TIMESLOT_AFTERSCHOOL: return "After School";
        case TIMESLOT_EVENING:     return "Evening";
        case TIMESLOT_NIGHT:       return "Night";
        default:                   return "Unknown";
    }
}

const char* dayofweek_name(DayOfWeek day) {
    switch (day) {
        case DAY_MONDAY:    return "Monday";
        case DAY_TUESDAY:   return "Tuesday";
        case DAY_WEDNESDAY: return "Wednesday";
        case DAY_THURSDAY:  return "Thursday";
        case DAY_FRIDAY:    return "Friday";
        case DAY_SATURDAY:  return "Saturday";
        case DAY_SUNDAY:    return "Sunday";
        default:            return "Unknown";
    }
}

const char* weather_name(Weather w) {
    switch (w) {
        case WEATHER_CLEAR:  return "Clear";
        case WEATHER_CLOUDY: return "Cloudy";
        case WEATHER_RAIN:   return "Rain";
        default:             return "Unknown";
    }
}