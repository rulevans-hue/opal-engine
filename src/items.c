#include "items.h"
#include <string.h>

void itemdb_init(ItemDB* db) {
    db->count = 0;

    // Try data-driven load first; fall back to hardcoded below if it fails.
    if (item_load_json(db, "data/items.json"))
        return;

    #define ADD_ITEM(ID, NAME, DESC, TYPE, PWR, CURES, ELEM, ALL, PRICE) \
    { \
        Item it = {0}; \
        it.id = ID; \
        strncpy(it.name, NAME, 63); \
        strncpy(it.description, DESC, 127); \
        it.type       = TYPE; \
        it.power      = PWR; \
        it.cures      = CURES; \
        it.element    = ELEM; \
        it.target_all = ALL; \
        it.price      = PRICE; \
        db->items[db->count++] = it; \
    }

    // ── Healing Items ──────────────────────────────────────────────────
    ADD_ITEM(0,  "Medicine",
             "Restores 50 HP to one ally.",
             ITEM_HEALING, 50, STATUS_NONE, ELEM_PHYSICAL, 0, 300)

    ADD_ITEM(1,  "Medicine DX",
             "Restores 150 HP to one ally.",
             ITEM_HEALING, 150, STATUS_NONE, ELEM_PHYSICAL, 0, 900)

    ADD_ITEM(2,  "Bead",
             "Fully restores HP to one ally.",
             ITEM_FULL_HEAL, 0, STATUS_NONE, ELEM_PHYSICAL, 0, 1200)

    ADD_ITEM(3,  "Bead Chain",
             "Fully restores HP to all allies.",
             ITEM_FULL_HEAL_ALL, 0, STATUS_NONE, ELEM_PHYSICAL, 1, 5000)

    ADD_ITEM(4,  "Amrita Drop",
             "Fully restores HP and cures all status for one ally.",
             ITEM_FULL_HEAL, 0, STATUS_NONE, ELEM_PHYSICAL, 0, 2000)

    ADD_ITEM(5,  "Amrita Shower",
             "Fully restores HP and cures all status for all allies.",
             ITEM_FULL_HEAL_ALL, 0, STATUS_NONE, ELEM_PHYSICAL, 1, 8000)

    ADD_ITEM(6,  "Soma",
             "Fully restores HP and SP to one ally.",
             ITEM_FULL_HEAL, 0, STATUS_NONE, ELEM_PHYSICAL, 0, 10000)

    ADD_ITEM(7,  "Soma Prime",
             "Fully restores HP and SP to all allies.",
             ITEM_FULL_HEAL_ALL, 0, STATUS_NONE, ELEM_PHYSICAL, 1, 50000)

    ADD_ITEM(8,  "Takemedic-All V",
             "Restores 50 HP to all allies.",
             ITEM_HEALING, 50, STATUS_NONE, ELEM_PHYSICAL, 1, 1500)

    ADD_ITEM(9,  "Takemedic-All DX",
             "Restores 150 HP to all allies.",
             ITEM_HEALING, 150, STATUS_NONE, ELEM_PHYSICAL, 1, 4000)

    // ── SP Items ───────────────────────────────────────────────────────
    ADD_ITEM(10, "Snuff Soul",
             "Restores 30 SP to one ally.",
             ITEM_SP, 30, STATUS_NONE, ELEM_PHYSICAL, 0, 1500)

    ADD_ITEM(11, "Snuff Soul DX",
             "Restores 100 SP to one ally.",
             ITEM_SP, 100, STATUS_NONE, ELEM_PHYSICAL, 0, 5000)

    ADD_ITEM(12, "Royal Jelly",
             "Fully restores SP to one ally.",
             ITEM_FULL_SP, 0, STATUS_NONE, ELEM_PHYSICAL, 0, 12000)

    ADD_ITEM(13, "Hiranya",
             "Fully restores SP to all allies.",
             ITEM_FULL_SP, 0, STATUS_NONE, ELEM_PHYSICAL, 1, 50000)

    // ── Revival Items ──────────────────────────────────────────────────
    ADD_ITEM(14, "Revival Bead",
             "Revives one ally with 50% HP.",
             ITEM_REVIVAL, 50, STATUS_NONE, ELEM_PHYSICAL, 0, 1500)

    ADD_ITEM(15, "Balm of Life",
             "Revives one ally with full HP.",
             ITEM_REVIVAL, 100, STATUS_NONE, ELEM_PHYSICAL, 0, 5000)

    // ── Status Cure Items ──────────────────────────────────────────────
    ADD_ITEM(16, "Digestive",
             "Cures Burn for one ally.",
             ITEM_STATUS_CURE, 0, STATUS_BURN, ELEM_PHYSICAL, 0, 400)

    ADD_ITEM(17, "Stomach Medicine",
             "Cures Dizzy for one ally.",
             ITEM_STATUS_CURE, 0, STATUS_DIZZY, ELEM_PHYSICAL, 0, 400)

    ADD_ITEM(18, "Eye Drops",
             "Cures Forget for one ally.",
             ITEM_STATUS_CURE, 0, STATUS_FORGET, ELEM_PHYSICAL, 0, 400)

    ADD_ITEM(19, "Tranquilizer",
             "Cures Fear and Despair for one ally.",
             ITEM_STATUS_CURE, 0, STATUS_FEAR, ELEM_PHYSICAL, 0, 400)

    ADD_ITEM(20, "Antibiotic Gel",
             "Cures Shock for one ally.",
             ITEM_STATUS_CURE, 0, STATUS_SHOCK, ELEM_PHYSICAL, 0, 400)

    ADD_ITEM(21, "De-Freeze",
             "Cures Freeze for one ally.",
             ITEM_STATUS_CURE, 0, STATUS_FREEZE, ELEM_PHYSICAL, 0, 400)

    ADD_ITEM(22, "Smelling Salts",
             "Cures Sleep, Confuse and Brainwash for one ally.",
             ITEM_STATUS_CURE, 0, STATUS_SLEEP, ELEM_PHYSICAL, 0, 400)

    // ── Battle Items ───────────────────────────────────────────────────
    ADD_ITEM(23, "Flame Urn",
             "Deals Fire damage to all enemies.",
             ITEM_BATTLE, 40, STATUS_NONE, ELEM_FIRE, 1, 800)

    ADD_ITEM(24, "Freeze Spray",
             "Deals Ice damage to all enemies.",
             ITEM_BATTLE, 40, STATUS_NONE, ELEM_ICE, 1, 800)

    ADD_ITEM(25, "Thunder Bomb",
             "Deals Electric damage to all enemies.",
             ITEM_BATTLE, 40, STATUS_NONE, ELEM_ELECTRIC, 1, 800)

    ADD_ITEM(26, "Gust Bottle",
             "Deals Wind damage to all enemies.",
             ITEM_BATTLE, 40, STATUS_NONE, ELEM_WIND, 1, 800)

    ADD_ITEM(27, "Psy Bomb",
             "Deals Psychic damage to all enemies.",
             ITEM_BATTLE, 40, STATUS_NONE, ELEM_PSYCHIC, 1, 800)

    ADD_ITEM(28, "Nuke Bottle",
             "Deals Nuclear damage to all enemies.",
             ITEM_BATTLE, 40, STATUS_NONE, ELEM_NUCLEAR, 1, 800)

    ADD_ITEM(29, "Bless Charm",
             "Deals Bless damage to all enemies.",
             ITEM_BATTLE, 40, STATUS_NONE, ELEM_BLESS, 1, 800)

    ADD_ITEM(30, "Curse Charm",
             "Deals Curse damage to all enemies.",
             ITEM_BATTLE, 40, STATUS_NONE, ELEM_CURSE, 1, 800)

    ADD_ITEM(31, "Stun Needle",
             "Inflicts Shock on one enemy.",
             ITEM_BATTLE, 0, STATUS_NONE, ELEM_ELECTRIC, 0, 600)

    ADD_ITEM(32, "Smoke Bomb",
             "Guarantees escape from battle.",
             ITEM_BATTLE, 0, STATUS_NONE, ELEM_PHYSICAL, 0, 800)

    ADD_ITEM(33, "Escape Rope",
             "Instantly escape from the Hungerscape.",
             ITEM_BATTLE, 0, STATUS_NONE, ELEM_PHYSICAL, 0, 1000)

    #undef ADD_ITEM
}

Item* itemdb_get(ItemDB* db, int id) {
    for (int i = 0; i < db->count; i++)
        if (db->items[i].id == id) return &db->items[i];
    return NULL;
}