#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#define	OK 0

#define VERSION "69"

#define ddlogi(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define ddlogw(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "\033[33m" "warning " "\033[0m" "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define ddloge(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "\033[31m" "error " "\033[0m" "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)


typedef struct {
	char filepath[128];
	uint8_t fast9_threshold;
	uint8_t dim_coef;
} main_args_t;

errno_t parse_main_args(
	int argc, char **argv,
	main_args_t *main_args
);

#endif
