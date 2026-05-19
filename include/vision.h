#ifndef IMG_DEFS_H
#define IMG_DEFS_H

#include "defs.h"
#include "my_vector.h"
#include "img_defs.h"

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

/**
 * @brief Performs DBSCAN clustering and returns cluster centers
 * @param pixels_cloud Input data (point coordinates)
 * @param epsilon Maximum distance between points in the same cluster
 * @param min_points Minimum number of points to form a cluster
 * @param cluster_centers_out Clustering result (includes cluster centers)
 * @return OK on success, otherwise ESP_FAIL
 */
errno_t dbscan_old(
    pixels_cloud_t *pixels_cloud,
    const uint16_t epsilon,
    const uint16_t min_points,
    vector_t *cluster_centers_out
);


#define DBSCAN_MAX_DISTANCE 25	// min 2D distance between points to attribute the point to the cluster
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
	const config_t *conf
);

#endif
