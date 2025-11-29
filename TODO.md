# IREC-2026 - TODO

> **Last Updated:** November 29, 2025

---

## COMPLETED

### Sensor Drivers
- [x] MPU9250 IMU driver (I2C)
- [x] BNO055 IMU driver (I2C, sensor fusion)
- [x] BMP380 barometer driver (I2C)
- [x] MS5611 barometer driver (I2C)
- [x] NEO-7M GPS driver (UART, NMEA)
- [x] E32-433T30D LoRa driver (UART)

---

## HIGH PRIORITY

### Firmware Core
- [ ] Main flight state machine
- [ ] Sensor fusion (complementary/Kalman filter)
- [ ] Apogee detection algorithm
- [ ] Main deployment trigger (457m AGL)
- [ ] Pyro channel firing logic
- [ ] Data logging to flash

### Hardware
- [ ] Finalize PCB schematic (Altium)
- [ ] PCB layout (4-layer)
- [ ] Order PCBs from JLCPCB
- [ ] Component assembly

---

## MEDIUM PRIORITY

### Firmware Features
- [ ] W25Q flash memory driver
- [ ] Telemetry packet protocol
- [ ] Ground station communication
- [ ] Battery voltage monitoring
- [ ] Watchdog timer
- [ ] Error handling & recovery

### Testing
- [ ] Bench test each sensor
- [ ] Integration test (all sensors)
- [ ] Vibration test
- [ ] Altitude chamber test
- [ ] Ground station range test

---

## LOW PRIORITY

### Optimization
- [ ] DMA for sensor reads
- [ ] RTOS integration (FreeRTOS)
- [ ] Power optimization
- [ ] Code documentation

### Ground Station
- [ ] Real-time data display
- [ ] Flight data visualization
- [ ] Post-flight analysis tools

---

## Notes

- All drivers use STM32 HAL library
- I2C sensors on same bus (different addresses)
- UART1: GPS, UART2: LoRa
- Redundancy: 2 IMUs, 2 barometers
