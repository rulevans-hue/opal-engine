#include "notify_sdl2.h"

void notify_render(NotifySystem* ns, SDL_Renderer* renderer,
                   TTF_Font* font) {
    int y = 140;
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (!ns->notes[i].active) continue;

        // Fade out in last 60 frames
        Uint8 alpha = 255;
        if (ns->notes[i].timer < 60)
            alpha = (Uint8)(ns->notes[i].timer * 255 / 60);

        // Background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 5, 5, 20, (Uint8)(alpha * 0.8f));
        SDL_Rect bg = { 960, y - 2, 310, 30 };
        SDL_RenderFillRect(renderer, &bg);

        // Text
        SDL_Color col = {
            ns->notes[i].r,
            ns->notes[i].g,
            ns->notes[i].b,
            alpha
        };
        SDL_Surface* surf = TTF_RenderText_Blended(font,
                                ns->notes[i].text, col);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_SetTextureAlphaMod(tex, alpha);
            SDL_Rect dst = { 968, y, surf->w, surf->h };
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }

        y += 34;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}