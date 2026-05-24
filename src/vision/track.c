#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#include "defs.h"
#include "vision.h"

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

	// Look for nearest track
	for (size_t j = 0; j < tracks_count; j++) {
		if (track_updated[j]) continue;

		track_t *trk = (track_t *)vector_get(tracker->active_tracks, j);

		// predict where the object should be, using its past speed
		int32_t predicted_x = trk->current.x + trk->dx;
		int32_t predicted_y = trk->current.y + trk->dy;

		int32_t dx = center->x - predicted_x;
		int32_t dy = center->y - predicted_y;
		uint32_t dist_sq = dx * dx + dy * dy;

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

		// If a suitable track is found - we update it
		if (best_track_idx != -1) {
			track_t *trk = (track_t *)vector_get(tracker->active_tracks, best_track_idx);
			trk->previous = trk->current;
			trk->current = *center;
			trk->prev_dx = trk->dx;
			trk->prev_dy = trk->dy;
			trk->dx = trk->current.x - trk->previous.x;
			trk->dy = trk->current.y - trk->previous.y;
			trk->age++;
			trk->missed_frames = 0;
			track_updated[best_track_idx] = true;
		} else {
			// If not found - this is a new object, create a track
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
				.is_most_deviated = 0
			};
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

		// If the object is lost for a long time, we delete the track
		if (trk->missed_frames > max_missed) {
			vector_erase(tracker->active_tracks, j); 
		} else {
			// Optional: continue to move the lost object by inertia
			trk->previous = trk->current;
			trk->current.x += trk->dx;
			trk->current.y += trk->dy;
		}
	}
}


static void detect_anomalous_track(
	tracker_context_t *tracker,
	pixel_coord_t *aim
) {
	if (!tracker || !tracker->active_tracks || tracker->active_tracks->size == 0) 
		return;

	int32_t sum_dx = 0, sum_dy = 0;
	size_t valid_tracks_count = 0;

	// 1. find the global motion vector of the background
	for (size_t i = 0; i < tracker->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);
		if (trk->age > 2 && trk->missed_frames == 0) { // age > 2 for prev_d<x|y>
			sum_dx += trk->dx;
			sum_dy += trk->dy;
			valid_tracks_count++;
		}
	}

	if (valid_tracks_count == 0) return;

	int16_t global_dx = sum_dx / (int16_t)valid_tracks_count;
	int16_t global_dy = sum_dy / (int16_t)valid_tracks_count;

	// Unified scoring system to heavily rely on track maturity (age) and target lock (hysteresis)
	uint64_t max_score_strict = 0;
	int best_strict_idx = -1;

	uint64_t max_score_fallback = 0;
	int best_fallback_idx = -1;

	for (size_t i = 0; i < tracker->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);
		
		// Capture target status from the previous frame before resetting it
		bool was_target = trk->is_most_deviated;
		trk->is_most_deviated = false;

		if (trk->missed_frames == 0) {
			// --- ANOMALY CALCULATION ---
			int32_t dx = trk->dx - global_dx;
			int32_t dy = trk->dy - global_dy;
			trk->deviation_squared = dx * dx + dy * dy;

			// --- KINEMATIC STABILITY SCORING ---
			// Base tracking score combines deviation and track maturity:
			// $Score = D^2 \times Age$
			// This scaling prevents short-lived noise spikes from hijacking the aim.
			uint64_t score = (uint64_t)trk->deviation_squared * trk->age;

			// --- TARGET HYSTERESIS (STICKY LOCK) ---
			// If this specific track was tracked in the previous frame, multiply its score.
			// This anchors the camera lock and stops the aim from erratically jumping across the screen.
			if (was_target)
				score *= 3;

			// --- SUBSTITUTE CANDIDATE (Fallback) ---
			// Evaluates all active entities using the robust age-weighted scoring matrix
			if (score >= max_score_fallback) {
				max_score_fallback = score;
				best_fallback_idx = (int)i;
			}
			
			// --- QUALITY CANDIDATE (Strict) ---
			// Dot product = dx*pdx + dy*pdy
			// If the object moves randomly (Brownian motion), the angle between the vectors is often obtuse or 90°, 
			// so dot_product will be <= 0. We need steady forward motion.
			if (trk->age > 2) {
				int32_t dot_prod =	(trk->dx * trk->prev_dx) +
									(trk->dy * trk->prev_dy);
				
				// Only if the movement is consistent
				if (dot_prod > 0) {
					if (score >= max_score_strict) {
						max_score_strict = score;
						best_strict_idx = (int)i;
					}
				}
			}
		}
	}

	// The priority is a high-quality track (strict). If there is no such thing - take at least noise (fallback).
	int target_idx = (best_strict_idx != -1) ? best_strict_idx : best_fallback_idx;

	if (target_idx != -1) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, target_idx);
		trk->is_most_deviated = true;
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

	// An array to indicate whether the track received an update in this frame
	bool *track_updated = (bool *)calloc(initial_tracks_count, sizeof(bool));
	if (initial_tracks_count > 0 && !track_updated)
		return aim;

	match_new_centers(tracker, new_centers, initial_tracks_count, vconf->track_conf.max_distance, track_updated);
	handle_missing_tracks(tracker, initial_tracks_count, track_updated, vconf->track_conf.max_missed);
	detect_anomalous_track(tracker, &aim);

	if (dbg_lvl)
		printf(" got tracks ntid %d, aim: x %d y %d", tracker->next_track_id, aim.x, aim.y);	// just in progress bar line

	free(track_updated);
	return aim;
}
