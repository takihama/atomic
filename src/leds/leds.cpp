#include "leds/leds.h"

#include <string.h>

#include "hardware/pins.h"

namespace {

struct LedDescriptor {
    const char* name;
    uint8_t pin;
};

// Index must match the Led enum. GPIO numbers live only in pins.h.
const LedDescriptor kLeds[] = {
    {"green", Pins::LED_GREEN},
    {"red", Pins::LED_RED},
    {"yellow", Pins::LED_YELLOW},
    {"blue", Pins::LED_BLUE},
};

static_assert(sizeof(kLeds) / sizeof(kLeds[0]) == Leds::kLedCount, "LED table must match kLedCount");
static_assert(static_cast<uint8_t>(Led::Green) == 0, "Led::Green index");
static_assert(static_cast<uint8_t>(Led::Red) == 1, "Led::Red index");
static_assert(static_cast<uint8_t>(Led::Yellow) == 2, "Led::Yellow index");
static_assert(static_cast<uint8_t>(Led::Blue) == 3, "Led::Blue index");

bool isValid(Led led) {
    return static_cast<uint8_t>(led) < Leds::kLedCount;
}

}  // namespace

Leds::Leds() {
    for (uint8_t index = 0; index < kLedCount; index++) {
        states_[index] = false;
    }
}

void Leds::begin() {
    for (uint8_t index = 0; index < kLedCount; index++) {
        states_[index] = false;
        pinMode(kLeds[index].pin, OUTPUT);
        digitalWrite(kLeds[index].pin, LOW);
    }
}

bool Leds::set(Led led, bool on) {
    if (!isValid(led)) {
        return false;
    }

    uint8_t index = static_cast<uint8_t>(led);
    states_[index] = on;
    digitalWrite(kLeds[index].pin, on ? HIGH : LOW);
    return true;
}

bool Leds::get(Led led) const {
    if (!isValid(led)) {
        return false;
    }
    return states_[static_cast<uint8_t>(led)];
}

void Leds::setAll(const bool states[kLedCount]) {
    if (states == nullptr) {
        return;
    }

    for (uint8_t index = 0; index < kLedCount; index++) {
        set(static_cast<Led>(index), states[index]);
    }
}

void Leds::allOff() {
    bool states[kLedCount] = {};
    setAll(states);
}

bool Leds::tryParse(const char* name, Led& led) {
    if (name == nullptr) {
        return false;
    }

    for (uint8_t index = 0; index < kLedCount; index++) {
        if (strcmp(name, kLeds[index].name) == 0) {
            led = static_cast<Led>(index);
            return true;
        }
    }
    return false;
}

const char* Leds::nameOf(Led led) {
    if (!isValid(led)) {
        return nullptr;
    }
    return kLeds[static_cast<uint8_t>(led)].name;
}
