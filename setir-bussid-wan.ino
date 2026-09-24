#include <Arduino.h>

const int PIN_SETIR = 0; // GPIO 0 (Potensio Setir)

// Parameter Smoothing khusus Potensio 100k
float nilaiSmoothed = 2048.0; 
float alpha = 0.08; // Semakin kecil (misal 0.05), semakin halus/anteng tapi respon sedikit lambat

void setup() {
  Serial.begin(115200);
}

void loop() {
  int raw = analogRead(PIN_SETIR);

  // Exponential Moving Average (EMA) Filter
  nilaiSmoothed = (alpha * raw) + ((1.0 - alpha) * nilaiSmoothed);

  // Tampilkan ke Serial Monitor
  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print(" | Halus: ");
  Serial.println((int)nilaiSmoothed);

  delay(15);
}
