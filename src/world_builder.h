#ifndef WORLD_BUILDER_H
#define WORLD_BUILDER_H

// ── World Builder API ─────────────────────────────────────────────────────────
// Clean API for constructing overworld rooms without touching room.c internals.
// Each area is defined in its own file (area_1.c, area_2.c, etc.)
//
// Usage pattern:
//
//   RoomManager rm;
//   WorldBuilder wb;
//   wb_init(&wb, &rm, &enemy_registry);
//
//   int r0 = wb_add_room(&wb, "Town Square");
//     wb_set_map_walls(&wb, r0, ...);   // optional custom walls
//     wb_add_npc(&wb, r0, "Stranger", 400, 300, 0, 4);
//     wb_add_shop(&wb, r0, "Takemi Clinic", items, item_count);
//     wb_add_enemy(&wb, r0, ENEMY_SHADOW_T1, 2, "defeated_shadows", 600, 200);
//     wb_add_transition(&wb, r0, 1, 200, 64);   // -> room 1, spawn at 200,64
//
//   int r1 = wb_add_room(&wb, "Back Alley");
//     wb_add_npc(&wb, r1, "Hooded Figure", 200, 200, 5, 7);
//     wb_add_transition(&wb, r1, 0, 200, 580);  // -> room 0
//
//   wb_finish(&wb);

#include "room.h"
#include "enemy_registry.h"
#include "shop.h"

#define WB_MAX_SHOP_ITEMS 16

typedef struct {
    RoomManager*   rm;
    EnemyRegistry* reg;
} WorldBuilder;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void wb_init(WorldBuilder* wb, RoomManager* rm, EnemyRegistry* reg);
void wb_finish(WorldBuilder* wb);

// ── Room creation ─────────────────────────────────────────────────────────────

// Add a new room — returns its ID
int wb_add_room(WorldBuilder* wb, const char* name);

// ── Map customisation ─────────────────────────────────────────────────────────

// Add a rectangular wall block
void wb_add_wall_rect(WorldBuilder* wb, int room_id,
                      int row_start, int row_end,
                      int col_start, int col_end);

// Add a floor tile (carve a gap in walls)
void wb_add_floor(WorldBuilder* wb, int room_id, int row, int col);

// Restrict walkable area to columns [col_min, col_max] (walls everywhere else)
void wb_restrict_cols(WorldBuilder* wb, int room_id,
                      int col_min, int col_max);

// ── Entities ──────────────────────────────────────────────────────────────────

// Add an NPC — dialogue_start and dialogue_repeat are node IDs
void wb_add_npc(WorldBuilder* wb, int room_id,
                const char* name,
                int x, int y,
                int dialogue_start, int dialogue_repeat);

// Add an enemy group — uses enemy registry IDs
// enemy2_id = ENEMY_NONE if only one type
void wb_add_enemy(WorldBuilder* wb, int room_id,
                  EnemyID enemy1_id, int enemy1_count,
                  EnemyID enemy2_id, int enemy2_count,
                  const char* win_flag,
                  int x, int y);

// ── Shop ──────────────────────────────────────────────────────────────────────

// Add a shop to the room — shop NPC is placed at (npc_x, npc_y)
// item_ids: array of item IDs to sell (-1 terminates)
void wb_add_shop(WorldBuilder* wb, int room_id,
                 const char* shop_name,
                 const char* npc_name,
                 int npc_x, int npc_y,
                 int npc_dialogue_start,
                 int* item_ids, int item_count);

// ── Transitions ───────────────────────────────────────────────────────────────

// Add a room transition trigger at the bottom edge -> target room
void wb_add_transition_bottom(WorldBuilder* wb, int room_id,
                               int target_room_id,
                               float spawn_x, float spawn_y);

// Add a room transition trigger at the top edge -> target room
void wb_add_transition_top(WorldBuilder* wb, int room_id,
                            int target_room_id,
                            float spawn_x, float spawn_y);

// Add a custom transition zone
void wb_add_transition(WorldBuilder* wb, int room_id,
                       int trigger_x, int trigger_y,
                       int trigger_w, int trigger_h,
                       int target_room_id,
                       float spawn_x, float spawn_y);

#endif