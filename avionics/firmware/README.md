# Flight Computer Firmware

STM32F429ZIT6 Flight Computer for IREC 2026

## Project Structure

```
firmware/
├── Inc/                    # Header files
│   ├── main.h              # Pin definitions, peripheral config
│   ├── flight_state.h      # Flight state machine
│   ├── sensor_fusion.h     # Kalman filter sensor fusion
│   ├── telemetry.h         # LoRa telemetry protocol
│   └── data_logger.h       # Flash data logging
│
├── Src/                    # Source files
│   ├── main.c              # Main application loop
│   ├── flight_state.c      # State machine implementation
│   ├── sensor_fusion.c     # Dual-sensor fusion with failover
│   ├── telemetry.c         # Packet TX/RX with CRC-16
│   └── data_logger.c       # 50Hz flight data logging
│
├── Drivers/                # Peripheral drivers
│   ├── MPU9250/            # Primary IMU (I2C)
│   ├── BNO055/             # Backup IMU with onboard fusion (I2C)
│   ├── BMP380/             # Primary barometer (I2C)
│   ├── MS5611/             # Backup barometer (SPI)
│   ├── NEO7M/              # GPS with NMEA parsing (UART)
│   ├── E32_LoRa/           # 433MHz LoRa telemetry (UART)
│   ├── W25Q/               # 512KB flash memory (SPI)
│   └── Pyro/               # 2-channel pyrotechnic control
│
└── Startup/                # STM32 startup files (from CubeMX)
```

## Hardware

| Component | Model | Interface |
|-----------|-------|-----------|
| MCU | STM32F429ZIT6 | ARM Cortex-M4F @ 180MHz |
| IMU 1 | MPU-9250 | I2C1 |
| IMU 2 | BNO055 | I2C2 |
| Baro 1 | BMP380 | I2C1 |
| Baro 2 | MS5611 | SPI2 |
| GPS | NEO-7M | UART2 |
| LoRa | E32-433T30D | UART3 |
| Flash | W25Q40 | SPI1 |
| Pyro | 2x IRFU120 MOSFET | GPIO |

## Flight States

```
IDLE → ARMED → BOOST → COAST → APOGEE → DESCENT → MAIN → LANDED
         │                       │          │
      Arm Switch              Drogue    Main @457m AGL
```

## Building

1. Generate STM32 HAL project with STM32CubeMX for STM32F429ZIT6
2. Copy `Inc/`, `Src/`, and `Drivers/` into generated project
3. Add driver include paths to your IDE
4. Build with ARM GCC, Keil, or IAR
