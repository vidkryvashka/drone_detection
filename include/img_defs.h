#ifndef IMG_PROC_H
#define IMG_PROC_H

#include "defs.h"
#include "my_vector.h"


typedef struct {
	uint16_t x;
	uint16_t y;
} pixel_coord_t;

typedef struct {
    vector_t *coords;
    pixel_coord_t center_coord;
} pixels_cloud_t;


enum CHANNELS {
	GRAY = 1,	// mostly used
	RGB = 3,
	RGBA = 4
};

typedef struct {
	uint8_t *pixels;
	uint16_t width;
	uint16_t height;
	enum CHANNELS channel;
} image_t;


#endif
