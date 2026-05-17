#include "defs.h"
#include "img_proc.h"

#define TAG "img_proc_tools"

errno_t locate_keypoints_on_gray_img(
	image_t *img,
	const vector_t *keypoints,
	const uint8_t dim_coef,
	const bool is_img_empty
) {
	if (!img || !img->pixels || !keypoints || img->channel != GRAY) {
		ddloge(TAG, "invalid args");
		return EINVAL;
	}

	if (is_img_empty)
		goto skip_dim;

	if (dim_coef >= MAX_DIM_COEF)
		ddlogw(TAG, "dim_coef >= 8 (%d)", dim_coef);
	else
		for (size_t i = 0; i < img->width * img->height; i++)
			img->pixels[i] = (uint8_t)(img->pixels[i] * dim_coef / MAX_DIM_COEF);
skip_dim:

	for (size_t i = 0; i < keypoints->size; i++) {
		pixel_coord_t* p = vector_get(keypoints, i);
		if (p->x < img->width && p->y < img->height)
			img->pixels[p->y * img->width + p->x] = 255;
	}
	return OK;
}
