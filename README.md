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

- [x] WiFi connection and Shelly Plug Gen2/3 discovery via mDNS
- [ ] Sensor integration (DHT22 and other)
- [ ] Time-based switching rules with sensor thresholds
- [ ] Battery-backed RTC (DS3231) for timekeeping - including timezone support,
automatic daylight saving time adjustment and NTP synchronization
- [ ] SD-Card for setup and logging
- [ ] Touch display interface
- [ ] Optional Database connection


## Hardware

The following diagram shows the wiring of the MainUnit v1 prototype. 
Depending on the components used, wiring may differ -
particularly for the display module, as variations in pinout 
and layout are common across manufacturers.

![TerraControl MainUnit v1](hardware/mainunit_v1_bb.svg)
