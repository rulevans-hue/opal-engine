#ifndef ENEMY_REGISTRY_H
#define ENEMY_REGISTRY_H

#include "entity.h"
#include "stats.h"

// ── Enemy IDs ─────────────────────────────────────────────────────────────────
// Add new enemy types here. These are the canonical IDs used everywhere —
// room builders, dungeon builders, event scripts. Never reference enemies
// by name string in game logic.

// Sentinel — pass as enemy ID when a slot is unused
#define ENEMY_NONE (-1)

typedef enum {
    // Tier 1 — Floor 1
    ENEMY_SHADOW_T1 = 0,
    ENEMY_FLAME_DANCER,
    ENEMY_ICE_WRAITH,

    // Tier 2 — Floor 2
    ENEMY_SHADOW_T2,
    ENEMY_DARK_SPIRIT_T1,
    ENEMY_STORM_HOUND,
    ENEMY_CURSED_ARMOR,

    // Tier 3 — Floor 3
    ENEMY_SHADOW_T3,
    ENEMY_DARK_SPIRIT_T2,

    // Dungeon 2 enemies — Forest of Lust
    ENEMY_ALLURING_WISP,    // Curse/Bless, weak to Fire
    ENEMY_THORNED_SHADE,    // Physical/Wind, weak to Ice
    ENEMY_TWISTED_DRYAD,    // Wind/Curse, weak to Electric

    ENEMY_COUNT
} EnemyID;

// ── Registry ──────────────────────────────────────────────────────────────────

#define ENEMY_REGISTRY_MAX  64

typedef struct {
    EnemyTemplate templates[ENEMY_REGISTRY_MAX];
    int           count;
} EnemyRegistry;

// Init — populates all templates. Call once at startup.
void enemy_registry_init(EnemyRegistry* reg);

// Get a template by ID — returns a copy for modification safety
EnemyTemplate enemy_registry_get(EnemyRegistry* reg, EnemyID id);

// Get a pointer — use when you need direct access
EnemyTemplate* enemy_registry_get_ptr(EnemyRegistry* reg, EnemyID id);

#endif