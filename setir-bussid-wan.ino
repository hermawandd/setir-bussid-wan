#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// PIN ASSIGNMENT ESP32-C3 SUPERMINI (PIN AMAN BEBAS NGE-HANG)
#define POT_STEER_PIN   0     // Potensio Setir (Analog 3.3V)

// TOTAL 10 TOMBOL DIGITAL DIRECT GND (KEBAL NOISE & BEBAS STRAPPING)
#define BTN_1_PIN       1     // Tombol 1  (Gas / R2)
#define BTN_2_PIN       2     // Tombol 2  (Rem / L2)
#define BTN_3_PIN       3     // Tombol 3  (Klakson)
#define BTN_4_PIN       4     // Tombol 4  (Sein Kiri)
#define BTN_5_PIN       5     // Tombol 5  (Sein Kanan)
#define BTN_6_PIN       6     // Tombol 6  (Lampu Utama)
#define BTN_7_PIN       7     // Tombol 7  (Wiper)
#define BTN_8_PIN       10    // Tombol 8  (Handbrake)
#define BTN_9_PIN       20    // Tombol 9  (Kamera)
#define BTN_10_PIN      21    // Tombol 10 (Gigi / Transmisi)

#define DEVICE_NAME     "SETIR BUS V2"

// Filter Halus & Super Responsif khusus Potensio B10K
float steerSmoothed = 2048.0;
float alpha = 0.20;          // Respon cepat untuk belok tipis/milimeter

float outX_smoothed = 0.0;
float outY_smoothed = -124.0;
float alphaOut = 0.40;       // Mengikuti gerakan jari secara instan

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
  void onConnect(BLEServer* pServer) { 
    deviceConnected = true; 
    memset(lastBuffer, 0xFF, sizeof(lastBuffer)); // Force send laporan pertama
  }
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

// Filter Sampling Ringan (5x saja) agar respons instan & hilangkan lag mikro
int readADCFiltered(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 5; i++) sum += analogRead(pin);
  return sum / 5;
}

void setup() {
  // SET SEMUA PIN TOMBOL KE INPUT_PULLUP (DIRECT GND)
  pinMode(BTN_1_PIN, INPUT_PULLUP);
  pinMode(BTN_2_PIN, INPUT_PULLUP);
  pinMode(BTN_3_PIN, INPUT_PULLUP);
  pinMode(BTN_4_PIN, INPUT_PULLUP);
  pinMode(BTN_5_PIN, INPUT_PULLUP);
  pinMode(BTN_6_PIN, INPUT_PULLUP);
  pinMode(BTN_7_PIN, INPUT_PULLUP);
  pinMode(BTN_8_PIN, INPUT_PULLUP);
  pinMode(BTN_9_PIN, INPUT_PULLUP);
  pinMode(BTN_10_PIN, INPUT_PULLUP);

  // Setir Potensio (3.3V Analog)
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

    // 1. PEMBACAAN POTENSIO SETIR (SUPER LINIER TANPA RESET X0)
    int rawPot = readADCFiltered(POT_STEER_PIN);
    steerSmoothed = (alpha * rawPot) + ((1.0 - alpha) * steerSmoothed);

    // Pembatasan ADC 12-bit murni
    int potLimited = constrain((int)steerSmoothed, 10, 4085);
    int totalStep = map(potLimited, 10, 4085, 0, 319);
    int indexPoint = totalStep % 80;

    float targetX = -tableX[indexPoint];
    float targetY = tableY[indexPoint];

    outX_smoothed = (alphaOut * targetX) + ((1.0 - alphaOut) * outX_smoothed);
    outY_smoothed = (alphaOut * targetY) + ((1.0 - alphaOut) * outY_smoothed);

    // 2. PEMBACAAN 10 TOMBOL DIGITAL DIRECT GND
    uint16_t btnState = 0;

    if (digitalRead(BTN_1_PIN) == LOW)   btnState |= (1 << 0);  // Tombol 1
    if (digitalRead(BTN_2_PIN) == LOW)   btnState |= (1 << 1);  // Tombol 2
    if (digitalRead(BTN_3_PIN) == LOW)   btnState |= (1 << 2);  // Tombol 3
    if (digitalRead(BTN_4_PIN) == LOW)   btnState |= (1 << 3);  // Tombol 4
    if (digitalRead(BTN_5_PIN) == LOW)   btnState |= (1 << 4);  // Tombol 5
    if (digitalRead(BTN_6_PIN) == LOW)   btnState |= (1 << 5);  // Tombol 6
    if (digitalRead(BTN_7_PIN) == LOW)   btnState |= (1 << 6);  // Tombol 7
    if (digitalRead(BTN_8_PIN) == LOW)   btnState |= (1 << 7);  // Tombol 8
    if (digitalRead(BTN_9_PIN) == LOW)   btnState |= (1 << 8);  // Tombol 9
    if (digitalRead(BTN_10_PIN) == LOW)  btnState |= (1 << 9);  // Tombol 10

    // SUSUN BUFFER HID REPORT
    bufferLaporan[0] = btnState & 0xFF;         
    bufferLaporan[1] = (btnState >> 8) & 0xFF;  
    bufferLaporan[2] = 8; // Hat Switch Netral                      

    // LANGSUNG MASUKKAN SUMBU SETIR TANPA INTERUPSI PULSE
    bufferLaporan[3] = (int8_t)outX_smoothed;   
    bufferLaporan[4] = (int8_t)outY_smoothed;   

    bufferLaporan[5] = 0;
    bufferLaporan[6] = 0;
    bufferLaporan[7] = 0;
    bufferLaporan[8] = 0;

    // 3. CONDITIONAL SENDING
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
