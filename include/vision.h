#ifndef IMG_DEFS_H
#define IMG_DEFS_H

#include "defs.h"
#include "my_vector.h"
#include "img_defs.h"

typedef struct {
	uint16_t frame_width;
	uint16_t frame_height;
	uint16_t dbscan_max_distance_img_diagonal_percent;
	uint8_t fast9_threshold;
} vision_conf_t;

#define DEFAULT_THRESHOLD 60
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
#define DEFAULT_DBSCAN_MAX_DISTANCE_IMG_DIAGONAL_PERCENT 8	// min 2D distance between points to attribute the point to the cluster
#define DBSCAN_MIN_CLUSTER_SIZE 3	// min points number in cluster
#define DBSCAN_CLUSTER_UNCLASSIFIED 255
#define DBSCAN_NOISE 254
#define DBSCAN_CLUSTER_MAX_UNIQUE_COUNT 253

typedef struct {
	uint8_t *ids;
	size_t size;	// equal to keypoints count
	uint8_t unique_count;
	pixel_coord_t *centers;
} clusters_context_t;


clusters_context_t dbscan(
	const vector_t *keypoints,
	const uint16_t min_points,
	vision_conf_t *vconf,
	bool is_test
);

#endif
