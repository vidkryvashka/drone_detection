#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <assert.h>
#include "defs.h"

#define STB_IMAGE_IMPLEMENTATION
#include "foreign/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "foreign/stb_image_write.h"

#include "defs.h"
#include "my_vector.h"
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

vector_t *get_filepathes_from_dir(
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
		goto get_filepathes_from_dir_fail;
	}

	while ((de = readdir(dir)))
		if (is_numbered_img_file(de->d_name)) {
			str_t f_item;
			snprintf(f_item.name, sizeof(f_item.name), "%s", de->d_name);
			if (vector_push_back(filenames, &f_item) != OK)
				goto get_filepathes_from_dir_fail;
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
		ddloge(TAG, "didn't qsort filenames");

	return filenames;

get_filepathes_from_dir_fail:
	closedir(dir);
	vector_destroy(filenames);
	return NULL;
}


errno_t image_save_jpg(
	const char* input_filepath,
	const char* output_dir,
	const image_t* img,
	const uint8_t dbg_lvl
) {
	assert(img && img->pixels && input_filepath && output_dir);

	const char *last_slash = strrchr(input_filepath, '/');
	const char *pure_filename = (last_slash != NULL) ? (last_slash + 1) : input_filepath;

	size_t dir_len = strlen(output_dir);
	const char *separator = "";
	
	if (dir_len > 0 && output_dir[dir_len - 1] != '/')
		separator = "/";

	size_t needed_size = dir_len + strlen(separator) + strlen(pure_filename) + 1;
	
	char custom_output_file_path[needed_size];
	snprintf(custom_output_file_path, needed_size, "%s%s%s", output_dir, separator, pure_filename);

	if (stbi_write_jpg(custom_output_file_path, img->width, img->height, img->channel, img->pixels, 0)) {
		if (dbg_lvl >= 2)
			ddlogi(TAG, "saved to \033[0;32m%s\033[0;0m", custom_output_file_path);
		return OK;
	}

	ddloge(TAG, "couldn't stbi_write_jpg %s", custom_output_file_path);
	return EIO;
}


errno_t image_free(
	image_t* img
) {
	assert(img);
	stbi_image_free(img->pixels);
	free(img);
	return OK;
}


errno_t image_gray_to_rgb(
	image_t *img
) {
	assert(img && img->pixels);

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


errno_t images_to_video(
	const char *output_img_dir,
	const char *output_video_path
) {
	assert(output_img_dir && output_video_path);

	char cmd[STR_MAX_LEN * 3 + 1];
	int written = snprintf(cmd, sizeof(cmd), 
		"ffmpeg -framerate 24 -i \"%s/%%d.jpg\" -c:v libx264 -pix_fmt yuv420p %s -y -loglevel panic",
		output_img_dir, output_video_path);

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


pixel_coord_t get_label_drone_coord(
	const img_io_conf_t *iio_conf
) {
	pixel_coord_t coord = {.x = 0, .y = 0};

	if (!iio_conf || !iio_conf->input_filepath[0]) {
		return coord;
	}

	const char *last_slash = strrchr(iio_conf->input_filepath, '/');
	const char *pure_filename = (last_slash != NULL) ? (last_slash + 1) : iio_conf->input_filepath;

	int frame_num = 0;
	if (sscanf(pure_filename, "%d", &frame_num) != 1) {
		return coord;
	}

	char json_filepath[STR_MAX_LEN];
	snprintf(json_filepath, sizeof(json_filepath), "%s/labels/%d.json", iio_conf->input_img_dir, frame_num);

	FILE *file = fopen(json_filepath, "r");
	if (!file) {
		return coord; 
	}

	char buffer[4096];
	size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
	buffer[bytes_read] = '\0';
	fclose(file);

	if (!strstr(buffer, "\"title\": \"Drone\"") && !strstr(buffer, "\"title\":\"Drone\"")) {
		return coord;
	}

	char *x_ptr = strstr(buffer, "\"x\"");
	char *y_ptr = strstr(buffer, "\"y\"");
	char *w_ptr = strstr(buffer, "\"width\"");
	char *h_ptr = strstr(buffer, "\"height\"");

	if (x_ptr && y_ptr && w_ptr && h_ptr) {
		float x_val = 0.0f;
		float y_val = 0.0f;
		float w_val = 0.0f;
		float h_val = 0.0f;

		if (sscanf(x_ptr, "\"x\" : %f", &x_val) != 1) sscanf(x_ptr, "\"x\":%f", &x_val);
		if (sscanf(y_ptr, "\"y\" : %f", &y_val) != 1) sscanf(y_ptr, "\"y\":%f", &y_val);
		if (sscanf(w_ptr, "\"width\" : %f", &w_val) != 1) sscanf(w_ptr, "\"width\":%f", &w_val);
		if (sscanf(h_ptr, "\"height\" : %f", &h_val) != 1) sscanf(h_ptr, "\"height\":%f", &h_val);

		coord = (pixel_coord_t){
			.x = (int16_t)(x_val + (w_val / 2.0f)),
			.y = (int16_t)(y_val + (h_val / 2.0f))
		};
	}

	return coord;
}


errno_t locate_single_point_on_img(
	image_t *img,
	const pixel_coord_t pixel_coord,
	const uint32_t color,
	const uint16_t radius_px
) {
	assert(img && img->pixels);

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


errno_t locate_keypoints_on_img(
	image_t *img,
	const vector_t *keypoints,
	const uint32_t color
) {
	assert(img && img->pixels && keypoints);
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


static uint32_t generate_cluster_color(uint16_t cluster_id) {
	if (cluster_id == DBSCAN_POINT_NOISE) {
		// keep noise dim
		return COLOR_RGB_ENCODE(90, 90, 90); 
	}

	// keep this pretty
	static const uint32_t bright_palette[] = {
		neon_red,
		bright_lime,
		electric_blue,
		clear_yellow,
		magenta,
		bright_cian,
		flame_orange,
		light_violet,
		neon_mint,
		hot_pink,
		acid_shatrez,
		light_green
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
	const void *cctx_vp,
	const bool enable_locate_clusters
) {
	assert(img && keypoints && cctx_vp);
	if (img->channel == GRAY && image_gray_to_rgb(img) != OK) {
		ddloge(TAG, "failed to convert image to RGB");
		return -1;
	}

	clusters_context_t *cctx = (clusters_context_t*)cctx_vp;
	for (size_t i = 0; i < keypoints->size; i++) {
		pixel_coord_t *point = (pixel_coord_t*)vector_get(keypoints, i);
		if (!point) continue;

		uint16_t cluster_id = cctx->ids[i];
		uint32_t color = generate_cluster_color(cluster_id);

		locate_single_point_on_img(img, *point, color, 1);
	}

	if (enable_locate_clusters && cctx->centers)
		for (size_t i = 0; i < cctx->centers->size; i++) {
			pixel_coord_t *center = (pixel_coord_t *)vector_get(cctx->centers, i);
			if (center)
				locate_single_point_on_img(img, *center, dim_blue, 3);
		}

	return OK;
}


static errno_t draw_line_on_img(
	image_t *img,
	const pixel_coord_t start,
	const pixel_coord_t end,
	const uint32_t color
) {
	assert(img && img->pixels);

	// Brezenham algorithm for a 1-pixel line
	int16_t x0 = (int16_t)start.x;
	int16_t y0 = (int16_t)start.y;
	int16_t x1 = (int16_t)end.x;
	int16_t y1 = (int16_t)end.y;

	int16_t dx = abs(x1 - x0);
	int16_t dy = abs(y1 - y0);
	int16_t sx = (x0 < x1) ? 1 : -1;
	int16_t sy = (y0 < y1) ? 1 : -1;
	int16_t err = dx - dy;

	while (true) {
		if (x0 >= 0 && x0 < (int16_t)img->width && y0 >= 0 && y0 < (int16_t)img->height) {
			size_t base_idx = ((size_t)y0 * img->width + x0) * img->channel;
			
			if (img->channel == GRAY) {
				img->pixels[base_idx] = COLOR_A_DECODE(color);
			} else if (img->channel == RGB || img->channel == RGBA) {
				img->pixels[base_idx + 0] = COLOR_R_DECODE(color);
				img->pixels[base_idx + 1] = COLOR_G_DECODE(color);
				img->pixels[base_idx + 2] = COLOR_B_DECODE(color);
				if (img->channel == RGBA) {
					img->pixels[base_idx + 3] = COLOR_A_DECODE(color);
				}
			}
		}

		if (x0 == x1 && y0 == y1) break;

		int16_t e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}

	return OK;
}


errno_t locate_tracks_on_img(
	image_t *img,
	const void *tracker_ctx_vp,
	uint16_t track_deviation_squared_threshold
) {
	assert(img);
	if (!tracker_ctx_vp)
		return EINVAL;
	tracker_context_t *tracker_ctx = (tracker_context_t*)tracker_ctx_vp;
	if (!tracker_ctx->active_tracks)
		return EINVAL;

	for (size_t i = 0; i < tracker_ctx->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker_ctx->active_tracks, i);
		if (!trk)
			continue;
		if (trk->age > 1 && trk->missed_frames == 0) {
			if (trk->is_most_deviated) {
				draw_line_on_img(img, trk->previous, trk->current, red);
				locate_single_point_on_img(img, trk->current, red, 5);
			} else if (trk->deviation_squared > track_deviation_squared_threshold) {
				draw_line_on_img(img, trk->previous, trk->current, dim_magenta);
				locate_single_point_on_img(img, trk->current, dim_magenta, 4);
			} else {
				draw_line_on_img(img, trk->previous, trk->current, light_violet);
				locate_single_point_on_img(img, trk->current, light_violet, 3);
			}
		}
	}
	return OK;
}
