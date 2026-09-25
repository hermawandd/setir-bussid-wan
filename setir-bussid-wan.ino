#include <BleKeyboard.h>

BleKeyboard bleKeyboard("SETIR C3", "HF BUS", 100);

void setup() {
  Serial.begin(115200);

  Serial.println("Starting BLE Keyboard...");

  bleKeyboard.begin();
}

void loop() {

  if (bleKeyboard.isConnected()) {
    Serial.println("BLUETOOTH CONNECTED!");

    delay(3000);

    bleKeyboard.print("TEST C3");

    delay(5000);
  }

  delay(100);
}
