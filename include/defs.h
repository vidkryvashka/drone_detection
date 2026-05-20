#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#define	OK 0

#define VERSION "69"

#define ddlogi(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define ddlogw(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "\033[33m" "warning " "\033[0m" "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define ddloge(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "\033[31m" "error " "\033[0m" "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define STR_MAX_LEN 127
typedef struct {
	char name[STR_MAX_LEN + 1]; // +1 for '\0'
} str_t;

enum IO_MODES {
	not_selected,
	single_img_file,
	input_img_dir,
};

typedef struct {
	char input_filepath[STR_MAX_LEN + 1];
	char input_img_dir[STR_MAX_LEN + 1];
	char output_dir[STR_MAX_LEN + 1];
	uint8_t dim_coef;	// 0 - 8 where 0 is black. applies only while saving image
	enum IO_MODES io_mode;
} main_conf_t;

errno_t parse_conf(
	int argc, char **argv,
	main_conf_t *conf
);

errno_t apply_io_mode(
	const main_conf_t *conf
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
