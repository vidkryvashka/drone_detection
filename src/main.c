#include <stdio.h>
#include "img_io.h"
#include "my_vector.h"


#define TAG "my_main "

static void process_pixels(image_t *img, vector_t *keypoints) {
	for (size_t i = 0; i < img->width * img->height; i++) {
		img->pixels[i] = (uint8_t)(img->pixels[i] * 0.5f);
	}

	for (size_t i = 0; i < keypoints->size; i++) {
		pixel_coord_t* p = vector_get(keypoints, i);
		if (p->x < img->width && p->y < img->height) {
			img->pixels[p->y * img->width + p->x] = 255;
		}
	}
}


static vector_t *paint_diagonal_points_test(image_t *img) {
	vector_t* pts = vector_create(sizeof(pixel_coord_t));
	if (!pts)
		return NULL;
	for (uint16_t i = 0; i < 100; i++) {
		pixel_coord_t p = {i * 5, i * 5};
		vector_push_back(pts, &p);
	}
	return pts;
}


int main(int argc, char **argv) {
	if (argc < 2) {
		ddlogi(TAG, "Usage: %s <input.png>", argv[0]);
		return EINVAL;
	}

	image_t* img = image_load_gray(argv[1]);
	if (!img) {
		fprintf(stderr, "Could not load image\n");
		return EINVAL;
	}

	// vector_t *pts = paint_diagonal_points_test(img);
	vector_t *pts = fast9(img, 70);
	process_pixels(img, pts);

	if (image_save_png("output.png", img) == OK) {
		printf("Success! Saved to output.png\n");
	} else {
		printf("Failed to save image\n");
	}

	vector_destroy(pts);
	image_free(img);

	return 0;
}