# Pool Temperature Meter

M5Stack Core2 pool temperature display built with PlatformIO and Arduino.

## Purpose

The app reads a Dallas DS18B20 temperature probe connected to Port A on GPIO 32 and shows the measured value on the Core2 screen using LVGL.

Current behavior:
- Cold start shows a fresh temperature reading first, then connects to Wi-Fi and MQTT and updates the status dot.
- (currently disabled) Touch wake performs a fresh local temperature measurement only, without Wi-Fi or MQTT.
- Timer wake measures temperature, connects to Wi-Fi, publishes the value over MQTT, advertises the sensor through Home Assistant MQTT discovery, and then enters deep sleep.
- The screen shows `N/A` when the probe is missing or a read fails.
- Deep sleep powers down the LCD/backlight through the PMIC.

## Architecture

- `src/main.cpp` hosts the full app.
- LVGL renders the temperature text and a small Wi-Fi status dot in the top-left corner.
- The Wi-Fi dot reflects the last Wi-Fi connection attempt result.
- Timer wake performs measurement, Wi-Fi/MQTT publishing, discovery, and deep sleep.
- (currently disabled) Touch wake measures locally and shows the fresh reading without network use.
- Cold boot shows the fresh reading, then performs Wi-Fi/MQTT work.
- A 30-second inactivity timeout sleeps the screen on display wake paths.
- Serial logging at 115200 baud is controlled by `POOL_TEMP_LOGGING`.

## Stack

- PlatformIO
- Platform: `espressif32` 6.12.0
- Board: `m5stack-core2`
- Framework: `arduino` (`framework-arduinoespressif32` 3.20017.241212)
- Libraries: `M5Core2` 0.2.0, `lvgl` 8.4.0, `OneWire` 2.3.8, `DallasTemperature` 4.0.6, `PubSubClient` 2.8
- Build flags: `LV_CONF_SKIP`, `LV_COLOR_DEPTH=16`, `LV_FONT_MONTSERRAT_48=1`, `POOL_TEMP_LOGGING=1`

## Run

Build:

```sh
pio run
```

Upload:

```sh
pio run --target upload
```

Monitor logs:

```sh
pio device monitor --baud 115200
```

Configuration:

- Fill in `include/secrets.h` with your Wi-Fi SSID/password and MQTT broker settings.
- `MQTT_BASE_TOPIC` controls the retained state and discovery topics.
- Timer wake interval is currently 60 minutes in `src/main.cpp`.