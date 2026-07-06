#include <Arduino.h>
#include <DallasTemperature.h>
#include <M5Core2.h>
#include <OneWire.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <lvgl.h>

#include "secrets.h"

#ifndef POOL_TEMP_LOGGING
#define POOL_TEMP_LOGGING 1
#endif

#if POOL_TEMP_LOGGING
#define LOG_LINE(message) Serial.println(message)
#define LOG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define LOG_LINE(message) do { } while (0)
#define LOG_PRINTF(...) do { } while (0)
#endif

namespace {
constexpr uint8_t kOneWirePin = 32;
constexpr uint8_t kTouchWakePin = 39;
constexpr uint32_t kMeasurementIntervalMs = 60UL * 60UL * 1000UL;
constexpr uint32_t kDisplayInactivityMs = 30UL * 1000UL;
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kMqttConnectTimeoutMs = 7000;
constexpr int32_t kScreenWidth = 240;
constexpr int32_t kScreenHeight = 320;
constexpr size_t kLvglBufferLines = 20;

OneWire oneWire(kOneWirePin);
DallasTemperature temperatureSensor(&oneWire);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

lv_disp_draw_buf_t drawBuffer;
lv_color_t drawBufferMemory[kScreenWidth * kLvglBufferLines];
lv_obj_t *temperatureLabel = nullptr;
lv_obj_t *wifiIndicator = nullptr;

bool displayInitialized = false;
bool displaySleeping = false;
bool displayModeActive = false;
bool lcdHardwareInitialized = false;
bool touchWakePending = false;
bool lastTemperatureValid = false;
RTC_DATA_ATTR bool wifiConnectionSuccessful = false;
float lastTemperatureC = DEVICE_DISCONNECTED_C;
char currentDisplayText[16] = "N/A";
uint32_t lastInteractionMs = 0;
uint32_t touchEnableAtMs = 0;

char deviceId[32];
char mqttClientId[64];
char stateTopic[96];
char availabilityTopic[96];
char discoveryTopic[128];
char discoveryPayload[1024];

void configureDeepSleepWakeSources() {
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(kMeasurementIntervalMs) * 1000ULL);
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(kTouchWakePin), 0);
}

void powerDownDisplayHardware() {
    // backlight rail
    M5.Axp.SetDCDC3(false);
    // LCD logic
    M5.Axp.SetLDOEnable(2, false);
}

void enterDeepSleep(const char *reason) {
    LOG_PRINTF("[%lu ms] Deep sleep: %s\r\n", millis(), reason);
    M5.Lcd.sleep();
    powerDownDisplayHardware();
    configureDeepSleepWakeSources();
    delay(100);
    esp_deep_sleep_start();
}

void formatTemperatureText(float temperatureC, bool valid, char *buffer, size_t bufferSize) {
    if (!valid) {
        snprintf(buffer, bufferSize, "N/A");
        return;
    }

    snprintf(buffer, bufferSize, "%.1f\xC2\xB0""C", temperatureC);
}

void formatTemperatureState(float temperatureC, bool valid, char *buffer, size_t bufferSize) {
    if (!valid) {
        snprintf(buffer, bufferSize, "nan");
        return;
    }

    snprintf(buffer, bufferSize, "%.1f", temperatureC);
}

void setTemperatureText(const char *text) {
    strncpy(currentDisplayText, text, sizeof(currentDisplayText) - 1);
    currentDisplayText[sizeof(currentDisplayText) - 1] = '\0';

    if (temperatureLabel) {
        lv_label_set_text(temperatureLabel, currentDisplayText);
        lv_obj_center(temperatureLabel);
    }
}

void setWifiIndicator(bool connected) {
    wifiConnectionSuccessful = connected;

    if (!wifiIndicator) {
        return;
    }

    const lv_color_t color = connected ? lv_color_hex(0x00C853) : lv_color_hex(0xD50000);
    lv_obj_set_style_bg_color(wifiIndicator, color, LV_PART_MAIN);
    lv_obj_set_style_border_color(wifiIndicator, color, LV_PART_MAIN);
    lv_obj_invalidate(wifiIndicator);
}

void refreshDisplayNow() {
    if (!displayInitialized || displaySleeping) {
        return;
    }

    lv_refr_now(nullptr);
}

void measureLocalTemperature() {
    const uint8_t probeCount = temperatureSensor.getDeviceCount();
    LOG_PRINTF("[%lu ms] Local temperature measurement started. Probe count: %u\r\n", millis(), probeCount);

    temperatureSensor.requestTemperatures();
    const float temperatureC = temperatureSensor.getTempCByIndex(0);

    if (probeCount == 0 || temperatureC == DEVICE_DISCONNECTED_C || !isfinite(temperatureC)) {
        lastTemperatureValid = false;
        lastTemperatureC = DEVICE_DISCONNECTED_C;
        formatTemperatureText(DEVICE_DISCONNECTED_C, false, currentDisplayText, sizeof(currentDisplayText));
        LOG_PRINTF("[%lu ms] Local temperature read failed. Showing N/A.\r\n", millis());
        return;
    }

    lastTemperatureValid = true;
    lastTemperatureC = temperatureC;
    formatTemperatureText(temperatureC, true, currentDisplayText, sizeof(currentDisplayText));
    LOG_PRINTF("[%lu ms] Local temperature read OK: %.2f C.\r\n", millis(), temperatureC);
}

void publishAvailability(bool online) {
    if (!mqttClient.connected()) {
        return;
    }

    mqttClient.publish(availabilityTopic, online ? "online" : "offline", true);
}

void publishTemperatureState() {
    if (!mqttClient.connected()) {
        return;
    }

    if (!lastTemperatureValid) {
        LOG_PRINTF("[%lu ms] MQTT state skipped: invalid temperature.\r\n", millis());
        return;
    }

    char stateText[16];
    formatTemperatureState(lastTemperatureC, lastTemperatureValid, stateText, sizeof(stateText));
    mqttClient.publish(stateTopic, stateText, true);
    LOG_PRINTF("[%lu ms] MQTT state published: %s -> %s\r\n", millis(), stateTopic, stateText);
}

void publishDiscovery() {
    if (!mqttClient.connected()) {
        return;
    }

    snprintf(discoveryPayload, sizeof(discoveryPayload),
             "{"
             "\"name\":\"Pool Temperature\","
             "\"object_id\":\"pool_temperature\","
             "\"unique_id\":\"%s_temperature\","
             "\"device_class\":\"temperature\","
             "\"state_class\":\"measurement\","
             "\"unit_of_measurement\":\"\\u00b0C\","
             "\"suggested_display_precision\":1,"
             "\"state_topic\":\"%s\","
             "\"availability_topic\":\"%s\","
             "\"payload_available\":\"online\","
             "\"payload_not_available\":\"offline\","
             "\"device\":{"
                 "\"identifiers\":[\"%s\"],"
                 "\"name\":\"Pool Temperature Meter\","
                 "\"manufacturer\":\"M5Stack\","
                 "\"model\":\"Core2\","
                 "\"sw_version\":\"1.0.0\""
             "}"
             "}",
             deviceId, stateTopic, availabilityTopic, deviceId);

    const bool publishOk = mqttClient.publish(discoveryTopic, discoveryPayload, true);
    LOG_PRINTF("[%lu ms] MQTT discovery publish: %s (%s)\r\n", millis(), discoveryTopic, publishOk ? "OK" : "FAILED");
}

void initTopics() {
    const uint64_t chipId = ESP.getEfuseMac();
    snprintf(deviceId, sizeof(deviceId), "pool_temp_meter_%012llX", static_cast<unsigned long long>(chipId));
    snprintf(mqttClientId, sizeof(mqttClientId), "%s_client", deviceId);
    snprintf(stateTopic, sizeof(stateTopic), "%s/%s/state", MQTT_BASE_TOPIC, deviceId);
    snprintf(availabilityTopic, sizeof(availabilityTopic), "%s/%s/status", MQTT_BASE_TOPIC, deviceId);
    snprintf(discoveryTopic, sizeof(discoveryTopic), "homeassistant/sensor/%s/config", deviceId);
}

void displayTemperature() {
    if (!displayInitialized) {
        return;
    }

    if (!displaySleeping) {
        lv_label_set_text(temperatureLabel, currentDisplayText);
        lv_obj_center(temperatureLabel);
        lv_obj_invalidate(temperatureLabel);
    }
}

void wakeDisplayIfNeeded() {
    if (!displayInitialized || !displaySleeping) {
        return;
    }

    M5.Lcd.wakeup();
    M5.Lcd.fillScreen(TFT_BLACK);
    displaySleeping = false;
    setWifiIndicator(wifiConnectionSuccessful);
    displayTemperature();
    refreshDisplayNow();
    LOG_PRINTF("[%lu ms] Display woke up.\r\n", millis());
}

void sleepDisplay() {
    if (!displayInitialized || displaySleeping) {
        return;
    }

    M5.Lcd.sleep();
    displaySleeping = true;
    LOG_PRINTF("[%lu ms] Display sleeping.\r\n", millis());
}

void initDisplay() {
    lv_init();
    lv_disp_draw_buf_init(&drawBuffer, drawBufferMemory, nullptr, kScreenWidth * kLvglBufferLines);

    static lv_disp_drv_t displayDriver;
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = kScreenWidth;
    displayDriver.ver_res = kScreenHeight;
    displayDriver.flush_cb = [](lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *colorBuffer) {
        const uint32_t width = area->x2 - area->x1 + 1;
        const uint32_t height = area->y2 - area->y1 + 1;
        M5.Lcd.startWrite();
        M5.Lcd.setAddrWindow(area->x1, area->y1, width, height);
        M5.Lcd.pushColors(reinterpret_cast<uint16_t *>(&colorBuffer->full), width * height, true);
        M5.Lcd.endWrite();
        lv_disp_flush_ready(disp);
    };
    displayDriver.draw_buf = &drawBuffer;
    lv_disp_drv_register(&displayDriver);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

    temperatureLabel = lv_label_create(lv_scr_act());
    lv_obj_set_width(temperatureLabel, kScreenWidth);
    lv_obj_set_style_text_font(temperatureLabel, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(temperatureLabel, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_MAIN);
    lv_obj_set_style_text_align(temperatureLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    wifiIndicator = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(wifiIndicator);
    lv_obj_set_size(wifiIndicator, 12, 12);
    lv_obj_set_pos(wifiIndicator, 8, 8);
    lv_obj_clear_flag(wifiIndicator, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(wifiIndicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(wifiIndicator, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(wifiIndicator, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(wifiIndicator, LV_OPA_COVER, LV_PART_MAIN);
    setWifiIndicator(wifiConnectionSuccessful);

    displayInitialized = true;
    LOG_LINE("LVGL initialized.");
}

void beginDisplayHardware() {
    if (lcdHardwareInitialized) {
        return;
    }

    M5.Axp.SetDCDC3(true);
    M5.Axp.SetLDOEnable(2, true);
    M5.Lcd.wakeup();
    M5.Lcd.begin();
    lcdHardwareInitialized = true;
    LOG_LINE("LCD hardware initialized.");
}

bool connectWifiBlocking() {
    LOG_PRINTF("[%lu ms] Connecting to Wi-Fi SSID '%s'...\r\n", millis(), WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < kWifiConnectTimeoutMs) {
        delay(250);
        M5.update();
    }

    if (WiFi.status() == WL_CONNECTED) {
        uint32_t settleStartMs = millis();
        while (millis() - settleStartMs < 500 && WiFi.status() != WL_CONNECTED) {
            delay(50);
            M5.update();
        }
        const String ipAddress = WiFi.localIP().toString();
        LOG_PRINTF("[%lu ms] Wi-Fi connected. IP: %s\r\n", millis(), ipAddress.c_str());
        return true;
    } else {
        LOG_PRINTF("[%lu ms] Wi-Fi connect timed out.\r\n", millis());
        return false;
    }
}

bool connectMqttBlocking() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setBufferSize(1024);

    LOG_PRINTF("[%lu ms] Connecting to MQTT broker '%s:%u'...\r\n", millis(), MQTT_SERVER, MQTT_PORT);
    const uint32_t startMs = millis();
    while (!mqttClient.connected() && millis() - startMs < kMqttConnectTimeoutMs) {
        const bool connected = mqttClient.connect(
            mqttClientId,
            MQTT_USERNAME,
            MQTT_PASSWORD);

        if (connected) {
            LOG_PRINTF("[%lu ms] MQTT connection established.\r\n", millis());
            return true;
        }
        
        M5.update();
    }

    LOG_PRINTF("[%lu ms] MQTT connect timed out. state=%d\r\n", millis(), mqttClient.state());
    return false;
}

bool measureAndPublish() {
    const uint8_t probeCount = temperatureSensor.getDeviceCount();
    LOG_PRINTF("[%lu ms] Temperature refresh started. Probe count: %u\r\n", millis(), probeCount);

    temperatureSensor.requestTemperatures();
    const float temperatureC = temperatureSensor.getTempCByIndex(0);

    if (probeCount == 0 || temperatureC == DEVICE_DISCONNECTED_C || !isfinite(temperatureC)) {
        lastTemperatureValid = false;
        lastTemperatureC = DEVICE_DISCONNECTED_C;
        LOG_PRINTF("[%lu ms] Temperature read failed. Displaying N/A.\r\n", millis());
    } else {
        lastTemperatureValid = true;
        lastTemperatureC = temperatureC;
        LOG_PRINTF("[%lu ms] Temperature read OK: %.2f C.\r\n", millis(), temperatureC);
    }

    bool wifiConnected = WiFi.status() == WL_CONNECTED;

    if (!wifiConnected) {
        wifiConnected = connectWifiBlocking();
    }

    if (wifiConnected && connectMqttBlocking()) {
        publishAvailability(lastTemperatureValid);
        publishDiscovery();
        publishTemperatureState();
        mqttClient.loop();
    }
    return wifiConnected;
}

void runMeasurementCycle() {
    const bool wifiConnected = measureAndPublish();
    setWifiIndicator(wifiConnected);
    enterDeepSleep("measurement cycle complete");
}

void runDisplayMode() {
    measureLocalTemperature();
    beginDisplayHardware();
    M5.Lcd.setRotation(2);
    initDisplay();
    M5.Lcd.fillScreen(TFT_BLACK);
    setTemperatureText(currentDisplayText);
    displayTemperature();
    refreshDisplayNow();
    const bool wifiConnected = measureAndPublish();
    setWifiIndicator(wifiConnected);
    refreshDisplayNow();
    displayModeActive = true;
    lastInteractionMs = millis();
    LOG_PRINTF("[%lu ms] Display mode active. Current value: %s\r\n", millis(), currentDisplayText);
}

void runTouchWakeMode() {
    measureLocalTemperature();
    displayModeActive = true;
    displayInitialized = false;
    displaySleeping = false;
    touchWakePending = true;
    touchEnableAtMs = millis() + 300;
    lastInteractionMs = millis();
    M5.Lcd.sleep();
    LOG_PRINTF("[%lu ms] Touch wake mode armed.\r\n");
}

void startDisplayForTouch() {
    beginDisplayHardware();
    M5.Lcd.setRotation(2);
    initDisplay();
    M5.Lcd.fillScreen(TFT_BLACK);
    setTemperatureText(currentDisplayText);
    displayTemperature();
    refreshDisplayNow();
    displaySleeping = false;
    lastInteractionMs = millis();
    touchWakePending = false;
    LOG_PRINTF("[%lu ms] Display enabled after touch. Current value: %s\r\n", millis(), currentDisplayText);
}

bool touchDetected() {
    return M5.Touch.ispressed();
}

bool touchReady() {
    return millis() >= touchEnableAtMs;
}
}  // namespace

void setup() {
    M5.begin(false, false, POOL_TEMP_LOGGING, false);
    delay(100);

    initTopics();

    temperatureSensor.begin();
    temperatureSensor.setResolution(12);

    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    LOG_PRINTF("[%lu ms] Wake cause: %d\r\n", millis(), static_cast<int>(wakeCause));

    if (wakeCause == ESP_SLEEP_WAKEUP_TIMER) {
        runMeasurementCycle();
        return;
    }

    if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) {
        runTouchWakeMode();
        return;
    }

    runDisplayMode();
}

void loop() {
    M5.update();

    if (displayModeActive) {
        if (!displayInitialized) {
            if (touchWakePending && touchReady()) {
                LOG_PRINTF("[%lu ms] Touch wake ready, starting display.\r\n", millis());
                startDisplayForTouch();
            }
        } else if (touchDetected()) {
            wakeDisplayIfNeeded();
            lastInteractionMs = millis();
        }

        if (!displaySleeping && millis() - lastInteractionMs >= kDisplayInactivityMs) {
            sleepDisplay();
            enterDeepSleep("display inactivity timeout");
        }

        if (!displaySleeping) {
            lv_timer_handler();
        }
    }

    delay(20);
}
