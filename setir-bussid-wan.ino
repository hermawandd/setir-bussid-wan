#include <Arduino.h>
#include <BleGamepad.h>

// Nama Bluetooth yang bakal muncul di HP
BleGamepad bleGamepad("Setir DIY BUSSID", "DIY Project", 100);

// Pin Out ESP32-C3
const int PIN_SETIR = 0;   // GPIO 0 untuk Potensio Setir (Analog)
const int PIN_GAS   = 1;   // GPIO 1 untuk Potensio Gas (Analog) - Opsional
const int PIN_REM   = 2;   // GPIO 2 untuk Potensio Rem (Analog) - Opsional

// Pin Tombol (Digital INPUT_PULLUP)
const int BTN_KLAKSON    = 3; // GPIO 3
const int BTN_SEIN_KIRI  = 4; // GPIO 4
const int BTN_SEIN_KANAN = 5; // GPIO 5
const int BTN_DIM        = 6; // GPIO 6

void setup() {
  // Konfigurasi Pin Tombol
  pinMode(BTN_KLAKSON, INPUT_PULLUP);
  pinMode(BTN_SEIN_KIRI, INPUT_PULLUP);
  pinMode(BTN_SEIN_KANAN, INPUT_PULLUP);
  pinMode(BTN_DIM, INPUT_PULLUP);

  // Inisialisasi Bluetooth Gamepad
  BleGamepadConfig config;
  config.setAutoReport(true);
  config.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  
  // Aktifkan hanya 1 Sumbu Analog (X = Setir) biar gak bikin LStick liar
  config.setWhichAxes(true, false, false, false, false, false, false, false); // Hanya X-axis (Setir)
  config.setButtonCount(4); // 4 Tombol
  
  bleGamepad.begin(&config);
}

void loop() {
  if (bleGamepad.isConnected()) {
    // 1. Baca Potensio Setir (GPIO 0)
    int rawSteer = analogRead(PIN_SETIR);
    
    // Map nilai ADC ESP32 (0 - 4095) ke Sumbu Gamepad (-32767 sampai 32767)
    int steerValue = map(rawSteer, 0, 4095, -32767, 32767);
    bleGamepad.setX(steerValue);

    // 2. Baca Tombol-Tombol (Aktif LOW / Terhubung ke GND)
    if (digitalRead(BTN_KLAKSON) == LOW)    bleGamepad.press(BUTTON_1); else bleGamepad.release(BUTTON_1);
    if (digitalRead(BTN_SEIN_KIRI) == LOW)  bleGamepad.press(BUTTON_2); else bleGamepad.release(BUTTON_2);
    if (digitalRead(BTN_SEIN_KANAN) == LOW) bleGamepad.press(BUTTON_3); else bleGamepad.release(BUTTON_3);
    if (digitalRead(BTN_DIM) == LOW)        bleGamepad.press(BUTTON_4); else bleGamepad.release(BUTTON_4);

    // 3. DELAY WAJIB: Mencegah spam data / layar nyentuh sendiri
    delay(10);
  }
}
