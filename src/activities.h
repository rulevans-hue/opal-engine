#ifndef ACTIVITIES_H
#define ACTIVITIES_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "social_stats.h"
#include "time_system.h"
#include "party.h"
#include "notify.h"
#include "inventory.h"

#define MAX_ACTIVITIES      32
#define MAX_STAT_GAINS       3
#define MAX_REQUIREMENTS     2

typedef struct {
    char           name[64];
    char           description[128];
    char           location[64];
    int            available_slots[TIMESLOT_COUNT];
    SocialStatType raises[MAX_STAT_GAINS];
    int            amounts[MAX_STAT_GAINS];
    int            gain_count;
    SocialStatType req_stat[MAX_REQUIREMENTS];
    int            req_rank[MAX_REQUIREMENTS];
    int            req_count;
    int            restores_hp_sp;
    int            advances_day;
    int            yen_reward;     // yen earned from this activity (0 = none)
} Activity;

typedef struct {
    Activity activities[MAX_ACTIVITIES];
    int      count;
} ActivityDB;

typedef enum {
    ACTIVITY_CLOSED,
    ACTIVITY_OPEN
} ActivityMenuState;

typedef struct {
    ActivityMenuState state;
    int               selected;
} ActivityMenu;

void activitydb_init(ActivityDB* db);
void activitydb_add(ActivityDB* db, Activity act);
int  activity_is_available(Activity* act, TimeSlot slot, SocialStats* ss);

void activitymenu_init(ActivityMenu* am);
void activitymenu_open(ActivityMenu* am);
void activitymenu_close(ActivityMenu* am);
void activitymenu_handle_input(ActivityMenu* am, SDL_Event* event,
                                ActivityDB* db, SocialStats* ss,
                                TimeSystem* ts, Party* party,
                                NotifySystem* ns, Inventory* inv);
void activitymenu_render(ActivityMenu* am, SDL_Renderer* renderer,
                          TTF_Font* font, ActivityDB* db,
                          SocialStats* ss, TimeSystem* ts,
                          Inventory* inv);

#endif