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

// Variabel Sudut (volatile karena dibaca di dalam Interrupt)
volatile int currentAngle = 0;
volatile int lastClkState;

// ISR (Interrupt Service Routine) untuk membaca encoder secara presisi
void IRAM_ATTR updateEncoder() {
  int currentClk = digitalRead(PIN_CLK);
  if (currentClk != lastClkState && currentClk == LOW) {
    if (digitalRead(PIN_DT) != currentClk) {
      currentAngle += 15; // Putar Kanan
    } else {
      currentAngle -= 15; // Putar Kiri
    }

    // Wrap around 0 - 360 derajat
    if (currentAngle >= 360) currentAngle -= 360;
    if (currentAngle < 0) currentAngle += 360;
  }
  lastClkState = currentClk;
}

void setup() {
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(BTN_GAS, INPUT_PULLUP);
  pinMode(BTN_REM, INPUT_PULLUP);

  lastClkState = digitalRead(PIN_CLK);

  // Pasang Interrupt pada pin CLK
  attachInterrupt(digitalPinToInterrupt(PIN_CLK), updateEncoder, CHANGE);

  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setAutoReport(true);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  
  // Aktifkan Sumbu X dan Y
  bleGamepadConfig.setWhichAxes(true, true, false, false, false, false, false, false);
  bleGamepadConfig.setButtonCount(2);
  
  bleGamepad.begin(&bleGamepadConfig);
}

void loop() {
  if (bleGamepad.isConnected()) {
    // 1. Hitung Posisi Lingkaran X & Y dari Sudut saat ini
    float angleRad = currentAngle * (PI / 180.0);
    int axisX = cos(angleRad) * 32767;
    int axisY = sin(angleRad) * 32767;

    // Send posisi ke Left Thumbstick
    bleGamepad.setLeftThumb(axisX, axisY);

    // 2. Baca Tombol Gas (Button 1) & Rem (Button 2)
    if (digitalRead(BTN_GAS) == LOW) bleGamepad.press(BUTTON_1); else bleGamepad.release(BUTTON_1);
    if (digitalRead(BTN_REM) == LOW) bleGamepad.press(BUTTON_2); else bleGamepad.release(BUTTON_2);

    delay(10); // Jeda pengiriman data BLE
  }
}
