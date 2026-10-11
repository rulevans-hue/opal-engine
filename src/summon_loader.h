#ifndef SUMMON_LOADER_H
#define SUMMON_LOADER_H

#include "summon.h"

// Load summons from JSON into the DB. Returns 1 on success, 0 on failure.
int summon_load_json(SummonDB* db, const char* filepath);

#endif