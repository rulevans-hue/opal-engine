#include "map.h"
#include <string.h>

void map_init(Map* map) {
    // Fill everything with floor first
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            map->tiles[r][c] = TILE_FLOOR;

    // Border walls
    for (int c = 0; c < MAP_COLS; c++) {
        map->tiles[0][c] = TILE_WALL;
        // No bottom wall — bottom edge is used for room transitions
    }
    for (int r = 0; r < MAP_ROWS; r++) {
        map->tiles[r][0]          = TILE_WALL;
        map->tiles[r][MAP_COLS-1] = TILE_WALL;
    }

    // A small building
    for (int c = 10; c <= 20; c++) {
        map->tiles[5][c]  = TILE_WALL;
        map->tiles[15][c] = TILE_WALL;
    }
    for (int r = 5; r <= 15; r++) {
        map->tiles[r][10] = TILE_WALL;
        map->tiles[r][20] = TILE_WALL;
    }
    // Doorway
    map->tiles[15][15] = TILE_FLOOR;
    map->tiles[15][16] = TILE_FLOOR;
}

void map_render(SDL_Renderer* renderer, Map* map, Camera* cam) {
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            SDL_Rect rect = {
                c * TILE_SIZE - (int)cam->x,
                r * TILE_SIZE - (int)cam->y,
                TILE_SIZE,
                TILE_SIZE
            };

            // Skip tiles completely off screen
            if (rect.x + TILE_SIZE < 0 || rect.x > CAM_W) continue;
            if (rect.y + TILE_SIZE < 0 || rect.y > CAM_H) continue;

            if (map->tiles[r][c] == TILE_WALL)
                SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
            else
                SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);

            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

int map_is_wall(Map* map, int x, int y) {
    int col = x / TILE_SIZE;
    int row = y / TILE_SIZE;
    if (col < 0 || col >= MAP_COLS || row < 0 || row >= MAP_ROWS) return 1;
    return map->tiles[row][col] == TILE_WALL;
}