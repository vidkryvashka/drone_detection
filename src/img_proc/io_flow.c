#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defs.h"
#include "img_io.h"
#include "vision.h"
#include "my_vector.h"

#define TAG "io_flow "


static errno_t process_one_image(
	const main_conf_t *conf,
	tracker_context_t *tracker_ctx
) {
	img_io_conf_t *iio_conf = (img_io_conf_t*)conf->img_io_conf;
	vision_conf_t *vconf = (vision_conf_t*)conf->vision_conf;
	image_t* img = image_load(iio_conf->input_filepath, GRAY);
	if (!img) {
		ddloge(TAG, "could not image_load %s", iio_conf->input_filepath);
		return EINVAL;
	}
	vconf->frame_size = (pixel_coord_t){.x = img->width, .y = img->height};


	// core processing
	vector_t *kpts = fast9(img, vconf->fast9_threshold, conf->dbg_lvl);
	clusters_context_t cctx = dbscan(kpts, vconf, conf->dbg_lvl);
	update_tracker(tracker_ctx, cctx.centers, conf->dbg_lvl);


	dim_img(img, iio_conf->dim_coef);
	locate_clusters_on_img(img, kpts, &cctx, true);
	locate_tracks_on_img(img, tracker_ctx, vconf->track_conf.deviation_squared_threshold);
	if (image_save_jpg(iio_conf->input_filepath, iio_conf->output_dir, img, conf->dbg_lvl) != OK)
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
	if (!conf ||
		!conf->img_io_conf ||
		!conf->vision_conf ||
		!((img_io_conf_t*)(conf->img_io_conf))->input_img_dir[0]
	) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}

	img_io_conf_t *iio_conf = (img_io_conf_t*)conf->img_io_conf;
	vector_t *filenames = get_filepathes_from_dir(iio_conf->input_img_dir);
	if (!filenames || !filenames->size) {
		ddloge(TAG, "couldn't get_filepathes_from_dir %s", iio_conf->input_img_dir);
		return EIO;
	}
	tracker_context_t tracker_ctx = {
		.active_tracks = vector_create(10, sizeof(track_t)),
		.next_track_id = 0,
		.max_distance = vconf->track_conf.max_distance,
		.max_missed = vconf->track_conf.max_missed
	};

	clock_t start = clock();
	for (size_t i = 0; i < filenames->size; i++) {
		print_progress_bar(__func__, i + 1, filenames->size);
		str_t *filename = (str_t*)vector_get(filenames, i);
		if (!filename)
			break;
		snprintf(iio_conf->input_filepath, STR_MAX_LEN, "%s/%s", iio_conf->input_img_dir , filename->name);
		process_one_image(conf, &tracker_ctx);
	}
	double cpu_time_used_ms = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000;
	printf(" %.0f ms\n", cpu_time_used_ms);

	images_to_video(iio_conf->output_dir, iio_conf->output_video_path);

	vector_destroy(tracker_ctx.active_tracks);
	vector_destroy(filenames);
	return OK;
}


errno_t apply_io_mode(
	main_conf_t *conf
) {
	if (!conf) {
		ddloge(TAG, "invalid arg");
		return EINVAL;
	}

	img_io_conf_t *iio_conf = conf->img_io_conf;
	vision_conf_t *vconf = conf->vision_conf;

	errno_t err;
	// clock_t start = clock();

	switch (iio_conf->io_mode) {
	case not_selected:
		ddloge(TAG, "io_mode not selected");
		return EINVAL;
	case single_img_file:
		err = process_one_image(conf, NULL);
		break;
	case input_img_dir:
		err = process_img_dir(conf, vconf);
		break;
	}

	// double cpu_time_used_ms = ((double) (clock() - start)) / CLOCKS_PER_SEC * 1000;
	// ddlogi(TAG, " %.3f ms", cpu_time_used_ms);

	return err;
}
