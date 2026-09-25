#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// =====================================================
// PINOUT ESP32-C3
// =====================================================
#define CLK_PIN         7
#define DT_PIN          6
#define SW_PIN          5

// Pin Joystick Kanan & Switch
#define JOY_X_PIN       2     // Sumbu X Joystick Kanan (ADC1_CH2)
#define JOY_Y_PIN       3     // Sumbu Y Joystick Kanan (ADC1_CH3)
#define SW_JOY_PIN      4     // Tombol Tekan Joystick Kanan

// Pin R2 & L2 dipindah ke pin 20 & 21
#define R2_PIN          20
#define L2_PIN          21

#define ANALOG_PIN_1    0     // Ladder 1
#define ANALOG_PIN_2    1     // Ladder 2

// =====================================================
// DEVICE & MODE
// =====================================================
#define DEVICE_NAME         "SETIR BUS V2"
#define DEVICE_MANUFACTURER "ESP32"

bool gameModeBussid = true;

// Variabel offset titik tengah joystick (hasil autokalibrasi)
int centerX = 2048;
int centerY = 2048;

// =====================================================
// BUSSID & CARX
// =====================================================
const int TOTAL_STEPS = 80;
volatile int indexPoint = 0;
const int MAX_STEPS_CARX = 40;
volatile int totalEncoderSteps = 0;

portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

// =====================================================
// VARIABEL ENCODER INTERRUPT
// =====================================================
volatile uint8_t encoderOldState = 0;
volatile bool encoderChanged = false;

// =====================================================
// TABEL X & Y BUSSID
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
// BLE HID
// =====================================================
BLEHIDDevice* hid;
BLECharacteristic* inputGamepad;
bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE CONNECTED");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE DISCONNECTED");
    delay(100);
    pServer->getAdvertising()->start();
  }
};

const uint8_t reportMapGamepad[] = {
  0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
  0x09, 0x05,        // USAGE (Gamepad)
  0xA1, 0x01,        // COLLECTION (Application)

  // 8 Tombol (Ladder 1, Ladder 2, SW Encoder, SW Joystick)
  0x05, 0x09,        //   USAGE_PAGE (Button)
  0x19, 0x01,        //   USAGE_MINIMUM (Button 1)
  0x29, 0x08,        //   USAGE_MAXIMUM (Button 8)
  0x15, 0x00,        //   LOGICAL_MINIMUM (0)
  0x25, 0x01,        //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,        //   REPORT_SIZE (1)
  0x95, 0x08,        //   REPORT_COUNT (8)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)

  // Sumbu Setir: X, Y (-127 s/d 127)
  0x05, 0x01,        //   USAGE_PAGE (Generic Desktop)
  0x09, 0x30,        //   USAGE (X)
  0x09, 0x31,        //   USAGE (Y)
  0x15, 0x81,        //   LOGICAL_MINIMUM (-127)
  0x25, 0x7F,        //   LOGICAL_MAXIMUM (127)
  0x75, 0x08,        //   REPORT_SIZE (8)
  0x95, 0x02,        //   REPORT_COUNT (2)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)

  // Sumbu Joystick Kanan: Z, Rz (-127 s/d 127)
  0x05, 0x01,        //   USAGE_PAGE (Generic Desktop)
  0x09, 0x32,        //   USAGE (Z)
  0x09, 0x35,        //   USAGE (Rz)
  0x15, 0x81,        //   LOGICAL_MINIMUM (-127)
  0x25, 0x7F,        //   LOGICAL_MAXIMUM (127)
  0x75, 0x08,        //   REPORT_SIZE (8)
  0x95, 0x02,        //   REPORT_COUNT (2)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)

  // Analog Pedal: L2, R2 (0 s/d 255)
  0x05, 0x02,        //   USAGE_PAGE (Simulation Controls)
  0x09, 0xC5,        //   USAGE (Brake)
  0x09, 0xC4,        //   USAGE (Accelerator)
  0x15, 0x00,        //   LOGICAL_MINIMUM (0)
  0x25, 0xFF,        //   LOGICAL_MAXIMUM (255)
  0x75, 0x08,        //   REPORT_SIZE (8)
  0x95, 0x02,        //   REPORT_COUNT (2)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)

  0xC0               // END_COLLECTION
};

// =====================================================
// ISR ENCODER
// =====================================================
void IRAM_ATTR readEncoderISR() {
  uint8_t s1 = digitalRead(CLK_PIN);
  uint8_t s2 = digitalRead(DT_PIN);
  uint8_t currentState = (s1 << 1) | s2;

  if (currentState != encoderOldState) {
    portENTER_CRITICAL_ISR(&encoderMux);

    if ((encoderOldState == 0 && currentState == 2) ||
        (encoderOldState == 2 && currentState == 3) ||
        (encoderOldState == 3 && currentState == 1) ||
        (encoderOldState == 1 && currentState == 0)) {

      if (gameModeBussid) {
        indexPoint = (indexPoint + 1) % TOTAL_STEPS;
      } else {
        if (totalEncoderSteps < 150) totalEncoderSteps++;
      }

      encoderChanged = true;
    }

    else if ((encoderOldState == 0 && currentState == 1) ||
             (encoderOldState == 1 && currentState == 3) ||
             (encoderOldState == 3 && currentState == 2) ||
             (encoderOldState == 2 && currentState == 0)) {

      if (gameModeBussid) {
        indexPoint = (indexPoint - 1 + TOTAL_STEPS) % TOTAL_STEPS;
      } else {
        if (totalEncoderSteps > -150) totalEncoderSteps--;
      }

      encoderChanged = true;
    }

    portEXIT_CRITICAL_ISR(&encoderMux);
    encoderOldState = currentState;
  }
}

// =====================================================
// BACA ADC DENGAN RATA-RATA DENGAN PENGAMBILAN SAMPLE
// =====================================================
int readADCFiltered(uint8_t pin) {
  long sum = 0;

  for (int i = 0; i < 10; i++) {
    sum += analogRead(pin);
  }

  return sum / 10;
}

// =====================================================
// AUTOKALIBRASI PADA SAAT STARTUP
// =====================================================
void kalibrasiJoystick() {
  long sumX = 0;
  long sumY = 0;

  // Mengambil 50 sampel saat joystick posisi diam
  for (int i = 0; i < 50; i++) {
    sumX += analogRead(JOY_X_PIN);
    sumY += analogRead(JOY_Y_PIN);
    delay(5);
  }

  centerX = sumX / 50;
  centerY = sumY / 50;
}

uint8_t bacaTombolLadder1() {
  int adc = readADCFiltered(ANALOG_PIN_1);

  if (adc >= 1300 && adc <= 2200) return 1;
  if (adc >= 2800 && adc <= 3700) return 2;
  if (adc >= 3700 && adc <= 4000) return 3;

  return 0;
}

uint8_t bacaTombolLadder2() {
  int adc = readADCFiltered(ANALOG_PIN_2);

  if (adc >= 1300 && adc <= 2200) return 4;
  if (adc >= 2700 && adc <= 3700) return 5;
  if (adc >= 3700 && adc <= 4000) return 6;

  return 0;
}

// =====================================================
// BACA JOYSTICK DENGAN OFFSET CENTER & DEADZONE
// =====================================================
int8_t readJoystickAxis(uint8_t pin, int centerVal) {
  int raw = readADCFiltered(pin);
  int mapped;

  // Pemetaan bertahap berdasarkan posisi tengah aktual
  if (raw < centerVal) {
    mapped = map(raw, 0, centerVal, -127, 0);
  } else {
    mapped = map(raw, centerVal, 4095, 0, 127);
  }

  // Deadzone toleransi area tengah (dikunci di 0)
  if (abs(mapped) < 15) return 0;

  return (int8_t)constrain(mapped, -127, 127);
}

// =====================================================
// KOREKSI ROTASI JOYSTICK 90° SEARAH JARUM JAM
// =====================================================
void readJoystickRotated(int8_t &joyX, int8_t &joyY) {

  int8_t rawX = readJoystickAxis(JOY_X_PIN, centerX);
  int8_t rawY = readJoystickAxis(JOY_Y_PIN, centerY);

  // Koreksi orientasi joystick:
  //
  // Joystick diputar 90° searah jarum jam.
  //
  // X baru = -Y lama
  // Y baru =  X lama
  //
  joyX = -rawY;
  joyY = rawX;
}

// =====================================================
// LADDER DEBOUNCE
// =====================================================
uint8_t stableL1 = 0, lastL1 = 0;
uint8_t stableL2 = 0, lastL2 = 0;

unsigned long timeL1 = 0;
unsigned long timeL2 = 0;

void updateLadderDebounce() {

  uint8_t raw1 = bacaTombolLadder1();

  if (raw1 != lastL1) {
    lastL1 = raw1;
    timeL1 = millis();
  }

  if ((millis() - timeL1) >= 10) {
    stableL1 = lastL1;
  }

  uint8_t raw2 = bacaTombolLadder2();

  if (raw2 != lastL2) {
    lastL2 = raw2;
    timeL2 = millis();
  }

  if ((millis() - timeL2) >= 10) {
    stableL2 = lastL2;
  }
}

// =====================================================
// KIRIM PAKET HID
// =====================================================
void kirimPaketHID() {

  if (!deviceConnected) return;

  uint8_t bufferLaporan[7] = {
    0, 0, 0, 0, 0, 0, 0
  };

  bool swState    = (digitalRead(SW_PIN) == LOW);
  bool swJoyState = (digitalRead(SW_JOY_PIN) == LOW);
  bool l2State    = (digitalRead(L2_PIN) == LOW);
  bool r2State    = (digitalRead(R2_PIN) == LOW);

  // ===================================================
  // BITMASK TOMBOL
  // ===================================================
  if (stableL1 == 1) bufferLaporan[0] |= (1 << 0);
  if (stableL1 == 2) bufferLaporan[0] |= (1 << 1);
  if (stableL1 == 3) bufferLaporan[0] |= (1 << 6);

  if (stableL2 == 4) bufferLaporan[0] |= (1 << 3);
  if (stableL2 == 5) bufferLaporan[0] |= (1 << 4);
  if (stableL2 == 6) bufferLaporan[0] |= (1 << 7);

  if (swState)       bufferLaporan[0] |= (1 << 2);
  if (swJoyState)    bufferLaporan[0] |= (1 << 5);

  // ===================================================
  // SUMBU SETIR ENCODER (X & Y)
  // ===================================================
  int localIndex = 0;
  int localTotalSteps = 0;

  portENTER_CRITICAL(&encoderMux);

  localIndex = indexPoint;
  localTotalSteps = totalEncoderSteps;

  portEXIT_CRITICAL(&encoderMux);

  if (gameModeBussid) {

    bufferLaporan[1] = tableX[localIndex];
    bufferLaporan[2] = tableY[localIndex];

  } else {

    int effectiveSteps = localTotalSteps;

    if (effectiveSteps > MAX_STEPS_CARX)
      effectiveSteps = MAX_STEPS_CARX;

    if (effectiveSteps < -MAX_STEPS_CARX)
      effectiveSteps = -MAX_STEPS_CARX;

    bufferLaporan[1] =
      (effectiveSteps * 127) / MAX_STEPS_CARX;

    bufferLaporan[2] = 0;
  }

  // ===================================================
  // SUMBU JOYSTICK KANAN
  // KOREKSI ROTASI 90° SEARAH JARUM JAM
  // ===================================================
  int8_t joyX;
  int8_t joyY;

  readJoystickRotated(joyX, joyY);

  bufferLaporan[3] = joyX;
  bufferLaporan[4] = joyY;

  // ===================================================
  // PEDAL L2 / R2
  // ===================================================
  bufferLaporan[5] = l2State ? 255 : 0;
  bufferLaporan[6] = r2State ? 255 : 0;

  // ===================================================
  // KIRIM HID
  // ===================================================
  inputGamepad->setValue(
    bufferLaporan,
    sizeof(bufferLaporan)
  );

  inputGamepad->notify();
}

// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);

  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(SW_JOY_PIN, INPUT_PULLUP);

  pinMode(R2_PIN, INPUT_PULLUP);
  pinMode(L2_PIN, INPUT_PULLUP);

  pinMode(JOY_X_PIN, INPUT);
  pinMode(JOY_Y_PIN, INPUT);

  pinMode(ANALOG_PIN_1, INPUT);
  pinMode(ANALOG_PIN_2, INPUT);

  analogReadResolution(12);

  // ===================================================
  // KALIBRASI TITIK TENGAH JOYSTICK
  // Pastikan joystick tidak disentuh saat startup
  // ===================================================
  kalibrasiJoystick();

  // ===================================================
  // ENCODER
  // ===================================================
  encoderOldState =
    (digitalRead(CLK_PIN) << 1) |
     digitalRead(DT_PIN);

  attachInterrupt(
    digitalPinToInterrupt(CLK_PIN),
    readEncoderISR,
    CHANGE
  );

  attachInterrupt(
    digitalPinToInterrupt(DT_PIN),
    readEncoderISR,
    CHANGE
  );

  // ===================================================
  // BLE
  // ===================================================
  BLEDevice::init(DEVICE_NAME);

  BLESecurity* pSecurity = new BLESecurity();

  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSecurity->setCapability(ESP_IO_CAP_NONE);

  BLEServer* pServer = BLEDevice::createServer();

  pServer->setCallbacks(
    new MyServerCallbacks()
  );

  hid = new BLEHIDDevice(pServer);

  inputGamepad = hid->inputReport(0);

  hid->pnp(
    0x02,
    0x05ac,
    0x820a,
    0x0210
  );

  hid->hidInfo(0x00, 0x01);

  hid->reportMap(
    (uint8_t*)reportMapGamepad,
    sizeof(reportMapGamepad)
  );

  hid->startServices();

  BLEAdvertising* pAdvertising =
    BLEDevice::getAdvertising();

  pAdvertising->setAppearance(0x03C4);

  pAdvertising->addServiceUUID(
    hid->hidService()->getUUID()
  );

  pAdvertising->start();
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  static unsigned long lastUpdate = 0;

  bool stateChanged = false;

  // ===================================================
  // 1. CEK ENCODER
  // ===================================================
  if (encoderChanged) {

    portENTER_CRITICAL(&encoderMux);

    encoderChanged = false;

    portEXIT_CRITICAL(&encoderMux);

    stateChanged = true;
  }

  // ===================================================
  // 2. CEK TOMBOL LADDER
  // ===================================================
  static uint8_t lastSentL1 = 0;
  static uint8_t lastSentL2 = 0;

  updateLadderDebounce();

  if (stableL1 != lastSentL1 ||
      stableL2 != lastSentL2) {

    lastSentL1 = stableL1;
    lastSentL2 = stableL2;

    stateChanged = true;
  }

  // ===================================================
  // 3. CEK TOMBOL R2 / L2 / SW JOYSTICK
  // ===================================================
  static bool lastR2 = HIGH;
  static bool lastL2 = HIGH;
  static bool lastSWJoy = HIGH;

  bool currentR2 =
    digitalRead(R2_PIN);

  bool currentL2 =
    digitalRead(L2_PIN);

  bool currentSWJoy =
    digitalRead(SW_JOY_PIN);

  if (currentR2 != lastR2 ||
      currentL2 != lastL2 ||
      currentSWJoy != lastSWJoy) {

    lastR2 = currentR2;
    lastL2 = currentL2;
    lastSWJoy = currentSWJoy;

    stateChanged = true;
  }

  // ===================================================
  // 4. CEK PERUBAHAN SUMBU JOYSTICK KANAN
  // SUDAH MENGGUNAKAN KOREKSI ROTASI
  // ===================================================
  static int8_t lastJoyX = 0;
  static int8_t lastJoyY = 0;

  int8_t currentJoyX;
  int8_t currentJoyY;

  readJoystickRotated(
    currentJoyX,
    currentJoyY
  );

  if (abs(currentJoyX - lastJoyX) > 2 ||
      abs(currentJoyY - lastJoyY) > 2) {

    lastJoyX = currentJoyX;
    lastJoyY = currentJoyY;

    stateChanged = true;
  }

  // ===================================================
  // 5. CEK SWITCH MODE (SW ENCODER)
  // ===================================================
  static unsigned long pressTime = 0;
  static bool buttonWasPressed = false;

  int currentSW = digitalRead(SW_PIN);

  if (currentSW == LOW &&
      !buttonWasPressed) {

    buttonWasPressed = true;
    pressTime = millis();

  } else if (currentSW == HIGH &&
             buttonWasPressed) {

    unsigned long duration =
      millis() - pressTime;

    buttonWasPressed = false;

    portENTER_CRITICAL(&encoderMux);

    if (duration > 50 &&
        duration < 1000) {

      indexPoint = 0;
      totalEncoderSteps = 0;

    } else if (duration >= 1000) {

      gameModeBussid = !gameModeBussid;

      indexPoint = 0;
      totalEncoderSteps = 0;
    }

    portEXIT_CRITICAL(&encoderMux);

    stateChanged = true;
  }

  // ===================================================
  // KIRIM DATA
  // ===================================================
  // Kirim langsung saat ada perubahan
  // atau berkala setiap 50ms
  // ===================================================
  if (stateChanged ||
      (millis() - lastUpdate > 50)) {

    kirimPaketHID();

    lastUpdate = millis();
  }
}