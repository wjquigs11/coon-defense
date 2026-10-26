# Rain Tank Level Monitor

ESP32 firmware that measures rainwater tank level with an ultrasonic distance sensor and serves it over WiFi.

## Features

- Ultrasonic distance sampling (non-blocking, one sample every 60 s, averaged over the last 10)
- Distance converted to gallons using configurable tank top, tank bottom, and capacity
- Web UI at `/rain` with a live gauge (Server-Sent Events on `/events`) and a calibration form
- JSON readings at `/readings`
- WiFi manager / captive portal, mDNS, NTP time, ElegantOTA updates, WebSerial console

## Hardware

The sensor is an HC-SR04-compatible RCWL-9610 module that runs on 2.8–5.5 V, so it can be powered from the ESP32's 3.3 V rail. Its datasheet is in [ultra-docs](ultra-docs/ultrasonic.md). The firmware uses GPIO mode (the module default).

| Sensor pin | ESP32 |
|------------|-------|
| Vcc        | 3.3 V |
| Trig       | GPIO 17 |
| Echo       | GPIO 19 |
| Gnd        | GND   |

## Calibration

Tank geometry is stored in NVS (`raintank` namespace) and can be changed from the `/rain` page:

- `tanktop`: inches from the sensor to the water surface when full (default 5)
- `tankbottom`: inches from the sensor to the water surface when empty (default 46)
- `capacity`: total gallons (default 776)

## Building

PlatformIO project. Build the firmware and upload the `data/` folder as a LittleFS image.
