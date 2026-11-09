#include <Arduino.h>
#include <string.h>

#include "command/command_handler.h"
#include "leds/leds.h"
#include "net/mqtt.h"
#include "net/wifi.h"
#include "protocol/serial_protocol.h"
#include "sequence/sequence_runner.h"

// Declaration order matters: the runner keeps a reference to leds.
// Serial starts before Wi-Fi so boot logs are visible. MQTT reads Wifi.
// setup() registers both transports on commandHandler.
namespace {

constexpr size_t decimalDigits(uint32_t value) {
    return value < 10 ? 1 : 1 + decimalDigits(value / 10);
}

// Compact sequence at the runner limits. false is longer than true, and every LED is present.
constexpr size_t kRepeatDigits = decimalDigits(SequenceRunner::kMaxRepeat);
constexpr size_t kDurationDigits = decimalDigits(SequenceRunner::kMaxDurationMs);
constexpr size_t kFalseLedsBytes =
    sizeof("\"green\":false,\"red\":false,\"yellow\":false,\"blue\":false") - 1;
constexpr size_t kMaxStepBytes = sizeof("{\"durationMs\":") - 1 + kDurationDigits +
                                  sizeof(",\"leds\":{") - 1 + kFalseLedsBytes +
                                  sizeof("}}") - 1;
constexpr size_t kMaxSequencePayloadBytes =
    sizeof("{\"cmd\":\"sequence\",\"repeat\":") - 1 + kRepeatDigits + sizeof(",\"steps\":[") - 1 +
    SequenceRunner::kMaxSteps * kMaxStepBytes + (SequenceRunner::kMaxSteps - 1) +
    sizeof("]}") - 1;

// QoS 0 PUBLISH for atomic/<12 hex digits>/cmd plus that payload.
constexpr size_t kCommandTopicBytes = sizeof("atomic/") - 1 + 12 + sizeof("/cmd") - 1;

constexpr size_t mqttRemainingLengthBytes(size_t remaining) {
    return remaining < 128 ? 1 : remaining < 16384 ? 2 : remaining < 2097152 ? 3 : 4;
}

constexpr size_t kMaxSequenceRemainingBytes = 2 + kCommandTopicBytes + kMaxSequencePayloadBytes;
constexpr size_t kMaxSequencePacketBytes =
    1 + mqttRemainingLengthBytes(kMaxSequenceRemainingBytes) + kMaxSequenceRemainingBytes;

static_assert(MQTT_MAX_PACKET_SIZE == kMaxSequencePacketBytes,
              "MQTT_MAX_PACKET_SIZE must hold one sequence at the runner limits");

// Mutable so ArduinoJson can point into it instead of copying strings.
char commandBody[kMaxSequencePayloadBytes + 1];

Leds leds;
SequenceRunner sequenceRunner(leds);
CommandHandler commandHandler(leds, sequenceRunner);
Wifi wifi;
Mqtt mqtt(wifi);
SerialProtocol serialProtocol;

CommandResult handleSerialCommand(char* json, size_t length) {
    return commandHandler.handle(json, length, CommandSource::Serial);
}

void handleMqttCommand(char*, uint8_t* payload, unsigned int length) {
    if (payload == nullptr || length == 0 || length > kMaxSequencePayloadBytes) {
        Serial.println("MQTT command ignored");
        return;
    }

    memcpy(commandBody, payload, length);
    commandBody[length] = '\0';

    CommandResult result = commandHandler.handle(commandBody, length, CommandSource::Mqtt);
    if (!result.ok) {
        Serial.println("MQTT command ignored");
    }
}

}  // namespace

void setup() {
    leds.begin();
    serialProtocol.begin();
    wifi.begin();
    serialProtocol.setCallback(handleSerialCommand);
    mqtt.setCallback(handleMqttCommand);
}

void loop() {
    serialProtocol.poll();
    sequenceRunner.update();
    mqtt.update();
}
