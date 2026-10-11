#include "world_builder.h"
#include "enemy_registry.h"

// ── Area 1: Starting Town ─────────────────────────────────────────────────────
// Two rooms: Town Square (id=0) and Back Alley (id=1).
//
// To add a new room to this area:
//   1. Call wb_add_room and store the returned ID
//   2. Add walls, NPCs, enemies, transitions
//   3. Add a transition from existing rooms to the new one
//
// To add a new area entirely, create area_2.c and call build_area_2
// from roommanager_init in room.c.

void build_area_1(WorldBuilder* wb) {

    // ── Room 0: Town Square ───────────────────────────────────────────────────
    int r0 = wb_add_room(wb, "Town Square");

    // Interior building — a small walled structure with a door
    wb_add_wall_rect(wb, r0, 10, 20, 20, 30);
    wb_add_floor(wb, r0, 20, 25);   // doorway
    wb_add_floor(wb, r0, 20, 26);

    // NPC — Stranger near the building
    wb_add_npc(wb, r0, "Stranger", 400, 300, 0, 48);

    // Shop — Takemi Medical Clinic
    int clinic_items[] = {
         0,   // Medicine
         1,   // Medicine DX
        10,   // Snuff Soul
        11,   // Snuff Soul DX
        14,   // Revival Bead
        15,   // Balm of Life
        16,   // Digestive
        17,   // Stomach Medicine
        23,   // Flame Urn
        24,   // Freeze Spray
        25,   // Thunder Bomb
    };
    wb_add_shop(wb, r0,
                "Takemi Medical Clinic",
                "Shopkeeper",
                200, 500,
                15,             // dialogue node — shop open choice
                clinic_items, 11);

    // Enemy — 2x Shadow T1 near the east side
    wb_add_enemy(wb, r0,
                 ENEMY_SHADOW_T1, 2,
                 ENEMY_NONE,     0,
                 "defeated_shadows_0",
                 600, 200);

    // Transition: walk south -> Back Alley, spawn near top
    wb_add_transition_bottom(wb, r0, 1, 200, 64);

    // ── Room 1: Back Alley ────────────────────────────────────────────────────
    int r1 = wb_add_room(wb, "Back Alley");

    // Narrow corridor — only columns 5-35 are walkable
    wb_restrict_cols(wb, r1, 5, 35);

    // NPC — Hooded Figure
    wb_add_npc(wb, r1, "Hooded Figure", 200, 200, 5, 7);

    // Enemy — Dark Spirit + Shadow
    wb_add_enemy(wb, r1,
                 ENEMY_DARK_SPIRIT_T1, 1,
                 ENEMY_SHADOW_T1,      1,
                 "defeated_dark_ones",
                 500, 400);

    // Transition: walk north -> Town Square, spawn near bottom
    wb_add_transition_top(wb, r1, 0, 200, (MAP_ROWS - 3) * TILE_SIZE);
}