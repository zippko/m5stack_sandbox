## M5Stack Sandbox

Workspace for PlatformIO-based M5Stack Core2 experiments.

## Available Applications

### `pool_temp_meter`

Battery-oriented pool temperature meter for `M5Stack Core2`.

Current high-level behavior:
- Reads a `DS18B20` probe on Port A / `GPIO 32`.
- Shows the measured temperature on the Core2 display using `LVGL`.
- Displays `N/A` when the sensor is missing or a read fails.
- Attempts Wi-Fi and MQTT publish for Home Assistant integration.
- Uses deep sleep after the measurement/publish cycle.
- Maintains a small Wi-Fi status indicator on the screen.

Project files:
- App code: `pool_temp_meter/src/main.cpp`
- App notes: `pool_temp_meter/README.md`
- Build config: `pool_temp_meter/platformio.ini`

### `eng_quiz`

Touch-based English vocabulary quiz for `M5Stack Core2`.

Current high-level behavior:
- Displays a Slovak meaning and four English answer choices.
- Uses the touch screen for answer selection.
- Tracks score during a short round.
- Shows immediate correct/incorrect feedback.
- Restarts the quiz after the final score screen is tapped.

Project files:
- App code: `eng_quiz/src/main.cpp`
- Build config: `eng_quiz/platformio.ini`

## Stack

- PlatformIO
- Board target: `m5stack-core2`
- Framework: `arduino`

Version notes:
- `pool_temp_meter` pins `espressif32@6.12.0` and explicit library versions.
- `eng_quiz` uses `espressif32` without a pinned platform version and depends on `M5Core2`.

## Run

Build a project:

```sh
pio run
```

Upload a project:

```sh
pio run --target upload
```

Monitor serial output:

```sh
pio device monitor --baud 115200
```

Run commands from the specific project directory, for example:

```sh
cd pool_temp_meter
pio run
```

## Changelog-lite

- Added workspace-level overview for the available applications.
