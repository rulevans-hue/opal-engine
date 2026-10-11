#ifndef SAVE_H
#define SAVE_H

#include "party.h"
#include "inventory.h"
#include "gamestate.h"
#include "time_system.h"
#include "social_stats.h"
#include "bond.h"
#include "dungeon.h"

#define SAVE_SLOTS    16
#define SAVE_MAGIC    0x50524547
#define SAVE_VERSION  8          // bumped — dungeon_room_visited

// Max chest slots we serialise (matches MAX_CHEST_SLOTS in dungeon.h)
#define SAVE_MAX_CHESTS 16

typedef struct {
    // ── Overworld ─────────────────────────────────────────────────────────────
    float         player_x;
    float         player_y;
    int           current_room;
    char          location_name[64];   // for save slot display

    // ── Party ─────────────────────────────────────────────────────────────────
    int           party_count;
    StatBlock     members[PARTY_MAX];

    // ── Inventory ─────────────────────────────────────────────────────────────
    int           slot_count;
    InventorySlot slots[INVENTORY_MAX_SLOTS];
    int           yen;

    // ── Game state ────────────────────────────────────────────────────────────
    int           flag_count;
    Flag          flags[MAX_FLAGS];
    int           var_count;
    GameVar       vars[MAX_VARS];
    int           knowledge_count;
    EnemyKnowledge knowledge[MAX_ENEMY_TYPES];

    // ── Time ──────────────────────────────────────────────────────────────────
    int           day;
    int           day_of_week;
    int           slot;
    int           weather;
    int           month;
    int           date;

    // ── Social stats ──────────────────────────────────────────────────────────
    int           social_exp[SOCIAL_COUNT];
    int           social_rank[SOCIAL_COUNT];

    // ── Bonds ────────────────────────────────────────────────────────────
    int           bond_rank[MAX_BONDS];
    int           bond_points[MAX_BONDS];

    // ── Summon stock ─────────────────────────────────────────────────────────
    int           summon_count;
    int           summon_active_idx;
    char          summon_names[12][64];
    int           summon_levels[12];
    int           summon_arcana[12];
    int           summon_skill_ids[12][8];
    int           summon_skill_counts[12];
    // Compendium
    int           comp_count;
    char          comp_names[64][64];
    int           comp_levels[64];
    int           comp_arcana[64];
    int           comp_skill_ids[64][8];
    int           comp_skill_counts[64];

    // ── Dungeon / Dungeon ──────────────────────────────────────────────────────
    int           dungeon_active;
    int           dungeon_idx;        // which dungeon was active
    int           dungeon_current_floor;
    int           dungeon_current_room;   // room on current floor
    int           dungeon_alert_level;
    int           dungeon_boss_defeated;
    // Per-floor per-room chest state (floor 0-4, room 0-15)
    int           dungeon_chest_opened[5][SAVE_MAX_CHESTS];
    // Per-floor per-room cleared state
    int           dungeon_room_cleared[5][SAVE_MAX_CHESTS];
    int           dungeon_room_visited[5][SAVE_MAX_CHESTS];

    // ── Meta ──────────────────────────────────────────────────────────────────
    int           used;
    char          timestamp[32];
} SaveData;

int  save_game(int slot, SaveData* data);
int  load_game(int slot, SaveData* data);
int  delete_save(int slot);
int  save_exists(int slot);
int save_get_version(int slot);


void save_data_from_state(SaveData* sd, Party* party, Inventory* inv,
                          GameState* gs, TimeSystem* ts, SocialStats* ss,
                          BondDB* cdb,
                          float player_x, float player_y,
                          int current_room,
                          DungeonSystem* dungeon_sys);

void save_data_to_state(SaveData* sd, Party* party, Inventory* inv,
                        GameState* gs, TimeSystem* ts, SocialStats* ss,
                        BondDB* cdb,
                        float* player_x, float* player_y,
                        int* current_room,
                        DungeonSystem* dungeon_sys);

void save_build_timestamp(SaveData* sd, TimeSystem* ts);
void save_set_directory(const char* dir);

#endif