#include <BleKeyboard.h>

// Nama Bluetooth baru murni tanpa NimBLE
BleKeyboard bleKeyboard("SETIR C3 MURNI", "ESP32", 100);

void setup() {
  Serial.begin(115200);
  
  // Langsung jalankan BLE Keyboard bawaan standar tanpa NimBLE
  bleKeyboard.begin();
}

void loop() {
  // Delay santai, fokus tes apakah nama 'SETIR C3 MURNI' mau "Terhubung"
  delay(1000);
}
