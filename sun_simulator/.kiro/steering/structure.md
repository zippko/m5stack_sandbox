# Structure: sun_simulator

```
sun_simulator/
├── platformio.ini   # env m5stack-atom, pinned platform and FastLED
├── .gitignore       # ignores .pio and generated .vscode files
└── src/
    └── main.cpp     # whole application
```

`.pio/` is generated build output and downloaded libraries (including FastLED). Do not read, edit, or search it for project code.

## main.cpp layout
1. Header comment: purpose and wiring.
2. `static constexpr` configuration constants.
3. `Eruption` struct and static state (`leds`, `eruptions`, `nextEruptionAt`, `lastButton`).
4. Helpers: `startEruption`, `scheduleNext`, `updateEruptions` (physics/timing), `render` (drawing).
5. `setup()`: serial, button, FastLED init, power limit, RNG seeding.
6. `loop()`: frame gate, button edge, eruption scheduling, update, render, `FastLED.show()`.

Keep logic and drawing separate: `updateEruptions` changes state, `render` only draws from it.

## Docs
There is no README for this project yet. If you add one, keep it short: purpose, wiring, stack with versions, run commands.
