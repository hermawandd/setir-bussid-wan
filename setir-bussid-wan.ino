#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <Wire.h>

// =====================================================
// PINOUT ESP32-C3 SUPERMINI
// =====================================================
#define BTN_CAL_PIN     0     // Tombol Reset Titik Tengah (Direct GND)

#define I2C_SDA_PIN     1     // AS5600 SDA
#define I2C_SCL_PIN     2     // AS5600 SCL

#define BTN_1_PIN       3     // Button 1
#define BTN_2_PIN       4     // Button 2
#define BTN_3_PIN       5     // Button 3
#define BTN_4_PIN       6     // Button 4
#define BTN_5_PIN       7     // Button 5
#define BTN_6_PIN       10    // Button 6

#define PEDAL_GAS_PIN   20    // Pedal Gas (R2)
#define PEDAL_REM_PIN   21    // Pedal Rem (L2)

#define DEVICE_NAME     "SETIR HF"

// =====================================================
// KONFIGURASI AS5600 & LOGIKA MULTI-TURN
// =====================================================
#define AS5600_ADDR     0x36

uint16_t rawAnglePrevious = 0;
long totalRawPulse = 0; 
long centerOffset = 0;

float steerSmoothed = 0.0;
float alphaSteer = 0.25; // Low-pass filter setir

float outX_smoothed = 0.0;
float outY_smoothed = -124.0;
float alphaOut = 0.35;   // Low-pass filter output

uint8_t lastBuffer[9] = {0};

// TABEL LOOKUP KOORDINAT LINGKARAN (80 Titik)
const float tableX[80] = {
   0, 10, 20, 30, 40, 50, 59, 67, 76, 83,
  90, 97,102,107,112,115,118,120,122,123,
 124,123,122,120,118,115,112,107,102, 97,
  90, 83, 76, 67, 59, 50, 40, 30, 20, 10,
   0,-10,-20,-30,-40,-50,-59,-67,-76,-83,
 -90,-97,-102,-107,-112,-115,-118,-120,-122,-123,
-124,-123,-122,-120,-118,-115,-112,-107,-102,-97,
 -90,-83,-76,-67,-59,-50,-40,-30,-20,-10
};

const float tableY[80] = {
 -124,-123,-122,-120,-118,-115,-112,-107,-102,-97,
 -90,-83,-76,-67,-59,-50,-40,-30,-20,-10,
   0, 10, 20, 30, 40, 50, 59, 67, 76, 83,
  90, 97,102,107,112,115,118,120,122,123,
 124,123,122,120,118,115,112,107,102,97,
 90,83,76,67,59,50,40,30,20,10,
  0,-10,-20,-30,-40,-50,-59,-67,-76,-83,
 -90,-97,-102,-107,-112,-115,-118,-120,-122,-123
};

// BLE HID CONFIGURATION
BLEHIDDevice* hid;
BLECharacteristic* inputGamepad;
bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { 
    deviceConnected = true; 
    memset(lastBuffer, 0xFF, sizeof(lastBuffer));
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    pServer->getAdvertising()->start();
  }
};

const uint8_t reportMapGamepad[] = {
  0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
  0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02,
  0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x02,
  0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
  0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  0x05, 0x01, 0x09, 0x32, 0x09, 0x35, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  0x05, 0x02, 0x09, 0xC5, 0x09, 0xC4, 0x15, 0x00, 0x25, 0xFF, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  0xC0
};

// =====================================================
// BACA ANGLE AS5600 OVERSAMPLING 4X
// =====================================================
uint16_t readAS5600Raw() {
  long sum = 0;
  for (int i = 0; i < 4; i++) {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(0x0C);
    Wire.endTransmission(false);
    Wire.requestFrom(AS5600_ADDR, 2);

    if (Wire.available() >= 2) {
      uint16_t highByte = Wire.read();
      uint16_t lowByte  = Wire.read();
      sum += ((highByte << 8) | lowByte) & 0x0FFF;
    } else {
      sum += rawAnglePrevious;
    }
  }
  return (uint16_t)(sum / 4);
}

void updateAS5600MultiTurn() {
  uint16_t currentRaw = readAS5600Raw();
  int delta = (int)currentRaw - (int)rawAnglePrevious;

  if (delta < -2048) delta += 4096;
  else if (delta > 2048) delta -= 4096;

  totalRawPulse += delta;
  rawAnglePrevious = currentRaw;
}

// =====================================================
// INTERPOLASI DESIMAL (FLOAT LERP) UNTUK KEHALUSAN 100%
// =====================================================
void getSmoothTableXY(float stepVal, float &outX, float &outY) {
  float wrappedStep = fmod(stepVal, 80.0f);
  if (wrappedStep < 0) wrappedStep += 80.0f;

  int idx1 = (int)wrappedStep;
  int idx2 = (idx1 + 1) % 80;
  float fraction = wrappedStep - idx1;

  outX = tableX[idx1] + fraction * (tableX[idx2] - tableX[idx1]);
  outY = tableY[idx1] + fraction * (tableY[idx2] - tableY[idx1]);
}

void setup() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000); 

  pinMode(BTN_CAL_PIN, INPUT_PULLUP);

  pinMode(BTN_1_PIN, INPUT_PULLUP);
  pinMode(BTN_2_PIN, INPUT_PULLUP);
  pinMode(BTN_3_PIN, INPUT_PULLUP);
  pinMode(BTN_4_PIN, INPUT_PULLUP);
  pinMode(BTN_5_PIN, INPUT_PULLUP);
  pinMode(BTN_6_PIN, INPUT_PULLUP);

  pinMode(PEDAL_GAS_PIN, INPUT_PULLUP);
  pinMode(PEDAL_REM_PIN, INPUT_PULLUP);

  rawAnglePrevious = readAS5600Raw();

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
  updateAS5600MultiTurn();

  // TOMBOL RESET TITIK TENGAH (GPIO 0)
  if (digitalRead(BTN_CAL_PIN) == LOW) {
    centerOffset = totalRawPulse; 
  }

  if (deviceConnected) {
    uint8_t bufferLaporan[9] = {0};

    // 1. PEMROSESAN SETIR DENGAN SKALA 1.5X PAS (120 TITIK TABEL)
    long relativePulse = totalRawPulse - centerOffset;
    steerSmoothed = (alphaSteer * (float)relativePulse) + ((1.0f - alphaSteer) * steerSmoothed);

    // 6144 pulsa = 1.5 putaran magnet -> dikonversi ke 120 titik (1.5x putaran lingkaran joystick)
    float stepsFromCenter = (steerSmoothed / 6144.0f) * 120.0f;
    float totalStepFloat = 160.0f + stepsFromCenter;

    // Pembatas Rentang Mentok Kiri & Kanan (40 = Mentok Kiri 1.5x, 280 = Mentok Kanan 1.5x)
    totalStepFloat = constrain(totalStepFloat, 40.0f, 280.0f); 

    // Hitung koordinat X dan Y menggunakan interpolasi float LERP
    float targetX, targetY;
    getSmoothTableXY(totalStepFloat, targetX, targetY);

    outX_smoothed = (alphaOut * targetX) + ((1.0f - alphaOut) * outX_smoothed);
    outY_smoothed = (alphaOut * targetY) + ((1.0f - alphaOut) * outY_smoothed);

    // 2. TOMBOL DIGITAL DIRECT GND
    uint16_t btnState = 0;
    if (digitalRead(BTN_1_PIN) == LOW) btnState |= (1 << 0);
    if (digitalRead(BTN_2_PIN) == LOW) btnState |= (1 << 1);
    if (digitalRead(BTN_3_PIN) == LOW) btnState |= (1 << 2);
    if (digitalRead(BTN_4_PIN) == LOW) btnState |= (1 << 3);
    if (digitalRead(BTN_5_PIN) == LOW) btnState |= (1 << 4);
    if (digitalRead(BTN_6_PIN) == LOW) btnState |= (1 << 5);

    // 3. PEDAL GAS & REM
    uint8_t valGas = (digitalRead(PEDAL_GAS_PIN) == LOW) ? 255 : 0;
    uint8_t valRem = (digitalRead(PEDAL_REM_PIN) == LOW) ? 255 : 0;

    // BUFFER HID REPORT
    bufferLaporan[0] = btnState & 0xFF;         
    bufferLaporan[1] = (btnState >> 8) & 0xFF;  
    bufferLaporan[2] = 8; // D-Pad Netral                     

    bufferLaporan[3] = (int8_t)round(outX_smoothed);   
    bufferLaporan[4] = (int8_t)round(outY_smoothed);   

    bufferLaporan[5] = 0;
    bufferLaporan[6] = 0;
    bufferLaporan[7] = valRem; 
    bufferLaporan[8] = valGas; 

    // CONDITIONAL SENDING
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
  delay(5);
}
