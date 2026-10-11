#include "notify.h"
#include <string.h>
#include <stdio.h>

void notify_init(NotifySystem* ns) {
    ns->count = 0;
    for (int i = 0; i < MAX_NOTIFICATIONS; i++)
        ns->notes[i].active = 0;
}

void notify_add(NotifySystem* ns, const char* text,
                unsigned char r, unsigned char g, unsigned char b) {
    // Find an empty slot or overwrite oldest
    int slot = 0;
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (!ns->notes[i].active) { slot = i; break; }
        if (ns->notes[i].timer < ns->notes[slot].timer) slot = i;
    }
    strncpy(ns->notes[slot].text, text, 127);
    ns->notes[slot].timer  = NOTIFY_DURATION;
    ns->notes[slot].active = 1;
    ns->notes[slot].r = r;
    ns->notes[slot].g = g;
    ns->notes[slot].b = b;
    if (slot >= ns->count) ns->count = slot + 1;
}

void notify_update(NotifySystem* ns) {
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (!ns->notes[i].active) continue;
        ns->notes[i].timer--;
        if (ns->notes[i].timer <= 0)
            ns->notes[i].active = 0;
    }
}

void notify_info(NotifySystem* ns, const char* text) {
    notify_add(ns, text, 180, 180, 220);
}
void notify_success(NotifySystem* ns, const char* text) {
    notify_add(ns, text, 100, 220, 100);
}
void notify_warning(NotifySystem* ns, const char* text) {
    notify_add(ns, text, 220, 180, 60);
}