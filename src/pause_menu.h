#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "save.h"
#include "party.h"
#include "inventory.h"
#include "gamestate.h"
#include "time_system.h"
#include "social_stats.h"
#include "bond.h"

typedef enum {
    PAUSE_CLOSED,
    PAUSE_MAIN,
    PAUSE_SAVE,
    PAUSE_LOAD
} PauseMenuState;

typedef struct {
    PauseMenuState state;
    int            selected;
    SaveData       slots[SAVE_SLOTS];
    int            slots_loaded;
} PauseMenu;

void pause_menu_init(PauseMenu* pm);
void pause_menu_open(PauseMenu* pm);
void pause_menu_close(PauseMenu* pm);
void pause_menu_handle_input(PauseMenu* pm, SDL_Event* event,
                              Party* party, Inventory* inv,
                              GameState* gs, TimeSystem* ts,
                              SocialStats* ss, BondDB* cdb,
                              float player_x, float player_y,
                              int current_room,
                              float* out_x, float* out_y,
                              int* out_room, int* loaded);
void pause_menu_render(PauseMenu* pm, SDL_Renderer* renderer,
                       TTF_Font* font, Party* party);

#endif