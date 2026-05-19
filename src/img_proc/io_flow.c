#include <stdio.h>
#include <sys/_types/_errno_t.h>
#include <time.h>
#include <dirent.h>
#include <string.h>

#include "defs.h"
#include "img_defs.h"
#include "img_io.h"
#include "vision.h"
#include "my_vector.h"


#define TAG "io_flow "


/**
 * @brief function for test and as example for fuhrther video stream implementation
 */
static errno_t primitive_process_one_image(
	config_t *conf
) {
	image_t* img = image_load(conf->input_filepath, GRAY);
	if (!img) {
		ddloge(TAG, "could not image_load %s", conf->input_filepath);
		return EINVAL;
	}
	conf->frame_width = img->width;
	conf->frame_height = img->height;

	vector_t *kpts = fast9(img, conf->fast9_threshold);

	if (image_gray_to_rgb(img) != OK) {
		ddloge(TAG, "failed to convert image to RGB");
		vector_destroy(kpts);
	}

	clusters_context_t cctx = dbscan(kpts, DBSCAN_MIN_CLUSTER_SIZE, conf);
	locate_clusters_on_img(img, kpts, cctx.ids, cctx.centers, cctx.unique_count, conf->dim_coef, 0);

	if (image_save_jpg(conf->input_filepath, conf->output_dir, img, 1) != OK)
		ddloge(TAG, "failed to save image");

	free(cctx.ids);
	free(cctx.centers);
	vector_destroy(kpts);
	image_free(img);

	return OK;
}


static bool is_numbered_img_file(
	const char *filename
) {
	if (!filename)
		return false;

	int num;
	int chars_consumed = 0;

	int parsed_jpg = sscanf(filename, "%d.jpg%n", &num, &chars_consumed);
	int parsed_png = sscanf(filename, "%d.png%n", &num, &chars_consumed);

	if (parsed_jpg || parsed_png)
		if (chars_consumed == strlen(filename))
			return true;

	return false;
}


static int compare_string_pointers(const void *a, const void *b) {
	const str_t *file_a = (const str_t *)a;
	const str_t *file_b = (const str_t *)b;

	int num_a = atoi(file_a->name);
	int num_b = atoi(file_b->name);

	return num_a - num_b;
}


static void vector_print_strs(vector_t *vec) {
	ddlogi(TAG, "size %zu:", vec->size);
	for (size_t i = 0; i < vec->size; i++) {
		str_t *str = (str_t*)vector_get(vec, i);
		if (str)
			printf("%s ", str->name);
	}
	printf("\n");
}


/**
 * @brief allocs filenames str_t vector from img_dir
 */
static vector_t *get_filenames_from_dir(
	const char *img_dir
) {
	DIR *dir = opendir(img_dir);
	if (!dir) {
		ddloge(TAG, "couldn't open %s", img_dir);
		return NULL;
	}
	struct dirent *de;
	vector_t *filenames = vector_create(VECTOR_DEFAULT_INIT_CAPACITY, sizeof(str_t));
	if (!filenames) {
		ddloge(TAG, "couldn't vector_create");
		goto gffd_cleanup_error;
	}

	while ((de = readdir(dir)))
		if (is_numbered_img_file(de->d_name)) {
			str_t f_item;
			snprintf(f_item.name, sizeof(f_item.name), "%s", de->d_name);
			if (vector_push_back(filenames, &f_item) != OK)
				goto gffd_cleanup_error;
		}
	closedir(dir);

	if (filenames && filenames->size > 2 && filenames->data)
		qsort(
			filenames->data,
			filenames->size,
			filenames->sizeof_element,
			compare_string_pointers
		);
	else
		ddlogw(TAG, "didn't qsort filenames");

	return filenames;

gffd_cleanup_error:
	closedir(dir);
	vector_destroy(filenames);
	return NULL;
}


static vector_t *generate_frames_keypoints(
	const vector_t *filenames,
	config_t *conf
) {
	vector_t *frames_kpts = vector_create(filenames->size, sizeof(vector_t*));
	if (!frames_kpts) {
		ddloge(TAG, "couldn't vector_create");
		return NULL;
	}

	clock_t start = clock();
	for (size_t i = 0; i < filenames->size; i++) {
		print_progress_bar(__func__, i + 1, filenames->size);
		str_t *filename = (str_t*)vector_get(filenames, i);
		if (filename) {
			char imgpath[STR_MAX_LEN + 1];
			snprintf(imgpath, sizeof(imgpath), "%s/%s", conf->input_img_dir , filename->name);
			
			image_t* img = image_load(imgpath, GRAY);
			if (!img) {
				progress_bar_interrupt();
				ddloge(TAG, "could not image_load %s", imgpath);
				vector_destroy(frames_kpts);
				return NULL;
			} else {
				if (!conf->frame_width || !conf->frame_height) {
					conf->frame_width = img->width;
					conf->frame_height = img->height;
				}
				vector_t *kpts = fast9(img, conf->fast9_threshold);
				if (kpts) {
					vector_push_back(frames_kpts, &kpts);
				}
				image_free(img);
			}
		}
	}
	double cpu_time_used_ms = ((double) (clock() - start)) / CLOCKS_PER_SEC * 1000;
	printf(" %.0f ms\n", cpu_time_used_ms);
	
	return frames_kpts;
}


static errno_t save_frames_keypoints_to_imgs(
	const vector_t *frames_kpts,
	const config_t *conf
) {
	if (!frames_kpts || !conf || !conf->output_dir[0]) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}

	clock_t start = clock();
	for (size_t i = 0; i < frames_kpts->size; i++) {
		print_progress_bar(__func__, i + 1, frames_kpts->size);

		char new_filename[STR_MAX_LEN + 1];
		snprintf(new_filename, sizeof(new_filename), "%zu.jpg", i + 1);
		
		image_t *img = image_create(conf->frame_width, conf->frame_height, RGB);
		if (!img) {
			progress_bar_interrupt();
			ddloge(TAG, "failed to create image for frame %zu", i + 1);
			return -1;
		}
		vector_t *kpts = *(vector_t**)vector_get(frames_kpts, i);
		
		if (kpts) {
			clusters_context_t cctx = dbscan(kpts, DBSCAN_MIN_CLUSTER_SIZE, conf);
			locate_clusters_on_img(img, kpts, cctx.ids, cctx.centers, cctx.unique_count, 0, 1);
			// locate_keypoints_on_img(img, kpts, 0, 255, 1);
		}
		image_save_jpg(new_filename, conf->output_dir, img, 0);
		image_free(img);
	}
	double cpu_time_used_ms = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000;
	printf(" %.0f ms\n", cpu_time_used_ms);
	return OK;	// hope so
}


static errno_t images_to_video(
	const char *output_img_dir,
	const char *output_dir
) {
	if (!output_img_dir || !output_dir) {
		return EINVAL;
	}

	char cmd[STR_MAX_LEN * 3 + 1];
	
	int written = snprintf(cmd, sizeof(cmd), 
		"ffmpeg -framerate 24 -i \"%s/%%d.jpg\" -c:v libx264 -pix_fmt yuv420p \"%s/dildo.mp4\" -y", 
		output_img_dir, output_dir);

	if (written < 0 || (size_t)written >= sizeof(cmd)) {
		ddloge(TAG, "command buffer overflowed");
		return ENOMEM;
	}
	
	if (system(cmd) == -1) {
		ddloge(TAG, "failed to execute ffmpeg command");
		return EIO;
	}

	return OK;
}


static errno_t process_img_dir(
	config_t *conf
) {
	if (!conf || !conf->input_img_dir[0]) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}
	vector_t *filenames = get_filenames_from_dir(conf->input_img_dir);
	if (!filenames) {
		ddloge(TAG, "couldn't get_filenames_from_dir %s", conf->input_img_dir);
		return EIO;
	}

	vector_t *frames_kpts = generate_frames_keypoints(filenames, conf);
	if (!frames_kpts)
		return -1;
	save_frames_keypoints_to_imgs(frames_kpts, conf);

	images_to_video(conf->output_dir, DEFAULT_OUTPUT_DIR);

	vector_destroy(filenames);
	for (size_t i = 0; i < frames_kpts->size; i++) {
		vector_t *kpts = *(vector_t**)vector_get(frames_kpts, i);
		vector_destroy(kpts);
	}
	return OK;	// hope so
}


errno_t apply_io_mode(
	config_t *conf
) {
	if (!conf) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}

	errno_t err;
	clock_t start = clock();

	switch (conf->io_mode) {
	case not_selected:
		ddloge(TAG, "io_mode not selected");
		return EINVAL;
	case single_img_file:
		err = primitive_process_one_image(conf);
		break;
	case input_img_dir:
		err = process_img_dir(conf);
		break;
	}

	double cpu_time_used_ms = ((double) (clock() - start)) / CLOCKS_PER_SEC * 1000;
	ddlogi(TAG, "took %.3f ms", cpu_time_used_ms);

	return err;
}
