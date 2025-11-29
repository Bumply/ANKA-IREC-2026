---
layout: default
title: Home
nav_order: 1
---

# IREC 2026 Flight Computer

**STM32F429ZIT6 Avionics System** | ARM Cortex-M4F @ 180MHz | Dual-Redundant Sensors

---

## System Specifications

| Parameter | Value |
|-----------|-------|
| **MCU** | STM32F429ZIT6 (ARM Cortex-M4F) |
| **Clock** | 180 MHz |
| **Flash** | 2 MB internal + 512 KB external |
| **RAM** | 256 KB SRAM |
| **Sensors** | Dual IMU, Dual Barometer, GPS |
| **Telemetry** | 433 MHz LoRa, 10 Hz update |
| **Target Altitude** | 10,000 ft AGL |

---

## Documentation

| Section | Description |
|---------|-------------|
| [System Overview](system-overview) | Architecture, block diagrams, hardware specs |
| [Flight State Machine](flight-state-machine) | State transitions, detection logic |
| [Sensor Fusion](sensor-fusion) | Kalman filter, data processing |
| [Hardware Drivers](drivers) | Sensor and peripheral APIs |
| [Telemetry Protocol](telemetry) | Packet format, ground station communication |
| [Data Logging](data-logging) | Flash memory layout, record formats |

---

## Architecture

```mermaid
flowchart TB
    subgraph SENS["SENSORS"]
        IMU1["MPU9250<br/>IMU #1"]
        IMU2["BNO055<br/>IMU #2"]
        BARO1["BMP380<br/>Baro #1"]
        BARO2["MS5611<br/>Baro #2"]
        GPS["NEO-7M<br/>GPS"]
    end

    subgraph MCU["STM32F429ZIT6"]
        FUSION["Sensor Fusion"]
        FSM["State Machine"]
        LOG["Data Logger"]
        TELEM["Telemetry"]
    end

    subgraph OUT["OUTPUTS"]
        FLASH["W25Q40<br/>Flash"]
        RADIO["E32-433T<br/>LoRa"]
        PYRO["Pyrotechnics"]
    end

    SENS --> FUSION
    FUSION --> FSM
    FUSION --> LOG
    FUSION --> TELEM
    FSM --> PYRO
    LOG --> FLASH
    TELEM --> RADIO
```

---

## Flight Profile

```mermaid
stateDiagram-v2
    direction LR
    [*] --> PAD
    PAD --> BOOST: Accel > 2.5g
    BOOST --> COAST: Burnout
    COAST --> APOGEE: Vel = 0
    APOGEE --> DROGUE: Deploy
    DROGUE --> MAIN: Alt < 457m
    MAIN --> LANDED: Vel = 0
    LANDED --> [*]
```

---

## Quick Start

```bash
# Clone repository
git clone https://github.com/Bumply/IREC-2026.git

# Open in STM32CubeIDE
# Build and flash to target
```

> **Note:** Full build instructions in [System Overview](system-overview)
