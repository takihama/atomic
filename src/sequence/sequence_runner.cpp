#include "sequence/sequence_runner.h"

static_assert(
    static_cast<uint32_t>(SequenceRunner::kMaxSteps) * SequenceRunner::kMaxRepeat <= 65535u,
    "steps remaining must fit in uint16_t");

SequenceRunner::SequenceRunner(Leds& leds)
    : leds_(leds),
      stepCount_(0),
      repeatCount_(0),
      repeatIndex_(0),
      stepIndex_(0),
      stepStartedAtMs_(0),
      running_(false) {}

bool SequenceRunner::start(const SequenceStep* steps, uint8_t stepCount, uint8_t repeatCount) {
    if (steps == nullptr || stepCount == 0 || stepCount > kMaxSteps) {
        return false;
    }
    if (repeatCount < kMinRepeat || repeatCount > kMaxRepeat) {
        return false;
    }

    for (uint8_t index = 0; index < stepCount; index++) {
        uint16_t durationMs = steps[index].durationMs;
        if (durationMs < kMinDurationMs || durationMs > kMaxDurationMs) {
            return false;
        }
    }

    for (uint8_t index = 0; index < stepCount; index++) {
        steps_[index] = steps[index];
    }

    stepCount_ = stepCount;
    repeatCount_ = repeatCount;
    repeatIndex_ = 0;
    stepIndex_ = 0;
    running_ = true;
    applyCurrentStep();
    stepStartedAtMs_ = millis();
    return true;
}

void SequenceRunner::cancel() {
    running_ = false;
    stepCount_ = 0;
    repeatCount_ = 0;
    repeatIndex_ = 0;
    stepIndex_ = 0;
    stepStartedAtMs_ = 0;
}

void SequenceRunner::stop() {
    cancel();
    leds_.allOff();
}

void SequenceRunner::update() {
    if (!running_) {
        return;
    }

    // Unsigned subtraction stays correct when millis() wraps.
    unsigned long elapsed = millis() - stepStartedAtMs_;
    if (elapsed < steps_[stepIndex_].durationMs) {
        return;
    }

    stepIndex_++;

    if (stepIndex_ >= stepCount_) {
        stepIndex_ = 0;
        repeatIndex_++;
        if (repeatIndex_ >= repeatCount_) {
            running_ = false;
            // The last step stays on the LEDs.
            return;
        }
    }

    applyCurrentStep();
    stepStartedAtMs_ = millis();
}

bool SequenceRunner::isRunning() const {
    return running_;
}

uint16_t SequenceRunner::stepsRemaining() const {
    if (!running_) {
        return 0;
    }

    // The current step is still remaining; update() has not advanced past it.
    uint16_t total = static_cast<uint16_t>(stepCount_) * static_cast<uint16_t>(repeatCount_);
    uint16_t completed = static_cast<uint16_t>(repeatIndex_) * static_cast<uint16_t>(stepCount_) +
                         stepIndex_;
    if (completed >= total) {
        return 0;
    }
    return total - completed;
}

void SequenceRunner::applyCurrentStep() {
    leds_.setAll(steps_[stepIndex_].on);
}
