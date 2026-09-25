#define USE_NIMBLE // Gunakan NimBLE agar Bluetooth C3 ringan & stabil
#include <BleKeyboard.h>

BleKeyboard bleKeyboard("SETIR", "ESP32", 100);

const int POT_PIN = 0;   // GPIO 0
const int GAS_PIN = 1;   // GPIO 1
const int BRAKE_PIN = 2; // GPIO 2

float steerSmoothed = 2048.0;
const float EMA_ALPHA = 0.15; // Filter perhalus gerak

unsigned long lastSteerPulse = 0;
bool steerKeyPressed = false;

void setup() {
  Serial.begin(115200);

  pinMode(GAS_PIN, INPUT_PULLUP);
  pinMode(BRAKE_PIN, INPUT_PULLUP);

  bleKeyboard.begin();
}

void loop() {
  if (!bleKeyboard.isConnected()) {
    delay(100);
    return;
  }

  unsigned long currentMillis = millis();

  // 1. PEDAL GAS ('w') & REM ('s')
  if (digitalRead(GAS_PIN) == LOW) {
    bleKeyboard.press('w');
  } else {
    bleKeyboard.release('w');
  }

  if (digitalRead(BRAKE_PIN) == LOW) {
    bleKeyboard.press('s');
  } else {
    bleKeyboard.release('s');
  }

  // 2. PEMBACAAN POTENSIO & PEMBATASAN 1.5 PUTARAN KIRI/KANAN
  int rawValue = analogRead(POT_PIN);
  steerSmoothed = (EMA_ALPHA * rawValue) + ((1.0 - EMA_ALPHA) * steerSmoothed);

  // BATAS ADC: Mengambil 1.5 putaran kiri (-1.5) sampai 1.5 putaran kanan (+1.5) -> Total 3 Putaran Aktif
  // Nilai 650 = 1.5 putaran kiri, Nilai 3350 = 1.5 putaran kanan
  int potLimited = constrain((int)steerSmoothed, 650, 3350);

  // Map 3 putaran aktif tersebut ke range steering -100 (Kiri) s/d +100 (Kanan)
  int steerSteering = map(potLimited, 650, 3350, -100, 100);

  // 3. LOGIKA PULSA PWM SETIR ('a' & 'd')
  int absSteer = abs(steerSteering);

  if (absSteer < 5) { 
    // Deadzone Tengah (Setir Lurus 0 Derajat)
    bleKeyboard.release('a');
    bleKeyboard.release('d');
    steerKeyPressed = false;
  } 
  else {
    int pulseCycle = 40; // Total periode pulsa (40ms)
    int onTime = map(absSteer, 5, 100, 5, pulseCycle);

    if ((currentMillis - lastSteerPulse) < onTime) {
      if (!steerKeyPressed) {
        if (steerSteering < 0) {
          bleKeyboard.press('a');  // Belok Kiri
          bleKeyboard.release('d');
        } else {
          bleKeyboard.press('d');  // Belok Kanan
          bleKeyboard.release('a');
        }
        steerKeyPressed = true;
      }
    } else if ((currentMillis - lastSteerPulse) < pulseCycle) {
      if (steerKeyPressed) {
        bleKeyboard.release('a');
        bleKeyboard.release('d');
        steerKeyPressed = false;
      }
    } else {
      lastSteerPulse = currentMillis;
    }
  }

  delay(5);
}
