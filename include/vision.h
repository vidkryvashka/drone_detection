#ifndef VISION_H
#define VISION_H

#include "defs.h"
#include "my_vector.h"

typedef struct {
	uint16_t max_distance_img_diagonal_percent;
	uint8_t min_cluster_size;
	uint16_t min_clusters_count_merge;
	bool enable_geometric_filtering;
} dbscan_conf_t;

typedef struct {
	uint16_t max_distance;					// maximum 2D distance in pixels that an object can move in 1 frame
	uint16_t max_missed;					// how many frames to wait before deleting a lost track
	uint16_t deviation_squared_threshold;
} track_conf_t;

typedef struct {
	pixel_coord_t frame_size;
	uint8_t fast9_threshold;
	dbscan_conf_t dbscan_conf;
	track_conf_t track_conf;
} vision_conf_t;

#define FAST9_DEFAULT_THRESHOLD 40

#define DBSCAN_MAX_DISTANCE_IMG_DIAGONAL_PERCENT_DEFAULT	4			// max 2D distance between points to attribute the point to the cluster
#define DBSCAN_MIN_CLUSTER_SIZE_DEFAULT						3			// min points number in cluster
#define DBSCAN_MIN_CLUSTERS_COUNT_MERGE_DEFAULT				24			// Trigger threshold for secondary DBSCAN
#define DBSCAN_ENABLE_GEOM_FILTERING_DEFAULT				false
#define DBSCAN_CLUSTER_POINT_UNCLASSIFIED					UINT16_MAX
#define DBSCAN_POINT_NOISE									UINT16_MAX - 1
#define DBSCAN_CLUSTER_MAX_UNIQUE_COUNT						UINT16_MAX - 2

#define TRACK_MAX_DISTANCE_DEFAULT							30
#define TRACK_MAX_MISSED_DEFAULT							5
#define TRACK_DEVIATION_THRESHOLD_SQUARED_DEFAULT			8

/**
 * @brief Keypoints search algorithm, took from habr and modified types
 */
vector_t* fast9(
	const image_t *gray_img,
	const uint8_t threshold,
	const uint8_t dbg_lvl
);


typedef struct {
	vector_t *centers;
	uint16_t *ids;
	uint16_t unique_count;
} clusters_context_t;


clusters_context_t dbscan(
	const vector_t *keypoints,
	vision_conf_t *vconf,
	const uint8_t dbg_lvl
);


typedef struct {
	uint32_t id;
	pixel_coord_t current;
	pixel_coord_t previous;
	int16_t dx;
	int16_t dy;
	int16_t prev_dx;
    int16_t prev_dy;
	uint16_t age;			// How many frames in a row is it visible (confidence)
	uint16_t missed_frames;	// How many frames isn't it visible (not to delete immediately)
	uint16_t deviation_squared;
	bool is_most_deviated;
} track_t;

// The tracker context that lives between process_one_image calls
typedef struct {
	vector_t *active_tracks;
	uint16_t next_track_id;		// counter for issuing new IDs
} tracker_context_t;


pixel_coord_t update_tracker(
	tracker_context_t *tracker,
	const vector_t *new_centers,
		const vision_conf_t *vconf,
	const uint8_t dbg_lvl
);

#endif
