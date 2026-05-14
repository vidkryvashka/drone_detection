#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "img_io.h"

#define TAG "img_io "

image_t* image_load_gray(const char* filename) {
	image_t* img = malloc(sizeof(image_t));
	img->pixels = stbi_load(filename, (int*)&img->width, (int*)&img->height, (int*)&img->channel, 1);
	img->channel = 1; 

	if (!img->pixels) {
		free(img);
		return NULL;
	}
	return img;
}


image_t* image_create(uint16_t width, uint16_t height, enum CHANNELS channel) {
	image_t* img = (image_t*)malloc(sizeof(image_t));
	if (!img) {
		perror(TAG "image_create: struct malloc failed");
		return NULL;
	}

	img->width = width;
	img->height = height;
	img->channel = channel;

	size_t buffer_size = (size_t)width * height * channel;

	img->pixels = (uint8_t*)calloc(buffer_size, sizeof(uint8_t));

	if (!img->pixels) {
		perror(TAG "image_create: pixels calloc failed");
		free(img);
		return NULL;
	}

	return img;
}


errno_t image_save_png(const char* filename, const image_t* img) {
	if (!img || !img->pixels) return EINVAL;
	/* 0 at the end — is stride (automatic width * channel) */
	if (stbi_write_png(filename, img->width, img->height, img->channel, img->pixels, 0)) {
		return OK;
	}
	printf(TAG "%s unexpected error", __func__);
	return -1;
}


errno_t image_free(image_t* img) {
	if (img) {
		stbi_image_free(img->pixels);
		free(img);
		return OK;
	} else
		return EINVAL;
}