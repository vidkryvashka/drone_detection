#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include "defs.h"
#include "img_proc.h"

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
image_t* image_create(uint16_t width, uint16_t height, enum CHANNELS channel);


/**
 * @brief Free image
 * return EINVAL in case image is NULL
 */
errno_t image_free(image_t* img);

#endif
