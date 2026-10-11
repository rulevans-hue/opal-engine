#ifndef NOTIFY_SDL2_H
#define NOTIFY_SDL2_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "notify.h"

void notify_render(NotifySystem* ns, SDL_Renderer* renderer, TTF_Font* font);

#endif