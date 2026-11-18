# Tech: sun_simulator

## Stack
- Build system: PlatformIO (project dir: `sun_simulator/`)
- Platform: `espressif32@6.12.0` (pinned)
- Board: `m5stack-atom` (env `m5stack-atom`)
- Framework: Arduino
- Library: `fastled/FastLED@3.9.20` (pinned). Pin any new library to an exact version.
- Serial monitor: 115200 baud

## Commands
Run from `sun_simulator/`:
```sh
pio run                          # build
pio run --target upload          # flash
pio device monitor --baud 115200 # serial output
```
No tests exist. Do not add any unless asked.

## Code conventions
- Single file: `src/main.cpp`. Keep it that way unless the user asks to split it.
- Arduino/C++ style, 2-space indent. Put the header comment (purpose, wiring) at the top of the file.
- Tunables are `static constexpr` constants near the top, with a short unit/range comment (for example `// ~60 fps`).
- Module-level state and helpers are `static`.
- Animation is non-blocking and timed with `millis()`. Never use `delay()` in `loop()`. Frame rate is gated by `FRAME_MS` and `dt` is in seconds.
- Use FastLED helpers (`inoise8`, `scale8`, `CHSV`, `CRGB`, `nscale8_video`). Avoid per-frame heap allocation.
- Seed randomness with `esp_random()`.

## Hardware rules
- Do not remove or raise `FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS)` (600 mA) without asking. It protects the USB supply.
- If you change the pin, LED count, or LED type, update the wiring comment in `main.cpp` and `platformio.ini`.
- The button on G39 is active low and has no internal pull-up. Read it with `INPUT`.
- No secrets or credentials are used here.
