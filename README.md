# TerraControl

*Smart terrarium control for those who don't want their setup 
to stop working the day some company decides to shut down their 
servers — and for those who need more than one or two plugs. 
A work in progress measured in years, not sprints, with a parts budget that long surpassed the 
price of the off-the-shelf box, not counting the ones that 
released their magic smoke. And if this accidentally turns out 
to be a solid foundation for a home automation system — well, 
that was definitely not the plan.*



## Current Status

This is an early prototype. Further sensors and actuators 
are planned and will be evaluated in future versions.

- [x] WiFi connection and Shelly Plug Gen2/3 discovery via mDNS
- [x] Shelly status retrieval and switching via SHA-256 Digest Auth, identified by MAC address — robust against IP reassignment
- [ ] Sensor integration (DHT22 and other)
- [ ] Time-based switching rules with sensor thresholds
- [ ] Battery-backed RTC (DS3231) for timekeeping - including timezone support, automatic daylight saving time adjustment and NTP synchronization
- [ ] SD-Card for setup and logging
- [ ] Touch display interface
- [ ] Optional Database connection


## Hardware

The following diagram shows the wiring of the MainUnit v1 prototype. 
Depending on the components used, wiring may differ -
particularly for the display module, as variations in pinout 
and layout are common across manufacturers.


![TerraControl MainUnit v1](hardware/TerraControl_MainUnit_V1_bb.png)

*[Download full diagram (PDF)](hardware/TerraControl_MainUnit_V1_bb.pdf)*

## Hardware List

| Component | Description | Quantity |
|---|---|---|
| ESP32 DevKit V1 | ESP32-WROOM-32D, 38-pin | 1 |
| TFT Display | 3.2" SPI ILI9341 / XPT2046 Touch / SD-Card slot | 1 |
| DS3231 RTC | I²C Real Time Clock with battery backup | 1 |
| LIR2032 | Rechargeable backup battery for DS3231 | 1 |
| DHT22 | Temperature and humidity sensor | 4 |
| MB102 | Breadboard power supply 3.3V / 5V | 1 |
| S8050 | NPN transistor, TO-92 | 1 |
| Resistor | 1kΩ, metal film | 1 |
| Capacitor | 100nF ceramic (104) | 2 |
| Capacitor | 100µF electrolytic | 1 |
| Capacitor | 10µF electrolytic | 2 |
| Breadboard | 830 holes | 2 |
| Dupont cables | Male-to-Male / Male-to-Female / Female-to-Female | assorted |
| USB cable | Micro-USB, for ESP32 power and flashing | 1 |
| Power supply | 9-12V DC barrel jack, min. 1A | 1 |
| Coffee | Black, strong | ∞ |

## Getting Started

### Prerequisites
- [VS Code](https://code.visualstudio.com/) with 
[PlatformIO](https://platformio.org/) extension
- ESP32 DevKit V1 (ESP32-WROOM-32D) and the full hardware 
setup as described in the Hardware section

### Installation

1. Clone the repository
```bash
   git clone https://github.com/lernenrocks/TerraControl.git
```

2. Copy the WiFi configuration template and fill in your credentials
```bash
   cp wifi_config_example.h wifi_config.h
```

3. Open the project in VS Code with PlatformIO and upload to your ESP32

### Libraries
The following libraries are required and will be installed 
automatically by PlatformIO:
- `northernwidget/DS3231`
- `bodmer/TFT_eSPI`
- `adafruit/DHT sensor library`
- `bblanchon/ArduinoJson`

> **Note:** Library versions may change as development progresses, 
> particularly if LVGL is adopted for the display interface.

## Feedback
Questions and feedback are always welcome — feel free to open an issue. 
Please note that this is a hobby project and response times may vary.

## Author
Andy / lernenrocks<br>
[www.lernen.rocks](https://lernen.rocks/)
