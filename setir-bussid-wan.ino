#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// =====================================================
// PINOUT KHUSUS ESP32-C3 SUPER MINI
// =====================================================
#define POT_STEER_PIN   0     // Potensio Setir 100k (GPIO 0)
#define R2_PIN          1     // Pedal Gas / Tombol 1 (GPIO 1)
#define L2_PIN          2     // Pedal Rem / Tombol 2 (GPIO 2)

#define DEVICE_NAME         "SETIR BUS V2"
#define DEVICE_MANUFACTURER "ESP32"

// Filter Kehalusan Potensio Setir (EMA Filter)
float steerSmoothed = 2048.0;
float alpha = 0.03;

// =====================================================
// TABEL X & Y BUSSID (360 DEGREE - 80 LANGKAH)
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

// =====================================================
// DESCRIPTOR HID GAMEPAD UNIVERSAL (STATIONARY GAMEPAD)
// =====================================================
const uint8_t reportMapGamepad[] = {
  0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
  0x09, 0x05,        // USAGE (Gamepad)
  0xA1, 0x01,        // COLLECTION (Application)
  0x85, 0x01,        //   REPORT_ID (1)

  // 1. Sumbu X, Y (-127 s/d 127)
  0x05, 0x01,        //   USAGE_PAGE (Generic Desktop)
  0x09, 0x30,        //   USAGE (X)
  0x09, 0x31,        //   USAGE (Y)
  0x15, 0x81,        //   LOGICAL_MINIMUM (-127)
  0x25, 0x7F,        //   LOGICAL_MAXIMUM (127)
  0x75, 0x08,        //   REPORT_SIZE (8)
  0x95, 0x02,        //   REPORT_COUNT (2)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)

  // 2. Tombol (Button 1 = Gas, Button 2 = Rem)
  0x05, 0x09,        //   USAGE_PAGE (Button)
  0x19, 0x01,        //   USAGE_MINIMUM (Button 1)
  0x29, 0x02,        //   USAGE_MAXIMUM (Button 2)
  0x15, 0x00,        //   LOGICAL_MINIMUM (0)
  0x25, 0x01,        //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,        //   REPORT_SIZE (1)
  0x95, 0x02,        //   REPORT_COUNT (2)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)

  // Padding bit pemicu (6-bit padding agar total 1 byte)
  0x75, 0x06,        //   REPORT_SIZE (6)
  0x95, 0x01,        //   REPORT_COUNT (1)
  0x81, 0x03,        //   INPUT (Cnst,Var,Abs)

  0xC0               // END_COLLECTION
};

int readADCFiltered(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 15; i++) {
    sum += analogRead(pin);
  }
  return sum / 15;
}

void setup() {
  Serial.begin(115200);
  pinMode(R2_PIN, INPUT_PULLUP);
  pinMode(L2_PIN, INPUT_PULLUP);
  pinMode(POT_STEER_PIN, INPUT);

  analogReadResolution(12);

  BLEDevice::init(DEVICE_NAME);
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  hid = new BLEHIDDevice(pServer);
  inputGamepad = hid->inputReport(1); // Report ID 1

  hid->pnp(0x01, 0x02e8, 0x000a, 0x0110); // Standard Controller Vendor ID
  hid->hidInfo(0x00, 0x01);
  hid->reportMap((uint8_t*)reportMapGamepad, sizeof(reportMapGamepad));
  hid->startServices();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setAppearance(0x03C4); // Joysticks / Gamepad
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();
}

void loop() {
  if (deviceConnected) {
    uint8_t bufferLaporan[3] = {0, 0, 0};

    // 1. BACA & FILTER POTENSIO SETIR (GPIO 0)
    int rawPot = readADCFiltered(POT_STEER_PIN);
    steerSmoothed = (alpha * rawPot) + ((1.0 - alpha) * steerSmoothed);

    // 2. CONSTRAIN DEADZONE UJUNG (FREE PLAY 0.5 PUTARAN)
    int potLimited = constrain((int)steerSmoothed, 500, 3595);

    // 3. MAPPING KE 80 INDEKS TABEL MELINGKAR
    int indexPoint = map(potLimited, 500, 3595, 0, 79);

    // 4. SUMBU SETIR X & Y
    bufferLaporan[0] = tableX[indexPoint];
    bufferLaporan[1] = tableY[indexPoint];

    // 5. TOMBOL PEDAL GAS (Button 1) & REM (Button 2)
    uint8_t buttons = 0;
    if (digitalRead(R2_PIN) == LOW) buttons |= (1 << 0); // Button 1 (Gas)
    if (digitalRead(L2_PIN) == LOW) buttons |= (1 << 1); // Button 2 (Rem)
    bufferLaporan[2] = buttons;

    // 6. KIRIM KE HP VIA BLE HID
    inputGamepad->setValue(bufferLaporan, sizeof(bufferLaporan));
    inputGamepad->notify();
  }
  delay(10);
}
