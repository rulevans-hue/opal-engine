#ifndef ITEM_LOADER_H
#define ITEM_LOADER_H

#include "items.h"

// Load items from JSON. Returns 1 on success (>=1 item), 0 on failure.
int item_load_json(ItemDB* db, const char* filepath);

#endif