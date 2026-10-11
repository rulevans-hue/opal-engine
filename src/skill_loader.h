#ifndef SKILL_LOADER_H
#define SKILL_LOADER_H

#include "skills.h"

// Load skills from JSON. Returns 1 on success (>=1 skill), 0 on failure.
int skill_load_json(SkillDB* db, const char* filepath);

#endif