# Firmware: ESP32 Air Monitor

> **TODO:** วางโค้ด Arduino ที่ใช้อยู่ตอนนี้ไว้ในโฟลเดอร์นี้ชื่อ `esp32-air-monitor.ino`
> (Arduino IDE บังคับให้ชื่อไฟล์ .ino ตรงกับชื่อโฟลเดอร์)

## ฮาร์ดแวร์และการต่อสาย

| อุปกรณ์ | อินเทอร์เฟซ | หมายเหตุ |
|---|---|---|
| PMS5003 ×2 | UART (Serial1, Serial2) | PM1.0 / PM2.5 / PM10 |
| ENS160 | I2C (0x52 หรือ 0x53) | TVOC, eCO₂, AQI-UBA ต้องอุ่นเครื่องก่อนใช้ค่า ส่งอุณหภูมิ/ความชื้นจาก BME280 ให้ชดเชย |
| BME280 | I2C (0x76 หรือ 0x77) | อุณหภูมิ ความชื้น ความกดอากาศ |
| MQ-7 | ADC | ใช้เป็นดัชนี CO เชิงสัมพัทธ์ ไม่รายงานเป็น ppm |
| SD card | SPI | เก็บข้อมูลสำรองเมื่อไม่มีสัญญาณ |

ใส่ขาที่ใช้จริงในตารางนี้ และวางรูปวงจรไว้ที่ `hardware/wiring-diagram.png`

## ตั้งค่า

1. คัดลอก `config.example.h` เป็น `config.h` แล้วใส่ค่า Wi-Fi และบัญชีอุปกรณ์
2. `config.h` ถูก ignore ไว้แล้ว ตรวจด้วย `git status` ว่าไม่ขึ้นมาก่อน commit

## สิ่งที่ต้องแก้ในโค้ดเดิม

ตอนนี้ ESP32 เขียนทับ `/sensor` อย่างเดียว ข้อมูลเก่าจึงหายทุกครั้ง ต้องเพิ่มอีก 2 อย่าง

1. **ล็อกอินก่อนเขียน** เพื่อให้ใช้ Rules ที่ล็อกการเขียนได้ (`firebase/database.rules.json`)
2. **push ประวัติพร้อมเวลา** ไปที่ `/history` ทุกครั้งที่วัด (ใช้ทำ ML และแผนที่)

ตัวอย่างสำหรับไลบรารี *Firebase ESP Client* (Mobizt) ซึ่ง tutorial ส่วนใหญ่ใช้ ถ้าใช้ไลบรารีอื่นให้ปรับตามหลักเดียวกัน

```cpp
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "config.h"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

void setupFirebase() {
  fbConfig.api_key = API_KEY;
  fbConfig.database_url = DATABASE_URL;
  auth.user.email = DEVICE_EMAIL;          // ล็อกอินด้วยบัญชีอุปกรณ์ (แทน anonymous / ไม่ล็อกอิน)
  auth.user.password = DEVICE_PASSWORD;
  fbConfig.token_status_callback = tokenStatusCallback;
  Firebase.begin(&fbConfig, &auth);
  Firebase.reconnectWiFi(true);
}

void sendReading(float pm1, float pm25, float pm10, float tvoc, float eco2,
                 int coRaw, float temp, float rh, double lat, double lng) {
  FirebaseJson j;
  j.set("device", DEVICE_ID);
  j.set("pm1_0", pm1);  j.set("pm2_5", pm25);  j.set("pm10", pm10);
  j.set("tvoc", tvoc);  j.set("eco2", eco2);   j.set("co", coRaw);
  j.set("temperature", temp);  j.set("humidity", rh);
  if (lat != 0) { j.set("lat", lat); j.set("lng", lng); }
  j.set("timestamp/.sv", "timestamp");     // เวลาฝั่งเซิร์ฟเวอร์ (มิลลิวินาที)

  if (Firebase.ready()) {
    Firebase.RTDB.setJSON(&fbdo, "/sensor", &j);    // ค่าล่าสุด (หน้าเว็บเดิมยังใช้ได้)
    Firebase.RTDB.pushJSON(&fbdo, "/history", &j);  // เพิ่มประวัติ 1 รายการ
  } else {
    // ไม่มีสัญญาณ: เขียนบรรทัด CSV ลง SD card แล้วค่อยอัปโหลดทีหลัง
  }
}
```

ชื่อฟิลด์ต้องตรงกับ `fields` ใน `docs/index.html` (`pm2_5`, `pm10`, `pm1_0`, `tvoc`, `eco2`, `co`, `aqi`, `temperature`, `humidity`, `lat`, `lng`, `timestamp`)

ถ้าต้องการให้หน้าเว็บอ่านจากประวัติแทนค่าล่าสุด ให้ตั้ง `rtdbPath: "/history"` และ `rtdbMode: "list"` ใน `docs/index.html`
