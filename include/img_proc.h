#ifndef IMG_PROC_H
#define IMG_PROC_H

#include "defs.h"
#include "my_vector.h"


typedef struct {
	uint16_t x;
	uint16_t y;
} pixel_coord_t;

enum CHANNELS {
	GRAY = 1,
	RGB = 3,
	RGBA = 4
};

typedef struct {
	uint8_t *pixels;
	uint16_t width;
	uint16_t height;
	enum CHANNELS channel;
} image_t;


#define START_THRESHOLD 120
#define KEYPOINTS_MAX_COUNT 32
#define BRIEF_SIZE 256
#define PATCH_SIZE 31
#define SIGMA 5
#define MAX_KEYPOINTS 10000
#define SCALE_FACTOR 1.41421356237 // sqrt(2)
#define NLEVELS 8
#define EDGE_THRESHOLD 31
#define SAVIMG_IMAGE_DIM_COEF 0.4

/**
 * @brief Magic algorithm to detect keypoints, took from habr and rewrote
 */
vector_t* fast9(
	const image_t *gimg,
	const uint8_t threshold
);

#endif
