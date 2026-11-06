#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <math.h>

// ================ I2C TERPISAH ================
TwoWire I2C_MAX = TwoWire(0);
TwoWire I2C_MPU = TwoWire(1);

MAX30105 max30102;
Adafruit_MPU6050 mpu;

// MAX30102: SDA 21, SCL 22
const int MAX_SDA = 21;
const int MAX_SCL = 22;

// MPU6050: SDA 17, SCL 4
const int MPU_SDA = 17;
const int MPU_SCL = 4;

// ================ OUTPUT PINS ================
const int VIB_PIN = 25;
const int BUZZER_PIN = 27;

// Buzzer aktif-LOW
#define BUZ_ON  LOW
#define BUZ_OFF HIGH

// Vibrator aktif-LOW
#define VIB_ON  HIGH
#define VIB_OFF LOW

// Buzzer aktif, bukan passive buzzer
const bool PASSIVE_BUZZER = false;

// ================ KONFIGURASI DETEKSI ================
const float HEAD_DOWN_THRESHOLD = 25.0;
const float HEAD_RETURN_THRESHOLD = 6.0;
const float LOW_HR_THRESHOLD = 90.0;

const int REQUIRED_NODS = 2;

const unsigned long WINDOW_MS = 7000;
const unsigned long PRINT_INTERVAL = 200;
const unsigned long MIN_NOD_INTERVAL = 300;

const long FINGER_THRESHOLD = 50000;

// ================ KONFIGURASI ALARM ================
const unsigned long STAGE2_MS = 45000;

// ================ VARIABEL DETEKSI ================
unsigned long lastPrint = 0;
unsigned long lastBeat = 0;
unsigned long lastNodTime = 0;

float referencePitch = 0;
float bpm = 0;
float bpmSmooth = 0;

bool calibrated = false;
bool headDown = false;
bool fingerDetected = false;
bool validBPM = false;

unsigned long nodTimes[20];
int nodCount = 0;

// ================ PYTHON EYE ALERT ================
bool eyeAlert = false;
String eyeTimestamp = "";

unsigned long lastEyeRxMs = 0;

// ================ VARIABEL ALARM ================
bool ringing = false;
bool buzzerState = false;

unsigned long alarmStart = 0;
unsigned long lastBuzzerToggle = 0;

int sirenDelay = 80;
int sirenDirection = -1;

// ================ MATIKAN SEMUA OUTPUT ================
void allOff() {
  if (PASSIVE_BUZZER) {
    noTone(BUZZER_PIN);
  }

  digitalWrite(BUZZER_PIN, BUZ_OFF);
  digitalWrite(VIB_PIN, VIB_OFF);

  buzzerState = false;
}

// ================ MULAI ALARM ================
void startAlarm() {
  ringing = true;
  alarmStart = millis();
  lastBuzzerToggle = millis();

  sirenDelay = 80;
  sirenDirection = -1;
  buzzerState = false;

  digitalWrite(VIB_PIN, VIB_ON);

  Serial.println("INDIKASI MENGANTUK: ALARM AKTIF");
}

// ================ HENTIKAN ALARM ================
void stopAlarm() {
  ringing = false;
  allOff();
}

// ================ UPDATE ALARM ================
// Alarm berjalan non-blocking agar sensor dan data Python tetap terbaca.
void updateAlarm(bool sleepy, unsigned long now) {
  if (!sleepy) {
    if (ringing) {
      Serial.println("Status NORMAL: ALARM DIMATIKAN");
    }

    stopAlarm();
    return;
  }

  if (!ringing) {
    startAlarm();
  }

  // Output ini dipertahankan aktif selama status mengantuk.
  digitalWrite(VIB_PIN, VIB_ON);

  bool fastYelp = (now - alarmStart >= STAGE2_MS);

  if (now - lastBuzzerToggle < (unsigned long)sirenDelay) {
    return;
  }

  lastBuzzerToggle = now;
  buzzerState = !buzzerState;

  if (PASSIVE_BUZZER) {
    if (buzzerState) {
      // Untuk buzzer pasif: variasi nada sederhana.
      tone(BUZZER_PIN, fastYelp ? 1500 : 1000);
    } else {
      noTone(BUZZER_PIN);
    }
  } else {
    // Active buzzer mengikuti pola ON/OFF dari kode asli.
    digitalWrite(BUZZER_PIN, buzzerState ? BUZ_ON : BUZ_OFF);
  }

  if (fastYelp) {
    // Tahap 2: yelp cepat, meniru pola yelpActive().
    sirenDelay -= 4;
    if (sirenDelay < 6) {
      sirenDelay = 30;
    }
  } else {
    // Tahap 1: wail, semakin cepat lalu melambat.
    sirenDelay += sirenDirection * 5;

    if (sirenDelay <= 10) {
      sirenDelay = 10;
      sirenDirection = 1;
    } else if (sirenDelay >= 80) {
      sirenDelay = 80;
      sirenDirection = -1;
    }
  }
}

// ================ RECEIVE PYTHON DATA ================
void readEyeSerial(float currentPitch) {
  static String line = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n') {
      line.trim();

      if (line == "CALIBRATE" ||
          line == "c" ||
          line == "C") {

        referencePitch = currentPitch;
        calibrated = true;
        headDown = false;
        nodCount = 0;
        lastNodTime = 0;

        Serial.println("Kalibrasi kepala berhasil.");
      }
      else if (line.startsWith("EYE,")) {
        int separator = line.indexOf(',', 4);

        if (separator > 0) {
          int alertValue =
              line.substring(4, separator).toInt();

          eyeTimestamp = line.substring(separator + 1);
          eyeAlert = (alertValue == 1);

          lastEyeRxMs = millis();
        }
      }

      line = "";
    }
    else if (c != '\r') {
      line += c;

      if (line.length() > 150) {
        line = "";
      }
    }
  }

  // Jika data Python terputus, eyeAlert dianggap tidak aktif
  if (lastEyeRxMs == 0 ||
      millis() - lastEyeRxMs > 1500) {
    eyeAlert = false;
  }
}

// ================ TIMESTAMP ================
void printTimestamp(unsigned long elapsed) {
  unsigned long hours = elapsed / 3600000UL;
  unsigned long minutes =
      (elapsed % 3600000UL) / 60000UL;
  unsigned long seconds =
      (elapsed % 60000UL) / 1000UL;
  unsigned long ms = elapsed % 1000UL;

  if (hours < 10) Serial.print('0');
  Serial.print(hours);
  Serial.print(':');

  if (minutes < 10) Serial.print('0');
  Serial.print(minutes);
  Serial.print(':');

  if (seconds < 10) Serial.print('0');
  Serial.print(seconds);
  Serial.print('.');

  if (ms < 100) Serial.print('0');
  if (ms < 10) Serial.print('0');
  Serial.print(ms);
}

// ================ WINDOW 7 DETIK ================
void updateNodCount(unsigned long now) {
  int newCount = 0;

  for (int i = 0; i < nodCount; i++) {
    if (now - nodTimes[i] < WINDOW_MS) {
      nodTimes[newCount++] = nodTimes[i];
    }
  }

  nodCount = newCount;
}

// ================ SETUP ================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(VIB_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  allOff();

  // ----- MAX30102 -----
  I2C_MAX.begin(MAX_SDA, MAX_SCL, 100000);

  Serial.println("Memulai MAX30102...");

  if (!max30102.begin(
        I2C_MAX,
        I2C_SPEED_STANDARD)) {

    Serial.println("ERROR: MAX30102 tidak terdeteksi!");

    while (true) {
      allOff();
      delay(1000);
    }
  }

  max30102.setup(60, 1, 2, 100, 411, 4096);
  max30102.setPulseAmplitudeRed(0x1F);
  max30102.setPulseAmplitudeIR(0x1F);
  max30102.setPulseAmplitudeGreen(0);

  Serial.println("MAX30102 terdeteksi.");

  // ----- MPU6050 -----
  I2C_MPU.begin(MPU_SDA, MPU_SCL, 100000);

  Serial.println("Memulai MPU6050...");

  if (!mpu.begin(0x68, &I2C_MPU)) {
    Serial.println("ERROR: MPU6050 tidak terdeteksi!");

    while (true) {
      allOff();
      delay(1000);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 terdeteksi.");
  Serial.println("Tegakkan kepala, lalu ketik c dan Enter.");
  Serial.println("Letakkan jari pada MAX30102.");
  Serial.println("Vibrator: GPIO 25");
  Serial.println("Buzzer: GPIO 27");
  Serial.println();

  Serial.println(
    "Timestamp | Sudut | HR | Nods/7s | Status | Alarm"
  );
}

// ================ LOOP ================
void loop() {
  unsigned long now = millis();

  // ----- BACA MPU6050 -----
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  float pitch = atan2(
    -accel.acceleration.x,
    sqrt(
      accel.acceleration.y * accel.acceleration.y +
      accel.acceleration.z * accel.acceleration.z
    )
  ) * 180.0 / PI;

  // Baca data Python dan kalibrasi
  readEyeSerial(pitch);

  float relativeAngle = pitch - referencePitch;
  float headAngle = fabs(relativeAngle);

  // ----- DETEKSI ANGGUKAN -----
  updateNodCount(now);

  if (calibrated &&
      !headDown &&
      headAngle > HEAD_DOWN_THRESHOLD) {

    headDown = true;

    if (lastNodTime == 0 ||
        now - lastNodTime >= MIN_NOD_INTERVAL) {

      if (nodCount < 20) {
        nodTimes[nodCount++] = now;
      }

      lastNodTime = now;
    }
  }

  if (headDown &&
      headAngle < HEAD_RETURN_THRESHOLD) {
    headDown = false;
  }

  // ----- BACA MAX30102 -----
  long irValue = max30102.getIR();

  if (irValue < FINGER_THRESHOLD) {
    fingerDetected = false;
    validBPM = false;
    bpm = 0;
    bpmSmooth = 0;
    lastBeat = 0;
  } else {
    fingerDetected = true;

    if (checkForBeat(irValue)) {
      if (lastBeat != 0) {
        unsigned long delta = now - lastBeat;

        if (delta > 0) {
          float newBPM = 60000.0 / delta;

          if (newBPM >= 40 && newBPM <= 200) {
            bpm = newBPM;

            if (!validBPM) {
              bpmSmooth = bpm;
              validBPM = true;
            } else {
              bpmSmooth =
                  0.7 * bpmSmooth + 0.3 * bpm;
            }
          }
        }
      }

      lastBeat = now;
    }
  }

  // ----- LOGIKA DETEKSI MENGANTUK -----

  bool heartRateOK =
      !fingerDetected ||
      (validBPM && bpmSmooth < LOW_HR_THRESHOLD);

  bool frequentNods =
      nodCount >= REQUIRED_NODS;

  bool sleepy =
      eyeAlert &&
      frequentNods &&
      heartRateOK;

  // ----- KONTROL, BUZZER, VIBRATOR -----
  updateAlarm(sleepy, now);

  // ----- OUTPUT STATUS -----
  if (now - lastPrint >= PRINT_INTERVAL) {
    lastPrint = now;

    if (eyeTimestamp.length() > 0) {
      Serial.print(eyeTimestamp);
    } else {
      Serial.print("Belum menerima timestamp Python");
    }

    Serial.print(" | Sudut: ");

    if (calibrated) {
      Serial.print(relativeAngle, 1);
      Serial.print(" deg");
    } else {
      Serial.print("Belum kalibrasi");
    }

    Serial.print(" | HR: ");

    if (fingerDetected && validBPM) {
      Serial.print(bpmSmooth, 0);
      Serial.print(" BPM");
    } else if (!fingerDetected) {
      Serial.print("Jari tidak terdeteksi");
    } else {
      Serial.print("Mengukur...");
    }

    Serial.print(" | Nods/7s: ");
    Serial.print(nodCount);

    Serial.print(" | Status: ");
    Serial.print(
      sleepy ? "INDIKASI MENGANTUK" : "NORMAL"
    );

    Serial.print(" | Alarm: ");
    Serial.println(ringing ? "ON" : "OFF");
  }

  delay(5);
}