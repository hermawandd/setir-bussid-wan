#include <Arduino.h>

const int PIN_SETIR = 0; // GPIO 0 (Potensio Setir)

// Parameter Smoothing khusus Potensio 100k
float nilaiSmoothed = 2048.0; 
float alpha = 0.08; // Semakin kecil (misal 0.05) semakin halus

void setup() {
  // Inisialisasi Serial untuk ESP32-C3 USB CDC
  Serial.begin(115200);
  
  // Tunggu koneksi USB siap (maksimal 2 detik)
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 2000)) {
    delay(10);
  }
}

void loop() {
  int raw = analogRead(PIN_SETIR);

  // Exponential Moving Average (EMA) Filter
  nilaiSmoothed = (alpha * raw) + ((1.0 - alpha) * nilaiSmoothed);

  // Tampilkan ke Serial Terminal
  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print(" | Halus: ");
  Serial.println((int)nilaiSmoothed);

  delay(20);
}
