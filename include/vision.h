#ifndef IMG_DEFS_H
#define IMG_DEFS_H

#include "defs.h"
#include "my_vector.h"
#include "img_defs.h"
#include <stdint.h>

typedef struct {
	uint16_t frame_width;
	uint16_t frame_height;
	uint16_t dbscan_max_distance_img_diagonal_percent;
	uint16_t dbscan_min_cluster_size;
	bool dbscan_enable_geometry_filtering;
	uint8_t fast9_threshold;
} vision_conf_t;

#define DEFAULT_THRESHOLD 40
// #define START_THRESHOLD 120
// #define EDGE_THRESHOLD 31
// #define KEYPOINTS_MAX_COUNT 32
// #define SCALE_FACTOR 1.41421356237 // sqrt(2)

/**
 * @brief Keypoints searching algorithm, took from habr and rewrote
 */
vector_t* fast9(
	const image_t *gray_img,
	const uint8_t threshold
);


// #define DBSCAN_MAX_DISTANCE 25	// min 2D distance between points to attribute the point to the cluster
#define DBSCAN_DEFAULT_MAX_DISTANCE_IMG_DIAGONAL_PERCENT 4	// min 2D distance between points to attribute the point to the cluster
#define DBSCAN_MIN_CLUSTER_SIZE	3	// min points number in cluster
#define DBSCAN_CLUSTER_UNCLASSIFIED		UINT16_MAX // 255
#define DBSCAN_NOISE	UINT16_MAX - 1 // 254
#define DBSCAN_CLUSTER_MAX_UNIQUE_COUNT		UINT16_MAX - 2 // 253
#define DBSCAN_MAX_CENTERS_COUNT_BEFORE_RECURSION 16 // Trigger threshold for secondary DBSCAN

typedef struct {
	size_t size;	// equal to keypoints count
	vector_t *centers;
	uint16_t *ids;
	uint16_t unique_count;
} clusters_context_t;


clusters_context_t dbscan(
	const vector_t *keypoints,
	vision_conf_t *vconf,
	bool is_test
);


typedef struct {
	uint32_t id;
	pixel_coord_t current;
	pixel_coord_t previous;
	int16_t dx;
	int16_t dy;
	int32_t prev_dx;
    int32_t prev_dy;
	uint16_t age;			// How many frames in a row is it visible (confidence)
	uint16_t missed_frames;	// How many frames isn't it visible (not to delete immediately)
	uint16_t abnormality;
} track_t;

// The tracker context that will "live" between process_one_image calls
typedef struct {
	vector_t *active_tracks;
	uint32_t next_track_id;		// Counter for issuing new IDs
	uint16_t max_distance;		// The maximum distance in pixels that an object can move in 1 frame
	uint16_t max_missed;		// How many frames to wait before deleting a lost track
} tracker_context_t;


errno_t update_tracker(
	tracker_context_t *tracker,
	const vector_t *new_centers
);

#endif
