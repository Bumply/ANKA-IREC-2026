# 🚀 ANKA Avionics - IREC 2026

<p align="center">
  <img src="https://img.shields.io/badge/Competition-IREC%202026-red?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Category-10K%20COTS-blue?style=for-the-badge" />
  <img src="https://img.shields.io/badge/MCU-STM32F429ZIT6-green?style=for-the-badge" />
</p>

---

## 📋 SRAD Flight Computer

| Parameter | Value |
|-----------|-------|
| **MCU** | STM32F429ZIT6 (ARM Cortex-M4F @ 180MHz) |
| **IMU 1** | MPU-9250 (Accel/Gyro/Mag) |
| **IMU 2** | BNO055 (9-DOF + Sensor Fusion) |
| **Baro 1** | BMP380 (0.016 Pa resolution) |
| **Baro 2** | MS5611 (±10 cm resolution) |
| **GPS** | NEO-7M (5 Hz, NMEA/UBX) |
| **Telemetry** | E32-433T30D LoRa (433 MHz, 30dBm) |
| **Storage** | W25Q40CLSNIG Flash (512 KB) |
| **Pyro Channels** | 2x IRFU120 MOSFET drivers |

---

## 🏗️ Project Structure

```
avionics/
├── firmware/
│   ├── Drivers/
│   │   ├── MPU9250/        # Primary IMU driver
│   │   ├── BNO055/         # Backup IMU + fusion
│   │   ├── BMP380/         # Primary barometer
│   │   ├── MS5611/         # Backup barometer
│   │   ├── NEO7M/          # GPS (NMEA parser)
│   │   └── E32_LoRa/       # LoRa telemetry
│   └── README.md
└── README.md
```

---

## 🎯 Sensor Redundancy

| Type | Primary | Backup | Purpose |
|------|---------|--------|---------|
| **IMU** | MPU9250 | BNO055 | Orientation, acceleration |
| **Barometer** | BMP380 | MS5611 | Altitude, apogee detection |
| **GPS** | NEO-7M | — | Position tracking |
| **Radio** | E32-433T30D | — | Live telemetry |

---

## 🔧 Driver Features

### MPU9250 (Primary IMU)
- 9-DOF (Accel ±16g, Gyro ±2000°/s, Mag)
- I2C interface, configurable sample rates
- Gyroscope calibration routine

### BNO055 (Backup IMU)
- Built-in sensor fusion (Cortex-M0)
- Euler angles, Quaternions output
- Linear acceleration (gravity removed)
- Calibration save/restore

### BMP380 (Primary Barometer)
- 0.016 Pa RMS noise
- IIR filtering, up to 200 Hz ODR
- Altitude calculation with sea-level calibration

### MS5611 (Backup Barometer)
- 24-bit ADC, ±10cm resolution
- Second-order temperature compensation
- Fast conversion (OSR selectable)

### NEO-7M (GPS)
- NMEA parsing (GGA, RMC, GSA, GSV)
- UBX protocol for configuration
- Haversine distance calculation

### E32-433T30D (LoRa)
- 433 MHz ISM band
- 30dBm TX power (8km range LOS)
- Transparent & fixed address modes

---

## 👥 Team

**Zenith Rocket Team** - Atılım University, Ankara, Turkey

---

## 📜 License

Educational project for IREC 2026 competition.
