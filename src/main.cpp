
#include <Arduino.h>
#include <stdint.h>


enum class LedState : uint8_t {
    Off,
    On
};

struct AppConfig {
    static constexpr uint8_t LED_PIN = 2;
    static constexpr uint32_t BLINK_INTERVAL_MS = 500U;
};

class Led {
public:
    explicit Led(uint8_t pin)
        : pin_(pin) {}

    void init() {
        pinMode(pin_, OUTPUT);
        set(LedState::Off);
    }

    void set(LedState state) {
        digitalWrite(
            pin_,
            state == LedState::On ? HIGH : LOW
        );
    }

private:
    uint8_t pin_;
};

class BlinkController {
public:
    BlinkController()
        : led_(AppConfig::LED_PIN),
          state_(LedState::Off),
          lastToggleMs_(0U) {}

    void init() {
        led_.init();
        lastToggleMs_ = millis();
    }

    void update() {
        const uint32_t now =
            static_cast<uint32_t>(millis());

        if (now - lastToggleMs_
            >= AppConfig::BLINK_INTERVAL_MS) {

            lastToggleMs_ = now;

            state_ = (state_ == LedState::On)
                ? LedState::Off
                : LedState::On;

            led_.set(state_);
        }
    }

private:
    Led led_;
    LedState state_;
    uint32_t lastToggleMs_;
};

// Єдиний екземпляр контролера зі статичним часом життя.
BlinkController& controller() {
    static BlinkController instance;
    return instance;
}

void setup() {
    controller().init();
}

void loop() {
    controller().update();
}
