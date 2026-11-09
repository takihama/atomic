#include "command/command_handler.h"

#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#include "leds/leds.h"
#include "sequence/sequence_runner.h"

// Serial lines use "command" and "id". MQTT payloads use "cmd" and no id.
// Each message is interpreted only for the transport that received it.

namespace {

static_assert(CommandResult::kLedCount == Leds::kLedCount, "CommandResult LED slots must match Leds");
static_assert(CommandResult::kMaxIdLength == 64, "id error text says 64 characters");

constexpr size_t kSerialCapacity =
    JSON_OBJECT_SIZE(4) +
    JSON_ARRAY_SIZE(SequenceRunner::kMaxSteps) +
    SequenceRunner::kMaxSteps * (JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(Leds::kLedCount)) +
    3072;

constexpr size_t kMqttCapacity =
    JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(SequenceRunner::kMaxSteps) +
    SequenceRunner::kMaxSteps * (JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(Leds::kLedCount));

StaticJsonDocument<kSerialCapacity> serialDoc;
StaticJsonDocument<kMqttCapacity> mqttDoc;

const char* jsonErrorMessage(DeserializationError error) {
    if (error == DeserializationError::NoMemory) {
        return "message too large";
    }
    if (error == DeserializationError::TooDeep) {
        return "json too deeply nested";
    }
    return "invalid json";
}

void setError(CommandResult& result, const char* error) {
    result.ok = false;
    result.body = CommandResult::Body::None;
    snprintf(result.error, sizeof(result.error), "%s", error);
}

void fillLeds(CommandResult& result, const Leds& leds) {
    for (uint8_t index = 0; index < Leds::kLedCount; index++) {
        Led led = static_cast<Led>(index);
        result.ledNames[index] = Leds::nameOf(led);
        result.ledOn[index] = leds.get(led);
    }
}

bool readId(JsonObject object, CommandResult& result) {
    if (!object.containsKey("id")) {
        setError(result, "missing id");
        return false;
    }

    JsonVariant id = object["id"];
    if (id.is<const char*>()) {
        const char* text = id.as<const char*>();
        if (text == nullptr || strlen(text) > CommandResult::kMaxIdLength) {
            setError(result, "id must be at most 64 characters");
            return false;
        }
        memcpy(result.idText, text, strlen(text) + 1);
        result.idType = CommandResult::IdType::Text;
        return true;
    }

    if (id.is<long>()) {
        result.idNumber = id.as<long>();
        result.idType = CommandResult::IdType::Number;
        return true;
    }

    setError(result, "id must be a string or number");
    return false;
}

void handleSetLed(Leds& leds, JsonObject object, CommandResult& result) {
    if (!object.containsKey("led")) {
        setError(result, "missing led");
        return;
    }
    if (!object["led"].is<const char*>()) {
        setError(result, "led must be a string");
        return;
    }
    if (!object.containsKey("state")) {
        setError(result, "missing state");
        return;
    }
    if (!object["state"].is<bool>()) {
        setError(result, "state must be a boolean");
        return;
    }

    Led led;
    if (!Leds::tryParse(object["led"].as<const char*>(), led)) {
        setError(result, "unknown led");
        return;
    }

    bool state = object["state"].as<bool>();
    if (!leds.set(led, state)) {
        setError(result, "unknown led");
        return;
    }

    result.ok = true;
    result.body = CommandResult::Body::SetLed;
    result.ledName = Leds::nameOf(led);
    result.ledState = state;
}

void handleSetAllLeds(Leds& leds, JsonObject object, CommandResult& result) {
    if (!object.containsKey("states")) {
        setError(result, "missing states");
        return;
    }
    if (!object["states"].is<JsonObject>()) {
        setError(result, "states must be an object");
        return;
    }

    JsonObject states = object["states"].as<JsonObject>();
    for (JsonPair pair : states) {
        Led led;
        if (!Leds::tryParse(pair.key().c_str(), led)) {
            setError(result, "unknown led");
            return;
        }
    }

    bool values[Leds::kLedCount];
    for (uint8_t index = 0; index < Leds::kLedCount; index++) {
        Led led = static_cast<Led>(index);
        const char* name = Leds::nameOf(led);
        if (!states.containsKey(name)) {
            char message[64];
            snprintf(message, sizeof(message), "missing state for %s", name);
            setError(result, message);
            return;
        }
        if (!states[name].is<bool>()) {
            char message[64];
            snprintf(message, sizeof(message), "state for %s must be a boolean", name);
            setError(result, message);
            return;
        }
        values[index] = states[name].as<bool>();
    }

    leds.setAll(values);

    result.ok = true;
    result.body = CommandResult::Body::AllLeds;
    fillLeds(result, leds);
}

bool parseStep(JsonObject step, size_t stepIndex, SequenceStep& parsed, CommandResult& result) {
    unsigned humanIndex = static_cast<unsigned>(stepIndex + 1);
    char message[96];

    if (!step.containsKey("leds")) {
        snprintf(message, sizeof(message), "missing leds in step %u", humanIndex);
        setError(result, message);
        return false;
    }
    if (!step["leds"].is<JsonArray>()) {
        snprintf(message, sizeof(message), "leds in step %u must be an array", humanIndex);
        setError(result, message);
        return false;
    }

    JsonArray ledNames = step["leds"].as<JsonArray>();
    if (ledNames.size() > Leds::kLedCount) {
        snprintf(message, sizeof(message), "too many leds in step %u", humanIndex);
        setError(result, message);
        return false;
    }

    for (uint8_t index = 0; index < Leds::kLedCount; index++) {
        parsed.on[index] = false;
    }

    for (size_t index = 0; index < ledNames.size(); index++) {
        JsonVariant ledValue = ledNames[index];
        if (!ledValue.is<const char*>()) {
            snprintf(message, sizeof(message), "led in step %u must be a string", humanIndex);
            setError(result, message);
            return false;
        }

        Led led;
        if (!Leds::tryParse(ledValue.as<const char*>(), led)) {
            snprintf(message, sizeof(message), "unknown led in step %u", humanIndex);
            setError(result, message);
            return false;
        }

        uint8_t ledIndex = static_cast<uint8_t>(led);
        if (parsed.on[ledIndex]) {
            snprintf(message, sizeof(message), "duplicate led in step %u", humanIndex);
            setError(result, message);
            return false;
        }
        parsed.on[ledIndex] = true;
    }

    if (!step.containsKey("duration_ms")) {
        snprintf(message, sizeof(message), "missing duration_ms in step %u", humanIndex);
        setError(result, message);
        return false;
    }
    if (!step["duration_ms"].is<long>()) {
        snprintf(message, sizeof(message), "duration_ms in step %u must be an integer", humanIndex);
        setError(result, message);
        return false;
    }

    long durationMs = step["duration_ms"].as<long>();
    if (durationMs < SequenceRunner::kMinDurationMs || durationMs > SequenceRunner::kMaxDurationMs) {
        snprintf(message, sizeof(message), "duration_ms in step %u must be from %u to %u",
                 humanIndex,
                 static_cast<unsigned>(SequenceRunner::kMinDurationMs),
                 static_cast<unsigned>(SequenceRunner::kMaxDurationMs));
        setError(result, message);
        return false;
    }

    parsed.durationMs = static_cast<uint16_t>(durationMs);
    return true;
}

void handleRunSequence(SequenceRunner& sequence, JsonObject object, CommandResult& result) {
    if (!object.containsKey("repeat")) {
        setError(result, "missing repeat");
        return;
    }
    if (!object["repeat"].is<long>()) {
        setError(result, "repeat must be an integer");
        return;
    }

    long repeat = object["repeat"].as<long>();
    if (repeat < SequenceRunner::kMinRepeat || repeat > SequenceRunner::kMaxRepeat) {
        char message[64];
        snprintf(message, sizeof(message), "repeat must be from %u to %u",
                 static_cast<unsigned>(SequenceRunner::kMinRepeat),
                 static_cast<unsigned>(SequenceRunner::kMaxRepeat));
        setError(result, message);
        return;
    }

    if (!object.containsKey("steps")) {
        setError(result, "missing steps");
        return;
    }
    if (!object["steps"].is<JsonArray>()) {
        setError(result, "steps must be an array");
        return;
    }

    JsonArray steps = object["steps"].as<JsonArray>();
    if (steps.size() < 1 || steps.size() > SequenceRunner::kMaxSteps) {
        char message[64];
        snprintf(message, sizeof(message), "sequence must contain from 1 to %u steps",
                 static_cast<unsigned>(SequenceRunner::kMaxSteps));
        setError(result, message);
        return;
    }

    SequenceStep parsed[SequenceRunner::kMaxSteps];
    for (size_t index = 0; index < steps.size(); index++) {
        JsonVariant stepValue = steps[index];
        if (!stepValue.is<JsonObject>()) {
            char message[64];
            snprintf(message, sizeof(message), "step %u must be an object",
                     static_cast<unsigned>(index + 1));
            setError(result, message);
            return;
        }
        if (!parseStep(stepValue.as<JsonObject>(), index, parsed[index], result)) {
            return;
        }
    }

    if (!sequence.start(parsed, static_cast<uint8_t>(steps.size()), static_cast<uint8_t>(repeat))) {
        setError(result, "unable to start sequence");
        return;
    }

    result.ok = true;
    result.body = CommandResult::Body::Sequence;
    result.repeat = static_cast<uint8_t>(repeat);
    result.steps = static_cast<uint8_t>(steps.size());
    result.stepsRemaining = sequence.stepsRemaining();
}

void handleStopSequence(SequenceRunner& sequence, CommandResult& result) {
    sequence.stop();

    result.ok = true;
    result.body = CommandResult::Body::Stopped;
    result.running = false;
}

void handleGetStatus(const Leds& leds, const SequenceRunner& sequence, CommandResult& result) {
    result.ok = true;
    result.body = CommandResult::Body::Status;
    fillLeds(result, leds);
    result.running = sequence.isRunning();
    result.stepsRemaining = sequence.stepsRemaining();
}

void handleSerial(Leds& leds, SequenceRunner& sequence, char* json, size_t length, CommandResult& result) {
    serialDoc.clear();
    DeserializationError error = deserializeJson(serialDoc, json, length);
    if (error) {
        setError(result, jsonErrorMessage(error));
        return;
    }

    if (!serialDoc.is<JsonObject>()) {
        setError(result, "request must be an object");
        return;
    }

    JsonObject object = serialDoc.as<JsonObject>();
    if (!readId(object, result)) {
        return;
    }

    if (!object.containsKey("command")) {
        setError(result, "missing command");
        return;
    }
    if (!object["command"].is<const char*>()) {
        setError(result, "command must be a string");
        return;
    }

    const char* command = object["command"].as<const char*>();
    if (strcmp(command, "set_led") == 0) {
        handleSetLed(leds, object, result);
    } else if (strcmp(command, "set_all_leds") == 0) {
        handleSetAllLeds(leds, object, result);
    } else if (strcmp(command, "run_sequence") == 0) {
        handleRunSequence(sequence, object, result);
    } else if (strcmp(command, "stop_sequence") == 0) {
        handleStopSequence(sequence, result);
    } else if (strcmp(command, "get_status") == 0) {
        handleGetStatus(leds, sequence, result);
    } else {
        setError(result, "unknown command");
    }
}

bool parseMqttSequence(JsonObject object, SequenceStep* steps, uint8_t& stepCount, uint8_t& repeatCount) {
    if (!object["repeat"].is<long>() || !object["steps"].is<JsonArray>()) {
        return false;
    }

    long repeat = object["repeat"].as<long>();
    if (repeat < SequenceRunner::kMinRepeat || repeat > SequenceRunner::kMaxRepeat) {
        return false;
    }

    JsonArray jsonSteps = object["steps"].as<JsonArray>();
    if (jsonSteps.size() < 1 || jsonSteps.size() > SequenceRunner::kMaxSteps) {
        return false;
    }

    for (size_t index = 0; index < jsonSteps.size(); index++) {
        if (!jsonSteps[index].is<JsonObject>()) {
            return false;
        }
        JsonObject step = jsonSteps[index].as<JsonObject>();
        if (!step["durationMs"].is<long>() || !step["leds"].is<JsonObject>()) {
            return false;
        }

        long durationMs = step["durationMs"].as<long>();
        if (durationMs < SequenceRunner::kMinDurationMs ||
            durationMs > SequenceRunner::kMaxDurationMs) {
            return false;
        }

        bool seen[Leds::kLedCount] = {};
        bool on[Leds::kLedCount] = {};
        for (JsonPair pair : step["leds"].as<JsonObject>()) {
            if (!pair.value().is<bool>()) {
                return false;
            }
            Led led;
            if (!Leds::tryParse(pair.key().c_str(), led)) {
                return false;
            }
            uint8_t ledIndex = static_cast<uint8_t>(led);
            if (seen[ledIndex]) {
                return false;
            }
            seen[ledIndex] = true;
            on[ledIndex] = pair.value().as<bool>();
        }

        steps[index].durationMs = static_cast<uint16_t>(durationMs);
        for (uint8_t ledIndex = 0; ledIndex < Leds::kLedCount; ledIndex++) {
            steps[index].on[ledIndex] = on[ledIndex];
        }
    }

    stepCount = static_cast<uint8_t>(jsonSteps.size());
    repeatCount = static_cast<uint8_t>(repeat);
    return true;
}

void handleMqtt(Leds& leds, SequenceRunner& sequence, char* json, size_t length, CommandResult& result) {
    mqttDoc.clear();
    if (deserializeJson(mqttDoc, json, length) || !mqttDoc["cmd"].is<const char*>()) {
        return;
    }

    const char* cmd = mqttDoc["cmd"].as<const char*>();
    if (strcmp(cmd, "stop") == 0) {
        sequence.stop();
        result.ok = true;
        return;
    }
    if (strcmp(cmd, "led") == 0) {
        Led led;
        if (mqttDoc["led"].is<const char*>() && mqttDoc["on"].is<bool>() &&
            Leds::tryParse(mqttDoc["led"].as<const char*>(), led)) {
            sequence.cancel();
            leds.set(led, mqttDoc["on"].as<bool>());
            result.ok = true;
        }
        return;
    }
    if (strcmp(cmd, "sequence") == 0) {
        SequenceStep steps[SequenceRunner::kMaxSteps];
        uint8_t stepCount = 0;
        uint8_t repeatCount = 0;
        if (parseMqttSequence(mqttDoc.as<JsonObject>(), steps, stepCount, repeatCount) &&
            sequence.start(steps, stepCount, repeatCount)) {
            result.ok = true;
        }
    }
}

}  // namespace

CommandHandler::CommandHandler(Leds& leds, SequenceRunner& sequence)
    : leds_(leds),
      sequence_(sequence) {}

CommandResult CommandHandler::handle(char* json, size_t length, CommandSource source) {
    CommandResult result;
    if (json == nullptr) {
        return result;
    }
    if (source == CommandSource::Mqtt) {
        handleMqtt(leds_, sequence_, json, length, result);
    } else {
        handleSerial(leds_, sequence_, json, length, result);
    }
    return result;
}
