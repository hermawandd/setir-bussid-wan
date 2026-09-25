#include <SkBleKeyboard.h>

// Inisialisasi sesuai contoh resmi library
SkBleKeyboard keyboard("SETIR BUS V2", "ESP32-C3", 100);

const int POT_PIN = 0;   // GPIO 0 untuk Potensio Setir
const int GAS_PIN = 1;   // GPIO 1 untuk Pedal Gas
const int BRAKE_PIN = 2; // GPIO 2 untuk Pedal Rem

float steerSmoothed = 2048.0;
const float EMA_ALPHA = 0.15;

unsigned long lastSteerPulse = 0;
bool steerKeyPressed = false;

void setup() {
  Serial.begin(115200);

  pinMode(GAS_PIN, INPUT_PULLUP);
  pinMode(BRAKE_PIN, INPUT_PULLUP);

  // Mulai layanan SkBleKeyboard
  keyboard.begin();
}

void loop() {
  // Wajib dipanggil terus menerus di loop sesuai contoh repo
  keyboard.service();

  // Cek koneksi pakai .connected() sesuai contoh repo
  if (!keyboard.connected()) {
    delay(10);
    return;
  }

  unsigned long currentMillis = millis();

  // 1. PEDAL GAS ('w') & REM ('s')
  if (digitalRead(GAS_PIN) == LOW) {
    keyboard.press('w');
  } else {
    keyboard.release('w');
  }

  if (digitalRead(BRAKE_PIN) == LOW) {
    keyboard.press('s');
  } else {
    keyboard.release('s');
  }

  // 2. PEMBACAAN POTENSIO (1.5 Putaran Kiri - 1.5 Putaran Kanan)
  int rawValue = analogRead(POT_PIN);
  steerSmoothed = (EMA_ALPHA * rawValue) + ((1.0 - EMA_ALPHA) * steerSmoothed);

  int potLimited = constrain((int)steerSmoothed, 650, 3350);
  int steerSteering = map(potLimited, 650, 3350, -100, 100);

  // 3. LOGIKA PULSA PWM SETIR ('a' & 'd')
  int absSteer = abs(steerSteering);

  if (absSteer < 5) { 
    keyboard.release('a');
    keyboard.release('d');
    steerKeyPressed = false;
  } 
  else {
    int pulseCycle = 40; 
    int onTime = map(absSteer, 5, 100, 5, pulseCycle);

    if ((currentMillis - lastSteerPulse) < onTime) {
      if (!steerKeyPressed) {
        if (steerSteering < 0) {
          keyboard.press('a');
          keyboard.release('d');
        } else {
          keyboard.press('d');
          keyboard.release('a');
        }
        steerKeyPressed = true;
      }
    } else if ((currentMillis - lastSteerPulse) < pulseCycle) {
      if (steerKeyPressed) {
        keyboard.release('a');
        keyboard.release('d');
        steerKeyPressed = false;
      }
    } else {
      lastSteerPulse = currentMillis;
    }
  }

  delay(5);
}
