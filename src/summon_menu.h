#ifndef SUMMON_MENU_H
#define SUMMON_MENU_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "summon.h"
#include "notify.h"
#include "inventory.h"
#include "skills.h"

typedef enum {
    PMENU_CLOSED,
    PMENU_BROWSE,
    PMENU_FUSE_A,
    PMENU_FUSE_B,
    PMENU_CONFIRM,
    PMENU_COMPENDIUM
} SummonMenuState;

typedef struct {
    SummonMenuState    state;
    int                 selected;
    int                 fuse_idx_a;
    Summon             fuse_result;
    SummonCompendium*  comp_ref;
    Inventory*          inv_ref;
} SummonMenu;

void summon_menu_init(SummonMenu* pm);
void summon_menu_open(SummonMenu* pm, SummonCompendium* comp, Inventory* inv);
void summon_menu_close(SummonMenu* pm);

void summon_menu_handle_input(SummonMenu* pm, SDL_Event* event,
                                SummonStock* stock, SummonDB* db,
                                SummonCompendium* comp,
                                NotifySystem* ns, Inventory* inv);

void summon_menu_render(SummonMenu* pm, SDL_Renderer* renderer,
                          TTF_Font* font, SummonStock* stock,
                          SkillDB* skilldb);

#endif