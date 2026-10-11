#ifndef HUD_H
#define HUD_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "time_system.h"
#include "world.h"
#include "social_stats.h"
#include "dungeon.h"

// When dungeon->active is 1, the dungeon HUD in dungeon_render() handles
// the top-right panel. This function only renders the overworld HUD.
void hud_render(SDL_Renderer* renderer, TTF_Font* font,
                TimeSystem* ts, WorldSystem* ws, SocialStats* ss,
                DungeonSystem* dungeon_sys);

#endif