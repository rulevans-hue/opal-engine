#ifndef SHOP_H
#define SHOP_H

#include "bond.h"
#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "items.h"
#include "inventory.h"

#define MAX_SHOP_ITEMS 16

typedef struct {
    int item_id;
    int stock;      // -1 = unlimited
} ShopEntry;

typedef struct {
    char       name[64];
    ShopEntry  entries[MAX_SHOP_ITEMS];
    int        entry_count;
} Shop;

typedef enum {
    SHOP_CLOSED,
    SHOP_BROWSING,
    SHOP_CONFIRM_BUY
} ShopState;

typedef struct {
    ShopState state;
    int       selected;
    int       quantity;     // how many to buy
    Shop*     current_shop;
    BondDB* bonds;
} ShopMenu;

void shop_init(Shop* shop, const char* name);
void shop_add_item(Shop* shop, int item_id, int stock);

void shopmenu_init(ShopMenu* sm);
void shopmenu_open(ShopMenu* sm, Shop* shop);
void shopmenu_close(ShopMenu* sm);
void shopmenu_handle_input(ShopMenu* sm, SDL_Event* event,
                            Inventory* inv, ItemDB* db);
void shopmenu_render(ShopMenu* sm, SDL_Renderer* renderer,
                     TTF_Font* font, Inventory* inv, ItemDB* db);

#endif