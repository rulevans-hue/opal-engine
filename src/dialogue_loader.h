#ifndef DIALOGUE_LOADER_H
#define DIALOGUE_LOADER_H

#include "dialogue.h"
#include "gamestate.h"

// Load dialogue from JSON. Parses both:
//   "nodes"          -> legacy flat DialogueSystem array (global indices)
//   "conversations"  -> keyed DialogueDB (local indices per conversation)
// Either or both may be present. Returns 1 on success, 0 on failure
// (on failure, dialogue.c hardcoded nodes are used instead).
int dialogue_load_json(DialogueSystem* ds, DialogueDB* db,
                       GameState* gs, const char* filepath);

#endif