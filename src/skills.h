#ifndef SKILLS_H
#define SKILLS_H

#include "stats.h"

#define MAX_SKILLS 128

typedef enum {
    TARGET_ONE_ENEMY,
    TARGET_ALL_ENEMIES,
    TARGET_ONE_ALLY,
    TARGET_ALL_ALLIES,
    TARGET_SELF
} SkillTarget;

typedef struct {
    int          id;
    char         name[64];
    char         description[128];
    Element      element;
    int          sp_cost;
    int          power;
    SkillTarget  target;
    StatusEffect inflicts;
    float        status_chance;
    int          is_heal;
    int          is_buff;
    int          stat_buff;    // which stat to buff (0=none)
    int          buff_amount;
} Skill;

typedef struct {
    Skill skills[MAX_SKILLS];
    int   count;
} SkillDB;

void skilldb_init(SkillDB* db);
Skill* skilldb_get(SkillDB* db, int id);
int    skilldb_add(SkillDB* db, Skill skill);

#endif