#pragma once

#include <Arduino.h>

enum class Led : uint8_t {
    Green = 0,
    Red = 1,
    Yellow = 2,
    Blue = 3
};

class Leds {
 public:
    static constexpr uint8_t kLedCount = 4;

    Leds();

    void begin();
    bool set(Led led, bool on);
    bool get(Led led) const;
    void setAll(const bool states[kLedCount]);
    void allOff();

    static bool tryParse(const char* name, Led& led);
    static const char* nameOf(Led led);

 private:
    bool states_[kLedCount];
};
