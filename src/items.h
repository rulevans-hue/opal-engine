#ifndef ITEMS_H
#define ITEMS_H

#include "stats.h"

#define MAX_ITEMS 64

typedef enum {
    ITEM_HEALING,
    ITEM_SP,
    ITEM_REVIVAL,
    ITEM_STATUS_CURE,
    ITEM_BATTLE,
    ITEM_FULL_HEAL,
    ITEM_FULL_SP,
    ITEM_FULL_HEAL_ALL,
    ITEM_FULL_HEAL_SP_ALL
} ItemType;

typedef struct {
    int         id;
    char        name[64];
    char        description[128];
    ItemType    type;
    int         power;          // HP/SP restored, or damage dealt
    StatusEffect cures;         // which status it cures (STATUS_NONE if none)
    Element     element;        // for battle items
    int         target_all;     // 1 = all allies/enemies
    int         price;          // shop price
} Item;

typedef struct {
    Item items[MAX_ITEMS];
    int  count;
} ItemDB;

void itemdb_init(ItemDB* db);
Item* itemdb_get(ItemDB* db, int id);

#endif