#ifndef CAMERA_H
#define CAMERA_H

#define CAM_W 1280
#define CAM_H 720

typedef struct {
    float x, y;
} Camera;

void camera_update(Camera* cam, float player_x, float player_y,
                   int player_w, int player_h,
                   int map_pixel_w, int map_pixel_h);

#endif