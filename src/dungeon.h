#ifndef DUNGEON_H
#define DUNGEON_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "map.h"
#include "entity.h"
#include "combat.h"
#include "party.h"
#include "inventory.h"
#include "gamestate.h"
#include "time_system.h"
#include "notify.h"
#include "dialogue.h"
#include "skills.h"
#include "equipment.h"

#define MAX_DUNGEON_FLOORS    5
#define MAX_DUNGEON_ROOMS     16
#define MAX_CHEST_SLOTS       16
#define MAX_DUNGEONS           8

// ── Room types ────────────────────────────────────────────────────────────────

typedef enum {
    ROOM_NORMAL,
    ROOM_SAFE,
    ROOM_CHEST,
    ROOM_BOSS
} DungeonRoomType;

typedef enum {
    EXIT_NORTH = 0,
    EXIT_SOUTH = 1,
    EXIT_EAST  = 2,
    EXIT_WEST  = 3
} ExitDir;

// ── Treasure chest ────────────────────────────────────────────────────────────

typedef struct {
    int item_id;
    int item_qty;
    int equip_id;
    int opened;
    int is_dungeon_treasure;
    char sets_flag[64];   // optional: flag set when opened ("" = none)
} TreasureChest;

// ── Boss configuration ────────────────────────────────────────────────────────

typedef struct {
    CombatEnemy enemy;
    int phase2_hp_pct;
    int phase2_skill_ids[4];
    int phase2_skill_count;
    int phase2_immune_elem;
    int pre_battle_node;
    int post_battle_node;
    int in_phase2;
    int defeated;
    // Dungeon treasure given on defeat
    int treasure_equip_id;  // -1 = none
    int treasure_item_id;   // -1 = none
    int treasure_item_qty;
} BossConfig;

// ── Dungeon room ──────────────────────────────────────────────────────────────

typedef struct {
    int              id;
    DungeonRoomType  type;
    char             name[64];
    char             description[128];
    Map              map;
    EntityList       entities;
    TreasureChest    chest;
    BossConfig       boss;
    int              exits[4];
    int              visited;
    int              cleared;
} DungeonRoom;

// ── Dungeon floor ─────────────────────────────────────────────────────────────

typedef struct {
    int         floor_number;
    char        name[64];
    DungeonRoom rooms[MAX_DUNGEON_ROOMS];
    int         room_count;
    int         current_room;
    int         stairs_room_id;
    int         safe_room_ids[4];
    int         safe_room_count;
} DungeonFloor;

// ── Dungeon ────────────────────────────────────────────────────────────────────

typedef struct {
    char         name[64];
    char         ruler[64];
    DungeonFloor floors[MAX_DUNGEON_FLOORS];
    int          floor_count;
    int          current_floor;
    int          deadline_day;
    char         clear_flag[64];
    char         entry_flag[64];
    int          alert_level;
    int          active;
    float        player_x;
    float        player_y;
    // Per-dungeon team meeting (triggered on first final floor visit)
    int          team_meeting_node;  // -1 = none
    char         team_meeting_flag[64];
} Dungeon;

// ── Dungeon system ────────────────────────────────────────────────────────────

#include "dungeon_db.h"

typedef struct {
    // Only the currently active dungeon is kept in memory.
    // Built on demand when entered, reset when a save is loaded.
    Dungeon       dungeon;              // the active dungeon
    int          current_dungeon_idx;  // index into dungeon_db entries
    int          dungeon_count;        // total registered dungeons

    // Reference to dungeon registry (not owned)
    DungeonDB* dungeon_db;

    // Combat state
    int          in_combat;
    int          combat_is_boss;
    int           alert_decay_counter; 
    int           boss_engaged_this_visit;  // one-shot guard for boss combat trigger
    int          preemptive;
    int          ambush;

    // UI state
    int          fast_travel_open;
    int          fast_travel_selected;
    int          safe_menu_open;
    int          safe_menu_selected;
    int          chest_open;
    int          transition_cooldown;

    // System references (not owned)
    Party*          party;
    Inventory*      inventory;
    GameState*      gs;
    TimeSystem*     ts;
    NotifySystem*   notify;
    SkillDB*        skilldb;
    ItemDB*         itemdb;
    EquipDB*        equipdb;
    DialogueSystem* ds;
    CombatSystem*   cs;
} DungeonSystem;

// ── Public API ────────────────────────────────────────────────────────────────

// Handcrafted-room pipeline: public wrappers for Godot bridge.
int  dungeon_use_exit_dir(DungeonSystem* ds_sys, int dir,
                          float* player_x, float* player_y);
int  dungeon_trigger_encounter(DungeonSystem* ds_sys, int entity_index,
                               float player_x, float player_y);

// Map initialiser — exposed for dungeon_builder.c
void dungeon_map_init(Map* m);

// Init — now takes a DungeonDB pointer
void dungeon_init(DungeonSystem* ds_sys,
                  Party* party, Inventory* inv, GameState* gs,
                  TimeSystem* ts, NotifySystem* notify,
                  SkillDB* skilldb, ItemDB* itemdb, EquipDB* equipdb,
                  DialogueSystem* ds, CombatSystem* cs,
                  DungeonDB* dungeon_db);

int dungeon_check_edge_transition(DungeonSystem* ds_sys,
                                  float* player_x, float* player_y,
                                  int player_w, int player_h);

// Reset current dungeon to initial state (before restoring a save)
void dungeon_reset_active(DungeonSystem* ds_sys);

// Enter / exit
int  dungeon_can_enter(DungeonSystem* ds_sys);
void dungeon_enter(DungeonSystem* ds_sys, float* player_x, float* player_y);
void dungeon_exit(DungeonSystem* ds_sys, float* player_x, float* player_y);

// Game loop
void dungeon_handle_input(DungeonSystem* ds_sys, SDL_Event* event,
                          float* player_x, float* player_y);
void dungeon_update(DungeonSystem* ds_sys, float player_x, float player_y);
void dungeon_render(DungeonSystem* ds_sys, SDL_Renderer* renderer,
                    TTF_Font* font, float player_x, float player_y,
                    Camera* cam);

// Post-combat callback
void dungeon_on_combat_end(DungeonSystem* ds_sys, int player_won);

// Alert
void dungeon_raise_alert(DungeonSystem* ds_sys, int amount);
void dungeon_reset_alert(DungeonSystem* ds_sys);

// Accessors — always return data for the active dungeon
Dungeon*       dungeon_get_active(DungeonSystem* ds_sys);
DungeonFloor* dungeon_current_floor(DungeonSystem* ds_sys);
DungeonRoom*  dungeon_current_room(DungeonSystem* ds_sys);

// Spawn helper
void dungeon_find_spawn(Map* m, float target_x, float target_y,
                        float* out_x, float* out_y,
                        int player_w, int player_h);

// Movement
#define DUNGEON_SPEED 4
void dungeon_move(DungeonSystem* ds_sys, const Uint8* keys,
                  float* player_x, float* player_y,
                  int player_w, int player_h);

#endif