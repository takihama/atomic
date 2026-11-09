#pragma once

#include <Arduino.h>

class Leds;
class SequenceRunner;

enum class CommandSource : uint8_t { Serial, Mqtt };

// Filled by CommandHandler. SerialProtocol prints it; MQTT only checks ok.
struct CommandResult {
    enum class IdType : uint8_t { Null, Text, Number };
    enum class Body : uint8_t { None, SetLed, AllLeds, Sequence, Stopped, Status };

    static constexpr size_t kMaxIdLength = 64;
    static constexpr size_t kMaxErrorLength = 96;
    static constexpr uint8_t kLedCount = 4;

    IdType idType = IdType::Null;
    char idText[kMaxIdLength + 1] = {};
    long idNumber = 0;

    bool ok = false;
    char error[kMaxErrorLength] = {};
    Body body = Body::None;

    const char* ledName = nullptr;
    bool ledState = false;
    const char* ledNames[kLedCount] = {};
    bool ledOn[kLedCount] = {};
    uint8_t repeat = 0;
    uint8_t steps = 0;
    uint16_t stepsRemaining = 0;
    bool running = false;
};

// Executes LED and sequence commands for both transports.
// Does not write to Serial.
class CommandHandler {
 public:
    CommandHandler(Leds& leds, SequenceRunner& sequence);

    // json is mutable: ArduinoJson parses it in place.
    CommandResult handle(char* json, size_t length, CommandSource source);

 private:
    Leds& leds_;
    SequenceRunner& sequence_;
};
