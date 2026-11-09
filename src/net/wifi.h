#pragma once

#include <Arduino.h>

// Joins the LAN as a station. MQTT reads the device id from here.
class Wifi {
 public:
    void begin();
    bool connected() const;
    const char* deviceId() const;

 private:
    char id_[13] = {};
};
