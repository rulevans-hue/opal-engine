#include "room.h"
#include "world_builder.h"
#include "enemy_registry.h"
#include <string.h>
#include <stdio.h>

// Forward declare area builders — each area is in its own file
void build_area_1(WorldBuilder* wb);
// void build_area_2(WorldBuilder* wb);  // uncomment when area_2.c is ready

// Global enemy registry — defined in main.c
extern EnemyRegistry g_enemy_registry;

void roommanager_init(RoomManager* rm) {
    WorldBuilder wb;
    wb_init(&wb, rm, &g_enemy_registry);

    // Build all areas — add new ones here as the game grows
    build_area_1(&wb);
    // build_area_2(&wb);

    wb_finish(&wb);
}

void roommanager_load(RoomManager* rm, int room_id,
                      float* player_x, float* player_y) {
    for (int i = 0; i < rm->count; i++) {
        if (rm->rooms[i].id == room_id) {
            rm->current = i;
            printf("Loaded room: %s\n", rm->rooms[i].name);
            return;
        }
    }
    printf("Room %d not found — defaulting to room 0\n", room_id);
    rm->current = 0;
}

Room* roommanager_current(RoomManager* rm) {
    return &rm->rooms[rm->current];
}