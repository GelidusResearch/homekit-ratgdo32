# DHT22 Temperature and Humidity Sensor Feature

## Overview
The DHT22 feature adds temperature and humidity monitoring to the homekit-ratgdo32 firmware, exposing separate Temperature Sensor and Humidity Sensor accessories to HomeKit.

## Conditional Compilation
The DHT22 feature is controlled by the `USE_DHT22` build flag to allow enabling/disabling the feature at compile time.

### Enabling DHT22 Support
To **enable** DHT22 support, include the build flag in `platformio.ini`:
```ini
build_flags =
    ...
    -D USE_DHT22
```

### Disabling DHT22 Support
To **disable** DHT22 support (saves ~2.7KB flash), comment out or remove the build flag:
```ini
build_flags =
    ...
    ;-D USE_DHT22  // Commented out
```

## Code Sections Protected by `#ifdef USE_DHT22`

### Header Files
- **src/homekit.h**:
  - `HOMEKIT_AID_TEMP_HUMIDITY` constant
  - `DEV_TempHumidity` struct definition
  - `tempHumidity` extern pointer declaration

- **src/config.h**:
  - `cfg_dht22Pin` and `cfg_dht22TempFormat` configuration constants
  - `getDHT22Pin()` and `getDHT22TempFormat()` getter methods

### Implementation Files
- **src/homekit.cpp**:
  - `DEV_TempHumidity` pointer and implementation
  - Temperature/Humidity service constructor, setValues(), and loop()
  - `notify_homekit_temperature_humidity()` notification function
  - Accessory creation logic in `setup_homekit()`

- **src/web.cpp**:
  - `#include "DHT.h"` library include
  - DHT22 sensor reading and caching logic
  - Temperature format handling (Celsius/Fahrenheit)
  - HomeKit notification calls
  - Configuration JSON output (`dht22Pin`, `dht22TempFormat`)

- **src/config.cpp**:
  - `dht22TempFormatBuf` buffer allocation
  - Settings map entries for DHT22 configuration

## Dependencies
When USE_DHT22 is defined:
- **DHT sensor library @ 1.4.6** (Adafruit)
- **Adafruit Unified Sensor** (dependency of DHT library)

## Build Statistics
- With USE_DHT22: Flash 89.0% (1,807,239 bytes), RAM 21.0% (68,784 bytes)
- Flash overhead: ~2,672 bytes

## Configuration
When enabled, DHT22 settings are available via web UI and stored in NVS:
- **GPIO Pin**: -1 (disabled) to 40 (requires reboot when changed)
- **Temperature Format**: "C" (Celsius) or "F" (Fahrenheit) (no reboot required)

## HomeKit Integration
- Creates separate **TemperatureSensor** and **HumiditySensor** services (AID 10)
- Temperature always sent to HomeKit in **Celsius** (per HAP specification)
- Web UI displays in user-selected format (°C or °F)
- Sensor readings update every 60 seconds
- Format changes invalidate cache for immediate re-read

## Important Notes
1. **Build Flag Naming**: Uses `USE_DHT22` (not `DHT22`) to avoid conflict with DHT library's `DHT22` constant
2. **Clean Build Required**: When changing the flag, run `pio run -e <env> -t clean` before rebuilding
3. **Library Overhead**: DHT library is always downloaded but only compiled when `USE_DHT22` is defined
