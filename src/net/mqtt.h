#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "net/wifi.h"

// Connects, reconnects, and subscribes to atomic/<id>/cmd.
// Payloads are delivered to the one callback registered with setCallback.
// The device id comes from Wifi.
class Mqtt {
 public:
    explicit Mqtt(Wifi& wifi);

    void setCallback(void (*callback)(char* topic, uint8_t* payload, unsigned int length));
    bool publish(const char* topic, const char* payload);
    void update();

 private:
    Wifi& wifi_;
    WiFiClient wifiClient_;
    PubSubClient client_;

    unsigned long lastAttemptMs_ = 0;
    bool loggedSta_ = false;
};
