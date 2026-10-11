#ifndef EQUIPMENT_H
#define EQUIPMENT_H

#include "stats.h"

#define MAX_EQUIPMENT     128
#define MAX_STAT_MODS     8
#define EQUIP_SLOT_COUNT  3

// Which slot the piece goes in
typedef enum {
    EQUIP_WEAPON    = 0,
    EQUIP_ARMOR     = 1,
    EQUIP_ACCESSORY = 2
} EquipSlot;

// Which stat the mod affects
typedef enum {
    MOD_ST = 0,
    MOD_MA,
    MOD_EN,
    MOD_AG,
    MOD_LU,
    MOD_MAX_HP,
    MOD_MAX_SP
} StatModType;

typedef struct {
    StatModType type;
    int         value;  // can be negative (cursed gear etc.)
} StatMod;

typedef struct {
    int     id;
    char    name[64];
    char    description[128];
    EquipSlot slot;
    int     price;
    StatMod mods[MAX_STAT_MODS];
    int     mod_count;
} EquipmentPiece;

typedef struct {
    EquipmentPiece pieces[MAX_EQUIPMENT];
    int            count;
} EquipDB;

// ── DB management ────────────────────────────────────────────────────────────
void          equipdb_init(EquipDB* db);
EquipmentPiece* equip_get(EquipDB* db, int id);

// ── Equip / unequip ──────────────────────────────────────────────────────────
// Removes the old piece's stat bonuses, applies the new piece's bonuses.
// Pass id = -1 to just unequip.
void equip_set(EquipDB* db, StatBlock* sb, EquipSlot slot, int piece_id);

// ── Queries ──────────────────────────────────────────────────────────────────
// Returns the total bonus from all equipped pieces for one stat type.
int  equip_get_bonus(EquipDB* db, StatBlock* sb, StatModType type);

// Human-readable slot name
const char* equip_slot_name(EquipSlot slot);

// Human-readable stat mod name
const char* equip_mod_name(StatModType type);

#endif