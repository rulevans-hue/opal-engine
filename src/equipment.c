#include "equipment.h"
#include <string.h>
#include <stdio.h>
#include "equipment_loader.h"

// ── Helpers ──────────────────────────────────────────────────────────────────

const char* equip_slot_name(EquipSlot slot) {
    switch (slot) {
        case EQUIP_WEAPON:    return "Weapon";
        case EQUIP_ARMOR:     return "Armor";
        case EQUIP_ACCESSORY: return "Accessory";
        default:              return "Unknown";
    }
}

const char* equip_mod_name(StatModType type) {
    switch (type) {
        case MOD_ST:     return "ST";
        case MOD_MA:     return "MA";
        case MOD_EN:     return "EN";
        case MOD_AG:     return "AG";
        case MOD_LU:     return "LU";
        case MOD_MAX_HP: return "HP";
        case MOD_MAX_SP: return "SP";
        default:         return "?";
    }
}

// ── DB init ───────────────────────────────────────────────────────────────────

// Macro to reduce boilerplate when building pieces
#define BEGIN_PIECE(ID, NAME, DESC, SLOT, PRICE) \
{ \
    EquipmentPiece p = {0}; \
    p.id = (ID); \
    strncpy(p.name, (NAME), 63); \
    strncpy(p.description, (DESC), 127); \
    p.slot  = (SLOT); \
    p.price = (PRICE); \
    p.mod_count = 0;

#define ADD_MOD(TYPE, VAL) \
    p.mods[p.mod_count].type  = (TYPE); \
    p.mods[p.mod_count].value = (VAL); \
    p.mod_count++;

#define END_PIECE() \
    db->pieces[db->count++] = p; \
}

void equipdb_init(EquipDB* db) {
    db->count = 0;

    // Try data-driven load first; fall back to hardcoded below if it fails.
    if (equipment_load_json(db, "data/equipment.json"))
        return;

    // ── Weapons ───────────────────────────────────────────────────────────────

    BEGIN_PIECE(0, "Wooden Sword",
                "A basic training sword. Better than nothing.",
                EQUIP_WEAPON, 200)
        ADD_MOD(MOD_ST, 2)
    END_PIECE()

    BEGIN_PIECE(1, "Iron Sword",
                "A reliable iron blade.",
                EQUIP_WEAPON, 800)
        ADD_MOD(MOD_ST, 5)
    END_PIECE()

    BEGIN_PIECE(2, "Steel Sword",
                "A well-crafted steel sword.",
                EQUIP_WEAPON, 2000)
        ADD_MOD(MOD_ST, 9)
        ADD_MOD(MOD_EN, 1)
    END_PIECE()

    BEGIN_PIECE(3, "Crystal Blade",
                "A blade infused with arcane energy.",
                EQUIP_WEAPON, 5000)
        ADD_MOD(MOD_ST, 8)
        ADD_MOD(MOD_MA, 5)
    END_PIECE()

    BEGIN_PIECE(4, "Staff",
                "A wooden staff that channels magical power.",
                EQUIP_WEAPON, 600)
        ADD_MOD(MOD_MA, 4)
        ADD_MOD(MOD_ST, -1)
    END_PIECE()

    BEGIN_PIECE(5, "Silver Staff",
                "A finely crafted staff for magic users.",
                EQUIP_WEAPON, 2500)
        ADD_MOD(MOD_MA, 9)
        ADD_MOD(MOD_MAX_SP, 10)
    END_PIECE()

    BEGIN_PIECE(6, "Paintbrush",
                "An artist's brush repurposed as a weapon.",
                EQUIP_WEAPON, 1000)
        ADD_MOD(MOD_MA, 5)
        ADD_MOD(MOD_AG, 2)
    END_PIECE()

    BEGIN_PIECE(7, "Ornate Dagger",
                "Fast and elegant. Made for swift strikes.",
                EQUIP_WEAPON, 1800)
        ADD_MOD(MOD_ST, 4)
        ADD_MOD(MOD_AG, 5)
        ADD_MOD(MOD_LU, 2)
    END_PIECE()

    // ── Armor ─────────────────────────────────────────────────────────────────

    BEGIN_PIECE(10, "Cloth Robe",
                "Simple cloth that offers minimal protection.",
                EQUIP_ARMOR, 150)
        ADD_MOD(MOD_EN, 1)
        ADD_MOD(MOD_MAX_HP, 10)
    END_PIECE()

    BEGIN_PIECE(11, "Leather Jacket",
                "Light leather armor. Good mobility.",
                EQUIP_ARMOR, 600)
        ADD_MOD(MOD_EN, 3)
        ADD_MOD(MOD_MAX_HP, 20)
    END_PIECE()

    BEGIN_PIECE(12, "Chain Mail",
                "Interlocked rings provide solid defense.",
                EQUIP_ARMOR, 1800)
        ADD_MOD(MOD_EN, 7)
        ADD_MOD(MOD_MAX_HP, 40)
        ADD_MOD(MOD_AG, -1)
    END_PIECE()

    BEGIN_PIECE(13, "Combat Vest",
                "A reinforced vest balancing offense and defense.",
                EQUIP_ARMOR, 3000)
        ADD_MOD(MOD_EN, 8)
        ADD_MOD(MOD_ST, 2)
        ADD_MOD(MOD_MAX_HP, 30)
    END_PIECE()

    BEGIN_PIECE(14, "Mage Robe",
                "Enchanted cloth that amplifies magical power.",
                EQUIP_ARMOR, 2200)
        ADD_MOD(MOD_EN, 3)
        ADD_MOD(MOD_MA, 4)
        ADD_MOD(MOD_MAX_SP, 20)
    END_PIECE()

    BEGIN_PIECE(15, "Shadow Coat",
                "A dark coat that boosts speed and evasion.",
                EQUIP_ARMOR, 4000)
        ADD_MOD(MOD_EN, 5)
        ADD_MOD(MOD_AG, 5)
        ADD_MOD(MOD_LU, 3)
    END_PIECE()

    // ── Accessories ───────────────────────────────────────────────────────────

    BEGIN_PIECE(20, "Lucky Charm",
                "A small trinket said to bring good fortune.",
                EQUIP_ACCESSORY, 400)
        ADD_MOD(MOD_LU, 5)
    END_PIECE()

    BEGIN_PIECE(21, "Power Band",
                "A weighted band that builds strength over time.",
                EQUIP_ACCESSORY, 800)
        ADD_MOD(MOD_ST, 4)
        ADD_MOD(MOD_EN, 2)
    END_PIECE()

    BEGIN_PIECE(22, "Magic Ring",
                "Increases magical potency.",
                EQUIP_ACCESSORY, 1000)
        ADD_MOD(MOD_MA, 4)
        ADD_MOD(MOD_MAX_SP, 15)
    END_PIECE()

    BEGIN_PIECE(23, "Agility Anklet",
                "Lightweight brace that sharpens reflexes.",
                EQUIP_ACCESSORY, 1200)
        ADD_MOD(MOD_AG, 6)
    END_PIECE()

    BEGIN_PIECE(24, "Vitality Bead",
                "A carved bead that bolsters the body.",
                EQUIP_ACCESSORY, 1500)
        ADD_MOD(MOD_MAX_HP, 50)
        ADD_MOD(MOD_EN, 2)
    END_PIECE()

    BEGIN_PIECE(25, "Spirit Stone",
                "A glowing stone that reserves SP.",
                EQUIP_ACCESSORY, 2000)
        ADD_MOD(MOD_MAX_SP, 40)
        ADD_MOD(MOD_MA, 2)
    END_PIECE()

    BEGIN_PIECE(26, "Champion's Medal",
                "Awarded to those who have proven themselves.",
                EQUIP_ACCESSORY, 8000)
        ADD_MOD(MOD_ST, 5)
        ADD_MOD(MOD_MA, 5)
        ADD_MOD(MOD_EN, 5)
        ADD_MOD(MOD_AG, 5)
        ADD_MOD(MOD_LU, 5)
    END_PIECE()

    #undef BEGIN_PIECE
    #undef ADD_MOD
    #undef END_PIECE
}

// ── DB lookup ─────────────────────────────────────────────────────────────────

EquipmentPiece* equip_get(EquipDB* db, int id) {
    if (id < 0) return NULL;
    for (int i = 0; i < db->count; i++)
        if (db->pieces[i].id == id) return &db->pieces[i];
    return NULL;
}

// ── Apply / remove a piece's stat bonuses from a StatBlock ───────────────────

static void apply_mods(StatBlock* sb, EquipmentPiece* piece, int sign) {
    for (int i = 0; i < piece->mod_count; i++) {
        int v = piece->mods[i].value * sign;
        switch (piece->mods[i].type) {
            case MOD_ST:     sb->st     += v; break;
            case MOD_MA:     sb->ma     += v; break;
            case MOD_EN:     sb->en     += v; break;
            case MOD_AG:     sb->ag     += v; break;
            case MOD_LU:     sb->lu     += v; break;
            case MOD_MAX_HP:
                sb->max_hp += v;
                sb->hp     += v;
                if (sb->hp < 1)        sb->hp = 1;
                if (sb->hp > sb->max_hp) sb->hp = sb->max_hp;
                break;
            case MOD_MAX_SP:
                sb->max_sp += v;
                sb->sp     += v;
                if (sb->sp < 0)        sb->sp = 0;
                if (sb->sp > sb->max_sp) sb->sp = sb->max_sp;
                break;
        }
    }
}

// ── equip_set ────────────────────────────────────────────────────────────────
// Unequips whatever is in the slot, then equips the new piece.
// Pass piece_id = -1 to only unequip.

void equip_set(EquipDB* db, StatBlock* sb, EquipSlot slot, int piece_id) {
    // Remove old piece if any
    int old_id = sb->equipped[slot];
    if (old_id >= 0) {
        EquipmentPiece* old = equip_get(db, old_id);
        if (old) apply_mods(sb, old, -1);
    }

    // Set new slot
    sb->equipped[slot] = piece_id;

    // Apply new piece if any
    if (piece_id >= 0) {
        EquipmentPiece* piece = equip_get(db, piece_id);
        if (piece) apply_mods(sb, piece, +1);
    }
}

// ── equip_get_bonus ───────────────────────────────────────────────────────────
// Returns total bonus from all equipped pieces for one stat type.
// Mainly useful for display purposes — stats are already baked in.

int equip_get_bonus(EquipDB* db, StatBlock* sb, StatModType type) {
    int total = 0;
    for (int s = 0; s < EQUIP_SLOT_COUNT; s++) {
        EquipmentPiece* p = equip_get(db, sb->equipped[s]);
        if (!p) continue;
        for (int m = 0; m < p->mod_count; m++)
            if (p->mods[m].type == type)
                total += p->mods[m].value;
    }
    return total;
}