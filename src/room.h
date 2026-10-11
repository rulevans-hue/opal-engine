#ifndef ROOM_H
#define ROOM_H

#include "map.h"
#include "entity.h"
#include "trigger.h"
#include "shop.h"

#define MAX_ROOMS 16

typedef struct {
    int        id;
    char       name[64];
    Map        map;
    EntityList entities;
    TriggerList triggers;
    Shop       shop;
    int        has_shop;
} Room;

typedef struct {
    Room rooms[MAX_ROOMS];
    int  count;
    int  current;
} RoomManager;

void roommanager_init(RoomManager* rm);
void roommanager_load(RoomManager* rm, int room_id,
                      float* player_x, float* player_y);
Room* roommanager_current(RoomManager* rm);

#endif