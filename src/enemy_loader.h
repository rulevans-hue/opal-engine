#ifndef ENEMY_LOADER_H
#define ENEMY_LOADER_H

#include "enemy_registry.h"

// Load enemy templates from JSON into the registry.
// Returns 1 on success (>=1 enemy loaded), 0 on failure.
int enemy_load_json(EnemyRegistry* reg, const char* filepath);

#endif