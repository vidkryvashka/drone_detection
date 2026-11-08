#include <getopt.h>
#include <iso646.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "defs.h"
#include "img_io.h"
#include "vision.h"
#include "parce_toml.h"

#define TAG "parce_args "

static bool dbg_lvl_explicitly_set = false;

static inline void print_help() {
	printf("\
version %s\n\
-h --help\n\
-c --config        path to toml config file (default: config.toml)\n\
-i --input         path to image file or directory\n\
-o --output_dir    path to folder for frames output\n\
-d --dim_coef      0 - %d, 0 is black img output, points only, default %d\n\
   --output_video  path to video saving location, default %s\n\
   --dbg_lvl       0 | 1 | 2\n\n\
examples:\n\
   $ ./bin -c config.toml -i expl.jpg\n\
   $ ./bin -c config.toml assets/mavic_in_bush.jpg\n\
   $ ./bin expl.png\n",
		VERSION, MAX_DIM_COEF, DIM_COEF_DEFAULT, OUTPUT_VIDEO_PATH_DEFAULT
	);
}


static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"input", required_argument, NULL, 'i'},
	{"output_dir", required_argument, NULL, 'o'},
	{"dim_coef", required_argument, NULL, 'd'},
	{"output_video", required_argument, NULL, 0},
	{"dbg_lvl", required_argument, NULL, 0},
	{"config", optional_argument, NULL, 'c'},
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
	
	return not_selected;
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
		if (!dbg_lvl_explicitly_set)
			conf->dbg_lvl = 2;
		break;
	case input_img_dir:
		snprintf(iio_conf->input_img_dir, sizeof(iio_conf->input_img_dir), "%s", input_file_or_dir_name);
		iio_conf->io_mode = input_img_dir;
		break;
	case not_selected:
		ddloge(TAG, "input file/dir %s does not exist", input_file_or_dir_name);
		return EINVAL;
	default:
		ddloge(TAG, "impossible case default, %s", input_file_or_dir_name);
		return EINVAL;
	}
	return OK;
}


static errno_t ensure_dir(
	const char *path
) {
	switch (file_or_dir_exists(path)) {
	case input_img_dir:
		return OK;
	case single_img_file:
		ddloge(TAG, "file \"%s\" exists instead of directory, can't create", path);
		return EINVAL;
	case not_selected:
		if (mkdir(path, 0777) == -1) {
			ddloge(TAG, "failed to create directory \"%s\"", path);
			return errno;
		}
		ddlogw(TAG, "directory \"%s\" does not exist, created", path);
		return OK;
	default:
		ddloge(TAG, "impossible case default, \"%s\"", path);
		return EINVAL;
	}
}


static errno_t organize_img_output(
	const char *output_file_or_dir_name,
	img_io_conf_t *iio_conf
) {
	(void)iio_conf;
	errno_t err = ensure_dir(output_file_or_dir_name);
	if (err != OK)
		return err;

	char intermediate[STR_MAX_LEN + sizeof(INTERMEDIATE_IMAGES_SUBDIR) + 2];
	int written = snprintf(intermediate, sizeof(intermediate), "%s/%s",
		output_file_or_dir_name, INTERMEDIATE_IMAGES_SUBDIR);
	if (written < 0 || (size_t)written >= sizeof(intermediate)) {
		ddloge(TAG, "intermediate path overflowed for \"%s\"", output_file_or_dir_name);
		return ENOMEM;
	}
	return ensure_dir(intermediate);
}


static errno_t init_default_conf(
	main_conf_t *conf
) {
	conf->img_io_conf = malloc(sizeof(img_io_conf_t));
	conf->vision_conf = malloc(sizeof(vision_conf_t));
	img_io_conf_t *iio_conf = conf->img_io_conf;
	vision_conf_t *vconf = conf->vision_conf;

	*iio_conf = (img_io_conf_t){
		.dim_coef = DIM_COEF_DEFAULT
	};

	snprintf(iio_conf->output_dir, sizeof(iio_conf->output_dir), "%s", OUTPUT_IMG_DIR_DEFAULT);
	snprintf(iio_conf->output_video_path, sizeof(iio_conf->output_video_path), "%s", OUTPUT_VIDEO_PATH_DEFAULT);

	*vconf = (vision_conf_t){
		.frame_size = (pixel_coord_t){.x = 0, .y = 0},	// inits fully while image io
		.fast9_threshold = FAST9_DEFAULT_THRESHOLD,
		.dbscan_conf.max_distance_img_diagonal_percent = DBSCAN_MAX_DISTANCE_IMG_DIAGONAL_PERCENT_DEFAULT,
		.dbscan_conf.min_cluster_size = DBSCAN_MIN_CLUSTER_SIZE_DEFAULT,
		.dbscan_conf.min_clusters_count_merge = DBSCAN_MIN_CLUSTERS_COUNT_MERGE_DEFAULT,
		.dbscan_conf.enable_geometric_filtering = DBSCAN_ENABLE_GEOM_FILTERING_DEFAULT,
		.track_conf.max_distance = TRACK_MAX_DISTANCE_DEFAULT,
		.track_conf.max_missed = TRACK_MAX_MISSED_DEFAULT,
		.track_conf.deviation_squared_threshold = TRACK_DEVIATION_THRESHOLD_SQUARED_DEFAULT
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


static inline void print_vconf(
	const vision_conf_t *vconf
) {
	ddlogi(TAG, "vision config:\n\
fast9_threshold %d\n\
dbscan_max_distance_img_diagonal_percent %d\n\
dbscan_min_cluster_size %d\n\
dbscan_min_clusters_count_merge %d\n\
dbscan_enable_geometric_filtering %s\n\
track_max_distance %d\n\
track_max_missed %d\n\
track_deviation_squared_threshold %d",\
		vconf->fast9_threshold,
		vconf->dbscan_conf.max_distance_img_diagonal_percent,
		vconf->dbscan_conf.min_cluster_size,
		vconf->dbscan_conf.min_clusters_count_merge,
		vconf->dbscan_conf.enable_geometric_filtering ? "on" : "off",
		vconf->track_conf.max_distance,
		vconf->track_conf.max_missed,
		vconf->track_conf.deviation_squared_threshold
	);
}


static const char *find_config_file_arg(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				const char *dot = strrchr(argv[i + 1], '.');
				if (dot && (!strcmp(dot, ".toml") || !strcmp(dot, ".tml")))
					return argv[i + 1];
			}
			return "config.toml";
		}
		if (strncmp(argv[i], "--config=", 9) == 0) return argv[i] + 9;
		if (strncmp(argv[i], "-c", 2) == 0 && argv[i][2]) return argv[i] + 2;
	}
	return NULL;
}

static inline void cfg_get_u64(toml_datum_t tab, const char *k1, const char *k2, void *dst, size_t sz) {
	toml_datum_t d = toml_get(tab, k1);
	if (d.type == TOML_UNKNOWN && k2) d = toml_seek(tab, k2);
	if (d.type == TOML_INT64) {
		if (sz == sizeof(uint8_t)) *(uint8_t *)dst = (uint8_t)d.u.int64;
		else if (sz == sizeof(uint16_t)) *(uint16_t *)dst = (uint16_t)d.u.int64;
	}
}

static errno_t parse_toml_config(
	const char *filepath,
	main_conf_t *conf
) {
	if (!filepath)
		return EINVAL;

	toml_result_t result = toml_parse_file_ex(filepath);
	if (!result.ok) {
		ddloge(TAG, "failed to parse TOML config \"%s\": %s", filepath, result.errmsg);
		toml_free(result);
		return EINVAL;
	}

	img_io_conf_t *iio_conf = conf->img_io_conf;
	vision_conf_t *vconf = conf->vision_conf;
	toml_datum_t t = result.toptab;

	toml_datum_t d = toml_get(t, "intermediate_dir");
	if (d.type == TOML_UNKNOWN) d = toml_get(t, "output_dir");
	if (d.type == TOML_STRING && d.u.s)
		snprintf(iio_conf->output_dir, sizeof(iio_conf->output_dir), "%s", d.u.s);

	d = toml_get(t, "output_video");
	if (d.type == TOML_UNKNOWN) d = toml_get(t, "output_video_path");
	if (d.type == TOML_STRING && d.u.s) {
		const char *slash = strchr(d.u.s, '/');
		if (d.u.s[0] == '$' && slash)
			snprintf(iio_conf->output_video_path, sizeof(iio_conf->output_video_path), "%s/%s", iio_conf->output_dir, slash + 1);
		else
			snprintf(iio_conf->output_video_path, sizeof(iio_conf->output_video_path), "%s", d.u.s);
	}

	d = toml_get(t, "dbg_lvl");
	if (d.type == TOML_INT64) {
		conf->dbg_lvl = (uint8_t)d.u.int64;
		dbg_lvl_explicitly_set = true;
	}

	d = toml_get(t, "dim_coefficient");
	if (d.type == TOML_UNKNOWN) d = toml_get(t, "dim_coef");
	if (d.type == TOML_INT64 && d.u.int64 >= 0 && d.u.int64 <= MAX_DIM_COEF)
		iio_conf->dim_coef = (uint8_t)d.u.int64;

	cfg_get_u64(t, "fast9_threshold", NULL, &vconf->fast9_threshold, sizeof(uint8_t));
	cfg_get_u64(t, "dbscan_max_distance", "dbscan.max_distance", &vconf->dbscan_conf.max_distance_img_diagonal_percent, sizeof(uint16_t));
	cfg_get_u64(t, "dbscan_min_cluster_size", "dbscan.min_cluster_size", &vconf->dbscan_conf.min_cluster_size, sizeof(uint8_t));
	cfg_get_u64(t, "dbscan_min_clusters_count_merge", "dbscan.min_clusters_count_merge", &vconf->dbscan_conf.min_clusters_count_merge, sizeof(uint16_t));
	cfg_get_u64(t, "track_max_distance", "track.max_distance", &vconf->track_conf.max_distance, sizeof(uint16_t));
	cfg_get_u64(t, "track_max_missed", "track.max_missed", &vconf->track_conf.max_missed, sizeof(uint16_t));
	cfg_get_u64(t, "track_deviation_squared_threshold", "track.deviation_squared_threshold", &vconf->track_conf.deviation_squared_threshold, sizeof(uint16_t));

	d = toml_get(t, "dbscan_enable_geometric_filtering");
	if (d.type == TOML_UNKNOWN) d = toml_seek(t, "dbscan.enable_geometric_filtering");
	if (d.type == TOML_BOOLEAN) vconf->dbscan_conf.enable_geometric_filtering = d.u.boolean;

	d = toml_get(t, "input");
	if (d.type == TOML_STRING && d.u.s && d.u.s[0])
		organize_input(d.u.s, conf);

	toml_free(result);
	ddlogi(TAG, "loaded config from \"%s\"", filepath);
	return OK;
}


static errno_t apply_opts(
	main_conf_t *conf,
	int argc, char **argv,
	const char *config_file
) {
	assert(conf && argc && argv);
	int opt, longindex;
	char shortopts[] = "hi:o:d:c::";
	img_io_conf_t *iio_conf = conf->img_io_conf;
	optind = 1;
	while ((opt = getopt_long(
		argc,
		argv,
		shortopts,
		longopts,
		&longindex)) != -1
	) {
		switch (opt) {
		case 'c':
			if (optarg == NULL && optind < argc && config_file != NULL &&
			    strcmp(argv[optind], config_file) == 0) {
				optind++;
			}
			break;
		case 'i':
			if (organize_input(optarg, conf) != OK) {
				ddloge(TAG, "organize_input failed");
				return EINVAL;
			}
			break;
		case 'o':
			snprintf(iio_conf->output_dir, sizeof(iio_conf->output_dir), "%s", optarg);
			break;
		case 'd':
			iio_conf->dim_coef = parce_int(optarg, 0, MAX_DIM_COEF, DIM_COEF_DEFAULT);
			break;
		case 'h':
			print_help();
			exit(0);
		case 0:
			if (longindex == 4) { // output_video
				snprintf(iio_conf->output_video_path, sizeof(iio_conf->output_video_path), "%s", optarg);
			} else if (longindex == 5) { // dbg_lvl
				conf->dbg_lvl = parce_int(optarg, 0, UINT8_MAX, 0);
				dbg_lvl_explicitly_set = true;
			}
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

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			print_help();
			exit(0);
		}
	}

	init_default_conf(conf);
	img_io_conf_t *iio_conf = conf->img_io_conf;

	const char *config_file = find_config_file_arg(argc, argv);
	if (config_file && parse_toml_config(config_file, conf) != OK)
		return EINVAL;

	if (apply_opts(conf, argc, argv, config_file))
		return EINVAL;

	int leftovers = argc - optind;
	if (leftovers > 1 || (leftovers == 1 && iio_conf->io_mode != not_selected)) {
		ddloge(TAG, "too many unknown args");
		print_help();
		return EINVAL;
	}
	if (leftovers == 1 && organize_input(argv[optind], conf) != OK)
		return EINVAL;

	if (iio_conf->io_mode == not_selected) {
		ddloge(TAG, "couldn't choose input");
		return EINVAL;
	}

	if (conf->dbg_lvl >= 2)
		print_vconf(conf->vision_conf);

	if (organize_img_output(iio_conf->output_dir, iio_conf) != OK) {
		ddloge(TAG, "failed for %s", iio_conf->output_dir);
		return EINVAL;
	}

	return OK;
}
