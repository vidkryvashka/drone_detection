#include <getopt.h>
#include <iso646.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "img_io.h"
#include "vision.h"

#define TAG "parce_args "


static inline void print_help() {
	printf("\
version %s\n\
-h --help\n\
-i --input                            path to image file or directory containing images path, default in Makefile\n\
-i --output_dir                       path to folder for frames output. video goes to default \"output\" folder\n\
-d --dim_coef                         0 - %d value, 0 is black img output, points only, default %d\n\
   --fast9_threshold                  default %d\n\
   --dbscan_max_distance              max 2D distance between points to attribute the point to the cluster, default %d\n\
   --dbscan_min_cluster_size          min points number in cluster, default %d\n\
   --dbscan_enable_geometry_filtering no arg. may be excessive for optical flow calculate, default %s\n\
   --track_max_distance               the maximum distance in pixels that an object can move in 1 frame, default %d\n\
   --track_max_missed                 how many frames to wait before deleting a lost track, default %d\n\
   --dbg_lvl                          0 | 1 | 2\n\n\
examples:\n\
   $ ./bin -i expl.jpg --fast9_threshold 70\n\
   $ ./bin expl.png\n",
		VERSION, MAX_DIM_COEF, DEFAULT_DIM_COEF,
		FAST9_DEFAULT_THRESHOLD,
		DBSCAN_DEFAULT_MAX_DISTANCE_IMG_DIAGONAL_PERCENT,
		DBSCAN_DEFAULT_MIN_CLUSTER_SIZE,
		DBSCAN_DEFAULT_ENABLE_GEOM_FILTERING ? "on" : "off",
		TRACK_DEFAULT_MAX_DISTANCE,
		TRACK_DEFAULT_MAX_MISSED
	);
}


static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"input", required_argument, NULL, 'i'},
	{"output_dir", required_argument, NULL, 'o'},
	{"dim_coef", required_argument, NULL, 'd'},
	{"fast9_threshold", required_argument, NULL, 0},
	{"dbscan_max_distance", required_argument, NULL, 0},
	{"dbscan_min_cluster_size", required_argument, NULL, 0},
	{"dbscan_enable_geometry_filtering", no_argument, NULL, 0},
	{"track_max_distance", required_argument, NULL, 0},
	{"track_max_missed", required_argument, NULL, 0},
	{"dbg_lvl", required_argument, NULL, 0},
	{NULL, 0, NULL, 0}
};


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


static errno_t organize_input(
	const char *input_file_or_dir_name,
	main_conf_t *conf
) {
	img_io_conf_t *iio_conf = conf->img_io_conf;
	switch (file_or_dir_exists(input_file_or_dir_name)) {
	case single_img_file:
		snprintf(iio_conf->input_filepath, sizeof(iio_conf->input_filepath), "%s", input_file_or_dir_name);
		iio_conf->io_mode = single_img_file;
		conf->dbg_lvl = 2;
		break;
	case input_img_dir:
		snprintf(iio_conf->input_img_dir, sizeof(iio_conf->input_img_dir), "%s", input_file_or_dir_name);
		iio_conf->io_mode = input_img_dir;
		break;
	case not_selected:
		ddloge(TAG, "input file/dir %s does not exist", input_file_or_dir_name);
		break;
	default:
		ddloge(TAG, "impossible case default, %s", input_file_or_dir_name);
		return EINVAL;
	}
	return OK;
}


static errno_t organize_output(
	const char *output_file_or_dir_name,
	img_io_conf_t *iio_conf
) {
	switch (file_or_dir_exists(output_file_or_dir_name)) {
	case 1:
		ddloge(TAG, "file %s exists insteed of directory, can't create", output_file_or_dir_name);
		break;
	case 0:
		if (mkdir(output_file_or_dir_name, 0777) == -1) {
			ddloge(TAG, "Error creating directory");
			return ENOMEM;
		}
		ddlogw(TAG, "Directory %s does not exist, created", output_file_or_dir_name);
	case 2:
		snprintf(iio_conf->output_dir, sizeof(iio_conf->output_dir), "%s", output_file_or_dir_name);
		break;
	default:
		ddloge(TAG, "impossible case default, %s", output_file_or_dir_name);
		return EINVAL;
	}
	return OK;
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
		.dbscan_min_cluster_size = DBSCAN_DEFAULT_MIN_CLUSTER_SIZE,
		.track_max_distance = TRACK_DEFAULT_MAX_DISTANCE,
		.track_max_missed = TRACK_DEFAULT_MAX_MISSED,
		.dbscan_enable_geometry_filtering = DBSCAN_DEFAULT_ENABLE_GEOM_FILTERING,
		.fast9_threshold = FAST9_DEFAULT_THRESHOLD
	};
	conf->dbg_lvl = 0;

	return OK;
}


static int32_t parce_int(
	char *optarg,
	int32_t bound0,
	int32_t bound1,
	int32_t default_val
) {
	char *endptr = NULL;
	int32_t val = strtol(optarg, &endptr, 10);
	if (endptr == optarg || val < bound0 || val > bound1) {
		ddloge(TAG, "Invalid %s value: %d", optarg, val);
		return default_val;
	}
	return val;
}


static void parce_longopt(
	int longindex,
	main_conf_t *conf
) {
	vision_conf_t *vconf = conf->vision_conf;
	switch (longindex) {
	case 4:
		vconf->fast9_threshold = parce_int(optarg, 0, UINT8_MAX, FAST9_DEFAULT_THRESHOLD);
		break;
	case 5:
		vconf->dbscan_max_distance_img_diagonal_percent = parce_int(optarg, 0, UINT16_MAX, DBSCAN_DEFAULT_MAX_DISTANCE_IMG_DIAGONAL_PERCENT);
		break;
	case 6:
		vconf->dbscan_min_cluster_size = parce_int(optarg, 0, UINT8_MAX, DBSCAN_DEFAULT_MIN_CLUSTER_SIZE);
		break;
	case 7:
		vconf->dbscan_enable_geometry_filtering = true;
		break;
	case 8:
		vconf->track_max_distance = parce_int(optarg, 0, UINT16_MAX, TRACK_DEFAULT_MAX_DISTANCE);
		break;
	case 9:
		vconf->track_max_missed = parce_int(optarg, 0, UINT16_MAX, TRACK_DEFAULT_MAX_MISSED);
		break;
	case 10:
		conf->dbg_lvl = parce_int(optarg, 0, UINT8_MAX, 0);
		break;
	default:
		ddloge(TAG, "long option --%s %s not supported", longopts[longindex].name, optarg);
		break;
	}
}


static inline void print_vconf(
	const vision_conf_t *vconf
) {
	ddlogi(TAG, "vision config:\n\
fast9_threshold %d\n\
dbscan_max_distance_img_diagonal_percent %d\n\
dbscan_enable_geometry_filtering %d\n\
dbscan_min_cluster_size %d\n\
track_max_distance %d\n\
track_max_missed %d",\
		vconf->fast9_threshold,
		vconf->dbscan_max_distance_img_diagonal_percent,
		vconf->dbscan_enable_geometry_filtering,
		vconf->dbscan_min_cluster_size,
		vconf->track_max_distance,
		vconf->track_max_missed
	);
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
		conf->dbg_lvl = 2;
	}

	int opt, longindex;
	char shortopts[] = "hi:o:d:";	// leading : Enables silent error reporting. X:: optional close arg -Xarg. no : no arg
	while ((opt = getopt_long(
		argc,
		argv,
		shortopts,
		longopts,
		&longindex)) != -1
	) {
		// ddlogi(TAG, "opt:%c optarg:%s", opt, optarg); // dbg
		switch (opt) {
		case 'i':
			if (organize_input(optarg, conf) != OK) {
				ddloge(TAG, "organize_input failed");
				return EINVAL;
			}
			break;
		case 'o':
			if (organize_output(optarg, iio_conf) != OK) {
				ddloge(TAG, "organize_output failed");
				return EINVAL;
			}
			break;
		case 'd':
			iio_conf->dim_coef = parce_int(optarg, 0, MAX_DIM_COEF, DEFAULT_DIM_COEF);
			break;
		case 'h':
			print_help();
			exit(0);
		case 0:
			parce_longopt(longindex, conf);
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

	if (conf->dbg_lvl >= 2)
		print_vconf(vconf);

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
