#define USE_NIMBLE
#include <BleKeyboard.h>
#include <NimBLEDevice.h>

// Nama perangkat Bluetooth baru
BleKeyboard bleKeyboard("SETIR C3 NIMBLE", "ESP32", 100);

const int GAS_PIN = 1;   // GPIO 1 (Pedal Gas)
const int BRAKE_PIN = 2; // GPIO 2 (Pedal Rem)

void setup() {
  Serial.begin(115200);

  pinMode(GAS_PIN, INPUT_PULLUP);
  pinMode(BRAKE_PIN, INPUT_PULLUP);

  // --- TRIK KHUSUS: Mengunci Konfigurasi NimBLE ala Gamepad ---
  NimBLEDevice::init("SETIR C3 NIMBLE");
  
  // Matikan semua proteksi bonding & enkripsi ketat Android
  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  bleKeyboard.begin();
}

void loop() {
  if (!bleKeyboard.isConnected()) {
    delay(50);
    return;
  }

  // 1. PEDAL GAS ('w')
  if (digitalRead(GAS_PIN) == LOW) {
    bleKeyboard.press('w');
  } else {
    bleKeyboard.release('w');
  }

  // 2. PEDAL REM ('s')
  if (digitalRead(BRAKE_PIN) == LOW) {
    bleKeyboard.press('s');
  } else {
    bleKeyboard.release('s');
  }

  delay(10);
}
