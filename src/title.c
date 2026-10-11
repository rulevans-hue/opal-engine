#include "title.h"
#include <string.h>
#include <stdio.h>

static void draw_text(SDL_Renderer* r, TTF_Font* font,
                      const char* text, SDL_Color color, int x, int y) {
    if (!text || !text[0]) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

static void draw_rect(SDL_Renderer* r, int x, int y, int w, int h,
                      Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void draw_border(SDL_Renderer* r, int x, int y, int w, int h,
                        Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderDrawRect(r, &rect);
}

void title_init(TitleScreen* t) {
    t->state     = TITLE_MAIN;
    t->selected  = 0;
    t->load_slot = -1;

    // Check which save slots exist
    for (int i = 0; i < SAVE_SLOTS; i++) {
        if (!load_game(i, &t->slots[i]))
            t->slots[i].used = 0;
    }
}

void title_handle_input(TitleScreen* t, SDL_Event* event) {
    if (event->type != SDL_KEYDOWN) return;
    SDL_Keycode key = event->key.keysym.sym;

    if (t->state == TITLE_MAIN) {
        // Options: New Game, Load Game, Quit
        switch (key) {
            case SDLK_UP:
                t->selected = (t->selected - 1 + 3) % 3;
                break;
            case SDLK_DOWN:
                t->selected = (t->selected + 1) % 3;
                break;
            case SDLK_e:
            case SDLK_RETURN:
                if (t->selected == 0) {
                    t->state = TITLE_DONE_NEW;
                } else if (t->selected == 1) {
                    t->state    = TITLE_LOAD;
                    t->selected = 0;
                } else {
                    // Quit — signal via special state
                    t->state = TITLE_DONE_NEW;
                }
                break;
        }

    } else if (t->state == TITLE_LOAD) {
        switch (key) {
            case SDLK_UP:
                t->selected = (t->selected - 1 + SAVE_SLOTS) % SAVE_SLOTS;
                break;
            case SDLK_DOWN:
                t->selected = (t->selected + 1) % SAVE_SLOTS;
                break;
            case SDLK_ESCAPE:
                t->state    = TITLE_MAIN;
                t->selected = 0;
                break;
            case SDLK_e:
            case SDLK_RETURN:
                if (t->slots[t->selected].used) {
                    t->load_slot = t->selected;
                    t->state     = TITLE_DONE_LOAD;
                }
                break;
        }
    }
}

int title_is_done(TitleScreen* t) {
    return t->state == TITLE_DONE_NEW ||
           t->state == TITLE_DONE_LOAD;
}

void title_render(TitleScreen* t, SDL_Renderer* renderer,
                  TTF_Font* font) {
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color red    = {220,  60,  60, 255};

    // Background
    draw_rect(renderer, 0, 0, 1280, 720, 5, 5, 20, 255);

    // Decorative lines
    for (int i = 0; i < 5; i++) {
        SDL_SetRenderDrawColor(renderer, 30 + i*8, 30 + i*4, 60 + i*10, 255);
        SDL_RenderDrawLine(renderer, 0, 100 + i*4, 1280, 100 + i*4);
        SDL_RenderDrawLine(renderer, 0, 620 - i*4, 1280, 620 - i*4);
    }

    if (t->state == TITLE_MAIN) {
        // Game title
        draw_text(renderer, font, "P R E G s o n a", yellow, 460, 180);
        draw_text(renderer, font, "- - - - - - - - - - - - - -",
                  gray, 420, 220);

        // Menu options
        const char* opts[] = { "New Game", "Load Game", "Quit" };
        draw_rect(renderer,   490, 300, 300, 180, 10, 10, 30, 255);
        draw_border(renderer, 490, 300, 300, 180, 80, 80, 160, 255);

        for (int i = 0; i < 3; i++) {
            SDL_Color col = (i == t->selected) ? yellow : white;
            char label[32];
            snprintf(label, 32, "%s %s",
                     i == t->selected ? ">" : " ", opts[i]);
            draw_text(renderer, font, label, col, 520, 320 + i * 48);
        }

        draw_text(renderer, font, "E / Enter to select",
                  gray, 530, 500);

    } else if (t->state == TITLE_LOAD) {
        draw_text(renderer, font, "Load Game", white, 560, 150);
        draw_text(renderer, font, "- - - - - - - - - - - - - -",
                  gray, 420, 185);

        for (int i = 0; i < SAVE_SLOTS; i++) {
            int sy = 220 + i * 100;
            int sx = 340;

            SDL_Color bc = (i == t->selected) ?
                           (SDL_Color){60, 60, 100, 255} :
                           (SDL_Color){20, 20, 50,  255};
            draw_rect(renderer,   sx, sy, 600, 85,
                      bc.r, bc.g, bc.b, 255);
            draw_border(renderer, sx, sy, 600, 85,
                        i == t->selected ? 200 : 80,
                        i == t->selected ? 200 : 80,
                        i == t->selected ? 255 : 160,
                        255);

            char slot_label[16];
            snprintf(slot_label, 16, "Slot %d", i + 1);
            draw_text(renderer, font, slot_label, yellow, sx + 12, sy + 8);

            if (t->slots[i].used) {
                draw_text(renderer, font,
                          t->slots[i].timestamp, white,
                          sx + 12, sy + 34);

                char party_str[128] = "";
                for (int j = 0; j < t->slots[i].party_count; j++) {
                    char buf[32];
                    snprintf(buf, 32, "%s Lv.%d  ",
                             t->slots[i].members[j].name,
                             t->slots[i].members[j].level);
                    strncat(party_str, buf,
                            sizeof(party_str) - strlen(party_str) - 1);
                }
                draw_text(renderer, font, party_str, gray,
                          sx + 12, sy + 58);
            } else {
                draw_text(renderer, font, "- Empty -", gray,
                          sx + 12, sy + 34);
            }
        }

        draw_text(renderer, font, "E: load  Esc: back",
                  gray, 530, 560);
    }
}