#ifndef NPC_H
#define NPC_H

#include "gamestate.h"
#include "dialogue.h"
#include "bond.h"

#define MAX_NPCS        64
#define NPC_ID_LEN      64
#define NPC_NAME_LEN    64

// A placeable, talkable NPC in the overworld.
typedef struct {
    char  id[NPC_ID_LEN];           // "stranger" — stable identifier
    char  display_name[NPC_NAME_LEN];

    float x, y, z;                  // world position (overworld)

    char  convo_first[CONVO_KEY_LEN];   // conversation on first meeting
    char  convo_repeat[CONVO_KEY_LEN];  // conversation on later meetings
    char  met_flag[FLAG_NAME_LEN];      // gamestate flag: have we met?

    char  bond_name[NPC_NAME_LEN]; // link to bond (empty = none)

    int   active;
} NPCDefinition;

typedef struct {
    NPCDefinition npcs[MAX_NPCS];
    int           count;
} NPCRegistry;

// Init / load
void npc_registry_init(NPCRegistry* reg);          // clears + loads npcs.json
int  npc_registry_load_json(NPCRegistry* reg, const char* filepath);

// Lookup
NPCDefinition* npc_find_by_id(NPCRegistry* reg, const char* id);
NPCDefinition* npc_get(NPCRegistry* reg, int index);

// Interaction: resolve first/repeat and start the conversation.
// Sets the met_flag on first interaction. Returns 1 if a conversation started.
int npc_interact(NPCRegistry* reg, const char* id,
                 DialogueSystem* ds, GameState* gs, BondDB* cdb);
#endif