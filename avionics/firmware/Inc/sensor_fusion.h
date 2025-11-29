/**
 * @file sensor_fusion.h
 * @brief Sensor Fusion for Redundant Sensors
 * @author Zenith Rocket Team
 * @date 2025
 * 
 * Combines data from:
 * - 2x IMUs (MPU9250 + BNO055) 
 * - 2x Barometers (BMP380 + MS5611)
 * 
 * Features:
 * - Weighted averaging based on sensor health
 * - Kalman filter for altitude/velocity estimation
 * - Automatic failover on sensor failure
 * - Vertical acceleration extraction
 */

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

/* Sensor indices */
#define SENSOR_PRIMARY      0
#define SENSOR_BACKUP       1
#define SENSOR_COUNT        2

/* Kalman filter tuning */
#define KF_PROCESS_NOISE_ALT    0.1f    /* Process noise for altitude */
#define KF_PROCESS_NOISE_VEL    1.0f    /* Process noise for velocity */
#define KF_MEASURE_NOISE_BARO   2.0f    /* Measurement noise for barometer */
#define KF_MEASURE_NOISE_ACCEL  0.5f    /* Measurement noise for accelerometer */

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/* Raw sensor readings */
typedef struct {
    /* Accelerometer (m/s²) */
    float accel_x;
    float accel_y;
    float accel_z;
    
    /* Gyroscope (deg/s) */
    float gyro_x;
    float gyro_y;
    float gyro_z;
    
    /* Magnetometer (µT) */
    float mag_x;
    float mag_y;
    float mag_z;
    
    /* Temperature (°C) */
    float temperature;
    
    /* Status */
    bool valid;
    uint32_t timestamp;
    
} IMU_Reading_t;

typedef struct {
    float pressure;         /* Pa */
    float temperature;      /* °C */
    float altitude;         /* meters */
    
    bool valid;
    uint32_t timestamp;
    
} Baro_Reading_t;

/* Fused output */
typedef struct {
    /* Position */
    float altitude;         /* meters AGL */
    float altitude_msl;     /* meters MSL */
    
    /* Velocity */
    float velocity;         /* m/s vertical (positive up) */
    float velocity_3d;      /* m/s total magnitude */
    
    /* Acceleration */
    float accel_vertical;   /* m/s² vertical (positive up) */
    float accel_total;      /* m/s² total magnitude */
    
    /* Orientation (from BNO055 fusion) */
    float roll;             /* degrees */
    float pitch;            /* degrees */
    float yaw;              /* degrees */
    
    /* Quaternion */
    float quat_w;
    float quat_x;
    float quat_y;
    float quat_z;
    
    /* Confidence */
    float altitude_confidence;  /* 0-1 */
    float velocity_confidence;  /* 0-1 */
    
    /* Sensor health */
    uint8_t imu_source;         /* 0=primary, 1=backup, 2=fused */
    uint8_t baro_source;        /* 0=primary, 1=backup, 2=fused */
    
    uint32_t timestamp;
    
} Fused_Data_t;

/* Kalman filter state */
typedef struct {
    /* State vector [altitude, velocity] */
    float x[2];
    
    /* Error covariance matrix (2x2) */
    float P[2][2];
    
    /* Process noise */
    float Q[2][2];
    
    /* Measurement noise */
    float R_baro;
    float R_accel;
    
    /* Last update time */
    uint32_t last_update_ms;
    
    bool initialized;
    
} KalmanFilter_t;

/* Sensor health tracking */
typedef struct {
    bool available;
    uint32_t error_count;
    uint32_t last_valid_time;
    float weight;               /* 0-1, used for weighted averaging */
    
} SensorHealth_t;

/* Main fusion handle */
typedef struct {
    /* Sensor readings */
    IMU_Reading_t imu[SENSOR_COUNT];
    Baro_Reading_t baro[SENSOR_COUNT];
    
    /* Sensor health */
    SensorHealth_t imu_health[SENSOR_COUNT];
    SensorHealth_t baro_health[SENSOR_COUNT];
    
    /* Kalman filter */
    KalmanFilter_t kf;
    
    /* Output */
    Fused_Data_t fused;
    
    /* Reference values */
    float ground_pressure;      /* Pa at ground level */
    float ground_altitude;      /* meters MSL at ground */
    
    /* Configuration */
    float sensor_timeout_ms;    /* Time before sensor marked unhealthy */
    float max_altitude_diff;    /* Max allowed difference between baros */
    float max_accel_diff;       /* Max allowed difference between IMUs */
    
    /* Orientation matrix for converting body to world frame */
    float rotation_matrix[3][3];
    
    bool initialized;
    
} SensorFusion_t;

/*============================================================================
 * FUNCTION PROTOTYPES
 *============================================================================*/

/**
 * @brief Initialize sensor fusion
 * @param sf Pointer to sensor fusion handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef SensorFusion_Init(SensorFusion_t *sf);

/**
 * @brief Set ground reference
 * @param sf Pointer to sensor fusion handle
 * @param pressure Ground pressure in Pa
 * @param altitude Ground altitude MSL in meters
 */
void SensorFusion_SetGroundReference(SensorFusion_t *sf, float pressure, float altitude);

/**
 * @brief Update IMU reading
 * @param sf Pointer to sensor fusion handle
 * @param index Sensor index (0=primary, 1=backup)
 * @param reading Pointer to IMU reading
 */
void SensorFusion_UpdateIMU(SensorFusion_t *sf, uint8_t index, const IMU_Reading_t *reading);

/**
 * @brief Update barometer reading
 * @param sf Pointer to sensor fusion handle
 * @param index Sensor index (0=primary, 1=backup)
 * @param reading Pointer to baro reading
 */
void SensorFusion_UpdateBaro(SensorFusion_t *sf, uint8_t index, const Baro_Reading_t *reading);

/**
 * @brief Update orientation from BNO055
 * @param sf Pointer to sensor fusion handle
 * @param roll Roll angle in degrees
 * @param pitch Pitch angle in degrees  
 * @param yaw Yaw angle in degrees
 * @param qw Quaternion W
 * @param qx Quaternion X
 * @param qy Quaternion Y
 * @param qz Quaternion Z
 */
void SensorFusion_UpdateOrientation(SensorFusion_t *sf, float roll, float pitch, float yaw,
                                    float qw, float qx, float qy, float qz);

/**
 * @brief Run sensor fusion algorithm
 * @param sf Pointer to sensor fusion handle
 * @param dt Time since last update in seconds
 */
void SensorFusion_Process(SensorFusion_t *sf, float dt);

/**
 * @brief Get fused data
 * @param sf Pointer to sensor fusion handle
 * @return Pointer to fused data structure
 */
const Fused_Data_t* SensorFusion_GetData(SensorFusion_t *sf);

/**
 * @brief Get altitude AGL
 * @param sf Pointer to sensor fusion handle
 * @return Altitude above ground level in meters
 */
float SensorFusion_GetAltitude(SensorFusion_t *sf);

/**
 * @brief Get vertical velocity
 * @param sf Pointer to sensor fusion handle
 * @return Vertical velocity in m/s (positive up)
 */
float SensorFusion_GetVelocity(SensorFusion_t *sf);

/**
 * @brief Get vertical acceleration
 * @param sf Pointer to sensor fusion handle
 * @return Vertical acceleration in m/s²
 */
float SensorFusion_GetAcceleration(SensorFusion_t *sf);

/**
 * @brief Check if primary IMU is healthy
 * @param sf Pointer to sensor fusion handle
 * @return true if healthy
 */
bool SensorFusion_IsPrimaryIMUHealthy(SensorFusion_t *sf);

/**
 * @brief Check if primary barometer is healthy
 * @param sf Pointer to sensor fusion handle
 * @return true if healthy
 */
bool SensorFusion_IsPrimaryBaroHealthy(SensorFusion_t *sf);

/**
 * @brief Get number of healthy IMUs
 * @param sf Pointer to sensor fusion handle
 * @return 0, 1, or 2
 */
uint8_t SensorFusion_GetHealthyIMUCount(SensorFusion_t *sf);

/**
 * @brief Get number of healthy barometers
 * @param sf Pointer to sensor fusion handle
 * @return 0, 1, or 2
 */
uint8_t SensorFusion_GetHealthyBaroCount(SensorFusion_t *sf);

/**
 * @brief Reset Kalman filter
 * @param sf Pointer to sensor fusion handle
 */
void SensorFusion_ResetFilter(SensorFusion_t *sf);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_FUSION_H */
