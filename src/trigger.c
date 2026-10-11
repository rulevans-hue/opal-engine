#include "trigger.h"
#include <string.h>

void triggerlist_init(TriggerList* tl) {
    tl->count = 0;
}

void triggerlist_add_transition(TriggerList* tl,
                                float x, float y, int w, int h,
                                int target_room,
                                float spawn_x, float spawn_y) {
    if (tl->count >= MAX_TRIGGERS) return;
    Trigger* t    = &tl->triggers[tl->count++];
    t->type        = TRIGGER_ROOM_TRANSITION;
    t->x           = x;
    t->y           = y;
    t->width       = w;
    t->height      = h;
    t->target_room = target_room;
    t->spawn_x     = spawn_x;
    t->spawn_y     = spawn_y;
    t->active      = 1;
}

int triggerlist_check(TriggerList* tl, float px, float py,
                      int pw, int ph,
                      int* out_room, float* out_x, float* out_y) {
    for (int i = 0; i < tl->count; i++) {
        Trigger* t = &tl->triggers[i];
        if (!t->active) continue;

        // Simple AABB overlap
        if (px < t->x + t->width  &&
            px + pw > t->x        &&
            py < t->y + t->height &&
            py + ph > t->y) {

            if (t->type == TRIGGER_ROOM_TRANSITION) {
                *out_room = t->target_room;
                *out_x    = t->spawn_x;
                *out_y    = t->spawn_y;
                return 1;
            }
        }
    }
    return 0;
}