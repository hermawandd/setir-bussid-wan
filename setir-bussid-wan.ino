#define USE_NIMBLE
#include <BleKeyboard.h>
#include <NimBLEDevice.h>

BleKeyboard bleKeyboard("SETIR C3 FIX", "ESP32", 100);

const int GAS_PIN = 1;   // GPIO 1 (Pedal Gas)
const int BRAKE_PIN = 2; // GPIO 2 (Pedal Rem)

void setup() {
  Serial.begin(115200);

  pinMode(GAS_PIN, INPUT_PULLUP);
  pinMode(BRAKE_PIN, INPUT_PULLUP);

  // Inisialisasi NimBLE dengan mode Enkripsi Bonding
  NimBLEDevice::init("SETIR C3 FIX");
  
  // Setel keamanan sesuai rekomendasi perbaikan (Bonding tanpa PIN IO)
  NimBLEDevice::setSecurityAuth(true, false, false, BLE_SM_PAIR_AUTHREQ_BOND);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  bleKeyboard.begin();
}

void loop() {
  if (!bleKeyboard.isConnected()) {
    delay(50);
    return;
  }

  // Pedal Gas ('w')
  if (digitalRead(GAS_PIN) == LOW) {
    bleKeyboard.press('w');
  } else {
    bleKeyboard.release('w');
  }

  // Pedal Rem ('s')
  if (digitalRead(BRAKE_PIN) == LOW) {
    bleKeyboard.press('s');
  } else {
    bleKeyboard.release('s');
  }

  delay(10);
}
