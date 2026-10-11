#ifndef TIME_SYSTEM_H
#define TIME_SYSTEM_H

#define MAX_DAY_EVENTS 8

typedef enum {
    TIMESLOT_MORNING = 0,
    TIMESLOT_LUNCH,
    TIMESLOT_AFTERNOON,
    TIMESLOT_AFTERSCHOOL,
    TIMESLOT_EVENING,
    TIMESLOT_NIGHT,
    TIMESLOT_COUNT
} TimeSlot;

typedef enum {
    DAY_MONDAY = 0,
    DAY_TUESDAY,
    DAY_WEDNESDAY,
    DAY_THURSDAY,
    DAY_FRIDAY,
    DAY_SATURDAY,
    DAY_SUNDAY
} DayOfWeek;

typedef enum {
    WEATHER_CLEAR,
    WEATHER_CLOUDY,
    WEATHER_RAIN
} Weather;

typedef struct {
    int        day;          // absolute day number (starts at 1)
    DayOfWeek  day_of_week;
    TimeSlot   slot;
    Weather    weather;
    int        month;
    int        date;         // day of month
} TimeSystem;

void timesystem_init(TimeSystem* ts);
void timesystem_advance(TimeSystem* ts);  // advance one time slot
void timesystem_sleep(TimeSystem* ts);    // advance to next morning
int  timesystem_is_school_day(TimeSystem* ts);
int  timesystem_is_last_slot(TimeSystem* ts); // is it night?

const char* timeslot_name(TimeSlot slot);
const char* dayofweek_name(DayOfWeek day);
const char* weather_name(Weather w);

#endif