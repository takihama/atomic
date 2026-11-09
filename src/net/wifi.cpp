#include "net/wifi.h"
#include "wifi_credentials.h"

#include <stdio.h>

#include <WiFi.h>

void Wifi::begin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint8_t mac[6] = {};
    WiFi.macAddress(mac);
    snprintf(id_, sizeof(id_), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.printf("Device %s\n", id_);
}

bool Wifi::connected() const {
    return WiFi.status() == WL_CONNECTED && static_cast<uint32_t>(WiFi.localIP()) != 0;
}

const char* Wifi::deviceId() const {
    return id_;
}
