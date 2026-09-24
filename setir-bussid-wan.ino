#include <Arduino.h>
#include <BleGamepad.h>

// Nama Bluetooth yang muncul di HP
BleGamepad bleGamepad("Setir DIY BUSSID", "DIY Project", 100);

// Pin Tombol di ESP32-C3 Super Mini
const int BTN_GAS = 1; // GPIO 1 (Tombol Gas)
const int BTN_REM = 2; // GPIO 2 (Tombol Rem)

void setup() {
  // Aktifkan Internal Pullup (Kabel tombol cukup ke GPIO dan GND)
  pinMode(BTN_GAS, INPUT_PULLUP);
  pinMode(BTN_REM, INPUT_PULLUP);

  // Konfigurasi BleGamepad khusus 2 tombol
  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setAutoReport(true);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  
  // MATIKAN SEMUA ANALOG / SETIR (Biar anteng & gak ada ghost touch)
  bleGamepadConfig.setWhichAxes(false, false, false, false, false, false, false, false);
  
  // Cuma aktifkan 2 Tombol
  bleGamepadConfig.setButtonCount(2);
  
  bleGamepad.begin(&bleGamepadConfig);
}

void loop() {
  if (bleGamepad.isConnected()) {
    // Tombol Gas -> Terdeteksi sebagai Button 1
    if (digitalRead(BTN_GAS) == LOW) {
      bleGamepad.press(BUTTON_1);
    } else {
      bleGamepad.release(BUTTON_1);
    }

    // Tombol Rem -> Terdeteksi sebagai Button 2
    if (digitalRead(BTN_REM) == LOW) {
      bleGamepad.press(BUTTON_2);
    } else {
      bleGamepad.release(BUTTON_2);
    }

    delay(10); // Jeda pembacaan
  }
}
