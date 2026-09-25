#include <BleKeyboard.h>

// Inisialisasi Nama Bluetooth Keyboard
BleKeyboard bleKeyboard("SETIR BUS V2", "ESP32", 100);

// Pin ESP32-C3
const int POT_PIN = 0;   // GPIO 0 untuk Potensio Setir
const int GAS_PIN = 1;   // GPIO 1 untuk Pedal Gas (INPUT_PULLUP)
const int BRAKE_PIN = 2; // GPIO 2 untuk Pedal Rem (INPUT_PULLUP)

// Filter & Kalibrasi Potensio
float steerSmoothed = 2048.0;
const float EMA_ALPHA = 0.15; // Filter perhalus gerak setir

// Timer PWM Pulse untuk Setir Presisi
unsigned long lastSteerPulse = 0;
bool steerKeyPressed = false;

void setup() {
  Serial.begin(115200);

  pinMode(GAS_PIN, INPUT_PULLUP);
  pinMode(BRAKE_PIN, INPUT_PULLUP);

  // Inisialisasi Bluetooth Keyboard
  bleKeyboard.begin();
  Serial.println("Bluetooth Keyboard Siap Dipairing...");
}

void loop() {
  if (!bleKeyboard.isConnected()) {
    delay(50);
    return;
  }

  unsigned long currentMillis = millis();

  // -------------------------------------------------------------
  // 1. PEMBACAAN PEDAL GAS & REM (DIGITAL INPUT)
  // -------------------------------------------------------------
  // Pedal Gas -> Kirim 'W' atau Panah Atas
  if (digitalRead(GAS_PIN) == LOW) {
    bleKeyboard.press(KEY_UP_ARROW);
  } else {
    bleKeyboard.release(KEY_UP_ARROW);
  }

  // Pedal Rem -> Kirim 'S' atau Panah Bawah
  if (digitalRead(BRAKE_PIN) == LOW) {
    bleKeyboard.press(KEY_DOWN_ARROW);
  } else {
    bleKeyboard.release(KEY_DOWN_ARROW);
  }

  // -------------------------------------------------------------
  // 2. PEMBACAAN POTENSIO & KALIBRASI
  // -------------------------------------------------------------
  int rawValue = analogRead(POT_PIN);
  steerSmoothed = (EMA_ALPHA * rawValue) + ((1.0 - EMA_ALPHA) * steerSmoothed);

  // Nilai Tengah Lurus Ideal ~2048 (Range Potensio: 200 - 3800)
  int potLimited = constrain((int)steerSmoothed, 200, 3800);

  // Map nilai ke range steering (-100 kencang kiri, 0 tengah, +100 kencang kanan)
  int steerSteering = map(potLimited, 200, 3800, -100, 100);

  // -------------------------------------------------------------
  // 3. LOGIKA PULSA (PWM) SETIR AGAR DITAHAN BERHENTI SESUAI SUDUT
  // -------------------------------------------------------------
  int absSteer = abs(steerSteering);

  if (absSteer < 5) { // Deadzone Tengah (Setir Lurus)
    bleKeyboard.release(KEY_LEFT_ARROW);
    bleKeyboard.release(KEY_RIGHT_ARROW);
    steerKeyPressed = false;
  } 
  else {
    // Menghitung rasio Waktu Tekan (ON) vs Waktu Lepas (OFF)
    // Semakin miring setir, waktu ON semakin lama
    int pulseCycle = 40; // Total periode pulsa (40ms)
    int onTime = map(absSteer, 5, 100, 5, pulseCycle);

    if ((currentMillis - lastSteerPulse) < onTime) {
      if (!steerKeyPressed) {
        if (steerSteering < 0) {
          bleKeyboard.press(KEY_LEFT_ARROW);
          bleKeyboard.release(KEY_RIGHT_ARROW);
        } else {
          bleKeyboard.press(KEY_RIGHT_ARROW);
          bleKeyboard.release(KEY_LEFT_ARROW);
        }
        steerKeyPressed = true;
      }
    } else if ((currentMillis - lastSteerPulse) < pulseCycle) {
      if (steerKeyPressed) {
        bleKeyboard.release(KEY_LEFT_ARROW);
        bleKeyboard.release(KEY_RIGHT_ARROW);
        steerKeyPressed = false;
      }
    } else {
      lastSteerPulse = currentMillis;
    }
  }

  delay(5);
}
