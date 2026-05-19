#include <stdbool.h>
#include <stdint.h>
#include "img_defs.h"

#define STB_IMAGE_IMPLEMENTATION
#include "foreign/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "foreign/stb_image_write.h"

#include "my_vector.h"
#include "img_defs.h"
#include "img_io.h"
#include "vision.h"

#define TAG "img_io "


image_t* image_load(
	const char *input_filepath,
	const enum CHANNELS channel
) {
	image_t* img = malloc(sizeof(image_t));
	img->pixels = stbi_load(input_filepath, (int*)&img->width, (int*)&img->height, (int*)&img->channel, channel);
	img->channel = channel;

	if (!img->pixels) {
		ddloge(TAG, "stbi_load %s failed", input_filepath);
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


errno_t image_gray_to_rgb(
	image_t *img
) {
	if (!img || !img->pixels) {
		ddloge(TAG, "invalid image for conversion");
		return EINVAL;
	}

	if (img->channel == RGB || img->channel == RGBA) {
		return OK; 
	}

	if (img->channel != GRAY) {
		ddloge(TAG, "unsupported source channel count: %d", img->channel);
		return ENOTSUP;
	}

	size_t pixel_count = (size_t)img->width * img->height;
	size_t new_buffer_size = pixel_count * RGB;

	uint8_t *rgb_pixels = (uint8_t *)malloc(new_buffer_size);
	if (!rgb_pixels) {
		ddloge(TAG, "malloc failed for GRAY->RGB conversion");
		return ENOMEM;
	}

	for (size_t i = 0; i < pixel_count; i++) {
		uint8_t gray_val = img->pixels[i];
		
		rgb_pixels[i * RGB + 0] = gray_val;
		rgb_pixels[i * RGB + 1] = gray_val;
		rgb_pixels[i * RGB + 2] = gray_val;
	}

	stbi_image_free(img->pixels);

	img->pixels = rgb_pixels;
	img->channel = RGB;

	return OK;
}


static void dim_img(
	image_t *img,
	const uint8_t dim_coef
) {
	if (dim_coef >= MAX_DIM_COEF) {
		ddlogw(TAG, "dim_coef >= 8 (%d)", dim_coef);
	} else {
		size_t total_bytes = (size_t)img->width * img->height * img->channel;
		for (size_t i = 0; i < total_bytes; i++)
			img->pixels[i] = (uint8_t)((int)img->pixels[i] * dim_coef / MAX_DIM_COEF);
	}
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

	if (!is_img_empty)
		dim_img(img, dim_coef);

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

	int16_t cx = (int16_t)pixel_coord.x;
	int16_t cy = (int16_t)pixel_coord.y;
	uint16_t r_squared = radius_px * radius_px;

	for (int16_t dy = -radius_px; dy <= radius_px; dy++) {
		for (int16_t dx = -radius_px; dx <= radius_px; dx++) {
			if (dx * dx + dy * dy < r_squared) {
				int16_t px = cx + dx;
				int16_t py = cy + dy;
				if (px >= 0 && px < (int16_t)img->width && py >= 0 && py < (int16_t)img->height) {
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


static uint32_t generate_cluster_color(uint8_t cluster_id) {
	if (cluster_id == DBSCAN_NOISE) {
		// Keep noise dim
		return COLOR_RGB_ENCODE(80, 80, 80); 
	}

	static const uint32_t bright_palette[] = {
		COLOR_RGB_ENCODE(255, 0, 50),    // 0: Неоновий Червоний (ближче до карміну, ультра-видимий)
		COLOR_RGB_ENCODE(0, 255, 0),     // 1: Яскравий Лайм (максимальна чутливість ока)
		COLOR_RGB_ENCODE(100, 180, 255), // 2: ВИПРАВЛЕНО: Електрик Синій (замість темного синього — яскравий неоновий блакить)
		COLOR_RGB_ENCODE(255, 255, 0),   // 3: Чистий Жовтий (кислотний)
		COLOR_RGB_ENCODE(255, 0, 255),   // 4: Маджента / Фуксія
		COLOR_RGB_ENCODE(0, 255, 255),   // 5: Яскравий Ціан / Бірюза
		COLOR_RGB_ENCODE(255, 140, 0),   // 6: Вогняний Помаранчевий
		COLOR_RGB_ENCODE(210, 100, 255), // 7: ВИПРАВЛЕНО: Світло-Фіолетовий / Яскравий Бузковий (замість темного)
		COLOR_RGB_ENCODE(0, 255, 140),   // 8: Неонова М'ята
		COLOR_RGB_ENCODE(255, 50, 150),  // 9: Яскравий Хот-Пінк
		COLOR_RGB_ENCODE(170, 255, 0),   // 10: Кислотний Шартрез
		COLOR_RGB_ENCODE(190, 255, 190)  // 11: МОДИФІКОВАНО: Світло-салатовий неоновий (краще за білий, не плутається із бліками)
	};

	size_t palette_size = sizeof(bright_palette) / sizeof(bright_palette[0]);

	if (cluster_id < palette_size) {
		return bright_palette[cluster_id];
	}

	// shift the shades around the circle so that every next 12 clusters do not copy the previous ones exactly.
	uint32_t base_color = bright_palette[cluster_id % palette_size];
	
	uint8_t r = COLOR_R_DECODE(base_color);
	uint8_t g = COLOR_G_DECODE(base_color);
	uint8_t b = COLOR_B_DECODE(base_color);

	if (r < 130) r = 130 + (cluster_id * 13) % 115;
	if (g < 130) g = 130 + (cluster_id * 23) % 115;
	if (b < 130) b = 130 + (cluster_id * 33) % 115;

	return COLOR_RGB_ENCODE(r, g, b);
}


errno_t locate_clusters_on_img(
	image_t *img,
	const vector_t *keypoints,
	const uint8_t *clusters_indexes,
	const pixel_coord_t *clusters_centers,
	const size_t clusters_count,
	const uint8_t dim_coef,
	const bool is_img_empty
) {
	if (!img || !keypoints || !clusters_indexes) {
		return EINVAL;
	}

	if (!is_img_empty)
		dim_img(img, dim_coef);

	for (size_t i = 0; i < keypoints->size; i++) {
		pixel_coord_t *point = (pixel_coord_t *)vector_get(keypoints, i);
		if (!point) continue;

		uint8_t cluster_id = clusters_indexes[i];
		uint32_t color = generate_cluster_color(cluster_id);

		locate_single_point_on_img(img, *point, color, 1);
	}

	for (uint8_t i = 0; i < clusters_count; i++) {
		if (clusters_centers && (clusters_centers[i].x != 0 || clusters_centers[i].y != 0)) {
			uint32_t center_marker_color = COLOR_RGB_ENCODE(255, 255, 0);
			locate_single_point_on_img(img, clusters_centers[i], center_marker_color, 3);
		}
	}

	return OK;
}
