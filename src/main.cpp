// Завдання 1: Базова реалізація (без debounce, робили на уроці)
// Реалізувати GPIO interrupt на FALLING
// При кожному interrupt:
// інкрементувати лічильник
// Виводити лічильник у лог
// Очікуваний результат

// 1 фізичне натискання -> кілька interrupt
// Interrupt викликається і при відпусканні кнопки

// #include <Arduino.h>

// constexpr uint8_t BUTTON_PIN = 18;

// volatile uint32_t interruptCount = 0;
// volatile uint32_t firstInterruptUs = 0;
// volatile bool eventPending = false;

// void IRAM_ATTR buttonISR() {
//     const uint32_t now = micros();

//     interruptCount++;

//     // Запам'ятовуємо час першої події
//     // до того, як loop() забере прапорець.
//     if (!eventPending) {
//         firstInterruptUs = now;
//         eventPending = true;
//     }
// }

// void setup() {
//     Serial.begin(115200);

//     pinMode(BUTTON_PIN, INPUT_PULLUP);

//     attachInterrupt(
//         digitalPinToInterrupt(BUTTON_PIN),
//         buttonISR,
//         FALLING
//     );
// }

// void loop() {
//     bool hasEvent;
//     uint32_t eventUs;
//     uint32_t count;

//     // Безпечне копіювання спільних змінних
//     noInterrupts();

//     hasEvent = eventPending;
//     eventUs = firstInterruptUs;
//     count = interruptCount;
//     eventPending = false;

//     interrupts();

//     if (hasEvent) {
//         const uint32_t handledUs = micros();
//         const uint32_t latencyUs = handledUs - eventUs;

//         Serial.print("Interrupt count: ");
//         Serial.println(count);

//         Serial.print("Latency: ");
//         Serial.print(latencyUs);
//         Serial.print(" us (");

//         Serial.print(latencyUs / 1000.0);
//         Serial.println(" ms)");
//     }
// }


// Завдання 2: Software debounce через таймер (time-based)
// Додати debounce на основі часу:
// ігнорувати interrupt, якщо з моменту попереднього пройшло < 50 мс
// Реалізувати поза ISR
// Очікуваний результат

// Менше хибних спрацювань
// Все ще можливі спрацювання при відпусканні



#include <Arduino.h>

// Configuration
constexpr uint8_t BUTTON_PIN = 18;
constexpr uint32_t DEBOUNCE_MS = 50;
constexpr uint32_t LOG_INTERVAL_MS = 1000;

// Variables shared with ISR
volatile uint32_t rawInterrupts = 0;
volatile uint32_t pendingInterrupts = 0;
volatile uint32_t firstPendingUs = 0;

// Main program variables
uint32_t acceptedInterrupts = 0;
uint32_t ignoredInterrupts = 0;

uint32_t lastAcceptedMs = 0;
uint32_t lastLogMs = 0;
uint32_t lastLatencyUs = 0;

bool hasAcceptedEvent = false;

// Interrupt handler
void IRAM_ATTR buttonISR() {
    if (pendingInterrupts == 0) {
        firstPendingUs = micros();
    }

    ++rawInterrupts;
    ++pendingInterrupts;
}

void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(BUTTON_PIN),
        buttonISR,
        CHANGE
    );

    lastLogMs = millis();

    Serial.println("Time-based debounce started");
}

void loop() {
    uint32_t newInterrupts = 0;
    uint32_t rawCount = 0;
    uint32_t firstEventUs = 0;

    // Safely copy ISR data
    noInterrupts();

    newInterrupts = pendingInterrupts;
    pendingInterrupts = 0;

    rawCount = rawInterrupts;
    firstEventUs = firstPendingUs;

    interrupts();

    const uint32_t now = millis();

    // Measure ISR-to-loop latency
    if (newInterrupts > 0) {
        lastLatencyUs = micros() - firstEventUs;
    }

    // Time-based debounce
    for (uint32_t i = 0; i < newInterrupts; ++i) {

        if (!hasAcceptedEvent ||
            now - lastAcceptedMs >= DEBOUNCE_MS) {

            ++acceptedInterrupts;

            lastAcceptedMs = now;
            hasAcceptedEvent = true;

        } else {
            ++ignoredInterrupts;
        }
    }

    // Print statistics every second
    if (now - lastLogMs >= LOG_INTERVAL_MS) {
        lastLogMs = now;

        Serial.println("\n--- Time-based Debounce ---");

        Serial.print("Raw interrupts: ");
        Serial.println(rawCount);

        Serial.print("Accepted: ");
        Serial.println(acceptedInterrupts);

        Serial.print("Ignored: ");
        Serial.println(ignoredInterrupts);

        Serial.print("ISR -> loop latency: ");
        Serial.print(lastLatencyUs);
        Serial.println(" us");

        Serial.println("---------------------------");
    }
}
