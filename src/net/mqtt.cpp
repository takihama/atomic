#include "net/mqtt.h"

#include <stdio.h>

namespace {

constexpr const char* MQTT_HOST = "test.mosquitto.org";
constexpr uint16_t MQTT_PORT = 1883;
constexpr unsigned long kRetryMs = 2000;

}

Mqtt::Mqtt(Wifi& wifi) : wifi_(wifi), client_(wifiClient_) {
    client_.setServer(MQTT_HOST, MQTT_PORT);
    // connect() blocks. Two seconds fails a dead broker without stalling the loop.
    client_.setSocketTimeout(2);
}

void Mqtt::setCallback(void (*callback)(char* topic, uint8_t* payload, unsigned int length)) {
    client_.setCallback(callback);
}

bool Mqtt::publish(const char* topic, const char* payload) {
    if (topic == nullptr || payload == nullptr) {
        return false;
    }
    return client_.publish(topic, payload);
}

void Mqtt::update() {
    if (!wifi_.connected()) {
        loggedSta_ = false;
        if (client_.connected()) {
            client_.disconnect();
        }
        return;
    }

    if (!loggedSta_) {
        Serial.printf("\nWiFi connected %s %s\n", WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str());
        loggedSta_ = true;
    }

    if (client_.connected()) {
        client_.loop();
        return;
    }

    unsigned long now = millis();
    if (lastAttemptMs_ != 0 && now - lastAttemptMs_ < kRetryMs) {
        return;
    }
    lastAttemptMs_ = now;

    if (!client_.connect(wifi_.deviceId())) {
        Serial.printf("MQTT connect failed (%d)\n", client_.state());
        return;
    }

    Serial.printf("MQTT connected to %s:%u\n", MQTT_HOST, MQTT_PORT);

    char topic[24];
    snprintf(topic, sizeof(topic), "atomic/%s/cmd", wifi_.deviceId());
    if (!client_.subscribe(topic)) {
        Serial.println("MQTT subscribe failed");
    }
}
