#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// PIN ASSIGNMENT ESP32-C3
#define POT_STEER_PIN   0     // Potensio Setir

#define R2_PIN          1     // Button 1 (Gas)
#define L2_PIN          2     // Button 2 (Rem)

#define BTN3_PIN        3     // Button 3
#define BTN4_PIN        4     // Button 4
#define BTN5_PIN        5     // Button 5
#define BTN6_PIN        6     // Button 6
#define BTN7_PIN        7     // Button 7
#define BTN8_PIN        8     // Button 8
#define BTN9_PIN        9     // Button 9
#define BTN10_PIN       10    // Button 10

#define DEVICE_NAME         "SETIR BUS V2"

// Filter Halus Potensio & Output
float steerSmoothed = 2048.0;
float alpha = 0.05;          // Filter ADC

float outX_smoothed = 0.0;
float outY_smoothed = -124.0;
float alphaOut = 0.25;       // Filter X & Y

// Variabel Trigger Center Pulse
bool hasLeftCenter = false;
unsigned long centerPulseTimer = 0;
bool isPulsing = false;

// Buffer penampung data sebelumnya (Conditional Send)
uint8_t lastBuffer[9] = {0};

// TABEL LOOKUP KOORDINAT LINGKARAN (80 Titik)
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
  // 16 Tombol Digital (Byte 0 & Byte 1)
  0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02,
  // Hat Switch / D-Pad Netral (Byte 2)
  0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x02,
  0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
  // Sumbu Setir X & Y (Byte 3 & Byte 4)
  0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  // Sumbu Right Stick Z & Rz (Byte 5 & Byte 6)
  0x05, 0x01, 0x09, 0x32, 0x09, 0x35, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  // Analog Triggers (Byte 7 & Byte 8)
  0x05, 0x02, 0x09, 0xC5, 0x09, 0xC4, 0x15, 0x00, 0x25, 0xFF, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  0xC0
};

int readADCFiltered(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 20; i++) sum += analogRead(pin);
  return sum / 20;
}

void setup() {
  pinMode(R2_PIN, INPUT_PULLUP);
  pinMode(L2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(BTN5_PIN, INPUT_PULLUP);
  pinMode(BTN6_PIN, INPUT_PULLUP);
  pinMode(BTN7_PIN, INPUT_PULLUP);
  pinMode(BTN8_PIN, INPUT_PULLUP);
  pinMode(BTN9_PIN, INPUT_PULLUP);
  pinMode(BTN10_PIN, INPUT_PULLUP);

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
    uint8_t bufferLaporan[9] = {0};

    // 1. PEMBACAAN DAN FILTERING POTENSIO
    int rawPot = readADCFiltered(POT_STEER_PIN);
    steerSmoothed = (alpha * rawPot) + ((1.0 - alpha) * steerSmoothed);

    int potLimited = constrain((int)steerSmoothed, 50, 4000);
    int totalStep = map(potLimited, 50, 4000, 0, 319);
    int indexPoint = totalStep % 80;

    // Target X dan Y dari Tabel
    float targetX = -tableX[indexPoint];
    float targetY = tableY[indexPoint];

    // Smooth filter
    outX_smoothed = (alphaOut * targetX) + ((1.0 - alphaOut) * outX_smoothed);
    outY_smoothed = (alphaOut * targetY) + ((1.0 - alphaOut) * outY_smoothed);

    // 2. DETEKSI LINTASAN PEMICU (BERPATOKAN MURNI PADA X = 0)
    int currentX = (int)outX_smoothed;
    bool isAtCenterX = (abs(currentX) <= 2); // Rapat di X = 0

    if (!isAtCenterX) {
      // Setir sedang belok (X bergerak menjauhi 0)
      hasLeftCenter = true;
    } 
    else if (isAtCenterX && hasLeftCenter && !isPulsing) {
      // PEMICU AKTIF: Setir menyentuh / melintasi X = 0!
      isPulsing = true;
      centerPulseTimer = millis();
      hasLeftCenter = false; // Kunci pemicu agar 1x jalan saja
    }

    // 3. PEMBACAAN 10 TOMBOL DIGITAL
    uint16_t btnState = 0;
    if (digitalRead(R2_PIN) == LOW)   btnState |= (1 << 0);
    if (digitalRead(L2_PIN) == LOW)   btnState |= (1 << 1);
    if (digitalRead(BTN3_PIN) == LOW)  btnState |= (1 << 2);
    if (digitalRead(BTN4_PIN) == LOW)  btnState |= (1 << 3);
    if (digitalRead(BTN5_PIN) == LOW)  btnState |= (1 << 4);
    if (digitalRead(BTN6_PIN) == LOW)  btnState |= (1 << 5);
    if (digitalRead(BTN7_PIN) == LOW)  btnState |= (1 << 6);
    if (digitalRead(BTN8_PIN) == LOW)  btnState |= (1 << 7);
    if (digitalRead(BTN9_PIN) == LOW)  btnState |= (1 << 8);
    if (digitalRead(BTN10_PIN) == LOW) btnState |= (1 << 9);

    // SUSUN BUFFER HID REPORT
    bufferLaporan[0] = btnState & 0xFF;         
    bufferLaporan[1] = (btnState >> 8) & 0xFF;  
    bufferLaporan[2] = 8; // Hat Switch Netral                      

    // 4. PENANGANAN REFRESH PULSE (X DAN Y DIRESET KE 0 SEMENTARA)
    if (isPulsing) {
      // Kirim X = 0 dan Y = 0 selama 25 ms
      bufferLaporan[3] = 0; 
      bufferLaporan[4] = 0; 
      
      if (millis() - centerPulseTimer >= 25) {
        isPulsing = false; // Setelah 25ms selesai, lepas ke data asli
      }
    } else {
      // Kirim data koordinat asli
      bufferLaporan[3] = (int8_t)outX_smoothed;   
      bufferLaporan[4] = (int8_t)outY_smoothed;   
    }

    bufferLaporan[5] = 0;
    bufferLaporan[6] = 0;
    bufferLaporan[7] = 0;
    bufferLaporan[8] = 0;

    // 5. CONDITIONAL SENDING
    bool dataChanged = false;
    for (int i = 0; i < 9; i++) {
      if (bufferLaporan[i] != lastBuffer[i]) {
        dataChanged = true;
        break;
      }
    }

    if (dataChanged) {
      inputGamepad->setValue(bufferLaporan, sizeof(bufferLaporan));
      inputGamepad->notify();
      memcpy(lastBuffer, bufferLaporan, sizeof(bufferLaporan));
    }
  }
  delay(10);
}
