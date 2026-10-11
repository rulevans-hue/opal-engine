#include "enemy_registry.h"
#include "enemy_loader.h"

// Global enemy registry — defined here, extern in dungeon.c
EnemyRegistry g_enemy_registry;
#include <string.h>

// ── Internal helper ───────────────────────────────────────────────────────────

static EnemyTemplate make(const char* name,
                           int hp, int sp,
                           int st, int ma, int en, int ag, int lu,
                           int exp) {
    EnemyTemplate t = make_enemy_template(name, hp, sp,
                                           st, ma, en, ag, lu, exp);
    return t;
}

// ── Registry init ─────────────────────────────────────────────────────────────
// All enemies defined once here. To add a new enemy:
//   1. Add its ID to EnemyID enum in enemy_registry.h
//   2. Define it below
//   3. Reference it by ID in dungeon_builder or room builders

void enemy_registry_init(EnemyRegistry* reg) {
    memset(reg, 0, sizeof(EnemyRegistry));

    // Try data-driven load first; fall back to hardcoded below if it fails.
    if (enemy_load_json(reg, "data/enemies.json"))
        return;

    EnemyTemplate* t;
    // ── ENEMY_SHADOW_T1 ───  (everything below stays as the fallback) ──
    t = &reg->templates[ENEMY_SHADOW_T1];
    *t = make("Shadow", 80, 15, 10, 8, 8, 7, 7, 20);
    enemy_template_set_affinity(t, ELEM_FIRE,  AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_BLESS, AFF_NULL);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 13;  // Spark
    t->skill_ids[2] = 16;  // Gust
    t->skill_count  = 3;
    enemy_template_add_loot(t,  0, 40);  // Medicine
    enemy_template_add_loot(t, 10, 20);  // Snuff Soul
    enemy_template_add_loot(t, 23, 15);  // Flame Urn

    // ── ENEMY_FLAME_DANCER ────────────────────────────────────────────────────
    t = &reg->templates[ENEMY_FLAME_DANCER];
    *t = make("Flame Dancer", 130, 40, 13, 18, 9, 12, 9, 55);
    enemy_template_set_affinity(t, ELEM_ICE,  AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_FIRE, AFF_NULL);
    enemy_template_set_affinity(t, ELEM_WIND, AFF_RESIST);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 5;   // Ember
    t->skill_ids[2] = 6;   // Blaze
    t->skill_ids[3] = 8;   // Ember Wave
    t->skill_count  = 4;
    enemy_template_add_loot(t, 24, 35);  // Freeze Spray
    enemy_template_add_loot(t,  0, 25);  // Medicine
    enemy_template_add_loot(t, 23, 20);  // Flame Urn

    // ── ENEMY_ICE_WRAITH ──────────────────────────────────────────────────────
    t = &reg->templates[ENEMY_ICE_WRAITH];
    *t = make("Ice Wraith", 110, 40, 10, 15, 8, 13, 10, 50);
    enemy_template_set_affinity(t, ELEM_FIRE,     AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_ICE,      AFF_NULL);
    enemy_template_set_affinity(t, ELEM_ELECTRIC, AFF_RESIST);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 9;   // Frost
    t->skill_ids[2] = 10;  // Freeze
    t->skill_ids[3] = 12;  // Frost Wave
    t->skill_count  = 4;
    enemy_template_add_loot(t, 23, 35);  // Flame Urn
    enemy_template_add_loot(t, 10, 25);  // Snuff Soul

    // ── ENEMY_SHADOW_T2 ───────────────────────────────────────────────────────
    t = &reg->templates[ENEMY_SHADOW_T2];
    *t = make("Shadow", 120, 25, 14, 12, 10, 10, 8, 40);
    enemy_template_set_affinity(t, ELEM_FIRE,  AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_BLESS, AFF_NULL);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 13;  // Spark
    t->skill_ids[2] = 16;  // Gust
    t->skill_count  = 3;
    enemy_template_add_loot(t,  0, 40);  // Medicine
    enemy_template_add_loot(t, 10, 25);  // Snuff Soul
    enemy_template_add_loot(t, 14, 10);  // Revival Bead

    // ── ENEMY_DARK_SPIRIT_T1 ─────────────────────────────────────────────────
    t = &reg->templates[ENEMY_DARK_SPIRIT_T1];
    *t = make("Dark Spirit", 150, 40, 14, 16, 12, 10, 8, 60);
    enemy_template_set_affinity(t, ELEM_BLESS, AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_CURSE, AFF_NULL);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 26;  // Hex
    t->skill_ids[2] = 19;  // Psi
    t->skill_ids[3] = 10;  // Freeze
    t->skill_count  = 4;
    enemy_template_add_loot(t, 10, 40);  // Snuff Soul
    enemy_template_add_loot(t, 14, 20);  // Revival Bead
    enemy_template_add_loot(t,  1, 15);  // Medicine DX

    // ── ENEMY_STORM_HOUND ─────────────────────────────────────────────────────
    t = &reg->templates[ENEMY_STORM_HOUND];
    *t = make("Storm Hound", 160, 30, 18, 10, 14, 16, 8, 65);
    enemy_template_set_affinity(t, ELEM_ELECTRIC, AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_WIND,     AFF_NULL);
    enemy_template_set_affinity(t, ELEM_PHYSICAL, AFF_RESIST);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 1;   // Cleave
    t->skill_ids[2] = 16;  // Gust
    t->skill_ids[3] = 18;  // Gust Wave
    t->skill_count  = 4;
    enemy_template_add_loot(t, 25, 35);  // Thunder Bomb
    enemy_template_add_loot(t,  0, 30);  // Medicine

    // ── ENEMY_CURSED_ARMOR ────────────────────────────────────────────────────
    t = &reg->templates[ENEMY_CURSED_ARMOR];
    *t = make("Cursed Armor", 220, 20, 22, 8, 20, 6, 7, 90);
    enemy_template_set_affinity(t, ELEM_BLESS,    AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_CURSE,    AFF_ABSORB);
    enemy_template_set_affinity(t, ELEM_PHYSICAL, AFF_RESIST);
    enemy_template_set_affinity(t, ELEM_GUN,      AFF_RESIST);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 1;   // Cleave
    t->skill_ids[2] = 26;  // Hex
    t->skill_ids[3] = 27;  // Eigaon
    t->skill_count  = 4;
    enemy_template_add_loot(t, 29, 30);  // Bless Charm
    enemy_template_add_loot(t, 14, 25);  // Revival Bead
    enemy_template_add_loot(t,  1, 20);  // Medicine DX

    // ── ENEMY_SHADOW_T3 ───────────────────────────────────────────────────────
    t = &reg->templates[ENEMY_SHADOW_T3];
    *t = make("Shadow", 180, 35, 18, 16, 14, 12, 10, 70);
    enemy_template_set_affinity(t, ELEM_FIRE,  AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_BLESS, AFF_NULL);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 13;  // Spark
    t->skill_ids[2] = 16;  // Gust
    t->skill_ids[3] = 15;  // Spark Wave
    t->skill_count  = 4;
    enemy_template_add_loot(t,  0, 40);  // Medicine
    enemy_template_add_loot(t, 10, 25);  // Snuff Soul
    enemy_template_add_loot(t, 14, 10);  // Revival Bead

    // ── ENEMY_DARK_SPIRIT_T2 ─────────────────────────────────────────────────
    t = &reg->templates[ENEMY_DARK_SPIRIT_T2];
    *t = make("Dark Spirit", 200, 50, 18, 20, 14, 12, 10, 80);
    enemy_template_set_affinity(t, ELEM_BLESS, AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_CURSE, AFF_NULL);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 26;  // Hex
    t->skill_ids[2] = 27;  // Eigaon
    t->skill_ids[3] = 19;  // Psi
    t->skill_count  = 4;
    enemy_template_add_loot(t, 10, 40);  // Snuff Soul
    enemy_template_add_loot(t, 14, 25);  // Revival Bead
    enemy_template_add_loot(t,  1, 20);  // Medicine DX


    // ── ENEMY_ALLURING_WISP ───────────────────────────────────────────────────
    t = &reg->templates[ENEMY_ALLURING_WISP];
    *t = make("Alluring Wisp", 140, 60, 10, 20, 10, 14, 12, 65);
    enemy_template_set_affinity(t, ELEM_FIRE,  AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_CURSE, AFF_NULL);
    enemy_template_set_affinity(t, ELEM_BLESS, AFF_RESIST);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 26;  // Hex
    t->skill_ids[2] = 27;  // Eigaon
    t->skill_ids[3] = 19;  // Psi
    t->skill_count  = 4;
    enemy_template_add_loot(t, 10, 35);  // Snuff Soul
    enemy_template_add_loot(t,  1, 20);  // Medicine DX
    enemy_template_add_loot(t, 29, 15);  // Bless Charm

    // ── ENEMY_THORNED_SHADE ───────────────────────────────────────────────────
    t = &reg->templates[ENEMY_THORNED_SHADE];
    *t = make("Thorned Shade", 200, 20, 22, 10, 18, 10, 8, 75);
    enemy_template_set_affinity(t, ELEM_ICE,      AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_PHYSICAL, AFF_RESIST);
    enemy_template_set_affinity(t, ELEM_WIND,     AFF_RESIST);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 1;   // Cleave
    t->skill_ids[2] = 2;   // Rampage
    t->skill_ids[3] = 16;  // Gust
    t->skill_count  = 4;
    enemy_template_add_loot(t,  0, 40);  // Medicine
    enemy_template_add_loot(t, 24, 25);  // Freeze Spray
    enemy_template_add_loot(t, 14, 10);  // Revival Bead

    // ── ENEMY_TWISTED_DRYAD ───────────────────────────────────────────────────
    t = &reg->templates[ENEMY_TWISTED_DRYAD];
    *t = make("Twisted Dryad", 160, 50, 14, 18, 12, 16, 10, 80);
    enemy_template_set_affinity(t, ELEM_ELECTRIC, AFF_WEAK);
    enemy_template_set_affinity(t, ELEM_WIND,     AFF_NULL);
    enemy_template_set_affinity(t, ELEM_CURSE,    AFF_RESIST);
    enemy_template_set_affinity(t, ELEM_ICE,      AFF_RESIST);
    t->skill_ids[0] = 0;   // Attack
    t->skill_ids[1] = 16;  // Gust
    t->skill_ids[2] = 18;  // Gust Wave
    t->skill_ids[3] = 26;  // Hex
    t->skill_count  = 4;
    enemy_template_add_loot(t, 10, 35);  // Snuff Soul
    enemy_template_add_loot(t, 25, 25);  // Thunder Bomb
    enemy_template_add_loot(t,  1, 15);  // Medicine DX

    reg->count = ENEMY_COUNT;
}

EnemyTemplate enemy_registry_get(EnemyRegistry* reg, EnemyID id) {
    if (id < 0 || id >= ENEMY_COUNT) {
        EnemyTemplate blank;
        memset(&blank, 0, sizeof(EnemyTemplate));
        return blank;
    }
    return reg->templates[id];
}

EnemyTemplate* enemy_registry_get_ptr(EnemyRegistry* reg, EnemyID id) {
    if (id < 0 || id >= ENEMY_COUNT) return NULL;
    return &reg->templates[id];
}