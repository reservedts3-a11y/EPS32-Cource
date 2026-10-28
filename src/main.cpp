#include <Arduino.h>


/// Фінальний проєкт «Мультизадачний пристрій» — базова версія
// ESP32-S3
// LED1: GPIO15 — індикатор паузи
// LED2: GPIO16 — період задає BTN1 (1000 → 500 → 200 → 1000)
// LED3: GPIO17 — завжди 1000 мс
// BTN1: GPIO21, BTN2: GPIO18 — між 3V3 і входом, INPUT_PULLDOWN (натиснуто = HIGH)
// Час — лише millis() у формі now - last >= INTERVAL. Жодного delay()/while() у loop().


// ---------- Таймер світлодіода ----------
struct Blinker
{
uint8_t pin;
unsigned long period;     // мс між перемиканнями
unsigned long lastToggle; // момент останньої зміни
bool state;
};

// ---------- Кнопка з неблокуючим дебаунсом ----------
struct Button
{
uint8_t pin;
uint8_t mode;             // INPUT_PULLDOWN або INPUT (для BOOT)
int activeLevel;          // HIGH для схеми з 3V3, LOW для BOOT (GPIO0)
bool stable;              // підтверджений стан: true = натиснута
bool lastReading;         // останнє «сире» читання
unsigned long lastChange; // коли сире читання змінилось востаннє
};


const unsigned long DEBOUNCE_MS = 40;
const unsigned long REPORT_PERIOD = 1000;


const uint8_t LED_PAUSE_PIN = 15;


Blinker led2 = {16, 1000, 0, false};
Blinker led3 = {17, 1000, 0, false};


Button btn1 = {21, INPUT_PULLDOWN, HIGH, false, false, 0};
Button btn2 = {18, INPUT_PULLDOWN, HIGH, false, false, 0};
// Немає другої кнопки? BTN2 = вбудована BOOT:
// Button btn2 = {0, INPUT, LOW, false, false, 0};


const unsigned long LED2_PERIODS[] = {1000, 500, 200};
const size_t LED2_PERIOD_COUNT = sizeof(LED2_PERIODS) / sizeof(LED2_PERIODS[0]);
size_t led2PeriodIndex = 0;


bool paused = false;


unsigned long loopCount = 0;
unsigned long lastReport = 0;


// ---------- Допоміжні функції ----------


void updateBlinker(Blinker &b, unsigned long now)
{
if (now - b.lastToggle >= b.period)
  {
b.lastToggle = now;
b.state = !b.state;
digitalWrite(b.pin, b.state ? HIGH : LOW);
  }
}


void initButton(Button &b)
{
pinMode(b.pin, b.mode);
  // Стартуємо з реального стану, щоб не було «фантомного» натискання при старті
bool reading = (digitalRead(b.pin) == b.activeLevel);
b.stable = reading;
b.lastReading = reading;
b.lastChange = millis();
}


// true рівно один раз — у момент підтвердженого натискання (фронт), не під час утримання
bool wasPressed(Button &b, unsigned long now)
{
bool reading = (digitalRead(b.pin) == b.activeLevel);


if (reading != b.lastReading)
  { // сигнал «стрибнув» — перезапускаємо відлік
b.lastReading = reading;
b.lastChange = now;
  }


if (now - b.lastChange >= DEBOUNCE_MS && reading != b.stable)
  {
b.stable = reading; // стан протримався 40 мс — приймаємо
return b.stable;    // true лише для переходу «відпущена → натиснута»
  }
return false;
}


// ---------- Arduino ----------


void setup()
{
  Serial.begin(115200);


pinMode(LED_PAUSE_PIN, OUTPUT);
pinMode(led2.pin, OUTPUT);
pinMode(led3.pin, OUTPUT);
digitalWrite(LED_PAUSE_PIN, LOW);
digitalWrite(led2.pin, LOW);
digitalWrite(led3.pin, LOW);


initButton(btn1);
initButton(btn2);


  Serial.println("Старт: LED2 = 1000 мс, LED3 = 1000 мс");
}


void loop()
{
unsigned long now = millis();
  loopCount++;


  // 1. Кнопки читаємо ЗАВЖДИ, навіть у паузі — інакше дебаунс «застаріє»
bool btn1Pressed = wasPressed(btn1, now);
bool btn2Pressed = wasPressed(btn2, now);


  // 2. Пауза
if (btn2Pressed)
  {
    paused = !paused;
digitalWrite(LED_PAUSE_PIN, paused ? HIGH : LOW);


if (!paused)
    {
      // Після паузи починаємо відлік заново, інакше LED миттєво «доганяють» пропущене
      led2.lastToggle = now;
      led3.lastToggle = now;
    }
    Serial.println(paused ? "ПАУЗА увімк" : "ПАУЗА вимк");
  }


  // 3. Швидкість LED2
if (btn1Pressed)
  {
if (paused)
    {
      Serial.println("BTN1 ігнорується (пауза)");
    }
else
    {
      led2PeriodIndex = (led2PeriodIndex + 1) % LED2_PERIOD_COUNT;
      led2.period = LED2_PERIODS[led2PeriodIndex];
      Serial.print("BTN1 -> LED2 = ");
      Serial.print(led2.period);
      Serial.println(" мс");
    }
  }


  // 4. Таймери (у паузі LED2 і LED3 завмирають у поточному стані)
if (!paused)
  {
updateBlinker(led2, now);
updateBlinker(led3, now);
  }


  // 5. Звіт раз на секунду
if (now - lastReport >= REPORT_PERIOD)
  {
    Serial.print("loop() за секунду: ");
    Serial.println(loopCount);
    loopCount = 0;
    lastReport = now;
  }
}