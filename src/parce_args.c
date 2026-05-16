#include <getopt.h>
#include <string.h>

#include "defs.h"
#include "img_proc.h"


#define TAG "parce_args "

static void print_help() {
	printf("\
version %s\n\
-i --input <char *filename>\timage file path, default in Makefile\n\
-t --threshold <uint8_t>\tfast9 threshold, \tdefault %d\n\
-d --dim-coef <uint8_t>\t\t0 - %d value, 0 is black img output, points only, default %d\n\
-h --help\n\
examples:\n\
\t$ binary -i expl.png -t 70 -d 3\n\
\t$ binary expl.png\n",
		VERSION, DEFAULT_THRESHOLD, MAX_DIM_COEF, DEAFAULT_DIM_COEF);
}

errno_t parse_main_args(
	int argc, char **argv,
	main_args_t *main_args
) {
	if (argc < 2) {
		print_help();
		return EINVAL;
	}

	bzero(main_args, sizeof(*main_args));

	if (argc > 1 && argv[1][0] != '-')
		strcpy(main_args->filepath, argv[1]);

	main_args->fast9_threshold = DEFAULT_THRESHOLD;
	main_args->dim_coef = DEAFAULT_DIM_COEF;

	struct option longopts[] = {
		{"input", required_argument, NULL, 'i'},
		{"threshold", required_argument, NULL, 't'},
		{"dim-coef", required_argument, NULL, 'd'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	int opt, longindex;
	char shortopts[] = "i:t:d:h";	// leading : Enables silent error reporting. X:: optional close arg -Xarg. no : no arg
	char *endptr;
	long val;
	while ((opt = getopt_long(
		argc,
		argv,
		shortopts,
		longopts,
		&longindex)) != -1
	) {
		// ddlogi(TAG, "opt: %c optarg: %s", opt, optarg);
		switch (opt) {
		case 'i':
			// ddlogi(TAG, "case -i %s", optarg);
			snprintf(main_args->filepath, sizeof(main_args->filepath), "%s", optarg);
			break;
		case 't':
			// ddlogi(TAG, "case -t %s", optarg);
			endptr = NULL;
			val = strtol(optarg, &endptr, 10);
			if (endptr == optarg || val < 0 || val > 255) {
				ddloge(TAG, "Invalid fast9_threshold value: %s", optarg);
				break;
			}
			main_args->fast9_threshold = (uint8_t)val;
			break;
		case 'd':
			// ddlogi(TAG, "case -d %s", optarg);
			endptr = NULL;
			val = strtol(optarg, &endptr, 10);
			if (endptr == optarg || val < 0 || val > 255) {
				ddloge(TAG, "Invalid fast9_threshold value: %s", optarg);
				break;
			}
			main_args->dim_coef = (uint8_t)val;
			break;
		case 'h':
			print_help();
			break;
		case 0:
			ddloge(TAG, "long options not supported, --%s", optarg);
			break;
		case ':':
			ddlogi(TAG, "option -%c needs a value\n", optopt);
			break;
		case '?':
			ddlogi(TAG, "unknown option -%c", optopt);
			break;
		default:
			ddlogi(TAG, "case default, impossible, mb excessive letter in shortopts[]");
		}
	}

	if (argc - optind > 1) {
		ddloge(TAG, "\nToo many unknown args\n");
		print_help();
		return EINVAL;
	}

	if (!main_args->filepath[0]) {
		ddloge(TAG, "main_args->filepath empty");
		return -1;
	}

	if (main_args->fast9_threshold < main_args->dim_coef)
		ddlogw(TAG, "In case you could blunderЖ t: %d d: %d", main_args->fast9_threshold, main_args->dim_coef);

	return OK;
}
