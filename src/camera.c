#include "camera.h"

void camera_update(Camera* cam, float player_x, float player_y,
                   int player_w, int player_h,
                   int map_pixel_w, int map_pixel_h) {

    // Center camera on player
    cam->x = player_x + player_w / 2 - CAM_W / 2;
    cam->y = player_y + player_h / 2 - CAM_H / 2;

    // Clamp so we don't show outside the map
    if (cam->x < 0) cam->x = 0;
    if (cam->y < 0) cam->y = 0;
    if (cam->x + CAM_W > map_pixel_w) cam->x = map_pixel_w - CAM_W;
    if (cam->y + CAM_H > map_pixel_h) cam->y = map_pixel_h - CAM_H;
}