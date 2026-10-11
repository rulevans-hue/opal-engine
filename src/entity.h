#ifndef ENTITY_H
#define ENTITY_H

#include "dialogue.h"

#define MAX_ENTITIES        32

typedef enum {
    ENTITY_NPC,
    ENTITY_ENEMY
} EntityType;
#define MAX_GROUP_SIZE      4
#define MAX_LOOT_ENTRIES    6

// Defines one enemy in a group
typedef struct {
    char name[64];
    int  hp, sp;
    int  st, ma, en, ag, lu;
    int  exp_reward;
    int  affinities[10];
    int  skill_ids[8];
    int  skill_count;

    // Loot table — item_id + drop chance (0-100)
    int  loot_ids[MAX_LOOT_ENTRIES];
    int  loot_chances[MAX_LOOT_ENTRIES];  // percent chance per item on kill
    int  loot_count;
} EnemyTemplate;

typedef struct {
    EntityType    type;
    float         x, y;
    int           width, height;

    // NPC fields
    int           dialogue_start;
    int           dialogue_repeat;

    // Enemy group fields
    EnemyTemplate group[MAX_GROUP_SIZE];
    int           group_size;
    char          win_flag[64];
    int           defeated;
    float         pre_combat_x;
    float         pre_combat_y;

    // Shared
    char          name[64];
    int           active;
} Entity;

typedef struct {
    Entity entities[MAX_ENTITIES];
    int    count;
} EntityList;

void entitylist_init(EntityList* el);

void entitylist_add_npc(EntityList* el, float x, float y,
                        int w, int h, const char* name,
                        int dialogue_start, int dialogue_repeat);

void entitylist_add_enemy(EntityList* el, float x, float y,
                          int w, int h, const char* name,
                          const char* win_flag,
                          EnemyTemplate* group, int group_size);

EnemyTemplate make_enemy_template(const char* name,
                                   int hp, int sp,
                                   int st, int ma, int en, int ag, int lu,
                                   int exp_reward);

void enemy_template_set_affinity(EnemyTemplate* t, int element, int affinity);

// Add a loot entry to a template (item_id, drop chance 0-100)
void enemy_template_add_loot(EnemyTemplate* t, int item_id, int chance);

#endif