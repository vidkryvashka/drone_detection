#include <stdio.h>
#include "defs.h"
#include "img_proc.h"
#include "my_vector.h"


#define TAG "main "



int main(int argc, char **argv) {
	if (argc < 2) {
		ddlogi(TAG, "Usage: %s <input.png>", argv[0]);
		return EINVAL;
	}

	image_t* img = image_load(argv[1], GRAY);
	if (!img) {
		fprintf(stderr, "Could not load image\n");
		return EINVAL;
	}

	vector_t *pts = fast9(img, 70);
	place_points_on_img(img, pts, 3);

	if (image_save_png(argv[1], img) == OK) {
		ddlogi(TAG, "\033[0;32mSuccess!\033[0;0m");
	} else {
		ddloge(TAG, "Failed to save image");
	}

	vector_destroy(pts);
	image_free(img);

	return 0;
}
