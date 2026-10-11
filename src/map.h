#ifndef MAP_H
#define MAP_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#endif
#include "camera.h"

#define TILE_SIZE 32
#define MAP_COLS  80
#define MAP_ROWS  46

typedef enum {
    TILE_FLOOR = 0,
    TILE_WALL  = 1
} TileType;

typedef struct {
    TileType tiles[MAP_ROWS][MAP_COLS];
} Map;

void map_init(Map* map);
void map_render(SDL_Renderer* renderer, Map* map, Camera* cam);
int  map_is_wall(Map* map, int x, int y);

#endif