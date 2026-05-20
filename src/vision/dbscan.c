// #include <cstddef.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "img_defs.h"
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
	bool is_test
) {
	if (!keypoints || !cctx || cctx->unique_count == 0 || !cctx->ids || !cctx->centers) {
		return EINVAL;
	}

	cluster_stat_t *stats = (cluster_stat_t *)calloc(cctx->unique_count, sizeof(cluster_stat_t));
	if (!stats) {
		return ENOMEM;
	}

	// minimums init (maximums are 0 already due to calloc)
	for (uint8_t g = 0; g < cctx->unique_count; g++) {
		stats[g].min_x = 0xFFFF;
		stats[g].min_y = 0xFFFF;
	}

	// first pass: each cluster metrics collection
	for (size_t i = 0; i < keypoints->size; i++) {
		uint8_t id = cctx->ids[i];

		if (id == DBSCAN_NOISE || id == DBSCAN_CLUSTER_UNCLASSIFIED || id >= cctx->unique_count)
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
	for (uint8_t g = 0; g < cctx->unique_count; g++) {
		if (stats[g].count == 0) {
			continue; // skip empty clusters
		}

		uint16_t cluster_w = stats[g].max_x - stats[g].min_x + 1;
		uint16_t cluster_h = stats[g].max_y - stats[g].min_y + 1;

		// --- filtering clouds criteria ---
		
		// size > 40%
		bool is_too_large = (cluster_w > (vconf->frame_width  * 4 / 10)) || 
		                    (cluster_h > (vconf->frame_height * 4 / 10));

		// Sides relation > 3.5.
		bool is_too_eccentric = (cluster_w * 2 > cluster_h * 7) || 
		                        (cluster_h * 2 > cluster_w * 7);

		if (vconf->dbscan_enable_geometry_filtering && (is_too_large || is_too_eccentric)) {
			if (is_test)
				ddlogw(TAG, "cluster %d filtered out (W:%d, H:%d)%s%s.", g, cluster_w, cluster_h,
					is_too_large ? " too_large" : "", is_too_eccentric ? " too_eccentric" : "");
		} else {
			pixel_coord_t valid_center;
			valid_center.x = (uint16_t)(stats[g].sum_x / stats[g].count);
			valid_center.y = (uint16_t)(stats[g].sum_y / stats[g].count);
			
			vector_push_back(cctx->centers, &valid_center);
			
			if (is_test)
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
	bool is_test,
	bool allow_merge
) {
	if (!keypoints || !vconf) {
		ddloge(TAG, "invalid arg");
		return (clusters_context_t){0};
	}

	clusters_context_t cctx = {
		.ids = calloc(keypoints->size, sizeof(uint8_t)),
		.size = keypoints->size,
		.unique_count = 0,
		.centers = NULL
	};
	if (!cctx.ids) {
		return (clusters_context_t){0};
	}

	uint16_t max_distance = sqrt(
		vconf->frame_width * vconf->frame_width + vconf->frame_height * vconf->frame_height
	) / 100 * vconf->dbscan_max_distance_img_diagonal_percent;

	for (size_t i = 0; i < keypoints->size; i++)
		cctx.ids[i] = DBSCAN_CLUSTER_UNCLASSIFIED;

	for (size_t i = 0; i < keypoints->size; i++)
		if (cctx.ids[i] == DBSCAN_CLUSTER_UNCLASSIFIED)
			if (expand_cluster(i, max_distance, vconf->dbscan_min_cluster_size, keypoints, &cctx) == OK)
				cctx.unique_count++;

	if (cctx.unique_count == 0)
		return cctx;

	cctx.centers = vector_create(cctx.unique_count, sizeof(pixel_coord_t));
	if (!cctx.centers) {
		ddloge(TAG, "failed to calloc centers vector");
		return cctx;
	}

	calculate_and_filter_cluster_centers(&cctx, keypoints, vconf, is_test);

	// --- SECONDARY DBSCAN: MERGE CLOUDS OF CENTERS ---
	if (allow_merge && cctx.centers->size > DBSCAN_MAX_CENTERS_BEFORE_RECURSION) {
		if (is_test) {
			ddlogw(TAG, "too many centers (%zu). Running secondary DBSCAN to merge them...", cctx.centers->size);
		}

		// Backup vconf fields to temporarily override them for centers merging
		uint16_t orig_min_cluster = vconf->dbscan_min_cluster_size;
		bool orig_geom_filter = vconf->dbscan_enable_geometry_filtering;
		uint8_t orig_dist_percent = vconf->dbscan_max_distance_img_diagonal_percent;

		vconf->dbscan_min_cluster_size = 1;
		vconf->dbscan_enable_geometry_filtering = false;
		vconf->dbscan_max_distance_img_diagonal_percent = orig_dist_percent * 2;

		// Call core recursively on the centers vector. allow_merge = false prevents infinite recursion
		clusters_context_t merged_cctx = dbscan_core(cctx.centers, vconf, is_test, false);

		// Restore original config
		vconf->dbscan_min_cluster_size = orig_min_cluster;
		vconf->dbscan_enable_geometry_filtering = orig_geom_filter;
		vconf->dbscan_max_distance_img_diagonal_percent = orig_dist_percent;

		// Overwrite old centers vector with the newly merged vector
		vector_destroy(cctx.centers);
		cctx.centers = merged_cctx.centers;

		// Clean up the internal ids array from the secondary run (we don't need it)
		free(merged_cctx.ids);
		
		// Note: cctx.ids still maps points to the ORIGINAL raw clusters. 
		// The centers vector now contains the distilled final targets.
	}

	return cctx;
}

clusters_context_t dbscan(
	const vector_t *keypoints,
	vision_conf_t *vconf,
	bool is_test
) {
	return dbscan_core(keypoints, vconf, is_test, true);
}