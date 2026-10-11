#include "world_builder.h"
#include <string.h>
#include <stdio.h>

void wb_init(WorldBuilder* wb, RoomManager* rm, EnemyRegistry* reg) {
    wb->rm  = rm;
    wb->reg = reg;
    rm->count   = 0;
    rm->current = 0;
}

void wb_finish(WorldBuilder* wb) {
    printf("World built: %d rooms\n", wb->rm->count);
}

int wb_add_room(WorldBuilder* wb, const char* name) {
    RoomManager* rm = wb->rm;
    if (rm->count >= MAX_ROOMS) {
        printf("wb_add_room: too many rooms\n");
        return -1;
    }
    int id = rm->count;
    Room* r = &rm->rooms[id];
    memset(r, 0, sizeof(Room));
    r->id = id;
    strncpy(r->name, name, 63);
    map_init(&r->map);
    entitylist_init(&r->entities);
    triggerlist_init(&r->triggers);
    r->has_shop = 0;
    rm->count++;
    return id;
}

// ── Map helpers ───────────────────────────────────────────────────────────────

void wb_add_wall_rect(WorldBuilder* wb, int room_id,
                      int row_start, int row_end,
                      int col_start, int col_end) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    Map* m = &wb->rm->rooms[room_id].map;
    for (int r = row_start; r <= row_end && r < MAP_ROWS; r++)
        for (int c = col_start; c <= col_end && c < MAP_COLS; c++)
            m->tiles[r][c] = TILE_WALL;
}

void wb_add_floor(WorldBuilder* wb, int room_id, int row, int col) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return;
    wb->rm->rooms[room_id].map.tiles[row][col] = TILE_FLOOR;
}

void wb_restrict_cols(WorldBuilder* wb, int room_id,
                      int col_min, int col_max) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    Map* m = &wb->rm->rooms[room_id].map;
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            if (c < col_min || c > col_max)
                m->tiles[r][c] = TILE_WALL;
}

// ── Entity helpers ────────────────────────────────────────────────────────────

void wb_add_npc(WorldBuilder* wb, int room_id,
                const char* name,
                int x, int y,
                int dialogue_start, int dialogue_repeat) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    entitylist_add_npc(&wb->rm->rooms[room_id].entities,
                       x, y, 32, 48, name,
                       dialogue_start, dialogue_repeat);
}

void wb_add_enemy(WorldBuilder* wb, int room_id,
                  EnemyID enemy1_id, int enemy1_count,
                  EnemyID enemy2_id, int enemy2_count,
                  const char* win_flag,
                  int x, int y) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    if (!wb->reg) return;

    EnemyTemplate group[8];
    int total = 0;

    if (enemy1_id >= 0 && enemy1_count > 0) {
        EnemyTemplate base = enemy_registry_get(wb->reg, enemy1_id);
        for (int i = 0; i < enemy1_count && total < 8; i++)
            group[total++] = base;
    }
    if (enemy2_id >= 0 && enemy2_count > 0) {
        EnemyTemplate base = enemy_registry_get(wb->reg, enemy2_id);
        for (int i = 0; i < enemy2_count && total < 8; i++)
            group[total++] = base;
    }

    if (total > 0)
        entitylist_add_enemy(&wb->rm->rooms[room_id].entities,
                             x, y, 32, 32,
                             "Shadows", win_flag,
                             group, total);
}

// ── Shop helper ───────────────────────────────────────────────────────────────

void wb_add_shop(WorldBuilder* wb, int room_id,
                 const char* shop_name,
                 const char* npc_name,
                 int npc_x, int npc_y,
                 int npc_dialogue_start,
                 int* item_ids, int item_count) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    Room* r = &wb->rm->rooms[room_id];

    r->has_shop = 1;
    shop_init(&r->shop, shop_name);
    for (int i = 0; i < item_count; i++)
        shop_add_item(&r->shop, item_ids[i], -1);

    // Shop NPC — uses the same dialogue start for "browse" prompt
    entitylist_add_npc(&r->entities,
                       npc_x, npc_y, 32, 48,
                       npc_name,
                       npc_dialogue_start,
                       npc_dialogue_start);
}

// ── Transition helpers ────────────────────────────────────────────────────────

void wb_add_transition_bottom(WorldBuilder* wb, int room_id,
                               int target_room_id,
                               float spawn_x, float spawn_y) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    triggerlist_add_transition(&wb->rm->rooms[room_id].triggers,
        0, (MAP_ROWS - 2) * TILE_SIZE,
        MAP_COLS * TILE_SIZE, TILE_SIZE,
        target_room_id, (int)spawn_x, (int)spawn_y);
}

void wb_add_transition_top(WorldBuilder* wb, int room_id,
                            int target_room_id,
                            float spawn_x, float spawn_y) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    triggerlist_add_transition(&wb->rm->rooms[room_id].triggers,
        0, TILE_SIZE,
        MAP_COLS * TILE_SIZE, TILE_SIZE,
        target_room_id, (int)spawn_x, (int)spawn_y);
}

void wb_add_transition(WorldBuilder* wb, int room_id,
                       int trigger_x, int trigger_y,
                       int trigger_w, int trigger_h,
                       int target_room_id,
                       float spawn_x, float spawn_y) {
    if (room_id < 0 || room_id >= wb->rm->count) return;
    triggerlist_add_transition(&wb->rm->rooms[room_id].triggers,
        trigger_x, trigger_y, trigger_w, trigger_h,
        target_room_id, (int)spawn_x, (int)spawn_y);
}