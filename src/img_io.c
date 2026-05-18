#include "img_proc.h"
#define STB_IMAGE_IMPLEMENTATION
#include "foreign/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "foreign/stb_image_write.h"

#include "img_proc.h"

#define TAG "img_io "

image_t* image_load(
	const char* filepath,
	const enum CHANNELS channel
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
	const uint16_t width,
	const uint16_t height,
	const enum CHANNELS channel
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


errno_t locate_keypoints_on_img_old(
	image_t *img,
	const vector_t *keypoints,
	const uint8_t dim_coef,
	const bool is_img_empty
) {
	if (!img || !img->pixels || !keypoints) {
		ddloge(TAG, "invalid args");
		return EINVAL;
	}

	if (is_img_empty)
		goto skip_dim;

	if (dim_coef >= MAX_DIM_COEF)
		ddlogw(TAG, "dim_coef >= 8 (%d)", dim_coef);
	else
		for (size_t i = 0; i < img->width * img->height; i++)
			img->pixels[i] = (uint8_t)(img->pixels[i] * dim_coef / MAX_DIM_COEF);
skip_dim:

	for (size_t i = 0; i < keypoints->size; i++) {
		pixel_coord_t* p = vector_get(keypoints, i);
		if (p->x < img->width && p->y < img->height)
			img->pixels[p->y * img->width + p->x] = 255;
	}
	return OK;
}


errno_t locate_keypoints_on_img(
	image_t *img,
	const vector_t *keypoints,
	const uint8_t dim_coef,
	const uint32_t color,
	const bool is_img_empty
) {
	if (!img || !img->pixels || !keypoints) {
		ddloge(TAG, "invalid args");
		return EINVAL;
	}

	if (is_img_empty)
		goto skip_dim;

	if (dim_coef >= MAX_DIM_COEF) {
		ddlogw(TAG, "dim_coef >= 8 (%d)", dim_coef);
	} else {
		size_t total_bytes = (size_t)img->width * img->height * img->channel;
		for (size_t i = 0; i < total_bytes; i++)
			img->pixels[i] = (uint8_t)((int)img->pixels[i] * dim_coef / MAX_DIM_COEF);
	}
skip_dim:

	for (size_t i = 0; i < keypoints->size; i++) {
		pixel_coord_t* p = vector_get(keypoints, i);
		if (!p)
			continue;
		if (p->x < img->width && p->y < img->height) {
			size_t base_idx = ((size_t)p->y * img->width + p->x) * img->channel;
			if (img->channel == GRAY)
				img->pixels[base_idx] = COLOR_A_DECODE(color);
			else if (img->channel == RGB || img->channel == RGBA) {
				img->pixels[base_idx + 0] = COLOR_R_DECODE(color);
				img->pixels[base_idx + 1] = COLOR_G_DECODE(color);
				img->pixels[base_idx + 2] = COLOR_B_DECODE(color);
				if (img->channel == RGBA)
					img->pixels[base_idx + 3] = COLOR_A_DECODE(color);
			}
		}
	}
	return OK;
}


errno_t locate_single_point_on_img(
	image_t *img,
	const pixel_coord_t pixel_coord,
	const uint32_t color,
	const uint16_t radius_px
) {
	if (!img || !img->pixels) {
		return EINVAL;
	}

	int32_t cx = (int32_t)pixel_coord.x;
	int32_t cy = (int32_t)pixel_coord.y;
	int32_t r_int = (int32_t)radius_px;
	int32_t r_squared = r_int * r_int;

	for (int32_t dy = -r_int; dy <= r_int; dy++) {
		for (int32_t dx = -r_int; dx <= r_int; dx++) {
			if (dx * dx + dy * dy <= r_squared) {
				int32_t px = cx + dx;
				int32_t py = cy + dy;
				if (px >= 0 && px < (int32_t)img->width && py >= 0 && py < (int32_t)img->height) {
					size_t base_idx = ((size_t)py * img->width + px) * img->channel;
					if (img->channel == GRAY)
						img->pixels[base_idx] = COLOR_A_DECODE(color);
					else if (img->channel == RGB || img->channel == RGBA) {
						img->pixels[base_idx + 0] = COLOR_R_DECODE(color);
						img->pixels[base_idx + 1] = COLOR_G_DECODE(color);
						img->pixels[base_idx + 2] = COLOR_B_DECODE(color);
						if (img->channel == RGBA)
							img->pixels[base_idx + 3] = COLOR_A_DECODE(color);
					}
				}
			}
		}
	}
	return OK;
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