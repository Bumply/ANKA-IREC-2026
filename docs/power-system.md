---
layout: default
title: Power System
nav_order: 8
---

# Power System

Complete battery and power distribution system for the IREC 2026 flight computer.

---

## Batteries Used in Recovery System

Detailed specifications of all batteries in the flight electronics system:

### Battery Inventory

> **⚠️ IREC Compliance Note:** Per DTEG Section 6.17.1.1, **LiPo batteries are PROHIBITED** due to fire hazard. We use **Li-Ion 18650 cells in cylindrical metallic casing** which is explicitly allowed per DTEG 6.17.2.1.

| Component | Battery Type | Chemistry | Cells | Size | Capacity | Protection | Voltage Range |
|-----------|--------------|-----------|-------|------|----------|------------|---------------|
| **Flight Computer** | 18650 Li-Ion Pack | Lithium Ion | 4S | 18mm×65mm×4 | 2600mAh | **Protected** (PCB + cell-level) | 12.0V - 16.8V |
| **Video Payload** | (Shared with FC) | Lithium Ion | 4S | - | - | **Protected** | - |

### Flight Computer Battery - Detailed Specifications

```mermaid
flowchart LR
    subgraph PACK["4S Li-Ion 18650 Battery Pack"]
        subgraph CELLS["Cylindrical Cells (IREC Compliant)"]
            C1["18650 Cell 1<br/>3.7V nom"]
            C2["18650 Cell 2<br/>3.7V nom"]
            C3["18650 Cell 3<br/>3.7V nom"]
            C4["18650 Cell 4<br/>3.7V nom"]
        end
        
        subgraph BMS["Protection PCB"]
            OV["Overcharge<br/>4.2V/cell"]
            UV["Undervoltage<br/>3.0V/cell"]
            OC["Overcurrent<br/>Protection"]
            SC["Short Circuit<br/>Protection"]
        end
    end

    C1 --> C2 --> C3 --> C4
    CELLS --> BMS
```

| Parameter | Specification |
|-----------|---------------|
| **Cell Type** | 18650 Lithium Ion (cylindrical metallic casing) |
| **Recommended Cells** | Samsung 25R, Sony VTC6, LG HG2, Panasonic NCR18650B |
| **Chemistry** | Lithium Ion (Li-Ion) |
| **Configuration** | 4S1P (4 cells in series) |
| **Nominal Voltage** | 14.8V (3.7V × 4) |
| **Full Charge Voltage** | 16.8V (4.2V × 4) |
| **Discharge Cutoff** | 12.0V (3.0V × 4) |
| **Capacity** | 2500-3000mAh (depending on cell) |
| **Max Discharge** | 20A continuous (Samsung 25R) |
| **Physical Size** | ~75 × 40 × 20 mm (4S holder) |
| **Weight** | ~200g (4 × 50g cells) |
| **Main Connector** | XT30 (rated 30A continuous) |
| **Balance Connector** | JST-XH 5-pin |
| **Protection** | PCB with balance leads + cell-level protection |
| **IREC Compliance** | ✅ DTEG 6.17.2.1 - Cylindrical metallic casing allowed |

### Protection Circuit Details

The battery pack includes integrated protection:

| Protection Type | Threshold | Action |
|-----------------|-----------|--------|
| **Overcharge** | >4.25V per cell | Cuts charging current |
| **Overdischarge** | <2.8V per cell | Disconnects load |
| **Overcurrent** | >25A | Current limiting |
| **Short Circuit** | Instantaneous | Immediate disconnect |
| **Cell Balancing** | Via balance connector | During charging only |

### Why 18650 Li-Ion? (IREC Compliant & Affordable)

| Feature | 18650 Li-Ion | LiPo (prohibited) | LiFePO4 |
|---------|--------------|-------------------|---------|
| **IREC Allowed** | ✅ Yes (metallic casing) | ❌ **BANNED** | ✅ Yes |
| **Cost** | **~$5/cell** | ~$15/pack | ~$30/pack |
| **Availability** | Everywhere | - | Specialty |
| **Energy Density** | High | Highest | Lower |
| **Safety** | Good (metal case) | Poor (pouch) | Excellent |
| **Cycle Life** | 500+ cycles | 300 cycles | 2000+ cycles |

> **Note:** External protection is also provided on the flight computer PCB via Schottky diode (reverse polarity) and 10Ω current-limiting resistor.

---

## Battery Management Practices

### Charging Procedures

```mermaid
flowchart TB
    subgraph CHARGE["Charging Protocol"]
        START["Connect to<br/>Balance Charger"]
        SET["Set Mode:<br/>Li-Ion 4S<br/>1.0A charge rate"]
        BALANCE["Enable<br/>Balance Charging"]
        MONITOR["Monitor:<br/>- Cell voltages<br/>- Temperature<br/>- Current"]
        COMPLETE["Charge Complete<br/>when all cells = 4.2V"]
    end

    START --> SET --> BALANCE --> MONITOR --> COMPLETE
```

| Practice | Procedure |
|----------|-----------|
| **Charger Type** | Balance charger with Li-Ion mode (e.g., ISDT D2, SkyRC B6, Nitecore) |
| **Charge Rate** | 0.5C recommended, 1C maximum (2.5A for 2500mAh pack) |
| **Charge Mode** | **Li-Ion Balance mode** (4.2V/cell cutoff) |
| **Cell Balance** | All cells within 0.02V at end of charge |
| **Max Voltage** | 4.20V per cell (16.8V total) - NEVER exceed |
| **Temperature** | Charge at 10-40°C ambient only |
| **Supervision** | Monitor during charging |
| **Fire Safety** | Charge on fireproof surface, away from flammables |

### Capacity Verification

Before each flight campaign:

```mermaid
flowchart LR
    FULL["Charge to<br/>100%<br/>(16.8V)"]
    DISCHARGE["Discharge at<br/>0.5C to 12.0V"]
    MEASURE["Record<br/>mAh delivered"]
    EVALUATE["Capacity<br/>>80% nominal?"]
    PASS["✓ Flight<br/>Ready"]
    FAIL["✗ Retire<br/>Battery"]

    FULL --> DISCHARGE --> MEASURE --> EVALUATE
    EVALUATE -->|Yes| PASS
    EVALUATE -->|No| FAIL
```

| Test | Acceptance Criteria |
|------|---------------------|
| **Full Capacity** | ≥80% of rated capacity (≥2000mAh for 2500mAh cells) |
| **Cell Balance** | All cells within 0.05V under load |
| **Internal Resistance** | <80mΩ per cell (measured by charger) |
| **Physical Inspection** | No dents, rust, or damage to metallic casing |

### Storage Voltage Protocol

```mermaid
flowchart TB
    subgraph STORAGE["Storage Protocol"]
        CHARGE_ST["Charge/Discharge<br/>to Storage Voltage"]
        VOLTAGE["Target: 3.7V/cell<br/>(14.8V total)"]
        CONTAINER["Store in<br/>cool, dry location"]
        LOCATION["Room temperature<br/>15-25°C"]
    end

    CHARGE_ST --> VOLTAGE --> CONTAINER --> LOCATION
```

| Storage Practice | Specification |
|------------------|---------------|
| **Storage Voltage** | 3.6-3.8V per cell (14.4-15.2V total) - ~40-60% SOC |
| **Storage Mode** | Use charger's "Storage" function |
| **Temperature** | Store at 15-25°C (room temperature) |
| **Humidity** | Low humidity, avoid condensation |
| **Container** | Any safe container |
| **Duration** | Re-check voltage every 2-3 months |
| **Before Flight** | Charge to full within 24 hours of launch |

### Battery Age Monitoring

| Tracking Item | Method | Action Threshold |
|---------------|--------|------------------|
| **Purchase Date** | Label on battery | Retire after 3 years |
| **Cycle Count** | Log in spreadsheet | Retire after 300 cycles |
| **Capacity Fade** | Periodic capacity test | Retire if <80% capacity |
| **Internal Resistance** | Charger measurement | Retire if >100mΩ/cell |
| **Physical Condition** | Visual inspection | Retire if dented/damaged |

### Pre-Flight Battery Checklist

- [ ] Battery fully charged within last 24 hours
- [ ] Voltage verified: 16.4V - 16.8V (>98% charge)
- [ ] All cells balanced within 0.02V
- [ ] Visual inspection: metallic casing intact, no dents
- [ ] Capacity verified within last 30 days (>80%)
- [ ] Cycle count logged and within limits (<300)
- [ ] Battery age within limits (<3 years)
- [ ] Connectors clean and secure
- [ ] Battery secured in rocket with padding
- [ ] **IREC compliance: 18650 cylindrical metallic casing confirmed** ✅

### Emergency Procedures

| Situation | Action |
|-----------|--------|
| **Overheating during charge** | Disconnect immediately, move to fireproof area |
| **Physical damage** | Do not use, discharge safely, dispose at battery recycling |
| **Cell venting** | Evacuate area, do not inhale fumes, allow to cool |
| **Crash recovery** | Inspect cells for dents before handling |

> **Note:** 18650 Li-Ion cells with metallic casing are safer than pouch LiPo cells - the metal case provides structural protection and contains any potential failure.

---

## System Overview

### Power Distribution Architecture

```mermaid
flowchart TB
    subgraph BATT["4S Li-Ion 18650 Battery"]
        CELL["4S1P 14.8V<br/>2500mAh"]
        XT30["XT30<br/>Connector"]
    end

    subgraph PROT["Input Protection"]
        DIODE["Schottky<br/>Reverse Prot"]
        FUSE["10Ω Resistor"]
        CAP["100µF<br/>Bulk Cap"]
    end

    subgraph DIST["Power Distribution"]
        VBAT["VBAT_PROT<br/>14.8V"]
    end

    subgraph REG5V["5V Rail"]
        BUCK["MP1584EN<br/>Buck Converter<br/>5V @ 3A"]
    end

    subgraph REG33V["3.3V Rail"]
        LDO["AMS1117-3.3<br/>LDO<br/>3.3V @ 1A"]
    end

    subgraph LOAD5V["5V Devices"]
        GPS["NEO-7M<br/>GPS<br/>40mA"]
        LORA["E32-433T<br/>LoRa<br/>18-120mA"]
    end

    subgraph LOAD33V["3.3V Devices"]
        MCU["STM32F429<br/>100mA"]
        IMU1["MPU9250<br/>4mA"]
        IMU2["BNO055<br/>12mA"]
        BARO1["BMP380<br/>1mA"]
        BARO2["MS5611<br/>2mA"]
        FLASH["W25Q40<br/>15mA"]
    end

    subgraph PYRO["Pyro System"]
        PYRO_PWR["Direct from<br/>VBAT_PROT"]
    end

    subgraph VIDEO["Video Payload"]
        VBUCK["5V Buck<br/>Mini360<br/>3A"]
        VID_LOAD["OpenIPC + Camera<br/>+ VTX<br/>1.3A"]
    end

    CELL --> XT30
    XT30 --> DIODE
    DIODE --> FUSE
    FUSE --> CAP
    CAP --> VBAT

    VBAT --> BUCK
    VBAT --> PYRO_PWR
    VBAT --> VBUCK

    BUCK --> LDO
    BUCK --> GPS
    BUCK --> LORA

    LDO --> MCU
    LDO --> IMU1
    LDO --> IMU2
    LDO --> BARO1
    LDO --> BARO2
    LDO --> FLASH

    VBUCK --> VID_LOAD
```

### Voltage Rails

```mermaid
flowchart LR
    subgraph IN["Input"]
        B["Battery<br/>14.8V"]
    end

    subgraph RAILS["Voltage Rails"]
        V14["14.8V<br/>Pyro"]
        V5["5V<br/>GPS, LoRa"]
        V33["3.3V<br/>MCU, Sensors"]
        V5V["5V Video<br/>OpenIPC"]
    end

    B --> V14
    B -->|Buck| V5
    V5 -->|LDO| V33
    B -->|Buck| V5V
```

---

## Pyrotechnic Power Circuit

### 4-Inhibit Safety Chain (IREC Compliant)

Per IREC DTEG requirements, pyrotechnic systems must have **multiple independent inhibits**. Our system uses a **4-key arming architecture** where ALL conditions must be TRUE before any pyro channel can fire.

```mermaid
flowchart TB
    subgraph INHIBIT1["INHIBIT 1: Main Power"]
        PWR["Main Power Switch<br/>(External Toggle)"]
    end

    subgraph INHIBIT2["INHIBIT 2: Hardware Arm"]
        ARM["Pull Pin Arm Switch<br/>(External - PE4)"]
    end

    subgraph INHIBIT3["INHIBIT 3: Software Arm"]
        SW["Software Arm Flag<br/>(Ground Station Command)"]
    end

    subgraph INHIBIT4["INHIBIT 4: Flight State"]
        STATE["State Machine<br/>(ARMED/DESCENT/etc)"]
    end

    subgraph VERIFY["Verification"]
        CONT["Continuity Check<br/>(E-Match Connected)"]
    end

    subgraph FIRE["Fire Decision"]
        AND{{"AND Gate<br/>(ALL must be TRUE)"}}
    end

    subgraph PYRO["Pyro Channels"]
        DRG["Drogue Channel<br/>(Fires at Apogee)"]
        MAIN["Main Channel<br/>(Fires at 500m AGL)"]
    end

    PWR --> AND
    ARM --> AND
    SW --> AND
    STATE --> AND
    CONT -.->|"Verified"| AND
    AND --> DRG
    AND --> MAIN
```

### Inhibit Details

| Inhibit | Type | Location | Arm Method | Safe State |
|---------|------|----------|------------|------------|
| **1. Main Power** | Hardware | External toggle switch | Flip ON at pad | OFF = No power to system |
| **2. Pull Pin Arm** | Hardware | External pull pin (PE4 input) | Remove pin at pad | Pin IN = Pyro circuit disabled |
| **3. Software Arm** | Software | Ground station via LoRa | Send ARM command | Default DISARMED on boot |
| **4. Flight State** | Software | State machine logic | Auto-transitions in flight | IDLE/PAD = Cannot fire |

### Arming Sequence

```mermaid
flowchart LR
    subgraph GROUND["Ground Operations"]
        G1["1. Power ON<br/>(Main Switch)"]
        G2["2. System Boot<br/>(Self-test)"]
        G3["3. Verify Continuity<br/>(Ground Station)"]
    end

    subgraph PAD["At Launch Pad"]
        P1["4. Remove Pull Pin<br/>(Hardware Arm)"]
        P2["5. Send ARM Command<br/>(Software Arm)"]
        P3["6. Verify ARMED<br/>(Green LED)"]
    end

    subgraph FLIGHT["In Flight"]
        F1["7. Detect Launch<br/>(Accel Trigger)"]
        F2["8. Detect Apogee<br/>(State → DESCENT)"]
        F3["9. FIRE Drogue<br/>(All 4 Keys TRUE)"]
        F4["10. Altitude < 500m"]
        F5["11. FIRE Main<br/>(All 4 Keys TRUE)"]
    end

    G1 --> G2 --> G3 --> P1 --> P2 --> P3 --> F1 --> F2 --> F3 --> F4 --> F5
```

### Fire Logic (C Code)

```c
// 4-Inhibit Arming Check
bool can_fire_pyro(void) {
    return (
        main_power_on &&                    // Inhibit 1: Power switch ON
        gpio_read(PULL_PIN_ARM) == HIGH &&  // Inhibit 2: Pull pin removed
        software_arm_flag == true &&        // Inhibit 3: Ground station armed
        (flight_state == DESCENT ||         // Inhibit 4: Correct flight state
         flight_state == MAIN_DEPLOY)
    );
}

// Drogue Fire Decision
void check_drogue_fire(void) {
    if (can_fire_pyro() && 
        flight_state == DESCENT && 
        drogue_continuity_ok &&
        !drogue_fired) {
        fire_drogue();
        drogue_fired = true;
    }
}

// Main Fire Decision  
void check_main_fire(void) {
    if (can_fire_pyro() && 
        altitude_agl < 500 &&  // meters
        main_continuity_ok &&
        drogue_fired &&        // Drogue must fire first
        !main_fired) {
        fire_main();
        main_fired = true;
    }
}
```

### Safety Features

| Feature | Purpose |
|---------|---------|
| **Default DISARMED** | Software arm flag is FALSE on boot |
| **Pull pin physical** | Cannot accidentally arm remotely |
| **State machine required** | Won't fire on ground even if armed |
| **Continuity check** | Warns if e-match disconnected |
| **100ms auto-off** | MOSFET turns off after firing to prevent damage |
| **Dual channels independent** | One failure doesn't affect other |

### Pyro Channel Circuit

```mermaid
flowchart TB
    subgraph POWER["Power Source"]
        VBAT["VBAT_PROT<br/>14.8V Direct"]
    end

    subgraph CONTROL["Control Circuit"]
        GPIO["PE0/PE2<br/>GPIO Output"]
        RGATE["100Ω<br/>Gate Resistor"]
        RPULL["10kΩ<br/>Pull-down"]
    end

    subgraph SWITCH["Power Switch"]
        FET["IRLZ44N<br/>N-MOSFET<br/>55V 47A"]
    end

    subgraph LOAD["Pyrotechnic"]
        EMATCH["E-Match<br/>1-2Ω"]
    end

    subgraph SENSE["Continuity"]
        ADC["PC2/PC3<br/>ADC Input"]
        RSENSE["10kΩ<br/>Sense Resistor"]
    end

    VBAT --> EMATCH
    EMATCH --> FET
    FET --> GND["GND"]

    GPIO --> RGATE
    RGATE --> FET
    RPULL --> GND

    EMATCH --> RSENSE
    RSENSE --> ADC
```

### Dual Channel Layout

```mermaid
flowchart LR
    subgraph DROGUE["Drogue Channel"]
        D_GPIO["PE0"]
        D_FET["MOSFET Q1"]
        D_EMATCH["Drogue<br/>E-Match"]
        D_CONT["PC2 ADC"]
    end

    subgraph MAIN["Main Channel"]
        M_GPIO["PE2"]
        M_FET["MOSFET Q2"]
        M_EMATCH["Main<br/>E-Match"]
        M_CONT["PC3 ADC"]
    end

    D_GPIO --> D_FET --> D_EMATCH
    D_EMATCH -.-> D_CONT

    M_GPIO --> M_FET --> M_EMATCH
    M_EMATCH -.-> M_CONT
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
| **Flight Computer Subtotal** | | **192mA** | **733mW** |
| | | | |
| **VIDEO PAYLOAD** | | | |
| OpenIPC SSC338Q | 5V | 500mA | 2500mW |
| IMX307 Camera | 5V | 200mA | 1000mW |
| 5.8GHz VTX (25mW mode) | 5V | 300mA | 1500mW |
| **Video Subtotal** | | **1000mA** | **5000mW** |
| | | | |
| **TOTAL SYSTEM** | | **~1.2A** | **~5.7W** |

### Peak Operation (Transmitting + Video at Max Power)

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| Flight Computer (all) | - | 192mA | 733mW |
| E32 LoRa (TX) | 5V | +102mA | +510mW |
| Video (600mW TX mode) | 5V | +1300mA | +6500mW |
| **TOTAL PEAK** | | **~1.6A** | **~7.7W** |

### Pyro Firing (100ms pulse)

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| E-match ignition | 14.8V | 2A | 29.6W |
| Duration | | 100ms | |
| **Energy per fire** | | | **2.96 Wh** |

---

## Battery Selection

### 4S Li-Ion 18650 Pack (2500mAh)

> **Note:** Li-Ion 18650 cells with cylindrical metallic casing are IREC-compliant per DTEG 6.17.2.1.

| Parameter | Value |
|-----------|-------|
| **Chemistry** | Lithium-Ion (Li-Ion) |
| **Cell Type** | 18650 Cylindrical (Samsung 25R / Sony VTC6) |
| **Configuration** | 4S1P (4 cells in series) |
| **Nominal Voltage** | 14.8V (3.7V/cell) |
| **Full Charge** | 16.8V (4.2V/cell) |
| **Cutoff (safe)** | 12.0V (3.0V/cell) |
| **Capacity** | 2500mAh |
| **Max Continuous** | 20A (Samsung 25R) / 30A (Sony VTC6) |
| **Weight** | ~200g (50g × 4 cells) |
| **Connector** | XT30 (main) + JST-XH (balance) |
| **IREC Compliance** | ✅ DTEG 6.17.2.1 - Cylindrical metallic casing |

### Flight Duration Calculation

```
Battery Capacity:     2500mAh
Average Draw:         ~1200mA (flight computer + video streaming)
Theoretical Runtime:  2500 / 1200 = 2.08 hours = 125 minutes

Actual Flight Time:   ~3 minutes (boost to landing)
Ground Ops:           ~30 minutes (power on to launch)
Total Mission:        ~35 minutes

Safety Margin:        125 / 35 = 3.57x margin ✓ (EXCELLENT)

Note: For longer ground ops, keep video in standby until T-5 minutes
      Standby draw: ~400mA → 6.25 hour runtime
```

---

## Voltage Monitoring

### ADC Configuration

```mermaid
flowchart LR
    subgraph DIVIDER["Voltage Divider (6:1)"]
        VBAT["VBAT<br/>14.8-16.8V"]
        R1["50kΩ"]
        R2["10kΩ"]
        ADC["PC0 ADC<br/>2.2-2.8V"]
    end

    VBAT --> R1
    R1 --> ADC
    ADC --> R2
    R2 --> GND["GND"]
```

**Divider Calculation:**
- Ratio: 10k / (50k + 10k) = 0.167
- At 16.8V (full): ADC sees 2.8V ✓
- At 14.8V (nominal): ADC sees 2.47V ✓
- At 13.2V (empty): ADC sees 2.2V ✓

### Voltage Thresholds

| Battery Voltage | ADC Voltage | Status | Action |
|-----------------|-------------|--------|--------|
| > 16.4V | > 2.73V | Full | OK (just charged) |
| 14.8V - 16.4V | 2.47V - 2.73V | **Normal** | OK |
| 13.0V - 14.8V | 2.17V - 2.47V | Low | Warning LED |
| 12.0V - 13.0V | 2.00V - 2.17V | Critical | Abort launch |
| < 12.0V | < 2.00V | Cutoff | Shutdown |

---

## Safety Features

### 1. Reverse Polarity Protection
Schottky diode in series prevents damage if battery connected backwards.

### 2. Pyro Safety Chain

```mermaid
flowchart LR
    HW["Hardware<br/>Arm Switch<br/>(PE4)"]
    SW["Software<br/>Arm Flag"]
    STATE["State<br/>Machine"]
    AND{{"AND"}}
    FIRE["🔥 FIRE<br/>PYRO"]

    HW --> AND
    SW --> AND
    STATE --> AND
    AND --> FIRE
```

All three conditions must be TRUE to fire.

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

### Connectors Overview

```mermaid
flowchart LR
    subgraph XT30["XT30 Power"]
        PLUS["+ VBAT"]
        MINUS["- GND"]
    end

    subgraph JSTXH["JST-XH Balance (5-pin)"]
        P1["1: GND<br/>0V"]
        P2["2: B1<br/>3.7V"]
        P3["3: B2<br/>7.4V"]
        P4["4: B3<br/>11.1V"]
        P5["5: B4<br/>14.8V"]
    end
```

---

## Bill of Materials

### Flight Computer Power

| Component | Part Number | Qty | Notes |
|-----------|-------------|-----|-------|
| Li-Ion Battery | 4S1P 18650 (Samsung 25R) | 1 | Main power - IREC compliant |
| Buck Converter | MP1584EN module | 1 | 5V output (FC) |
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
| 18650 Holder | 4-cell series | 1 | Battery enclosure |
| BMS | 4S 20A | 1 | Protection circuit |

### Video Payload Power

| Component | Part Number | Qty | Notes |
|-----------|-------------|-----|-------|
| Buck Converter | Mini360 (MP2307) | 1 | 5V @ 3A for video |
| Capacitor | 100µF 10V | 2 | Video power filtering |
| LC Filter | 10µH + 100µF | 1 | RF noise suppression |
| Switch | SPST Toggle | 1 | Video power switch |

---

## Pre-Flight Checklist

### Power System
- [ ] Battery voltage > 16.0V (freshly charged)
- [ ] Cell balance within 0.02V
- [ ] Main arm switch OFF
- [ ] Pyro arm switch OFF
- [ ] Video power switch OFF
- [ ] Power ON - verify LED
- [ ] Check battery voltage on ground station
- [ ] Verify drogue continuity
- [ ] Verify main continuity

### Video Payload
- [ ] Video power switch ON (T-5 min)
- [ ] Verify video stream on ground receiver
- [ ] Check video latency < 100ms
- [ ] Confirm recording indicator

### Final
- [ ] All systems GO
- [ ] **IREC battery compliance verified (Li-Ion 18650)** ✅
- [ ] Arm pyro switch (at pad)
- [ ] Clear area
- [ ] Launch!
