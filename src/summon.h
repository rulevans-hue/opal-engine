#ifndef SUMMON_H
#define SUMMON_H

// ── Summon System ────────────────────────────────────────────────────────────
// Prota can hold multiple Summons and switch between them.
// Each Summon has its own skills, affinities, and stat modifiers.
// Prota's HP/SP are his own; everything else comes from the active Summon.
//
// Key rules:
// - Prota can hold up to MAX_PROTA_SUMMONS at once
// - Active Summon supplies: affinities, skills, MA bonus
// - Prota's base stats supply: HP, SP, ST, EN, AG, LU
// - Switching Summons is free (once per turn in combat, any time outside)
// - Fusing two Summons creates a new one; both parents are consumed
// - The compendium records every Summon ever held; can resummon for yen

#include "stats.h"

#define MAX_PROTA_SUMMONS   12    // max Summons in Prota's stock
#define MAX_SUMMON_SKILLS    8    // max skills a Summon can have
#define MAX_SUMMON_IN_DB   64    // total Summons defined in the game
#define MAX_COMPENDIUM      64    // Summons registered in compendium

// ── Arcana ────────────────────────────────────────────────────────────────────

typedef enum {
    ARCANA_FOOL = 0,
    ARCANA_MAGICIAN,
    ARCANA_PRIESTESS,
    ARCANA_EMPRESS,
    ARCANA_EMPEROR,
    ARCANA_HIEROPHANT,
    ARCANA_LOVERS,
    ARCANA_CHARIOT,
    ARCANA_JUSTICE,
    ARCANA_HERMIT,
    ARCANA_WHEEL,
    ARCANA_STRENGTH,
    ARCANA_HANGEDMAN,
    ARCANA_DEATH,
    ARCANA_TEMPERANCE,
    ARCANA_DEVIL,
    ARCANA_TOWER,
    ARCANA_STAR,
    ARCANA_MOON,
    ARCANA_SUN,
    ARCANA_JUDGEMENT,
    ARCANA_COUNT
} Arcana;

// ── Summon definition ────────────────────────────────────────────────────────

typedef struct {
    char     name[64];
    Arcana   arcana;
    int      base_level;       // minimum level to use this Summon
    int      level;            // current level (grows with Prota)
    int      exp;              // EXP toward next level

    // Stat modifiers applied on top of Prota's base stats
    int      ma_mod;           // magic modifier
    int      ag_mod;           // agility modifier

    // Affinities — override Prota's base affinities in combat
    Affinity affinities[ELEMENT_COUNT];

    // Skills
    int      skill_ids[MAX_SUMMON_SKILLS];
    int      skill_count;

    // Skill learn thresholds — skill learned at this level
    int      learn_levels[MAX_SUMMON_SKILLS];

    int      valid;            // 1 = slot occupied
} Summon;

// ── Summon DB ────────────────────────────────────────────────────────────────
// All Summons that exist in the game. Defined in summon_db.c.

typedef struct {
    Summon  entries[MAX_SUMMON_IN_DB];
    int      count;
} SummonDB;

// ── Summon Compendium ────────────────────────────────────────────────────────
// Records every Summon Prota has ever held. Can resummon for yen.
// Stores a full snapshot so leveled-up Summons keep their stats.
// Cost to resummon = base_level * 50 yen.

typedef struct {
    Summon entries[MAX_COMPENDIUM];  // full snapshots
    int     count;
} SummonCompendium;

// Cost to resummon a Summon from compendium
int summon_resummon_cost(Summon* p);

// ── Prota's Summon stock ─────────────────────────────────────────────────────

typedef struct {
    Summon  stock[MAX_PROTA_SUMMONS];
    int      count;           // how many Summons held
    int      active_idx;      // which one is currently active
} SummonStock;

// ── Public API ────────────────────────────────────────────────────────────────

// Init the global Summon DB
void summondb_init(SummonDB* db);

// Init Prota's starting stock
void summon_stock_init(SummonStock* stock, SummonDB* db);

// Get Prota's active Summon (NULL if stock empty)
Summon* summon_active(SummonStock* stock);

// Switch active Summon by index
void summon_switch(SummonStock* stock, int idx);

// Apply active Summon to Prota's StatBlock in combat
// Call before combat_start — updates Prota's affinities and skills
void summon_apply_to_statblock(SummonStock* stock, StatBlock* prota);

// Add a Summon to stock (returns 0 if full)
int summon_add(SummonStock* stock, Summon* p);

// Remove a Summon from stock by index
void summon_remove(SummonStock* stock, int idx);

// Fuse two Summons in stock (by index) — produces result, consumes both
// Returns 1 on success, 0 on failure
int summon_fuse(SummonStock* stock, int idx_a, int idx_b,
                 SummonDB* db, Summon* result_out);

// Register a Summon in the compendium (auto-called on obtain/fuse)
// If already registered, updates the snapshot if the new one is higher level
void summon_register(SummonCompendium* comp, Summon* p);

// Manually re-register active Summon (saves current leveled stats)
void summon_reregister(SummonCompendium* comp, SummonStock* stock);

// Check if a Summon name is in the compendium
int summon_in_compendium(SummonCompendium* comp, const char* name);

// Resummon from compendium by index — costs yen, adds to stock
// Returns 1 on success, 0 on failure (stock full / not enough yen)
int summon_resummon(SummonCompendium* comp, int idx,
                     SummonStock* stock, int* yen);

// Add EXP to active Summon
void summon_add_exp(SummonStock* stock, int exp);

// Arcana name
const char* arcana_name(Arcana a);

#endif