#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include "defs.h"
#include "img_proc.h"
#include "my_vector.h"


#define TAG "my_io_flow "



static errno_t process_one_image(
	const config_t *conf
) {
	image_t* img = image_load(conf->input_filepath, GRAY);
	if (!img) {
		ddloge(TAG, "could not image_load %s", conf->input_filepath);
		return EINVAL;
	}
	vector_t *kpts = fast9(img, conf->fast9_threshold);
	locate_keypoints_on_gray_img(img, kpts, conf->dim_coef, 0);

	if (image_save_jpg(conf->input_filepath, conf->output_dir, img, 1) != OK)
		ddloge(TAG, "Failed to save image");
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
		if (str) {
			printf("%s ", str->name);
		}
	}
	printf("\n");
}


/**
 * @brievs allocs filenames vector in img_dir
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
	vector_t *filenames = vector_create(sizeof(str_t));
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


static void print_progress_bar(
	const char *prefix,
	size_t current,
	size_t total,
	const size_t bar_width
) {
	if (total == 0)
		return;
	
	int percentage = (int)(current * 100 / total);
	int filled = (int)(current * bar_width / total);

	printf("\r%s:\t[", prefix);
	for (int j = 0; j < bar_width; j++) {
		if (j < filled) printf("#");
		else printf(" ");
	}
	printf("] %d%% (%zu/%zu)", percentage, current, total);
	fflush(stdout);
}

/**
 * @brief safe bar interrupt to print error
 */
static void progress_bar_interrupt(void) {
	// \r returns to start, gaps "erase" old bar
	printf("\r%80s\r", ""); 
}


static vector_t *generate_frames_keypoints(
	const vector_t *filenames,
	config_t *conf
) {
	vector_t *frames_kpts = vector_create(sizeof(vector_t*));
	if (!frames_kpts) {
		ddloge(TAG, "couldn't vector_create");
		return NULL;
	}

	size_t total_files = filenames->size;
	clock_t start = clock();

	for (size_t i = 0; i < total_files; i++) {
		print_progress_bar(__func__, i + 1, total_files, 50);

		str_t *filename = (str_t*)vector_get(filenames, i);
		if (filename) {
			char imgpath[STR_MAX_LEN + 1];
			snprintf(imgpath, sizeof(imgpath), "%s/%s", conf->input_img_dir , filename->name);
			
			image_t* img = image_load(imgpath, GRAY);
			if (!img) {
				progress_bar_interrupt();
				ddloge(TAG, "could not image_load %s", imgpath);
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
	printf("\n");

	clock_t end = clock();
	double cpu_time_used_ms = ((double) (end - start)) / CLOCKS_PER_SEC * 1000;
	ddlogw(TAG, "took %.3f ms", cpu_time_used_ms);
	
	return frames_kpts;
}


static errno_t save_frames_keypoints_to_imgs_old(
	const vector_t *frames_kpts,
	const config_t *conf
) {
	if (!frames_kpts || !conf || !conf->output_dir[0]) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}
	for (size_t i = 0; i < frames_kpts->size; i++) {
		char new_filename[STR_MAX_LEN + 1];
		snprintf(new_filename, sizeof(new_filename), "%zu.jpg", i + 1);
		
		image_t *img = image_create(conf->frame_width, conf->frame_height, GRAY);
		vector_t *kpts = *(vector_t**)vector_get(frames_kpts, i);
		locate_keypoints_on_gray_img(img, kpts, 0, 1);
		image_save_jpg(new_filename, conf->output_dir, img, 0);
	}
	return OK; // I hope
}

static errno_t save_frames_keypoints_to_imgs(
	const vector_t *frames_kpts,
	const config_t *conf
) {
	if (!frames_kpts || !conf || !conf->output_dir[0]) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}

	size_t total_frames = frames_kpts->size;

	for (size_t i = 0; i < total_frames; i++) {
		print_progress_bar(__func__, i + 1, total_frames, 50);

		char new_filename[STR_MAX_LEN + 1];
		snprintf(new_filename, sizeof(new_filename), "%zu.jpg", i + 1);
		
		image_t *img = image_create(conf->frame_width, conf->frame_height, GRAY);
		if (!img) {
			progress_bar_interrupt(); // <--- Захист бару від зсуву!
			ddloge(TAG, "failed to create image for frame %zu", i + 1);
			continue;
		}
		vector_t *kpts = *(vector_t**)vector_get(frames_kpts, i);
		if (kpts) {
			locate_keypoints_on_gray_img(img, kpts, 0, 1);
		}
		image_save_jpg(new_filename, conf->output_dir, img, 0);
		image_free(img); 
	}
	printf("\n");
	return OK;	// hope so
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
		ddloge(TAG, "couldn't get_filenames_from_dir");
		return EIO;
	}

	// vector_print_strs(filenames);
	vector_t *frames_kpts = generate_frames_keypoints(filenames, conf);
	save_frames_keypoints_to_imgs(frames_kpts, conf);

	vector_destroy(filenames);
	for (size_t i = 0; i < frames_kpts->size; i++) {
		vector_t *kpts = *(vector_t**)vector_get(frames_kpts, i);
		vector_destroy(kpts);
	}
	vector_destroy(frames_kpts);
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
	clock_t start, end;
	double cpu_time_used_ms;
	start = clock();

	switch (conf->io_mode) {
	case not_selected:
		ddloge(TAG, "io_mode not selected");
		return EINVAL;
	case single_img_file:
		err = process_one_image(conf);
		break;
	case input_img_dir:
		err = process_img_dir(conf);
		break;
	}

	end = clock();
	cpu_time_used_ms = ((double) (end - start)) / CLOCKS_PER_SEC * 1000;
	ddlogw(TAG, "took %.3f ms", cpu_time_used_ms);

	return err;
}
