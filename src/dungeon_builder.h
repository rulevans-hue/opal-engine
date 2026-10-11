#ifndef DUNGEON_BUILDER_H
#define DUNGEON_BUILDER_H

// ── Dungeon Builder API ────────────────────────────────────────────────────────
// A clean, high-level API for constructing dungeons without touching dungeon.c
// internals. Each dungeon is defined in its own file (e.g. dungeon_1.c).
//
// Usage pattern:
//
//   Dungeon p;
//   DungeonBuilder pb;
//   pb_init(&pb, &p, reg);
//   pb_set_meta(&pb, "Castle of Greed", "The Greedy King",
//               "dungeon_1_cleared", "hungerscape_unlocked", 19);
//
//   pb_begin_floor(&pb, 1, "Entry Hall");
//     int r0 = pb_add_safe_room(&pb, "Dungeon Entrance");
//     int r1 = pb_add_normal_room(&pb, "Guard Chamber", "f1r1_cleared",
//                                  ENEMY_SHADOW_T2, 2,
//                                  ENEMY_NONE, 0);
//     pb_connect(&pb, r0, EXIT_SOUTH, r1);
//     pb_set_stairs(&pb, r4);
//   pb_end_floor(&pb);
//
//   pb_begin_floor(&pb, 2, "Treasury");
//     // ...
//   pb_end_floor(&pb);
//
//   pb_finish(&pb);

#include "dungeon.h"
#include "enemy_registry.h"

// Max enemies per room group (matching entity.h MAX_GROUP_SIZE)
#define PB_MAX_ENEMIES_PER_ROOM 4

// Sentinel — pass as enemy ID when a slot is unused
#define ENEMY_NONE  (-1)

typedef struct {
    Dungeon*        dungeon;
    EnemyRegistry* reg;
    int            current_floor;   // index into dungeon->floors
    int            building;        // 1 = mid-floor, 0 = between floors
} DungeonBuilder;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

// Initialise builder. dungeon must be a zeroed Dungeon struct.
void pb_init(DungeonBuilder* pb, Dungeon* dungeon, EnemyRegistry* reg);

// Set dungeon metadata — call after pb_init, before any floors
void pb_set_meta(DungeonBuilder* pb,
                 const char* name,
                 const char* ruler,
                 const char* clear_flag,
                 const char* entry_flag,
                 int         deadline_day);

// Begin a new floor. Call pb_add_*_room functions, then pb_end_floor.
void pb_begin_floor(DungeonBuilder* pb, int floor_number, const char* name);

// Finalise current floor.
void pb_end_floor(DungeonBuilder* pb);

// Mark the stairs room on the current floor (call after adding the room).
void pb_set_stairs(DungeonBuilder* pb, int room_id);

// Tag a chest room's chest with a flag that gets set when the chest is opened.
// Used to drive quests off specific chest pickups.
void pb_set_chest_flag(DungeonBuilder* pb, int room_id, const char* flag);

// Finish building — validates connections. Call after all floors.
void pb_finish(DungeonBuilder* pb);

// Set the team meeting dialogue node for this dungeon
void pb_set_team_meeting(DungeonBuilder* pb, int node, const char* flag);

// ── Room adders ───────────────────────────────────────────────────────────────
// All return the room ID (index into current floor's rooms array).

int pb_add_safe_room(DungeonBuilder* pb, const char* name);

// Normal room with up to 2 enemy groups (pass ENEMY_NONE / 0 for unused slots)
int pb_add_normal_room(DungeonBuilder* pb,
                       const char* name,
                       const char* win_flag,
                       EnemyID     enemy1_id, int enemy1_count,
                       EnemyID     enemy2_id, int enemy2_count);

// Chest room with item reward
int pb_add_chest_room(DungeonBuilder* pb,
                      const char* name,
                      int item_id, int item_qty);

// Chest room with equipment reward
int pb_add_chest_room_equip(DungeonBuilder* pb,
                             const char* name,
                             int equip_id);

// Dungeon treasure room (final chest — raises alert on open)
int pb_add_treasure_room(DungeonBuilder* pb,
                          const char* name,
                          int equip_id);

// Boss room
int pb_add_boss_room(DungeonBuilder* pb,
                     const char* name,
                     BossConfig* boss);

// ── Connections ───────────────────────────────────────────────────────────────
// Connect two rooms bidirectionally on the current floor.
void pb_connect(DungeonBuilder* pb,
                int room_a, ExitDir dir_from_a,
                int room_b);

// Connect one-way only (rarely needed)
void pb_connect_one_way(DungeonBuilder* pb,
                         int from_room, ExitDir dir,
                         int to_room);

// ── Boss config helpers ───────────────────────────────────────────────────────
// Convenience for building BossConfig without touching the struct directly.

BossConfig pb_make_boss(EnemyRegistry* reg,
                         EnemyID id,
                         int hp, int sp,
                         int st, int ma, int en, int ag, int lu,
                         int exp_reward,
                         int pre_battle_node,
                         int post_battle_node);

void pb_boss_add_phase2_skill(BossConfig* boss, int skill_id);
void pb_boss_set_phase2_immune(BossConfig* boss, int element);
void pb_boss_set_phase2_hp_pct(BossConfig* boss, int pct);
void pb_boss_add_loot(BossConfig* boss, int item_id, int chance);
void pb_boss_set_treasure_equip(BossConfig* boss, int equip_id);
void pb_boss_set_treasure_item(BossConfig* boss, int item_id, int qty);

#endif