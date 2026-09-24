#include <Arduino.h>

const int PIN_SETIR = 0; // GPIO 0 (Potensio Setir)

float nilaiSmoothed = 2048.0; 
float alpha = 0.08; // Filter kehalusan

void setup() {
  // Paksa aktifkan USB CDC bawaan ESP32-C3
  USBSerial.begin(115200);
}

void loop() {
  int raw = analogRead(PIN_SETIR);

  // Exponential Moving Average (EMA) Filter
  nilaiSmoothed = (alpha * raw) + ((1.0 - alpha) * nilaiSmoothed);

  // Kirim data langsung via Hardware USB CDC
  USBSerial.print("Raw: ");
  USBSerial.print(raw);
  USBSerial.print(" | Halus: ");
  USBSerial.println((int)nilaiSmoothed);

  delay(20);
}
