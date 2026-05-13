#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include "defs.h"


typedef struct {
    uint16_t x;
    uint16_t y;
} pixel_coord_t;

typedef struct {
    uint8_t *pixels;
    int width;
    int height;
    int channels; // 1 для Gray, 3 для RGB, 4 для RGBA
} image_t;


/**
 * @brief Load any (PNG, JPG, BMP) and transform to grayscale
 */
image_t* image_load_gray(const char* filename);


/**
 * @brief Save PNG
 */
errno_t image_save_png(const char* filename, const image_t* img);


/**
 * @brief Create empty image
 */
image_t* image_create(int w, int h, int channels);


void image_free(image_t* img);

#endif