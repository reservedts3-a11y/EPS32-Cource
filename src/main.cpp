// ============================================================
//  Homework: relay timing + software PWM + UART as PWM source
//  Board: ESP32-S3
// ============================================================
//
//  Choose which task to run and upload:
//    1 - Task 1: relay switching time measurement
//    2 - Task 2: software PWM for a motor (potentiometer)
//    3 - Experiment: UART as a PWM-like signal source
#define TASK 1

#include <Arduino.h>

// ============================================================
//  TASK 1: Relay switching time measurement
// ============================================================
//  Wiring:
//    ESP 5V    -> relay module VCC
//    ESP GND   -> relay module GND
//    GPIO5     -> relay module IN
//    GPIO4     -> relay contact NO
//    relay COM -> ESP GND (second GND pin)
#if TASK == 1

constexpr uint8_t RELAY_PIN   = 5;  // controls relay coil
constexpr uint8_t CONTACT_PIN = 4;  // reads relay dry contact (INPUT_PULLUP)

// Many relay modules are "active LOW" (relay turns on with LOW).
// If your relay works inverted - swap HIGH and LOW here.
constexpr uint8_t RELAY_ON  = HIGH;
constexpr uint8_t RELAY_OFF = LOW;

constexpr int      MEASUREMENTS = 10;   // number of on/off cycles
constexpr uint32_t SETTLE_MS    = 100;  // wait until contact stops bouncing
constexpr uint32_t PAUSE_MS     = 300;  // pause between switchings

// "volatile" - these variables are changed inside the interrupt,
// so the compiler must always read them from memory
volatile bool     measuring   = false;  // ISR records edges only while measuring
volatile uint32_t firstEdgeUs = 0;      // time of the FIRST contact change
volatile uint32_t lastEdgeUs  = 0;      // time of the LAST change (end of bounce)
volatile uint32_t edgeCount   = 0;      // number of changes (1 = no bounce)

// Interrupt handler: called on every change of the contact pin.
// IRAM_ATTR - keeps the function in fast RAM (required for ISR on ESP32)
void IRAM_ATTR onContactChange() {
  if (!measuring) {
    return;
  }
  uint32_t now = micros();
  // Contact bounce: one switching gives several fast edges.
  // The FIRST edge is the real moment the contact touched/released,
  // the rest are bounce - we only count them, not treat them as new switchings
  if (edgeCount == 0) {
    firstEdgeUs = now;
  }
  lastEdgeUs = now;
  edgeCount++;
}

struct Result {
  bool     ok;        // contact actually switched
  uint32_t delayUs;   // command -> first contact change (relay delay)
  uint32_t bounceUs;  // first edge -> last edge (bounce duration)
  uint32_t edges;     // number of edges
};

// Switches the relay and measures how long the contact takes to react
Result measure(uint8_t relayLevel, int expectedContact) {
  edgeCount   = 0;
  firstEdgeUs = 0;
  lastEdgeUs  = 0;

  measuring = true;
  uint32_t startUs = micros();        // moment of the command
  digitalWrite(RELAY_PIN, relayLevel);

  delay(SETTLE_MS);                   // relay switches and bounce ends meanwhile
  measuring = false;

  Result r;
  // Valid only if there was an edge and the contact ended up in the expected state
  r.ok       = edgeCount > 0 && digitalRead(CONTACT_PIN) == expectedContact;
  r.delayUs  = firstEdgeUs - startUs;
  r.bounceUs = lastEdgeUs - firstEdgeUs;
  r.edges    = edgeCount;
  return r;
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // time to open Serial Monitor

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(CONTACT_PIN, INPUT_PULLUP);  // open contact = HIGH, closed = LOW
  delay(500);

  // Interrupt on any change of the contact (both closing and opening)
  attachInterrupt(digitalPinToInterrupt(CONTACT_PIN), onContactChange, CHANGE);

  Serial.println("Relay timing test");
  Serial.println(" #  | ON delay, ms | ON bounce, ms | edges | OFF delay, ms | OFF bounce, ms | edges");
  Serial.println("----+--------------+---------------+-------+---------------+----------------+------");

  uint32_t sumOn = 0, sumOff = 0;
  int countOn = 0, countOff = 0;

  for (int i = 1; i <= MEASUREMENTS; i++) {
    Result on = measure(RELAY_ON, LOW);    // relay on -> contact closes -> LOW
    delay(PAUSE_MS);
    Result off = measure(RELAY_OFF, HIGH); // relay off -> contact opens -> HIGH
    delay(PAUSE_MS);

    if (on.ok)  { sumOn  += on.delayUs;  countOn++;  }
    if (off.ok) { sumOff += off.delayUs; countOff++; }

    Serial.printf("%3d | %12.2f | %13.2f | %5lu | %13.2f | %14.2f | %5lu %s\n",
                  i,
                  on.delayUs / 1000.0, on.bounceUs / 1000.0, on.edges,
                  off.delayUs / 1000.0, off.bounceUs / 1000.0, off.edges,
                  (on.ok && off.ok) ? "" : "<- ERROR: contact did not switch");
  }

  Serial.println();
  if (countOn > 0) {
    Serial.printf("Average ON  time: %.2f ms (%d measurements)\n", sumOn / 1000.0 / countOn, countOn);
  }
  if (countOff > 0) {
    Serial.printf("Average OFF time: %.2f ms (%d measurements)\n", sumOff / 1000.0 / countOff, countOff);
  }
  if (countOn == 0 && countOff == 0) {
    Serial.println("No valid measurements - check wiring and RELAY_ON level");
  }
}

void loop() {
  // Test runs once in setup(). Press RESET to repeat.
}

// ============================================================
//  TASK 2: Software PWM for a motor
// ============================================================
//  Wiring:
//    Potentiometer: outer pins -> 3.3V and GND, middle pin -> GPIO1
//    GPIO6 -> 1 kOhm -> 2N2222 base
//    Motor between +5V and collector, emitter -> GND
//    Diode 1N4007 across the motor (cathode to +5V) - protects the transistor
#elif TASK == 2

constexpr uint8_t  POT_PIN   = 1;     // potentiometer (ADC)
constexpr uint8_t  MOTOR_PIN = 6;     // transistor base via 1 kOhm
constexpr uint32_t PERIOD_US = 2000;  // PWM period: 2000 us = 500 Hz

uint32_t highUs = 0;          // how long the signal is HIGH in one period
uint32_t periodStartUs = 0;   // when the current period started
uint32_t lastPotReadMs = 0;
uint32_t lastPrintMs = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_PIN, OUTPUT);
}

void loop() {
  // Read potentiometer every 20 ms (no need to do it every period)
  if (millis() - lastPotReadMs >= 20) {
    lastPotReadMs = millis();
    int raw = analogRead(POT_PIN);              // 0..4095
    highUs = (uint32_t)raw * PERIOD_US / 4095;  // ADC -> HIGH time
    // LOW time = PERIOD_US - highUs
  }

  // Software PWM: HIGH at the start of the period, LOW after highUs
  uint32_t elapsed = micros() - periodStartUs;
  if (elapsed >= PERIOD_US) {
    periodStartUs = micros();  // start a new period
    elapsed = 0;
  }
  digitalWrite(MOTOR_PIN, elapsed < highUs ? HIGH : LOW);

  // Print current duty cycle twice per second
  if (millis() - lastPrintMs >= 500) {
    lastPrintMs = millis();
    Serial.printf("HIGH: %4lu us | LOW: %4lu us | duty: %5.1f %%\n",
                  highUs, PERIOD_US - highUs, highUs * 100.0 / PERIOD_US);
  }
}

// ============================================================
//  TASK 3 (experiment): UART as a PWM-like signal source
// ============================================================
//  UART 8N1 sends every byte as 10 bits:
//    start bit (LOW) + 8 data bits (lowest bit first) + stop bit (HIGH)
//  Sending the same byte non-stop gives a repeating pattern with its own duty:
//    0x00 -> 0 000000000 1  -> HIGH 1 of 10 bits  (~10%)
//    0x0F -> 0 11110000  1  -> HIGH 5 of 10 bits  (~50%, wide pulses)
//    0x55 -> 0 10101010  1  -> HIGH 5 of 10 bits  (~50%, square wave)
//    0xFF -> 0 11111111  1  -> HIGH 9 of 10 bits  (~90%)
//  Connect a logic analyzer to GPIO7 to see the signal.
//  Type 1/2/3/4 in Serial Monitor to change the byte.
#elif TASK == 3

constexpr uint8_t UART_TX_PIN = 7;  // UART signal output (logic analyzer here)
constexpr uint32_t UART_BAUD  = 9600;

const uint8_t PATTERNS[] = {0x00, 0x0F, 0x55, 0xFF};
uint8_t currentByte = 0x55;

void setup() {
  Serial.begin(115200);
  // Serial1: only TX is used (RX = -1, not connected)
  Serial1.begin(UART_BAUD, SERIAL_8N1, -1, UART_TX_PIN);
  Serial.println("Type 1-4 to choose byte: 1=0x00, 2=0x0F, 3=0x55, 4=0xFF");
}

void loop() {
  // Change the byte from Serial Monitor
  if (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '4') {
      currentByte = PATTERNS[c - '1'];
      Serial.printf("Sending 0x%02X\n", currentByte);
    }
  }

  // Send the byte non-stop, so the signal on the pin is continuous
  Serial1.write(currentByte);
}

#endif
