/**
 * @file data_logger.c
 * @brief Flight Data Logger Implementation
 * @author Zenith Rocket Team
 * @date 2025
 */

#include "data_logger.h"
#include "telemetry.h"  /* For CRC function */
#include <string.h>

/*============================================================================
 * PRIVATE FUNCTIONS
 *============================================================================*/

/**
 * @brief Get timestamp relative to launch
 */
static uint16_t get_timestamp(DataLogger_t *logger)
{
    uint32_t now = HAL_GetTick();
    if (logger->launch_tick == 0) {
        return 0;
    }
    /* Return ms since launch, wraps at 65535 (~65 seconds) */
    return (uint16_t)(now - logger->launch_tick);
}

/**
 * @brief Write buffer to flash
 */
static HAL_StatusTypeDef write_buffer(DataLogger_t *logger)
{
    if (logger->buffer_idx == 0) {
        return HAL_OK;
    }
    
    /* Check space */
    if (logger->write_addr + logger->buffer_idx > LOG_DATA_ADDR + LOG_DATA_SIZE) {
        return HAL_ERROR;  /* Out of space */
    }
    
    /* Write data */
    HAL_StatusTypeDef status = W25Q_Write(logger->flash, logger->write_addr,
                                          logger->buffer, logger->buffer_idx);
    
    if (status == HAL_OK) {
        logger->write_addr += logger->buffer_idx;
        logger->buffer_idx = 0;
    }
    
    return status;
}

/**
 * @brief Add data to buffer, flush if needed
 */
static HAL_StatusTypeDef buffer_write(DataLogger_t *logger, const uint8_t *data, uint16_t len)
{
    if (logger->buffer_idx + len > sizeof(logger->buffer)) {
        /* Buffer full, flush first */
        HAL_StatusTypeDef status = write_buffer(logger);
        if (status != HAL_OK) {
            return status;
        }
    }
    
    /* Add to buffer */
    memcpy(&logger->buffer[logger->buffer_idx], data, len);
    logger->buffer_idx += len;
    
    return HAL_OK;
}

/**
 * @brief Calculate CRC for header
 */
static uint16_t calculate_header_crc(const LogHeader_t *header)
{
    /* CRC over everything except the CRC field itself */
    return Telemetry_CRC16((const uint8_t *)header, sizeof(LogHeader_t) - 2);
}

/**
 * @brief Write header to flash
 */
static HAL_StatusTypeDef write_header(DataLogger_t *logger)
{
    /* Calculate CRC */
    logger->header.crc = calculate_header_crc(&logger->header);
    
    /* Erase header sector first */
    HAL_StatusTypeDef status = W25Q_EraseSector(logger->flash, LOG_HEADER_ADDR);
    if (status != HAL_OK) {
        return status;
    }
    
    /* Write header */
    return W25Q_Write(logger->flash, LOG_HEADER_ADDR, 
                      (uint8_t *)&logger->header, sizeof(LogHeader_t));
}

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

HAL_StatusTypeDef DataLogger_Init(DataLogger_t *logger, W25Q_Handle_t *flash)
{
    if (logger == NULL || flash == NULL) {
        return HAL_ERROR;
    }
    
    memset(logger, 0, sizeof(DataLogger_t));
    
    logger->flash = flash;
    logger->state = LOGGER_STATE_IDLE;
    logger->write_addr = LOG_DATA_ADDR;
    
    /* Try to read existing header */
    LogHeader_t existing_header;
    if (DataLogger_ReadHeader(logger, &existing_header) == HAL_OK) {
        /* Valid header exists, increment flight number */
        logger->header.flight_number = existing_header.flight_number + 1;
    } else {
        /* No valid header, start at flight 1 */
        logger->header.flight_number = 1;
    }
    
    /* Initialize header */
    logger->header.magic = LOG_HEADER_MAGIC;
    logger->header.version = 1;
    logger->header.data_start = LOG_DATA_ADDR;
    
    logger->initialized = true;
    logger->state = LOGGER_STATE_READY;
    
    return HAL_OK;
}

HAL_StatusTypeDef DataLogger_Erase(DataLogger_t *logger)
{
    if (logger == NULL || !logger->initialized) {
        return HAL_ERROR;
    }
    
    /* Erase all data sectors */
    /* W25Q40 has 128 sectors of 4KB each */
    /* Data area starts at sector 1 (after header) */
    
    uint32_t addr = LOG_DATA_ADDR;
    while (addr < LOG_DATA_ADDR + LOG_DATA_SIZE) {
        HAL_StatusTypeDef status = W25Q_EraseSector(logger->flash, addr);
        if (status != HAL_OK) {
            logger->state = LOGGER_STATE_ERROR;
            return status;
        }
        addr += 4096;  /* 4KB sectors */
    }
    
    /* Reset write position */
    logger->write_addr = LOG_DATA_ADDR;
    logger->record_count = 0;
    logger->buffer_idx = 0;
    
    /* Reset header statistics */
    logger->header.launch_time = 0;
    logger->header.apogee_time = 0;
    logger->header.main_time = 0;
    logger->header.landed_time = 0;
    logger->header.max_altitude = 0;
    logger->header.max_velocity = 0;
    logger->header.max_accel = 0;
    logger->header.data_end = LOG_DATA_ADDR;
    logger->header.record_count = 0;
    
    logger->state = LOGGER_STATE_READY;
    
    return HAL_OK;
}

void DataLogger_SetGroundReference(DataLogger_t *logger, int32_t altitude, uint32_t pressure)
{
    if (logger == NULL) return;
    
    logger->header.ground_altitude = altitude;
    logger->header.ground_pressure = pressure;
}

HAL_StatusTypeDef DataLogger_Start(DataLogger_t *logger)
{
    if (logger == NULL || !logger->initialized) {
        return HAL_ERROR;
    }
    
    if (logger->state != LOGGER_STATE_READY) {
        return HAL_ERROR;
    }
    
    /* Record launch tick */
    logger->launch_tick = HAL_GetTick();
    logger->header.start_time = logger->launch_tick;  /* Could be RTC time */
    
    logger->state = LOGGER_STATE_LOGGING;
    
    return HAL_OK;
}

HAL_StatusTypeDef DataLogger_Stop(DataLogger_t *logger)
{
    if (logger == NULL || !logger->initialized) {
        return HAL_ERROR;
    }
    
    if (logger->state != LOGGER_STATE_LOGGING) {
        return HAL_ERROR;
    }
    
    /* Flush any remaining data */
    HAL_StatusTypeDef status = DataLogger_Flush(logger);
    if (status != HAL_OK) {
        return status;
    }
    
    /* Update header with final values */
    logger->header.data_end = logger->write_addr;
    logger->header.record_count = logger->record_count;
    logger->header.landed_time = HAL_GetTick() - logger->launch_tick;
    
    /* Write header */
    status = write_header(logger);
    
    logger->state = LOGGER_STATE_STOPPED;
    
    return status;
}

HAL_StatusTypeDef DataLogger_LogSensor(DataLogger_t *logger,
                                       int32_t altitude, int16_t velocity, int16_t accel,
                                       int16_t roll, int16_t pitch, int16_t yaw,
                                       int16_t ax, int16_t ay, int16_t az,
                                       uint8_t state, uint8_t flags)
{
    if (logger == NULL || logger->state != LOGGER_STATE_LOGGING) {
        return HAL_ERROR;
    }
    
    LogSensorRecord_t record;
    record.magic = LOG_RECORD_MAGIC;
    record.type = LOG_RECORD_SENSOR;
    record.timestamp = get_timestamp(logger);
    record.altitude = altitude;
    record.velocity = velocity;
    record.accel = accel;
    record.roll = roll;
    record.pitch = pitch;
    record.yaw = yaw;
    record.accel_x = ax;
    record.accel_y = ay;
    record.accel_z = az;
    record.state = state;
    record.flags = flags;
    
    HAL_StatusTypeDef status = buffer_write(logger, (uint8_t *)&record, sizeof(record));
    if (status == HAL_OK) {
        logger->record_count++;
    }
    
    return status;
}

HAL_StatusTypeDef DataLogger_LogGPS(DataLogger_t *logger,
                                    int32_t lat, int32_t lon, int32_t alt,
                                    uint16_t speed, int16_t heading,
                                    uint8_t sats, uint8_t fix)
{
    if (logger == NULL || logger->state != LOGGER_STATE_LOGGING) {
        return HAL_ERROR;
    }
    
    LogGPSRecord_t record;
    record.magic = LOG_RECORD_MAGIC;
    record.type = LOG_RECORD_GPS;
    record.timestamp = get_timestamp(logger);
    record.latitude = lat;
    record.longitude = lon;
    record.altitude = alt;
    record.speed = speed;
    record.heading = heading;
    record.satellites = sats;
    record.fix = fix;
    record.reserved = 0;
    
    HAL_StatusTypeDef status = buffer_write(logger, (uint8_t *)&record, sizeof(record));
    if (status == HAL_OK) {
        logger->record_count++;
    }
    
    /* Update header with launch coordinates if first GPS log */
    if (logger->header.launch_lat == 0 && logger->header.launch_lon == 0) {
        logger->header.launch_lat = lat;
        logger->header.launch_lon = lon;
    }
    
    return status;
}

HAL_StatusTypeDef DataLogger_LogEvent(DataLogger_t *logger,
                                      uint8_t event, uint8_t data, int16_t altitude)
{
    if (logger == NULL || logger->state != LOGGER_STATE_LOGGING) {
        return HAL_ERROR;
    }
    
    LogEventRecord_t record;
    record.magic = LOG_RECORD_MAGIC;
    record.type = LOG_RECORD_EVENT;
    record.timestamp = get_timestamp(logger);
    record.event = event;
    record.data = data;
    record.altitude = altitude;
    
    HAL_StatusTypeDef status = buffer_write(logger, (uint8_t *)&record, sizeof(record));
    if (status == HAL_OK) {
        logger->record_count++;
    }
    
    /* Update header timestamps for key events */
    uint32_t time_since_start = HAL_GetTick() - logger->header.start_time;
    switch (event) {
        case 0x03:  /* Launch */
            logger->header.launch_time = time_since_start;
            break;
        case 0x05:  /* Apogee */
            logger->header.apogee_time = time_since_start;
            break;
        case 0x07:  /* Main */
            logger->header.main_time = time_since_start;
            break;
        case 0x08:  /* Landed */
            logger->header.landed_time = time_since_start;
            break;
    }
    
    return status;
}

void DataLogger_UpdateStats(DataLogger_t *logger, 
                           int32_t max_alt, int16_t max_vel, int16_t max_acc)
{
    if (logger == NULL) return;
    
    if (max_alt > logger->header.max_altitude) {
        logger->header.max_altitude = max_alt;
    }
    if (max_vel > logger->header.max_velocity) {
        logger->header.max_velocity = max_vel;
    }
    if (max_acc > logger->header.max_accel) {
        logger->header.max_accel = max_acc;
    }
}

void DataLogger_SetLandingCoords(DataLogger_t *logger, int32_t lat, int32_t lon)
{
    if (logger == NULL) return;
    
    logger->header.landing_lat = lat;
    logger->header.landing_lon = lon;
}

HAL_StatusTypeDef DataLogger_Flush(DataLogger_t *logger)
{
    if (logger == NULL || !logger->initialized) {
        return HAL_ERROR;
    }
    
    return write_buffer(logger);
}

bool DataLogger_IsLogging(DataLogger_t *logger)
{
    if (logger == NULL) return false;
    return (logger->state == LOGGER_STATE_LOGGING);
}

uint32_t DataLogger_GetRecordCount(DataLogger_t *logger)
{
    if (logger == NULL) return 0;
    return logger->record_count;
}

HAL_StatusTypeDef DataLogger_ReadHeader(DataLogger_t *logger, LogHeader_t *header)
{
    if (logger == NULL || header == NULL || logger->flash == NULL) {
        return HAL_ERROR;
    }
    
    /* Read header from flash */
    HAL_StatusTypeDef status = W25Q_Read(logger->flash, LOG_HEADER_ADDR,
                                         (uint8_t *)header, sizeof(LogHeader_t));
    if (status != HAL_OK) {
        return status;
    }
    
    /* Verify magic number */
    if (header->magic != LOG_HEADER_MAGIC) {
        return HAL_ERROR;
    }
    
    /* Verify CRC */
    uint16_t calculated_crc = calculate_header_crc(header);
    if (header->crc != calculated_crc) {
        return HAL_ERROR;
    }
    
    return HAL_OK;
}

uint16_t DataLogger_ReadRecord(DataLogger_t *logger, uint32_t offset,
                               uint8_t *buffer, uint16_t max_len)
{
    if (logger == NULL || buffer == NULL || logger->flash == NULL) {
        return 0;
    }
    
    /* Calculate address */
    uint32_t addr = LOG_DATA_ADDR + offset;
    
    if (addr >= logger->header.data_end || addr >= LOG_DATA_ADDR + LOG_DATA_SIZE) {
        return 0;  /* Beyond data */
    }
    
    /* Read first 2 bytes to get record type */
    uint8_t header_bytes[2];
    if (W25Q_Read(logger->flash, addr, header_bytes, 2) != HAL_OK) {
        return 0;
    }
    
    /* Check magic */
    if (header_bytes[0] != LOG_RECORD_MAGIC) {
        return 0;
    }
    
    /* Determine record size based on type */
    uint16_t record_size;
    switch (header_bytes[1]) {
        case LOG_RECORD_SENSOR:
            record_size = sizeof(LogSensorRecord_t);
            break;
        case LOG_RECORD_GPS:
            record_size = sizeof(LogGPSRecord_t);
            break;
        case LOG_RECORD_EVENT:
            record_size = sizeof(LogEventRecord_t);
            break;
        default:
            return 0;  /* Unknown type */
    }
    
    if (record_size > max_len) {
        return 0;  /* Buffer too small */
    }
    
    /* Read full record */
    if (W25Q_Read(logger->flash, addr, buffer, record_size) != HAL_OK) {
        return 0;
    }
    
    return record_size;
}

uint32_t DataLogger_GetFreeSpace(DataLogger_t *logger)
{
    if (logger == NULL) return 0;
    
    uint32_t used = logger->write_addr - LOG_DATA_ADDR;
    if (used >= LOG_DATA_SIZE) {
        return 0;
    }
    
    return LOG_DATA_SIZE - used;
}
