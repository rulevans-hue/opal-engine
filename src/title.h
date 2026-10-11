#ifndef TITLE_H
#define TITLE_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "save.h"

typedef enum {
    TITLE_MAIN,
    TITLE_LOAD,
    TITLE_DONE_NEW,
    TITLE_DONE_LOAD
} TitleState;

typedef struct {
    TitleState state;
    int        selected;
    SaveData   slots[SAVE_SLOTS];
    int        load_slot;   // which slot was chosen
} TitleScreen;

void title_init(TitleScreen* t);
void title_handle_input(TitleScreen* t, SDL_Event* event);
void title_render(TitleScreen* t, SDL_Renderer* renderer, TTF_Font* font);
int  title_is_done(TitleScreen* t);

#endif