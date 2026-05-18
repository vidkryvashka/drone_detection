#ifndef IMG_PROC_H
#define IMG_PROC_H

#include "defs.h"
#include "my_vector.h"


typedef struct {
	uint16_t x;
	uint16_t y;
} pixel_coord_t;

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

/*
 * Image IO
 */
#define DEFAULT_OUTPUT_DIR "output"
image_t* image_load(
	const char* filename,
	const enum CHANNELS channel
);

errno_t image_save_jpg(
	const char* filename,		// filename of saved output is the same
	const char* output_dir,
	const image_t* img,
	const bool enable_print_save
);

image_t* image_create(
	const uint16_t width,
	const uint16_t height,
	const enum CHANNELS channel
);

errno_t image_free(
	image_t* img
);

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
 * @brief accepts 0 - 255 colors and packs into single uint32_t
 */
#define COLOR_RGBA_ENCODE(r, g, b, a) \
	(((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a))

/**
 * @brief accepts 0 - 255 colors and packs into single uint32_t with maximum alpha channel
 */
#define COLOR_RGB_ENCODE(r, g, b) COLOR_RGBA_ENCODE(r, g, b, 255)

#define COLOR_R_DECODE(color) (((color) >> 24) & 0xFF)
#define COLOR_G_DECODE(color) (((color) >> 16) & 0xFF)
#define COLOR_B_DECODE(color) (((color) >> 8)  & 0xFF)
#define COLOR_A_DECODE(color) ((color)         & 0xFF)

/**
 * @brief locate_keypoints_on_img exactly
 * 
 * @param image image_t* would be nice if grey
 * @param keypoints vector* <pixel_coord_t> with information
 * @param brightness_coef uint8_t	0 - MAX_DIM_COEF brightness lvl for original pixels
 * @param color uint32_t in case channel GRAY: alfa is used as brightness lvl
 * @param is_img_empty bool in case there is no sense to dim, just optimization
 */
errno_t locate_keypoints_on_img(
	image_t *img,
	const vector_t *keypoints,
	const uint8_t brightness_coef,
	const uint32_t color,
	const bool is_img_empty
);

errno_t locate_single_point_on_img(
	image_t *img,
	const pixel_coord_t pixel_coord,
	const uint32_t color,
	const uint16_t radius_px
);

/**
 * @brief Keypoints searching algorithm, took from habr and rewrote
 */
vector_t* fast9(
	const image_t *gimg,
	const uint8_t threshold
);

#endif
