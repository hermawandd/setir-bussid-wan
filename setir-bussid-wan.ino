#include <BleGamepad.h>

BleGamepad bleGamepad("Setir DIY BUSSID", "BossDIY", 100);

#define PIN_POT_SETIR 0  // Potensio Setir (GPIO 0 / ADC)
#define PIN_SW_GAS    1  // Microswitch Gas (GPIO 1)
#define PIN_SW_REM    2  // Microswitch Rem (GPIO 2)

void setup() {
  pinMode(PIN_SW_GAS, INPUT_PULLUP);
  pinMode(PIN_SW_REM, INPUT_PULLUP);
  bleGamepad.begin();
}

void loop() {
  if (bleGamepad.isConnected()) {
    // 1. LOGIKA SETIR & CLAMPING 540 DERAJAT
    int valPot = analogRead(PIN_POT_SETIR);
    float sudutFisik = map(valPot, 0, 4095, -720, 720);

    if (sudutFisik > 540) {
      sudutFisik = 540;
    } else if (sudutFisik < -540) {
      sudutFisik = -540;
    }

    int outputSetir = map(sudutFisik, -540, 540, -32767, 32767);
    bleGamepad.setX(outputSetir);

    // 2. LOGIKA PEDAL GAS & REM (MICROSWITCH)
    bool isGasPressed = (digitalRead(PIN_SW_GAS) == LOW);
    bool isRemPressed = (digitalRead(PIN_SW_REM) == LOW);

    if (isGasPressed) bleGamepad.press(BUTTON_1);
    else bleGamepad.release(BUTTON_1);

    if (isRemPressed) bleGamepad.press(BUTTON_2);
    else bleGamepad.release(BUTTON_2);

    bleGamepad.sendReport();
  }
  delay(10);
}
