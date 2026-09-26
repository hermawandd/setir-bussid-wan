#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// =====================================================
// PINOUT ESP32-C3 SUPERMINI
// =====================================================
#define POT_STEER_PIN   0     // Potensio Setir (GPIO 0 / ADC1_CH0)

// ALOKASI 10 TOMBOL DIGITAL (Bisa Diatur di Mapping K2er)
#define BTN1_PIN        1     // Pedal Gas (Button 1)
#define BTN2_PIN        2     // Pedal Rem (Button 2)
#define BTN3_PIN        3     // Maju / Mundur (Button 3)
#define BTN4_PIN        4     // Wiper (Button 4)
#define BTN5_PIN        5     // Sein Kiri (Button 5)
#define BTN6_PIN        6     // Sein Kanan (Button 6)
#define BTN7_PIN        7     // Kamera (Button 7)
#define BTN8_PIN        8     // Klakson / Telolet (Button 8)
#define BTN9_PIN        9     // Lampu Hazard (Button 9)
#define BTN10_PIN       10    // Rem Tangan / Pintu (Button 10)

#define DEVICE_NAME         "SETIR BUS V2"

// Filter Halus Potensio
float steerSmoothed = 2048.0;
float alpha = 0.1; // Responsif & Stabil

// =====================================================
// TABEL X & Y BUSSID (360 DEGREE - 80 INDEKS)
// =====================================================
const int8_t tableX[80] = {
   0, 10, 20, 30, 40, 50, 59, 67, 76, 83,
  90, 97,102,107,112,115,118,120,122,123,
 124,123,122,120,118,115,112,107,102, 97,
  90, 83, 76, 67, 59, 50, 40, 30, 20, 10,
   0,-10,-20,-30,-40,-50,-59,-67,-76,-83,
 -90,-97,-102,-107,-112,-115,-118,-120,-122,-123,
-124,-123,-122,-120,-118,-115,-112,-107,-102,-97,
 -90,-83,-76,-67,-59,-50,-40,-30,-20,-10
};

const int8_t tableY[80] = {
 -124,-123,-122,-120,-118,-115,-112,-107,-102,-97,
 -90,-83,-76,-67,-59,-50,-40,-30,-20,-10,
   0, 10, 20, 30, 40, 50, 59, 67, 76, 83,
  90, 97,102,107,112,115,118,120,122,123,
 124,123,122,120,118,115,112,107,102,97,
 90,83,76,67,59,50,40,30,20,10,
  0,-10,-20,-30,-40,-50,-59,-67,-76,-83,
 -90,-97,-102,-107,-112,-115,-118,-120,-122,-123
};

BLEHIDDevice* hid;
BLECharacteristic* inputGamepad;
bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    pServer->getAdvertising()->start();
  }
};

const uint8_t reportMapGamepad[] = {
  0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
  // 16 Tombol Digital (Button 1 s/d Button 16)
  0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02,
  // Hat Switch / D-Pad
  0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x02,
  0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
  // Sumbu Setir: X, Y
  0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  // Sumbu Right Stick: Z, Rz
  0x05, 0x01, 0x09, 0x32, 0x09, 0x35, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  // Analog Triggers: L2, R2
  0x05, 0x02, 0x09, 0xC5, 0x09, 0xC4, 0x15, 0x00, 0x25, 0xFF, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  0xC0
};

int readADCFiltered(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 15; i++) sum += analogRead(pin);
  return sum / 15;
}

void setup() {
  // Inisialisasi Seluruh Pin Tombol (GND Switch)
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(BTN5_PIN, INPUT_PULLUP);
  pinMode(BTN6_PIN, INPUT_PULLUP);
  pinMode(BTN7_PIN, INPUT_PULLUP);
  pinMode(BTN8_PIN, INPUT_PULLUP);
  pinMode(BTN9_PIN, INPUT_PULLUP);
  pinMode(BTN10_PIN, INPUT_PULLUP);

  // Inisialisasi Pin Potensio Setir
  pinMode(POT_STEER_PIN, INPUT);

  analogReadResolution(12);

  BLEDevice::init(DEVICE_NAME);
  BLESecurity* pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSecurity->setCapability(ESP_IO_CAP_NONE);

  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  hid = new BLEHIDDevice(pServer);
  inputGamepad = hid->inputReport(0);

  hid->pnp(0x02, 0x05ac, 0x820a, 0x0210);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap((uint8_t*)reportMapGamepad, sizeof(reportMapGamepad));
  hid->startServices();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setAppearance(0x03C4);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();
}

void loop() {
  if (deviceConnected) {
    uint8_t bufferLaporan[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    // 1. BACA POTENSIO SETIR (REALTIME)
    int rawPot = readADCFiltered(POT_STEER_PIN);
    steerSmoothed = (alpha * rawPot) + ((1.0 - alpha) * steerSmoothed);

    int potLimited = constrain((int)steerSmoothed, 50, 4000);
    int totalStep = map(potLimited, 50, 4000, 0, 319);
    
    // --- FITUR AUTO-CENTER LOCK ---
    // Mengunci posisi tengah murni (step 160) ketika setir posisi lurus
    if (totalStep >= 157 && totalStep <= 163) {
      totalStep = 160; 
    }

    int indexPoint = totalStep % 80;

    // 2. BACA 10 TOMBOL DIGITAL (BITMASK 2 BYTE)
    uint16_t btnState = 0;

    if (digitalRead(BTN1_PIN) == LOW)  btnState |= (1 << 0);  // Button 1 (Gas)
    if (digitalRead(BTN2_PIN) == LOW)  btnState |= (1 << 1);  // Button 2 (Rem)
    if (digitalRead(BTN3_PIN) == LOW)  btnState |= (1 << 2);  // Button 3
    if (digitalRead(BTN4_PIN) == LOW)  btnState |= (1 << 3);  // Button 4
    if (digitalRead(BTN5_PIN) == LOW)  btnState |= (1 << 4);  // Button 5
    if (digitalRead(BTN6_PIN) == LOW)  btnState |= (1 << 5);  // Button 6
    if (digitalRead(BTN7_PIN) == LOW)  btnState |= (1 << 6);  // Button 7
    if (digitalRead(BTN8_PIN) == LOW)  btnState |= (1 << 7);  // Button 8
    if (digitalRead(BTN9_PIN) == LOW)  btnState |= (1 << 8);  // Button 9
    if (digitalRead(BTN10_PIN) == LOW) btnState |= (1 << 9);  // Button 10

    bufferLaporan[0] = btnState & 0xFF;         // Byte 0: Button 1 s/d 8
    bufferLaporan[1] = (btnState >> 8) & 0xFF;  // Byte 1: Button 9 s/d 10

    // 3. SUMBU X & Y SETIR (POSISI ABSOLUT MELINGKAR)
    bufferLaporan[2] = -tableX[indexPoint]; // - Digunakan agar putaran searah
    bufferLaporan[3] = tableY[indexPoint];

    bufferLaporan[4] = 0;
    bufferLaporan[5] = 0;
    bufferLaporan[6] = 0;
    bufferLaporan[7] = 0;

    inputGamepad->setValue(bufferLaporan, sizeof(bufferLaporan));
    inputGamepad->notify();
  }
  delay(10);
}
