#include <getopt.h>
#include <iso646.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "img_io.h"
#include "vision.h"

#define TAG "parce_args "


static void print_help() {
	printf("\
version %s\n\
-h --help\n\
-i --input <char *>\timage file or directory containing images path, default in Makefile\n\
-i --output_dir <char *>\tfolder for outputs\n\
-t --threshold <uint8_t>\tfast9 threshold, \tdefault %d\n\
-d --dim-coef <uint8_t>\t\t0 - %d value, 0 is black img output, points only, default %d\n\
examples:\n\
\t$ binary -i expl.png -t 70 -d 3\n\
\t$ binary expl.png\n",
		VERSION, FAST9_DEFAULT_THRESHOLD, MAX_DIM_COEF, DEFAULT_DIM_COEF);
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

static errno_t init_default_conf(
	main_conf_t *conf
) {
	conf->img_io_conf = malloc(sizeof(img_io_conf_t));
	conf->vision_conf = malloc(sizeof(vision_conf_t));
	img_io_conf_t *iio_conf = conf->img_io_conf;
	vision_conf_t *vconf = conf->vision_conf;

	*iio_conf = (img_io_conf_t){
		.dim_coef = DEFAULT_DIM_COEF
	};

	*vconf = (vision_conf_t){
		.frame_size = (pixel_coord_t){.x = 0, .y = 0},	// inits fully while image io
		.dbscan_max_distance_img_diagonal_percent = DBSCAN_DEFAULT_MAX_DISTANCE_IMG_DIAGONAL_PERCENT,
		.dbscan_min_cluster_size = DBSCAN_MIN_CLUSTER_SIZE,
		.track_max_distance = TRACK_DEFAULT_MAX_DISTANCE,
		.dbscan_enable_geometry_filtering = true,
		.fast9_threshold = FAST9_DEFAULT_THRESHOLD
	};
	
	conf->is_test = 0;

	return OK;
}

errno_t parse_conf(
	int argc, char **argv,
	main_conf_t *conf
) {
	if (argc < 2) {
		print_help();
		return EINVAL;
	}

	init_default_conf(conf);
	img_io_conf_t *iio_conf = conf->img_io_conf;
	vision_conf_t *vconf = conf->vision_conf;

	if (argc > 1 && argv[1][0] != '-') {
		snprintf(iio_conf->input_filepath, sizeof(iio_conf->input_filepath), "%s", argv[1]);
		snprintf(iio_conf->output_dir, sizeof(iio_conf->output_dir), "%s", DEFAULT_OUTPUT_DIR);
		iio_conf->io_mode = single_img_file;
		return OK;
	}

	struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"input", required_argument, NULL, 'i'},
		{"output-dir", required_argument, NULL, 'o'},
		{"dim-coef", required_argument, NULL, 'd'},
		{NULL, 0, NULL, 0}
	};

	int opt, longindex;
	char shortopts[] = "i:o:d:h";	// leading : Enables silent error reporting. X:: optional close arg -Xarg. no : no arg
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
				snprintf(iio_conf->input_filepath, sizeof(iio_conf->input_filepath), "%s", optarg);
				iio_conf->io_mode = single_img_file;
				break;
			case input_img_dir:
				snprintf(iio_conf->input_img_dir, sizeof(iio_conf->input_img_dir), "%s", optarg);
				iio_conf->io_mode = input_img_dir;
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
				snprintf(iio_conf->output_dir, sizeof(iio_conf->output_dir), "%s", optarg);
				break;
			default:
				ddloge(TAG, "impossible case default, %s", optarg);
				return EINVAL;
			}
			break;
		case 'd':
			endptr = NULL;
			val = strtol(optarg, &endptr, 10);
			if (endptr == optarg || val < 0 || val > 255) {
				ddloge(TAG, "Invalid dim_coef value: %s", optarg);
				break;
			}
			iio_conf->dim_coef = (uint8_t)val;
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

	if (!iio_conf->input_filepath[0] && !iio_conf->input_img_dir[0]) {
		ddloge(TAG, "couldn't choose input");
		return EINVAL;
	}

	if (!iio_conf->output_dir[0]) {
		snprintf(iio_conf->output_dir, sizeof(iio_conf->output_dir), "%s", DEFAULT_OUTPUT_DIR);
		ddlogw(TAG, "output_dir set default %s", iio_conf->output_dir);
	}

	return OK;
}
