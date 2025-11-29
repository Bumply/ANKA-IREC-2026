# IREC-2026

<p align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=gradient&customColorList=6,11,20&height=180&section=header&text=IREC%202026&fontSize=42&fontColor=fff&animation=twinkling&fontAlignY=32"/>
</p>

<p align="center">
  <img src="https://media.giphy.com/media/3oKIPtjElfqwMOTbH2/giphy.gif" width="200"/>
</p>

<p align="center">
  <img src="https://readme-typing-svg.demolab.com?font=Fira+Code&weight=600&size=22&pause=1000&color=3584E4&center=true&vCenter=true&random=false&width=435&lines=Flight+Computer+Firmware;Dual+Redundant+Avionics;10K+COTS+Category" alt="Typing SVG" />
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Competition-IREC%202026-red?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Category-10K%20COTS-blue?style=for-the-badge" />
  <img src="https://img.shields.io/badge/MCU-STM32F429ZIT6-green?style=for-the-badge" />
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Status-In%20Development-yellow?style=flat-square" />
  <img src="https://img.shields.io/github/last-commit/Bumply/IREC-2026?style=flat-square" />
</p>

<p align="center">
  <a href="https://bumply.github.io/IREC-2026/"><strong>View Full Documentation</strong></a>
</p>

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| MCU | STM32F429ZIT6 | 180MHz Cortex-M4F |
| IMU 1 | MPU-9250 | I2C1 (0x68) |
| IMU 2 | BNO055 | I2C2 (0x28) |
| Baro 1 | BMP380 | I2C1 (0x77) |
| Baro 2 | MS5611 | I2C1 (0x76) |
| GPS | NEO-7M | UART2 |
| Radio | E32-433T30D | UART3 |
| Flash | W25Q40 | SPI1 |
| Pyro | 2x IRFU120 | GPIO |

---

## Project Structure

```
avionics/
└── firmware/
    ├── Drivers/    # Sensor & peripheral drivers
    ├── Inc/        # Header files
    ├── Src/        # Source files
    └── Startup/    # Boot code
```

---

## Documentation

Full technical documentation available at **[bumply.github.io/IREC-2026](https://bumply.github.io/IREC-2026/)**

- System Overview
- Flight State Machine
- Sensor Fusion (Kalman Filter)
- Hardware Drivers
- Telemetry Protocol
- Data Logging
