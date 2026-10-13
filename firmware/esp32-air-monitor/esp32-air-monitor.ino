/*
  Smart Air Quality Monitor — ESP32 firmware
  ระบบตรวจวัดคุณภาพอากาศอัจฉริยะด้วย IoT

  อ่านค่า   PMS5003 ×2 (PM1.0/2.5/10), ENS160 (TVOC, eCO2, AQI-UBA), MQ-7 (ดัชนี CO), MQ135 (ดัชนีก๊าซรวม), BME280 (T, RH, P)
  ส่งไป     Firebase Realtime Database
              /sensor        ค่าล่าสุด (เขียนทับ)  ← หน้าเว็บ docs/index.html อ่านจากที่นี่
              /history       ประวัติ 1 รายการต่อรอบ พร้อม timestamp ของเซิร์ฟเวอร์
  สำรอง     SD card ไฟล์ /YYYYMMDD.csv ทุกรอบ (คอลัมน์ uploaded = 0 ถ้าส่งขึ้น Firebase ไม่สำเร็จ)

  ใช้ได้ทั้งตอนที่ Database Rules ยังเปิด (เว้น DEVICE_EMAIL ว่าง) และหลังล็อกการเขียนแล้ว (ใส่บัญชีอุปกรณ์)
  เลือกเซนเซอร์ที่ติดตั้งได้ใน config.h (USE_ENS160, USE_BME280, USE_MQ7, USE_MQ135) ตัวที่ปิดไว้ไม่ต้องลงไลบรารี
  PMS5003 ตัวที่ 2 และ SD card ตรวจหาเองตอนเปิดเครื่อง ถ้าไม่มีก็ข้ามไป

  วิธีตั้งค่าและอัปโหลดดูที่ README.md ในโฟลเดอร์นี้
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>

#if __has_include("config.h")
#include "config.h"
#else
#error "Copy config.example.h to config.h (same folder) and fill in Wi-Fi / Firebase settings"
#endif

// เซนเซอร์ที่ติดตั้ง ตั้งใน config.h (0 = ไม่มี ไม่ต้องลงไลบรารีของตัวนั้น)
#ifndef USE_ENS160
#define USE_ENS160 0
#endif
#ifndef USE_BME280
#define USE_BME280 0
#endif
#ifndef USE_MQ7
#define USE_MQ7 0
#endif
#ifndef USE_MQ135
#define USE_MQ135 0
#endif

#if USE_ENS160
#include <SparkFun_ENS160.h>   // Library Manager: "SparkFun Indoor Air Quality Sensor - ENS160"
#endif
#if USE_BME280
#include <Adafruit_BME280.h>   // Library Manager: "Adafruit BME280 Library"
#endif

#ifndef SEND_INTERVAL_MS
#define SEND_INTERVAL_MS 10000
#endif

// ============ ขาที่ต่อ (แก้ให้ตรงกับวงจรจริง) ============
#define PMS_A_RX   16   // ESP32 RX  <- TX ของ PMS5003 ตัวที่ 1
#define PMS_A_TX   17   // ESP32 TX  -> RX ของ PMS5003 ตัวที่ 1 (ไม่ต่อก็ได้)
#define PMS_B_RX   25   // ESP32 RX  <- TX ของ PMS5003 ตัวที่ 2
#define PMS_B_TX   26
#define I2C_SDA    21   // ENS160 + BME280 ใช้บัส I2C ร่วมกัน
#define I2C_SCL    22
#define MQ7_PIN 33   // ต้องเป็นขา ADC1 (32–39) เพราะ ADC2 ใช้ไม่ได้ตอนเปิด Wi-Fi
#define MQ135_PIN 32   // ADC1 เช่นกัน
#define SD_CS       5   // SPI: SCK 18, MISO 19, MOSI 23
// =========================================================

// ค่าที่ส่งในแต่ละรอบ (ต้องประกาศก่อนฟังก์ชันแรก เพราะ Arduino IDE สร้าง prototype ไว้เหนือฟังก์ชันแรก)
struct Reading {
  float pm1, pm25, pm10, pm25a, pm25b, pm1cf, pm25cf, pm10cf;
  float tvoc, eco2, aqi, co, mq135, temp, rh, pres;
  int ensStatus;
};

// ---------- PMS5003: อ่านเฟรม 32 ไบต์แบบไม่บล็อก แล้วเฉลี่ยทุกเฟรมในรอบส่ง ----------
struct PmsSensor {
  HardwareSerial *port;
  uint8_t buf[32];
  uint8_t idx = 0;
  double sum[6] = {0};       // 0–2 = CF=1 (PM1.0, PM2.5, PM10), 3–5 = ATM (ค่าบรรยากาศ)
  uint32_t n = 0, frames = 0, bad = 0, lastFrame = 0;

  explicit PmsSensor(HardwareSerial *p) : port(p) {}
  uint16_t u16(int i) const { return (uint16_t(buf[i]) << 8) | buf[i + 1]; }

  void feed(uint8_t b) {
    if (idx == 0 && b != 0x42) return;                       // รอหัวเฟรม 0x42 0x4D
    if (idx == 1 && b != 0x4D) { idx = (b == 0x42) ? 1 : 0; return; }
    buf[idx++] = b;
    if (idx < 32) return;
    idx = 0;
    uint16_t sum16 = 0;
    for (int i = 0; i < 30; i++) sum16 += buf[i];
    if (u16(2) != 28 || sum16 != u16(30)) { bad++; return; }   // ความยาวหรือ checksum ไม่ตรง
    for (int i = 0; i < 6; i++) sum[i] += u16(4 + 2 * i);
    n++; frames++; lastFrame = millis();
  }
  void poll() { while (port->available()) feed((uint8_t)port->read()); }
  float mean(int i) const { return n ? (float)(sum[i] / n) : NAN; }
  void reset() { for (int i = 0; i < 6; i++) sum[i] = 0; n = 0; }
  bool alive() const { return frames > 0 && millis() - lastFrame < 10000; }
};

PmsSensor pmsA(&Serial2);
PmsSensor pmsB(&Serial1);
#if USE_ENS160
SparkFun_ENS160 ens;
#endif
#if USE_BME280
Adafruit_BME280 bme;
#endif
bool ensOk = false, bmeOk = false, sdOk = false;

// ค่าล่าสุดของ ENS160 (อ่านได้ราว 1 ครั้ง/วินาที)
float ensTvoc = NAN, ensEco2 = NAN, ensAqi = NAN;
int ensStatus = -1;          // 0 = ปกติ, 1 = อุ่นเครื่อง (~3 นาที), 2 = เริ่มใช้ครั้งแรก (~1 ชม.), 3 = ไม่มีค่า

// MQ-7 / MQ135 เฉลี่ยค่า ADC ทุก 100 ms
double mqSum = 0, mq135Sum = 0;
uint32_t mqN = 0, mq135N = 0, lastMq = 0, lastEns = 0;

// Firebase
String dbBase;               // DATABASE_URL ไม่มี / ท้าย
String idToken;
uint32_t tokenAt = 0;
bool useAuth() { return strlen(DEVICE_EMAIL) > 0; }


// ---------------- helpers ----------------
float avg2(float a, float b) {
  if (isnan(a)) return b;
  if (isnan(b)) return a;
  return (a + b) / 2;
}

int i2cRead8(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((int)addr, 1) != 1) return -1;
  return Wire.read();
}

void i2cScan() {
  Serial.print("[i2c] found:");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); found++; }
  }
  Serial.println(found ? "" : " none (check SDA/SCL wiring and 3.3V power)");
}

// ดึงค่า "key": "value" จาก JSON แบบง่าย (พอสำหรับคำตอบของ Firebase Auth)
String jsonString(const String &s, const char *key) {
  int k = s.indexOf(String("\"") + key + "\"");
  if (k < 0) return "";
  int c = s.indexOf(':', k);
  int q1 = s.indexOf('"', c + 1);
  int q2 = s.indexOf('"', q1 + 1);
  if (c < 0 || q1 < 0 || q2 < 0) return "";
  return s.substring(q1 + 1, q2);
}

void addNum(String &j, const char *key, float v, int decimals) {
  if (isnan(v)) return;                      // ไม่ส่งฟิลด์ที่ไม่มีค่า
  if (j.length() > 1) j += ',';
  j += '"'; j += key; j += "\":"; j += String(v, decimals);
}

// ---------------- Wi-Fi ----------------
const char *wifiReason(int st) {
  switch (st) {
    case WL_NO_SSID_AVAIL:   return "network name not found (wrong SSID, 5 GHz only, or out of range)";
    case WL_CONNECT_FAILED:  return "connect failed (wrong password?)";
    case WL_CONNECTION_LOST: return "connection lost";
    case WL_DISCONNECTED:    return "disconnected (often wrong password, or AP rejected)";
    case WL_IDLE_STATUS:     return "idle / still trying";
    default:                 return "unknown";
  }
}

// แสดงชื่อ Wi-Fi ที่ ESP32 มองเห็น (เฉพาะ 2.4 GHz) เพื่อเทียบกับ WIFI_SSID ใน config.h
void scanWiFi() {
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  Serial.printf("[wifi] %d networks visible (ESP32 sees 2.4 GHz only):\n", n);
  bool found = false;
  for (int i = 0; i < n && i < 15; i++) {
    bool mine = WiFi.SSID(i) == String(WIFI_SSID);
    found |= mine;
    Serial.printf("   \"%s\"  %d dBm%s\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), mine ? "   <-- matches config.h" : "");
  }
  if (!found) Serial.printf("   \"%s\" from config.h is NOT in the list: check spelling/case, or switch the hotspot to 2.4 GHz\n", WIFI_SSID);
  WiFi.scanDelete();
}

bool timeStarted = false, wifiWasUp = false;
void onWiFiUp() {
  wifiWasUp = true;
  Serial.printf("[wifi] connected  IP %s  RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  if (!timeStarted) { configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com"); timeStarted = true; }  // เวลาไทยสำหรับ SD
}

void connectWiFi(uint32_t waitMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connecting to \"%s\"", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < waitMs) { delay(500); Serial.print('.'); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) { onWiFiUp(); return; }
  Serial.printf("[wifi] FAILED: %s\n", wifiReason(WiFi.status()));
  scanWiFi();
  Serial.println("[wifi] will keep retrying every 30 s; readings are still logged to SD");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void keepWiFi() {
  static uint32_t lastTry = 0;
  bool up = WiFi.status() == WL_CONNECTED;
  if (up && !wifiWasUp) onWiFiUp();
  if (!up) wifiWasUp = false;
  if (up || millis() - lastTry < 30000) return;
  lastTry = millis();
  Serial.printf("[wifi] not connected (%s), retrying...\n", wifiReason(WiFi.status()));
  WiFi.disconnect();                          // เริ่มใหม่ทั้งรอบ แทน reconnect() ที่ชนกับรอบที่ยังค้างอยู่
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ---------------- Firebase (REST API) ----------------
bool signIn() {
  WiFiClientSecure client;
  client.setInsecure();                      // ไม่ตรวจใบรับรอง: ง่ายสำหรับต้นแบบ (ดู README หัวข้อความปลอดภัย)
  HTTPClient http;
  http.setTimeout(10000);
  String url = String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + API_KEY;
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"email\":\"") + DEVICE_EMAIL + "\",\"password\":\"" + DEVICE_PASSWORD +
                "\",\"returnSecureToken\":true}";
  int code = http.POST(body);
  String res = http.getString();
  http.end();
  if (code != 200) {
    Serial.printf("[auth] sign-in failed (%d): %s\n", code, res.substring(0, 160).c_str());
    return false;
  }
  idToken = jsonString(res, "idToken");
  tokenAt = millis();
  Serial.println(idToken.length() ? "[auth] signed in" : "[auth] no idToken in response");
  return idToken.length() > 0;
}

int fbRequest(const char *method, const char *path, const String &body) {
  if (useAuth() && (idToken.isEmpty() || millis() - tokenAt > 50UL * 60 * 1000)) {   // token อายุ 1 ชม.
    if (!signIn()) return -100;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  String url = dbBase + path + ".json";
  if (useAuth()) url += "?auth=" + idToken;
  if (!http.begin(client, url)) return -101;
  http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest(method, body);
  if (code != 200) {
    Serial.printf("[firebase] %s %s -> %d %s\n", method, path, code,
                  code > 0 ? http.getString().substring(0, 160).c_str() : http.errorToString(code).c_str());
    if (code == 401 || code == 403) {
      if (useAuth()) idToken = "";           // ให้ล็อกอินใหม่รอบหน้า
      else Serial.println("[firebase] Rules deny writes: set DEVICE_EMAIL/DEVICE_PASSWORD in config.h");
    }
  }
  http.end();
  return code;
}

bool sendFirebase(const Reading &r) {
  if (WiFi.status() != WL_CONNECTED) return false;
  String j = "{";
  j += "\"device\":\""; j += DEVICE_ID; j += '"';
  addNum(j, "pm1_0", r.pm1, 1);       addNum(j, "pm2_5", r.pm25, 1);     addNum(j, "pm10", r.pm10, 1);
  addNum(j, "pm2_5_a", r.pm25a, 1);   addNum(j, "pm2_5_b", r.pm25b, 1);
  addNum(j, "pm1_0_cf1", r.pm1cf, 1); addNum(j, "pm2_5_cf1", r.pm25cf, 1); addNum(j, "pm10_cf1", r.pm10cf, 1);
  addNum(j, "tvoc", r.tvoc, 0);       addNum(j, "eco2", r.eco2, 0);      addNum(j, "aqi", r.aqi, 0);
  addNum(j, "ens_status", r.ensStatus < 0 ? NAN : (float)r.ensStatus, 0);
  addNum(j, "co", r.co, 0);           addNum(j, "mq135", r.mq135, 0);
  addNum(j, "temperature", r.temp, 1); addNum(j, "humidity", r.rh, 1);  addNum(j, "pressure", r.pres, 1);
  j += ",\"timestamp\":{\".sv\":\"timestamp\"}}";   // เวลาฝั่งเซิร์ฟเวอร์ (ms)

  int a = fbRequest("PUT", "/sensor", j);     // ค่าล่าสุด
  int b = fbRequest("POST", "/history", j);   // เพิ่มประวัติ
  return a == 200 && b == 200;
}

// ---------------- SD card ----------------
void logSd(const Reading &r, bool uploaded) {
  if (!sdOk) return;
  struct tm t;
  bool haveTime = getLocalTime(&t, 10);
  char fname[20], ts[32];
  if (haveTime) {
    strftime(fname, sizeof fname, "/%Y%m%d.csv", &t);
    strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%S", &t);
  } else {
    strcpy(fname, "/notime.csv");                          // ยังไม่ได้เวลาจาก NTP
    snprintf(ts, sizeof ts, "uptime_%lu", (unsigned long)(millis() / 1000));
  }
  bool isNew = !SD.exists(fname);
  File f = SD.open(fname, FILE_APPEND);
  if (!f) { Serial.println("[sd] write failed — card removed?"); sdOk = false; return; }
  if (isNew) f.println("time,device,pm1_0,pm2_5,pm10,pm2_5_a,pm2_5_b,pm1_0_cf1,pm2_5_cf1,pm10_cf1,"
                       "tvoc,eco2,aqi,ens_status,co,mq135,temperature,humidity,pressure,uploaded");
  f.printf("%s,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%d,%.0f,%.0f,%.2f,%.2f,%.1f,%d\n",
           ts, DEVICE_ID, r.pm1, r.pm25, r.pm10, r.pm25a, r.pm25b, r.pm1cf, r.pm25cf, r.pm10cf,
           r.tvoc, r.eco2, r.aqi, r.ensStatus, r.co, r.mq135, r.temp, r.rh, r.pres, uploaded ? 1 : 0);
  f.close();
}

// ---------------- sensors ----------------
void setupSensors() {
#if USE_ENS160 || USE_BME280
  Wire.begin(I2C_SDA, I2C_SCL);
  i2cScan();
#endif

#if USE_ENS160
  for (uint8_t addr : {0x53, 0x52}) {
    if (ens.begin(addr)) {
      ens.setOperatingMode(SFE_ENS160_RESET);
      delay(100);
      ens.setOperatingMode(SFE_ENS160_STANDARD);
      ensOk = true;
      Serial.printf("[ens160] OK at 0x%02X (warm-up ~3 min before values are valid)\n", addr);
      break;
    }
  }
  if (!ensOk) Serial.println("[ens160] not found at 0x53/0x52");
#else
  Serial.println("[ens160] disabled (USE_ENS160 0 in config.h)");
#endif

#if USE_BME280
  for (uint8_t addr : {0x76, 0x77}) {
    if (bme.begin(addr, &Wire)) { bmeOk = true; Serial.printf("[bme280] OK at 0x%02X\n", addr); break; }
    if (i2cRead8(addr, 0xD0) == 0x58) Serial.printf("[bme280] chip at 0x%02X is a BMP280 (no humidity sensor)\n", addr);
  }
  if (!bmeOk) Serial.println("[bme280] not found at 0x76/0x77");
#else
  Serial.println("[bme280] disabled (USE_BME280 0 in config.h)");
#endif
#if !USE_MQ7
  Serial.println("[mq7] disabled (USE_MQ7 0 in config.h)");
#endif
#if !USE_MQ135
  Serial.println("[mq135] disabled (USE_MQ135 0 in config.h)");
#endif
#if USE_MQ7 || USE_MQ135
  Serial.println("[mq] heater needs ~3 min to stabilise after power-on (24-48 h burn-in on first use)");
#endif

  Serial2.setRxBufferSize(1024);
  Serial2.begin(9600, SERIAL_8N1, PMS_A_RX, PMS_A_TX);
  Serial1.setRxBufferSize(1024);
  Serial1.begin(9600, SERIAL_8N1, PMS_B_RX, PMS_B_TX);

  analogReadResolution(12);

  SPI.begin(18, 19, 23, SD_CS);
  sdOk = SD.begin(SD_CS);
  Serial.println(sdOk ? "[sd] OK" : "[sd] no card (continuing without SD backup)");
}

void pollSensors() {
  pmsA.poll();
  pmsB.poll();
  if (millis() - lastMq >= 100) {
    lastMq = millis();
#if USE_MQ7
    mqSum += analogRead(MQ7_PIN);
    mqN++;
#endif
#if USE_MQ135
    mq135Sum += analogRead(MQ135_PIN);
    mq135N++;
#endif
  }
#if USE_ENS160
  if (ensOk && millis() - lastEns >= 250) {            // ไม่ต้องถาม ENS160 ถี่กว่านี้
    lastEns = millis();
    if (ens.checkDataStatus()) {
      ensStatus = ens.getFlags();
      if (ensStatus != 3 && ensStatus != 0xFF) {
        ensTvoc = ens.getTVOC();
        ensEco2 = ens.getECO2();
        ensAqi = ens.getAQI();
      }
    }
  }
#endif
}

Reading collect() {
  Reading r;
  float aCf[3], aAtm[3], bCf[3], bAtm[3];
  for (int i = 0; i < 3; i++) {
    aCf[i] = pmsA.mean(i); aAtm[i] = pmsA.mean(i + 3);
    bCf[i] = pmsB.mean(i); bAtm[i] = pmsB.mean(i + 3);
  }
  pmsA.reset(); pmsB.reset();
  r.pm1 = avg2(aAtm[0], bAtm[0]);  r.pm25 = avg2(aAtm[1], bAtm[1]);  r.pm10 = avg2(aAtm[2], bAtm[2]);
  r.pm25a = aAtm[1];               r.pm25b = bAtm[1];
  r.pm1cf = avg2(aCf[0], bCf[0]);  r.pm25cf = avg2(aCf[1], bCf[1]);  r.pm10cf = avg2(aCf[2], bCf[2]);

  r.tvoc = ensTvoc; r.eco2 = ensEco2; r.aqi = ensAqi; r.ensStatus = ensStatus;
  r.co = mqN ? (float)(mqSum / mqN) : NAN;
  r.mq135 = mq135N ? (float)(mq135Sum / mq135N) : NAN;
  mqSum = 0; mqN = 0; mq135Sum = 0; mq135N = 0;

  r.temp = r.rh = r.pres = NAN;
#if USE_BME280
  if (bmeOk) {
    r.temp = bme.readTemperature();
    r.rh = bme.readHumidity();
    r.pres = bme.readPressure() / 100.0f;            // hPa
  }
#endif
#if USE_ENS160
  if (ensOk && !isnan(r.temp) && !isnan(r.rh)) {     // ชดเชยอุณหภูมิ/ความชื้นให้ ENS160
    ens.setTempCompensationCelsius(r.temp);
    ens.setRHCompensationFloat(r.rh);
  }
#endif
  return r;
}

void printReading(const Reading &r, bool uploaded) {
  struct tm t;
  char ts[12] = "--:--:--";
  if (getLocalTime(&t, 0)) strftime(ts, sizeof ts, "%H:%M:%S", &t);
  Serial.printf("[%s] PM2.5 %.1f (A %.1f / B %.1f) PM10 %.1f", ts, r.pm25, r.pm25a, r.pm25b, r.pm10);
#if USE_ENS160
  Serial.printf(" | TVOC %.0f eCO2 %.0f ens=%d", r.tvoc, r.eco2, r.ensStatus);
#endif
#if USE_MQ7
  Serial.printf(" | MQ7 %.0f", r.co);
#endif
#if USE_MQ135
  Serial.printf(" | MQ135 %.0f", r.mq135);
#endif
#if USE_BME280
  Serial.printf(" | %.1fC %.0f%%", r.temp, r.rh);
#endif
  Serial.printf(" | %s%s\n", uploaded ? "Firebase OK" : "Firebase --", sdOk ? " SD OK" : "");
  if (!pmsA.alive()) Serial.printf("   ! PMS5003 #1: no data on RX %d (frames %lu, bad %lu)\n", PMS_A_RX,
                                   (unsigned long)pmsA.frames, (unsigned long)pmsA.bad);
}

// ---------------- main ----------------
uint32_t lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Smart Air Quality Monitor ===");
  Serial.printf("device %s, send every %d s, auth %s\n", DEVICE_ID, SEND_INTERVAL_MS / 1000,
                useAuth() ? "email/password" : "none (Rules must allow writes)");
  dbBase = DATABASE_URL;
  while (dbBase.endsWith("/")) dbBase.remove(dbBase.length() - 1);
  setupSensors();
  connectWiFi(20000);
  lastSend = millis();
}

void loop() {
  pollSensors();
  keepWiFi();
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    Reading r = collect();
    bool ok = sendFirebase(r);
    logSd(r, ok);
    printReading(r, ok);
  }
}
