#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "defs.h"
#include "img_io.h"
#include "vision.h"
#include "my_vector.h"

#define TAG "io_flow "


static errno_t process_one_image(
	const main_conf_t *conf,
	tracker_context_t *tracker_ctx,
	double *precision,
	pixel_coord_t *out_label_drone
) {
	assert(conf);
	img_io_conf_t *iio_conf = (img_io_conf_t*)conf->img_io_conf;
	vision_conf_t *vconf = (vision_conf_t*)conf->vision_conf;
	image_t* img = image_load(iio_conf->input_filepath, GRAY);
	if (!img) {
		ddloge(TAG, "could not image_load %s", iio_conf->input_filepath);
		return EINVAL;
	}
	vconf->frame_size = (pixel_coord_t){.x = img->width, .y = img->height};
	pixel_coord_t label_drone = get_label_drone_coord(iio_conf);
	pixel_coord_t calculated_aim = (pixel_coord_t){.x = 0, .y = 0};

	if (out_label_drone) {
		*out_label_drone = label_drone;
	}

	// core processing
	vector_t *kpts = fast9(img, vconf->fast9_threshold, conf->dbg_lvl);
	clusters_context_t cctx = dbscan(kpts, vconf, conf->dbg_lvl);
	calculated_aim = update_tracker(tracker_ctx, cctx.centers, vconf, conf->dbg_lvl);

	// ddlogi(TAG, "dim coef %d", iio_conf->dim_coef);
	dim_img(img, iio_conf->dim_coef);
	locate_clusters_on_img(img, kpts, &cctx, true);
	locate_tracks_on_img(img, tracker_ctx, vconf->track_conf.deviation_squared_threshold);
	if (image_save_jpg(iio_conf->input_filepath, iio_conf->output_dir, img, conf->dbg_lvl) != OK)
		ddloge(TAG, "failed to save image");

	if (precision) {
		if (iio_conf->img_diagonal > 0) {
			double dx = (double)label_drone.x - (double)calculated_aim.x;
			double dy = (double)label_drone.y - (double)calculated_aim.y;
			double coord_label_calculated_diff = sqrt(dx * dx + dy * dy);
			*precision = coord_label_calculated_diff * 100.0 / (double)iio_conf->img_diagonal;
		} else {
			*precision = 0.0;
		}
	}
	if (cctx.ids)
		free(cctx.ids);
	if (cctx.centers)
		vector_destroy(cctx.centers);
	vector_destroy(kpts);
	image_free(img);
	return OK;
}


typedef struct {
	bool drone_detected;
	uint8_t steps_count;
	pixel_coord_t predicted_steps[TRACK_PREDICTION_STEPS_MAX];
} frame_pred_t;

#define PRED_HISTORY_LEN 32
#define PRED_MISS_THRESHOLD_PCT 5.0 // поріг похибки у % від діагоналі кадру, вище якого прогноз вважається несправдженим

static errno_t process_img_dir(
	main_conf_t *conf,
	vision_conf_t *vconf
) {
	assert(conf && conf->img_io_conf && conf->vision_conf);
	if (!((img_io_conf_t*)(conf->img_io_conf))->input_img_dir[0]) {
		ddloge(TAG, "no input_img_dir");
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
		.next_track_id = 0
	};

	frame_pred_t pred_history[PRED_HISTORY_LEN];
	memset(pred_history, 0, sizeof(pred_history));

	double sum_precision = 0.0;
	size_t count_precision_frames = 0;

	double sum_pred_error[TRACK_PREDICTION_STEPS_MAX] = {0};
	size_t count_pred_samples[TRACK_PREDICTION_STEPS_MAX] = {0};
	size_t count_pred_misses[TRACK_PREDICTION_STEPS_MAX] = {0};

	uint8_t max_eval_steps = vconf->track_conf.prediction_steps;
	if (max_eval_steps == 0) max_eval_steps = TRACK_PREDICTION_STEPS_DEFAULT;
	if (max_eval_steps > TRACK_PREDICTION_STEPS_MAX) max_eval_steps = TRACK_PREDICTION_STEPS_MAX;

	clock_t start = clock();
	for (size_t i = 0; i < filenames->size; i++) {
		print_progress_bar(__func__, i + 1, filenames->size);
		double precision = 0.0;	// img diagonal percent 2d distance between labled drone and calculated aim
		pixel_coord_t label_drone = {0, 0};
		str_t *filename = (str_t*)vector_get(filenames, i);
		if (!filename)
			break;
		snprintf(iio_conf->input_filepath, STR_MAX_LEN, "%s/%s", iio_conf->input_img_dir , filename->name);
		process_one_image(conf, &tracker_ctx, &precision, &label_drone);

		sum_precision += precision;
		count_precision_frames++;

		// Evaluate predictions made by past frames for the current frame i
		if (label_drone.x > 0 || label_drone.y > 0) {
			for (uint8_t s = 1; s <= max_eval_steps; s++) {
				if (i >= s) {
					size_t past_idx = (i - s) % PRED_HISTORY_LEN;
					frame_pred_t *past = &pred_history[past_idx];
					if (past->drone_detected && s <= past->steps_count) {
						pixel_coord_t pred_coord = past->predicted_steps[s - 1];
						double dx = (double)label_drone.x - (double)pred_coord.x;
						double dy = (double)label_drone.y - (double)pred_coord.y;
						double pred_diff = sqrt(dx * dx + dy * dy);
						double pred_err_pct = (iio_conf->img_diagonal > 0)
							? (pred_diff * 100.0 / (double)iio_conf->img_diagonal)
							: 0.0;
						sum_pred_error[s - 1] += pred_err_pct;
						count_pred_samples[s - 1]++;
						if (pred_err_pct > PRED_MISS_THRESHOLD_PCT) {
							count_pred_misses[s - 1]++;
						}
					}
				}
			}
		}

		// Save current frame's predictions into circular history buffer
		frame_pred_t *curr_pred = &pred_history[i % PRED_HISTORY_LEN];
		curr_pred->drone_detected = tracker_ctx.drone_detected;
		curr_pred->steps_count = tracker_ctx.drone_predicted_steps_count;
		for (uint8_t s = 0; s < tracker_ctx.drone_predicted_steps_count && s < TRACK_PREDICTION_STEPS_MAX; s++) {
			curr_pred->predicted_steps[s] = tracker_ctx.drone_predicted_steps[s];
		}
	}
	double cpu_time_used_ms = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000;
	double avg_distance = (count_precision_frames > 0) ? (sum_precision / (double)count_precision_frames) : 0.0;

	if (count_pred_samples[0] > 0) {
		double avg_pred_1 = sum_pred_error[0] / (double)count_pred_samples[0];
		double miss_rate_1 = ((double)count_pred_misses[0] * 100.0) / (double)count_pred_samples[0];
		printf(" %.0f ms, avg precision %.2f%%, avg pred precision %.2f%% (miss >%.1f%%: %.2f%%)\n",
			cpu_time_used_ms, avg_distance, avg_pred_1, PRED_MISS_THRESHOLD_PCT, miss_rate_1);
		printf("  prediction errors by horizon (steps ahead):\n");
		for (uint8_t s = 1; s <= max_eval_steps; s++) {
			if (count_pred_samples[s - 1] > 0) {
				double step_err = sum_pred_error[s - 1] / (double)count_pred_samples[s - 1];
				double step_miss_pct = ((double)count_pred_misses[s - 1] * 100.0) / (double)count_pred_samples[s - 1];
				printf("    step +%d: %.2f%%, miss %.2f%% (%zu/%zu samples)\n",
					s, step_err, step_miss_pct, count_pred_misses[s - 1], count_pred_samples[s - 1]);
			}
		}
	} else {
		printf(" %.0f ms, avg precision %.2f%%\n", cpu_time_used_ms, avg_distance);
	}

	images_to_video(iio_conf->output_dir, iio_conf->output_video_path);

	vector_destroy(tracker_ctx.active_tracks);
	vector_destroy(filenames);
	return OK;
}


errno_t apply_io_mode(
	main_conf_t *conf
) {
	assert(conf);

	img_io_conf_t *iio_conf = conf->img_io_conf;
	vision_conf_t *vconf = conf->vision_conf;
	errno_t err = OK;

	switch (iio_conf->io_mode) {
	case not_selected:
		ddloge(TAG, "io_mode not selected");
		return EINVAL;
	case single_img_file:
		err = process_one_image(conf, NULL, NULL, NULL);
		break;
	case input_img_dir:
		err = process_img_dir(conf, vconf);
		break;
	default:
		ddloge(TAG, "unknown io_mode");
		return EINVAL;
	}

	return err;
}
