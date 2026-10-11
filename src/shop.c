#include "shop.h"
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

void shop_init(Shop* shop, const char* name) {
    strncpy(shop->name, name, 63);
    shop->entry_count = 0;
}

void shop_add_item(Shop* shop, int item_id, int stock) {
    if (shop->entry_count >= MAX_SHOP_ITEMS) return;
    shop->entries[shop->entry_count].item_id = item_id;
    shop->entries[shop->entry_count].stock   = stock;
    shop->entry_count++;
}

void shopmenu_init(ShopMenu* sm) {
    sm->state        = SHOP_CLOSED;
    sm->selected     = 0;
    sm->quantity     = 1;
    sm->current_shop = NULL;
    sm->bonds   = NULL;
}

void shopmenu_open(ShopMenu* sm, Shop* shop) {
    sm->state        = SHOP_BROWSING;
    sm->selected     = 0;
    sm->quantity     = 1;
    sm->current_shop = shop;
}

void shopmenu_close(ShopMenu* sm) {
    sm->state        = SHOP_CLOSED;
    sm->current_shop = NULL;
}

void shopmenu_handle_input(ShopMenu* sm, SDL_Event* event,
                            Inventory* inv, ItemDB* db) {
    if (sm->state == SHOP_CLOSED) return;
    if (event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Shop* shop = sm->current_shop;

    if (sm->state == SHOP_BROWSING) {
        switch (key) {
            case SDLK_UP:
                sm->selected =
                    (sm->selected - 1 + shop->entry_count)
                    % shop->entry_count;
                break;
            case SDLK_DOWN:
                sm->selected =
                    (sm->selected + 1) % shop->entry_count;
                break;
            case SDLK_ESCAPE:
                shopmenu_close(sm);
                break;
            case SDLK_e:
            case SDLK_RETURN:
                sm->state    = SHOP_CONFIRM_BUY;
                sm->quantity = 1;
                break;
        }

    } else if (sm->state == SHOP_CONFIRM_BUY) {
        ShopEntry* entry = &shop->entries[sm->selected];
        Item* item = itemdb_get(db, entry->item_id);
        if (!item) { sm->state = SHOP_BROWSING; return; }

        switch (key) {
            case SDLK_UP:
            case SDLK_RIGHT:
                sm->quantity++;
                // Cap at what player can afford
                if (sm->quantity * item->price > inv->yen)
                    sm->quantity = inv->yen / item->price;
                if (sm->quantity < 1) sm->quantity = 1;
                break;
            case SDLK_DOWN:
            case SDLK_LEFT:
                sm->quantity--;
                if (sm->quantity < 1) sm->quantity = 1;
                break;
            case SDLK_ESCAPE:
                sm->state = SHOP_BROWSING;
                break;
            case SDLK_e:
            case SDLK_RETURN: {
                int price = item->price;
                // Apply bond discount
                if (sm->bonds) {
                    int discount = bond_get_shop_discount(sm->bonds);
                    if (discount > 0)
                        price = price - (price * discount / 100);
                    if (price < 1) price = 1;
                }
                int total = price * sm->quantity;
                if (inv->yen >= total) {
                    inv->yen -= total;
                    inventory_add(inv, item->id, sm->quantity);
                    printf("Bought %d x %s for %d yen\n",
                           sm->quantity, item->name, total);
                }
                sm->state    = SHOP_BROWSING;
                sm->quantity = 1;
                break;
            }
        }
    }
}

void shopmenu_render(ShopMenu* sm, SDL_Renderer* renderer,
                     TTF_Font* font, Inventory* inv, ItemDB* db) {
    if (sm->state == SHOP_CLOSED) return;

    Shop* shop = sm->current_shop;
    if (!shop) return;

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color green  = {100, 220, 100, 255};
    SDL_Color red    = {220,  60,  60, 255};

    // Dark overlay
    draw_rect(renderer, 0, 0, 1280, 720, 0, 0, 0, 180);

    // Main shop panel
    draw_rect(renderer,   160, 80, 960, 560, 10, 10, 30, 255);
    draw_border(renderer, 160, 80, 960, 560, 80, 80, 160, 255);

    // Shop name
    draw_text(renderer, font, shop->name, yellow, 180, 92);

    // Yen display
    char yen_str[32];
    snprintf(yen_str, 32, "Yen: %d", inv->yen);
    draw_text(renderer, font, yen_str, green, 980, 92);

    // Column headers
    draw_text(renderer, font, "Item", gray, 180, 125);
    draw_text(renderer, font, "Price", gray, 700, 125);
    draw_text(renderer, font, "Owned", gray, 820, 125);
    draw_rect(renderer, 180, 148, 920, 1, 80, 80, 160, 255);

    // Item list
    int visible = 12;
    int start = 0;
    if (sm->selected >= visible)
        start = sm->selected - visible + 1;

    for (int i = start;
         i < shop->entry_count && i < start + visible; i++) {
        ShopEntry* entry = &shop->entries[i];
        Item* item = itemdb_get(db, entry->item_id);
        if (!item) continue;

        int iy = 158 + (i - start) * 30;
        int owned = inventory_count(inv, item->id);

        // Highlight selected
        if (i == sm->selected) {
            draw_rect(renderer, 180, iy - 2, 920, 28,
                      40, 40, 80, 255);
        }

        SDL_Color col = (i == sm->selected) ? yellow : white;

        // Can't afford — show in red
        if (item->price > inv->yen)
            col = (SDL_Color){180, 80, 80, 255};

        char label[64];
        snprintf(label, 64, "%s %s",
                 i == sm->selected ? ">" : " ", item->name);
        draw_text(renderer, font, label, col, 180, iy);

        int display_price = item->price;
        if (sm->bonds) {
            int discount = bond_get_shop_discount(sm->bonds);
            if (discount > 0)
                display_price = item->price - (item->price * discount / 100);
            if (display_price < 1) display_price = 1;
        }
        char price_str[32];
        if (display_price < item->price)
            snprintf(price_str, 32, "%d (-%d%%)",
                     display_price,
                     bond_get_shop_discount(sm->bonds));
        else
            snprintf(price_str, 32, "%d", item->price);
        draw_text(renderer, font, price_str, col, 700, iy);

        char owned_str[8];
        snprintf(owned_str, 8, "%d", owned);
        draw_text(renderer, font, owned_str, gray, 820, iy);
    }

    // Selected item description
    if (sm->selected < shop->entry_count) {
        Item* item = itemdb_get(db,
            shop->entries[sm->selected].item_id);
        if (item) {
            draw_rect(renderer, 180, 520, 920, 1,
                      80, 80, 160, 255);
            draw_text(renderer, font, item->description,
                      gray, 180, 530);
        }
    }

    // Controls hint
    draw_text(renderer, font, "E: buy  Esc: close",
              gray, 180, 558);

    // Confirm buy overlay
    if (sm->state == SHOP_CONFIRM_BUY &&
        sm->selected < shop->entry_count) {
        Item* item = itemdb_get(db,
            shop->entries[sm->selected].item_id);
        if (item) {
            draw_rect(renderer,   390, 260, 500, 180,
                      15, 15, 40, 255);
            draw_border(renderer, 390, 260, 500, 180,
                        200, 200, 255, 255);

            draw_text(renderer, font, "How many?",
                      white, 560, 275);

            char qty_str[64];
            snprintf(qty_str, 64, "< %d >", sm->quantity);
            draw_text(renderer, font, qty_str, yellow, 555, 315);

            int total = item->price * sm->quantity;
            char total_str[64];
            snprintf(total_str, 64, "Total: %d yen", total);
            SDL_Color tc = total > inv->yen ? red : green;
            draw_text(renderer, font, total_str, tc, 530, 355);

            draw_text(renderer, font,
                      "E: confirm  Esc: cancel",
                      gray, 460, 400);
        }
    }
}

