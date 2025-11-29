# Avionics Subsystem - ANKA IREC 2026

## Overview

Triple-redundant avionics architecture for maximum flight safety and data integrity.

```
┌─────────────────────────────────────────────────────────────────┐
│                    AVIONICS ARCHITECTURE                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌───────────────┐   ┌───────────────┐   ┌───────────────┐    │
│   │  RRC3 Sport   │   │   EasyMini    │   │  SRAD Flight  │    │
│   │  (PRIMARY)    │   │   (BACKUP)    │   │   Computer    │    │
│   │               │   │               │   │   (TERTIARY)  │    │
│   │  ┌─────────┐  │   │  ┌─────────┐  │   │  ┌─────────┐  │    │
│   │  │Altimeter│  │   │  │Altimeter│  │   │  │ MS5611  │  │    │
│   │  └────┬────┘  │   │  └────┬────┘  │   │  └────┬────┘  │    │
│   │       │       │   │       │       │   │       │       │    │
│   │  ┌────▼────┐  │   │  ┌────▼────┐  │   │  ┌────▼────┐  │    │
│   │  │  Pyro   │  │   │  │  Pyro   │  │   │  │  Pyro   │  │    │
│   │  │Channels │  │   │  │Channels │  │   │  │Channels │  │    │
│   │  └────┬────┘  │   │  └────┬────┘  │   │  └────┬────┘  │    │
│   └───────┼───────┘   └───────┼───────┘   └───────┼───────┘    │
│           │                   │                   │            │
│           └───────────────────┼───────────────────┘            │
│                               │                                │
│                     ┌─────────▼─────────┐                      │
│                     │   HOT GAS GENS    │                      │
│                     │  (4x Redundant)   │                      │
│                     └───────────────────┘                      │
│                                                                 │
│   ┌───────────────────────────────────────────────────────┐    │
│   │                    GPS & TELEMETRY                     │    │
│   │  ┌─────────────────┐     ┌─────────────────────────┐  │    │
│   │  │ Featherweight   │     │  LoRa E32-433T30D       │  │    │
│   │  │ GPS (915 MHz)   │     │  Telemetry (433 MHz)    │  │    │
│   │  │ No HAM License  │     │  + NEO-7M GPS           │  │    │
│   │  └─────────────────┘     └─────────────────────────┘  │    │
│   └───────────────────────────────────────────────────────┘    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Components

### Primary: RRC3 Sport Altimeter (COTS)
- **Manufacturer:** Missile Works
- **MCU:** 16MHz 16-bit MSP430
- **Memory:** 8Mbit SST flash
- **Sensor:** MSI NS5697 pressure (24-bit ADC)
- **Mode:** Dual deployment
- **Events:**
  - Drogue: Apogee
  - Main: 1,500 ft AGL (457m)

### Secondary: EasyMini Altimeter (COTS)
- **Manufacturer:** Altus Metrum
- **MCU:** ARM Cortex M0 (NXP LPC11U24)
- **Memory:** 1MB SPI flash
- **Sensor:** MS5607
- **Mode:** Dual deployment (backup)

### Tertiary: SRAD Flight Computer
- **MCU:** STM32F429ZIT6 (ARM Cortex-M4F @ 180MHz)
- **Altimeter:** MS5611 GY-63 (±10cm, 100Hz)
- **IMU 1:** MPU-9250 9-DOF (±16g, 1kHz)
- **IMU 2:** BNO080/085 (400Hz quaternion output)
- **Storage:** W25Q40CLSNIG Flash (512KB)
- **Pyro:** 2x IRFU120 MOSFET drivers
- **PCB:** 4-layer, designed in Altium Designer
- **Manufacturing:** JLCPCB

### GPS & Telemetry
| Component | Frequency | License | Purpose |
|-----------|-----------|---------|---------|
| Featherweight GPS | 915 MHz | None | Primary tracking |
| LoRa E32-433T30D | 433 MHz | None (ISM) | Telemetry TX |
| NEO-7M GPS | - | - | Backup position |
| Quectel L86-M33 | - | - | Multi-constellation backup |

## Power Architecture

```
┌──────────────────────────────────────────────────────┐
│                   POWER DISTRIBUTION                  │
├──────────────────────────────────────────────────────┤
│                                                      │
│  ┌────────────┐    ┌────────────┐    ┌────────────┐ │
│  │ 2x Li-Ion  │    │ 2x Li-Ion  │    │ 2x Li-Ion  │ │
│  │  (Series)  │    │  (Series)  │    │  (Series)  │ │
│  └─────┬──────┘    └─────┬──────┘    └─────┬──────┘ │
│        │                 │                 │        │
│  ┌─────▼──────┐    ┌─────▼──────┐    ┌─────▼──────┐ │
│  │  Schurter  │    │  Schurter  │    │  Schurter  │ │
│  │   Switch   │    │   Switch   │    │   Switch   │ │
│  └─────┬──────┘    └─────┬──────┘    └─────┬──────┘ │
│        │                 │                 │        │
│  ┌─────▼──────┐    ┌─────▼──────┐    ┌─────▼──────┐ │
│  │    RRC3    │    │  EasyMini  │    │    SRAD    │ │
│  └────────────┘    └────────────┘    └────────────┘ │
│                                                      │
│  ┌────────────┐                                      │
│  │ 3S LiPo    │ ──► GPS/Telemetry System            │
│  │ 11.1V 2200 │    (>2 hours endurance)             │
│  │    mAh     │                                      │
│  └────────────┘                                      │
│                                                      │
└──────────────────────────────────────────────────────┘
```

## Deployment Sequence

| Event | Altitude | Trigger | Primary | Backup | Tertiary |
|-------|----------|---------|---------|--------|----------|
| Drogue Deploy | Apogee | Barometric | RRC3 | EasyMini | SRAD |
| Main Deploy | 457m (1,500ft) | Barometric | RRC3 | EasyMini | SRAD |

## File Structure

```
avionics/
├── firmware/               # STM32 flight computer code
│   ├── Inc/                # Header files
│   ├── Src/                # Source files  
│   ├── Drivers/            # Peripheral drivers
│   └── Startup/            # STM32 startup files
└── README.md
```

## Testing Checklist

- [ ] Individual component bench test
- [ ] Full system integration test
- [ ] Battery drain test (must last >2 hours)
- [ ] Telemetry range test
- [ ] Pyro continuity verification
- [ ] Altitude simulation test
- [ ] Vibration test
- [ ] Temperature cycling test

