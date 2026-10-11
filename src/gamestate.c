#include "gamestate.h"
#include <string.h>
#include <stdio.h>

void gamestate_init(GameState* gs) {
    gs->flag_count      = 0;
    gs->var_count       = 0;
    gs->knowledge_count = 0;
}

void gamestate_set_flag(GameState* gs, const char* name, int value) {
    for (int i = 0; i < gs->flag_count; i++) {
        if (strcmp(gs->flags[i].name, name) == 0) {
            gs->flags[i].value = value;
            return;
        }
    }
    if (gs->flag_count < MAX_FLAGS) {
        strncpy(gs->flags[gs->flag_count].name, name, FLAG_NAME_LEN - 1);
        gs->flags[gs->flag_count].value = value;
        gs->flag_count++;
    }
}

int gamestate_get_flag(GameState* gs, const char* name) {
    for (int i = 0; i < gs->flag_count; i++)
        if (strcmp(gs->flags[i].name, name) == 0)
            return gs->flags[i].value;
    return 0;
}

void gamestate_set_var(GameState* gs, const char* name, int value) {
    for (int i = 0; i < gs->var_count; i++) {
        if (strcmp(gs->vars[i].name, name) == 0) {
            gs->vars[i].value = value;
            return;
        }
    }
    if (gs->var_count < MAX_VARS) {
        strncpy(gs->vars[gs->var_count].name, name, FLAG_NAME_LEN - 1);
        gs->vars[gs->var_count].value = value;
        gs->var_count++;
    }
}

int gamestate_get_var(GameState* gs, const char* name) {
    for (int i = 0; i < gs->var_count; i++)
        if (strcmp(gs->vars[i].name, name) == 0)
            return gs->vars[i].value;
    return 0;
}

EnemyKnowledge* gamestate_get_knowledge(GameState* gs,
                                         const char* enemy_name) {
    // Find existing
    for (int i = 0; i < gs->knowledge_count; i++)
        if (strcmp(gs->knowledge[i].enemy_name, enemy_name) == 0)
            return &gs->knowledge[i];

    // Create new entry
    if (gs->knowledge_count < MAX_ENEMY_TYPES) {
        EnemyKnowledge* k = &gs->knowledge[gs->knowledge_count++];
        strncpy(k->enemy_name, enemy_name, FLAG_NAME_LEN - 1);
        for (int i = 0; i < ELEMENT_COUNT; i++)
            k->discovered[i] = 0;
        return k;
    }
    return NULL;
}

void gamestate_discover_affinity(GameState* gs, const char* enemy_name,
                                  int element) {
    EnemyKnowledge* k = gamestate_get_knowledge(gs, enemy_name);
    if (k) k->discovered[element] = 1;
}

int gamestate_is_discovered(GameState* gs, const char* enemy_name,
                              int element) {
    EnemyKnowledge* k = gamestate_get_knowledge(gs, enemy_name);
    if (k) return k->discovered[element];
    return 0;
}