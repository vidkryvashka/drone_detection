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

/*
 * Image IO
 */
#define DEFAULT_OUTPUT_DIR "output"
image_t* image_load(const char* filename, enum CHANNELS channel);
errno_t image_save_jpg(const char* filename, const char* output_dir, const image_t* img);	// filename of saved output is the same
image_t* image_create(uint16_t width, uint16_t height, enum CHANNELS channel);
errno_t image_free(image_t* img);

#define DEFAULT_THRESHOLD 70
#define DEAFAULT_DIM_COEF 3
#define MAX_DIM_COEF 12
#define START_THRESHOLD 120
#define EDGE_THRESHOLD 31
// #define MAX_KEYPOINTS 10000 // mb to prevent buffer overflow
// #define KEYPOINTS_MAX_COUNT 32 // idk
// #define BRIEF_SIZE 256 // mb for future fast9 development
// #define PATCH_SIZE 31
// #define SIGMA 5 /idk
// #define SCALE_FACTOR 1.41421356237 // sqrt(2)
// #define NLEVELS 8 // idk


/**
 * @brief place_points_on_img exactly
 * 
 * @param image image_t* would be nice if grey
 * @param keypoints vector* <pixel_coord_t> with information
 * @param brightness_coef uint8_t	0 - MAX_DIM_COEF brightness lvl for original pixels
 */
errno_t place_points_on_img(
	image_t *img,
	vector_t *keypoints,
	uint8_t brightness_coef
);

/**
 * @brief Keypoints searching algorithm, took from habr and rewrote
 */
vector_t* fast9(
	const image_t *gimg,
	const uint8_t threshold
);

#endif
