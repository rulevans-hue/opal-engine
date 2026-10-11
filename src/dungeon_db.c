#include "dungeon.h"
#include "dungeon_db.h"
#include <string.h>
#include <stdio.h>
#include "dungeon_loader.h"

// Forward declare all dungeon builders
void build_dungeon_1(Dungeon* p, EnemyRegistry* reg);
void build_dungeon_2(Dungeon* p, EnemyRegistry* reg);
// void build_dungeon_3(Dungeon* p, EnemyRegistry* reg);

// JSON-driven builders: load from dungeons.json, fall back to hardcoded on failure.
static void build_dungeon_1_json(Dungeon* p, EnemyRegistry* reg) {
    if (!dungeon_build_from_json(p, reg, "dungeon_1"))
        build_dungeon_1(p, reg);   // fallback
}
static void build_dungeon_2_json(Dungeon* p, EnemyRegistry* reg) {
    if (!dungeon_build_from_json(p, reg, "dungeon_2"))
        build_dungeon_2(p, reg);   // fallback
}

void dungeon_db_init(DungeonDB* db) {
    memset(db, 0, sizeof(DungeonDB));

    // Data-driven: register every dungeon straight from data/dungeons.json.
    // Adding a new dungeon = adding a JSON object there. No C edit needed.
    dungeon_load_registry(db);

    // Hardcoded dungeons are registered only if their entry_flag isn't already
    // present (dungeon_db_register skips duplicates). JSON dungeons win; C-only
    // dungeons (e.g. dungeon_2) keep working; a missing dungeons.json => all here.
    dungeon_db_register(db, 1, build_dungeon_1_json,
        "hungerscape_unlocked", "dungeon_1_cleared", 19, "Castle of Greed");
    dungeon_db_register(db, 2, build_dungeon_2_json,
        "dungeon_2_unlocked", "dungeon_2_cleared", 40, "The Primeval Forest");
}

// Build dungeon entry `idx` into `dungeon`: use its builder fn if set, otherwise
// build from JSON by its json_id (the data-driven path).
void dungeon_db_build(DungeonDB* db, int idx, Dungeon* dungeon, EnemyRegistry* reg) {
    if (idx < 0 || idx >= db->count) return;
    DungeonEntry* e = &db->entries[idx];
    if (e->builder) { e->builder(dungeon, reg); return; }
    if (e->json_id[0] && dungeon_build_from_json(dungeon, reg, e->json_id)) return;
    printf("dungeon_db: failed to build dungeon idx %d\n", idx);
}

void dungeon_db_register(DungeonDB* db,
                         int id, DungeonBuilderFn builder,
                         const char* entry_flag, const char* clear_flag,
                         int deadline_day, const char* name) {
    // Skip if a dungeon with this entry_flag is already registered
    // (e.g. it was loaded from dungeons.json — JSON overrides hardcoded).
    for (int i = 0; i < db->count; i++)
        if (strcmp(db->entries[i].entry_flag, entry_flag) == 0)
            return;

    if (db->count >= MAX_DUNGEONS) {
        printf("dungeon_db: too many dungeons registered\n");
        return;
    }
    DungeonEntry* e = &db->entries[db->count++];
    e->id           = id;
    e->builder      = builder;
    e->deadline_day = deadline_day;
    strncpy(e->entry_flag, entry_flag, 63);
    strncpy(e->clear_flag, clear_flag, 63);
    strncpy(e->name,       name,       63);
    e->json_id[0] = '\0';
}

int dungeon_db_get_active(DungeonDB* db, GameState* gs) {
    // Walk entries in order, find highest-ID dungeon that:
    // - has its entry_flag set
    // - has NOT had its clear_flag set
    // This means cleared dungeons are skipped automatically.
    int active = -1;
    for (int i = 0; i < db->count; i++) {
        DungeonEntry* e = &db->entries[i];
        if (gamestate_get_flag(gs, e->entry_flag) &&
            !gamestate_get_flag(gs, e->clear_flag)) {
            active = i;  // take the last matching one
        }
    }
    return active;  // index into entries[], or -1
}

DungeonEntry* dungeon_db_get(DungeonDB* db, int id) {
    for (int i = 0; i < db->count; i++)
        if (db->entries[i].id == id)
            return &db->entries[i];
    return NULL;
}