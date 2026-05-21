#include <stdint.h>
#include <math.h>
#include "defs.h"
#include "vision.h"


static int find_best_track(
	const tracker_context_t *tracker,
	const pixel_coord_t *center,
	const size_t tracks_count,
	const bool *track_updated
) {
	int best_track_idx = -1;
	uint32_t min_dist_sq = tracker->max_distance * tracker->max_distance;

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
	bool *track_updated
) {
	if (!new_centers) return;

	for (size_t i = 0; i < new_centers->size; i++) {
		pixel_coord_t *center = (pixel_coord_t *)vector_get(new_centers, i);
		if (!center) continue;

		int best_track_idx = find_best_track(tracker, center, initial_tracks_count, track_updated);

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
				.abnormality = 0
			};
			vector_push_back(tracker->active_tracks, &new_track);
		}
	}
}

static void handle_missing_tracks(
	tracker_context_t *tracker,
	const size_t initial_tracks_count,
	const bool *track_updated
) {
	for (int j = (int)initial_tracks_count - 1; j >= 0; j--) {
		if (track_updated[j]) continue;

		track_t *trk = (track_t *)vector_get(tracker->active_tracks, j);
		trk->missed_frames++;

		// If the object is lost for a long time, we delete the track
		if (trk->missed_frames > tracker->max_missed) {
			vector_erase(tracker->active_tracks, j); 
		} else {
			// Optional: continue to move the lost object by inertia
			trk->previous = trk->current;
			trk->current.x += trk->dx;
			trk->current.y += trk->dy;
		}
	}
}

static void detect_anomalous_track_old(
	tracker_context_t *tracker
) {
	if (!tracker || !tracker->active_tracks || tracker->active_tracks->size == 0) 
		return;

	int32_t sum_vx = 0, sum_vy = 0;
	size_t valid_tracks_count = 0;

	// 1. reset the old marks and calculate the average movement (camera/background movement)
	for (size_t i = 0; i < tracker->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);

		// We take into account only stable tracks that are currently being updated
		if (trk->age > 1 && trk->missed_frames == 0) {
			sum_vx += trk->dx;
			sum_vy += trk->dy;
			valid_tracks_count++;
		}
	}

	// At least a few points are needed to determine where the "majority" is going
	if (valid_tracks_count < 3) 
		return;

	int32_t global_dx = sum_vx / (int32_t)valid_tracks_count;
	int32_t global_dy = sum_vy / (int32_t)valid_tracks_count;

	// 2. look for the track with the largest deviation from the global movement
	uint32_t max_deviation_sq = 0;
	int max_abnormality_idx = -1;

	for (size_t i = 0; i < tracker->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);
		if (trk->age > 1 && trk->missed_frames == 0) {
			
			// To what extent the velocity vector of the object differs from the global one
			int32_t dx = trk->dx - global_dx;
			int32_t dy = trk->dy - global_dy;
			
			uint32_t deviation_sq = dx * dx + dy * dy;
			trk->abnormality = sqrt(deviation_sq);
		}
	}
}

static void detect_anomalous_track(tracker_context_t *tracker) {
	if (!tracker || !tracker->active_tracks || tracker->active_tracks->size == 0) 
		return;

	int32_t sum_vx = 0, sum_vy = 0;
	size_t valid_tracks_count = 0;

	// 1. find the global motion vector of the background
	for (size_t i = 0; i < tracker->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);

		if (trk->age > 2 && trk->missed_frames == 0) { // age > 2 щоб була prev_velocity
			sum_vx += trk->dx;
			sum_vy += trk->dy;
			valid_tracks_count++;
		}
	}

	if (valid_tracks_count < 3) return;

	int32_t global_vx = sum_vx / (int32_t)valid_tracks_count;
	int32_t global_vy = sum_vy / (int32_t)valid_tracks_count;

	uint32_t max_deviation_sq = 0;
	int best_anomaly_idx = -1;

	for (size_t i = 0; i < tracker->active_tracks->size; i++) {
		track_t *trk = (track_t *)vector_get(tracker->active_tracks, i);
		
		// check only "mature" tracks
		if (trk->age > 2 && trk->missed_frames == 0) {
			
			// --- FILTER 1: Scalar product (direction of movement) ---
			// V_current = (vx, vy), V_prev = (pvx, pvy)
			// Dot product = vx*pvx + vy*pvy
			// If the object moves randomly (Brownian motion), the angle between the vectors is often obtuse or 90°, 
			// so dot_product will be <= 0. We need steady forward motion.
			int32_t dot_product = (trk->dx * trk->prev_dx) + 
								  (trk->dy * trk->prev_dy);
			
			if (dot_product <= 0) {
				continue; // This is chaotic "Brownian" noise, let's skip it
			}

			// --- FILTER 2: Minimum amplitude of natural velocity ---
			// To filter out small jitter by 1-2 pixels
			uint32_t speed_sq = trk->dx * trk->dx + trk->dy * trk->dy;
			if (speed_sq < 9) { // For example, a rate of less than 3 pixels/frame is noise
				continue;
			}

			// --- ANOMALY CALCULATION ---
			int32_t diff_x = trk->dx - global_vx;
			int32_t diff_y = trk->dy - global_vy;
			
			uint32_t deviation_sq = diff_x * diff_x + diff_y * diff_y;
			trk->abnormality = sqrt(deviation_sq);
			//if (deviation_sq > max_deviation_sq) {
			//	max_deviation_sq = deviation_sq;
			//	best_anomaly_idx = (int)i;
			//}
		}
	}

	//uint32_t anomaly_threshold_sq = 25; // 5 пікселів різниці з фоном
	//if (best_anomaly_idx != -1 && max_deviation_sq > anomaly_threshold_sq) {
	//	track_t *trk = (track_t *)vector_get(tracker->active_tracks, best_anomaly_idx);
	//	trk->abnormality = 10; // crunch to pass > 8 to paint as red target
	//}
}

errno_t update_tracker(
	tracker_context_t *tracker,
	const vector_t *new_centers
) {
	if (!tracker || !tracker->active_tracks)
		return EINVAL;

	size_t initial_tracks_count = tracker->active_tracks->size;

	// An array to indicate whether the track received an update in this frame
	bool *track_updated = (bool *)calloc(initial_tracks_count, sizeof(bool));
	if (initial_tracks_count > 0 && !track_updated)
		return ENOMEM;

	match_new_centers(tracker, new_centers, initial_tracks_count, track_updated);
	handle_missing_tracks(tracker, initial_tracks_count, track_updated);
	detect_anomalous_track(tracker);

	free(track_updated);
	return OK;
}
