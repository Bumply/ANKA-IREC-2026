---
layout: default
title: Power System
nav_order: 8
---

# Power System

Complete battery and power distribution system for the IREC 2026 flight computer.

---

## System Overview

```
                                    IREC 2026 POWER SYSTEM
    ┌─────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                 │
    │  ┌─────────────────────────────────────────────────────────────────────────┐   │
    │  │                         BATTERY PACK                                     │   │
    │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐                        │   │
    │  │  │ CELL 1  │─│ CELL 2  │─│ CELL 3  │─│ CELL 4  │  LiPo 4S 14.8V        │   │
    │  │  │  3.7V   │ │  3.7V   │ │  3.7V   │ │  3.7V   │  650mAh 75C           │   │
    │  │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘                        │   │
    │  │       │           │           │           │                             │   │
    │  │       └─────┬─────┴─────┬─────┴─────┬─────┘                             │   │
    │  │             │           │           │                                   │   │
    │  │        ┌────┴────┐ ┌────┴────┐ ┌────┴────┐   BALANCE CONNECTOR         │   │
    │  │        │  BAL 1  │ │  BAL 2  │ │  BAL 3  │   (JST-XH 5-pin)            │   │
    │  │        └─────────┘ └─────────┘ └─────────┘                              │   │
    │  └─────────────────────────────────┬───────────────────────────────────────┘   │
    │                                    │                                           │
    │                                    │ XT30 CONNECTOR                            │
    │                                    │ (Main Power)                              │
    │                                    ▼                                           │
    │  ┌─────────────────────────────────────────────────────────────────────────┐   │
    │  │                      POWER INPUT PROTECTION                              │   │
    │  │                                                                          │   │
    │  │   VBAT ──┬──►│├──┬──[  10Ω  ]──┬──────────────────────► VBAT_PROT       │   │
    │  │          │      │              │                         (Protected)     │   │
    │  │          │   D1 Schottky       │                                        │   │
    │  │          │   (Reverse Prot)    │                                        │   │
    │  │          │                 ┌───┴───┐                                    │   │
    │  │          │                 │  100µF │  Bulk Capacitor                   │   │
    │  │          │                 │  25V   │  (Absorbs transients)             │   │
    │  │          │                 └───┬───┘                                    │   │
    │  │          │                     │                                        │   │
    │  │         GND ◄──────────────────┘                                        │   │
    │  │                                                                          │   │
    │  └─────────────────────────────────┬───────────────────────────────────────┘   │
    │                                    │                                           │
    │                                    │ VBAT_PROT (14.8V nominal)                 │
    │                                    │                                           │
    │               ┌────────────────────┼────────────────────┐                      │
    │               │                    │                    │                      │
    │               ▼                    ▼                    ▼                      │
    │  ┌────────────────────┐  ┌─────────────────┐  ┌─────────────────┐             │
    │  │   MAIN ARM SWITCH  │  │ VOLTAGE MONITOR │  │   PYRO POWER    │             │
    │  │                    │  │                 │  │                 │             │
    │  │  ┌──┐              │  │  VBAT_PROT      │  │  VBAT_PROT      │             │
    │  │  │SW│◄── PE5       │  │      │          │  │      │          │             │
    │  │  └┬─┘   (GPIO)     │  │  ┌───┴───┐      │  │      ▼          │             │
    │  │   │                │  │  │  10kΩ │      │  │  ┌───────┐      │             │
    │  │   ▼                │  │  └───┬───┘      │  │  │ PYRO  │      │             │
    │  │  LED (Power ON)    │  │      │          │  │  │CIRCUIT│      │             │
    │  │                    │  │      ├──► PC0   │  │  │(Below)│      │             │
    │  └────────┬───────────┘  │      │   (ADC)  │  │  └───────┘      │             │
    │           │              │  ┌───┴───┐      │  │                 │             │
    │           │              │  │  10kΩ │      │  │                 │             │
    │           │              │  └───┬───┘      │  │                 │             │
    │           │              │      │          │  │                 │             │
    │           │              │     GND         │  │                 │             │
    │           │              │                 │  │                 │             │
    │           │              │ Divider: 0.5    │  │                 │             │
    │           │              │ 14.8V → 7.4V    │  │                 │             │
    │           │              │ (within ADC)    │  │                 │             │
    │           │              └─────────────────┘  └─────────────────┘             │
    │           │                                                                    │
    │           │ VBAT_SWITCHED                                                      │
    │           │                                                                    │
    │           ▼                                                                    │
    │  ┌─────────────────────────────────────────────────────────────────────────┐   │
    │  │                         5V BUCK CONVERTER                                │   │
    │  │                           (MP1584EN)                                     │   │
    │  │                                                                          │   │
    │  │   VBAT_SW ──┬──[ L 33µH ]──┬──────────────────────────────► 5V_RAIL     │   │
    │  │             │              │                                             │   │
    │  │         ┌───┴───┐      ┌───┴───┐                                        │   │
    │  │         │MP1584 │      │ 22µF  │                                        │   │
    │  │         │  EN   │      │  10V  │                                        │   │
    │  │         └───┬───┘      └───┬───┘                                        │   │
    │  │             │              │                                             │   │
    │  │            GND ◄──────────┘                                              │   │
    │  │                                                                          │   │
    │  │   Input: 7-28V     Output: 5V @ 3A     Efficiency: 92%                  │   │
    │  │                                                                          │   │
    │  └─────────────────────────────────┬───────────────────────────────────────┘   │
    │                                    │                                           │
    │                                    │ 5V_RAIL                                   │
    │                                    │                                           │
    │               ┌────────────────────┼────────────────────┐                      │
    │               │                    │                    │                      │
    │               ▼                    ▼                    ▼                      │
    │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐                │
    │  │   3.3V LDO      │  │   GPS MODULE    │  │   LORA MODULE   │                │
    │  │   (AMS1117)     │  │   (NEO-7M)      │  │   (E32-433T)    │                │
    │  │                 │  │                 │  │                 │                │
    │  │ 5V ─►[AMS1117]  │  │  VCC: 5V        │  │  VCC: 5V        │                │
    │  │         │       │  │  Current: 40mA  │  │  TX: 120mA      │                │
    │  │         ▼       │  │                 │  │  RX: 18mA       │                │
    │  │      3.3V_RAIL  │  │                 │  │                 │                │
    │  │                 │  │                 │  │                 │                │
    │  │ Dropout: 1.2V   │  │                 │  │                 │                │
    │  │ Output: 1A max  │  │                 │  │                 │                │
    │  │                 │  │                 │  │                 │                │
    │  └────────┬────────┘  └─────────────────┘  └─────────────────┘                │
    │           │                                                                    │
    │           │ 3.3V_RAIL                                                          │
    │           │                                                                    │
    │           ▼                                                                    │
    │  ┌─────────────────────────────────────────────────────────────────────────┐   │
    │  │                         3.3V DEVICES                                     │   │
    │  │                                                                          │   │
    │  │  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐             │   │
    │  │  │ STM32F429 │  │  MPU9250  │  │  BNO055   │  │  BMP380   │             │   │
    │  │  │           │  │           │  │           │  │           │             │   │
    │  │  │  ~100mA   │  │   ~4mA    │  │  ~12mA    │  │   ~1mA    │             │   │
    │  │  └───────────┘  └───────────┘  └───────────┘  └───────────┘             │   │
    │  │                                                                          │   │
    │  │  ┌───────────┐  ┌───────────┐                                           │   │
    │  │  │  MS5611   │  │  W25Q40   │   Total 3.3V Load: ~130mA                 │   │
    │  │  │           │  │           │                                            │   │
    │  │  │   ~2mA    │  │  ~15mA    │                                            │   │
    │  │  └───────────┘  └───────────┘                                            │   │
    │  │                                                                          │   │
    │  └─────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                 │
    └─────────────────────────────────────────────────────────────────────────────────┘
```

---

## Pyrotechnic Power Circuit

```
                              PYRO CHANNEL DETAIL (x2: Drogue & Main)
    ┌─────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                 │
    │   VBAT_PROT (14.8V) ────────────────────────────────────┐                      │
    │                                                          │                      │
    │                                                          │                      │
    │                    ARM SWITCH ACTIVE                     │                      │
    │                          │                               │                      │
    │                          ▼                               │                      │
    │   ┌─────────────────────────────────────────┐           │                      │
    │   │            HARDWARE ARM CIRCUIT          │           │                      │
    │   │                                          │           │                      │
    │   │    PE4 ───►│◄─── ARM_SW_SENSE           │           │                      │
    │   │    (GPIO)   │    (Physical switch        │           │                      │
    │   │             │     in series)             │           │                      │
    │   │             │                            │           │                      │
    │   │             └────────────┐               │           │                      │
    │   │                          │               │           │                      │
    │   └──────────────────────────┼───────────────┘           │                      │
    │                              │                            │                      │
    │                              │ ARM_ENABLED                │                      │
    │                              │                            │                      │
    │   ┌──────────────────────────┼────────────────────────────┼─────────────────┐   │
    │   │                          │      DROGUE CHANNEL        │                 │   │
    │   │                          │                            │                 │   │
    │   │                          ▼                            │                 │   │
    │   │   PE0 ──┬──[ 100Ω ]──┬──║║──┬─────────────────────────┘                 │   │
    │   │  (GPIO) │   Gate     │  ║║  │                                           │   │
    │   │         │   Resistor │  ║║  │ Q1: IRLZ44N                               │   │
    │   │         │            │  ║║  │ N-Channel MOSFET                          │   │
    │   │     ┌───┴───┐        │  ║║  │ Vds: 55V, Id: 47A                         │   │
    │   │     │  10kΩ │        │  ╚╝  │ Rds(on): 22mΩ                             │   │
    │   │     └───┬───┘        │   │  │                                           │   │
    │   │         │            │   │  │                                           │   │
    │   │        GND           │   │  │                                           │   │
    │   │    (Pull-down)       │   │  │                                           │   │
    │   │                      │   │  │                                           │   │
    │   │                      │   ▼  │                                           │   │
    │   │                      │ ┌────┴────┐                                      │   │
    │   │                      │ │ E-MATCH │ DROGUE PYRO                          │   │
    │   │                      │ │  1-2Ω   │ (Igniter)                            │   │
    │   │                      │ └────┬────┘                                      │   │
    │   │                      │      │                                           │   │
    │   │                      │      ▼                                           │   │
    │   │                      │     GND                                          │   │
    │   │                      │                                                  │   │
    │   │   Continuity Check:  │                                                  │   │
    │   │   PC2 ◄──[ 10kΩ ]────┤  ADC reads voltage divider                      │   │
    │   │   (ADC)              │  With e-match: ~1.5V                             │   │
    │   │                      │  Without: 0V or 3.3V                             │   │
    │   │                                                                         │   │
    │   └─────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                 │
    │   ┌─────────────────────────────────────────────────────────────────────────┐   │
    │   │                          MAIN CHANNEL                                   │   │
    │   │                        (Same topology)                                  │   │
    │   │                                                                         │   │
    │   │   PE2 ──┬──[ 100Ω ]──┬──║║──┬─────── VBAT_PROT (14.8V)                 │   │
    │   │  (GPIO) │            │  ║║  │                                           │   │
    │   │     ┌───┴───┐        │  ║║  │ Q2: IRLZ44N                               │   │
    │   │     │  10kΩ │        │  ╚╝  │                                           │   │
    │   │     └───┬───┘        │   │  │                                           │   │
    │   │        GND           │   ▼  │                                           │   │
    │   │                      │ ┌────┴────┐                                      │   │
    │   │                      │ │ E-MATCH │ MAIN PYRO                            │   │
    │   │                      │ │  1-2Ω   │                                      │   │
    │   │                      │ └────┬────┘                                      │   │
    │   │                      │      ▼                                           │   │
    │   │                      │     GND                                          │   │
    │   │                      │                                                  │   │
    │   │   Continuity:        │                                                  │   │
    │   │   PC3 ◄──[ 10kΩ ]────┘                                                  │   │
    │   │   (ADC)                                                                 │   │
    │   │                                                                         │   │
    │   └─────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                 │
    └─────────────────────────────────────────────────────────────────────────────────┘
```

---

## Power Budget

### Normal Operation

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| STM32F429ZIT6 | 3.3V | 100mA | 330mW |
| MPU9250 | 3.3V | 4mA | 13mW |
| BNO055 | 3.3V | 12mA | 40mW |
| BMP380 | 3.3V | 1mA | 3mW |
| MS5611 | 3.3V | 2mA | 7mW |
| W25Q40 (write) | 3.3V | 15mA | 50mW |
| NEO-7M GPS | 5V | 40mA | 200mW |
| E32 LoRa (RX) | 5V | 18mA | 90mW |
| **Subtotal Normal** | | **192mA** | **733mW** |

### Peak Operation (Transmitting)

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| All above | - | 192mA | 733mW |
| E32 LoRa (TX) | 5V | +102mA | +510mW |
| **Subtotal TX** | | **294mA** | **1243mW** |

### Pyro Firing (100ms pulse)

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| E-match ignition | 14.8V | 2A | 29.6W |
| Duration | | 100ms | |
| **Energy per fire** | | | **2.96 Wh** |

---

## Battery Selection

### Recommended: 4S 650mAh 75C LiPo

| Parameter | Value |
|-----------|-------|
| **Chemistry** | Lithium Polymer (LiPo) |
| **Configuration** | 4S (4 cells in series) |
| **Nominal Voltage** | 14.8V |
| **Full Charge** | 16.8V |
| **Empty (safe)** | 13.2V (3.3V/cell) |
| **Capacity** | 650mAh |
| **C Rating** | 75C continuous |
| **Max Discharge** | 48.75A (way more than needed) |
| **Weight** | ~75g |
| **Connector** | XT30 (main) + JST-XH (balance) |

### Flight Duration Calculation

```
Battery Capacity:     650mAh
Average Draw:         ~300mA (including TX bursts)
Theoretical Runtime:  650 / 300 = 2.17 hours = 130 minutes

Actual Flight Time:   ~3 minutes (boost to landing)
Ground Ops:           ~30 minutes (power on to launch)
Total Mission:        ~35 minutes

Safety Margin:        130 / 35 = 3.7x margin ✓
```

---

## Voltage Monitoring

### ADC Configuration

```
Battery Voltage Divider:
    VBAT ──[ 10kΩ ]──┬──[ 10kΩ ]── GND
                     │
                     └──► PC0 (ADC)

Divider Ratio: 0.5
At 14.8V battery: ADC sees 7.4V (NEEDS CLAMPING!)

CORRECTION - Use 3:1 divider for safety:
    VBAT ──[ 20kΩ ]──┬──[ 10kΩ ]── GND
                     │
                     └──► PC0 (ADC)

Divider Ratio: 0.333
At 16.8V (full): ADC sees 5.6V (still too high!)

FINAL - Use 6:1 divider:
    VBAT ──[ 50kΩ ]──┬──[ 10kΩ ]── GND
                     │
                     └──► PC0 (ADC)

Divider Ratio: 0.167
At 16.8V (full): ADC sees 2.8V ✓
At 14.8V (nominal): ADC sees 2.47V ✓
At 13.2V (empty): ADC sees 2.2V ✓
```

### Voltage Thresholds

| Battery Voltage | ADC Voltage | Status | Action |
|-----------------|-------------|--------|--------|
| > 16.0V | > 2.67V | Overcharged | Warning |
| 14.8V - 16.0V | 2.47V - 2.67V | **Normal** | OK |
| 13.6V - 14.8V | 2.27V - 2.47V | Low | Warning LED |
| 13.2V - 13.6V | 2.20V - 2.27V | Critical | Abort launch |
| < 13.2V | < 2.20V | Dead | Shutdown |

---

## Safety Features

### 1. Reverse Polarity Protection
Schottky diode in series prevents damage if battery connected backwards.

### 2. Pyro Safety Chain
```
All conditions must be TRUE to fire:

    ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
    │  Hardware   │     │  Software   │     │   State     │
    │  Arm Switch │ AND │  Arm Flag   │ AND │   Machine   │
    │   (PE4)     │     │  (armed)    │     │  (correct)  │
    └──────┬──────┘     └──────┬──────┘     └──────┬──────┘
           │                   │                   │
           └───────────────────┴───────────────────┘
                               │
                               ▼
                        ┌─────────────┐
                        │  FIRE PYRO  │
                        └─────────────┘
```

### 3. Continuity Check
Before launch, verify e-matches are connected:
- Expected: ~1.5V (voltage divider with e-match resistance)
- Open circuit: 0V or 3.3V
- Short circuit: 0V

### 4. Auto-Shutoff
Pyro GPIO automatically turns OFF after 100ms to prevent:
- MOSFET overheating
- Battery drain
- Continuous firing

---

## Connector Pinout

### Main Power Connector (XT30)
```
    ┌─────────┐
    │  XT30   │
    │ ┌─┐ ┌─┐ │
    │ │+│ │-│ │
    │ └─┘ └─┘ │
    └─────────┘
      +   -
    VBAT GND
```

### Balance Connector (JST-XH 5-pin)
```
    ┌─────────────────────┐
    │      JST-XH 5P      │
    │ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ │
    │ │1│ │2│ │3│ │4│ │5│ │
    │ └─┘ └─┘ └─┘ └─┘ └─┘ │
    └─────────────────────┘
      │   │   │   │   │
     GND B1  B2  B3  B4
      0V 3.7V 7.4V 11.1V 14.8V
```

---

## Bill of Materials

| Component | Part Number | Qty | Notes |
|-----------|-------------|-----|-------|
| LiPo Battery | 4S 650mAh 75C | 1 | Main power |
| Buck Converter | MP1584EN module | 1 | 5V output |
| LDO Regulator | AMS1117-3.3 | 1 | 3.3V output |
| Schottky Diode | SS34 | 1 | Reverse protection |
| MOSFET | IRLZ44N | 2 | Pyro drivers |
| Capacitor | 100µF 25V | 1 | Input bulk |
| Capacitor | 22µF 10V | 2 | Buck output |
| Capacitor | 10µF 10V | 2 | LDO output |
| Resistor | 100Ω | 2 | Gate resistors |
| Resistor | 10kΩ | 6 | Pull-downs, dividers |
| Resistor | 50kΩ | 1 | Voltage divider |
| Inductor | 33µH | 1 | Buck converter |
| Connector | XT30 | 1 pair | Power |
| Connector | JST-XH 5P | 1 | Balance |
| Switch | SPST Toggle | 1 | Main arm |

---

## Pre-Flight Checklist

- [ ] Battery voltage > 15.5V (freshly charged)
- [ ] Main arm switch OFF
- [ ] Pyro arm switch OFF
- [ ] Power ON - verify LED
- [ ] Check battery voltage on ground station
- [ ] Verify drogue continuity
- [ ] Verify main continuity
- [ ] All systems GO
- [ ] Arm pyro switch (at pad)
- [ ] Clear area
- [ ] Launch!
