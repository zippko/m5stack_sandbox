// Sun simulator with eruptions.
// M5Stack Atom Lite + WS2813 strip (30 LED/m, 1 m) on the Grove port.
//
// Wiring (Grove / HY2.0-4P):
//   Yellow (G32)  -> strip DI (data)
//   Black         -> strip GND
//   Red (5V)      -> strip +5V
// Atom Lite button (G39): trigger an eruption manually.

#include <Arduino.h>
#include <FastLED.h>

static constexpr uint8_t  DATA_PIN       = 32;   // Grove yellow wire
static constexpr uint8_t  BUTTON_PIN     = 39;   // Atom Lite button, active low
static constexpr uint16_t NUM_LEDS       = 30;   // 30 LED/m * 1 m
static constexpr uint8_t  BRIGHTNESS     = 200;
static constexpr uint16_t MAX_MILLIAMPS  = 600;  // keeps load safe for USB supply
static constexpr uint8_t  MAX_ERUPTIONS  = 3;
static constexpr uint32_t FRAME_MS       = 16;   // ~60 fps

// Random eruption interval (ms)
static constexpr uint32_t ERUPT_MIN_MS   = 3000;
static constexpr uint32_t ERUPT_MAX_MS   = 10000;

struct Eruption {
  bool     active;
  float    center;     // LED position of the origin
  float    radius;     // current half-width in LEDs
  float    maxRadius;  // final half-width
  float    speed;      // LEDs per second
  float    energy;     // 0..1, decays after peak
  uint32_t startMs;
  uint32_t durationMs;
};

static CRGB      leds[NUM_LEDS];
static Eruption  eruptions[MAX_ERUPTIONS];
static uint32_t  nextEruptionAt = 0;
static bool      lastButton     = true;

static void startEruption(uint32_t now) {
  for (auto &e : eruptions) {
    if (e.active) continue;
    e.active     = true;
    e.center     = random(0, NUM_LEDS * 16) / 16.0f;
    e.radius     = 0.5f;
    e.maxRadius  = random(30, 90) / 10.0f;      // 3..9 LEDs
    e.speed      = random(60, 160) / 10.0f;     // 6..16 LEDs/s
    e.energy     = 1.0f;
    e.startMs    = now;
    e.durationMs = random(1200, 3000);
    return;
  }
}

static void scheduleNext(uint32_t now) {
  nextEruptionAt = now + random(ERUPT_MIN_MS, ERUPT_MAX_MS);
}

static void updateEruptions(uint32_t now, float dt) {
  for (auto &e : eruptions) {
    if (!e.active) continue;
    uint32_t age = now - e.startMs;
    if (age >= e.durationMs) { e.active = false; continue; }
    float t = (float)age / e.durationMs;          // 0..1
    if (e.radius < e.maxRadius) e.radius += e.speed * dt;
    // Fast rise, slow fade.
    e.energy = (t < 0.15f) ? (t / 0.15f) : (1.0f - (t - 0.15f) / 0.85f);
    e.energy = e.energy * e.energy;
  }
}

static void render(uint32_t now) {
  // Base photosphere: slowly drifting granulation, orange-yellow.
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t n = inoise8(i * 40, now / 6);
    uint8_t n2 = inoise8(i * 90 + 5000, now / 2);
    uint8_t heat = 120 + scale8(n, 70) + scale8(n2, 25);   // ~120..215
    // Map heat to a sun-like color: deep orange -> yellow, kept off pure red.
    leds[i] = CHSV(map(heat, 120, 215, 8, 32), 255 - (heat > 200 ? (heat - 200) * 6 : 0), heat + 30);
  }

  // Eruptions: bright, white-hot core with orange/red falloff, additive.
  for (auto &e : eruptions) {
    if (!e.active) continue;
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      float d = fabsf((float)i - e.center) / e.radius;
      if (d >= 1.0f) continue;
      float k = (1.0f - d);
      k = k * k * e.energy;                       // soft falloff
      // Flicker for turbulence.
      k *= 0.8f + 0.2f * (inoise8(i * 60, now / 3) / 255.0f);
      uint8_t v = (uint8_t)constrain(k * 255.0f, 0.0f, 255.0f);
      // Core goes white, edges stay orange.
      CRGB c = CHSV(20, 255 - scale8(v, 230), 255);
      leds[i] += c.nscale8_video(v);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  FastLED.clear(true);
  random16_add_entropy(esp_random());
  randomSeed(esp_random());
  scheduleNext(millis());
}

void loop() {
  static uint32_t lastFrame = 0;
  uint32_t now = millis();
  if (now - lastFrame < FRAME_MS) return;
  float dt = (now - lastFrame) / 1000.0f;
  lastFrame = now;

  bool btn = digitalRead(BUTTON_PIN);
  if (!btn && lastButton) startEruption(now);
  lastButton = btn;

  if (now >= nextEruptionAt) {
    startEruption(now);
    scheduleNext(now);
  }

  updateEruptions(now, dt);
  render(now);
  FastLED.show();
}
