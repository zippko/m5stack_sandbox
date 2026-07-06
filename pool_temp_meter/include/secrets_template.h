#pragma once

// Local-only secrets file for Wi-Fi and MQTT settings.
// Replace the placeholder values before deploying to hardware.
// Rename to secrets.h

#define WIFI_SSID "ssid"
#define WIFI_PASSWORD "password"

#define MQTT_SERVER "X.X.X.X"
#define MQTT_PORT 1883
#define MQTT_USERNAME "user"
#define MQTT_PASSWORD "password"
#define MQTT_BASE_TOPIC "pool_temp_meter"
