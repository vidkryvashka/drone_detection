#include <getopt.h>
#include <iso646.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "img_defs.h"
#include "img_io.h"
#include "vision.h"

#define TAG "parce_args "


static void print_help() {
	printf("\
version %s\n\
-i --input <char *>\timage file or directory containing images path, default in Makefile\n\
-i --output_dir <char *>\tfolder for outputs\n\
-t --threshold <uint8_t>\tfast9 threshold, \tdefault %d\n\
-d --dim-coef <uint8_t>\t\t0 - %d value, 0 is black img output, points only, default %d\n\
-h --help\n\
examples:\n\
\t$ binary -i expl.png -t 70 -d 3\n\
\t$ binary expl.png\n",
		VERSION, DEFAULT_THRESHOLD, MAX_DIM_COEF, DEAFAULT_DIM_COEF);
}

/**
 * @brief returns 1 if file exists, 2 if dir, 0 if noone
 * 
 * @param path <char *> filename or dirname
 */
static enum IO_MODES file_or_dir_exists(
	const char *path
) {
	if (!path)
		return not_selected;

	struct stat stats;
	
	if (stat(path, &stats) != 0)
		return not_selected;

	if (S_ISREG(stats.st_mode))
		return single_img_file;

	if (S_ISDIR(stats.st_mode))
		return input_img_dir;
	
	return 0;
}

static void set_default_conf(
	config_t *conf
) {
	bzero(conf, sizeof(*conf));
	conf->fast9_threshold = DEFAULT_THRESHOLD;
	conf->dim_coef = DEAFAULT_DIM_COEF;
	conf->frame_width = 0;
	conf->frame_height = 0;
	conf->is_test = false;
}

errno_t parse_conf(
	int argc, char **argv,
	config_t *conf
) {
	if (argc < 2) {
		print_help();
		return EINVAL;
	}

	set_default_conf(conf);

	if (argc > 1 && argv[1][0] != '-') {
		snprintf(conf->input_filepath, sizeof(conf->input_filepath), "%s", argv[1]);
		snprintf(conf->output_dir, sizeof(conf->output_dir), "%s", DEFAULT_OUTPUT_DIR);
		conf->io_mode = single_img_file;
		return OK;
	}

	struct option longopts[] = {
		{"input", required_argument, NULL, 'i'},
		{"output-dir", required_argument, NULL, 'o'},
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
		// ddlogi(TAG, "opt:%c optarg:%s", opt, optarg);	// dbg
		switch (opt) {
		case 'i':
			switch (file_or_dir_exists(optarg)) {
			case single_img_file:
				snprintf(conf->input_filepath, sizeof(conf->input_filepath), "%s", optarg);
				conf->io_mode = single_img_file;
				conf->is_test = true;
				break;
			case input_img_dir:
				snprintf(conf->input_img_dir, sizeof(conf->input_img_dir), "%s", optarg);
				conf->io_mode = input_img_dir;
				break;
			case not_selected:
				ddloge(TAG, "input file/dir %s does not exist", optarg);
				break;
			default:
				ddloge(TAG, "impossible case default, %s", optarg);
				return EINVAL;
			}
			// ddlogi(TAG, "io_mode %d", conf->io_mode);
			break;
		case 'o':
			switch (file_or_dir_exists(optarg)) {
			case 1:
				ddloge(TAG, "file %s exists insteed of directory, can't create", optarg);
				break;
			case 0:
				if (mkdir(optarg, 0777) == -1) {
					ddloge(TAG, "Error creating directory");
					return ENOMEM;
				}
				ddlogw(TAG, "Directory %s does not exist, created", optarg);
			case 2:
				snprintf(conf->output_dir, sizeof(conf->output_dir), "%s", optarg);
				break;
			default:
				ddloge(TAG, "impossible case default, %s", optarg);
				return EINVAL;
			}
			break;
		case 't':
			endptr = NULL;
			val = strtol(optarg, &endptr, 10);
			if (endptr == optarg || val < 0 || val > 255) {
				ddloge(TAG, "Invalid fast9_threshold value: %s", optarg);
				break;
			}
			conf->fast9_threshold = (uint8_t)val;
			break;
		case 'd':
			endptr = NULL;
			val = strtol(optarg, &endptr, 10);
			if (endptr == optarg || val < 0 || val > 255) {
				ddloge(TAG, "Invalid fast9_threshold value: %s", optarg);
				break;
			}
			conf->dim_coef = (uint8_t)val;
			break;
		case 'h':
			print_help();
			exit(0);
		case 0:
			ddloge(TAG, "long option --%s not supported", optarg);
			break;
		case ':':
			ddloge(TAG, "option -%c needs a value\n", optopt);
			break;
		case '?':
			ddloge(TAG, "unknown option -%c", optopt);
			break;
		default:
			ddlogw(TAG, "case default, impossible, mb excessive letter in shortopts[]");
		}
	}

	if (argc - optind > 1) {
		ddloge(TAG, "\nToo many unknown args\n");
		print_help();
		return EINVAL;
	}

	if (!conf->input_filepath[0] && !conf->input_img_dir[0]) {
		ddloge(TAG, "couldn't choose input");
		return EINVAL;
	}

	if (!conf->output_dir[0]) {
		snprintf(conf->output_dir, sizeof(conf->output_dir), "%s", DEFAULT_OUTPUT_DIR);
		ddlogw(TAG, "output_dir set default %s", conf->output_dir);
	}

	if (conf->fast9_threshold < conf->dim_coef)
		ddlogw(TAG, "In case you could blunder: threshold: %d dim_coef: %d", conf->fast9_threshold, conf->dim_coef);

	return OK;
}
