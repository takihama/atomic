#pragma once

#include <Arduino.h>

#include "leds/leds.h"

struct SequenceStep {
    bool on[Leds::kLedCount];
    uint16_t durationMs;
};

class SequenceRunner {
 public:
    static constexpr uint8_t kMinRepeat = 1;
    static constexpr uint8_t kMaxRepeat = 20;
    static constexpr uint8_t kMaxSteps = 50;
    static constexpr uint16_t kMinDurationMs = 50;
    static constexpr uint16_t kMaxDurationMs = 10000;

    explicit SequenceRunner(Leds& leds);

    // Copies steps and shows the first one immediately. Replaces a running sequence.
    bool start(const SequenceStep* steps, uint8_t stepCount, uint8_t repeatCount);
    // Stops the sequence, if any, and turns every LED off.
    void stop();
    // Drops a running sequence and leaves the LED pins as they are.
    void cancel();
    // Advances the sequence from millis(). Call this on every loop iteration.
    void update();

    bool isRunning() const;
    uint16_t stepsRemaining() const;

 private:
    void applyCurrentStep();

    Leds& leds_;
    SequenceStep steps_[kMaxSteps];
    uint8_t stepCount_;
    uint8_t repeatCount_;
    uint8_t repeatIndex_;
    uint8_t stepIndex_;
    unsigned long stepStartedAtMs_;
    bool running_;
};
