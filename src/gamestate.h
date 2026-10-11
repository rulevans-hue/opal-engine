#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "stats.h"

#define MAX_FLAGS       128
#define MAX_VARS        64
#define FLAG_NAME_LEN   64
#define MAX_ENEMY_TYPES 64

typedef struct {
    char name[FLAG_NAME_LEN];
    int  value;
} Flag;

typedef struct {
    char name[FLAG_NAME_LEN];
    int  value;
} GameVar;

// Tracks which affinities have been discovered for an enemy type
typedef struct {
    char     enemy_name[FLAG_NAME_LEN];
    int      discovered[ELEMENT_COUNT]; // 1 = revealed, 0 = hidden
} EnemyKnowledge;

typedef struct {
    Flag           flags[MAX_FLAGS];
    int            flag_count;
    GameVar        vars[MAX_VARS];
    int            var_count;
    EnemyKnowledge knowledge[MAX_ENEMY_TYPES];
    int            knowledge_count;
} GameState;

void gamestate_init(GameState* gs);

// Flags
void gamestate_set_flag(GameState* gs, const char* name, int value);
int  gamestate_get_flag(GameState* gs, const char* name);

// Vars
void gamestate_set_var(GameState* gs, const char* name, int value);
int  gamestate_get_var(GameState* gs, const char* name);

// Enemy knowledge
EnemyKnowledge* gamestate_get_knowledge(GameState* gs, const char* enemy_name);
void gamestate_discover_affinity(GameState* gs, const char* enemy_name,
                                 int element);
int  gamestate_is_discovered(GameState* gs, const char* enemy_name,
                              int element);

#endif