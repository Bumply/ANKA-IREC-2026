/**
 * @file flight_state.h
 * @brief Flight State Machine for Rocket Flight Computer
 * @author Zenith Rocket Team
 * @date 2025
 * 
 * Flight States:
 * 1. BOOT      - System initialization
 * 2. IDLE      - Waiting on pad, not armed
 * 3. ARMED     - Armed, ready for launch
 * 4. BOOST     - Motor burning, accelerating
 * 5. COAST     - Motor burnout, still ascending
 * 6. APOGEE    - Peak altitude detected, drogue deployed
 * 7. DESCENT   - Descending under drogue
 * 8. MAIN      - Main chute deployed
 * 9. LANDED    - On ground, flight complete
 * 10. ERROR    - System error state
 * 
 * Transitions are based on:
 * - Acceleration (detect launch and burnout)
 * - Altitude (detect apogee and main deployment altitude)
 * - Velocity (confirm direction of travel)
 */

#ifndef FLIGHT_STATE_H
#define FLIGHT_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * FLIGHT STATES
 *============================================================================*/

typedef enum {
    FLIGHT_STATE_BOOT = 0,
    FLIGHT_STATE_IDLE,
    FLIGHT_STATE_ARMED,
    FLIGHT_STATE_BOOST,
    FLIGHT_STATE_COAST,
    FLIGHT_STATE_APOGEE,
    FLIGHT_STATE_DESCENT,
    FLIGHT_STATE_MAIN,
    FLIGHT_STATE_LANDED,
    FLIGHT_STATE_ERROR
} FlightState_t;

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

typedef struct {
    /* Launch detection */
    float launch_accel_threshold;       /* m/s², typically 30-50 */
    uint16_t launch_accel_samples;      /* Number of samples above threshold */
    
    /* Burnout detection */
    float burnout_accel_threshold;      /* m/s², typically < 5 */
    uint16_t burnout_samples;           /* Number of samples below threshold */
    
    /* Apogee detection */
    float apogee_velocity_threshold;    /* m/s, typically < 5 */
    float apogee_altitude_drop;         /* meters, backup: altitude dropping */
    uint16_t apogee_samples;            /* Number of samples for confirmation */
    
    /* Main deployment */
    float main_deploy_altitude;         /* meters AGL, typically 457m (1500ft) */
    
    /* Landing detection */
    float landed_velocity_threshold;    /* m/s, typically < 2 */
    float landed_altitude_variance;     /* meters, altitude stable */
    uint16_t landed_samples;            /* Number of stable samples */
    
    /* Timeouts (safety backups) */
    uint32_t max_boost_time_ms;         /* Max expected boost duration */
    uint32_t max_coast_time_ms;         /* Max time to apogee */
    uint32_t max_descent_time_ms;       /* Max time under drogue */
    uint32_t apogee_lockout_ms;         /* Min time before apogee detection */
    
    /* Ground level */
    float ground_altitude;              /* Altitude at launch site (m ASL) */
    float ground_pressure;              /* Pressure at ground (Pa) */
    
} FlightConfig_t;

/*============================================================================
 * FLIGHT DATA
 *============================================================================*/

typedef struct {
    /* Current measurements */
    float altitude;             /* meters AGL */
    float velocity;             /* m/s vertical */
    float acceleration;         /* m/s² vertical */
    float pressure;             /* Pa */
    float temperature;          /* °C */
    
    /* Peak values */
    float max_altitude;         /* Maximum altitude reached */
    float max_velocity;         /* Maximum velocity reached */
    float max_acceleration;     /* Maximum acceleration experienced */
    
    /* Timestamps */
    uint32_t launch_time;       /* Tick when launch detected */
    uint32_t burnout_time;      /* Tick when burnout detected */
    uint32_t apogee_time;       /* Tick when apogee detected */
    uint32_t main_time;         /* Tick when main deployed */
    uint32_t landed_time;       /* Tick when landed */
    
    /* Sample counters for debouncing */
    uint16_t launch_count;
    uint16_t burnout_count;
    uint16_t apogee_count;
    uint16_t landed_count;
    
    /* Altitude history for apogee detection */
    float altitude_history[10];
    uint8_t altitude_history_idx;
    
} FlightData_t;

/*============================================================================
 * HANDLE STRUCTURE
 *============================================================================*/

typedef struct {
    FlightState_t state;
    FlightState_t prev_state;
    
    FlightConfig_t config;
    FlightData_t data;
    
    /* Callbacks */
    void (*on_state_change)(FlightState_t old_state, FlightState_t new_state);
    void (*on_fire_drogue)(void);
    void (*on_fire_main)(void);
    
    /* Status flags */
    bool drogue_fired;
    bool main_fired;
    bool error_flag;
    uint8_t error_code;
    
    /* Timing */
    uint32_t state_entry_tick;
    uint32_t last_update_tick;
    
    bool initialized;
    
} FlightStateMachine_t;

/*============================================================================
 * ERROR CODES
 *============================================================================*/

#define FLIGHT_ERROR_NONE               0x00
#define FLIGHT_ERROR_SENSOR_FAIL        0x01
#define FLIGHT_ERROR_TIMEOUT_BOOST      0x02
#define FLIGHT_ERROR_TIMEOUT_COAST      0x03
#define FLIGHT_ERROR_TIMEOUT_DESCENT    0x04
#define FLIGHT_ERROR_PYRO_FAIL          0x05
#define FLIGHT_ERROR_ALTITUDE_INVALID   0x06

/*============================================================================
 * FUNCTION PROTOTYPES
 *============================================================================*/

/**
 * @brief Initialize flight state machine
 * @param fsm Pointer to state machine handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef FlightState_Init(FlightStateMachine_t *fsm);

/**
 * @brief Set default configuration values
 * @param config Pointer to config structure
 */
void FlightState_DefaultConfig(FlightConfig_t *config);

/**
 * @brief Set configuration
 * @param fsm Pointer to state machine handle
 * @param config Pointer to config structure
 */
void FlightState_SetConfig(FlightStateMachine_t *fsm, const FlightConfig_t *config);

/**
 * @brief Set ground reference (call before arming)
 * @param fsm Pointer to state machine handle
 * @param altitude Current altitude in meters ASL
 * @param pressure Current pressure in Pa
 */
void FlightState_SetGroundReference(FlightStateMachine_t *fsm, float altitude, float pressure);

/**
 * @brief Set state change callback
 * @param fsm Pointer to state machine handle
 * @param callback Function to call on state change
 */
void FlightState_SetStateCallback(FlightStateMachine_t *fsm,
                                  void (*callback)(FlightState_t, FlightState_t));

/**
 * @brief Set pyro fire callbacks
 * @param fsm Pointer to state machine handle
 * @param drogue_cb Function to call when drogue should fire
 * @param main_cb Function to call when main should fire
 */
void FlightState_SetPyroCallbacks(FlightStateMachine_t *fsm,
                                  void (*drogue_cb)(void),
                                  void (*main_cb)(void));

/**
 * @brief Arm the flight computer
 * @param fsm Pointer to state machine handle
 * @return HAL_OK if armed successfully
 */
HAL_StatusTypeDef FlightState_Arm(FlightStateMachine_t *fsm);

/**
 * @brief Disarm the flight computer
 * @param fsm Pointer to state machine handle
 */
void FlightState_Disarm(FlightStateMachine_t *fsm);

/**
 * @brief Check if armed
 * @param fsm Pointer to state machine handle
 * @return true if armed
 */
bool FlightState_IsArmed(FlightStateMachine_t *fsm);

/**
 * @brief Update state machine with new sensor data
 * @param fsm Pointer to state machine handle
 * @param altitude Current altitude AGL in meters
 * @param velocity Current vertical velocity in m/s
 * @param acceleration Current vertical acceleration in m/s²
 */
void FlightState_Update(FlightStateMachine_t *fsm, float altitude, 
                        float velocity, float acceleration);

/**
 * @brief Get current state
 * @param fsm Pointer to state machine handle
 * @return Current flight state
 */
FlightState_t FlightState_GetState(FlightStateMachine_t *fsm);

/**
 * @brief Get state name as string
 * @param state Flight state
 * @return State name string
 */
const char* FlightState_GetStateName(FlightState_t state);

/**
 * @brief Get flight data
 * @param fsm Pointer to state machine handle
 * @return Pointer to flight data
 */
const FlightData_t* FlightState_GetData(FlightStateMachine_t *fsm);

/**
 * @brief Get maximum altitude reached
 * @param fsm Pointer to state machine handle
 * @return Maximum altitude in meters AGL
 */
float FlightState_GetMaxAltitude(FlightStateMachine_t *fsm);

/**
 * @brief Get time since launch
 * @param fsm Pointer to state machine handle
 * @return Time in milliseconds, 0 if not launched
 */
uint32_t FlightState_GetFlightTime(FlightStateMachine_t *fsm);

/**
 * @brief Check if flight is complete
 * @param fsm Pointer to state machine handle
 * @return true if landed
 */
bool FlightState_IsFlightComplete(FlightStateMachine_t *fsm);

/**
 * @brief Check for errors
 * @param fsm Pointer to state machine handle
 * @return Error code (0 if none)
 */
uint8_t FlightState_GetError(FlightStateMachine_t *fsm);

/**
 * @brief Force state transition (for testing only!)
 * @param fsm Pointer to state machine handle
 * @param state New state
 */
void FlightState_ForceState(FlightStateMachine_t *fsm, FlightState_t state);

/**
 * @brief Reset state machine
 * @param fsm Pointer to state machine handle
 */
void FlightState_Reset(FlightStateMachine_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* FLIGHT_STATE_H */
