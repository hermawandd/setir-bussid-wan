#include <Arduino.h>
#include <BleGamepad.h>

// Nama Bluetooth yang muncul di HP
BleGamepad bleGamepad("Setir DIY BUSSID", "DIY Project", 100);

// Pin ESP32-C3 Super Mini
const int PIN_SETIR = 0; // GPIO 0 (Potensio Setir 100k)
const int BTN_GAS   = 1; // GPIO 1 (Tombol Gas)
const int BTN_REM   = 2; // GPIO 2 (Tombol Rem)

// Filter Kehalusan Potensio Setir 100k
float steerSmoothed = 2048.0; 
float alpha = 0.05; // Filter lebih tenang biar tidak getar

void setup() {
  // Pin Tombol (Aktif LOW)
  pinMode(BTN_GAS, INPUT_PULLUP);
  pinMode(BTN_REM, INPUT_PULLUP);

  // Konfigurasi BleGamepad
  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setAutoReport(true);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  
  // HANYA Aktifkan Sumbu X (Setir / Steering Wheel)
  bleGamepadConfig.setWhichAxes(true, false, false, false, false, false, false, false);
  
  // Aktifkan 2 Tombol (Gas & Rem)
  bleGamepadConfig.setButtonCount(2);
  
  bleGamepad.begin(&bleGamepadConfig);
}

void loop() {
  if (bleGamepad.isConnected()) {
    // -------------------------------------------------------------
    // 1. Pembacaan Setir dengan Filter Kehalusan
    // -------------------------------------------------------------
    int rawSteer = analogRead(PIN_SETIR);
    steerSmoothed = (alpha * rawSteer) + ((1.0 - alpha) * steerSmoothed);

    // Deadzone Kecil di Tengah (~2048)
    int nilaiInt = (int)steerSmoothed;
    if (nilaiInt > 1980 && nilaiInt < 2110) {
      nilaiInt = 2048;
    }

    // Map nilai ADC ke Sumbu X (-32767 s/d 32767)
    int steerValue = map(nilaiInt, 0, 4095, -32767, 32767);
    bleGamepad.setX(steerValue);

    // -------------------------------------------------------------
    // 2. Pembacaan Tombol Gas (Button 1) & Rem (Button 2)
    // -------------------------------------------------------------
    if (digitalRead(BTN_GAS) == LOW) {
      bleGamepad.press(BUTTON_1);
    } else {
      bleGamepad.release(BUTTON_1);
    }

    if (digitalRead(BTN_REM) == LOW) {
      bleGamepad.press(BUTTON_2);
    } else {
      bleGamepad.release(BUTTON_2);
    }

    delay(10); // Jeda pembacaan
  }
}
