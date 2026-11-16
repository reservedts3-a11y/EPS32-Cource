#include <Arduino.h>
#include <esp_task_wdt.h>

#define GPIO_IN 34  // photoresistor
#define GPIO_OUT 14 // LED

const int THRESHOLD = 2000; // dark/light threshold
const int HYSTERESIS = 100; // prevents LED flicker near the threshold
const unsigned long READ_INTERVAL = 1000;

unsigned long lastRead = 0;
bool ledOn = false;

void setup()
{
    Serial.begin(115200);
    pinMode(GPIO_OUT, OUTPUT);

    // Watchdog: reboot if loop() does not respond for 5 seconds
    esp_task_wdt_init(5, true);
    esp_task_wdt_add(NULL);
}

void loop()
{
    esp_task_wdt_reset(); // "I'm alive"

    if (millis() - lastRead < READ_INTERVAL)
    {
        return;
    }
    lastRead = millis();

    int value = analogRead(GPIO_IN);
    delay(100); // small delay to stabilize the reading

    // Sensor check: 0 or 4095 most likely means a broken wire or short circuit
    if (value <= 10 || value >= 4085)
    {
        Serial.printf("ПОМИЛКА датчика: %d\n", value);
        digitalWrite(GPIO_OUT, LOW); // safe mode
        return;
    }

    // Hysteresis: turn on below 2000, turn off above 2200
    if (!ledOn && value < THRESHOLD - HYSTERESIS)
    {
        ledOn = true;
    }
    else if (ledOn && value > THRESHOLD + HYSTERESIS)
    {
        ledOn = false;
    }
    digitalWrite(GPIO_OUT, !ledOn ? HIGH : LOW);

    Serial.printf("light: %d, LED: %s\n", value, ledOn ? "ON" : "OFF");
}