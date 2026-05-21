#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defs.h"
#include "img_defs.h"
#include "img_io.h"
#include "vision.h"
#include "my_vector.h"


#define TAG "io_flow "


/**
 * @brief ...
 */
static errno_t process_one_image(
	const main_conf_t *conf,
	vision_conf_t *vconf,
	tracker_context_t *tracker_ctx
) {
	image_t* img = image_load(conf->input_filepath, GRAY);
	if (!img) {
		ddloge(TAG, "could not image_load %s", conf->input_filepath);
		return EINVAL;
	}
	vconf->frame_width = img->width;
	vconf->frame_height = img->height;

	vector_t *kpts = fast9(img, vconf->fast9_threshold);
	clusters_context_t cctx = dbscan(kpts, vconf, conf->is_test);
	update_tracker(tracker_ctx, cctx.centers);
	if (tracker_ctx)
		printf(" got tracks ntid %d ", tracker_ctx->next_track_id);

	locate_clusters_on_img(img, kpts, &cctx, MAX_DIM_COEF);
	locate_tracks_on_img(img, tracker_ctx, conf->dim_coef);

	if (image_save_jpg(conf->input_filepath, conf->output_dir, img, conf->is_test) != OK)
		ddloge(TAG, "failed to save image");

	if (cctx.ids)
		free(cctx.ids);
	if (cctx.centers)
		vector_destroy(cctx.centers);
	vector_destroy(kpts);
	image_free(img);
	return OK;
}


static errno_t process_img_dir(
	main_conf_t *conf,
	vision_conf_t *vconf
) {
	if (!conf || !conf->input_img_dir[0]) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}
	vector_t *filenames = get_filepathes_from_dir(conf->input_img_dir);
	if (!filenames || !filenames->size) {
		ddloge(TAG, "couldn't get_filepathes_from_dir %s", conf->input_img_dir);
		return EIO;
	}
	tracker_context_t tracker_ctx = {
		.active_tracks = vector_create(10, sizeof(track_t)),
		.next_track_id = 0,
		.max_distance = 50,
		.max_missed = 5
	};

	clock_t start = clock();
	for (size_t i = 0; i < filenames->size; i++) {
		print_progress_bar(__func__, i + 1, filenames->size);
		str_t *filename = (str_t*)vector_get(filenames, i);
		if (!filename)
			break;
		snprintf(conf->input_filepath, STR_MAX_LEN, "%s/%s", conf->input_img_dir , filename->name);
		process_one_image(conf, vconf, &tracker_ctx);
	}
	double cpu_time_used_ms = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000;
	printf(" %.0f ms\n", cpu_time_used_ms);

	images_to_video(conf->output_dir, DEFAULT_OUTPUT_DIR);

	vector_destroy(tracker_ctx.active_tracks);
	vector_destroy(filenames);
	return OK;	// hope so
}


errno_t apply_io_mode(
	main_conf_t *conf
) {
	if (!conf) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}

	vision_conf_t vconf = {
		.frame_width = 0,
		.frame_height = 0,
		.dbscan_max_distance_img_diagonal_percent = DBSCAN_DEFAULT_MAX_DISTANCE_IMG_DIAGONAL_PERCENT,
		.dbscan_min_cluster_size = DBSCAN_MIN_CLUSTER_SIZE,
		.dbscan_enable_geometry_filtering = 1,
		.fast9_threshold = DEFAULT_THRESHOLD
	};

	errno_t err;
	// clock_t start = clock();

	switch (conf->io_mode) {
	case not_selected:
		ddloge(TAG, "io_mode not selected");
		return EINVAL;
	case single_img_file:
		conf->is_test = 1;
		err = process_one_image(conf, &vconf, NULL);
		break;
	case input_img_dir:
		err = process_img_dir(conf, &vconf);
		break;
	}

	// double cpu_time_used_ms = ((double) (clock() - start)) / CLOCKS_PER_SEC * 1000;
	// ddlogi(TAG, "took %.3f ms", cpu_time_used_ms);

	return err;
}
