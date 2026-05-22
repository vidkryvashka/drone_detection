#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#define	OK 0

#define VERSION "69"

enum CHANNELS {
	GRAY = 1,	// mostly used
	RGB = 3,
	RGBA = 4
};

typedef struct {
	uint8_t *pixels;
	uint16_t width;
	uint16_t height;
	enum CHANNELS channel;
} image_t;

typedef struct {
	uint16_t x;
	uint16_t y;
} pixel_coord_t;


#define ddlogi(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define ddlogw(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "\033[33m" "warning " "\033[0m" "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define ddloge(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "\033[31m" "error " "\033[0m" "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define STR_MAX_LEN 127
typedef struct {
	char name[STR_MAX_LEN + 1];
} str_t;

enum IO_MODES {
	not_selected,
	single_img_file,
	input_img_dir,
};

typedef struct {
	void *img_io_conf;
	void *vision_conf;
	uint8_t dbg_lvl;
} main_conf_t;


errno_t parse_conf(
	int argc, char **argv,
	main_conf_t *conf
);

errno_t apply_io_mode(
	main_conf_t *conf
);

#define PROGRESS_BAR_WIDTH 50
/**
 * @brief should be called inside some heavy loop
 */
void print_progress_bar(
	const char *prefix,
	const size_t current,
	const size_t total
);

#endif
