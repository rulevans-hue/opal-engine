#include "entity.h"
#include <string.h>

void entitylist_init(EntityList* el) {
    el->count = 0;
}

void entitylist_add_npc(EntityList* el, float x, float y,
                        int w, int h, const char* name,
                        int dialogue_start, int dialogue_repeat) {
    if (el->count >= MAX_ENTITIES) return;
    Entity* e = &el->entities[el->count++];
    memset(e, 0, sizeof(Entity));
    e->type            = ENTITY_NPC;
    e->x               = x;  e->y      = y;
    e->width           = w;  e->height = h;
    e->dialogue_start  = dialogue_start;
    e->dialogue_repeat = dialogue_repeat;
    e->active          = 1;
    strncpy(e->name, name, 63);
}

void entitylist_add_enemy(EntityList* el, float x, float y,
                          int w, int h, const char* name,
                          const char* win_flag,
                          EnemyTemplate* group, int group_size) {
    if (el->count >= MAX_ENTITIES) return;
    Entity* e = &el->entities[el->count++];
    memset(e, 0, sizeof(Entity));
    e->type       = ENTITY_ENEMY;
    e->x          = x;  e->y      = y;
    e->width      = w;  e->height = h;
    e->defeated   = 0;
    e->active     = 1;
    e->group_size = group_size;
    strncpy(e->name,     name,     63);
    strncpy(e->win_flag, win_flag, 63);
    for (int i = 0; i < group_size && i < MAX_GROUP_SIZE; i++)
        e->group[i] = group[i];
}

EnemyTemplate make_enemy_template(const char* name,
                                   int hp, int sp,
                                   int st, int ma, int en, int ag, int lu,
                                   int exp_reward) {
    EnemyTemplate t;
    memset(&t, 0, sizeof(EnemyTemplate));
    strncpy(t.name, name, 63);
    t.hp = hp; t.sp = sp;
    t.st = st; t.ma = ma; t.en = en;
    t.ag = ag; t.lu = lu;
    t.exp_reward = exp_reward;
    for (int i = 0; i < 10; i++)
        t.affinities[i] = 0;
    t.loot_count = 0;
    return t;
}

void enemy_template_set_affinity(EnemyTemplate* t, int element, int affinity) {
    if (element >= 0 && element < 10)
        t->affinities[element] = affinity;
}

void enemy_template_add_loot(EnemyTemplate* t, int item_id, int chance) {
    if (t->loot_count >= MAX_LOOT_ENTRIES) return;
    t->loot_ids[t->loot_count]     = item_id;
    t->loot_chances[t->loot_count] = chance;
    t->loot_count++;
}