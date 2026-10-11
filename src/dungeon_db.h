#ifndef DUNGEON_DB_H
#define DUNGEON_DB_H
#include "dungeon.h"

// ── Dungeon Database ───────────────────────────────────────────────────────────
// Registers all dungeons by ID. The dungeon system queries this to determine
// which dungeon to enter based on current gamestate flags.
//
// To add a new dungeon:
//   1. Create dungeon_N.c with a build_dungeon_N function
//   2. Call dungeon_db_register in dungeon_db_init
//   3. Add dungeon_N.c to the Makefile
//   That's it. No other files need to change.

// Forward declarations to avoid circular includes
#include "enemy_registry.h"
#include "gamestate.h"

#ifndef MAX_DUNGEONS
#define MAX_DUNGEONS 8
#endif

#define MAX_DUNGEONS 8

// Function pointer type for dungeon builders
typedef void (*DungeonBuilderFn)(Dungeon* p, EnemyRegistry* reg);

typedef struct {
    int             id;
    DungeonBuilderFn builder;
    char            entry_flag[64];   // gamestate flag required to enter
    char            clear_flag[64];   // set when dungeon is cleared
    int             deadline_day;     // absolute game day deadline
    char            name[64];         // display name (for UI)
    char            json_id[64];      // dungeons.json id (data-driven build)
} DungeonEntry;

typedef struct {
    DungeonEntry entries[MAX_DUNGEONS];
    int         count;
} DungeonDB;

// Init — registers all dungeons. Call once at startup.
void dungeon_db_init(DungeonDB* db);

// Populate the DB straight from data/dungeons.json (id/name/flags/deadline).
// Returns the number of dungeons registered, or 0 on failure.
int  dungeon_load_registry(DungeonDB* db);

// Build dungeon entry `idx` into `dungeon` (via builder fn, or JSON by json_id).
void dungeon_db_build(DungeonDB* db, int idx, Dungeon* dungeon, EnemyRegistry* reg);

// Register a dungeon entry. Called from dungeon_db_init.
void dungeon_db_register(DungeonDB* db,
                         int             id,
                         DungeonBuilderFn builder,
                         const char*     entry_flag,
                         const char*     clear_flag,
                         int             deadline_day,
                         const char*     name);

// Find the active dungeon — highest ID whose entry_flag is set
// and whose clear_flag is NOT set. Returns -1 if none available.
int dungeon_db_get_active(DungeonDB* db, GameState* gs);

// Get a specific entry by ID. Returns NULL if not found.
DungeonEntry* dungeon_db_get(DungeonDB* db, int id);

#endif