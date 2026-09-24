#include <Arduino.h>
#include <BleGamepad.h>
#include <math.h>

BleGamepad bleGamepad("Setir DIY BUSSID", "DIY Project", 100);

// Pin KY-040 Rotary Encoder
const int PIN_CLK = 0; // GPIO 0
const int PIN_DT  = 3; // GPIO 3

// Pin Tombol Digital
const int BTN_GAS = 1; // GPIO 1
const int BTN_REM = 2; // GPIO 2

// Variabel Sudut Putaran (Derajat 0-360)
int currentAngle = 0; 
int lastClkState;

void setup() {
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(BTN_GAS, INPUT_PULLUP);
  pinMode(BTN_REM, INPUT_PULLUP);

  lastClkState = digitalRead(PIN_CLK);

  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setAutoReport(true);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  
  // Aktifkan Sumbu X dan Y untuk gerak melingkar
  bleGamepadConfig.setWhichAxes(true, true, false, false, false, false, false, false);
  bleGamepadConfig.setButtonCount(2);
  
  bleGamepad.begin(&bleGamepadConfig);
}

void loop() {
  if (bleGamepad.isConnected()) {
    // -------------------------------------------------------------
    // 1. Baca Digital Pulse dari KY-040 Encoder
    // -------------------------------------------------------------
    int currentClkState = digitalRead(PIN_CLK);

    if (currentClkState != lastClkState && currentClkState == LOW) {
      // Jika diputar searah jarum jam / berlawanan
      if (digitalRead(PIN_DT) != currentClkState) {
        currentAngle += 15; // Tambah sudut putar
      } else {
        currentAngle -= 15; // Kurang sudut putar
      }

      // Jaga agar nilai sudut tetap di rentang 0 - 359 derajat
      if (currentAngle >= 360) currentAngle -= 360;
      if (currentAngle < 0) currentAngle += 360;

      // -------------------------------------------------------------
      // 2. Hitung Posisi Lingkaran X & Y (Trigonometri)
      // -------------------------------------------------------------
      float angleRad = currentAngle * (PI / 180.0);
      int axisX = cos(angleRad) * 32767;
      int axisY = sin(angleRad) * 32767;

      bleGamepad.setLeftThumb(axisX, axisY);
    }
    lastClkState = currentClkState;

    // -------------------------------------------------------------
    // 3. Baca Tombol Gas (Button 1) & Rem (Button 2)
    // -------------------------------------------------------------
    if (digitalRead(BTN_GAS) == LOW) bleGamepad.press(BUTTON_1); else bleGamepad.release(BUTTON_1);
    if (digitalRead(BTN_REM) == LOW) bleGamepad.press(BUTTON_2); else bleGamepad.release(BUTTON_2);

    delay(2); // Delay super cepat untuk pembacaan encoder
  }
}
