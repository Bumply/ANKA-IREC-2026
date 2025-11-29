/**
 * @file data_logger.h
 * @brief Flight Data Logger for W25Q Flash
 * @author Zenith Rocket Team
 * @date 2025
 * 
 * Logs flight data to W25Q flash memory with:
 * - Header at start of flash with flight info
 * - High-rate sensor data during flight (50-100 Hz)
 * - Lower-rate GPS data (1 Hz)
 * - Event markers
 * - Post-flight data retrieval
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "w25q.h"
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

/* Flash layout for W25Q40 (512KB) */
#define LOG_HEADER_ADDR         0x000000    /* Flight header */
#define LOG_HEADER_SIZE         0x001000    /* 4KB for header */
#define LOG_DATA_ADDR           0x001000    /* Sensor data start */
#define LOG_DATA_SIZE           0x07F000    /* ~508KB for data */

/* Record types */
#define LOG_RECORD_SENSOR       0x01        /* High-rate sensor data */
#define LOG_RECORD_GPS          0x02        /* GPS data */
#define LOG_RECORD_EVENT        0x03        /* Event marker */
#define LOG_RECORD_STATE        0x04        /* State change */
#define LOG_RECORD_END          0xFF        /* End marker */

/* Magic numbers */
#define LOG_HEADER_MAGIC        0x5A454E49  /* "ZENI" */
#define LOG_RECORD_MAGIC        0xAB

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

/* Flight header (stored at start of flash) */
typedef struct {
    uint32_t magic;             /* Magic number (0x5A454E49) */
    uint32_t version;           /* Logger version */
    uint32_t flight_number;     /* Flight number */
    
    /* Flight times */
    uint32_t start_time;        /* Unix timestamp or RTC value */
    uint32_t launch_time;       /* ms from start to launch */
    uint32_t apogee_time;       /* ms from start to apogee */
    uint32_t main_time;         /* ms from start to main */
    uint32_t landed_time;       /* ms from start to landing */
    
    /* Flight statistics */
    int32_t max_altitude;       /* cm AGL */
    int16_t max_velocity;       /* cm/s */
    int16_t max_accel;          /* cm/s² */
    
    /* Ground reference */
    int32_t ground_altitude;    /* cm MSL */
    uint32_t ground_pressure;   /* Pa */
    
    /* GPS coordinates */
    int32_t launch_lat;         /* degrees * 1e7 */
    int32_t launch_lon;         /* degrees * 1e7 */
    int32_t landing_lat;
    int32_t landing_lon;
    
    /* Record info */
    uint32_t data_start;        /* Address of first data record */
    uint32_t data_end;          /* Address after last record */
    uint32_t record_count;      /* Total number of records */
    
    /* Checksum */
    uint16_t crc;
    
} LogHeader_t;

/* Sensor data record (28 bytes) - logged at 50-100 Hz */
typedef struct {
    uint8_t magic;              /* 0xAB */
    uint8_t type;               /* LOG_RECORD_SENSOR */
    uint16_t timestamp;         /* ms since launch (wraps at 65s) */
    
    /* Altitude/Velocity */
    int32_t altitude;           /* cm AGL */
    int16_t velocity;           /* cm/s */
    int16_t accel;              /* cm/s² */
    
    /* Orientation */
    int16_t roll;               /* degrees * 10 */
    int16_t pitch;              /* degrees * 10 */
    int16_t yaw;                /* degrees * 10 */
    
    /* Raw sensor */
    int16_t accel_x;            /* mg */
    int16_t accel_y;
    int16_t accel_z;
    
    uint8_t state;              /* Flight state */
    uint8_t flags;              /* Status flags */
    
} LogSensorRecord_t;

/* GPS record (24 bytes) - logged at 1 Hz */
typedef struct {
    uint8_t magic;              /* 0xAB */
    uint8_t type;               /* LOG_RECORD_GPS */
    uint16_t timestamp;         /* ms since launch */
    
    int32_t latitude;           /* degrees * 1e7 */
    int32_t longitude;          /* degrees * 1e7 */
    int32_t altitude;           /* mm MSL */
    
    uint16_t speed;             /* cm/s */
    int16_t heading;            /* degrees * 10 */
    
    uint8_t satellites;
    uint8_t fix;
    uint16_t reserved;
    
} LogGPSRecord_t;

/* Event record (8 bytes) */
typedef struct {
    uint8_t magic;              /* 0xAB */
    uint8_t type;               /* LOG_RECORD_EVENT */
    uint16_t timestamp;         /* ms since launch */
    
    uint8_t event;              /* Event type */
    uint8_t data;               /* Event data */
    int16_t altitude;           /* Altitude at event */
    
} LogEventRecord_t;

#pragma pack(pop)

/* Logger state */
typedef enum {
    LOGGER_STATE_IDLE,          /* Not logging */
    LOGGER_STATE_READY,         /* Ready to start */
    LOGGER_STATE_LOGGING,       /* Actively logging */
    LOGGER_STATE_STOPPED,       /* Logging stopped */
    LOGGER_STATE_ERROR          /* Error state */
} LoggerState_t;

/* Logger handle */
typedef struct {
    W25Q_Handle_t *flash;       /* Flash driver handle */
    
    LogHeader_t header;         /* Current flight header */
    LoggerState_t state;
    
    /* Write position */
    uint32_t write_addr;        /* Current write address */
    uint32_t record_count;      /* Records written */
    
    /* Timing */
    uint32_t launch_tick;       /* Tick at launch */
    
    /* Buffer for write operations */
    uint8_t buffer[256];
    uint16_t buffer_idx;
    
    bool initialized;
    
} DataLogger_t;

/*============================================================================
 * FUNCTION PROTOTYPES
 *============================================================================*/

/**
 * @brief Initialize data logger
 * @param logger Pointer to logger handle
 * @param flash Pointer to initialized W25Q handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef DataLogger_Init(DataLogger_t *logger, W25Q_Handle_t *flash);

/**
 * @brief Erase flash and prepare for new flight
 * @param logger Pointer to logger handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef DataLogger_Erase(DataLogger_t *logger);

/**
 * @brief Set ground reference
 * @param logger Pointer to logger handle
 * @param altitude Ground altitude MSL in cm
 * @param pressure Ground pressure in Pa
 */
void DataLogger_SetGroundReference(DataLogger_t *logger, int32_t altitude, uint32_t pressure);

/**
 * @brief Start logging (call at launch)
 * @param logger Pointer to logger handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef DataLogger_Start(DataLogger_t *logger);

/**
 * @brief Stop logging (call at landing)
 * @param logger Pointer to logger handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef DataLogger_Stop(DataLogger_t *logger);

/**
 * @brief Log sensor data
 * @param logger Pointer to logger handle
 * @param altitude Altitude AGL in cm
 * @param velocity Velocity in cm/s
 * @param accel Acceleration in cm/s²
 * @param roll Roll in degrees * 10
 * @param pitch Pitch in degrees * 10
 * @param yaw Yaw in degrees * 10
 * @param ax Accel X in mg
 * @param ay Accel Y in mg
 * @param az Accel Z in mg
 * @param state Flight state
 * @param flags Status flags
 * @return HAL_OK on success
 */
HAL_StatusTypeDef DataLogger_LogSensor(DataLogger_t *logger,
                                       int32_t altitude, int16_t velocity, int16_t accel,
                                       int16_t roll, int16_t pitch, int16_t yaw,
                                       int16_t ax, int16_t ay, int16_t az,
                                       uint8_t state, uint8_t flags);

/**
 * @brief Log GPS data
 * @param logger Pointer to logger handle
 * @param lat Latitude in degrees * 1e7
 * @param lon Longitude in degrees * 1e7
 * @param alt Altitude MSL in mm
 * @param speed Ground speed in cm/s
 * @param heading Heading in degrees * 10
 * @param sats Number of satellites
 * @param fix Fix type
 * @return HAL_OK on success
 */
HAL_StatusTypeDef DataLogger_LogGPS(DataLogger_t *logger,
                                    int32_t lat, int32_t lon, int32_t alt,
                                    uint16_t speed, int16_t heading,
                                    uint8_t sats, uint8_t fix);

/**
 * @brief Log event
 * @param logger Pointer to logger handle
 * @param event Event type
 * @param data Event data
 * @param altitude Altitude at event in cm
 * @return HAL_OK on success
 */
HAL_StatusTypeDef DataLogger_LogEvent(DataLogger_t *logger,
                                      uint8_t event, uint8_t data, int16_t altitude);

/**
 * @brief Update flight statistics
 * @param logger Pointer to logger handle
 * @param max_alt Maximum altitude in cm
 * @param max_vel Maximum velocity in cm/s
 * @param max_acc Maximum acceleration in cm/s²
 */
void DataLogger_UpdateStats(DataLogger_t *logger, 
                           int32_t max_alt, int16_t max_vel, int16_t max_acc);

/**
 * @brief Set landing GPS coordinates
 * @param logger Pointer to logger handle
 * @param lat Landing latitude
 * @param lon Landing longitude
 */
void DataLogger_SetLandingCoords(DataLogger_t *logger, int32_t lat, int32_t lon);

/**
 * @brief Flush buffer to flash
 * @param logger Pointer to logger handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef DataLogger_Flush(DataLogger_t *logger);

/**
 * @brief Check if logging
 * @param logger Pointer to logger handle
 * @return true if actively logging
 */
bool DataLogger_IsLogging(DataLogger_t *logger);

/**
 * @brief Get number of records logged
 * @param logger Pointer to logger handle
 * @return Record count
 */
uint32_t DataLogger_GetRecordCount(DataLogger_t *logger);

/**
 * @brief Read flight header
 * @param logger Pointer to logger handle
 * @param header Pointer to store header
 * @return HAL_OK if valid header found
 */
HAL_StatusTypeDef DataLogger_ReadHeader(DataLogger_t *logger, LogHeader_t *header);

/**
 * @brief Read data record at offset
 * @param logger Pointer to logger handle
 * @param offset Record offset (0 = first record)
 * @param buffer Pointer to store record
 * @param max_len Maximum buffer length
 * @return Number of bytes read, 0 on error
 */
uint16_t DataLogger_ReadRecord(DataLogger_t *logger, uint32_t offset,
                               uint8_t *buffer, uint16_t max_len);

/**
 * @brief Get available space in bytes
 * @param logger Pointer to logger handle
 * @return Available space
 */
uint32_t DataLogger_GetFreeSpace(DataLogger_t *logger);

#ifdef __cplusplus
}
#endif

#endif /* DATA_LOGGER_H */
