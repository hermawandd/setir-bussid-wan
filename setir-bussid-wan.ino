#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <Wire.h>

// =====================================================
// PINOUT ESP32-C3 SUPERMINI (AMAN GPIO 8 & 9)
// =====================================================
#define BTN_CAL_PIN     0     // Reset Titik Tengah (Direct GND)

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
// LOGIKA SETIR MENTAHAN: CONTINUOUS TRACKING & ADAPTIVE FILTER
// =====================================================
#define AS5600_ADDR     0x36

uint16_t as5600LastRaw = 0;
int32_t as5600ContinuousRaw = 0;
int32_t as5600CenterContinuousRaw = 0;
bool as5600TrackingInitialized = false;

// Filter Median 3 Sampel + Adaptive Alpha (Mentok Mentahan)
int32_t bussidSample[3] = {0, 0, 0};
uint8_t bussidSampleIndex = 0;
float bussidSmoothDeltaRaw = 0.0f;
bool bussidSmoothReady = false;

const float BUSSID_FAST_INPUT_THRESHOLD = 24.0f;
const float BUSSID_SMOOTH_ALPHA_FAST     = 0.92f; // Refleks cepat pas dibanting
const float BUSSID_SMOOTH_ALPHA_MEDIUM   = 0.75f; // Pergerakan sedang
const float BUSSID_SMOOTH_ALPHA_SLOW     = 0.55f; // Sangat halus pas pelan

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

// BACA ANGLE RAW AS5600
uint16_t readAS5600Raw() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(0x0C);
  if (Wire.endTransmission(false) != 0) return as5600LastRaw;
  if (Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) != 2) return as5600LastRaw;
  
  uint16_t highByte = Wire.read();
  uint16_t lowByte  = Wire.read();
  return ((highByte << 8) | lowByte) & 0x0FFF;
}

// LOGIKA CONTINUOUS TRACKING DARI MENTAHAN
void updateAS5600Continuous() {
  uint16_t raw = readAS5600Raw();

  if (!as5600TrackingInitialized) {
    as5600LastRaw = raw;
    as5600ContinuousRaw = raw;
    as5600CenterContinuousRaw = raw;
    as5600TrackingInitialized = true;
  } else {
    int32_t sampleDelta = (int32_t)raw - (int32_t)as5600LastRaw;
    if (sampleDelta > 2047) sampleDelta -= 4096;
    if (sampleDelta < -2048) sampleDelta += 4096;

    if (abs(sampleDelta) <= 1024) {
      as5600ContinuousRaw += sampleDelta;
      as5600LastRaw = raw;
    } else {
      as5600LastRaw = raw;
    }
  }
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

  as5600LastRaw = readAS5600Raw();

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
  updateAS5600Continuous();

  // RESET TITIK TENGAH (RESET TO 0)
  if (digitalRead(BTN_CAL_PIN) == LOW) {
    as5600CenterContinuousRaw = as5600ContinuousRaw;
    bussidSmoothReady = false;
  }

  if (deviceConnected) {
    uint8_t bufferLaporan[9] = {0};

    // 1. FILTER MEDIAN 3 SAMPEL DARI MENTAHAN
    int32_t target = as5600ContinuousRaw - as5600CenterContinuousRaw;

    bussidSample[bussidSampleIndex] = target;
    bussidSampleIndex = (bussidSampleIndex + 1) % 3;

    int32_t a = bussidSample[0];
    int32_t b = bussidSample[1];
    int32_t c = bussidSample[2];
    int32_t medianTarget = a + b + c - max(a, max(b, c)) - min(a, min(b, c));

    // 2. FILTER ALPHA ADAPTIF DARI MENTAHAN
    if (!bussidSmoothReady) {
      bussidSmoothDeltaRaw = (float)medianTarget;
      bussidSmoothReady = true;
    } else {
      float diff = (float)medianTarget - bussidSmoothDeltaRaw;
      float absDiff = fabsf(diff);
      float alpha;

      if (absDiff > BUSSID_FAST_INPUT_THRESHOLD) alpha = BUSSID_SMOOTH_ALPHA_FAST;
      else if (absDiff > 8.0f) alpha = BUSSID_SMOOTH_ALPHA_MEDIUM;
      else alpha = BUSSID_SMOOTH_ALPHA_SLOW;

      bussidSmoothDeltaRaw += diff * alpha;
      if (absDiff <= 0.35f) bussidSmoothDeltaRaw = (float)medianTarget;
    }

    // 3. SKALA 1.5X PUTARAN (6144 PULSA) DENGAN PEMBATAS CONSTRAIN
    float stepsFromCenter = (bussidSmoothDeltaRaw / 6144.0f) * 120.0f;
    float totalStepFloat = 160.0f + stepsFromCenter;
    totalStepFloat = constrain(totalStepFloat, 40.0f, 280.0f); // Mentok 1.5x putaran

    // 4. INTERPOLASI PECAHAN LINGKARAN DARI MENTAHAN
    int base = (int)totalStepFloat;
    int i0 = base % 80;
    if (i0 < 0) i0 += 80;
    int i1 = (i0 + 1) % 80;
    float frac = totalStepFloat - (float)base;

    int x = (int)((float)tableX[i0] + ((float)tableX[i1] - (float)tableX[i0]) * frac + 0.5f);
    int y = (int)((float)tableY[i0] + ((float)tableY[i1] - (float)tableY[i0]) * frac + 0.5f);

    x = constrain(x, -124, 124);
    y = constrain(y, -124, 124);

    // BACA TOMBOL DIGITAL DIRECT GND
    uint16_t btnState = 0;
    if (digitalRead(BTN_1_PIN) == LOW) btnState |= (1 << 0);
    if (digitalRead(BTN_2_PIN) == LOW) btnState |= (1 << 1);
    if (digitalRead(BTN_3_PIN) == LOW) btnState |= (1 << 2);
    if (digitalRead(BTN_4_PIN) == LOW) btnState |= (1 << 3);
    if (digitalRead(BTN_5_PIN) == LOW) btnState |= (1 << 4);
    if (digitalRead(BTN_6_PIN) == LOW) btnState |= (1 << 5);

    // PEDAL GAS & REM
    uint8_t valGas = (digitalRead(PEDAL_GAS_PIN) == LOW) ? 255 : 0;
    uint8_t valRem = (digitalRead(PEDAL_REM_PIN) == LOW) ? 255 : 0;

    // SUSUN REPORT HID GAMEPAD
    bufferLaporan[0] = btnState & 0xFF;         
    bufferLaporan[1] = (btnState >> 8) & 0xFF;  
    bufferLaporan[2] = 8; // D-Pad Netral                     

    bufferLaporan[3] = (int8_t)x;   
    bufferLaporan[4] = (int8_t)y;   

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
