#include "inventory.h"
#include <string.h>

void inventory_init(Inventory* inv) {
    inv->slot_count = 0;
    inv->yen        = 5000; // starting money

    // Start with a few items for testing
    inventory_add(inv, 0,  3);  // Medicine x3
    inventory_add(inv, 10, 2);  // Snuff Soul x2
    inventory_add(inv, 14, 2);  // Revival Bead x2
    inventory_add(inv, 23, 2);  // Flame Urn x2
    inventory_add(inv, 24, 2);  // Freeze Spray x2
}

void inventory_add(Inventory* inv, int item_id, int quantity) {
    // Check if item already exists
    for (int i = 0; i < inv->slot_count; i++) {
        if (inv->slots[i].item_id == item_id) {
            inv->slots[i].quantity += quantity;
            return;
        }
    }
    // Add new slot
    if (inv->slot_count < INVENTORY_MAX_SLOTS) {
        inv->slots[inv->slot_count].item_id  = item_id;
        inv->slots[inv->slot_count].quantity = quantity;
        inv->slot_count++;
    }
}

void inventory_remove(Inventory* inv, int item_id, int quantity) {
    for (int i = 0; i < inv->slot_count; i++) {
        if (inv->slots[i].item_id == item_id) {
            inv->slots[i].quantity -= quantity;
            if (inv->slots[i].quantity <= 0) {
                // Remove slot by shifting
                for (int j = i; j < inv->slot_count - 1; j++)
                    inv->slots[j] = inv->slots[j+1];
                inv->slot_count--;
            }
            return;
        }
    }
}

int inventory_count(Inventory* inv, int item_id) {
    for (int i = 0; i < inv->slot_count; i++)
        if (inv->slots[i].item_id == item_id)
            return inv->slots[i].quantity;
    return 0;
}

int inventory_use_item(Inventory* inv, ItemDB* db, int item_id,
                       StatBlock* target, Party* party,
                       BondDB* bonds) {
    Item* item = itemdb_get(db, item_id);
    if (!item) return 0;
    if (inventory_count(inv, item_id) <= 0) return 0;

    // Item potency perk: scales HP/SP restoration by a percentage bonus
    int potency_bonus = 0;
    if (bonds)
        potency_bonus = bond_get_perk_value(bonds, PERK_ITEM_POTENCY);

    // Helper macro — scales a power value by the potency bonus
    // e.g. potency_bonus=25 turns 50 into 62, potency_bonus=50 turns 50 into 75
    #define SCALE(x) ((x) + (x) * potency_bonus / 100)

    switch (item->type) {
        case ITEM_HEALING:
            if (!item->target_all) {
                target->hp += SCALE(item->power);
                if (target->hp > target->max_hp)
                    target->hp = target->max_hp;
            } else {
                for (int i = 0; i < party->count; i++) {
                    party->members[i].hp += SCALE(item->power);
                    if (party->members[i].hp > party->members[i].max_hp)
                        party->members[i].hp = party->members[i].max_hp;
                }
            }
            break;

        case ITEM_SP:
            if (!item->target_all) {
                target->sp += SCALE(item->power);
                if (target->sp > target->max_sp)
                    target->sp = target->max_sp;
            } else {
                for (int i = 0; i < party->count; i++) {
                    party->members[i].sp += SCALE(item->power);
                    if (party->members[i].sp > party->members[i].max_sp)
                        party->members[i].sp = party->members[i].max_sp;
                }
            }
            break;

        case ITEM_FULL_HEAL:
            target->hp = target->max_hp;
            target->sp = target->max_sp;
            statblock_clear_status(target);
            break;

        case ITEM_FULL_HEAL_ALL:
            for (int i = 0; i < party->count; i++) {
                party->members[i].hp = party->members[i].max_hp;
                party->members[i].sp = party->members[i].max_sp;
                statblock_clear_status(&party->members[i]);
            }
            break;

        case ITEM_FULL_SP:
            if (!item->target_all) {
                target->sp = target->max_sp;
            } else {
                for (int i = 0; i < party->count; i++)
                    party->members[i].sp = party->members[i].max_sp;
            }
            break;

        case ITEM_FULL_HEAL_SP_ALL:
            for (int i = 0; i < party->count; i++) {
                party->members[i].hp = party->members[i].max_hp;
                party->members[i].sp = party->members[i].max_sp;
            }
            break;

        case ITEM_REVIVAL:
            if (!target->alive) {
                target->alive = 1;
                target->hp = (item->power == 100)
                             ? target->max_hp
                             : target->max_hp / 2;
            }
            break;

        case ITEM_STATUS_CURE:
            statblock_clear_status(target);
            break;

        case ITEM_BATTLE:
            // Battle items are handled by combat system directly
            break;
    }

    #undef SCALE

    return 1;
}