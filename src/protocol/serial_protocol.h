#pragma once

#include <Arduino.h>

#include "command/command_handler.h"

class SerialProtocol {
 public:
    static constexpr size_t kMaxMessageBytes = 4096;
    static constexpr size_t kMaxIdLength = CommandResult::kMaxIdLength;

    SerialProtocol();

    void begin();
    // One JSON command line. The buffer is mutable. The result is written as the response line.
    void setCallback(CommandResult (*callback)(char* json, size_t length));
    // Reads complete lines without blocking. Handles at most one request per call.
    void poll();

 private:
    void handleLine(size_t length);

    CommandResult (*callback_)(char* json, size_t length);
    char buffer_[kMaxMessageBytes + 1];
    size_t length_;
    bool overflow_;
};
