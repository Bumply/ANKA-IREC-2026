/**
 * @file sensor_fusion.c
 * @brief Sensor Fusion Implementation
 * @author Zenith Rocket Team
 * @date 2025
 * 
 * Algorithm Overview:
 * 1. Check sensor health and mark sensors as valid/invalid
 * 2. Weighted average of redundant sensors based on health
 * 3. Convert body-frame acceleration to world-frame using orientation
 * 4. Kalman filter for altitude/velocity estimation
 * 5. Output fused state estimate
 */

#include "sensor_fusion.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define GRAVITY         9.80665f
#define DEFAULT_TIMEOUT 500.0f      /* ms before sensor marked unhealthy */
#define MAX_ALT_DIFF    50.0f       /* meters */
#define MAX_ACCEL_DIFF  10.0f       /* m/s² */

/*============================================================================
 * PRIVATE FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize Kalman filter
 */
static void kalman_init(KalmanFilter_t *kf)
{
    memset(kf, 0, sizeof(KalmanFilter_t));
    
    /* Initial state: zero altitude and velocity */
    kf->x[0] = 0.0f;  /* altitude */
    kf->x[1] = 0.0f;  /* velocity */
    
    /* Initial covariance - high uncertainty */
    kf->P[0][0] = 100.0f;
    kf->P[0][1] = 0.0f;
    kf->P[1][0] = 0.0f;
    kf->P[1][1] = 100.0f;
    
    /* Process noise */
    kf->Q[0][0] = KF_PROCESS_NOISE_ALT;
    kf->Q[0][1] = 0.0f;
    kf->Q[1][0] = 0.0f;
    kf->Q[1][1] = KF_PROCESS_NOISE_VEL;
    
    /* Measurement noise */
    kf->R_baro = KF_MEASURE_NOISE_BARO;
    kf->R_accel = KF_MEASURE_NOISE_ACCEL;
    
    kf->initialized = true;
}

/**
 * @brief Kalman filter predict step
 */
static void kalman_predict(KalmanFilter_t *kf, float accel, float dt)
{
    /* State prediction
     * altitude_new = altitude + velocity * dt + 0.5 * accel * dt²
     * velocity_new = velocity + accel * dt
     */
    float dt2 = dt * dt;
    
    kf->x[0] = kf->x[0] + kf->x[1] * dt + 0.5f * accel * dt2;
    kf->x[1] = kf->x[1] + accel * dt;
    
    /* Covariance prediction: P = F*P*F' + Q
     * State transition matrix F = [1, dt; 0, 1]
     */
    float P00 = kf->P[0][0] + dt * (kf->P[1][0] + kf->P[0][1]) + dt2 * kf->P[1][1];
    float P01 = kf->P[0][1] + dt * kf->P[1][1];
    float P10 = kf->P[1][0] + dt * kf->P[1][1];
    float P11 = kf->P[1][1];
    
    kf->P[0][0] = P00 + kf->Q[0][0];
    kf->P[0][1] = P01;
    kf->P[1][0] = P10;
    kf->P[1][1] = P11 + kf->Q[1][1];
}

/**
 * @brief Kalman filter update step with barometer measurement
 */
static void kalman_update_baro(KalmanFilter_t *kf, float altitude_meas)
{
    /* Measurement model: z = H * x, where H = [1, 0] */
    float y = altitude_meas - kf->x[0];  /* Innovation */
    
    /* Innovation covariance: S = H*P*H' + R */
    float S = kf->P[0][0] + kf->R_baro;
    
    /* Kalman gain: K = P*H'/S */
    float K0 = kf->P[0][0] / S;
    float K1 = kf->P[1][0] / S;
    
    /* State update */
    kf->x[0] = kf->x[0] + K0 * y;
    kf->x[1] = kf->x[1] + K1 * y;
    
    /* Covariance update: P = (I - K*H) * P */
    float P00 = (1.0f - K0) * kf->P[0][0];
    float P01 = (1.0f - K0) * kf->P[0][1];
    float P10 = kf->P[1][0] - K1 * kf->P[0][0];
    float P11 = kf->P[1][1] - K1 * kf->P[0][1];
    
    kf->P[0][0] = P00;
    kf->P[0][1] = P01;
    kf->P[1][0] = P10;
    kf->P[1][1] = P11;
}

/**
 * @brief Update sensor health
 */
static void update_sensor_health(SensorHealth_t *health, bool valid, uint32_t timestamp)
{
    if (valid) {
        health->available = true;
        health->last_valid_time = timestamp;
        if (health->error_count > 0) {
            health->error_count--;
        }
        health->weight = 1.0f - (health->error_count * 0.1f);
        if (health->weight < 0.0f) health->weight = 0.0f;
    } else {
        health->error_count++;
        if (health->error_count > 10) {
            health->available = false;
            health->weight = 0.0f;
        } else {
            health->weight = 1.0f - (health->error_count * 0.1f);
        }
    }
}

/**
 * @brief Weighted average of two values
 */
static float weighted_average(float v1, float w1, float v2, float w2)
{
    float total_weight = w1 + w2;
    if (total_weight < 0.001f) {
        return v1;  /* Fallback to first value */
    }
    return (v1 * w1 + v2 * w2) / total_weight;
}

/**
 * @brief Convert pressure to altitude using barometric formula
 */
static float pressure_to_altitude(float pressure, float sea_level_pressure)
{
    /* Barometric formula */
    const float T0 = 288.15f;    /* Standard temperature at sea level (K) */
    const float L = 0.0065f;     /* Temperature lapse rate (K/m) */
    const float R = 8.31447f;    /* Universal gas constant */
    const float M = 0.0289644f;  /* Molar mass of air (kg/mol) */
    
    float exponent = (R * L) / (GRAVITY * M);
    float altitude = (T0 / L) * (1.0f - powf(pressure / sea_level_pressure, exponent));
    
    return altitude;
}

/**
 * @brief Update rotation matrix from Euler angles
 */
static void update_rotation_matrix(SensorFusion_t *sf, float roll, float pitch, float yaw)
{
    /* Convert to radians */
    float r = roll * M_PI / 180.0f;
    float p = pitch * M_PI / 180.0f;
    float y = yaw * M_PI / 180.0f;
    
    float cr = cosf(r);
    float sr = sinf(r);
    float cp = cosf(p);
    float sp = sinf(p);
    float cy = cosf(y);
    float sy = sinf(y);
    
    /* ZYX Euler rotation matrix (yaw-pitch-roll) */
    sf->rotation_matrix[0][0] = cy * cp;
    sf->rotation_matrix[0][1] = cy * sp * sr - sy * cr;
    sf->rotation_matrix[0][2] = cy * sp * cr + sy * sr;
    
    sf->rotation_matrix[1][0] = sy * cp;
    sf->rotation_matrix[1][1] = sy * sp * sr + cy * cr;
    sf->rotation_matrix[1][2] = sy * sp * cr - cy * sr;
    
    sf->rotation_matrix[2][0] = -sp;
    sf->rotation_matrix[2][1] = cp * sr;
    sf->rotation_matrix[2][2] = cp * cr;
}

/**
 * @brief Transform body-frame acceleration to world-frame vertical
 */
static float body_to_world_vertical(SensorFusion_t *sf, float ax, float ay, float az)
{
    /* Vertical component (Z in world frame, positive up) */
    float az_world = sf->rotation_matrix[2][0] * ax +
                     sf->rotation_matrix[2][1] * ay +
                     sf->rotation_matrix[2][2] * az;
    
    /* Remove gravity */
    return az_world - GRAVITY;
}

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

HAL_StatusTypeDef SensorFusion_Init(SensorFusion_t *sf)
{
    if (sf == NULL) return HAL_ERROR;
    
    memset(sf, 0, sizeof(SensorFusion_t));
    
    /* Initialize Kalman filter */
    kalman_init(&sf->kf);
    
    /* Default configuration */
    sf->sensor_timeout_ms = DEFAULT_TIMEOUT;
    sf->max_altitude_diff = MAX_ALT_DIFF;
    sf->max_accel_diff = MAX_ACCEL_DIFF;
    
    /* Ground reference (sea level default) */
    sf->ground_pressure = 101325.0f;
    sf->ground_altitude = 0.0f;
    
    /* Initialize rotation matrix to identity */
    sf->rotation_matrix[0][0] = 1.0f;
    sf->rotation_matrix[1][1] = 1.0f;
    sf->rotation_matrix[2][2] = 1.0f;
    
    /* Mark all sensors as initially unavailable */
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sf->imu_health[i].available = false;
        sf->imu_health[i].weight = 0.0f;
        sf->baro_health[i].available = false;
        sf->baro_health[i].weight = 0.0f;
    }
    
    sf->initialized = true;
    
    return HAL_OK;
}

void SensorFusion_SetGroundReference(SensorFusion_t *sf, float pressure, float altitude)
{
    if (sf == NULL) return;
    sf->ground_pressure = pressure;
    sf->ground_altitude = altitude;
    
    /* Reset Kalman filter */
    SensorFusion_ResetFilter(sf);
}

void SensorFusion_UpdateIMU(SensorFusion_t *sf, uint8_t index, const IMU_Reading_t *reading)
{
    if (sf == NULL || index >= SENSOR_COUNT || reading == NULL) return;
    
    memcpy(&sf->imu[index], reading, sizeof(IMU_Reading_t));
    update_sensor_health(&sf->imu_health[index], reading->valid, reading->timestamp);
}

void SensorFusion_UpdateBaro(SensorFusion_t *sf, uint8_t index, const Baro_Reading_t *reading)
{
    if (sf == NULL || index >= SENSOR_COUNT || reading == NULL) return;
    
    memcpy(&sf->baro[index], reading, sizeof(Baro_Reading_t));
    update_sensor_health(&sf->baro_health[index], reading->valid, reading->timestamp);
}

void SensorFusion_UpdateOrientation(SensorFusion_t *sf, float roll, float pitch, float yaw,
                                    float qw, float qx, float qy, float qz)
{
    if (sf == NULL) return;
    
    sf->fused.roll = roll;
    sf->fused.pitch = pitch;
    sf->fused.yaw = yaw;
    sf->fused.quat_w = qw;
    sf->fused.quat_x = qx;
    sf->fused.quat_y = qy;
    sf->fused.quat_z = qz;
    
    update_rotation_matrix(sf, roll, pitch, yaw);
}

void SensorFusion_Process(SensorFusion_t *sf, float dt)
{
    if (sf == NULL || !sf->initialized || dt <= 0.0f) return;
    
    uint32_t now = HAL_GetTick();
    
    /* Check for sensor timeouts */
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sf->imu_health[i].available && 
            (now - sf->imu_health[i].last_valid_time) > sf->sensor_timeout_ms) {
            sf->imu_health[i].available = false;
            sf->imu_health[i].weight = 0.0f;
        }
        if (sf->baro_health[i].available && 
            (now - sf->baro_health[i].last_valid_time) > sf->sensor_timeout_ms) {
            sf->baro_health[i].available = false;
            sf->baro_health[i].weight = 0.0f;
        }
    }
    
    /*========================================================================
     * FUSE BAROMETER DATA
     *========================================================================*/
    
    float fused_altitude_msl = 0.0f;
    float baro_weight_sum = 0.0f;
    uint8_t baro_count = 0;
    
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sf->baro_health[i].available && sf->baro[i].valid) {
            float alt = pressure_to_altitude(sf->baro[i].pressure, sf->ground_pressure);
            fused_altitude_msl += alt * sf->baro_health[i].weight;
            baro_weight_sum += sf->baro_health[i].weight;
            baro_count++;
        }
    }
    
    if (baro_weight_sum > 0.001f) {
        fused_altitude_msl /= baro_weight_sum;
    }
    
    float fused_altitude_agl = fused_altitude_msl - sf->ground_altitude;
    
    /* Determine baro source */
    if (baro_count == 2) {
        sf->fused.baro_source = 2;  /* Fused */
    } else if (sf->baro_health[0].available) {
        sf->fused.baro_source = 0;  /* Primary */
    } else if (sf->baro_health[1].available) {
        sf->fused.baro_source = 1;  /* Backup */
    }
    
    /*========================================================================
     * FUSE IMU DATA
     *========================================================================*/
    
    float fused_ax = 0.0f, fused_ay = 0.0f, fused_az = 0.0f;
    float imu_weight_sum = 0.0f;
    uint8_t imu_count = 0;
    
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sf->imu_health[i].available && sf->imu[i].valid) {
            fused_ax += sf->imu[i].accel_x * sf->imu_health[i].weight;
            fused_ay += sf->imu[i].accel_y * sf->imu_health[i].weight;
            fused_az += sf->imu[i].accel_z * sf->imu_health[i].weight;
            imu_weight_sum += sf->imu_health[i].weight;
            imu_count++;
        }
    }
    
    if (imu_weight_sum > 0.001f) {
        fused_ax /= imu_weight_sum;
        fused_ay /= imu_weight_sum;
        fused_az /= imu_weight_sum;
    }
    
    /* Determine IMU source */
    if (imu_count == 2) {
        sf->fused.imu_source = 2;  /* Fused */
    } else if (sf->imu_health[0].available) {
        sf->fused.imu_source = 0;  /* Primary */
    } else if (sf->imu_health[1].available) {
        sf->fused.imu_source = 1;  /* Backup */
    }
    
    /* Convert to world frame and remove gravity */
    float vertical_accel = body_to_world_vertical(sf, fused_ax, fused_ay, fused_az);
    
    /* Total acceleration magnitude */
    float total_accel = sqrtf(fused_ax * fused_ax + fused_ay * fused_ay + fused_az * fused_az);
    
    /*========================================================================
     * KALMAN FILTER
     *========================================================================*/
    
    /* Predict step with acceleration */
    kalman_predict(&sf->kf, vertical_accel, dt);
    
    /* Update step with barometer (if available) */
    if (baro_count > 0) {
        kalman_update_baro(&sf->kf, fused_altitude_agl);
    }
    
    /*========================================================================
     * OUTPUT
     *========================================================================*/
    
    sf->fused.altitude = sf->kf.x[0];
    sf->fused.altitude_msl = sf->fused.altitude + sf->ground_altitude;
    sf->fused.velocity = sf->kf.x[1];
    sf->fused.accel_vertical = vertical_accel;
    sf->fused.accel_total = total_accel;
    
    /* 3D velocity (approximation - would need GPS for horizontal) */
    sf->fused.velocity_3d = fabsf(sf->fused.velocity);
    
    /* Confidence based on covariance */
    sf->fused.altitude_confidence = 1.0f / (1.0f + sf->kf.P[0][0]);
    sf->fused.velocity_confidence = 1.0f / (1.0f + sf->kf.P[1][1]);
    
    sf->fused.timestamp = now;
}

const Fused_Data_t* SensorFusion_GetData(SensorFusion_t *sf)
{
    if (sf == NULL) return NULL;
    return &sf->fused;
}

float SensorFusion_GetAltitude(SensorFusion_t *sf)
{
    if (sf == NULL) return 0.0f;
    return sf->fused.altitude;
}

float SensorFusion_GetVelocity(SensorFusion_t *sf)
{
    if (sf == NULL) return 0.0f;
    return sf->fused.velocity;
}

float SensorFusion_GetAcceleration(SensorFusion_t *sf)
{
    if (sf == NULL) return 0.0f;
    return sf->fused.accel_vertical;
}

bool SensorFusion_IsPrimaryIMUHealthy(SensorFusion_t *sf)
{
    if (sf == NULL) return false;
    return sf->imu_health[SENSOR_PRIMARY].available;
}

bool SensorFusion_IsPrimaryBaroHealthy(SensorFusion_t *sf)
{
    if (sf == NULL) return false;
    return sf->baro_health[SENSOR_PRIMARY].available;
}

uint8_t SensorFusion_GetHealthyIMUCount(SensorFusion_t *sf)
{
    if (sf == NULL) return 0;
    uint8_t count = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sf->imu_health[i].available) count++;
    }
    return count;
}

uint8_t SensorFusion_GetHealthyBaroCount(SensorFusion_t *sf)
{
    if (sf == NULL) return 0;
    uint8_t count = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sf->baro_health[i].available) count++;
    }
    return count;
}

void SensorFusion_ResetFilter(SensorFusion_t *sf)
{
    if (sf == NULL) return;
    kalman_init(&sf->kf);
}
