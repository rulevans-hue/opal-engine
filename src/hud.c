#include "hud.h"
#include <stdio.h>

static void draw_text(SDL_Renderer* r, TTF_Font* font,
                      const char* text, SDL_Color color, int x, int y) {
    if (!text || text[0] == '\0') return;
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

void hud_render(SDL_Renderer* renderer, TTF_Font* font,
                TimeSystem* ts, WorldSystem* ws, SocialStats* ss,
                DungeonSystem* dungeon_sys) {

    // Dungeon has its own HUD rendered inside dungeon_render()
    if (dungeon_sys && dungeon_get_active(dungeon_sys) && dungeon_get_active(dungeon_sys)->active) return;

    // ── Overworld HUD ─────────────────────────────────────────────────────────

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color cyan   = { 80, 200, 220, 255};
    SDL_Color red    = {220,  60,  60, 255};

    // Date/time panel — top right
    draw_rect(renderer, 980, 8, 292, 80, 5, 5, 20, 200);

    char date[64];
    snprintf(date, 64, "%s  %d/%d",
             dayofweek_name(ts->day_of_week), ts->month, ts->date);
    draw_text(renderer, font, date, white, 988, 14);

    char timeslot[32];
    snprintf(timeslot, 32, "%s", timeslot_name(ts->slot));
    draw_text(renderer, font, timeslot, yellow, 988, 38);
    draw_text(renderer, font, weather_name(ts->weather), cyan, 1140, 38);

    SDL_Color wcol = ws->current == WORLD_HUNGERSCAPE ? red : white;
    const char* wname = ws->current == WORLD_HUNGERSCAPE ?
                        "[ HUNGERSCAPE ]" : "[ Real World ]";
    draw_text(renderer, font, wname, wcol, 988, 62);

    // Social stats panel — bottom left
    draw_rect(renderer, 8, 600, 380, 112, 5, 5, 20, 200);

    SDL_Color stat_colors[SOCIAL_COUNT] = {
        {100, 180, 255, 255},
        {255, 150, 200, 255},
        {255, 140,  60, 255},
        {100, 220, 120, 255},
        {200, 160, 255, 255},
    };

    for (int i = 0; i < SOCIAL_COUNT; i++) {
        int sx = 14 + (i % 3) * 126;
        int sy = 606 + (i / 3) * 50;

        draw_text(renderer, font,
                  social_stat_name((SocialStatType)i),
                  stat_colors[i], sx, sy);
        draw_text(renderer, font,
                  social_rank_name((SocialStatType)i, ss->rank[i]),
                  white, sx, sy + 20);

        draw_rect(renderer, sx, sy + 38, 110, 6, 30, 30, 60, 255);
        if (ss->rank[i] < SOCIAL_MAX_RANK) {
            int fill = (ss->exp[i] * 110) / SOCIAL_EXP_PER_RANK;
            draw_rect(renderer, sx, sy + 38, fill, 6,
                      stat_colors[i].r, stat_colors[i].g,
                      stat_colors[i].b, 255);
        } else {
            draw_rect(renderer, sx, sy + 38, 110, 6,
                      stat_colors[i].r, stat_colors[i].g,
                      stat_colors[i].b, 255);
        }
    }
}