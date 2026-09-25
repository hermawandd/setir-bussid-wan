#include <BleKeyboard.h>

// Gunakan nama pendek "SETIR" agar paket BLE ringan & cepat di-respond MIUI
BleKeyboard bleKeyboard("SETIR", "ESP32", 100);

const int GAS_PIN = 1;   // GPIO 1 (Pedal Gas)
const int BRAKE_PIN = 2; // GPIO 2 (Pedal Rem)

void setup() {
  Serial.begin(115200);

  pinMode(GAS_PIN, INPUT_PULLUP);
  pinMode(BRAKE_PIN, INPUT_PULLUP);

  // Inisialisasi Bluetooth BLE
  bleKeyboard.begin();
}

void loop() {
  if (!bleKeyboard.isConnected()) {
    delay(50);
    return;
  }

  // 1. TES PEDAL GAS (Tombol 'w')
  if (digitalRead(GAS_PIN) == LOW) {
    bleKeyboard.press('w');
  } else {
    bleKeyboard.release('w');
  }

  // 2. TES PEDAL REM (Tombol 's')
  if (digitalRead(BRAKE_PIN) == LOW) {
    bleKeyboard.press('s');
  } else {
    bleKeyboard.release('s');
  }

  delay(10);
}
