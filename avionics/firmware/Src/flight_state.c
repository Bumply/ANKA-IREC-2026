/**
 * @file flight_state.c
 * @brief Flight State Machine Implementation
 * @author Zenith Rocket Team
 * @date 2025
 * 
 * State Transition Logic:
 * 
 * BOOT → IDLE: After successful initialization
 * IDLE → ARMED: When arm command received
 * ARMED → IDLE: When disarm command received
 * ARMED → BOOST: When sustained high acceleration detected
 * BOOST → COAST: When acceleration drops (motor burnout)
 * COAST → APOGEE: When velocity approaches zero / altitude decreasing
 * APOGEE → DESCENT: After drogue deployment (immediate)
 * DESCENT → MAIN: When altitude drops below main deployment altitude
 * MAIN → LANDED: When velocity ~0 and altitude stable
 */

#include "flight_state.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * PRIVATE DEFINES
 *============================================================================*/

#define ALTITUDE_HISTORY_SIZE   10

/*============================================================================
 * STATE NAMES
 *============================================================================*/

static const char* state_names[] = {
    "BOOT",
    "IDLE",
    "ARMED",
    "BOOST",
    "COAST",
    "APOGEE",
    "DESCENT",
    "MAIN",
    "LANDED",
    "ERROR"
};

/*============================================================================
 * PRIVATE FUNCTIONS
 *============================================================================*/

/**
 * @brief Transition to new state
 */
static void transition_to(FlightStateMachine_t *fsm, FlightState_t new_state)
{
    if (fsm->state == new_state) return;
    
    FlightState_t old_state = fsm->state;
    fsm->prev_state = old_state;
    fsm->state = new_state;
    fsm->state_entry_tick = HAL_GetTick();
    
    /* Record timestamps for key events */
    uint32_t now = HAL_GetTick();
    switch (new_state) {
        case FLIGHT_STATE_BOOST:
            fsm->data.launch_time = now;
            break;
        case FLIGHT_STATE_COAST:
            fsm->data.burnout_time = now;
            break;
        case FLIGHT_STATE_APOGEE:
            fsm->data.apogee_time = now;
            break;
        case FLIGHT_STATE_MAIN:
            fsm->data.main_time = now;
            break;
        case FLIGHT_STATE_LANDED:
            fsm->data.landed_time = now;
            break;
        default:
            break;
    }
    
    /* Fire state change callback */
    if (fsm->on_state_change) {
        fsm->on_state_change(old_state, new_state);
    }
}

/**
 * @brief Get time in current state
 */
static uint32_t time_in_state(FlightStateMachine_t *fsm)
{
    return HAL_GetTick() - fsm->state_entry_tick;
}

/**
 * @brief Compute average of altitude history
 */
static float altitude_average(FlightStateMachine_t *fsm)
{
    float sum = 0.0f;
    for (int i = 0; i < ALTITUDE_HISTORY_SIZE; i++) {
        sum += fsm->data.altitude_history[i];
    }
    return sum / ALTITUDE_HISTORY_SIZE;
}

/**
 * @brief Detect if altitude is decreasing
 */
static bool altitude_decreasing(FlightStateMachine_t *fsm)
{
    /* Check if recent samples show downward trend */
    uint8_t idx = fsm->data.altitude_history_idx;
    float newest = fsm->data.altitude_history[idx];
    
    /* Compare with older samples */
    uint8_t older_idx = (idx + ALTITUDE_HISTORY_SIZE - 3) % ALTITUDE_HISTORY_SIZE;
    float older = fsm->data.altitude_history[older_idx];
    
    return (older - newest) > fsm->config.apogee_altitude_drop;
}

/*============================================================================
 * STATE HANDLERS
 *============================================================================*/

static void handle_idle(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    /* In IDLE, we're just waiting to be armed */
    /* Update ground reference periodically */
    fsm->config.ground_altitude = alt;
    
    (void)vel;
    (void)acc;
}

static void handle_armed(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    (void)alt;
    (void)vel;
    
    /* Detect launch by sustained high acceleration */
    if (acc > fsm->config.launch_accel_threshold) {
        fsm->data.launch_count++;
        if (fsm->data.launch_count >= fsm->config.launch_accel_samples) {
            transition_to(fsm, FLIGHT_STATE_BOOST);
        }
    } else {
        /* Reset counter if acceleration drops */
        fsm->data.launch_count = 0;
    }
}

static void handle_boost(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    (void)vel;
    
    /* Track maximum values */
    if (alt > fsm->data.max_altitude) {
        fsm->data.max_altitude = alt;
    }
    if (acc > fsm->data.max_acceleration) {
        fsm->data.max_acceleration = acc;
    }
    
    /* Detect burnout by low acceleration */
    if (acc < fsm->config.burnout_accel_threshold) {
        fsm->data.burnout_count++;
        if (fsm->data.burnout_count >= fsm->config.burnout_samples) {
            transition_to(fsm, FLIGHT_STATE_COAST);
        }
    } else {
        fsm->data.burnout_count = 0;
    }
    
    /* Timeout safety - max boost time */
    if (time_in_state(fsm) > fsm->config.max_boost_time_ms) {
        transition_to(fsm, FLIGHT_STATE_COAST);
    }
}

static void handle_coast(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    (void)acc;
    
    /* Track maximum values */
    if (alt > fsm->data.max_altitude) {
        fsm->data.max_altitude = alt;
    }
    if (fabsf(vel) > fsm->data.max_velocity) {
        fsm->data.max_velocity = fabsf(vel);
    }
    
    /* Minimum time before apogee detection (prevent false triggers) */
    if (time_in_state(fsm) < fsm->config.apogee_lockout_ms) {
        return;
    }
    
    /* Detect apogee by:
     * 1. Low/negative velocity, OR
     * 2. Altitude decreasing
     */
    bool vel_trigger = (vel < fsm->config.apogee_velocity_threshold);
    bool alt_trigger = altitude_decreasing(fsm);
    
    if (vel_trigger || alt_trigger) {
        fsm->data.apogee_count++;
        if (fsm->data.apogee_count >= fsm->config.apogee_samples) {
            /* Record apogee altitude */
            fsm->data.max_altitude = alt;
            transition_to(fsm, FLIGHT_STATE_APOGEE);
        }
    } else {
        fsm->data.apogee_count = 0;
    }
    
    /* Timeout safety - max coast time */
    if (time_in_state(fsm) > fsm->config.max_coast_time_ms) {
        fsm->data.max_altitude = alt;
        transition_to(fsm, FLIGHT_STATE_APOGEE);
    }
}

static void handle_apogee(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    (void)alt;
    (void)vel;
    (void)acc;
    
    /* Fire drogue immediately */
    if (!fsm->drogue_fired) {
        fsm->drogue_fired = true;
        if (fsm->on_fire_drogue) {
            fsm->on_fire_drogue();
        }
    }
    
    /* Transition to descent immediately after drogue */
    transition_to(fsm, FLIGHT_STATE_DESCENT);
}

static void handle_descent(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    (void)vel;
    (void)acc;
    
    /* Check for main deployment altitude */
    if (alt <= fsm->config.main_deploy_altitude) {
        transition_to(fsm, FLIGHT_STATE_MAIN);
    }
    
    /* Timeout safety */
    if (time_in_state(fsm) > fsm->config.max_descent_time_ms) {
        transition_to(fsm, FLIGHT_STATE_MAIN);
    }
}

static void handle_main(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    (void)acc;
    
    /* Fire main immediately */
    if (!fsm->main_fired) {
        fsm->main_fired = true;
        if (fsm->on_fire_main) {
            fsm->on_fire_main();
        }
    }
    
    /* Detect landing by stable low velocity and altitude */
    if (fabsf(vel) < fsm->config.landed_velocity_threshold) {
        /* Check altitude variance */
        float avg_alt = altitude_average(fsm);
        if (fabsf(alt - avg_alt) < fsm->config.landed_altitude_variance) {
            fsm->data.landed_count++;
            if (fsm->data.landed_count >= fsm->config.landed_samples) {
                transition_to(fsm, FLIGHT_STATE_LANDED);
            }
        } else {
            fsm->data.landed_count = 0;
        }
    } else {
        fsm->data.landed_count = 0;
    }
}

static void handle_landed(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    /* Flight complete - nothing to do */
    (void)fsm;
    (void)alt;
    (void)vel;
    (void)acc;
}

static void handle_error(FlightStateMachine_t *fsm, float alt, float vel, float acc)
{
    /* Error state - nothing to do, requires reset */
    (void)fsm;
    (void)alt;
    (void)vel;
    (void)acc;
}

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

HAL_StatusTypeDef FlightState_Init(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return HAL_ERROR;
    
    memset(fsm, 0, sizeof(FlightStateMachine_t));
    
    fsm->state = FLIGHT_STATE_BOOT;
    fsm->prev_state = FLIGHT_STATE_BOOT;
    fsm->state_entry_tick = HAL_GetTick();
    
    /* Set default configuration */
    FlightState_DefaultConfig(&fsm->config);
    
    fsm->initialized = true;
    
    /* Transition to IDLE */
    transition_to(fsm, FLIGHT_STATE_IDLE);
    
    return HAL_OK;
}

void FlightState_DefaultConfig(FlightConfig_t *config)
{
    if (config == NULL) return;
    
    /* Launch detection: 3G for 5 samples */
    config->launch_accel_threshold = 30.0f;     /* ~3G */
    config->launch_accel_samples = 5;
    
    /* Burnout detection: < 0.5G for 5 samples */
    config->burnout_accel_threshold = 5.0f;
    config->burnout_samples = 5;
    
    /* Apogee detection */
    config->apogee_velocity_threshold = 5.0f;   /* m/s */
    config->apogee_altitude_drop = 2.0f;        /* meters */
    config->apogee_samples = 3;
    
    /* Main deployment at 457m (1500ft) AGL - IREC requirement */
    config->main_deploy_altitude = 457.0f;
    
    /* Landing detection */
    config->landed_velocity_threshold = 2.0f;
    config->landed_altitude_variance = 3.0f;
    config->landed_samples = 50;                /* ~5 seconds at 10Hz */
    
    /* Timeouts */
    config->max_boost_time_ms = 10000;          /* 10 seconds max burn */
    config->max_coast_time_ms = 60000;          /* 60 seconds to apogee */
    config->max_descent_time_ms = 120000;       /* 2 minutes under drogue */
    config->apogee_lockout_ms = 5000;           /* 5 seconds minimum before apogee */
    
    /* Ground reference (to be set later) */
    config->ground_altitude = 0.0f;
    config->ground_pressure = 101325.0f;        /* Sea level */
}

void FlightState_SetConfig(FlightStateMachine_t *fsm, const FlightConfig_t *config)
{
    if (fsm == NULL || config == NULL) return;
    memcpy(&fsm->config, config, sizeof(FlightConfig_t));
}

void FlightState_SetGroundReference(FlightStateMachine_t *fsm, float altitude, float pressure)
{
    if (fsm == NULL) return;
    fsm->config.ground_altitude = altitude;
    fsm->config.ground_pressure = pressure;
}

void FlightState_SetStateCallback(FlightStateMachine_t *fsm,
                                  void (*callback)(FlightState_t, FlightState_t))
{
    if (fsm == NULL) return;
    fsm->on_state_change = callback;
}

void FlightState_SetPyroCallbacks(FlightStateMachine_t *fsm,
                                  void (*drogue_cb)(void),
                                  void (*main_cb)(void))
{
    if (fsm == NULL) return;
    fsm->on_fire_drogue = drogue_cb;
    fsm->on_fire_main = main_cb;
}

HAL_StatusTypeDef FlightState_Arm(FlightStateMachine_t *fsm)
{
    if (fsm == NULL || !fsm->initialized) return HAL_ERROR;
    
    /* Can only arm from IDLE state */
    if (fsm->state != FLIGHT_STATE_IDLE) {
        return HAL_ERROR;
    }
    
    /* Reset flight data */
    memset(&fsm->data, 0, sizeof(FlightData_t));
    fsm->drogue_fired = false;
    fsm->main_fired = false;
    fsm->error_flag = false;
    fsm->error_code = FLIGHT_ERROR_NONE;
    
    transition_to(fsm, FLIGHT_STATE_ARMED);
    
    return HAL_OK;
}

void FlightState_Disarm(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return;
    
    /* Can only disarm from ARMED state (not during flight!) */
    if (fsm->state == FLIGHT_STATE_ARMED) {
        transition_to(fsm, FLIGHT_STATE_IDLE);
    }
}

bool FlightState_IsArmed(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return false;
    return (fsm->state >= FLIGHT_STATE_ARMED && fsm->state < FLIGHT_STATE_LANDED);
}

void FlightState_Update(FlightStateMachine_t *fsm, float altitude, 
                        float velocity, float acceleration)
{
    if (fsm == NULL || !fsm->initialized) return;
    
    /* Update current values */
    fsm->data.altitude = altitude;
    fsm->data.velocity = velocity;
    fsm->data.acceleration = acceleration;
    
    /* Update altitude history */
    fsm->data.altitude_history_idx = (fsm->data.altitude_history_idx + 1) % ALTITUDE_HISTORY_SIZE;
    fsm->data.altitude_history[fsm->data.altitude_history_idx] = altitude;
    
    /* Track maximum velocity */
    if (fabsf(velocity) > fsm->data.max_velocity) {
        fsm->data.max_velocity = fabsf(velocity);
    }
    
    fsm->last_update_tick = HAL_GetTick();
    
    /* State machine */
    switch (fsm->state) {
        case FLIGHT_STATE_BOOT:
            /* Should not be in BOOT after init */
            transition_to(fsm, FLIGHT_STATE_IDLE);
            break;
            
        case FLIGHT_STATE_IDLE:
            handle_idle(fsm, altitude, velocity, acceleration);
            break;
            
        case FLIGHT_STATE_ARMED:
            handle_armed(fsm, altitude, velocity, acceleration);
            break;
            
        case FLIGHT_STATE_BOOST:
            handle_boost(fsm, altitude, velocity, acceleration);
            break;
            
        case FLIGHT_STATE_COAST:
            handle_coast(fsm, altitude, velocity, acceleration);
            break;
            
        case FLIGHT_STATE_APOGEE:
            handle_apogee(fsm, altitude, velocity, acceleration);
            break;
            
        case FLIGHT_STATE_DESCENT:
            handle_descent(fsm, altitude, velocity, acceleration);
            break;
            
        case FLIGHT_STATE_MAIN:
            handle_main(fsm, altitude, velocity, acceleration);
            break;
            
        case FLIGHT_STATE_LANDED:
            handle_landed(fsm, altitude, velocity, acceleration);
            break;
            
        case FLIGHT_STATE_ERROR:
            handle_error(fsm, altitude, velocity, acceleration);
            break;
    }
}

FlightState_t FlightState_GetState(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return FLIGHT_STATE_ERROR;
    return fsm->state;
}

const char* FlightState_GetStateName(FlightState_t state)
{
    if (state > FLIGHT_STATE_ERROR) {
        return "UNKNOWN";
    }
    return state_names[state];
}

const FlightData_t* FlightState_GetData(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return NULL;
    return &fsm->data;
}

float FlightState_GetMaxAltitude(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return 0.0f;
    return fsm->data.max_altitude;
}

uint32_t FlightState_GetFlightTime(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return 0;
    
    if (fsm->data.launch_time == 0) {
        return 0;  /* Not launched yet */
    }
    
    if (fsm->data.landed_time != 0) {
        return fsm->data.landed_time - fsm->data.launch_time;
    }
    
    return HAL_GetTick() - fsm->data.launch_time;
}

bool FlightState_IsFlightComplete(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return false;
    return (fsm->state == FLIGHT_STATE_LANDED);
}

uint8_t FlightState_GetError(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return FLIGHT_ERROR_SENSOR_FAIL;
    return fsm->error_code;
}

void FlightState_ForceState(FlightStateMachine_t *fsm, FlightState_t state)
{
    if (fsm == NULL) return;
    transition_to(fsm, state);
}

void FlightState_Reset(FlightStateMachine_t *fsm)
{
    if (fsm == NULL) return;
    
    memset(&fsm->data, 0, sizeof(FlightData_t));
    fsm->drogue_fired = false;
    fsm->main_fired = false;
    fsm->error_flag = false;
    fsm->error_code = FLIGHT_ERROR_NONE;
    
    transition_to(fsm, FLIGHT_STATE_IDLE);
}
