// #include <cstddef.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "my_vector.h"
#include "vision.h"

#define TAG "dbscan "


static errno_t get_neighbors(
	const vector_t *keypoints,
	const size_t index,
	const uint16_t max_distance,
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
	const uint16_t max_distance,
	const uint8_t min_cluster_size,
	const vector_t *keypoints,
	clusters_context_t *cctx
) {
	// Creating a widthwise traversal (BFS) queue
	vector_t *seeds = vector_create(keypoints->size, sizeof(size_t));
	if (!seeds) {
		ddloge(TAG, "couldn't vector_create seeds");
		return ENOMEM;
	}

	// find the first neighbors for the starting point
	if (get_neighbors(keypoints, index, max_distance, seeds) != OK) {
		vector_destroy(seeds);
		ddloge(TAG, "couldn't get_neighbors for index %zu", index);
		return -1;
	}

	// Check on Core Point: if there are not enough neighbors, it's noise
	if (seeds->size < min_cluster_size) {
		cctx->ids[index] = DBSCAN_POINT_NOISE;
		vector_destroy(seeds);
		return OK;
	}

	// mark the starting point and its first neighbors with the current cluster ID
	cctx->ids[index] = cctx->unique_count;
	for (size_t i = 0; i < seeds->size; i++) {
		size_t neighbor_index = *(size_t*)vector_get(seeds, i);
		cctx->ids[neighbor_index] = cctx->unique_count;
	}

	// start bypassing the queue. seeds->size can grow dynamically during push_back
	for (size_t seed_index = 0; seed_index < seeds->size; seed_index++) {
		size_t current_index = *(size_t *)vector_get(seeds, seed_index);
		
		vector_t *neighbors = vector_create(keypoints->size, sizeof(size_t));
		if (!neighbors) {
			vector_destroy(seeds);
			ddloge(TAG, "couldn't vector_create neighbors");
			return ENOMEM;
		}

		if (get_neighbors(keypoints, current_index, max_distance, neighbors) != OK) {
			vector_destroy(neighbors);
			vector_destroy(seeds);
			ddloge(TAG, "couldn't get_neighbors");
			return -1;
		}

		// If the current point is also dense (Core Point), we expand the cluster
		if (neighbors->size >= min_cluster_size) {
			for (size_t j = 0; j < neighbors->size; j++) {
				size_t n_index = *(size_t *)vector_get(neighbors, j);

				// If the point has not been considered at all
				if (cctx->ids[n_index] == DBSCAN_CLUSTER_POINT_UNCLASSIFIED) {
					// mark it as "in the queue" so that other neighbors do not add it again
					cctx->ids[n_index] = cctx->unique_count; 
					vector_push_back(seeds, &n_index);
				} 
				// If it used to be noise, now it has become a peripheral point of the cluster
				else if (cctx->ids[n_index] == DBSCAN_POINT_NOISE) {
					cctx->ids[n_index] = cctx->unique_count;
				}
			}
		}
		vector_destroy(neighbors);
	}

	vector_destroy(seeds);
	return OK;
}


typedef struct {
	uint32_t sum_x;
	uint32_t sum_y;
	uint32_t count;
	uint16_t min_x;
	uint16_t max_x;
	uint16_t min_y;
	uint16_t max_y;
} cluster_stat_t;

static errno_t calculate_and_filter_cluster_centers(
	clusters_context_t *cctx,
	const vector_t *keypoints,
	const vision_conf_t *vconf,
	const uint8_t dbg_lvl
) {
	if (!keypoints || !cctx || cctx->unique_count == 0 || !cctx->ids || !cctx->centers) {
		return EINVAL;
	}

	cluster_stat_t *stats = (cluster_stat_t *)calloc(cctx->unique_count, sizeof(cluster_stat_t));
	if (!stats) {
		return ENOMEM;
	}

	// minimums init (maximums are 0 already due to calloc)
	for (uint16_t g = 0; g < cctx->unique_count; g++) {
		stats[g].min_x = 0xFFFF;
		stats[g].min_y = 0xFFFF;
	}

	// first pass: each cluster metrics collection
	for (size_t i = 0; i < keypoints->size; i++) {
		uint16_t id = cctx->ids[i];

		if (id == DBSCAN_POINT_NOISE || id == DBSCAN_CLUSTER_POINT_UNCLASSIFIED || id >= cctx->unique_count)
			continue;

		pixel_coord_t *p = (pixel_coord_t *)vector_get(keypoints, i);
		if (!p)
			continue;

		stats[id].sum_x += p->x;
		stats[id].sum_y += p->y;
		stats[id].count++;

		if (p->x < stats[id].min_x) stats[id].min_x = p->x;
		if (p->x > stats[id].max_x) stats[id].max_x = p->x;
		if (p->y < stats[id].min_y) stats[id].min_y = p->y;
		if (p->y > stats[id].max_y) stats[id].max_y = p->y;
	}

	// second pass: geometry, filtering and calculating final valid centers
	for (uint16_t g = 0; g < cctx->unique_count; g++) {
		if (stats[g].count == 0) {
			continue; // skip empty clusters
		}

		uint16_t cluster_w = stats[g].max_x - stats[g].min_x + 1;
		uint16_t cluster_h = stats[g].max_y - stats[g].min_y + 1;

		// --- filtering clouds criterias ---
		
		// size > 40%
		bool is_too_large = (cluster_w > (vconf->frame_size.x  * 4 / 10)) || 
		                    (cluster_h > (vconf->frame_size.y * 4 / 10));

		// Sides relation > 3.5.
		bool is_too_eccentric = (cluster_w * 2 > cluster_h * 7) || 
		                        (cluster_h * 2 > cluster_w * 7);

		if (vconf->dbscan_enable_geometry_filtering && (is_too_large || is_too_eccentric)) {
			if (dbg_lvl >= 2)
				ddlogw(TAG, "cluster %d filtered out (W:%d, H:%d)%s%s.", g, cluster_w, cluster_h,
					is_too_large ? " too_large" : "", is_too_eccentric ? " too_eccentric" : "");
		} else {
			pixel_coord_t valid_center;
			valid_center.x = (uint16_t)(stats[g].sum_x / stats[g].count);
			valid_center.y = (uint16_t)(stats[g].sum_y / stats[g].count);
			
			vector_push_back(cctx->centers, &valid_center);
			
			if (dbg_lvl >= 2)
				ddlogi(TAG, "cluster %d center calculated: x=%d, y=%d", g, valid_center.x, valid_center.y);
		}
	}

	free(stats);
	return OK;
}

// Internal recursive function to allow merging centers without infinite recursion
static clusters_context_t dbscan_core(
	const vector_t *keypoints,
	vision_conf_t *vconf,
	const uint8_t dbg_lvl,
	bool allow_merge
) {
	if (!keypoints || !vconf) {
		ddloge(TAG, "invalid arg");
		return (clusters_context_t){0};
	}

	clusters_context_t cctx = {
		.ids = calloc(keypoints->size, sizeof(uint16_t)),
		.unique_count = 0,
		.centers = NULL
	};
	if (!cctx.ids) {
		return (clusters_context_t){0};
	}

	uint16_t max_distance = sqrt(
		vconf->frame_size.x * vconf->frame_size.x + vconf->frame_size.y * vconf->frame_size.y
	) / 100 * vconf->dbscan_max_distance_img_diagonal_percent;

	for (size_t i = 0; i < keypoints->size; i++)
		cctx.ids[i] = DBSCAN_CLUSTER_POINT_UNCLASSIFIED;

	for (size_t i = 0; i < keypoints->size; i++)
		if (cctx.ids[i] == DBSCAN_CLUSTER_POINT_UNCLASSIFIED)
			if (expand_cluster(i, max_distance, vconf->dbscan_min_cluster_size, keypoints, &cctx) == OK)
				cctx.unique_count++;

	if (cctx.unique_count == 0)
		return cctx;

	cctx.centers = vector_create(cctx.unique_count, sizeof(pixel_coord_t));
	if (!cctx.centers) {
		ddloge(TAG, "failed to calloc centers vector");
		return cctx;
	}

	calculate_and_filter_cluster_centers(&cctx, keypoints, vconf, dbg_lvl);

	// --- SECONDARY DBSCAN: MERGE CLOUDS OF CENTERS ---
	if (allow_merge && cctx.centers->size > DBSCAN_MAX_CENTERS_COUNT_BEFORE_RECURSION) {
		if (dbg_lvl) {
			ddlogw(TAG, "too many centers (%zu). Running secondary DBSCAN to merge them...", cctx.centers->size);
		}

		// Backup vconf fields to temporarily override them for centers merging
		uint16_t orig_min_cluster = vconf->dbscan_min_cluster_size;
		bool orig_geom_filter = vconf->dbscan_enable_geometry_filtering;
		uint16_t orig_dist_percent = vconf->dbscan_max_distance_img_diagonal_percent;

		vconf->dbscan_min_cluster_size = 1;
		vconf->dbscan_enable_geometry_filtering = false;
		vconf->dbscan_max_distance_img_diagonal_percent = orig_dist_percent * 2;

		// Call core recursively on the centers vector. allow_merge = false prevents infinite recursion
		clusters_context_t merged_cctx = dbscan_core(cctx.centers, vconf, dbg_lvl, false);

		// Restore original config
		vconf->dbscan_min_cluster_size = orig_min_cluster;
		vconf->dbscan_enable_geometry_filtering = orig_geom_filter;
		vconf->dbscan_max_distance_img_diagonal_percent = orig_dist_percent;

		vector_destroy(cctx.centers);
		cctx.centers = merged_cctx.centers;

		free(merged_cctx.ids);
	}

	return cctx;
}

clusters_context_t dbscan(
	const vector_t *keypoints,
	vision_conf_t *vconf,
	const uint8_t dbg_lvl
) {
	return dbscan_core(keypoints, vconf, dbg_lvl, true);
}
