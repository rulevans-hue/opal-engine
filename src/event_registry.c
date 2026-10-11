#include "event_registry.h"
#include <string.h>
#include <stdio.h>

static GameEvent* add_event(EventSystem* es, const char* name,
                              int repeatable) {
    if (es->count >= MAX_EVENTS) {
        printf("event_registry: too many events\n");
        return NULL;
    }
    GameEvent* e = &es->events[es->count++];
    memset(e, 0, sizeof(GameEvent));
    strncpy(e->name, name, 63);
    e->triggered  = 0;
    e->repeatable = repeatable;
    return e;
}

GameEvent* er_on_day_slot(EventSystem* es, const char* name,
                           int day, TimeSlot slot) {
    GameEvent* e = add_event(es, name, 0);
    if (!e) return NULL;
    e->trigger_type = EVENT_TRIGGER_DAY_SLOT;
    e->trigger_day  = day;
    e->trigger_slot = slot;
    return e;
}

GameEvent* er_on_flag(EventSystem* es, const char* name,
                       const char* flag, int value) {
    GameEvent* e = add_event(es, name, 0);
    if (!e) return NULL;
    e->trigger_type       = EVENT_TRIGGER_FLAG;
    e->trigger_flag_value = value;
    strncpy(e->trigger_flag, flag, 63);
    return e;
}

GameEvent* er_on_stat_rank(EventSystem* es, const char* name,
                            SocialStatType stat, int rank) {
    GameEvent* e = add_event(es, name, 0);
    if (!e) return NULL;
    e->trigger_type      = EVENT_TRIGGER_STAT_RANK;
    e->trigger_stat      = stat;
    e->trigger_stat_rank = rank;
    return e;
}

GameEvent* er_on_day(EventSystem* es, const char* name, int day) {
    GameEvent* e = add_event(es, name, 0);
    if (!e) return NULL;
    e->trigger_type = EVENT_TRIGGER_DAY;
    e->trigger_day  = day;
    return e;
}

GameEvent* er_require_flag(GameEvent* e, const char* flag, int value) {
    if (!e || e->req_flag_count >= MAX_EVENT_FLAGS) return e;
    strncpy(e->req_flags[e->req_flag_count], flag, 63);
    e->req_flag_values[e->req_flag_count] = value;
    e->req_flag_count++;
    return e;
}

GameEvent* er_notify(GameEvent* e, const char* message) {
    if (!e) return NULL;
    e->action_type = EVENT_ACTION_NOTIFY;
    strncpy(e->action_message, message, 127);
    return e;
}

GameEvent* er_set_flag(GameEvent* e, const char* flag, int value,
                        const char* message) {
    if (!e) return NULL;
    e->action_type       = EVENT_ACTION_SET_FLAG;
    e->action_flag_value = value;
    strncpy(e->action_flag,    flag,    63);
    strncpy(e->action_message, message, 127);
    return e;
}

GameEvent* er_dialogue(GameEvent* e, int node) {
    if (!e) return NULL;
    e->action_type          = EVENT_ACTION_FORCE_DIALOGUE;
    e->action_dialogue_node = node;
    return e;
}

GameEvent* er_repeatable(GameEvent* e) {
    if (!e) return NULL;
    e->repeatable = 1;
    return e;
}