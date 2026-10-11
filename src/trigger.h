#ifndef TRIGGER_H
#define TRIGGER_H

#define MAX_TRIGGERS 16

typedef enum {
    TRIGGER_ROOM_TRANSITION,
    TRIGGER_EVENT
} TriggerType;

typedef struct {
    TriggerType type;
    float x, y;
    int   width, height;

    // For room transitions
    int   target_room;
    float spawn_x, spawn_y;

    // For events
    char event_flag[64];

    int active;
} Trigger;

typedef struct {
    Trigger triggers[MAX_TRIGGERS];
    int     count;
} TriggerList;

void triggerlist_init(TriggerList* tl);
void triggerlist_add_transition(TriggerList* tl,
                                float x, float y, int w, int h,
                                int target_room,
                                float spawn_x, float spawn_y);
int  triggerlist_check(TriggerList* tl, float px, float py,
                       int pw, int ph,
                       int* out_room, float* out_x, float* out_y);

#endif