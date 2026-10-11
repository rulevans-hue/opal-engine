#ifndef EVENTS_H
#define EVENTS_H

#include "time_system.h"
#include "gamestate.h"
#include "notify.h"
#include "social_stats.h"
#include "world.h"

#define MAX_EVENTS      64
#define MAX_EVENT_FLAGS  4

typedef enum {
    EVENT_TRIGGER_DAY,          // triggers on specific day
    EVENT_TRIGGER_DAY_SLOT,     // triggers on specific day + time slot
    EVENT_TRIGGER_FLAG,         // triggers when flag is set
    EVENT_TRIGGER_STAT_RANK     // triggers when social stat hits rank
} EventTriggerType;

typedef enum {
    EVENT_ACTION_NOTIFY,        // show a notification
    EVENT_ACTION_SET_FLAG,      // set a gamestate flag
    EVENT_ACTION_UNLOCK_ROOM,   // unlock a room
    EVENT_ACTION_FORCE_DIALOGUE,// force a dialogue to start
    EVENT_ACTION_WORLD_CHANGE,  // switch real/hungerscape
} EventActionType;

typedef struct {
    char            name[64];
    int             triggered;      // has this event fired?
    int             repeatable;     // can it fire more than once?

    // Trigger conditions (all must be true)
    EventTriggerType trigger_type;
    int             trigger_day;
    TimeSlot        trigger_slot;
    char            trigger_flag[64];
    int             trigger_flag_value;
    int             trigger_stat;
    int             trigger_stat_rank;

    // Required flags (must be set before this fires)
    char            req_flags[MAX_EVENT_FLAGS][64];
    int             req_flag_values[MAX_EVENT_FLAGS];
    int             req_flag_count;

    // Action
    EventActionType action_type;
    char            action_message[128];
    char            action_flag[64];
    int             action_flag_value;
    int             action_room_id;
    int             action_dialogue_node;
    WorldType       action_world;
} GameEvent;

typedef struct {
    GameEvent events[MAX_EVENTS];
    int       count;
} EventSystem;

void eventsystem_init(EventSystem* es);
void eventsystem_update(EventSystem* es, TimeSystem* ts,
                        GameState* gs, NotifySystem* ns,
                        SocialStats* ss, WorldSystem* ws,
                        int* force_dialogue_node);

#endif