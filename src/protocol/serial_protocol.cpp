#include "protocol/serial_protocol.h"

#include <stdio.h>

#include <ArduinoJson.h>

// Line-delimited JSON on Serial at 115200 baud.
// One request line in, one response line out.
// A sequence request returns as soon as it is accepted. It does not wait for the sequence to finish.
// The line buffer is mutable so the handler can parse it in place.

namespace {

constexpr size_t kResponseCapacity =
    JSON_OBJECT_SIZE(3) +
    JSON_OBJECT_SIZE(3) +
    JSON_OBJECT_SIZE(4) +
    JSON_OBJECT_SIZE(2) +
    SerialProtocol::kMaxIdLength +
    128;

StaticJsonDocument<kResponseCapacity> responseDoc;

bool isBlank(const char* data, size_t length) {
    for (size_t index = 0; index < length; index++) {
        char c = data[index];
        if (c != ' ' && c != '\t' && c != '\r') {
            return false;
        }
    }
    return true;
}

void writeResponse(const CommandResult& result) {
    responseDoc.clear();
    if (result.idType == CommandResult::IdType::Text) {
        responseDoc["id"] = result.idText;
    } else if (result.idType == CommandResult::IdType::Number) {
        responseDoc["id"] = result.idNumber;
    } else {
        responseDoc["id"] = static_cast<const char*>(nullptr);
    }

    responseDoc["ok"] = result.ok;
    if (!result.ok) {
        responseDoc["error"] = result.error;
        serializeJson(responseDoc, Serial);
        Serial.write('\n');
        return;
    }

    JsonObject body = responseDoc.createNestedObject("result");
    switch (result.body) {
        case CommandResult::Body::SetLed:
            body["led"] = result.ledName;
            body["state"] = result.ledState;
            break;
        case CommandResult::Body::AllLeds: {
            JsonObject states = body.createNestedObject("states");
            for (uint8_t index = 0; index < CommandResult::kLedCount; index++) {
                states[result.ledNames[index]] = result.ledOn[index];
            }
            break;
        }
        case CommandResult::Body::Sequence:
            body["repeat"] = result.repeat;
            body["steps"] = result.steps;
            body["steps_remaining"] = result.stepsRemaining;
            break;
        case CommandResult::Body::Stopped:
            body["running"] = result.running;
            break;
        case CommandResult::Body::Status: {
            JsonObject leds = body.createNestedObject("leds");
            for (uint8_t index = 0; index < CommandResult::kLedCount; index++) {
                leds[result.ledNames[index]] = result.ledOn[index];
            }
            JsonObject sequenceState = body.createNestedObject("sequence");
            sequenceState["running"] = result.running;
            sequenceState["steps_remaining"] = result.stepsRemaining;
            break;
        }
        case CommandResult::Body::None:
            break;
    }

    serializeJson(responseDoc, Serial);
    Serial.write('\n');
}

}  // namespace

SerialProtocol::SerialProtocol()
    : callback_(nullptr),
      length_(0),
      overflow_(false) {
    buffer_[0] = '\0';
}

void SerialProtocol::begin() {
    // The default UART buffer is 256 bytes, smaller than the largest accepted line.
    Serial.setRxBufferSize(kMaxMessageBytes);
    Serial.begin(115200);
}

void SerialProtocol::setCallback(CommandResult (*callback)(char* json, size_t length)) {
    callback_ = callback;
}

void SerialProtocol::poll() {
    while (Serial.available() > 0) {
        int raw = Serial.read();
        if (raw < 0) {
            return;
        }

        char c = static_cast<char>(raw);
        if (c != '\n') {
            if (overflow_ || length_ >= kMaxMessageBytes) {
                overflow_ = true;
            } else {
                buffer_[length_++] = c;
            }
            continue;
        }

        if (length_ > 0 && buffer_[length_ - 1] == '\r') {
            length_--;
        }
        buffer_[length_] = '\0';

        bool lineOverflow = overflow_;
        size_t lineLength = length_;
        overflow_ = false;
        length_ = 0;

        if (lineOverflow) {
            CommandResult result;
            result.ok = false;
            snprintf(result.error, sizeof(result.error), "%s", "message too long");
            writeResponse(result);
        } else if (!isBlank(buffer_, lineLength)) {
            handleLine(lineLength);
        }

        // One request per loop so a running sequence keeps its timing.
        return;
    }
}

void SerialProtocol::handleLine(size_t length) {
    if (callback_ == nullptr) {
        return;
    }
    CommandResult result = callback_(buffer_, length);
    writeResponse(result);
}
