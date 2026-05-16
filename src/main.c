#include <stdio.h>
#include "defs.h"
#include "img_proc.h"
#include "my_vector.h"


#define TAG "main "

int main(int argc, char **argv) {
	main_args_t main_args = {0};
	if (parse_main_args(argc, argv, &main_args)) {
		ddloge(TAG, "parse_main_args failed");
		return EINVAL;
	}

	image_t* img = image_load(main_args.input_filepath, GRAY);
	if (!img) {
		ddloge(TAG, "could not image_load %s", main_args.input_filepath);
		return EINVAL;
	}

	vector_t *pts = fast9(img, main_args.fast9_threshold);
	place_points_on_img(img, pts, main_args.dim_coef);

	if (image_save_jpg(main_args.input_filepath, main_args.output_dir, img) == OK) {
		// ddlogi(TAG, "\033[0;32mSuccess!\033[0;0m");
	} else {
		ddloge(TAG, "Failed to save image");
	}

	vector_destroy(pts);
	image_free(img);

	return 0;
}
