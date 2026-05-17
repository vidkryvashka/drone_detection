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


enum IO_MODES {
	not_selected,
	single_img_file,
	input_img_dir,
};

typedef struct {
	char input_filepath[128];
	char input_img_dir[128];
	char output_dir[128];
	uint8_t fast9_threshold;
	uint8_t dim_coef;
	enum IO_MODES io_mode;
} config_t;

errno_t parse_conf(
	int argc, char **argv,
	config_t *conf
);

errno_t apply_io_mode(
	const config_t *conf
);

#endif
