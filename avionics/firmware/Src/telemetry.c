/**
 * @file telemetry.c
 * @brief Telemetry Protocol Implementation
 * @author Zenith Rocket Team
 * @date 2025
 */

#include "telemetry.h"
#include <string.h>

/*============================================================================
 * RX STATE MACHINE
 *============================================================================*/

typedef enum {
    RX_STATE_SYNC1,
    RX_STATE_SYNC2,
    RX_STATE_TYPE,
    RX_STATE_SEQ,
    RX_STATE_LEN,
    RX_STATE_PAYLOAD,
    RX_STATE_CRC1,
    RX_STATE_CRC2
} RX_State_t;

static RX_State_t rx_state = RX_STATE_SYNC1;
static uint8_t rx_payload_remaining = 0;

/*============================================================================
 * CRC-16-CCITT
 *============================================================================*/

static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

uint16_t Telemetry_CRC16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    
    while (length--) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *data++];
    }
    
    return crc;
}

/*============================================================================
 * PRIVATE FUNCTIONS
 *============================================================================*/

/**
 * @brief Build packet with header and CRC
 */
static uint8_t build_packet(Telemetry_t *tlm, uint8_t type, 
                            const uint8_t *payload, uint8_t payload_len)
{
    if (payload_len > TLM_MAX_PAYLOAD) {
        return 0;
    }
    
    /* Build header */
    tlm->tx_buffer[0] = (TLM_SYNC_WORD >> 8) & 0xFF;  /* Sync high */
    tlm->tx_buffer[1] = TLM_SYNC_WORD & 0xFF;         /* Sync low */
    tlm->tx_buffer[2] = type;
    tlm->tx_buffer[3] = tlm->sequence++;
    tlm->tx_buffer[4] = payload_len;
    
    /* Copy payload */
    if (payload_len > 0 && payload != NULL) {
        memcpy(&tlm->tx_buffer[TLM_HEADER_SIZE], payload, payload_len);
    }
    
    /* Calculate CRC over header + payload */
    uint16_t crc = Telemetry_CRC16(tlm->tx_buffer, TLM_HEADER_SIZE + payload_len);
    
    /* Append CRC */
    tlm->tx_buffer[TLM_HEADER_SIZE + payload_len] = (crc >> 8) & 0xFF;
    tlm->tx_buffer[TLM_HEADER_SIZE + payload_len + 1] = crc & 0xFF;
    
    return TLM_HEADER_SIZE + payload_len + TLM_CRC_SIZE;
}

/**
 * @brief Validate received command checksum
 */
static bool validate_command(const TLM_Command_Payload_t *cmd)
{
    uint8_t sum = cmd->command + cmd->param1 + cmd->param2;
    return (cmd->checksum == (sum ^ 0xAA));
}

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

HAL_StatusTypeDef Telemetry_Init(Telemetry_t *tlm)
{
    if (tlm == NULL) return HAL_ERROR;
    
    memset(tlm, 0, sizeof(Telemetry_t));
    
    rx_state = RX_STATE_SYNC1;
    rx_payload_remaining = 0;
    
    tlm->initialized = true;
    
    return HAL_OK;
}

void Telemetry_SetTransmitCallback(Telemetry_t *tlm,
                                   HAL_StatusTypeDef (*transmit_fn)(uint8_t*, uint16_t))
{
    if (tlm == NULL) return;
    tlm->transmit = transmit_fn;
}

HAL_StatusTypeDef Telemetry_SendStatus(Telemetry_t *tlm, const TLM_Status_Payload_t *status)
{
    if (tlm == NULL || status == NULL || !tlm->initialized) {
        return HAL_ERROR;
    }
    
    tlm->tx_length = build_packet(tlm, TLM_TYPE_STATUS, 
                                  (const uint8_t *)status, 
                                  sizeof(TLM_Status_Payload_t));
    
    if (tlm->tx_length == 0) {
        return HAL_ERROR;
    }
    
    tlm->tx_count++;
    
    if (tlm->transmit) {
        return tlm->transmit(tlm->tx_buffer, tlm->tx_length);
    }
    
    return HAL_OK;
}

HAL_StatusTypeDef Telemetry_SendFull(Telemetry_t *tlm, const TLM_Full_Payload_t *full)
{
    if (tlm == NULL || full == NULL || !tlm->initialized) {
        return HAL_ERROR;
    }
    
    tlm->tx_length = build_packet(tlm, TLM_TYPE_FULL, 
                                  (const uint8_t *)full, 
                                  sizeof(TLM_Full_Payload_t));
    
    if (tlm->tx_length == 0) {
        return HAL_ERROR;
    }
    
    tlm->tx_count++;
    
    if (tlm->transmit) {
        return tlm->transmit(tlm->tx_buffer, tlm->tx_length);
    }
    
    return HAL_OK;
}

HAL_StatusTypeDef Telemetry_SendGPS(Telemetry_t *tlm, const TLM_GPS_Payload_t *gps)
{
    if (tlm == NULL || gps == NULL || !tlm->initialized) {
        return HAL_ERROR;
    }
    
    tlm->tx_length = build_packet(tlm, TLM_TYPE_GPS, 
                                  (const uint8_t *)gps, 
                                  sizeof(TLM_GPS_Payload_t));
    
    if (tlm->tx_length == 0) {
        return HAL_ERROR;
    }
    
    tlm->tx_count++;
    
    if (tlm->transmit) {
        return tlm->transmit(tlm->tx_buffer, tlm->tx_length);
    }
    
    return HAL_OK;
}

HAL_StatusTypeDef Telemetry_SendEvent(Telemetry_t *tlm, uint8_t event_type,
                                      uint8_t event_data, int16_t altitude)
{
    if (tlm == NULL || !tlm->initialized) {
        return HAL_ERROR;
    }
    
    TLM_Event_Payload_t event;
    event.timestamp = HAL_GetTick();
    event.event_type = event_type;
    event.event_data = event_data;
    event.altitude = altitude;
    
    tlm->tx_length = build_packet(tlm, TLM_TYPE_EVENT, 
                                  (const uint8_t *)&event, 
                                  sizeof(TLM_Event_Payload_t));
    
    if (tlm->tx_length == 0) {
        return HAL_ERROR;
    }
    
    tlm->tx_count++;
    
    if (tlm->transmit) {
        return tlm->transmit(tlm->tx_buffer, tlm->tx_length);
    }
    
    return HAL_OK;
}

bool Telemetry_ProcessByte(Telemetry_t *tlm, uint8_t byte)
{
    if (tlm == NULL || !tlm->initialized) {
        return false;
    }
    
    switch (rx_state) {
        case RX_STATE_SYNC1:
            if (byte == ((TLM_SYNC_WORD >> 8) & 0xFF)) {
                tlm->rx_buffer[0] = byte;
                tlm->rx_index = 1;
                rx_state = RX_STATE_SYNC2;
            }
            break;
            
        case RX_STATE_SYNC2:
            if (byte == (TLM_SYNC_WORD & 0xFF)) {
                tlm->rx_buffer[1] = byte;
                tlm->rx_index = 2;
                rx_state = RX_STATE_TYPE;
            } else {
                rx_state = RX_STATE_SYNC1;
            }
            break;
            
        case RX_STATE_TYPE:
            tlm->rx_buffer[tlm->rx_index++] = byte;
            rx_state = RX_STATE_SEQ;
            break;
            
        case RX_STATE_SEQ:
            tlm->rx_buffer[tlm->rx_index++] = byte;
            rx_state = RX_STATE_LEN;
            break;
            
        case RX_STATE_LEN:
            tlm->rx_buffer[tlm->rx_index++] = byte;
            rx_payload_remaining = byte;
            if (rx_payload_remaining > TLM_MAX_PAYLOAD) {
                /* Invalid length, reset */
                tlm->error_count++;
                rx_state = RX_STATE_SYNC1;
            } else if (rx_payload_remaining == 0) {
                rx_state = RX_STATE_CRC1;
            } else {
                rx_state = RX_STATE_PAYLOAD;
            }
            break;
            
        case RX_STATE_PAYLOAD:
            tlm->rx_buffer[tlm->rx_index++] = byte;
            rx_payload_remaining--;
            if (rx_payload_remaining == 0) {
                rx_state = RX_STATE_CRC1;
            }
            break;
            
        case RX_STATE_CRC1:
            tlm->rx_buffer[tlm->rx_index++] = byte;
            rx_state = RX_STATE_CRC2;
            break;
            
        case RX_STATE_CRC2:
            tlm->rx_buffer[tlm->rx_index++] = byte;
            
            /* Verify CRC */
            uint8_t payload_len = tlm->rx_buffer[4];
            uint16_t received_crc = (tlm->rx_buffer[TLM_HEADER_SIZE + payload_len] << 8) |
                                     tlm->rx_buffer[TLM_HEADER_SIZE + payload_len + 1];
            uint16_t calculated_crc = Telemetry_CRC16(tlm->rx_buffer, 
                                                       TLM_HEADER_SIZE + payload_len);
            
            if (received_crc == calculated_crc) {
                /* Valid packet received */
                tlm->rx_complete = true;
                tlm->rx_count++;
                
                /* Check if it's a command */
                if (tlm->rx_buffer[2] == TLM_TYPE_CMD && payload_len >= sizeof(TLM_Command_Payload_t)) {
                    memcpy(&tlm->last_cmd, &tlm->rx_buffer[TLM_HEADER_SIZE], 
                           sizeof(TLM_Command_Payload_t));
                    if (validate_command(&tlm->last_cmd)) {
                        tlm->cmd_pending = true;
                    }
                }
                
                rx_state = RX_STATE_SYNC1;
                return true;
            } else {
                /* CRC error */
                tlm->error_count++;
            }
            
            rx_state = RX_STATE_SYNC1;
            break;
    }
    
    return false;
}

bool Telemetry_IsCommandPending(Telemetry_t *tlm)
{
    if (tlm == NULL) return false;
    return tlm->cmd_pending;
}

HAL_StatusTypeDef Telemetry_GetCommand(Telemetry_t *tlm, TLM_Command_Payload_t *cmd)
{
    if (tlm == NULL || cmd == NULL) return HAL_ERROR;
    
    if (!tlm->cmd_pending) {
        return HAL_ERROR;
    }
    
    memcpy(cmd, &tlm->last_cmd, sizeof(TLM_Command_Payload_t));
    tlm->cmd_pending = false;
    
    return HAL_OK;
}

uint32_t Telemetry_GetTxCount(Telemetry_t *tlm)
{
    if (tlm == NULL) return 0;
    return tlm->tx_count;
}

uint32_t Telemetry_GetRxCount(Telemetry_t *tlm)
{
    if (tlm == NULL) return 0;
    return tlm->rx_count;
}

uint32_t Telemetry_GetErrorCount(Telemetry_t *tlm)
{
    if (tlm == NULL) return 0;
    return tlm->error_count;
}
