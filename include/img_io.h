#ifndef IMG_IO_H
#define IMG_IO_H

#include "defs.h"
#include "my_vector.h"


#define DEFAULT_DIM_COEF 2
#define MAX_DIM_COEF 16

typedef struct {
	char input_filepath[STR_MAX_LEN + 1];
	char input_img_dir[STR_MAX_LEN + 1];
	char output_dir[STR_MAX_LEN + 1];
	uint8_t dim_coef; // (0 - MAX_DIM_COEF) where 0 is black original image with painted metadata only. applies while saving image
	enum IO_MODES io_mode;
} img_io_conf_t;


/**
 * @brief load image and write its sise to global config
 */
image_t* image_load(
	const char *input_filepath,
	const enum CHANNELS channel
);

image_t* image_create(
	const uint16_t width,
	const uint16_t height,
	const enum CHANNELS channel
);

/**
 * @brief allocs filenames str_t vector from img_dir
 */
vector_t *get_filepathes_from_dir(
	const char *img_dir
);

errno_t image_save_jpg(
	const char* input_filepath,		// filename of saved output must be the same
	const char* output_dir,
	const image_t* img,
	const uint8_t dbg_lvl
);

errno_t image_free(
	image_t* img
);

errno_t image_gray_to_rgb(
	image_t *img
);

/**
 * @brief call ffmpeg
 */
errno_t images_to_video(
	const char *output_img_dir,
	const char *output_dir
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


inline void dim_img(
	image_t *img,
	const uint8_t dim_coef
) {
	size_t total_bytes = (size_t)img->width * img->height * img->channel;
	for (size_t i = 0; i < total_bytes; i++)
		img->pixels[i] = (uint8_t)((int)img->pixels[i] * dim_coef / MAX_DIM_COEF);
}

errno_t locate_keypoints_on_img(
	image_t *img,
	const vector_t *keypoints,
	const uint32_t color
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
	const void *cctx_vp,
	const bool enable_print_clusters
);


errno_t locate_tracks_on_img(
	image_t *img,
	const void *tracker_ctx_vp
);

#endif
