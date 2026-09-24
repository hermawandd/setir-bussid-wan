#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// Pin Input
const int PIN_SETIR = 0; // GPIO 0 (Potensio Setir 100k)
const int BTN_GAS   = 1; // GPIO 1 (Tombol Gas)
const int BTN_REM   = 2; // GPIO 2 (Tombol Rem)

// Filter Kehalusan Setir 100k
float steerSmoothed = 2048.0; 
float alpha = 0.08; // Semakin kecil (misal 0.05) semakin halus & tenang

void setup() {
  // Config Pin Tombol (Aktif LOW)
  pinMode(BTN_GAS, INPUT_PULLUP);
  pinMode(BTN_REM, INPUT_PULLUP);

  // Bluetooth Serial (Nama yang muncul di HP)
  SerialBT.begin("Cek_Setir_BUSSID"); 
}

void loop() {
  // Jika Bluetooth sudah terhubung ke aplikasi Terminal di HP
  if (SerialBT.hasClient()) { 
    // 1. Baca & Filter Potensio Setir
    int rawSteer = analogRead(PIN_SETIR);
    steerSmoothed = (alpha * rawSteer) + ((1.0 - alpha) * steerSmoothed);

    // 2. Baca Tombol Gas & Rem
    String statusGas = (digitalRead(BTN_GAS) == LOW) ? "DIPENCET" : "OFF";
    String statusRem = (digitalRead(BTN_REM) == LOW) ? "DIPENCET" : "OFF";

    // 3. Kirim Tampilan Data ke Serial Terminal
    SerialBT.print("Setir: ");
    SerialBT.print((int)steerSmoothed);
    SerialBT.print(" | Gas: ");
    SerialBT.print(statusGas);
    SerialBT.print(" | Rem: ");
    Serial.println(statusRem); // Kirim via Bluetooth
    SerialBT.println(statusRem);
  }

  delay(20); // Jeda pembacaan
}
