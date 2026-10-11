#ifndef INVENTORY_H
#define INVENTORY_H

#include "items.h"
#include "stats.h"
#include "party.h"
#include "bond.h"

#define INVENTORY_MAX_SLOTS 32

typedef struct {
    int item_id;
    int quantity;
} InventorySlot;

typedef struct {
    InventorySlot slots[INVENTORY_MAX_SLOTS];
    int           slot_count;
    int           yen;          // money
} Inventory;

void inventory_init(Inventory* inv);
void inventory_add(Inventory* inv, int item_id, int quantity);
void inventory_remove(Inventory* inv, int item_id, int quantity);
int  inventory_count(Inventory* inv, int item_id);
int  inventory_use_item(Inventory* inv, ItemDB* db, int item_id,
                        StatBlock* target, Party* party,
                        BondDB* bonds);

#endif