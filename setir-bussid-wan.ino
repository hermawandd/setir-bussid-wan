#include <Arduino.h>
#include <BleGamepad.h>

// Inisialisasi BleGamepad
BleGamepad bleGamepad("Setir DIY BUSSID", "DIY Project", 100);

// Pin Input ESP32-C3
const int PIN_SETIR = 0; // GPIO 0 (Potensio Setir 100k)
const int BTN_GAS   = 1; // GPIO 1 (Tombol Gas)
const int BTN_REM   = 2; // GPIO 2 (Tombol Rem)

// Parameter Filter Halus (EMA) Khusus Potensio 100k
float steerSmoothed = 2048.0; 
float alpha = 0.08; // Semakin kecil (misal 0.05), semakin halus & tenang

void setup() {
  // Config Pin Tombol (Aktif LOW / Terhubung ke GND)
  pinMode(BTN_GAS, INPUT_PULLUP);
  pinMode(BTN_REM, INPUT_PULLUP);

  // Configuration BleGamepad (0.8.0)
  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setAutoReport(true);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  
  // HANYA Aktifkan Sumbu X (Setir)
  bleGamepadConfig.setWhichAxes(true, false, false, false, false, false, false, false);
  
  // 2 Tombol (Gas & Rem)
  bleGamepadConfig.setButtonCount(2);
  
  bleGamepad.begin(&bleGamepadConfig);
}

void loop() {
  if (bleGamepad.isConnected()) {
    // 1. Baca & Filter Potensio Setir (GPIO 0)
    int rawSteer = analogRead(PIN_SETIR);
    steerSmoothed = (alpha * rawSteer) + ((1.0 - alpha) * steerSmoothed);

    // Deadzone Kecil di Titik Tengah (~2048)
    int nilaiInt = (int)steerSmoothed;
    if (nilaiInt > 1980 && nilaiInt < 2110) {
      nilaiInt = 2048;
    }

    // Map ke Sumbu X Gamepad (-32767 sampai 32767)
    int steerValue = map(nilaiInt, 0, 4095, -32767, 32767);
    bleGamepad.setX(steerValue);

    // 2. Baca Tombol Gas (Button 1) & Rem (Button 2)
    if (digitalRead(BTN_GAS) == LOW) bleGamepad.press(BUTTON_1); else bleGamepad.release(BUTTON_1);
    if (digitalRead(BTN_REM) == LOW) bleGamepad.press(BUTTON_2); else bleGamepad.release(BUTTON_2);

    delay(10); // Jeda pembacaan
  }
}
