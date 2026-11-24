#include <assert.h>
#include <stdint.h>
#include <math.h>

#include "defs.h"
#include "vision.h"
#include "kalman.h"

#define TAG "track "


static int find_best_track(
	const tracker_context_t *tracker,
	const pixel_coord_t *center,
	const size_t tracks_count,
	const bool *track_updated,
	const uint16_t max_distance
) {
	int best_track_idx = -1;
	uint32_t min_dist_sq = max_distance * max_distance;

	// Look for nearest track using Kalman filter predicted position for the current frame
	for (size_t j = 0; j < tracks_count; j++) {
		if (track_updated[j]) continue;

		track_t *trk = (track_t *)vector_get(tracker->active_tracks, j);

		int32_t predicted_x = (int32_t)roundf(trk->kf.kf_x.x);
		int32_t predicted_y = (int32_t)roundf(trk->kf.kf_y.x);

		int32_t dx = (int32_t)center->x - predicted_x;
		int32_t dy = (int32_t)center->y - predicted_y;
		uint32_t dist_sq = (uint32_t)(dx * dx + dy * dy);

		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			best_track_idx = (int)j;
		}
	}
	return best_track_idx;
}


static void match_new_centers(
	tracker_context_t *tracker,
	const vector_t *new_centers,
	const size_t initial_tracks_count,
	const uint16_t max_distance,
	bool *track_updated
) {
	assert(new_centers);

	for (size_t i = 0; i < new_centers->size; i++) {
		pixel_coord_t *center = (pixel_coord_t *)vector_get(new_centers, i);
		if (!center) continue;

		int best_track_idx = find_best_track(tracker, center, initial_tracks_count, track_updated, max_distance);

		// If a suitable track is found - update it with Kalman measurement
		if (best_track_idx != -1) {
			track_t *trk = (track_t *)vector_get(tracker->active_tracks, best_track_idx);
			trk->previous = trk->current;
			trk->current = *center;

			// Kalman measurement update
			kalman_2d_update(&trk->kf, (float)center->x, (float)center->y);

			trk->prev_dx = trk->dx;
			trk->prev_dy = trk->dy;
			trk->dx = (int16_t)roundf(trk->kf.kf_x.v);
			trk->dy = (int16_t)roundf(trk->kf.kf_y.v);

			// Predicted position for the next frame (k + 1)
			trk->predicted_next = kalman_2d_get_predicted_pos(&trk->kf, 1.0f);

			trk->age++;
			trk->missed_frames = 0;
			track_updated[best_track_idx] = true;
		} else {
			// If not found - this is a new object, create track and initialize Kalman filter
			track_t new_track = {
				.id = tracker->next_track_id++,
				.current = *center,
				.previous = *center,
				.dx = 0,
				.dy = 0,
				.prev_dx = 0,
				.prev_dy = 0,
				.age = 1,
				.missed_frames = 0,
				.deviation_squared = 0,
				.is_most_deviated = false
			};
			kalman_2d_init(&new_track.kf, (float)center->x, (float)center->y,
			               KF_Q_POS_DEFAULT, KF_Q_VEL_DEFAULT, KF_R_DEFAULT);
			new_track.predicted_next = kalman_2d_get_predicted_pos(&new_track.kf, 1.0f);
			vector_push_back(tracker->active_tracks, &new_track);
		}
	}
}


static void handle_missing_tracks(
	tracker_context_t *tracker,
	const size_t initial_tracks_count,
	const bool *track_updated,
	uint16_t max_missed
) {
	for (int j = (int)initial_tracks_count - 1; j >= 0; j--) {
		if (track_updated[j]) continue;

		track_t *trk = (track_t *)vector_get(tracker->active_tracks, j);
		trk->missed_frames++;

		// If the object is lost for too long, delete the track
		if (trk->missed_frames > max_missed) {
			vector_erase(tracker->active_tracks, j); 
		} else {
			// Object missed in this frame: coast forward along Kalman predicted trajectory
			trk->previous = trk->current;
			trk->current = kalman_2d_get_current_pos(&trk->kf);
			trk->prev_dx = trk->dx;
			trk->prev_dy = trk->dy;
			trk->dx = (int16_t)roundf(trk->kf.kf_x.v);
			trk->dy = (int16_t)roundf(trk->kf.kf_y.v);
			trk->predicted_next = kalman_2d_get_predicted_pos(&trk->kf, 1.0f);
		}
	}
}


static void detect_anomalous_track(
	tracker_context_t *tracker,
	const vision_conf_t *vconf,
	pixel_coord_t *aim
) {
	if (!tracker || !tracker->active_tracks || tracker->active_tracks->size == 0) 
		return;

	tracker->drone_detected = false;

	float sum_vx = 0.0f, sum_vy = 0.0f;
	size_t valid_tracks_count = 0;

	// 1. Find global background motion vector using Kalman filtered velocities
	for (size_t i = 0; i < tracker->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);
		if (trk->age > 2 && trk->missed_frames == 0) {
			sum_vx += trk->kf.kf_x.v;
			sum_vy += trk->kf.kf_y.v;
			valid_tracks_count++;
		}
	}

	float global_vx = 0.0f, global_vy = 0.0f;
	if (valid_tracks_count >= 2) {
		global_vx = sum_vx / (float)valid_tracks_count;
		global_vy = sum_vy / (float)valid_tracks_count;
	}

	uint64_t max_score_strict = 0;
	int best_strict_idx = -1;

	uint64_t max_score_fallback = 0;
	int best_fallback_idx = -1;

	for (size_t i = 0; i < tracker->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);
		
		// Capture target status from previous frame before resetting it
		bool was_target = trk->is_most_deviated;
		trk->is_most_deviated = false;

		if (trk->missed_frames == 0) {
			// --- ANOMALY CALCULATION ---
			// Compute motion deviation relative to global background using Kalman velocities
			float rel_vx = trk->kf.kf_x.v - global_vx;
			float rel_vy = trk->kf.kf_y.v - global_vy;
			float dev_sq = rel_vx * rel_vx + rel_vy * rel_vy;
			trk->deviation_squared = (uint16_t)roundf(dev_sq);

			// --- KINEMATIC STABILITY SCORING ---
			// Base tracking score combines deviation and track maturity:
			// Score = D^2 * Age
			uint64_t score = (uint64_t)trk->deviation_squared * trk->age;

			// --- TARGET HYSTERESIS (STICKY LOCK) ---
			// If this specific track was tracked in the previous frame, multiply its score
			if (was_target)
				score *= 3;

			// --- SUBSTITUTE CANDIDATE (Fallback) ---
			if (score >= max_score_fallback) {
				max_score_fallback = score;
				best_fallback_idx = (int)i;
			}
			
			// --- QUALITY CANDIDATE (Strict) ---
			// Forward motion consistency check:
			if (trk->age > 2) {
				int32_t dot_prod = (trk->dx * trk->prev_dx) + (trk->dy * trk->prev_dy);
				
				if (dot_prod > 0) {
					if (score >= max_score_strict) {
						max_score_strict = score;
						best_strict_idx = (int)i;
					}
				}
			}
		}
	}

	// Priority is a high-quality track (strict). If there is no such thing, take fallback.
	int target_idx = (best_strict_idx != -1) ? best_strict_idx : best_fallback_idx;

	if (target_idx != -1) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, target_idx);
		trk->is_most_deviated = true;
		tracker->drone_detected = true;
		tracker->drone_predicted = trk->predicted_next;
		*aim = (pixel_coord_t){
			.x = trk->current.x,
			.y = trk->current.y
		};
	}
}


pixel_coord_t update_tracker(
	tracker_context_t *tracker,
	const vector_t *new_centers,
	const vision_conf_t *vconf,
	const uint8_t dbg_lvl
) {
	pixel_coord_t aim = {0, 0};
	if (!tracker || !tracker->active_tracks)
		return aim;

	size_t initial_tracks_count = tracker->active_tracks->size;

	// Predict next state for all existing tracks to current frame
	for (size_t i = 0; i < initial_tracks_count; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);
		if (trk) {
			kalman_2d_predict(&trk->kf, 1.0f);
		}
	}

	// An array to indicate whether the track received an update in this frame
	bool *track_updated = NULL;
	if (initial_tracks_count > 0) {
		track_updated = (bool *)calloc(initial_tracks_count, sizeof(bool));
		if (!track_updated)
			return aim;
	}

	if (new_centers && new_centers->size > 0)
		match_new_centers(tracker, new_centers, initial_tracks_count, vconf->track_conf.max_distance, track_updated);
	handle_missing_tracks(tracker, initial_tracks_count, track_updated, vconf->track_conf.max_missed);
	detect_anomalous_track(tracker, vconf, &aim);

	if (dbg_lvl)
		printf(" got tracks ntid %d, aim: x %d y %d, pred: x %d y %d",
			tracker->next_track_id, aim.x, aim.y, tracker->drone_predicted.x, tracker->drone_predicted.y);

	free(track_updated);
	return aim;
}
