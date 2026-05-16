#include "img_proc.h"
#define STB_IMAGE_IMPLEMENTATION
#include "foreign/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "foreign/stb_image_write.h"

#include "img_proc.h"

#define TAG "img_io "

image_t* image_load(
	const char* filename,
	enum CHANNELS channel
) {
	image_t* img = malloc(sizeof(image_t));
	img->pixels = stbi_load(filename, (int*)&img->width, (int*)&img->height, (int*)&img->channel, channel);
	img->channel = channel;

	if (!img->pixels) {
		ddloge(TAG, "stbi_load failed");
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


errno_t image_save_png(
	const char* filepath,
	const image_t* img
) {
	if (!img || !img->pixels || !filepath) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}

	const char *last_slash = strrchr(filepath, '/');
	const char *pure_filename;

	if (last_slash != NULL)
		pure_filename = last_slash + 1;
	else
		pure_filename = filepath;

	size_t needed_size = strlen("output/") + strlen(pure_filename) + 1;
	char custom_file_path[needed_size];
	snprintf(custom_file_path, needed_size, "output/%s", pure_filename);

	/* 0 at the end — is stride (automatic width * channel) */
	if (stbi_write_png(custom_file_path, img->width, img->height, img->channel, img->pixels, 0)) {
		ddlogi(TAG, "saved to \033[0;32m%s\033[0;0m", custom_file_path);
		return OK;
	}
	ddloge(TAG, "couldn't stbi_write_png %s", custom_file_path);
	return -1;
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