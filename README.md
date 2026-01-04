# ESP32 WiFi Template

A template for ESP32 projects with WiFi connectivity, web interface, OTA updates, and flexible configuration options. This template provides a foundation for IoT projects with built-in logging, captive portal WiFi setup, and optional features like NTP time sync and deep sleep.

## Features

- **WiFi Management**: Captive portal for easy WiFi configuration with credentials stored in SPIFFS
- **Web Interface**: Responsive web UI with hamburger menu navigation
- **OTA Updates**: Over-the-air firmware updates via ElegantOTA
- **Logging System**: Multi-destination logging (Serial, SPIFFS file, WebSerial)
- **mDNS Support**: Access device via hostname.local
- **Time Synchronization**: Optional NTP or browser-based time sync
- **Deep Sleep**: Optional power-saving deep sleep mode
- **Preferences Storage**: Persistent configuration using ESP32 Preferences API
- **Double Reset Detection**: Quick WiFi reset via double-reset within 10 seconds

## Project Structure

```
├── data/                     # Web files (uploaded to SPIFFS)
│   ├── index.html            # Main web interface with hamburger menu
│   ├── wifimanager.html      # WiFi configuration page
│   ├── style.css             # Responsive CSS with mobile support
│   ├── script.js             # Main JavaScript
│   ├── client-time.js        # Browser time sync
│   ├── gauge.min.js          # Gauge visualization library
│   └── favicon.png/ico       # Site icons
├── src/
│   ├── main.cpp              # Main program entry point
│   ├── include-general.h     # Global includes and declarations
│   ├── wifi.cpp              # WiFi and captive portal logic
│   ├── webserver.cpp         # Web server routes and handlers specific to a project
│   ├── webserver-general.cpp # Basic web server functionality
│   ├── webserial.cpp         # WebSerial implementation
│   ├── time.cpp              # NTP time synchronization
│   └── logto.cpp/h           # Logging system
└── platformio.ini            # PlatformIO configuration
```

Keeping webserver-general and webserver separate helps when fetching changes from main into branched projects. Add project-specific "include.h" and optionally project-main.cpp and add your endpoints to webserver.cpp

## Configuration

### Build Flags (platformio.ini)

Enable/disable features by uncommenting build flags. I typically start a new project with most of these disabled to work on basic functionality before adding wifi and web interfaces.

```ini
build_flags =
    -D ELEGANTOTA_USE_ASYNC_WEBSERVER=1
    ; -D ELEGANTOTA      # Enable OTA updates
    ; -D WEBSERIAL       # Enable web-based serial console
    ; -D WIFI            # Enable WiFi functionality
    ; -D NTP             # Enable NTP time synchronization
    ; -D DEEPSLEEP       # Enable deep sleep mode
    -D TEMPLATE          # Enable template web interface
```

### WiFi Configuration

**Method 1: Captive Portal (First Boot)**
1. ESP32 creates AP named "ESP-SETUP"
2. Connect to it and configure WiFi settings
3. Credentials saved to `/wifi.txt` in SPIFFS

**Method 2: Manual Configuration**
Create `/wifi.txt` in SPIFFS with format:
```
SSID:password
SSID:password:static_ip
SSID:password:static_ip:gateway
```

**Method 3: Double Reset**
- Reset device twice within 10 seconds to clear WiFi settings
- Device will restart in captive portal mode

### Hostname Configuration

Default hostname: `ESPhost`

Change via:
- Captive portal form
- Web interface `/config?hostname=newname`
- Stored in Preferences (not SPIFFS)

## Web Interface

### Available Routes

Configured in webserver-general.cpp. Configure project-specific routes in webserver.cpp.
- `/` - Main dashboard with gauge display. The main page reads values set in a JSON variable "readings".
- `/wifimanager` - WiFi configuration page
- `/settings` - Application settings
- `/host` - Display hostname and MAC address
- `/update` - OTA firmware update page (if ELEGANTOTA enabled)
- `/readings` - JSON endpoint for sensor data
- `/config` - Configuration API endpoint

### Menu Navigation

The web interface includes a responsive hamburger menu with:
- Settings
- WiFi Settings
- Host Info
- Update (OTA)

## GPIO Considerations

**Avoid these pins for bidirectional sensors:**
- GPIO 34, 35, 36, 39 (input-only, no pull-up/pull-down)

**Recommended pins for sensors:**
- GPIO 4, 5, 12-19, 21-23, 25-27, 32-33

## Usage

### Quick Start

1. **Configure features** in `platformio.ini`
2. **Customize hostname** in `src/main.cpp` (line 11)
3. **Upload filesystem** (SPIFFS): `pio run -t uploadfs`
4. **Build and upload**: `pio run -t upload`
5. **Monitor serial**: `pio device monitor`

### First Boot

1. Device creates "ESP-SETUP" access point
2. Connect and navigate to captive portal
3. Enter WiFi credentials and hostname
4. Device restarts and connects to WiFi
5. Access via `http://hostname.local` or IP address

### Development Workflow

1. Start with all features disabled in `platformio.ini`
2. Enable features as needed
3. Customize `main.cpp` for your application
4. Modify web interface in `data/` folder
5. Update `getSensorReadings()` in webserver.cpp for your data

## Logging System

The `log::toAll()` function logs to multiple destinations:
- Serial console
- SPIFFS file (`/console.log`)
- WebSerial (if enabled)

Example:
```cpp
log::toAll("Sensor reading: " + String(value));
```

## Time Synchronization

When NTP is enabled:
1. Attempts NTP sync on WiFi connection
2. Falls back to browser time if NTP fails
3. Periodic resync every 24 hours
4. Browser sends time via `/clienttime` POST endpoint

## Deep Sleep Mode

When DEEPSLEEP is enabled:
- Device stays awake for `awakeTimer` seconds (default: 300s)
- Sleeps for `TIME_TO_SLEEP` seconds (default: 60s)
- Boot count persists in RTC memory
- Wakeup reason logged on boot

## Dependencies

- ESP32 Arduino Framework
- AsyncTCP
- ESPAsyncWebServer
- ArduinoJson
- ElegantOTA (optional)
- WebSerial Pro (optional)

## Troubleshooting

### mDNS Not Working
- Ensure hostname contains only alphanumeric characters and hyphens
- Check WiFi is connected (mDNS only works in station mode)
- Try accessing via IP address instead

### WiFi Won't Connect
- Double-reset to clear credentials
- Check `/wifi.txt` format in SPIFFS
- Verify SSID and password are correct

### OTA Upload Fails
- Ensure ELEGANTOTA is enabled in build flags
- Access `/update` route in browser
- Check device is on same network

## License

See LICENSE file for details.

## Customization Tips

1. **Change default hostname**: Edit line 11 in `src/main.cpp`
2. **Modify web interface**: Edit files in `data/` folder
3. **Add sensor readings**: Update `getSensorReadings()` in webserver.cpp
4. **Add web routes**: Add handlers in `startWebServer()` function
5. **Customize logging**: Modify `logto.cpp` for different log destinations

