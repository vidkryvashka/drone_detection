#ifndef IMG_IO_H
#define IMG_IO_H

#include "defs.h"
#include "img_defs.h"


#define DEAFAULT_DIM_COEF 3
#define MAX_DIM_COEF 12


/**
 * @brief load image and write its sise to global config
 */
image_t* image_load(
	const char *input_filepath,
	const enum CHANNELS channel
);

errno_t image_save_jpg(
	const char* filename,		// filename of saved output is the same
	const char* output_dir,
	const image_t* img,
	const bool enable_print_save
);

errno_t image_gray_to_rgb(
	image_t *img
);




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
 * @param dim_coef uint8_t	0 - MAX_DIM_COEF brightness lvl for original pixels
 * @param color uint32_t in case channel GRAY: alfa is used as brightness lvl
 * @param is_img_empty bool in case there is no sense to dim, just optimization
 */
errno_t locate_keypoints_on_img(
	image_t *img,
	const vector_t *keypoints,
	const uint8_t dim_coef,
	const uint32_t color,
	const bool is_img_empty
);

errno_t locate_single_point_on_img(
	image_t *img,
	const pixel_coord_t pixel_coord,
	const uint32_t color,
	const uint16_t radius_px
);

errno_t locate_clusters_on_img(
	image_t *img,
	const vector_t *keypoints,
	const uint8_t *clusters_indexes,
	const pixel_coord_t *clusters_centers,
	const size_t clusters_count,
	const uint8_t dim_coef,
	const bool is_img_empty
);

#endif
