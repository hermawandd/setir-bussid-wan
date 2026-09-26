#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// =====================================================
// PINOUT ESP32-C3 SUPERMINI (POTENSIO)
// =====================================================
#define POT_STEER_PIN   0     // Potensio Setir (GPIO 0 / ADC1_CH0)
#define R2_PIN          1     // Pedal Gas (GPIO 1)
#define L2_PIN          2     // Pedal Rem (GPIO 2)

#define DEVICE_NAME         "SETIR BUS V2"
#define DEVICE_MANUFACTURER "ESP32"

// Filter Kehalusan Potensio (EMA Filter)
float steerSmoothed = 2048.0;
float alpha = 0.05;

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

// =====================================================
// BLE HID & REPORT MAP
// =====================================================
BLEHIDDevice* hid;
BLECharacteristic* inputGamepad;
bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    pServer->getAdvertising()->start();
  }
};

const uint8_t reportMapGamepad[] = {
  0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
  0x09, 0x05,        // USAGE (Gamepad)
  0xA1, 0x01,        // COLLECTION (Application)
  
  // 8 Tombol Utama
  0x05, 0x09,        //     USAGE_PAGE (Button)
  0x19, 0x01,        //     USAGE_MINIMUM (Button 1)
  0x29, 0x08,        //     USAGE_MAXIMUM (Button 8)
  0x15, 0x00,        //     LOGICAL_MINIMUM (0)
  0x25, 0x01,        //     LOGICAL_MAXIMUM (1)
  0x75, 0x01,        //     REPORT_SIZE (1)
  0x95, 0x08,        //     REPORT_COUNT (8)
  0x81, 0x02,        //     INPUT (Data,Var,Abs)

  // Hat Switch / D-Pad (4-bit)
  0x05, 0x01,        //     USAGE_PAGE (Generic Desktop)
  0x09, 0x39,        //     USAGE (Hat switch)
  0x15, 0x00,        //     LOGICAL_MINIMUM (0)
  0x25, 0x07,        //     LOGICAL_MAXIMUM (7)
  0x35, 0x00,        //     PHYSICAL_MINIMUM (0)
  0x46, 0x3B, 0x01,  //     PHYSICAL_MAXIMUM (315)
  0x65, 0x14,        //     UNIT (English Rotation: Angular Pos)
  0x75, 0x04,        //     REPORT_SIZE (4)
  0x95, 0x01,        //     REPORT_COUNT (1)
  0x81, 0x02,        //     INPUT (Data,Var,Abs)

  // Padding 4-bit
  0x75, 0x04,        //     REPORT_SIZE (4)
  0x95, 0x01,        //     REPORT_COUNT (1)
  0x81, 0x03,        //     INPUT (Cnst,Var,Abs)

  // Sumbu Setir: X, Y (-127 s/d 127)
  0x05, 0x01,        //     USAGE_PAGE (Generic Desktop)
  0x09, 0x30,        //     USAGE (X)
  0x09, 0x31,        //     USAGE (Y)
  0x15, 0x81,        //     LOGICAL_MINIMUM (-127)
  0x25, 0x7F,        //     LOGICAL_MAXIMUM (127)
  0x75, 0x08,        //     REPORT_SIZE (8)
  0x95, 0x02,        //     REPORT_COUNT (2)
  0x81, 0x02,        //     INPUT (Data,Var,Abs)

  // Sumbu Joystick Kanan: Z, Rz
  0x05, 0x01,        //     USAGE_PAGE (Generic Desktop)
  0x09, 0x32,        //     USAGE (Z)
  0x09, 0x35,        //     USAGE (Rz)
  0x15, 0x81,        //     LOGICAL_MINIMUM (-127)
  0x25, 0x7F,        //     LOGICAL_MAXIMUM (127)
  0x75, 0x08,        //     REPORT_SIZE (8)
  0x95, 0x02,        //     REPORT_COUNT (2)
  0x81, 0x02,        //     INPUT (Data,Var,Abs)

  // Analog Pedal: L2, R2 (0 s/d 255)
  0x05, 0x02,        //     USAGE_PAGE (Simulation Controls)
  0x09, 0xC5,        //     USAGE (Brake)
  0x09, 0xC4,        //     USAGE (Accelerator)
  0x15, 0x00,        //     LOGICAL_MINIMUM (0)
  0x25, 0xFF,        //     LOGICAL_MAXIMUM (255)
  0x75, 0x08,        //     REPORT_SIZE (8)
  0x95, 0x02,        //     REPORT_COUNT (2)
  0x81, 0x02,        //     INPUT (Data,Var,Abs)
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

  // BLE INIT (Menggunakan format bonding seperti kodingan teman)
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

    // 1. BACA POTENSIO (Tegangan Analog ESP32-C3)
    int rawPot = readADCFiltered(POT_STEER_PIN);
    steerSmoothed = (alpha * rawPot) + ((1.0 - alpha) * steerSmoothed);

    // Batasi rentang ADC potensio (misal 100 - 3900)
    int potLimited = constrain((int)steerSmoothed, 100, 3900);

    // 2. MAPPING NILAI POTENSIO KE 80 INDEKS TABEL
    int indexPoint = map(potLimited, 100, 3900, 0, 79);

    // 3. SET PAKET HID (Sama persis struktur Byte-nya dengan teman Boss)
    bufferLaporan[0] = 0; // Buttons (kosong)
    bufferLaporan[1] = 8; // Hat switch released (8)
    
    // Byte 2 & 3 untuk Sumbu Setir X & Y
    bufferLaporan[2] = tableX[indexPoint];
    bufferLaporan[3] = tableY[indexPoint];

    // Byte 4 & 5 Joystick Kanan (kosong)
    bufferLaporan[4] = 0;
    bufferLaporan[5] = 0;

    // Byte 6 & 7 Pedal Rem (L2) & Gas (R2)
    bufferLaporan[6] = (digitalRead(L2_PIN) == LOW) ? 255 : 0; // Brake
    bufferLaporan[7] = (digitalRead(R2_PIN) == LOW) ? 255 : 0; // Accelerator

    // KIRIM PAKET
    inputGamepad->setValue(bufferLaporan, sizeof(bufferLaporan));
    inputGamepad->notify();
  }
  delay(10);
}
