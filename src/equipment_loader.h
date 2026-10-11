#ifndef EQUIPMENT_LOADER_H
#define EQUIPMENT_LOADER_H

#include "equipment.h"

// Load equipment from JSON. Returns 1 on success (>=1 piece), 0 on failure.
int equipment_load_json(EquipDB* db, const char* filepath);

#endif