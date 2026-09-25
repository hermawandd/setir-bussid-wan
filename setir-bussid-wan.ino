#include <Arduino.h>
#include <BleGamepad.h>

// Nama Bluetooth Gamepad untuk Tes
BleGamepad bleGamepad("GAMEPAD C3 TEST", "ESP32", 100);

void setup() {
  Serial.begin(115200);
  
  // Jalankan BLE Gamepad
  bleGamepad.begin();
}

void loop() {
  // Hanya delay, fokus murni tes apakah nama 'GAMEPAD C3 TEST' mau "Terhubung"
  delay(1000);
}
