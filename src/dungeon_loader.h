#ifndef DUNGEON_LOADER_H
#define DUNGEON_LOADER_H

#include "dungeon.h"
#include "enemy_registry.h"

// Build a dungeon from data/dungeons.json by its string id (e.g. "dungeon_1").
// Returns 1 on success, 0 on failure (caller should fall back to hardcoded).
int dungeon_build_from_json(Dungeon* p, EnemyRegistry* reg, const char* dungeon_id);

#endif