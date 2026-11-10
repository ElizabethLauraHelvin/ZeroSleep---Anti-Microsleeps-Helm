#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <math.h>

// ================ I2C TERPISAH ================
// Membuat objek komunikasi I2C untuk menghubungkan sensor.
TwoWire I2C_MAX = TwoWire(0);
// Membuat objek komunikasi I2C untuk menghubungkan sensor.
TwoWire I2C_MPU = TwoWire(1);

// Membuat objek untuk mengakses sensor.
MAX30105 max30102;
// Membuat objek untuk mengakses sensor.
Adafruit_MPU6050 mpu;

// MAX30102: SDA 21, SCL 22
// Menentukan nomor pin atau konfigurasi perangkat keras.
const int MAX_SDA = 21;
// Menentukan nomor pin atau konfigurasi perangkat keras.
const int MAX_SCL = 22;

// MPU6050: SDA 17, SCL 4
// Menentukan nomor pin atau konfigurasi perangkat keras.
const int MPU_SDA = 17;
// Menentukan nomor pin atau konfigurasi perangkat keras.
const int MPU_SCL = 4;

// ================ OUTPUT PINS ================
// Menentukan nomor pin atau konfigurasi perangkat keras.
const int VIB_PIN = 25;
// Menentukan nomor pin atau konfigurasi perangkat keras.
const int BUZZER_PIN = 27;

// Buzzer aktif-LOW
#define BUZ_ON  LOW
#define BUZ_OFF HIGH

// Vibrator aktif-LOW
#define VIB_ON  HIGH
#define VIB_OFF LOW

// Buzzer aktif, bukan passive buzzer
// Menentukan konfigurasi jenis buzzer yang digunakan.
const bool PASSIVE_BUZZER = false;

// ================ KONFIGURASI DETEKSI ================
// Menetapkan nilai ambang yang digunakan dalam proses deteksi.
const float HEAD_DOWN_THRESHOLD = 25.0;
// Menetapkan nilai ambang yang digunakan dalam proses deteksi.
const float HEAD_RETURN_THRESHOLD = 6.0;
// Menetapkan nilai ambang yang digunakan dalam proses deteksi.
const float LOW_HR_THRESHOLD = 90.0;

// Menentukan nomor pin atau konfigurasi perangkat keras.
const int REQUIRED_NODS = 2;

// Menetapkan interval waktu atau batas durasi dalam milidetik.
const unsigned long WINDOW_MS = 7000;
// Menetapkan interval waktu atau batas durasi dalam milidetik.
const unsigned long PRINT_INTERVAL = 200;
// Menetapkan interval waktu atau batas durasi dalam milidetik.
const unsigned long MIN_NOD_INTERVAL = 300;

// Menetapkan ambang nilai pembacaan sensor.
const long FINGER_THRESHOLD = 50000;

// ================ KONFIGURASI ALARM ================
// Menetapkan interval waktu atau batas durasi dalam milidetik.
const unsigned long STAGE2_MS = 45000;

// ================ VARIABEL DETEKSI ================
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
unsigned long lastPrint = 0;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
unsigned long lastBeat = 0;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
unsigned long lastNodTime = 0;

// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
float referencePitch = 0;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
float bpm = 0;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
float bpmSmooth = 0;

// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
bool calibrated = false;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
bool headDown = false;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
bool fingerDetected = false;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
bool validBPM = false;

// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
unsigned long nodTimes[20];
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
int nodCount = 0;

// ================ PYTHON EYE ALERT ================
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
bool eyeAlert = false;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
String eyeTimestamp = "";

// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
unsigned long lastEyeRxMs = 0;

// ================ VARIABEL ALARM ================
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
bool ringing = false;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
bool buzzerState = false;

// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
unsigned long alarmStart = 0;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
unsigned long lastBuzzerToggle = 0;

// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
int sirenDelay = 80;
// Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
int sirenDirection = -1;

// ================ MATIKAN SEMUA OUTPUT ================
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void allOff() {
  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (PASSIVE_BUZZER) {
    // Menghentikan keluaran nada pada buzzer pasif.
    noTone(BUZZER_PIN);
  }

  // Mengatur kondisi HIGH atau LOW pada pin aktuator.
  digitalWrite(BUZZER_PIN, BUZ_OFF);
  // Mengatur kondisi HIGH atau LOW pada pin aktuator.
  digitalWrite(VIB_PIN, VIB_OFF);

  // Memperbarui status ON/OFF buzzer.
  buzzerState = false;
}

// ================ MULAI ALARM ================
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void startAlarm() {
  // Memperbarui status apakah alarm sedang aktif.
  ringing = true;
  // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
  alarmStart = millis();
  // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
  lastBuzzerToggle = millis();

  // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
  sirenDelay = 80;
  // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
  sirenDirection = -1;
  // Memperbarui status ON/OFF buzzer.
  buzzerState = false;

  // Mengatur kondisi HIGH atau LOW pada pin aktuator.
  digitalWrite(VIB_PIN, VIB_ON);

  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("INDIKASI MENGANTUK: ALARM AKTIF");
}

// ================ HENTIKAN ALARM ================
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void stopAlarm() {
  // Memperbarui status apakah alarm sedang aktif.
  ringing = false;
  // Memanggil fungsi untuk mematikan seluruh output aktuator.
  allOff();
}

// ================ UPDATE ALARM ================
// Alarm berjalan non-blocking agar sensor dan data Python tetap terbaca.
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void updateAlarm(bool sleepy, unsigned long now) {
  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (!sleepy) {
    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (ringing) {
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.println("Status NORMAL: ALARM DIMATIKAN");
    }

    // Menghentikan mekanisme alarm dan mematikan output.
    stopAlarm();
    // Mengembalikan hasil fungsi atau menghentikan pemrosesan fungsi.
    return;
  }

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (!ringing) {
    // Memulai mekanisme alarm ketika kondisi mengantuk terdeteksi.
    startAlarm();
  }

  // Output ini dipertahankan aktif selama status mengantuk.
  // Mengatur kondisi HIGH atau LOW pada pin aktuator.
  digitalWrite(VIB_PIN, VIB_ON);

  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  bool fastYelp = (now - alarmStart >= STAGE2_MS);

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (now - lastBuzzerToggle < (unsigned long)sirenDelay) {
    // Mengembalikan hasil fungsi atau menghentikan pemrosesan fungsi.
    return;
  }

  // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
  lastBuzzerToggle = now;
  // Memperbarui status ON/OFF buzzer.
  buzzerState = !buzzerState;

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (PASSIVE_BUZZER) {
    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (buzzerState) {
      // Untuk buzzer pasif: variasi nada sederhana.
      // Menghasilkan nada dengan frekuensi tertentu pada buzzer pasif.
      tone(BUZZER_PIN, fastYelp ? 1500 : 1000);
    } else {
      // Menghentikan keluaran nada pada buzzer pasif.
      noTone(BUZZER_PIN);
    }
  } else {
    // Active buzzer mengikuti pola ON/OFF dari kode asli.
    // Mengatur kondisi HIGH atau LOW pada pin aktuator.
    digitalWrite(BUZZER_PIN, buzzerState ? BUZ_ON : BUZ_OFF);
  }

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (fastYelp) {
    // Tahap 2: yelp cepat, meniru pola yelpActive().
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    sirenDelay -= 4;
    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (sirenDelay < 6) {
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      sirenDelay = 30;
    }
  } else {
    // Tahap 1: wail, semakin cepat lalu melambat.
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    sirenDelay += sirenDirection * 5;

    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (sirenDelay <= 10) {
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      sirenDelay = 10;
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      sirenDirection = 1;
    } else if (sirenDelay >= 80) {
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      sirenDelay = 80;
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      sirenDirection = -1;
    }
  }
}

// ================ RECEIVE PYTHON DATA ================
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void readEyeSerial(float currentPitch) {
  // Mempertahankan nilai variabel selama fungsi dipanggil kembali.
  static String line = "";

  // Mengulang proses selama kondisi masih terpenuhi.
  while (Serial.available() > 0) {
    // Menyimpan satu karakter yang dibaca dari komunikasi serial.
    char c = Serial.read();

    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (c == '\n') {
      // Memproses buffer pesan serial yang diterima dari Python.
      line.trim();

      // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
      if (line == "CALIBRATE" ||
          // Memproses buffer pesan serial yang diterima dari Python.
          line == "c" ||
          // Memproses buffer pesan serial yang diterima dari Python.
          line == "C") {

        // Menyimpan sudut kepala sebagai posisi referensi setelah kalibrasi.
        referencePitch = currentPitch;
        // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
        calibrated = true;
        // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
        headDown = false;
        // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
        nodCount = 0;
        // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
        lastNodTime = 0;

        // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
        Serial.println("Kalibrasi kepala berhasil.");
      }
      // Memeriksa kondisi alternatif jika kondisi sebelumnya tidak terpenuhi.
      else if (line.startsWith("EYE,")) {
        // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
        int separator = line.indexOf(',', 4);

        // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
        if (separator > 0) {
          // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
          int alertValue =
              // Memproses buffer pesan serial yang diterima dari Python.
              line.substring(4, separator).toInt();

          // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
          eyeTimestamp = line.substring(separator + 1);
          // Menentukan status berdasarkan hasil pembacaan sensor dan deteksi mata.
          eyeAlert = (alertValue == 1);

          // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
          lastEyeRxMs = millis();
        }
      }

      // Memproses buffer pesan serial yang diterima dari Python.
      line = "";
    }
    // Memeriksa kondisi alternatif jika kondisi sebelumnya tidak terpenuhi.
    else if (c != '\r') {
      // Memproses buffer pesan serial yang diterima dari Python.
      line += c;

      // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
      if (line.length() > 150) {
        // Memproses buffer pesan serial yang diterima dari Python.
        line = "";
      }
    }
  }

  // Jika data Python terputus, eyeAlert dianggap tidak aktif
  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (lastEyeRxMs == 0 ||
      // Menggunakan waktu berjalan sejak ESP32 mulai aktif.
      millis() - lastEyeRxMs > 1500) {
    // Menentukan status berdasarkan hasil pembacaan sensor dan deteksi mata.
    eyeAlert = false;
  }
}

// ================ TIMESTAMP ================
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void printTimestamp(unsigned long elapsed) {
  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  unsigned long hours = elapsed / 3600000UL;
  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  unsigned long minutes =
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      (elapsed % 3600000UL) / 60000UL;
  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  unsigned long seconds =
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      (elapsed % 60000UL) / 1000UL;
  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  unsigned long ms = elapsed % 1000UL;

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (hours < 10) Serial.print('0');
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.print(hours);
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.print(':');

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (minutes < 10) Serial.print('0');
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.print(minutes);
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.print(':');

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (seconds < 10) Serial.print('0');
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.print(seconds);
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.print('.');

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (ms < 100) Serial.print('0');
  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (ms < 10) Serial.print('0');
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.print(ms);
}

// ================ WINDOW 7 DETIK ================
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void updateNodCount(unsigned long now) {
  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  int newCount = 0;

  // Mengulangi instruksi untuk setiap elemen atau data yang diproses.
  for (int i = 0; i < nodCount; i++) {
    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (now - nodTimes[i] < WINDOW_MS) {
      // Menyimpan waktu terjadinya anggukan untuk menghitung frekuensi anggukan.
      nodTimes[newCount++] = nodTimes[i];
    }
  }

  // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
  nodCount = newCount;
}

// ================ SETUP ================
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void setup() {
  // Memulai komunikasi serial dengan kecepatan yang ditentukan.
  Serial.begin(115200);
  // Memberikan jeda waktu sebelum instruksi berikutnya dijalankan.
  delay(500);

  // Mengatur pin perangkat sebagai input atau output.
  pinMode(VIB_PIN, OUTPUT);
  // Mengatur pin perangkat sebagai input atau output.
  pinMode(BUZZER_PIN, OUTPUT);

  // Memanggil fungsi untuk mematikan seluruh output aktuator.
  allOff();

  // ----- MAX30102 -----
  // Menginisialisasi atau menggunakan jalur komunikasi I2C sensor.
  I2C_MAX.begin(MAX_SDA, MAX_SCL, 100000);

  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("Memulai MAX30102...");

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (!max30102.begin(
        // Menginisialisasi atau menggunakan jalur komunikasi I2C sensor.
        I2C_MAX,
        // Menginisialisasi atau menggunakan jalur komunikasi I2C sensor.
        I2C_SPEED_STANDARD)) {

    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.println("ERROR: MAX30102 tidak terdeteksi!");

    // Mengulang proses selama kondisi masih terpenuhi.
    while (true) {
      // Memanggil fungsi untuk mematikan seluruh output aktuator.
      allOff();
      // Memberikan jeda waktu sebelum instruksi berikutnya dijalankan.
      delay(1000);
    }
  }

  // Mengatur atau membaca sensor MAX30102.
  max30102.setup(60, 1, 2, 100, 411, 4096);
  // Mengatur atau membaca sensor MAX30102.
  max30102.setPulseAmplitudeRed(0x1F);
  // Mengatur atau membaca sensor MAX30102.
  max30102.setPulseAmplitudeIR(0x1F);
  // Mengatur atau membaca sensor MAX30102.
  max30102.setPulseAmplitudeGreen(0);

  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("MAX30102 terdeteksi.");

  // ----- MPU6050 -----
  // Menginisialisasi atau menggunakan jalur komunikasi I2C sensor.
  I2C_MPU.begin(MPU_SDA, MPU_SCL, 100000);

  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("Memulai MPU6050...");

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (!mpu.begin(0x68, &I2C_MPU)) {
    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.println("ERROR: MPU6050 tidak terdeteksi!");

    // Mengulang proses selama kondisi masih terpenuhi.
    while (true) {
      // Memanggil fungsi untuk mematikan seluruh output aktuator.
      allOff();
      // Memberikan jeda waktu sebelum instruksi berikutnya dijalankan.
      delay(1000);
    }
  }

  // Mengatur atau membaca sensor MPU6050.
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  // Mengatur atau membaca sensor MPU6050.
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("MPU6050 terdeteksi.");
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("Tegakkan kepala, lalu ketik c dan Enter.");
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("Letakkan jari pada MAX30102.");
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("Vibrator: GPIO 25");
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println("Buzzer: GPIO 27");
  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println();

  // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
  Serial.println(
    "Timestamp | Sudut | HR | Nods/7s | Status | Alarm"
  );
}

// ================ LOOP ================
// Mendefinisikan fungsi untuk menjalankan proses pada bagian ini.
void loop() {
  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  unsigned long now = millis();

  // ----- BACA MPU6050 -----
  // Menyiapkan struktur untuk menampung data akselerometer, giroskop, dan suhu.
  sensors_event_t accel, gyro, temp;
  // Mengatur atau membaca sensor MPU6050.
  mpu.getEvent(&accel, &gyro, &temp);

  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  float pitch = atan2(
    -accel.acceleration.x,
    // Melakukan perhitungan matematis untuk memperoleh sudut atau nilai sensor.
    sqrt(
      accel.acceleration.y * accel.acceleration.y +
      accel.acceleration.z * accel.acceleration.z
    )
  // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
  ) * 180.0 / PI;

  // Baca data Python dan kalibrasi
  // Membaca pesan deteksi mata yang dikirimkan oleh program Python.
  readEyeSerial(pitch);

  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  float relativeAngle = pitch - referencePitch;
  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  float headAngle = fabs(relativeAngle);

  // ----- DETEKSI ANGGUKAN -----
  // Memperbarui jumlah anggukan yang masih berada dalam jendela waktu.
  updateNodCount(now);

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (calibrated &&
      !headDown &&
      headAngle > HEAD_DOWN_THRESHOLD) {

    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    headDown = true;

    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (lastNodTime == 0 ||
        // Menghitung selisih waktu untuk menentukan durasi atau interval.
        now - lastNodTime >= MIN_NOD_INTERVAL) {

      // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
      if (nodCount < 20) {
        // Menyimpan waktu terjadinya anggukan untuk menghitung frekuensi anggukan.
        nodTimes[nodCount++] = now;
      }

      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      lastNodTime = now;
    }
  }

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (headDown &&
      headAngle < HEAD_RETURN_THRESHOLD) {
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    headDown = false;
  }

  // ----- BACA MAX30102 -----
  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  long irValue = max30102.getIR();

  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (irValue < FINGER_THRESHOLD) {
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    fingerDetected = false;
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    validBPM = false;
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    bpm = 0;
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    bpmSmooth = 0;
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    lastBeat = 0;
  } else {
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    fingerDetected = true;

    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (checkForBeat(irValue)) {
      // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
      if (lastBeat != 0) {
        // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
        unsigned long delta = now - lastBeat;

        // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
        if (delta > 0) {
          // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
          float newBPM = 60000.0 / delta;

          // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
          if (newBPM >= 40 && newBPM <= 200) {
            // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
            bpm = newBPM;

            // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
            if (!validBPM) {
              // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
              bpmSmooth = bpm;
              // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
              validBPM = true;
            } else {
              bpmSmooth =
                  // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
                  0.7 * bpmSmooth + 0.3 * bpm;
            }
          }
        }
      }

      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      lastBeat = now;
    }
  }

  // ----- LOGIKA DETEKSI MENGANTUK -----

  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  bool heartRateOK =
      !fingerDetected ||
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      (validBPM && bpmSmooth < LOW_HR_THRESHOLD);

  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  bool frequentNods =
      // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
      nodCount >= REQUIRED_NODS;

  // Mendeklarasikan variabel untuk menyimpan status atau nilai hasil pemrosesan.
  bool sleepy =
      // Menentukan status berdasarkan hasil pembacaan sensor dan deteksi mata.
      eyeAlert &&
      // Menentukan status berdasarkan hasil pembacaan sensor dan deteksi mata.
      frequentNods &&
      // Menentukan status berdasarkan hasil pembacaan sensor dan deteksi mata.
      heartRateOK;

  // ----- KONTROL, BUZZER, VIBRATOR -----
  // Memperbarui kondisi buzzer dan vibrator berdasarkan status kantuk.
  updateAlarm(sleepy, now);

  // ----- OUTPUT STATUS -----
  // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
  if (now - lastPrint >= PRINT_INTERVAL) {
    // Menjalankan instruksi untuk memperbarui nilai atau memproses data.
    lastPrint = now;

    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (eyeTimestamp.length() > 0) {
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print(eyeTimestamp);
    } else {
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print("Belum menerima timestamp Python");
    }

    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.print(" | Sudut: ");

    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (calibrated) {
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print(relativeAngle, 1);
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print(" deg");
    } else {
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print("Belum kalibrasi");
    }

    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.print(" | HR: ");

    // Memeriksa kondisi sebelum menjalankan instruksi berikutnya.
    if (fingerDetected && validBPM) {
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print(bpmSmooth, 0);
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print(" BPM");
    } else if (!fingerDetected) {
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print("Jari tidak terdeteksi");
    } else {
      // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
      Serial.print("Mengukur...");
    }

    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.print(" | Nods/7s: ");
    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.print(nodCount);

    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.print(" | Status: ");
    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.print(
      // Menentukan status berdasarkan hasil pembacaan sensor dan deteksi mata.
      sleepy ? "INDIKASI MENGANTUK" : "NORMAL"
    );

    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.print(" | Alarm: ");
    // Mengirimkan informasi ke Serial Monitor untuk pemantauan.
    Serial.println(ringing ? "ON" : "OFF");
  }

  // Memberikan jeda waktu sebelum instruksi berikutnya dijalankan.
  delay(5);
}
