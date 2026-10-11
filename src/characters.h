#ifndef CHARACTERS_H
#define CHARACTERS_H

#include "stats.h"
#include "party.h"
#include "skills.h"

#define MAX_SKILL_LEARNS 16

// One entry in a character's skill learn table
typedef struct {
    int level;     // level at which this skill becomes available
    int skill_id;  // which skill to offer
} SkillLearnEntry;

// Per-character learn table
typedef struct {
    SkillLearnEntry entries[MAX_SKILL_LEARNS];
    int             count;
} SkillLearnTable;

// Returns the learn table for a named character.
// Returns NULL if the character has no learn table.
const SkillLearnTable* characters_get_learn_table(const char* name);

// Returns the skill_id to learn at a given level for a character,
// or -1 if nothing is learned at that level.
int characters_skill_at_level(const char* name, int level);

// Build the player's starting party
void build_starter_party(Party* p);

// Build a specific character's stat block
StatBlock make_protagonist();
StatBlock make_rook();
StatBlock make_wren();
StatBlock make_sable();

#endif