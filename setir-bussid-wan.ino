#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <Wire.h>
#include <math.h>

// =====================================================
// PINOUT ESP32-C3 SUPER MINI
// =====================================================

#define SDA_PIN  3
#define SCL_PIN  4

#define GAS_PIN  0
#define REM_PIN  1
#define RESET_PIN 2

#define LED_PIN  8

// =====================================================
// AS5600
// =====================================================

#define AS5600_ADDR   0x36
#define RAW_ANGLE_REG 0x0C

// =====================================================
// BLUETOOTH
// =====================================================

#define DEVICE_NAME         "HERMAWAN SETIR"
#define DEVICE_MANUFACTURER "ESP32"

// =====================================================
// MODE PERMAINAN
// =====================================================

// true  = BUSSID
// false = CARX STREET

bool gameModeBussid = true;

// =====================================================
// PENGATURAN BUSSID
// =====================================================

int BUSSID_FIRST_STEP = 0;

// =====================================================
// PENGATURAN CARX
// =====================================================

int CARX_FIRST_STEP = 20;

int CARX_CENTER_ZONE = 2;

float CARX_ANGLE_DIVIDER = 1.0f;

const long CARX_MAX_RAW_RANGE = 2048;

// =====================================================
// STEERING VARIABLE
// =====================================================

const int TOTAL_STEPS = 80;

volatile int indexPoint = 0;

volatile long carxRawPosition = 0;

uint16_t lastRawAngle = 0;

uint16_t zeroAngle = 0;

bool isFirstRead = true;

int16_t steeringX = 0;
int16_t steeringY = 0;

// =====================================================
// LED STATUS
// =====================================================

bool isBlinkingMode = false;

int blinkCount = 0;

unsigned long lastBlinkTime = 0;

bool ledState = LOW;

// =====================================================
// TABEL BUSSID ASLI
// =====================================================

const int8_t tableX[80] = {
     0,  10,  20,  30,  40,  50,  59,  67,  76,  83,
    90,  97, 102, 107, 112, 115, 118, 120, 122, 123,
   124, 123, 122, 120, 118, 115, 112, 107, 102,  97,
    90,  83,  76,  67,  59,  50,  40,  30,  20,  10,
     0, -10, -20, -30, -40, -50, -59, -67, -76, -83,
   -90, -97,-102,-107,-112,-115,-118,-120,-122,-123,
  -124,-123,-122,-120,-118,-115,-112,-107,-102, -97,
   -90, -83, -76, -67, -59, -50, -40, -30, -20, -10
};

const int8_t tableY[80] = {
  -124,-123,-122,-120,-118,-115,-112,-107,-102, -97,
   -90, -83, -76, -67, -59, -50, -40, -30, -20, -10,
     0,  10,  20,  30,  40,  50,  59,  67,  76,  83,
    90,  97, 102, 107, 112, 115, 118, 120, 122, 123,
   124, 123, 122, 120, 118, 115, 112, 107, 102,  97,
    90,  83,  76,  67,  59,  50,  40,  30,  20,  10,
     0, -10, -20, -30, -40, -50, -59, -67, -76, -83,
   -90, -97,-102,-107,-112,-115,-118,-120,-122,-123
};

// =====================================================
// BLE HID
// =====================================================

BLEHIDDevice* hid;

BLECharacteristic* inputGamepad;

bool deviceConnected = false;

// =====================================================
// TOMBOL GAS DAN REM
// =====================================================

bool lastGasState = HIGH;
bool lastRemState = HIGH;
bool lastResetState = HIGH;

// =====================================================
// BLE CALLBACK
// =====================================================

class MyServerCallbacks : public BLEServerCallbacks {

  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("BLE CONNECTED");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("BLE DISCONNECTED");

    delay(100);
    pServer->getAdvertising()->start();
  }
};

// =====================================================
// HID REPORT DESCRIPTOR
// 8 BUTTONS + HAT + STEERING X/Y + 2 PEDALS
// =====================================================

const uint8_t reportMapGamepad[] = {

  0x05, 0x01,
  0x09, 0x05,
  0xA1, 0x01,

  // 8 tombol
  0x05, 0x09,
  0x19, 0x01,
  0x29, 0x08,
  0x15, 0x00,
  0x25, 0x01,
  0x75, 0x01,
  0x95, 0x08,
  0x81, 0x02,

  // Hat switch
  0x05, 0x01,
  0x09, 0x39,
  0x15, 0x00,
  0x25, 0x07,
  0x35, 0x00,
  0x46, 0x3B, 0x01,
  0x65, 0x14,
  0x75, 0x04,
  0x95, 0x01,
  0x81, 0x02,

  // Padding
  0x75, 0x04,
  0x95, 0x01,
  0x81, 0x03,

  // Steering X dan Y, 16-bit
  0x05, 0x01,
  0x09, 0x30,
  0x09, 0x31,
  0x16, 0x00, 0x80,
  0x26, 0xFF, 0x7F,
  0x75, 0x10,
  0x95, 0x02,
  0x81, 0x02,

  // Pedal 1 dan pedal 2
  0x05, 0x02,
  0x09, 0xC5,
  0x09, 0xC4,
  0x15, 0x00,
  0x25, 0xFF,
  0x75, 0x08,
  0x95, 0x02,
  0x81, 0x02,

  0xC0
};

// =====================================================
// LED MODE
// =====================================================

void triggerModeBlink() {

  isBlinkingMode = true;
  blinkCount = 0;
  lastBlinkTime = millis();
  ledState = HIGH;

  digitalWrite(LED_PIN, ledState);
}

// =====================================================
// BACA AS5600
// =====================================================

uint16_t readAS5600RawAngle() {

  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(RAW_ANGLE_REG);

  if (Wire.endTransmission(false) != 0) {
    return lastRawAngle;
  }

  Wire.requestFrom(
    (uint8_t)AS5600_ADDR,
    (uint8_t)2
  );

  if (Wire.available() >= 2) {

    uint16_t highByte = Wire.read();
    uint16_t lowByte = Wire.read();

    return ((highByte << 8) | lowByte) & 0x0FFF;
  }

  return lastRawAngle;
}

// =====================================================
// RESET TITIK TENGAH SETIR
// =====================================================

void resetSteeringPosition() {

  zeroAngle = readAS5600RawAngle();
  lastRawAngle = zeroAngle;

  indexPoint = 0;
  carxRawPosition = 0;

  steeringX = 0;
  steeringY = 0;

  isFirstRead = false;
}

// =====================================================
// UPDATE AS5600
// =====================================================

bool updateAS5600Position() {

  uint16_t currentRawAngle = readAS5600RawAngle();

  if (isFirstRead) {

    lastRawAngle = currentRawAngle;
    zeroAngle = currentRawAngle;

    isFirstRead = false;

    steeringX = 0;
    steeringY = 0;

    return false;
  }

  int diff = (int)currentRawAngle - (int)lastRawAngle;

  // Menangani sensor melewati 4095 ke 0 atau sebaliknya
  if (diff > 2048) {
    diff -= 4096;
  }
  else if (diff < -2048) {
    diff += 4096;
  }

  if (diff == 0) {
    return false;
  }

  lastRawAngle = currentRawAngle;

  // ===================================================
  // MODE BUSSID
  // ===================================================

  if (gameModeBussid) {

    uint16_t correctedAngle =
      (currentRawAngle - zeroAngle + 4096) % 4096;

    float position =
      ((float)correctedAngle * TOTAL_STEPS) / 4096.0f;

    int index1 = (int)position;
    int index2 = index1 + 1;

    if (index1 >= TOTAL_STEPS) index1 = 0;
    if (index2 >= TOTAL_STEPS) index2 = 0;

    float fraction = position - floor(position);

    float x = tableX[index1] +
      (tableX[index2] - tableX[index1]) * fraction;

    float y = tableY[index1] +
      (tableY[index2] - tableY[index1]) * fraction;

    if (BUSSID_FIRST_STEP != 0) {

      if (x > 0) x += BUSSID_FIRST_STEP;
      else if (x < 0) x -= BUSSID_FIRST_STEP;

      if (y > 0) y += BUSSID_FIRST_STEP;
      else if (y < 0) y -= BUSSID_FIRST_STEP;
    }

    steeringX = (int16_t)constrain(
      round(x * 32767.0f / 124.0f),
      -32767,
      32767
    );

    steeringY = (int16_t)constrain(
      round(y * 32767.0f / 124.0f),
      -32767,
      32767
    );

    indexPoint = (int)position;

    return true;
  }

  // ===================================================
  // MODE CARX STREET
  // ===================================================

  else {

    carxRawPosition += diff;

    carxRawPosition = constrain(
      carxRawPosition,
      -CARX_MAX_RAW_RANGE,
      CARX_MAX_RAW_RANGE
    );

    float normalized =
      (float)carxRawPosition / (float)CARX_MAX_RAW_RANGE;

    normalized /= CARX_ANGLE_DIVIDER;

    normalized = constrain(normalized, -1.0f, 1.0f);

    float centerZone = (float)CARX_CENTER_ZONE / 100.0f;

    if (fabs(normalized) <= centerZone) {
      normalized = 0.0f;
    }
    else if (CARX_FIRST_STEP > 0) {

      float firstStep = (float)CARX_FIRST_STEP / 100.0f;

      if (normalized > 0.0f) normalized += firstStep;
      else if (normalized < 0.0f) normalized -= firstStep;

      normalized = constrain(normalized, -1.0f, 1.0f);
    }

    steeringX = (int16_t)round(normalized * 32767.0f);
    steeringY = 0;

    return true;
  }
}

// =====================================================
// LED STATUS
// =====================================================

void updateLEDStatus() {

  if (isBlinkingMode) {

    if (millis() - lastBlinkTime >= 100) {

      lastBlinkTime = millis();
      ledState = !ledState;

      digitalWrite(LED_PIN, ledState);

      blinkCount++;

      if (blinkCount >= 6) {
        isBlinkingMode = false;
      }
    }

    return;
  }

  if (deviceConnected) {

    digitalWrite(LED_PIN, HIGH);
  }
  else if (millis() - lastBlinkTime >= 500) {

    lastBlinkTime = millis();
    ledState = !ledState;

    digitalWrite(LED_PIN, ledState);
  }
}

// =====================================================
// KIRIM DATA HID
// =====================================================

void kirimPaketHID() {

  if (!deviceConnected) return;

  uint8_t bufferLaporan[10] = {0};

  bool gasState = digitalRead(GAS_PIN) == LOW;
  bool remState = digitalRead(REM_PIN) == LOW;

  // Tombol gas dan rem
  // Pedal 1 = gas, pedal 2 = rem
  bufferLaporan[8] = gasState ? 255 : 0;
  bufferLaporan[9] = remState ? 255 : 0;

  // Steering X dan Y
  bufferLaporan[2] = (uint8_t)(steeringX & 0xFF);
  bufferLaporan[3] = (uint8_t)((steeringX >> 8) & 0xFF);

  bufferLaporan[4] = (uint8_t)(steeringY & 0xFF);
  bufferLaporan[5] = (uint8_t)((steeringY >> 8) & 0xFF);

  inputGamepad->setValue(bufferLaporan, sizeof(bufferLaporan));
  inputGamepad->notify();
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN, 400000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(GAS_PIN, INPUT_PULLUP);
  pinMode(REM_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);

  analogReadResolution(12);

  // BLE
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

  hid->reportMap(
    (uint8_t*)reportMapGamepad,
    sizeof(reportMapGamepad)
  );

  hid->startServices();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();

  pAdvertising->setAppearance(0x03C4);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();

  Serial.println();
  Serial.println("================================");
  Serial.println("       HERMAWAN SETIR");
  Serial.println("================================");
  Serial.println("AS5600 : SDA GPIO 3, SCL GPIO 4");
  Serial.println("GAS    : GPIO 0");
  Serial.println("REM    : GPIO 1");
  Serial.println("RESET  : GPIO 2");
  Serial.println("MODE   : BUSSID");
  Serial.println("================================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  static unsigned long lastUpdate = 0;

  updateLEDStatus();

  // Baca sensor setir
  bool stateChanged = updateAS5600Position();

  // Tombol reset titik tengah
  bool currentResetState = digitalRead(RESET_PIN);

  if (lastResetState == HIGH && currentResetState == LOW) {

    resetSteeringPosition();

    Serial.println("TITIK TENGAH SETIR DIATUR ULANG");

    stateChanged = true;
  }

  lastResetState = currentResetState;

  // Kirim laporan gamepad setiap 5 ms
  if (stateChanged || millis() - lastUpdate >= 5) {

    kirimPaketHID();

    lastUpdate = millis();
  }
}
