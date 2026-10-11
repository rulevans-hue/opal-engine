#include "pause_menu.h"
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

void pause_menu_init(PauseMenu* pm) {
    pm->state        = PAUSE_CLOSED;
    pm->selected     = 0;
    pm->slots_loaded = 0;
}

void pause_menu_open(PauseMenu* pm) {
    pm->state    = PAUSE_MAIN;
    pm->selected = 0;
    for (int i = 0; i < SAVE_SLOTS; i++) {
        if (!load_game(i, &pm->slots[i]))
            pm->slots[i].used = 0;
    }
    pm->slots_loaded = 1;
}

void pause_menu_close(PauseMenu* pm) {
    pm->state = PAUSE_CLOSED;
}

void pause_menu_handle_input(PauseMenu* pm, SDL_Event* event,
                              Party* party, Inventory* inv,
                              GameState* gs, TimeSystem* ts,
                              SocialStats* ss, BondDB* cdb,
                              float player_x, float player_y,
                              int current_room,
                              float* out_x, float* out_y,
                              int* out_room, int* loaded) {
    if (pm->state == PAUSE_CLOSED) return;
    if (event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    *loaded = 0;

    if (pm->state == PAUSE_MAIN) {
        switch (key) {
            case SDLK_UP:
                pm->selected = (pm->selected - 1 + 3) % 3;
                break;
            case SDLK_DOWN:
                pm->selected = (pm->selected + 1) % 3;
                break;
            case SDLK_ESCAPE:
                pause_menu_close(pm);
                break;
            case SDLK_e:
            case SDLK_RETURN:
                if (pm->selected == 0) {
                    pm->state    = PAUSE_SAVE;
                    pm->selected = 0;
                } else if (pm->selected == 1) {
                    pm->state    = PAUSE_LOAD;
                    pm->selected = 0;
                } else {
                    pause_menu_close(pm);
                }
                break;
        }
    } else if (pm->state == PAUSE_SAVE) {
        switch (key) {
            case SDLK_UP:
                pm->selected = (pm->selected - 1 + SAVE_SLOTS) % SAVE_SLOTS;
                break;
            case SDLK_DOWN:
                pm->selected = (pm->selected + 1) % SAVE_SLOTS;
                break;
            case SDLK_ESCAPE:
                pm->state    = PAUSE_MAIN;
                pm->selected = 0;
                break;
            case SDLK_e:
            case SDLK_RETURN: {
                SaveData sd;
                // NULL for dungeon_sys — pause menu saves overworld state only.
                // Dungeon state is saved via the safe room save point in dungeon.c.
                save_data_from_state(&sd, party, inv, gs, ts, ss, cdb,
                                     player_x, player_y, current_room,
                                     NULL);
                save_game(pm->selected, &sd);
                pm->slots[pm->selected] = sd;
                pm->state    = PAUSE_MAIN;
                pm->selected = 0;
                break;
            }
        }
    } else if (pm->state == PAUSE_LOAD) {
        switch (key) {
            case SDLK_UP:
                pm->selected = (pm->selected - 1 + SAVE_SLOTS) % SAVE_SLOTS;
                break;
            case SDLK_DOWN:
                pm->selected = (pm->selected + 1) % SAVE_SLOTS;
                break;
            case SDLK_ESCAPE:
                pm->state    = PAUSE_MAIN;
                pm->selected = 0;
                break;
            case SDLK_e:
            case SDLK_RETURN:
                if (pm->slots[pm->selected].used) {
                    int room_id;
                    // NULL for dungeon_sys — load restores overworld position.
                    // Dungeon state restore happens in main.c on title screen load.
                    save_data_to_state(&pm->slots[pm->selected],
                                       party, inv, gs, ts, ss, cdb,
                                       out_x, out_y, &room_id,
                                       NULL);
                    *out_room = room_id;
                    *loaded   = 1;
                    pause_menu_close(pm);
                }
                break;
        }
    }
}

void pause_menu_render(PauseMenu* pm, SDL_Renderer* renderer,
                       TTF_Font* font, Party* party) {
    if (pm->state == PAUSE_CLOSED) return;

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};

    draw_rect(renderer, 0, 0, 1280, 720, 0, 0, 0, 180);

    if (pm->state == PAUSE_MAIN) {
        draw_rect(renderer,   490, 240, 300, 200, 10, 10, 30, 255);
        draw_border(renderer, 490, 240, 300, 200, 80, 80, 160, 255);
        draw_text(renderer, font, "PAUSED", white, 580, 255);

        const char* opts[] = { "Save", "Load", "Resume" };
        for (int i = 0; i < 3; i++) {
            SDL_Color col = (i == pm->selected) ? yellow : white;
            char label[32];
            snprintf(label, 32, "%s %s",
                     i == pm->selected ? ">" : " ", opts[i]);
            draw_text(renderer, font, label, col, 520, 300 + i * 44);
        }
        draw_text(renderer, font, "Esc to resume", gray, 520, 420);

    } else if (pm->state == PAUSE_SAVE ||
               pm->state == PAUSE_LOAD) {
        draw_rect(renderer,   340, 180, 600, 340, 10, 10, 30, 255);
        draw_border(renderer, 340, 180, 600, 340, 80, 80, 160, 255);

        const char* title = pm->state == PAUSE_SAVE ?
                            "Save Game" : "Load Game";
        draw_text(renderer, font, title, white, 580, 192);

        for (int i = 0; i < SAVE_SLOTS; i++) {
            int sy = 230 + i * 90;
            int sx = 360;

            SDL_Color bc = (i == pm->selected) ?
                           (SDL_Color){60, 60, 100, 255} :
                           (SDL_Color){20, 20, 50,  255};
            draw_rect(renderer,   sx, sy, 560, 80,
                      bc.r, bc.g, bc.b, 255);
            draw_border(renderer, sx, sy, 560, 80,
                        i == pm->selected ? 200 : 80,
                        i == pm->selected ? 200 : 80,
                        i == pm->selected ? 255 : 160,
                        255);

            char slot_label[16];
            snprintf(slot_label, 16, "Slot %d", i + 1);
            draw_text(renderer, font, slot_label, yellow, sx + 10, sy + 8);

            if (pm->slots[i].used) {
                draw_text(renderer, font,
                          pm->slots[i].timestamp, white,
                          sx + 10, sy + 34);
                char party_str[128] = "";
                for (int j = 0; j < pm->slots[i].party_count; j++) {
                    char buf[32];
                    snprintf(buf, 32, "%s Lv.%d  ",
                             pm->slots[i].members[j].name,
                             pm->slots[i].members[j].level);
                    strncat(party_str, buf,
                            sizeof(party_str) - strlen(party_str) - 1);
                }
                draw_text(renderer, font, party_str, gray,
                          sx + 10, sy + 56);
            } else {
                draw_text(renderer, font, "Empty", gray,
                          sx + 10, sy + 34);
            }
        }
        draw_text(renderer, font, "E: confirm  Esc: back",
                  gray, 480, 500);
    }
}