/**
 * @file telemetry.h
 * @brief Telemetry Packet Protocol for LoRa Transmission
 * @author Zenith Rocket Team
 * @date 2025
 * 
 * Packet Types:
 * - STATUS: Basic status and flight state (1 Hz on ground, 10 Hz in flight)
 * - FULL: Complete sensor data (1 Hz)
 * - GPS: GPS coordinates (1 Hz when available)
 * - EVENT: Event notifications (e.g., launch, apogee, landing)
 * 
 * Packet Format:
 * [SYNC][TYPE][SEQ][LENGTH][PAYLOAD...][CRC16]
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * PACKET DEFINITIONS
 *============================================================================*/

/* Sync word */
#define TLM_SYNC_WORD           0xAA55

/* Packet types */
#define TLM_TYPE_STATUS         0x01    /* Basic status */
#define TLM_TYPE_FULL           0x02    /* Full sensor data */
#define TLM_TYPE_GPS            0x03    /* GPS data */
#define TLM_TYPE_EVENT          0x04    /* Event notification */
#define TLM_TYPE_ACK            0x10    /* Acknowledgment */
#define TLM_TYPE_CMD            0x20    /* Command from ground */

/* Event types */
#define TLM_EVENT_BOOT          0x01
#define TLM_EVENT_ARMED         0x02
#define TLM_EVENT_LAUNCH        0x03
#define TLM_EVENT_BURNOUT       0x04
#define TLM_EVENT_APOGEE        0x05
#define TLM_EVENT_DROGUE        0x06
#define TLM_EVENT_MAIN          0x07
#define TLM_EVENT_LANDED        0x08
#define TLM_EVENT_ERROR         0xFF

/* Command types */
#define TLM_CMD_ARM             0x01
#define TLM_CMD_DISARM          0x02
#define TLM_CMD_FIRE_DROGUE     0x10    /* Manual pyro (use with caution!) */
#define TLM_CMD_FIRE_MAIN       0x11
#define TLM_CMD_BUZZER          0x20
#define TLM_CMD_RESET           0xFF

/* Maximum sizes */
#define TLM_MAX_PAYLOAD         64
#define TLM_MAX_PACKET          (TLM_MAX_PAYLOAD + 8)  /* Header + CRC */

/* Header size: Sync(2) + Type(1) + Seq(1) + Len(1) = 5 bytes */
#define TLM_HEADER_SIZE         5
/* CRC size */
#define TLM_CRC_SIZE            2

/*============================================================================
 * PACKET STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

/* Packet header */
typedef struct {
    uint16_t sync;          /* Sync word (0xAA55) */
    uint8_t type;           /* Packet type */
    uint8_t sequence;       /* Sequence number (0-255, wraps) */
    uint8_t length;         /* Payload length */
} TLM_Header_t;

/* Status packet payload (26 bytes) */
typedef struct {
    uint32_t timestamp;     /* ms since boot */
    uint8_t state;          /* Flight state */
    uint8_t flags;          /* Status flags */
    int16_t altitude;       /* meters AGL (scaled) */
    int16_t velocity;       /* m/s * 10 */
    int16_t accel;          /* m/s² * 10 */
    int16_t max_alt;        /* Maximum altitude */
    uint8_t battery;        /* Battery voltage * 10 (e.g., 84 = 8.4V) */
    uint8_t pyro_status;    /* Pyro continuity & status */
    int8_t temperature;     /* °C */
    int16_t rssi;           /* RSSI of last received packet */
} TLM_Status_Payload_t;

/* Full data packet payload (48 bytes) */
typedef struct {
    uint32_t timestamp;     /* ms since boot */
    uint8_t state;          /* Flight state */
    uint8_t sensor_status;  /* Which sensors are healthy */
    
    /* Altitude/velocity */
    int32_t altitude;       /* meters * 100 (cm precision) */
    int16_t velocity;       /* m/s * 100 */
    int16_t accel_vertical; /* m/s² * 100 */
    
    /* IMU */
    int16_t accel_x;        /* m/s² * 100 */
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;         /* deg/s * 10 */
    int16_t gyro_y;
    int16_t gyro_z;
    
    /* Orientation */
    int16_t roll;           /* degrees * 100 */
    int16_t pitch;
    int16_t yaw;
    
    /* Barometer */
    uint32_t pressure;      /* Pa */
    int16_t baro_temp;      /* °C * 100 */
    
    /* Max values */
    int32_t max_altitude;   /* cm */
    int16_t max_velocity;   /* cm/s */
    int16_t max_accel;      /* cm/s² */
    
} TLM_Full_Payload_t;

/* GPS packet payload (24 bytes) */
typedef struct {
    uint32_t timestamp;     /* ms since boot */
    int32_t latitude;       /* degrees * 1e7 */
    int32_t longitude;      /* degrees * 1e7 */
    int32_t altitude_msl;   /* mm above sea level */
    uint16_t ground_speed;  /* cm/s */
    int16_t heading;        /* degrees * 100 */
    uint8_t satellites;     /* Number of satellites */
    uint8_t fix_type;       /* 0=none, 1=2D, 2=3D */
    uint8_t hdop;           /* Horizontal DOP * 10 */
    uint8_t reserved;
} TLM_GPS_Payload_t;

/* Event packet payload (8 bytes) */
typedef struct {
    uint32_t timestamp;     /* ms since boot */
    uint8_t event_type;     /* Event type */
    uint8_t event_data;     /* Additional data */
    int16_t altitude;       /* Altitude at event */
} TLM_Event_Payload_t;

/* Command packet payload (4 bytes) */
typedef struct {
    uint8_t command;        /* Command type */
    uint8_t param1;         /* Parameter 1 */
    uint8_t param2;         /* Parameter 2 */
    uint8_t checksum;       /* Simple checksum for commands */
} TLM_Command_Payload_t;

#pragma pack(pop)

/*============================================================================
 * STATUS FLAGS
 *============================================================================*/

#define TLM_FLAG_IMU1_OK        0x01
#define TLM_FLAG_IMU2_OK        0x02
#define TLM_FLAG_BARO1_OK       0x04
#define TLM_FLAG_BARO2_OK       0x08
#define TLM_FLAG_GPS_OK         0x10
#define TLM_FLAG_FLASH_OK       0x20
#define TLM_FLAG_ARMED          0x40
#define TLM_FLAG_LOGGING        0x80

/*============================================================================
 * PYRO STATUS
 *============================================================================*/

#define TLM_PYRO1_CONT          0x01    /* Drogue continuity OK */
#define TLM_PYRO2_CONT          0x02    /* Main continuity OK */
#define TLM_PYRO1_ARMED         0x04    /* Drogue armed */
#define TLM_PYRO2_ARMED         0x08    /* Main armed */
#define TLM_PYRO1_FIRED         0x10    /* Drogue has fired */
#define TLM_PYRO2_FIRED         0x20    /* Main has fired */

/*============================================================================
 * TELEMETRY HANDLE
 *============================================================================*/

typedef struct {
    /* TX buffer */
    uint8_t tx_buffer[TLM_MAX_PACKET];
    uint8_t tx_length;
    
    /* RX buffer */
    uint8_t rx_buffer[TLM_MAX_PACKET];
    uint8_t rx_index;
    bool rx_complete;
    
    /* Sequence counter */
    uint8_t sequence;
    
    /* Statistics */
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t error_count;
    
    /* Last received command */
    TLM_Command_Payload_t last_cmd;
    bool cmd_pending;
    
    /* Transmit callback */
    HAL_StatusTypeDef (*transmit)(uint8_t *data, uint16_t len);
    
    bool initialized;
    
} Telemetry_t;

/*============================================================================
 * FUNCTION PROTOTYPES
 *============================================================================*/

/**
 * @brief Initialize telemetry module
 * @param tlm Pointer to telemetry handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef Telemetry_Init(Telemetry_t *tlm);

/**
 * @brief Set transmit callback function
 * @param tlm Pointer to telemetry handle
 * @param transmit_fn Function to transmit data
 */
void Telemetry_SetTransmitCallback(Telemetry_t *tlm,
                                   HAL_StatusTypeDef (*transmit_fn)(uint8_t*, uint16_t));

/**
 * @brief Build and send status packet
 * @param tlm Pointer to telemetry handle
 * @param status Pointer to status payload
 * @return HAL_OK on success
 */
HAL_StatusTypeDef Telemetry_SendStatus(Telemetry_t *tlm, const TLM_Status_Payload_t *status);

/**
 * @brief Build and send full data packet
 * @param tlm Pointer to telemetry handle
 * @param full Pointer to full data payload
 * @return HAL_OK on success
 */
HAL_StatusTypeDef Telemetry_SendFull(Telemetry_t *tlm, const TLM_Full_Payload_t *full);

/**
 * @brief Build and send GPS packet
 * @param tlm Pointer to telemetry handle
 * @param gps Pointer to GPS payload
 * @return HAL_OK on success
 */
HAL_StatusTypeDef Telemetry_SendGPS(Telemetry_t *tlm, const TLM_GPS_Payload_t *gps);

/**
 * @brief Build and send event packet
 * @param tlm Pointer to telemetry handle
 * @param event_type Event type
 * @param event_data Additional event data
 * @param altitude Altitude at event
 * @return HAL_OK on success
 */
HAL_StatusTypeDef Telemetry_SendEvent(Telemetry_t *tlm, uint8_t event_type,
                                      uint8_t event_data, int16_t altitude);

/**
 * @brief Process received byte
 * @param tlm Pointer to telemetry handle
 * @param byte Received byte
 * @return true if complete packet received
 */
bool Telemetry_ProcessByte(Telemetry_t *tlm, uint8_t byte);

/**
 * @brief Check if command is pending
 * @param tlm Pointer to telemetry handle
 * @return true if command received
 */
bool Telemetry_IsCommandPending(Telemetry_t *tlm);

/**
 * @brief Get last received command
 * @param tlm Pointer to telemetry handle
 * @param cmd Pointer to store command
 * @return HAL_OK if command available
 */
HAL_StatusTypeDef Telemetry_GetCommand(Telemetry_t *tlm, TLM_Command_Payload_t *cmd);

/**
 * @brief Calculate CRC-16-CCITT
 * @param data Pointer to data
 * @param length Data length
 * @return CRC-16 value
 */
uint16_t Telemetry_CRC16(const uint8_t *data, uint16_t length);

/**
 * @brief Get transmit statistics
 * @param tlm Pointer to telemetry handle
 * @return Number of packets transmitted
 */
uint32_t Telemetry_GetTxCount(Telemetry_t *tlm);

/**
 * @brief Get receive statistics
 * @param tlm Pointer to telemetry handle
 * @return Number of packets received
 */
uint32_t Telemetry_GetRxCount(Telemetry_t *tlm);

/**
 * @brief Get error count
 * @param tlm Pointer to telemetry handle
 * @return Number of errors
 */
uint32_t Telemetry_GetErrorCount(Telemetry_t *tlm);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
