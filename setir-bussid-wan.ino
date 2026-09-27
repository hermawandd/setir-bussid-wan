#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// PIN ASSIGNMENT ESP32-C3
#define POT_STEER_PIN   0     // Potensio Setir (3.3V)

#define R2_PIN          1     // Button 1 (Gas - Active LOW ke GND)
#define L2_PIN          2     // Button 2 (Rem - Active LOW ke GND)

// 5 PIN UNTUK 9 TOMBOL TAMBAHAN (Active HIGH ke 3.3V)
#define BTN_PAIR1_PIN   3     // Menampung Tombol 3 & Tombol 4
#define BTN_PAIR2_PIN   4     // Menampung Tombol 5 & Tombol 6
#define BTN_PAIR3_PIN   5     // Menampung Tombol 7 & Tombol 8
#define BTN_PAIR4_PIN   6     // Menampung Tombol 9 & Tombol 10
#define BTN_SINGLE_PIN  7     // Menampung Tombol 11 (Tombol Tunggal)

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

int readADCFiltered(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 20; i++) sum += analogRead(pin);
  return sum / 20;
}

void setup() {
  // Gas & Rem (Tetap Active LOW)
  pinMode(R2_PIN, INPUT_PULLUP);
  pinMode(L2_PIN, INPUT_PULLUP);

  // 5 Pin Tambahan Tombol Ganda (Active HIGH dengan Internal Pull-Down)
  pinMode(BTN_PAIR1_PIN, INPUT_PULLDOWN);
  pinMode(BTN_PAIR2_PIN, INPUT_PULLDOWN);
  pinMode(BTN_PAIR3_PIN, INPUT_PULLDOWN);
  pinMode(BTN_PAIR4_PIN, INPUT_PULLDOWN);
  pinMode(BTN_SINGLE_PIN, INPUT_PULLDOWN);

  // Setir Potensio
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

    // 1. PEMBACAAN DAN FILTERING POTENSIO (SAMA SEPERTI ASLI)
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

    // 2. DETEKSI LINTASAN PEMICU (SAMA SEPERTI ASLI)
    int currentX = (int)outX_smoothed;
    bool isAtCenterX = (abs(currentX) <= 2); 

    if (!isAtCenterX) {
      hasLeftCenter = true;
    } 
    else if (isAtCenterX && hasLeftCenter && !isPulsing) {
      isPulsing = true;
      centerPulseTimer = millis();
      hasLeftCenter = false; 
    }

    // 3. PEMBACAAN TOTAL 11 TOMBOL DIGITAL
    uint16_t btnState = 0;

    // --- Gas & Rem (Jalur GND) ---
    if (digitalRead(R2_PIN) == LOW)   btnState |= (1 << 0); // Tombol 1
    if (digitalRead(L2_PIN) == LOW)   btnState |= (1 << 1); // Tombol 2

    // --- GPIO 3 (Tombol 3 & 4) ---
    int valP1 = analogRead(BTN_PAIR1_PIN);
    if (valP1 > 3000)                        btnState |= (1 << 2); // Tombol 3 (Direct 3.3V)
    else if (valP1 > 1000 && valP1 <= 3000)  btnState |= (1 << 3); // Tombol 4 (Lewat Resistor 1k)

    // --- GPIO 4 (Tombol 5 & 6) ---
    int valP2 = analogRead(BTN_PAIR2_PIN);
    if (valP2 > 3000)                        btnState |= (1 << 4); // Tombol 5 (Direct 3.3V)
    else if (valP2 > 1000 && valP2 <= 3000)  btnState |= (1 << 5); // Tombol 6 (Lewat Resistor 1k)

    // --- GPIO 5 (Tombol 7 & 8) ---
    int valP3 = analogRead(BTN_PAIR3_PIN);
    if (valP3 > 3000)                        btnState |= (1 << 6); // Tombol 7 (Direct 3.3V)
    else if (valP3 > 1000 && valP3 <= 3000)  btnState |= (1 << 7); // Tombol 8 (Lewat Resistor 1k)

    // --- GPIO 6 (Tombol 9 & 10) ---
    int valP4 = analogRead(BTN_PAIR4_PIN);
    if (valP4 > 3000)                        btnState |= (1 << 8); // Tombol 9 (Direct 3.3V)
    else if (valP4 > 1000 && valP4 <= 3000)  btnState |= (1 << 9); // Tombol 10 (Lewat Resistor 1k)

    // --- GPIO 7 (Tombol 11 / Single Button) ---
    int valSingle = analogRead(BTN_SINGLE_PIN);
    if (valSingle > 2000)                    btnState |= (1 << 10); // Tombol 11 (Direct 3.3V)

    // SUSUN BUFFER HID REPORT
    bufferLaporan[0] = btnState & 0xFF;         
    bufferLaporan[1] = (btnState >> 8) & 0xFF;  
    bufferLaporan[2] = 8; // Hat Switch Netral                      

    // 4. PENANGANAN REFRESH PULSE (SAMA SEPERTI ASLI)
    if (isPulsing) {
      bufferLaporan[3] = 0; 
      bufferLaporan[4] = 0; 
      
      if (millis() - centerPulseTimer >= 25) {
        isPulsing = false; 
      }
    } else {
      bufferLaporan[3] = (int8_t)outX_smoothed;   
      bufferLaporan[4] = (int8_t)outY_smoothed;   
    }

    bufferLaporan[5] = 0;
    bufferLaporan[6] = 0;
    bufferLaporan[7] = 0;
    bufferLaporan[8] = 0;

    // 5. CONDITIONAL SENDING (SAMA SEPERTI ASLI)
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
