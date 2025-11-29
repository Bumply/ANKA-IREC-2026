/**
 * @file main.c
 * @brief Main Application - IREC 2026 Flight Computer
 * @author Zenith Rocket Team
 * @date 2025
 * 
 * Flight Computer Main Application
 * 
 * This is the main entry point that:
 * 1. Initializes all peripherals and drivers
 * 2. Runs self-test to verify all sensors
 * 3. Main loop: read sensors → fuse data → update state → control pyros → transmit telemetry
 */

#include "main.h"
#include "flight_state.h"
#include "sensor_fusion.h"
#include "telemetry.h"
#include "data_logger.h"

/* Drivers */
#include "mpu9250.h"
#include "bno055.h"
#include "bmp380.h"
#include "ms5611.h"
#include "neo7m.h"
#include "e32_lora.h"
#include "w25q.h"
#include "pyro.h"

#include <string.h>
#include <stdio.h>

/*============================================================================
 * PERIPHERAL HANDLES
 *============================================================================*/

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim9;

/*============================================================================
 * DRIVER HANDLES
 *============================================================================*/

static MPU9250_Handle_t mpu9250;
static BNO055_Handle_t bno055;
static BMP380_Handle_t bmp380;
static MS5611_Handle_t ms5611;
static NEO7M_Handle_t gps;
static E32_Handle_t lora;
static W25Q_Handle_t flash;
static Pyro_Handle_t pyro;

/* Core modules */
static FlightStateMachine_t fsm;
static SensorFusion_t fusion;
static Telemetry_t telemetry;
static DataLogger_t logger;

/*============================================================================
 * SYSTEM STATE
 *============================================================================*/

static SystemState_t system_state = SYS_STATE_INIT;
static uint8_t status_flags = 0;
static float battery_voltage = 0.0f;

/* Timing */
static uint32_t last_sensor_time = 0;
static uint32_t last_gps_time = 0;
static uint32_t last_telemetry_time = 0;
static uint32_t last_log_time = 0;

/*============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM9_Init(void);

static bool InitializeSensors(void);
static bool SelfTest(void);
static void ReadSensors(void);
static void ProcessGPS(void);
static void UpdateStateMachine(void);
static void ControlPyros(void);
static void TransmitTelemetry(void);
static void LogData(void);
static void CheckArmSwitch(void);
static void UpdateStatusLED(void);
static void ReadBattery(void);

/* Callbacks */
static void OnStateChange(FlightState_t old_state, FlightState_t new_state);
static void OnFireDrogue(void);
static void OnFireMain(void);
static HAL_StatusTypeDef LoRaTransmit(uint8_t *data, uint16_t len);

/*============================================================================
 * MAIN
 *============================================================================*/

int main(void)
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick */
    HAL_Init();
    
    /* Configure the system clock */
    SystemClock_Config();
    
    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    MX_ADC1_Init();
    MX_TIM9_Init();
    
    /* Status LED on during init */
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_SET);
    
    /*========================================================================
     * INITIALIZE DRIVERS AND MODULES
     *========================================================================*/
    
    system_state = SYS_STATE_INIT;
    
    /* Initialize core modules */
    FlightState_Init(&fsm);
    SensorFusion_Init(&fusion);
    Telemetry_Init(&telemetry);
    
    /* Set callbacks */
    FlightState_SetStateCallback(&fsm, OnStateChange);
    FlightState_SetPyroCallbacks(&fsm, OnFireDrogue, OnFireMain);
    Telemetry_SetTransmitCallback(&telemetry, LoRaTransmit);
    
    /* Initialize sensors */
    if (!InitializeSensors()) {
        system_state = SYS_STATE_ERROR;
        /* Continue anyway - will operate in degraded mode */
    }
    
    /* Initialize flash and logger */
    if (W25Q_Init(&flash, &hspi1, FLASH_CS_PORT, FLASH_CS_PIN) == HAL_OK) {
        status_flags |= TLM_FLAG_FLASH_OK;
        DataLogger_Init(&logger, &flash);
    }
    
    /* Initialize pyro system */
    Pyro_Init(&pyro);
    
    /*========================================================================
     * SELF TEST
     *========================================================================*/
    
    system_state = SYS_STATE_SELF_TEST;
    
    if (SelfTest()) {
        system_state = SYS_STATE_IDLE;
    } else {
        system_state = SYS_STATE_ERROR;
    }
    
    /* Short beep to indicate ready */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    
    /* Send boot event */
    Telemetry_SendEvent(&telemetry, TLM_EVENT_BOOT, status_flags, 0);
    
    /*========================================================================
     * MAIN LOOP
     *========================================================================*/
    
    while (1)
    {
        uint32_t now = HAL_GetTick();
        
        /* High-rate sensor reading (100 Hz) */
        if (now - last_sensor_time >= (1000 / SENSOR_RATE_HZ)) {
            last_sensor_time = now;
            
            ReadSensors();
            UpdateStateMachine();
            ControlPyros();
        }
        
        /* GPS processing (1 Hz) */
        if (now - last_gps_time >= (1000 / GPS_RATE_HZ)) {
            last_gps_time = now;
            
            ProcessGPS();
            ReadBattery();
        }
        
        /* Telemetry transmission (10 Hz in flight, 1 Hz on ground) */
        uint32_t tlm_period = (system_state == SYS_STATE_FLIGHT) ? 
                              (1000 / TELEMETRY_RATE_HZ) : 1000;
        if (now - last_telemetry_time >= tlm_period) {
            last_telemetry_time = now;
            
            TransmitTelemetry();
        }
        
        /* Data logging (50 Hz during flight) */
        if (system_state == SYS_STATE_FLIGHT) {
            if (now - last_log_time >= (1000 / LOG_RATE_HZ)) {
                last_log_time = now;
                
                LogData();
            }
        }
        
        /* Check arm switch and update LED */
        CheckArmSwitch();
        UpdateStatusLED();
        
        /* Process incoming telemetry commands */
        if (Telemetry_IsCommandPending(&telemetry)) {
            TLM_Command_Payload_t cmd;
            if (Telemetry_GetCommand(&telemetry, &cmd) == HAL_OK) {
                /* Handle command */
                switch (cmd.command) {
                    case TLM_CMD_ARM:
                        if (system_state == SYS_STATE_IDLE) {
                            FlightState_Arm(&fsm);
                        }
                        break;
                    case TLM_CMD_DISARM:
                        FlightState_Disarm(&fsm);
                        break;
                    case TLM_CMD_BUZZER:
                        /* Beep for locating */
                        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
                        HAL_Delay(500);
                        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

/*============================================================================
 * SENSOR INITIALIZATION
 *============================================================================*/

static bool InitializeSensors(void)
{
    bool all_ok = true;
    
    /* MPU9250 (Primary IMU) */
    if (MPU9250_Init(&mpu9250, &hi2c1, MPU9250_ACCEL_FS_16G, MPU9250_GYRO_FS_2000DPS) == HAL_OK) {
        status_flags |= TLM_FLAG_IMU1_OK;
    } else {
        all_ok = false;
    }
    
    /* BNO055 (Backup IMU with fusion) */
    if (BNO055_Init(&bno055, &hi2c2, BNO055_DEFAULT_ADDR) == HAL_OK) {
        BNO055_SetMode(&bno055, BNO055_MODE_NDOF);
        status_flags |= TLM_FLAG_IMU2_OK;
    } else {
        all_ok = false;
    }
    
    /* BMP380 (Primary Barometer) */
    if (BMP380_Init(&bmp380, &hi2c1, BMP380_DEFAULT_ADDR) == HAL_OK) {
        BMP380_Configure(&bmp380, BMP380_OVERSAMPLING_2X, BMP380_OVERSAMPLING_8X,
                         BMP380_ODR_50_HZ, BMP380_IIR_COEF_3);
        BMP380_SetMode(&bmp380, BMP380_MODE_NORMAL);
        status_flags |= TLM_FLAG_BARO1_OK;
    } else {
        all_ok = false;
    }
    
    /* MS5611 (Backup Barometer) - Using I2C1 */
    if (MS5611_Init(&ms5611, &hi2c1, MS5611_OSR_4096) == HAL_OK) {
        status_flags |= TLM_FLAG_BARO2_OK;
    } else {
        all_ok = false;
    }
    
    /* NEO-7M GPS */
    if (NEO7M_Init(&gps, &huart2) == HAL_OK) {
        NEO7M_SetUpdateRate(&gps, 5);  /* 5 Hz */
        status_flags |= TLM_FLAG_GPS_OK;
    } else {
        /* GPS failure is not critical */
    }
    
    /* E32 LoRa */
    E32_Init(&lora, &huart3, LORA_M0_PORT, LORA_M0_PIN,
             LORA_M1_PORT, LORA_M1_PIN, LORA_AUX_PORT, LORA_AUX_PIN);
    
    return all_ok;
}

/*============================================================================
 * SELF TEST
 *============================================================================*/

static bool SelfTest(void)
{
    bool passed = true;
    
    /* Check at least one IMU works */
    if (!(status_flags & (TLM_FLAG_IMU1_OK | TLM_FLAG_IMU2_OK))) {
        passed = false;
    }
    
    /* Check at least one barometer works */
    if (!(status_flags & (TLM_FLAG_BARO1_OK | TLM_FLAG_BARO2_OK))) {
        passed = false;
    }
    
    /* Check pyro continuity (just read, don't require it) */
    Pyro_CheckContinuity(&pyro, 0);
    Pyro_CheckContinuity(&pyro, 1);
    
    /* Read ground reference */
    if (status_flags & TLM_FLAG_BARO1_OK) {
        BMP380_ReadData(&bmp380);
        float ground_pressure = bmp380.pressure;
        float ground_altitude = BMP380_GetAltitude(&bmp380);
        
        SensorFusion_SetGroundReference(&fusion, ground_pressure, ground_altitude);
        FlightState_SetGroundReference(&fsm, ground_altitude, ground_pressure);
        DataLogger_SetGroundReference(&logger, (int32_t)(ground_altitude * 100), 
                                      (uint32_t)ground_pressure);
    }
    
    return passed;
}

/*============================================================================
 * SENSOR READING
 *============================================================================*/

static void ReadSensors(void)
{
    IMU_Reading_t imu_reading;
    Baro_Reading_t baro_reading;
    
    /* Read MPU9250 */
    if (status_flags & TLM_FLAG_IMU1_OK) {
        if (MPU9250_ReadAccel(&mpu9250) == HAL_OK &&
            MPU9250_ReadGyro(&mpu9250) == HAL_OK) {
            imu_reading.accel_x = mpu9250.accel.x;
            imu_reading.accel_y = mpu9250.accel.y;
            imu_reading.accel_z = mpu9250.accel.z;
            imu_reading.gyro_x = mpu9250.gyro.x;
            imu_reading.gyro_y = mpu9250.gyro.y;
            imu_reading.gyro_z = mpu9250.gyro.z;
            imu_reading.valid = true;
            imu_reading.timestamp = HAL_GetTick();
            
            SensorFusion_UpdateIMU(&fusion, SENSOR_PRIMARY, &imu_reading);
        }
    }
    
    /* Read BNO055 */
    if (status_flags & TLM_FLAG_IMU2_OK) {
        BNO055_Vector3_t lin_accel, gyro_vec;
        BNO055_Euler_t euler;
        BNO055_Quaternion_t quat;
        
        if (BNO055_ReadLinearAccel(&bno055, &lin_accel) == HAL_OK &&
            BNO055_ReadGyro(&bno055, &gyro_vec) == HAL_OK) {
            imu_reading.accel_x = lin_accel.x;
            imu_reading.accel_y = lin_accel.y;
            imu_reading.accel_z = lin_accel.z;
            imu_reading.gyro_x = gyro_vec.x;
            imu_reading.gyro_y = gyro_vec.y;
            imu_reading.gyro_z = gyro_vec.z;
            imu_reading.valid = true;
            imu_reading.timestamp = HAL_GetTick();
            
            SensorFusion_UpdateIMU(&fusion, SENSOR_BACKUP, &imu_reading);
            
            /* Get orientation from BNO055 */
            if (BNO055_ReadEuler(&bno055, &euler) == HAL_OK &&
                BNO055_ReadQuaternion(&bno055, &quat) == HAL_OK) {
                SensorFusion_UpdateOrientation(&fusion, 
                    euler.roll, euler.pitch, euler.heading,
                    quat.w, quat.x, quat.y, quat.z);
            }
        }
    }
    
    /* Read BMP380 */
    if (status_flags & TLM_FLAG_BARO1_OK) {
        if (BMP380_ReadData(&bmp380) == HAL_OK) {
            baro_reading.pressure = bmp380.pressure;
            baro_reading.temperature = bmp380.temperature;
            baro_reading.altitude = BMP380_GetAltitude(&bmp380);
            baro_reading.valid = true;
            baro_reading.timestamp = HAL_GetTick();
            
            SensorFusion_UpdateBaro(&fusion, SENSOR_PRIMARY, &baro_reading);
        }
    }
    
    /* Read MS5611 */
    if (status_flags & TLM_FLAG_BARO2_OK) {
        if (MS5611_ReadTemperatureAndPressure(&ms5611) == HAL_OK) {
            baro_reading.pressure = ms5611.pressure_mbar * 100.0f;  /* Convert mbar to Pa */
            baro_reading.temperature = ms5611.temperature_c;
            baro_reading.altitude = ms5611.altitude_m;
            baro_reading.valid = true;
            baro_reading.timestamp = HAL_GetTick();
            
            SensorFusion_UpdateBaro(&fusion, SENSOR_BACKUP, &baro_reading);
        }
    }
    
    /* Run sensor fusion */
    SensorFusion_Process(&fusion, 1.0f / SENSOR_RATE_HZ);
}

/*============================================================================
 * GPS PROCESSING
 *============================================================================*/

static void ProcessGPS(void)
{
    if (!(status_flags & TLM_FLAG_GPS_OK)) return;
    
    /* Process any incoming GPS data */
    NEO7M_ParseBuffer(&gps);
}

/*============================================================================
 * STATE MACHINE UPDATE
 *============================================================================*/

static void UpdateStateMachine(void)
{
    const Fused_Data_t *data = SensorFusion_GetData(&fusion);
    
    /* Update flight state machine */
    FlightState_Update(&fsm, data->altitude, data->velocity, data->accel_vertical);
    
    /* Update system state based on flight state */
    FlightState_t flight_state = FlightState_GetState(&fsm);
    
    switch (flight_state) {
        case FLIGHT_STATE_IDLE:
            system_state = SYS_STATE_IDLE;
            break;
        case FLIGHT_STATE_ARMED:
            system_state = SYS_STATE_ARMED;
            break;
        case FLIGHT_STATE_BOOST:
        case FLIGHT_STATE_COAST:
        case FLIGHT_STATE_APOGEE:
        case FLIGHT_STATE_DESCENT:
        case FLIGHT_STATE_MAIN:
            system_state = SYS_STATE_FLIGHT;
            break;
        case FLIGHT_STATE_LANDED:
            system_state = SYS_STATE_RECOVERY;
            break;
        case FLIGHT_STATE_ERROR:
            system_state = SYS_STATE_ERROR;
            break;
        default:
            break;
    }
    
    /* Update logger stats */
    DataLogger_UpdateStats(&logger, 
        (int32_t)(data->altitude * 100),
        (int16_t)(data->velocity * 100),
        (int16_t)(data->accel_vertical * 100));
}

/*============================================================================
 * PYRO CONTROL
 *============================================================================*/

static void ControlPyros(void)
{
    /* Update pyro system (handles timing, safety checks) */
    Pyro_Update(&pyro);
}

/*============================================================================
 * TELEMETRY
 *============================================================================*/

static void TransmitTelemetry(void)
{
    const Fused_Data_t *data = SensorFusion_GetData(&fusion);
    const FlightData_t *flight_data = FlightState_GetData(&fsm);
    
    /* Build status packet */
    TLM_Status_Payload_t status;
    status.timestamp = HAL_GetTick();
    status.state = (uint8_t)FlightState_GetState(&fsm);
    status.flags = status_flags;
    if (FlightState_IsArmed(&fsm)) status.flags |= TLM_FLAG_ARMED;
    if (DataLogger_IsLogging(&logger)) status.flags |= TLM_FLAG_LOGGING;
    
    status.altitude = (int16_t)data->altitude;
    status.velocity = (int16_t)(data->velocity * 10);
    status.accel = (int16_t)(data->accel_vertical * 10);
    status.max_alt = (int16_t)flight_data->max_altitude;
    status.battery = (uint8_t)(battery_voltage * 10);
    
    /* Pyro status */
    status.pyro_status = 0;
    if (pyro.channels[0].continuity) status.pyro_status |= TLM_PYRO1_CONT;
    if (pyro.channels[1].continuity) status.pyro_status |= TLM_PYRO2_CONT;
    if (pyro.channels[0].state == PYRO_STATE_ARMED) status.pyro_status |= TLM_PYRO1_ARMED;
    if (pyro.channels[1].state == PYRO_STATE_ARMED) status.pyro_status |= TLM_PYRO2_ARMED;
    if (pyro.channels[0].state == PYRO_STATE_FIRED) status.pyro_status |= TLM_PYRO1_FIRED;
    if (pyro.channels[1].state == PYRO_STATE_FIRED) status.pyro_status |= TLM_PYRO2_FIRED;
    
    status.temperature = (int8_t)bmp380.temperature;
    status.rssi = 0;  /* Would be set from LoRa RSSI */
    
    Telemetry_SendStatus(&telemetry, &status);
    
    /* Send GPS packet if we have a fix */
    if (gps.fix_valid) {
        TLM_GPS_Payload_t gps_tlm;
        gps_tlm.timestamp = HAL_GetTick();
        gps_tlm.latitude = (int32_t)(gps.latitude * 1e7);
        gps_tlm.longitude = (int32_t)(gps.longitude * 1e7);
        gps_tlm.altitude_msl = (int32_t)(gps.altitude * 1000);
        gps_tlm.ground_speed = (uint16_t)(gps.speed * 100);
        gps_tlm.heading = (int16_t)(gps.course * 100);
        gps_tlm.satellites = gps.satellites;
        gps_tlm.fix_type = gps.fix_valid ? 3 : 0;
        gps_tlm.hdop = (uint8_t)(gps.hdop * 10);
        gps_tlm.reserved = 0;
        
        Telemetry_SendGPS(&telemetry, &gps_tlm);
    }
}

/*============================================================================
 * DATA LOGGING
 *============================================================================*/

static void LogData(void)
{
    if (!DataLogger_IsLogging(&logger)) return;
    
    const Fused_Data_t *data = SensorFusion_GetData(&fusion);
    
    DataLogger_LogSensor(&logger,
        (int32_t)(data->altitude * 100),
        (int16_t)(data->velocity * 100),
        (int16_t)(data->accel_vertical * 100),
        (int16_t)(data->roll * 10),
        (int16_t)(data->pitch * 10),
        (int16_t)(data->yaw * 10),
        (int16_t)(mpu9250.accel_x * 1000 / 9.81f),
        (int16_t)(mpu9250.accel_y * 1000 / 9.81f),
        (int16_t)(mpu9250.accel_z * 1000 / 9.81f),
        (uint8_t)FlightState_GetState(&fsm),
        status_flags);
}

/*============================================================================
 * ARM SWITCH CHECK
 *============================================================================*/

static void CheckArmSwitch(void)
{
    static bool last_switch_state = false;
    
    /* Read arm switch (active low) */
    bool switch_active = (HAL_GPIO_ReadPin(ARM_SWITCH_PORT, ARM_SWITCH_PIN) == GPIO_PIN_RESET);
    
    /* Detect rising edge (switch just activated) */
    if (switch_active && !last_switch_state) {
        if (system_state == SYS_STATE_IDLE) {
            /* Arm the system */
            if (FlightState_Arm(&fsm) == HAL_OK) {
                Pyro_Arm(&pyro);
                
                /* Prepare logger */
                DataLogger_Erase(&logger);
                
                /* Double beep to confirm armed */
                HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
                HAL_Delay(100);
                HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
                HAL_Delay(100);
                HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
                HAL_Delay(100);
                HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
            }
        }
    }
    /* Detect falling edge (switch just deactivated) */
    else if (!switch_active && last_switch_state) {
        if (system_state == SYS_STATE_ARMED) {
            /* Disarm the system */
            FlightState_Disarm(&fsm);
            Pyro_Disarm(&pyro);
            
            /* Single long beep to confirm disarmed */
            HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
            HAL_Delay(500);
            HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        }
    }
    
    last_switch_state = switch_active;
}

/*============================================================================
 * STATUS LED
 *============================================================================*/

static void UpdateStatusLED(void)
{
    static uint32_t last_toggle = 0;
    uint32_t now = HAL_GetTick();
    
    uint32_t blink_period;
    
    switch (system_state) {
        case SYS_STATE_ERROR:
            blink_period = 100;  /* Fast blink = error */
            break;
        case SYS_STATE_ARMED:
            blink_period = 500;  /* Medium blink = armed */
            break;
        case SYS_STATE_FLIGHT:
            blink_period = 0;    /* Solid on = flight */
            HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_SET);
            return;
        case SYS_STATE_RECOVERY:
            blink_period = 1000; /* Slow blink = recovery */
            break;
        default:
            blink_period = 2000; /* Very slow = idle */
            break;
    }
    
    if (now - last_toggle >= blink_period / 2) {
        last_toggle = now;
        HAL_GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
    }
}

/*============================================================================
 * BATTERY READING
 *============================================================================*/

static void ReadBattery(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        uint32_t adc_value = HAL_ADC_GetValue(&hadc1);
        /* Assuming voltage divider: 3.3V reference, 12-bit ADC, 2:1 divider */
        battery_voltage = (adc_value / 4095.0f) * 3.3f * 2.0f;
    }
    HAL_ADC_Stop(&hadc1);
}

/*============================================================================
 * CALLBACKS
 *============================================================================*/

static void OnStateChange(FlightState_t old_state, FlightState_t new_state)
{
    const Fused_Data_t *data = SensorFusion_GetData(&fusion);
    int16_t alt = (int16_t)data->altitude;
    
    /* Map flight state to telemetry event */
    uint8_t event = 0;
    switch (new_state) {
        case FLIGHT_STATE_ARMED:
            event = TLM_EVENT_ARMED;
            break;
        case FLIGHT_STATE_BOOST:
            event = TLM_EVENT_LAUNCH;
            DataLogger_Start(&logger);
            break;
        case FLIGHT_STATE_COAST:
            event = TLM_EVENT_BURNOUT;
            break;
        case FLIGHT_STATE_APOGEE:
            event = TLM_EVENT_APOGEE;
            break;
        case FLIGHT_STATE_DESCENT:
            event = TLM_EVENT_DROGUE;
            break;
        case FLIGHT_STATE_MAIN:
            event = TLM_EVENT_MAIN;
            break;
        case FLIGHT_STATE_LANDED:
            event = TLM_EVENT_LANDED;
            DataLogger_Stop(&logger);
            /* Start recovery beeping */
            break;
        case FLIGHT_STATE_ERROR:
            event = TLM_EVENT_ERROR;
            break;
        default:
            return;
    }
    
    /* Send event telemetry */
    Telemetry_SendEvent(&telemetry, event, (uint8_t)old_state, alt);
    
    /* Log event */
    DataLogger_LogEvent(&logger, event, (uint8_t)old_state, alt);
}

static void OnFireDrogue(void)
{
    Pyro_Fire(&pyro, 0);  /* Channel 0 = drogue */
}

static void OnFireMain(void)
{
    Pyro_Fire(&pyro, 1);  /* Channel 1 = main */
}

static HAL_StatusTypeDef LoRaTransmit(uint8_t *data, uint16_t len)
{
    return (E32_Send(&lora, data, (uint8_t)len) == E32_OK) ? HAL_OK : HAL_ERROR;
}

/*============================================================================
 * PERIPHERAL INITIALIZATION (STM32CubeMX Generated - Modify for your board)
 *============================================================================*/

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    /* Configure Pyro Fire pins as outputs (start LOW) */
    HAL_GPIO_WritePin(PYRO1_FIRE_PORT, PYRO1_FIRE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PYRO2_FIRE_PORT, PYRO2_FIRE_PIN, GPIO_PIN_RESET);
    
    GPIO_InitStruct.Pin = PYRO1_FIRE_PIN | PYRO2_FIRE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /* Configure Pyro continuity pins as inputs */
    GPIO_InitStruct.Pin = PYRO1_CONT_PIN | PYRO2_CONT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /* Configure Arm switch as input with pull-up */
    GPIO_InitStruct.Pin = ARM_SWITCH_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ARM_SWITCH_PORT, &GPIO_InitStruct);
    
    /* Configure Buzzer as output */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = BUZZER_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);
    
    /* Configure Status LED as output */
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = LED_STATUS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_STATUS_PORT, &GPIO_InitStruct);
    
    /* Configure SPI CS pins as outputs (start HIGH) */
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MS5611_CS_PORT, MS5611_CS_PIN, GPIO_PIN_SET);
    
    GPIO_InitStruct.Pin = FLASH_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FLASH_CS_PORT, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = MS5611_CS_PIN;
    HAL_GPIO_Init(MS5611_CS_PORT, &GPIO_InitStruct);
    
    /* Configure LoRa control pins */
    HAL_GPIO_WritePin(LORA_M0_PORT, LORA_M0_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LORA_M1_PORT, LORA_M1_PIN, GPIO_PIN_RESET);
    
    GPIO_InitStruct.Pin = LORA_M0_PIN | LORA_M1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = LORA_AUX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(LORA_AUX_PORT, &GPIO_InitStruct);
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

static void MX_I2C2_Init(void)
{
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = 400000;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c2);
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi1);
}

static void MX_SPI2_Init(void)
{
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi2);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 9600;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);
    
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void MX_TIM9_Init(void)
{
    htim9.Instance = TIM9;
    htim9.Init.Prescaler = 179;
    htim9.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim9.Init.Period = 999;
    htim9.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim9);
}

/*============================================================================
 * SYSTEM CLOCK CONFIGURATION
 *============================================================================*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    
    /* Initializes the RCC Oscillators (HSE for 180 MHz) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;   /* Assuming 8 MHz crystal */
    RCC_OscInitStruct.PLL.PLLN = 360;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    /* Activate Over-Drive mode */
    HAL_PWREx_EnableOverDrive();
    
    /* Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

/*============================================================================
 * ERROR HANDLER
 *============================================================================*/

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
        for (volatile int i = 0; i < 500000; i++);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* User can add implementation to report the file name and line number */
    (void)file;
    (void)line;
}
#endif
