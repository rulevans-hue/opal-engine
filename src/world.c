#include "world.h"

void worldsystem_init(WorldSystem* ws) {
    ws->current              = WORLD_REAL;
    ws->can_enter_hungerscape  = 1;
}

void worldsystem_enter_hungerscape(WorldSystem* ws) {
    ws->current = WORLD_HUNGERSCAPE;
}

void worldsystem_return_real(WorldSystem* ws) {
    ws->current = WORLD_REAL;
}