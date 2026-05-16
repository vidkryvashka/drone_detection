#include <getopt.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "img_proc.h"


#define TAG "parce_args "

static void print_help() {
	printf("\
version %s\n\
-i --input <char *filename>\timage file path, default in Makefile\n\
-i --output_dir <char *output_dir>\tfolder for outputs\n\
-t --threshold <uint8_t>\tfast9 threshold, \tdefault %d\n\
-d --dim-coef <uint8_t>\t\t0 - %d value, 0 is black img output, points only, default %d\n\
-h --help\n\
examples:\n\
\t$ binary -i expl.png -t 70 -d 3\n\
\t$ binary expl.png\n",
		VERSION, DEFAULT_THRESHOLD, MAX_DIM_COEF, DEAFAULT_DIM_COEF);
}

uint8_t directory_exists(const char *path) {
	if (!path)
		return 0;
	struct stat stats;
	
	if (stat(path, &stats) != 0)
		return 0; 

	return S_ISDIR(stats.st_mode);
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
	main_args->fast9_threshold = DEFAULT_THRESHOLD;
	main_args->dim_coef = DEAFAULT_DIM_COEF;
	snprintf(main_args->output_dir, sizeof(main_args->output_dir), "%s", DEFAULT_OUTPUT_DIR);

	if (argc > 1 && argv[1][0] != '-') {
		snprintf(main_args->input_filepath, sizeof(main_args->input_filepath), "%s", argv[1]);
		return OK;
	}

	struct option longopts[] = {
		{"input", required_argument, NULL, 'i'},
		{"output_dir", required_argument, NULL, 'o'},
		{"threshold", required_argument, NULL, 't'},
		{"dim-coef", required_argument, NULL, 'd'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	int opt, longindex;
	char shortopts[] = "i:o:t:d:h";	// leading : Enables silent error reporting. X:: optional close arg -Xarg. no : no arg
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
			snprintf(main_args->input_filepath, sizeof(main_args->input_filepath), "%s", optarg);
			break;
		case 'o':
			// ddlogi(TAG, "case -o %s", optarg);
			if (!directory_exists(optarg)) {
				ddlogw(TAG, "Directory %s does not exist, creating..", optarg);
    			if (mkdir(optarg, 0777) == -1) {
        			ddloge(TAG, "Error creating directory");
           			return ENOMEM;
       			}
			}
			snprintf(main_args->output_dir, sizeof(main_args->output_dir), "%s", optarg);
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
			exit(0);
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

	if (!main_args->input_filepath[0]) {
		ddloge(TAG, "main_args->input_filepath empty");
		return -1;
	}

	if (!main_args->output_dir[0]) {
		snprintf(main_args->output_dir, sizeof(main_args->output_dir), "%s", "output");
		return -1;
	}

	if (main_args->fast9_threshold < main_args->dim_coef)
		ddlogw(TAG, "In case you could blunderЖ t: %d d: %d", main_args->fast9_threshold, main_args->dim_coef);

	return OK;
}
