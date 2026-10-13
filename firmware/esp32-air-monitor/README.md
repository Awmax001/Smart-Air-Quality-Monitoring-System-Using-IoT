# Firmware: ESP32 Air Monitor

`esp32-air-monitor.ino` อ่านค่าเซนเซอร์ทุกตัว ส่งขึ้น Firebase ทุก 10 วินาที และบันทึกสำรองลง SD card

- `/sensor` ค่าล่าสุด (หน้าเว็บอ่านจากที่นี่ เหมือนโค้ดเดิม)
- `/history` เพิ่ม 1 รายการต่อรอบ พร้อม `timestamp` ของเซิร์ฟเวอร์ (ใช้ทำ ML และแผนที่)
- เลือกเซนเซอร์ที่มีใน `config.h` (`USE_ENS160`, `USE_BME280`, `USE_MQ7`, `USE_MQ135`) ค่าเริ่มต้นปิดทั้งหมด ใช้ได้ทันทีกับ ESP32 + PMS5003 ตัวเดียว
- PMS5003 ตัวที่ 2 และ SD card โปรแกรมตรวจหาเอง ถ้าไม่มีก็ข้ามไป

> ถ้ามีโค้ดเดิมที่ใช้อยู่ ให้เก็บไว้ที่ `firmware/legacy/` เพื่ออ้างอิง แล้วเทียบขาที่ต่อจริงกับตารางด้านล่างก่อนอัปโหลด

---

## 1. ติดตั้ง Arduino IDE และบอร์ด ESP32 (ทำครั้งเดียวต่อเครื่อง)

1. ติดตั้ง **Arduino IDE 2.x** จาก arduino.cc/en/software
2. **File → Preferences → Additional boards manager URLs** ใส่
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager** ค้นหา `esp32` ติดตั้ง **esp32 by Espressif Systems** (รุ่น 3.x)
4. เสียบ ESP32 แล้วดูใน Device Manager → Ports (COM & LPT) ว่าขึ้น COM port
   ถ้าไม่ขึ้น ให้ดูชิปบนบอร์ดแล้วติดตั้งไดรเวอร์
   - ชิป **CP2102** → ไดรเวอร์ *Silicon Labs CP210x*
   - ชิป **CH340 / CH9102** → ไดรเวอร์ *WCH CH340/CH341*
   - ใช้สาย USB ที่ส่งข้อมูลได้ (สายชาร์จอย่างเดียวจะไม่ขึ้นพอร์ต)

## 2. ติดตั้งไลบรารี (เฉพาะเซนเซอร์ที่มี)

ถ้าตอนนี้มีแค่ ESP32 + PMS5003 **ข้ามข้อนี้ได้เลย** ไม่ต้องลงไลบรารีใด ๆ
เมื่อได้เซนเซอร์มาแล้ว เข้า **Sketch → Include Library → Manage Libraries** แล้วติดตั้งตัวที่ใช้ จากนั้นเปลี่ยน `USE_...` ใน `config.h` เป็น `1`

| ค้นหา | ไลบรารี | ใช้กับ |
|---|---|---|
| `ENS160` | **SparkFun Indoor Air Quality Sensor - ENS160** | ENS160 |
| `BME280` | **Adafruit BME280 Library** (กด *Install all* เพื่อลง Adafruit Unified Sensor และ Adafruit BusIO ด้วย) | BME280 |

ไม่ต้องลงไลบรารี Firebase เพราะโค้ดคุยกับ Firebase ผ่าน REST API ด้วย `HTTPClient` ที่มากับบอร์ด ESP32 อยู่แล้ว
ส่วน `WiFi`, `WiFiClientSecure`, `HTTPClient`, `Wire`, `SPI`, `SD` ก็มากับบอร์ด ไม่ต้องติดตั้งเพิ่ม

## 3. ต่อสาย

| อุปกรณ์ | ขาอุปกรณ์ | ESP32 | หมายเหตุ |
|---|---|---|---|
| PMS5003 ตัวที่ 1 | VCC / GND | 5V (VIN) / GND | พัดลมต้องใช้ 5V |
| | TX | GPIO 16 | สัญญาณ 3.3V ต่อตรงได้ |
| | RX | GPIO 17 | ไม่ต่อก็ได้ |
| PMS5003 ตัวที่ 2 | TX / RX | GPIO 25 / GPIO 26 | VCC 5V, GND ร่วม |
| ENS160 | VIN / GND | 3.3V / GND | |
| | SDA / SCL | GPIO 21 / GPIO 22 | ใช้บัสร่วมกับ BME280 |
| BME280 | VIN / GND / SDA / SCL | 3.3V / GND / 21 / 22 | |
| MQ-7 | VCC / GND | 5V / GND | ฮีตเตอร์กินไฟราว 150 mA |
| | AO | GPIO 34 **ผ่านตัวแบ่งแรงดัน** | AO ออกได้ถึง 5V แต่ ESP32 รับได้ไม่เกิน 3.3V: AO → 10kΩ → GPIO34 → 20kΩ → GND |
| MQ135 | VCC / GND | 5V / GND | ใช้แทน ENS160 ชั่วคราว (ดัชนีก๊าซรวม: ควัน, VOC, NH₃, NOx) |
| | AO | GPIO 35 **ผ่านตัวแบ่งแรงดัน** | ต่อแบบเดียวกับ MQ-7: AO → 10kΩ → GPIO35 → 20kΩ → GND |
| SD card | CS / SCK / MISO / MOSI | GPIO 5 / 18 / 19 / 23 | โมดูลที่มีเรกูเลเตอร์ใช้ VCC 5V, โมดูลเล็กไม่มีเรกูเลเตอร์ใช้ 3.3V |

ถ้าต่อขาไว้ต่างจากนี้ แก้ตัวเลขในส่วน `ขาที่ต่อ` ด้านบนของไฟล์ .ino
ห้ามย้าย MQ-7 / MQ135 ไปขา ADC2 (เช่น GPIO 0, 2, 4, 12–15, 25–27) เพราะอ่านค่าไม่ได้ตอนเปิด Wi-Fi

## 4. ตั้งค่า config.h

1. คัดลอก `config.example.h` เป็น `config.h` ในโฟลเดอร์เดียวกัน
2. ใส่ `WIFI_SSID` / `WIFI_PASSWORD` (ESP32 ใช้ได้เฉพาะ **2.4 GHz** ถ้าใช้ฮอตสปอตมือถือ iPhone เปิด *Maximize Compatibility*, Android เลือกย่าน 2.4 GHz)
3. `DEVICE_EMAIL` / `DEVICE_PASSWORD` **เว้นว่างไว้ก่อนได้** ถ้า Database Rules ยังเปิดให้เขียน
4. ตั้งเซนเซอร์ที่มีจริง เช่นตอนนี้มีแค่ PMS5003 ให้คงไว้ทั้งหมดเป็น `0`
   ```cpp
   #define USE_ENS160  0
   #define USE_BME280  0
   #define USE_MQ7     0
   ```
5. เปิด `esp32-air-monitor.ino` ใน Arduino IDE จะเห็น `config.h` เป็นแท็บข้างๆ
6. ตรวจว่า `config.h` ไม่ขึ้นใน `git status` ก่อน commit

## 5. อัปโหลด

1. **Tools → Board → esp32 → ESP32 Dev Module** (สำหรับบอร์ด ESP32 DevKit / WROOM-32 ทั่วไป)
2. **Tools → Port** เลือก COM ของบอร์ด
3. กด **Upload (→)**
   - ถ้าขึ้น `Failed to connect to ESP32: Timed out waiting for packet header` ให้กดปุ่ม **BOOT** ค้างไว้ตอนขึ้น `Connecting....` แล้วปล่อยเมื่อเริ่มอัปโหลด
4. **Tools → Serial Monitor** ตั้ง baud **115200** แล้วกดปุ่ม EN/RST บนบอร์ดหนึ่งครั้ง

ผลที่ควรเห็น เมื่อมีแค่ PMS5003

```
=== Smart Air Quality Monitor ===
device unit-01, send every 10 s, auth none (Rules must allow writes)
[ens160] disabled (USE_ENS160 0 in config.h)
[bme280] disabled (USE_BME280 0 in config.h)
[mq7] disabled (USE_MQ7 0 in config.h)
[sd] no card (continuing without SD backup)
[wifi] connecting to "MyWiFi"..... OK  IP 192.168.1.23  RSSI -58 dBm
[14:05:10] PM2.5 23.4 (A 23.4 / B nan) PM10 31.0 | Firebase OK
```

เมื่อต่อครบทุกตัว

```
=== Smart Air Quality Monitor ===
device unit-01, send every 10 s, auth none (Rules must allow writes)
[i2c] found: 0x53 0x76
[ens160] OK at 0x53 (warm-up ~3 min before values are valid)
[bme280] OK at 0x76
[sd] OK
[wifi] connecting to "MyWiFi"..... OK  IP 192.168.1.23  RSSI -58 dBm
[14:05:10] PM2.5 23.4 (A 23.1 / B 23.7) PM10 31.0 | TVOC 120 eCO2 450 ens=1 | CO 812 | 30.1C 65% | Firebase OK SD OK
```

5. เปิด Firebase Console → Realtime Database ต้องเห็น `/sensor` เปลี่ยนทุก 10 วินาที และ `/history` เพิ่มรายการเรื่อยๆ
6. เปิด `docs/index.html` ต้องเห็นค่าจริงและกราฟขยับ

## 6. แก้ปัญหาที่พบบ่อย

| อาการใน Serial Monitor | สาเหตุ / วิธีแก้ |
|---|---|
| `config.h: No such file` หรือ `Copy config.example.h to config.h` | ยังไม่ได้สร้าง `config.h` (ข้อ 4) |
| `fatal error: SparkFun_ENS160.h` / `Adafruit_BME280.h` | ตั้ง `USE_...` เป็น 1 แต่ยังไม่ได้ลงไลบรารี (ข้อ 2) หรือยังไม่มีเซนเซอร์ให้ตั้งกลับเป็น 0 |
| `[wifi] ... FAILED` | ชื่อ/รหัส Wi-Fi ผิด หรือเป็น 5 GHz เครื่องยังบันทึกลง SD ต่อได้ |
| `[i2c] found: none` | สาย SDA/SCL สลับกัน หรือไม่ได้จ่าย 3.3V |
| `chip at 0x76 is a BMP280` | โมดูลที่ซื้อเป็น BMP280 (ไม่มีความชื้น) ต้องเปลี่ยนเป็น BME280 แท้ |
| `! PMS5003 #1: no data on RX 16` | สาย TX ของ PMS5003 ไม่ได้ต่อเข้า GPIO 16 หรือ PMS5003 ไม่ได้ไฟ 5V (ฟังเสียงพัดลม) |
| `B nan` | ยังไม่ได้ต่อ PMS5003 ตัวที่ 2 (ปกติ) |
| `ens=1` | ENS160 กำลังอุ่นเครื่อง รอ ~3 นาที (ครั้งแรกที่ใช้ ens=2 นานราว 1 ชม.) |
| `[firebase] PUT /sensor -> 401 Permission denied` | Rules ล็อกแล้วแต่ยังไม่ได้ใส่บัญชีอุปกรณ์ หรือ uid ไม่อยู่ใน `/writers` |
| `[auth] sign-in failed (400): INVALID_LOGIN_CREDENTIALS` (หรือ `EMAIL_NOT_FOUND`, `INVALID_PASSWORD`) | อีเมล/รหัสผ่านใน `config.h` ไม่ตรงกับบัญชีใน Firebase Authentication |
| `[auth] sign-in failed (400): OPERATION_NOT_ALLOWED` | ยังไม่ได้เปิด Email/Password ใน Authentication → Sign-in method |
| `Brownout detector was triggered` บอร์ดรีสตาร์ตเอง | ไฟไม่พอ (MQ-7 + PMS5003 ×2) ใช้พาวเวอร์แบงก์หรืออะแดปเตอร์ 5V 2A |

## 7. ข้อมูลที่ส่ง

| ฟิลด์ | ความหมาย |
|---|---|
| `pm1_0`, `pm2_5`, `pm10` | ค่าเฉลี่ย ATM ของ PMS5003 ทุกตัวในรอบ 10 วินาที (µg/m³) |
| `pm2_5_a`, `pm2_5_b` | PM2.5 แยกตัวที่ 1 / 2 สำหรับประเมิน inter-unit precision |
| `pm1_0_cf1`, `pm2_5_cf1`, `pm10_cf1` | ค่า CF=1 ใช้กับสูตรแก้ความชื้นของ EPA (Barkjohn) |
| `tvoc` (ppb), `eco2` (ppm), `aqi` (1–5), `ens_status` | ENS160 ใช้เฉพาะแถวที่ `ens_status = 0` ในการวิเคราะห์ |
| `co` | ค่า ADC เฉลี่ยของ MQ-7 (0–4095) ใช้เป็นดัชนีเชิงสัมพัทธ์ ไม่ใช่ ppm |
| `mq135` | ค่า ADC เฉลี่ยของ MQ135 (0–4095) ดัชนีก๊าซรวมเชิงสัมพัทธ์ |
| `temperature`, `humidity`, `pressure` | BME280 (°C, %RH, hPa) |
| `device`, `timestamp` | ชื่อเครื่อง และเวลาฝั่งเซิร์ฟเวอร์ (ms) |

ไฟล์ใน SD card ชื่อ `/YYYYMMDD.csv` (เวลาไทยจาก NTP) มีคอลัมน์เดียวกัน และคอลัมน์ `uploaded` = 0 ถ้ารอบนั้นส่ง Firebase ไม่สำเร็จ
ถ้าเปิดเครื่องโดยไม่มีเน็ตเลย จะยังไม่มีเวลาจริง ข้อมูลจะไปอยู่ที่ `/notime.csv` พร้อมเวลานับจากเปิดเครื่อง

## 8. เมื่อจะล็อก Database Rules

1. สร้างบัญชีอุปกรณ์และเพิ่ม uid ลง `/writers` ตาม `firebase/README.md`
2. ใส่ `DEVICE_EMAIL` / `DEVICE_PASSWORD` ใน `config.h` แล้วอัปโหลดใหม่
3. Serial Monitor ต้องขึ้น `auth email/password` และ `[auth] signed in`
4. จากนั้นค่อย Publish `firebase/database.rules.json`

## หมายเหตุด้านความปลอดภัย

โค้ดใช้ `client.setInsecure()` คือเข้ารหัส HTTPS แต่ไม่ตรวจใบรับรองของเซิร์ฟเวอร์ เพื่อให้ใช้ง่ายในระยะต้นแบบ
ถ้าจะใช้งานจริงระยะยาว ควรใส่ root CA ของ Google ด้วย `client.setCACert(...)` แทน
