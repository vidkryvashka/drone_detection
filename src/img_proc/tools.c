#include "defs.h"
#include "img_proc.h"

#define TAG "img_proc_tools"

errno_t place_points_on_img(
	image_t *img,
	vector_t *keypoints,
	uint8_t brightness_coef		// 0 - 8 brightness lvl for original pixels
) {
	if (!img || !img->pixels || !keypoints) {
		ddloge(TAG, "invalid args");
		return EINVAL;
	}

	if (brightness_coef >= 8)
		ddlogw(TAG, "brightness_coef >= 8 (%d)", brightness_coef);
	else
		for (size_t i = 0; i < img->width * img->height; i++)
			img->pixels[i] = (uint8_t)(img->pixels[i] * brightness_coef / 8);

	for (size_t i = 0; i < keypoints->size; i++) {
		pixel_coord_t* p = vector_get(keypoints, i);
		if (p->x < img->width && p->y < img->height)
			img->pixels[p->y * img->width + p->x] = 255;
	}
	return OK;
}

// vector_t *test_paint_diagonal_points_test(
// 	image_t *img
// ) {
// 	vector_t* pts = vector_create(sizeof(pixel_coord_t));
// 	if (!pts)
// 		return NULL;
// 	for (uint16_t i = 0; i < 100; i++) {
// 		pixel_coord_t p = {i * 5, i * 5};
// 		vector_push_back(pts, &p);
// 	}
// 	return pts;
// }
