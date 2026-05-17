#include "img_proc.h"
#define STB_IMAGE_IMPLEMENTATION
#include "foreign/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "foreign/stb_image_write.h"

#include "img_proc.h"

#define TAG "img_io "

image_t* image_load(
	const char* filepath,
	enum CHANNELS channel
) {
	image_t* img = malloc(sizeof(image_t));
	img->pixels = stbi_load(filepath, (int*)&img->width, (int*)&img->height, (int*)&img->channel, channel);
	img->channel = channel;

	if (!img->pixels) {
		ddloge(TAG, "stbi_load %s failed", filepath);
		free(img);
		return NULL;
	}
	return img;
}


image_t* image_create(
	uint16_t width,
	uint16_t height,
	enum CHANNELS channel
) {
	image_t* img = (image_t*)malloc(sizeof(image_t));
	if (!img) {
		ddloge(TAG, "malloc image_t failed");
		return NULL;
	}

	img->width = width;
	img->height = height;
	img->channel = channel;

	size_t buffer_size = (size_t)width * height * channel;

	img->pixels = (uint8_t*)calloc(buffer_size, sizeof(uint8_t));

	if (!img->pixels) {
		ddloge(TAG, "pixels calloc failed");
		free(img);
		return NULL;
	}

	return img;
}


errno_t image_save_jpg(
	const char* input_filepath,
	const char* output_dir,
	const image_t* img,
	const bool enable_print_save
) {
	if (!img || !img->pixels || !input_filepath || !output_dir) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}

	const char *last_slash = strrchr(input_filepath, '/');
	const char *pure_filename = (last_slash != NULL) ? (last_slash + 1) : input_filepath;

	size_t dir_len = strlen(output_dir);
	const char *separator = "";
	
	if (dir_len > 0 && output_dir[dir_len - 1] != '/')
		separator = "/";

	size_t needed_size = dir_len + strlen(separator) + strlen(pure_filename) + 1;
	
	char custom_file_path[needed_size];
	snprintf(custom_file_path, needed_size, "%s%s%s", output_dir, separator, pure_filename);

	if (stbi_write_jpg(custom_file_path, img->width, img->height, img->channel, img->pixels, 0)) {
		if (enable_print_save)
			ddlogi(TAG, "saved to \033[0;32m%s\033[0;0m", custom_file_path);
		return OK;
	}

	ddloge(TAG, "couldn't stbi_write_jpg %s", custom_file_path);
	return EIO;
}



errno_t image_free(
	image_t* img
) {
	if (img) {
		stbi_image_free(img->pixels);
		free(img);
		return OK;
	} else
		return EINVAL;
}