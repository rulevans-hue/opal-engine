#ifndef BOND_LOADER_H
#define BOND_LOADER_H

#include "bond.h"

// Load bonds from JSON. Returns 1 on success (>=1 bond loaded),
// 0 on failure (file missing / parse error / zero bonds).
int bond_load_json(BondDB* db, const char* filepath);

#endif