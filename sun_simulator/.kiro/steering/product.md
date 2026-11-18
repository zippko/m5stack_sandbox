# Product: sun_simulator

Sun-like lighting effect on an LED strip, driven by an M5Stack Atom Lite.

- Base layer: slowly drifting orange-yellow "photosphere" built from Perlin noise.
- Eruptions: bright, white-hot flares that expand, peak fast, then fade slowly. Up to 3 at once.
- Eruptions start at random intervals (3-10 s). The Atom Lite button (G39) triggers one manually.
- Hardware: WS2813 strip (30 LED/m, 1 m, 30 LEDs) on the Grove port (G32 data, 5V, GND).
- Standalone device. No Wi-Fi, display, or network features.
