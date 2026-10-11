#include "summon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "summon_loader.h"

// Global summon state — defined here, used via extern in combat.c, save.c
SummonStock      g_summon_stock;
SummonDB         g_summon_db;
SummonCompendium g_summon_comp;

// ── Arcana names ──────────────────────────────────────────────────────────────

const char* arcana_name(Arcana a) {
    switch (a) {
        case ARCANA_FOOL:       return "Fool";
        case ARCANA_MAGICIAN:   return "Magician";
        case ARCANA_PRIESTESS:  return "Priestess";
        case ARCANA_EMPRESS:    return "Empress";
        case ARCANA_EMPEROR:    return "Emperor";
        case ARCANA_HIEROPHANT: return "Hierophant";
        case ARCANA_LOVERS:     return "Lovers";
        case ARCANA_CHARIOT:    return "Chariot";
        case ARCANA_JUSTICE:    return "Justice";
        case ARCANA_HERMIT:     return "Hermit";
        case ARCANA_WHEEL:      return "Wheel of Fortune";
        case ARCANA_STRENGTH:   return "Strength";
        case ARCANA_HANGEDMAN:  return "Hanged Man";
        case ARCANA_DEATH:      return "Death";
        case ARCANA_TEMPERANCE: return "Temperance";
        case ARCANA_DEVIL:      return "Devil";
        case ARCANA_TOWER:      return "Tower";
        case ARCANA_STAR:       return "Star";
        case ARCANA_MOON:       return "Moon";
        case ARCANA_SUN:        return "Sun";
        case ARCANA_JUDGEMENT:  return "Judgement";
        default:                return "Unknown";
    }
}

// ── SummonDB ─────────────────────────────────────────────────────────────────
// All Summons defined here. To add a new Summon:
//   1. Add it in summondb_init below
//   2. That's it — no other files need changing

static Summon make_summon(const char* name, Arcana arcana, int base_level,
                             int ma_mod, int ag_mod) {
    Summon p;
    memset(&p, 0, sizeof(Summon));
    strncpy(p.name, name, 63);
    p.arcana     = arcana;
    p.base_level = base_level;
    p.level      = base_level;
    p.exp        = 0;
    p.ma_mod     = ma_mod;
    p.ag_mod     = ag_mod;
    p.valid      = 1;
    // Default all affinities to normal
    for (int i = 0; i < ELEMENT_COUNT; i++)
        p.affinities[i] = AFF_NORMAL;
    return p;
}

void summondb_init(SummonDB* db) {
    memset(db, 0, sizeof(SummonDB));
     // Try data-driven load first; fall back to hardcoded below if it fails.
    if (summon_load_json(db, "data/summons.json"))
        return;
    Summon* p;

    // ── Opaline (Fool) — Prota's starting Summon ──────────────────────────────
    p = &db->entries[db->count++];
    *p = make_summon("Opaline", ARCANA_FOOL, 1, 2, 2);
    p->affinities[ELEM_BLESS] = AFF_WEAK;
    p->affinities[ELEM_CURSE] = AFF_RESIST;
    p->skill_ids[0] = 0;    // Attack
    p->skill_ids[1] = 5;    // Ember
    p->skill_ids[2] = 26;   // Hex
    p->skill_count  = 3;
    p->learn_levels[0] = 1;
    p->learn_levels[1] = 1;
    p->learn_levels[2] = 3;

    // ── Pixie (Lovers) ────────────────────────────────────────────────────────
    p = &db->entries[db->count++];
    *p = make_summon("Pixie", ARCANA_LOVERS, 2, 4, 3);
    p->affinities[ELEM_ELECTRIC] = AFF_WEAK;
    p->affinities[ELEM_WIND]     = AFF_RESIST;
    p->skill_ids[0] = 0;    // Attack
    p->skill_ids[1] = 29;   // Mend
    p->skill_ids[2] = 13;   // Spark
    p->skill_count  = 3;
    p->learn_levels[0] = 1;
    p->learn_levels[1] = 1;
    p->learn_levels[2] = 4;

    // ── Apsaras (Priestess) ───────────────────────────────────────────────────
    p = &db->entries[db->count++];
    *p = make_summon("Apsaras", ARCANA_PRIESTESS, 4, 5, 2);
    p->affinities[ELEM_ICE]      = AFF_RESIST;
    p->affinities[ELEM_FIRE]     = AFF_WEAK;
    p->skill_ids[0] = 0;    // Attack
    p->skill_ids[1] = 9;    // Frost
    p->skill_ids[2] = 29;   // Mend
    p->skill_ids[3] = 12;   // Frost Wave
    p->skill_count  = 4;
    p->learn_levels[0] = 1;
    p->learn_levels[1] = 1;
    p->learn_levels[2] = 2;
    p->learn_levels[3] = 6;

    // ── Agathion (Chariot) ────────────────────────────────────────────────────
    p = &db->entries[db->count++];
    *p = make_summon("Agathion", ARCANA_CHARIOT, 3, 1, 5);
    p->affinities[ELEM_WIND]     = AFF_WEAK;
    p->affinities[ELEM_ELECTRIC] = AFF_RESIST;
    p->skill_ids[0] = 0;    // Attack
    p->skill_ids[1] = 1;    // Cleave
    p->skill_ids[2] = 13;   // Spark
    p->skill_count  = 3;
    p->learn_levels[0] = 1;
    p->learn_levels[1] = 2;
    p->learn_levels[2] = 5;

    // ── Mandrake (Death) ──────────────────────────────────────────────────────
    p = &db->entries[db->count++];
    *p = make_summon("Mandrake", ARCANA_DEATH, 6, 3, 2);
    p->affinities[ELEM_CURSE]    = AFF_NULL;
    p->affinities[ELEM_BLESS]    = AFF_WEAK;
    p->skill_ids[0] = 0;    // Attack
    p->skill_ids[1] = 26;   // Hex
    p->skill_ids[2] = 27;   // Eigaon
    p->skill_ids[3] = 19;   // Psi
    p->skill_count  = 4;
    p->learn_levels[0] = 1;
    p->learn_levels[1] = 1;
    p->learn_levels[2] = 7;
    p->learn_levels[3] = 9;

    // ── Berith (Tower) ────────────────────────────────────────────────────────
    p = &db->entries[db->count++];
    *p = make_summon("Berith", ARCANA_TOWER, 8, 4, 3);
    p->affinities[ELEM_FIRE]     = AFF_RESIST;
    p->affinities[ELEM_ICE]      = AFF_WEAK;
    p->skill_ids[0] = 0;    // Attack
    p->skill_ids[1] = 5;    // Ember
    p->skill_ids[2] = 6;    // Blaze
    p->skill_ids[3] = 8;    // Ember Wave
    p->skill_count  = 4;
    p->learn_levels[0] = 1;
    p->learn_levels[1] = 1;
    p->learn_levels[2] = 9;
    p->learn_levels[3] = 11;

    // ── Eligor (Emperor) ─────────────────────────────────────────────────────
    p = &db->entries[db->count++];
    *p = make_summon("Eligor", ARCANA_EMPEROR, 12, 3, 4);
    p->affinities[ELEM_WIND]     = AFF_NULL;
    p->affinities[ELEM_ELECTRIC] = AFF_RESIST;
    p->affinities[ELEM_CURSE]    = AFF_WEAK;
    p->skill_ids[0] = 0;    // Attack
    p->skill_ids[1] = 1;    // Cleave
    p->skill_ids[2] = 16;   // Gust
    p->skill_ids[3] = 18;   // Gust Wave
    p->skill_count  = 4;
    p->learn_levels[0] = 1;
    p->learn_levels[1] = 1;
    p->learn_levels[2] = 13;
    p->learn_levels[3] = 15;

    // ── Succubus (Devil) ─────────────────────────────────────────────────────
    p = &db->entries[db->count++];
    *p = make_summon("Succubus", ARCANA_DEVIL, 15, 6, 4);
    p->affinities[ELEM_CURSE]    = AFF_RESIST;
    p->affinities[ELEM_BLESS]    = AFF_WEAK;
    p->affinities[ELEM_FIRE]     = AFF_RESIST;
    p->skill_ids[0] = 0;    // Attack
    p->skill_ids[1] = 26;   // Hex
    p->skill_ids[2] = 27;   // Eigaon
    p->skill_ids[3] = 19;   // Psi
    p->skill_ids[4] = 28;   // Mapsi
    p->skill_count  = 5;
    p->learn_levels[0] = 1;
    p->learn_levels[1] = 1;
    p->learn_levels[2] = 16;
    p->learn_levels[3] = 18;
    p->learn_levels[4] = 20;
}

// ── Stock init ────────────────────────────────────────────────────────────────

void summon_stock_init(SummonStock* stock, SummonDB* db) {
    memset(stock, 0, sizeof(SummonStock));
    stock->active_idx = 0;

    // Prota starts with Opaline (first entry in DB)
    if (db->count > 0) {
        stock->stock[0] = db->entries[0];  // Opaline
        stock->count    = 1;
    }
}

// ── Accessors ─────────────────────────────────────────────────────────────────

Summon* summon_active(SummonStock* stock) {
    if (stock->count == 0) return NULL;
    if (stock->active_idx >= stock->count) stock->active_idx = 0;
    if (!stock->stock[stock->active_idx].valid) return NULL;
    return &stock->stock[stock->active_idx];
}

void summon_switch(SummonStock* stock, int idx) {
    if (idx < 0 || idx >= stock->count) return;
    if (!stock->stock[idx].valid) return;
    stock->active_idx = idx;
    printf("Active Summon: %s\n", stock->stock[idx].name);
}

// ── Apply to StatBlock ────────────────────────────────────────────────────────
// Called before combat to set Prota's affinities and skills from his Summon.

void summon_apply_to_statblock(SummonStock* stock, StatBlock* prota) {
    Summon* p = summon_active(stock);
    if (!p) return;

    // Apply Summon's affinities to Prota
    for (int i = 0; i < ELEMENT_COUNT; i++)
        prota->affinities[i] = p->affinities[i];

    // Apply Summon's skills to Prota
    prota->skill_count = 0;
    for (int i = 0; i < p->skill_count && i < MAX_SKILLS_PER_COMBATANT; i++) {
        // Only include skills that Prota has reached the learn level for
        if (p->level >= p->learn_levels[i]) {
            prota->skill_ids[prota->skill_count++] = p->skill_ids[i];
        }
    }
}

// ── Stock management ──────────────────────────────────────────────────────────

int summon_add(SummonStock* stock, Summon* p) {
    if (stock->count >= MAX_PROTA_SUMMONS) return 0;
    stock->stock[stock->count++] = *p;
    return 1;
}

void summon_remove(SummonStock* stock, int idx) {
    if (idx < 0 || idx >= stock->count) return;
    // Shift remaining Summons down
    for (int i = idx; i < stock->count - 1; i++)
        stock->stock[i] = stock->stock[i + 1];
    stock->count--;
    // Keep active_idx valid
    if (stock->active_idx >= stock->count)
        stock->active_idx = stock->count > 0 ? stock->count - 1 : 0;
}

// ── Fusion ────────────────────────────────────────────────────────────────────
// Simplified fusion: higher-level Summon of adjacent arcana.
// For now: takes the higher-level parent's arcana + 1, averages stats.

int summon_fuse(SummonStock* stock, int idx_a, int idx_b,
                 SummonDB* db, Summon* result_out) {
    if (idx_a < 0 || idx_a >= stock->count) return 0;
    if (idx_b < 0 || idx_b >= stock->count) return 0;
    if (idx_a == idx_b) return 0;

    Summon* a = &stock->stock[idx_a];
    Summon* b = &stock->stock[idx_b];

    // Result level = average of parents + 1
    int result_level = (a->level + b->level) / 2 + 1;

    // Find the best matching Summon in DB at that level
    // (same or next arcana from the higher-level parent)
    Summon* higher = (a->level >= b->level) ? a : b;
    int target_arcana = (higher->arcana + 1) % ARCANA_COUNT;

    // Find closest Summon in DB at result_level with target_arcana
    Summon* best = NULL;
    int best_diff = 999;
    for (int i = 0; i < db->count; i++) {
        Summon* candidate = &db->entries[i];
        if (candidate->arcana == target_arcana) {
            int diff = abs(candidate->base_level - result_level);
            if (diff < best_diff) {
                best_diff = diff;
                best      = candidate;
            }
        }
    }

    // If no arcana match, just find any Summon near that level
    if (!best) {
        for (int i = 0; i < db->count; i++) {
            Summon* candidate = &db->entries[i];
            int diff = abs(candidate->base_level - result_level);
            if (diff < best_diff) {
                best_diff = diff;
                best      = candidate;
            }
        }
    }

    if (!best) return 0;

    // Inherit up to 2 skills from each parent
    *result_out = *best;
    result_out->level = result_level;
    result_out->exp   = 0;

    // Skill inheritance — add unique skills from parents that the result
    // doesn't already have
    for (int parent = 0; parent < 2; parent++) {
        Summon* src = (parent == 0) ? a : b;
        int inherited = 0;
        for (int si = 0; si < src->skill_count && inherited < 2; si++) {
            int skill = src->skill_ids[si];
            // Check if result already has this skill
            int has = 0;
            for (int ri = 0; ri < result_out->skill_count; ri++)
                if (result_out->skill_ids[ri] == skill) { has = 1; break; }
            if (!has && result_out->skill_count < MAX_SUMMON_SKILLS) {
                result_out->skill_ids[result_out->skill_count] = skill;
                result_out->learn_levels[result_out->skill_count] = 1;
                result_out->skill_count++;
                inherited++;
            }
        }
    }

    printf("Fusion: %s + %s = %s (Lv.%d)\n",
           a->name, b->name, result_out->name, result_out->level);
    return 1;
}

// ── Compendium ────────────────────────────────────────────────────────────────

int summon_resummon_cost(Summon* p) {
    // Cost scales with level: level * 50 yen minimum 100
    int cost = p->level * 50;
    return cost < 100 ? 100 : cost;
}

void summon_register(SummonCompendium* comp, Summon* p) {
    if (!p || !p->valid) return;
    // Check if already registered — update if new one is higher level
    for (int i = 0; i < comp->count; i++) {
        if (strcmp(comp->entries[i].name, p->name) == 0) {
            if (p->level > comp->entries[i].level)
                comp->entries[i] = *p;  // update to higher-level snapshot
            return;
        }
    }
    // New entry
    if (comp->count < MAX_COMPENDIUM)
        comp->entries[comp->count++] = *p;
    printf("Compendium: registered %s (Lv.%d)\n", p->name, p->level);
}

void summon_reregister(SummonCompendium* comp, SummonStock* stock) {
    Summon* p = summon_active(stock);
    if (!p) return;
    // Force update — always overwrite with current snapshot
    for (int i = 0; i < comp->count; i++) {
        if (strcmp(comp->entries[i].name, p->name) == 0) {
            comp->entries[i] = *p;
            printf("Compendium: updated %s to Lv.%d\n", p->name, p->level);
            return;
        }
    }
    summon_register(comp, p);
}

int summon_in_compendium(SummonCompendium* comp, const char* name) {
    for (int i = 0; i < comp->count; i++)
        if (strcmp(comp->entries[i].name, name) == 0) return 1;
    return 0;
}

int summon_resummon(SummonCompendium* comp, int idx,
                     SummonStock* stock, int* yen) {
    if (idx < 0 || idx >= comp->count) return 0;
    if (stock->count >= MAX_PROTA_SUMMONS) return 0;

    Summon* p = &comp->entries[idx];
    int cost = summon_resummon_cost(p);
    if (*yen < cost) return 0;

    // Check not already in stock
    for (int i = 0; i < stock->count; i++)
        if (strcmp(stock->stock[i].name, p->name) == 0) return 0;

    *yen -= cost;
    summon_add(stock, p);
    printf("Resummon: %s (cost %d yen)\n", p->name, cost);
    return 1;
}

// ── EXP ───────────────────────────────────────────────────────────────────────

void summon_add_exp(SummonStock* stock, int exp) {
    Summon* p = summon_active(stock);
    if (!p) return;
    p->exp += exp;
    // Level up at 100 EXP per level (simple curve)
    int threshold = p->level * 100;
    while (p->exp >= threshold) {
        p->exp -= threshold;
        p->level++;
        threshold = p->level * 100;
        printf("Summon level up: %s is now Lv.%d\n", p->name, p->level);
    }
}