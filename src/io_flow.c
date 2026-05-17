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
	vector_t *pts = fast9(img, conf->fast9_threshold);

	if (image_save_jpg(conf->input_filepath, conf->output_dir, img) == OK) {
		// ddlogi(TAG, "\033[0;32mSuccess!\033[0;0m");
	} else {
		ddloge(TAG, "Failed to save image");
	}
	vector_destroy(pts);
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

	// %n - records how many chars did it pass
	int parsed_jpg = sscanf(filename, "%d.jpg%n", &num, &chars_consumed);
	int parsed_png = sscanf(filename, "%d.png%n", &num, &chars_consumed);

	if (parsed_jpg || parsed_png)
		if (chars_consumed == strlen(filename))
			return true;

	return false;
}


static int compare_string_pointers(const void *a, const void *b) {
	const char *str_a = *(const char **)a;
	const char *str_b = *(const char **)b;
	
	int num_a = atoi(str_a);
	int num_b = atoi(str_b);

	return num_a - num_b;
}


static void vector_print_strs(vector_t *vec) {
	ddlogi(TAG, "size %zu :", vec->size);
	for (size_t i = 0; i < vec->size; i++) {
		char **str_ptr = (char**)vector_get(vec, i);
		if (str_ptr && *str_ptr) {
			printf("%s ", *str_ptr);
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
	vector_t *filenames = vector_create(sizeof(char*));
	if (!filenames) {
		closedir(dir);
		ddloge(TAG, "couldn't vector_create");
		return NULL;
	}

	while ((de = readdir(dir))) {
		if (is_numbered_img_file(de->d_name)) {
			char *name_copy = strdup(de->d_name);
			if (!name_copy)
				goto cleanup_error;
			if (vector_push_back(filenames, &name_copy) != OK) {
				free(name_copy);
				ddloge(TAG, "couldn't vector_push_back");
				goto cleanup_error;
			}
		}
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

cleanup_error:
	closedir(dir);
	for (size_t i = 0; i < filenames->size; i++) {
		char **str_ptr = (char**)vector_get(filenames, i);
		if (str_ptr && *str_ptr)
			free(*str_ptr);
	}
	vector_destroy(filenames);
	return NULL;
}


static errno_t process_img_dir(
	const config_t *conf
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

	vector_print_strs(filenames);
	// here processing imgages

	// clearing double pointers inside vector
	for (size_t i = 0; i < filenames->size; i++) {
		char **str_ptr = (char**)vector_get(filenames, i);
		if (str_ptr && *str_ptr) {
			free(*str_ptr);
		}
	}
	vector_destroy(filenames);
	return OK;
}


errno_t apply_io_mode(
	const config_t *conf
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
