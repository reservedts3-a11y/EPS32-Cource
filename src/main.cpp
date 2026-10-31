#include <Arduino.h>

constexpr uint8_t BUTTON_PIN = 18;

volatile uint32_t interruptCount = 0;
volatile uint32_t firstInterruptUs = 0;
volatile bool eventPending = false;

void IRAM_ATTR buttonISR() {
    const uint32_t now = micros();

    interruptCount++;

    // Запам'ятовуємо час першої події
    // до того, як loop() забере прапорець.
    if (!eventPending) {
        firstInterruptUs = now;
        eventPending = true;
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(BUTTON_PIN),
        buttonISR,
        FALLING
    );
}

void loop() {
    bool hasEvent;
    uint32_t eventUs;
    uint32_t count;

    // Безпечне копіювання спільних змінних
    noInterrupts();

    hasEvent = eventPending;
    eventUs = firstInterruptUs;
    count = interruptCount;
    eventPending = false;

    interrupts();

    if (hasEvent) {
        const uint32_t handledUs = micros();
        const uint32_t latencyUs = handledUs - eventUs;

        Serial.print("Interrupt count: ");
        Serial.println(count);

        Serial.print("Latency: ");
        Serial.print(latencyUs);
        Serial.print(" us (");

        Serial.print(latencyUs / 1000.0);
        Serial.println(" ms)");
    }
}