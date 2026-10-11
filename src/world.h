#ifndef WORLD_H
#define WORLD_H

typedef enum {
    WORLD_REAL,       // real world — school, shops, social links
    WORLD_HUNGERSCAPE   // dungeon — combat, exploration
} WorldType;

typedef struct {
    WorldType current;
    int       can_enter_hungerscape;  // flag — is the hungerscape accessible today?
} WorldSystem;

void worldsystem_init(WorldSystem* ws);
void worldsystem_enter_hungerscape(WorldSystem* ws);
void worldsystem_return_real(WorldSystem* ws);

#endif