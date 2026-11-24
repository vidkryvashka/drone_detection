#include <math.h>
#include <assert.h>
#include "kalman.h"

static void kalman_1d_init(
	kalman_1d_t *kf,
	float init_pos,
	float q_pos,
	float q_vel,
	float r
) {
	assert(kf);
	kf->x = init_pos;
	kf->v = 0.0f;
	kf->p00 = r;
	kf->p01 = 0.0f;
	kf->p11 = 50.0f; // High initial velocity uncertainty to quickly adapt
	kf->q_pos = q_pos;
	kf->q_vel = q_vel;
	kf->r = r;
}

static void kalman_1d_predict(
	kalman_1d_t *kf,
	float dt
) {
	assert(kf);
	// State extrapolation: x = x + v * dt
	kf->x += kf->v * dt;

	// Covariance extrapolation: P = F * P * F^T + Q
	float dt2 = dt * dt;
	kf->p00 += 2.0f * dt * kf->p01 + dt2 * kf->p11 + kf->q_pos;
	kf->p01 += dt * kf->p11;
	kf->p11 += kf->q_vel;
}

static void kalman_1d_update(
	kalman_1d_t *kf,
	float meas
) {
	assert(kf);
	// Innovation (measurement residual)
	float y = meas - kf->x;

	// Innovation covariance: S = H * P * H^T + R = p00 + r
	float s = kf->p00 + kf->r;
	if (s < 1e-6f)
		s = 1e-6f;

	// Kalman gain: K = P * H^T / S = [p00 / s; p01 / s]
	float k0 = kf->p00 / s;
	float k1 = kf->p01 / s;

	// Update state estimate: x = x + K * y
	kf->x += k0 * y;
	kf->v += k1 * y;

	// Update covariance: P = (I - K * H) * P
	float p00_temp = kf->p00;
	float p01_temp = kf->p01;

	kf->p00 = (1.0f - k0) * p00_temp;
	kf->p01 = (1.0f - k0) * p01_temp;
	kf->p11 = kf->p11 - k1 * p01_temp;

	// Safeguards against negative numerical precision drifts
	if (kf->p00 < 1e-4f) kf->p00 = 1e-4f;
	if (kf->p11 < 1e-4f) kf->p11 = 1e-4f;
}

void kalman_2d_init(
	kalman_2d_t *kf,
	float init_x,
	float init_y,
	float q_pos,
	float q_vel,
	float r
) {
	assert(kf);
	kalman_1d_init(&kf->kf_x, init_x, q_pos, q_vel, r);
	kalman_1d_init(&kf->kf_y, init_y, q_pos, q_vel, r);
}

void kalman_2d_predict(
	kalman_2d_t *kf,
	float dt
) {
	assert(kf);
	kalman_1d_predict(&kf->kf_x, dt);
	kalman_1d_predict(&kf->kf_y, dt);
}

void kalman_2d_update(
	kalman_2d_t *kf,
	float meas_x,
	float meas_y
) {
	assert(kf);
	kalman_1d_update(&kf->kf_x, meas_x);
	kalman_1d_update(&kf->kf_y, meas_y);
}

pixel_coord_t kalman_2d_get_predicted_pos_curved(
	const kalman_2d_t *kf,
	float ax,
	float ay,
	float dt
) {
	assert(kf);
	// Kinematic prediction with optional turning acceleration:
	// x(t + dt) = x + v * dt + 0.5 * a * dt^2 (with slight damping factor for further steps)
	float damp = 1.0f / (1.0f + 0.08f * dt);
	float px_f = kf->kf_x.x + kf->kf_x.v * dt + 0.5f * ax * dt * dt * damp;
	float py_f = kf->kf_y.x + kf->kf_y.v * dt + 0.5f * ay * dt * dt * damp;

	int32_t px = (int32_t)roundf(px_f);
	int32_t py = (int32_t)roundf(py_f);
	if (px < 0) px = 0;
	if (py < 0) py = 0;
	if (px > UINT16_MAX) px = UINT16_MAX;
	if (py > UINT16_MAX) py = UINT16_MAX;

	return (pixel_coord_t){(uint16_t)px, (uint16_t)py};
}

pixel_coord_t kalman_2d_get_predicted_pos(
	const kalman_2d_t *kf,
	float dt
) {
	return kalman_2d_get_predicted_pos_curved(kf, 0.0f, 0.0f, dt);
}

void kalman_2d_get_predicted_trajectory(
	const kalman_2d_t *kf,
	float ax,
	float ay,
	pixel_coord_t *out_coords,
	uint8_t steps
) {
	assert(kf && out_coords);
	for (uint8_t i = 0; i < steps; i++) {
		out_coords[i] = kalman_2d_get_predicted_pos_curved(kf, ax, ay, (float)(i + 1));
	}
}

pixel_coord_t kalman_2d_get_current_pos(
	const kalman_2d_t *kf
) {
	assert(kf);
	int32_t px = (int32_t)roundf(kf->kf_x.x);
	int32_t py = (int32_t)roundf(kf->kf_y.x);
	if (px < 0) px = 0;
	if (py < 0) py = 0;
	if (px > UINT16_MAX) px = UINT16_MAX;
	if (py > UINT16_MAX) py = UINT16_MAX;

	return (pixel_coord_t){(uint16_t)px, (uint16_t)py};
}

void kalman_2d_get_velocity(
	const kalman_2d_t *kf,
	float *vx,
	float *vy
) {
	assert(kf);
	if (vx) *vx = kf->kf_x.v;
	if (vy) *vy = kf->kf_y.v;
}
