#ifndef KALMAN_H
#define KALMAN_H

#include <stdint.h>
#include <stdbool.h>
#include "defs.h"

// Default process and measurement noise parameters
#define KF_Q_POS_DEFAULT 0.2f       // Process noise variance for position
#define KF_Q_VEL_DEFAULT 1.5f       // Process noise variance for velocity
#define KF_R_DEFAULT     4.0f       // Measurement noise variance (cluster centroid jitter)

// 1D Kalman filter state (position and velocity)
typedef struct {
	float x;        // Estimated position
	float v;        // Estimated velocity
	float p00;      // Error covariance: var(pos)
	float p01;      // Error covariance: cov(pos, vel)
	float p11;      // Error covariance: var(vel)
	float q_pos;    // Process noise (pos)
	float q_vel;    // Process noise (vel)
	float r;        // Measurement noise variance
} kalman_1d_t;

// 2D Kalman filter for (x, y) coordinates
typedef struct {
	kalman_1d_t kf_x;
	kalman_1d_t kf_y;
} kalman_2d_t;

void kalman_2d_init(
	kalman_2d_t *kf,
	float init_x,
	float init_y,
	float q_pos,
	float q_vel,
	float r
);

void kalman_2d_predict(
	kalman_2d_t *kf,
	float dt
);

void kalman_2d_update(
	kalman_2d_t *kf,
	float meas_x,
	float meas_y
);

pixel_coord_t kalman_2d_get_predicted_pos(
	const kalman_2d_t *kf,
	float dt
);

pixel_coord_t kalman_2d_get_current_pos(
	const kalman_2d_t *kf
);

void kalman_2d_get_velocity(
	const kalman_2d_t *kf,
	float *vx,
	float *vy
);

#endif // KALMAN_H
