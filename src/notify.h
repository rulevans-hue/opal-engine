#ifndef NOTIFY_H
#define NOTIFY_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif

#define MAX_NOTIFICATIONS 8
#define NOTIFY_DURATION   180  // frames (~3 seconds)

typedef struct {
    char text[128];
    int  timer;
    int  active;
    // Color
    Uint8 r, g, b;
} Notification;

typedef struct {
    Notification notes[MAX_NOTIFICATIONS];
    int          count;
} NotifySystem;

void notify_init(NotifySystem* ns);
void notify_add(NotifySystem* ns, const char* text,
                Uint8 r, Uint8 g, Uint8 b);
void notify_update(NotifySystem* ns);
void notify_render(NotifySystem* ns, SDL_Renderer* renderer,
                   TTF_Font* font);

// Convenience helpers
void notify_info(NotifySystem* ns, const char* text);
void notify_success(NotifySystem* ns, const char* text);
void notify_warning(NotifySystem* ns, const char* text);

#endif