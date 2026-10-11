#ifndef STATUS_SCREEN_H
#define STATUS_SCREEN_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "party.h"
#include "stats.h"
#include "skills.h"
#include "equipment.h"

typedef enum {
    STATUS_CLOSED,
    STATUS_MAIN,        // party overview
    STATUS_DETAIL,      // single character full stats + affinities
    STATUS_SKILLS,      // character skill list
    STATUS_EQUIP        // equipment management
} StatusScreenState;

typedef struct {
    StatusScreenState state;
    int               selected_member;
    int               selected_skill;
    int               selected_slot;    // which equip slot is highlighted
    int               browsing_equip;   // 1 = browsing items to equip
    int               equip_scroll;     // scroll offset in item list
    int               selected_item;    // which item is highlighted in browser
    int               scroll;
} StatusScreen;

void statusscreen_init(StatusScreen* ss);
void statusscreen_open(StatusScreen* ss);
void statusscreen_close(StatusScreen* ss);

void statusscreen_handle_input(StatusScreen* ss, SDL_Event* event,
                                Party* party, EquipDB* equipdb);

void statusscreen_render(StatusScreen* ss, SDL_Renderer* renderer,
                          TTF_Font* font, Party* party,
                          SkillDB* skilldb, EquipDB* equipdb);

#endif