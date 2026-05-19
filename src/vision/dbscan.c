// #include <cstddef.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "defs.h"
#include "img_defs.h"
#include "my_vector.h"
#include "vision.h"

#define TAG "dbscan "


static errno_t get_neighbors(
	const vector_t *keypoints,
	const size_t index,
	const uint8_t max_distance,
	vector_t *neighbors
) {
	pixel_coord_t *point = (pixel_coord_t*)vector_get(keypoints, index);
	if (!point) {
		ddloge(TAG, "couldn't vector_get");
		return -1;
	}

	vector_clear(neighbors);
	vector_reserve(neighbors, keypoints->size);
	for (size_t i = 0; i < keypoints->size; i++) {
		if (i == index)
			continue;
		pixel_coord_t *other = (pixel_coord_t *)vector_get(keypoints, i);
		int32_t dx = point->x - other->x;
		int32_t dy = point->y - other->y;
		if ((dx * dx + dy * dy) <= (int32_t)max_distance * max_distance)
			if (vector_push_back(neighbors, &i) != OK) {
				ddloge(TAG, "failed to vector_push_back neighbor");
				return EIO;
			}
	}
	return OK;
}


static errno_t expand_cluster(
	const size_t index,
	const uint8_t max_distance,
	const uint16_t min_points,
	const vector_t *keypoints,
	clusters_context_t *cctx
) {
	vector_t *seeds = vector_create(keypoints->size, sizeof(size_t));
	if (!seeds) {
		ddloge(TAG, "invalid args");
		return ENOMEM;
	}

	if (get_neighbors(keypoints, index, max_distance, seeds) != OK) {
		vector_destroy(seeds);
		ddloge(TAG, "couldn't get_neighbors");
		return -1;
	}

	if (seeds->size < min_points) {
		cctx->ids[index] = DBSCAN_NOISE;
		vector_destroy(seeds);
		return OK;
	}

	cctx->ids[index] = cctx->unique_count;
	for (size_t i = 0; i < seeds->size; i++) {
		uint8_t neighbor_index = *(size_t*)vector_get(seeds, i);
		cctx->ids[neighbor_index] = cctx->unique_count;
	}

	for (size_t seed_index = 0; seed_index < seeds->size; seed_index++) {
		uint8_t current_index = *(size_t *)vector_get(seeds, seed_index);
		vector_t *neighbors = vector_create(keypoints->size, sizeof(size_t));
		if (!neighbors) {
			vector_destroy(seeds);
			ddloge(TAG, "couldn't vector_create");
			return ENOMEM;
		}

		if (get_neighbors(keypoints, current_index, max_distance, neighbors) != OK) {
			vector_destroy(neighbors);
			vector_destroy(seeds);
			ddloge(TAG, "couldn't get_neighbors");
			return -1;
		}

		if (neighbors->size >= min_points) {
			for (size_t j = 0; j < neighbors->size; j++) {
				size_t n_index = *(size_t *)vector_get(neighbors, j);
				if (cctx->ids[n_index] == DBSCAN_CLUSTER_UNCLASSIFIED || cctx->ids[n_index] == DBSCAN_NOISE) {
					if (cctx->ids[n_index] == DBSCAN_CLUSTER_UNCLASSIFIED)
						vector_push_back(seeds, &n_index);
					cctx->ids[n_index] = cctx->unique_count;
				}
			}
		}
		vector_destroy(neighbors);
	}

	vector_destroy(seeds);
	return OK;
}


#include <string.h>

errno_t calculate_and_filter_cluster_centers(
	clusters_context_t *cctx,
	const vector_t *keypoints,
	const config_t *conf
) {
	if (!keypoints || !cctx || cctx->unique_count == 0 || !cctx->ids) {
		return EINVAL;
	}

	if (!cctx->centers) {
		ddloge(TAG, "failed to allocate centers array");
		return ENOMEM;
	}

	// create temporary arrays for accumulating coordinates and finding boundaries (Bounding Box), mb to use VLA
	uint32_t *sum_x = (uint32_t *)calloc(cctx->unique_count, sizeof(uint32_t));
	uint32_t *sum_y = (uint32_t *)calloc(cctx->unique_count, sizeof(uint32_t));
	uint32_t *pts_count = (uint32_t *)calloc(cctx->unique_count, sizeof(uint32_t));
	
	uint16_t *min_x = (uint16_t *)malloc(cctx->unique_count * sizeof(uint16_t));
	uint16_t *max_x = (uint16_t *)malloc(cctx->unique_count * sizeof(uint16_t));
	uint16_t *min_y = (uint16_t *)malloc(cctx->unique_count * sizeof(uint16_t));
	uint16_t *max_y = (uint16_t *)malloc(cctx->unique_count * sizeof(uint16_t));

	if (!sum_x || !sum_y || !pts_count || !min_x || !max_x || !min_y || !max_y) {
		free(sum_x); free(sum_y); free(pts_count);
		free(min_x); free(max_x); free(min_y); free(max_y);
		return ENOMEM;
	}

	// Boundries init
	for (uint8_t g = 0; g < cctx->unique_count; g++) {
		min_x[g] = 0xFFFF; min_y[g] = 0xFFFF;
		max_x[g] = 0;      max_y[g] = 0;
	}

	// The first pass: we collect metrics for each cluster
	for (size_t i = 0; i < keypoints->size; i++) {
		uint8_t cluster_id = cctx->ids[i];

		if (cluster_id == DBSCAN_NOISE || cluster_id == DBSCAN_CLUSTER_UNCLASSIFIED)
			continue;

		if (cluster_id >= cctx->unique_count)
			continue;

		pixel_coord_t *p = (pixel_coord_t *)vector_get(keypoints, i);
		if (!p)
			continue;

		sum_x[cluster_id] += p->x;
		sum_y[cluster_id] += p->y;
		pts_count[cluster_id]++;

		if (p->x < min_x[cluster_id]) min_x[cluster_id] = p->x;
		if (p->x > max_x[cluster_id]) max_x[cluster_id] = p->x;
		if (p->y < min_y[cluster_id]) min_y[cluster_id] = p->y;
		if (p->y > max_y[cluster_id]) max_y[cluster_id] = p->y;
	}

	// Second pass: we analyze the geometry and calculate the final centers
	for (uint8_t g = 0; g < cctx->unique_count; g++) {
		if (pts_count[g] == 0) {
			cctx->centers[g] = (pixel_coord_t){.x = 0, .y = 0};
			continue;
		}

		uint16_t cluster_w = max_x[g] - min_x[g] + 1;
		uint16_t cluster_h = max_y[g] - min_y[g] + 1;

		// --- CRITERIA FOR FILTERING LARGE CLOUDS AND ANOMALIES ---
		
		// Size Threshold: If the cluster is larger than 40% of the width or height of the frame
		bool is_too_large = (cluster_w > (conf->frame_width  / 2)) || 
		                    (cluster_h > (conf->frame_height / 2));

		// Shape threshold ("long bean" / line): aspect ratio greater than 3.5
		float aspect_ratio1 = (float)cluster_w / (float)cluster_h;
		float aspect_ratio2 = (float)cluster_h / (float)cluster_w;
		bool is_too_eccentric = (aspect_ratio1 > 3.5f) || (aspect_ratio2 > 3.5f);

		if (is_too_large || is_too_eccentric) {	// The cluster is not like a drone
			cctx->centers[g] = (pixel_coord_t){.x = 0, .y = 0};
			if (conf->is_test)
				ddlogw(TAG, "cluster %d filtered out (W:%d, H:%d)%s%s.", g, cluster_w, cluster_h,
					is_too_large? " too_large" : "", is_too_eccentric? " too_eccentric" : "");
		} else {
			// The cluster is valid - we calculate the geometric mean
			cctx->centers[g].x = (uint16_t)(sum_x[g] / pts_count[g]);
			cctx->centers[g].y = (uint16_t)(sum_y[g] / pts_count[g]);
			if (conf->is_test)
				ddlogi(TAG, "cluster %d center calculated: x=%d, y=%d", g, cctx->centers[g].x, cctx->centers[g].y);
		}
	}

	free(sum_x); free(sum_y); free(pts_count);
	free(min_x); free(max_x); free(min_y); free(max_y);
	return OK;
}


clusters_context_t dbscan(
	const vector_t *keypoints,
	const uint16_t min_points,
	const config_t *conf
) {
	if (!keypoints || !min_points) {
		ddloge(TAG, "invalid arg");
		return (clusters_context_t){0};
	}

	clusters_context_t cctx = {
		.ids = calloc(keypoints->size, sizeof(uint8_t)),
		.size = keypoints->size,
		.unique_count = 0,
		.centers = NULL
	};

	uint16_t max_distance = (conf->frame_width + conf->frame_height) / 20;

	for (size_t i = 0; i < keypoints->size; i++)
		cctx.ids[i] = DBSCAN_CLUSTER_UNCLASSIFIED;

	for (size_t i = 0; i < keypoints->size; i++)
		if (cctx.ids[i] == DBSCAN_CLUSTER_UNCLASSIFIED)
			if (expand_cluster(i, max_distance, min_points, keypoints, &cctx) == OK)
				cctx.unique_count++;

	cctx.centers = (pixel_coord_t *)calloc(((cctx.unique_count)? cctx.unique_count : 1), sizeof(pixel_coord_t));

	if (cctx.unique_count > 0)
		calculate_and_filter_cluster_centers(&cctx, keypoints, conf);

	return cctx;
}
