// คัดลอกไฟล์นี้เป็น config.h ในโฟลเดอร์เดียวกัน แล้วใส่ค่าจริง
// config.h อยู่ใน .gitignore จึงไม่ถูก push ขึ้น GitHub (อย่าใส่รหัส Wi-Fi ใน .ino โดยตรง)
#pragma once

// ---------- Wi-Fi (ESP32 ใช้ได้เฉพาะ 2.4 GHz) ----------
#define WIFI_SSID       "your-wifi-name"
#define WIFI_PASSWORD   "your-wifi-password"

// ---------- Firebase Realtime Database ----------
#define API_KEY         "AIzaSyBWNlV5O1PcqfnjyJk3RiMryhtGAVYal_I"
#define DATABASE_URL    "https://air-quality-project-7904c-default-rtdb.asia-southeast1.firebasedatabase.app"

// บัญชีของอุปกรณ์ใน Firebase Authentication
// - เว้นว่าง "" ไว้ก่อนได้ ถ้า Database Rules ยังเปิดให้เขียน (ส่งข้อมูลโดยไม่ล็อกอิน)
// - ก่อนเปลี่ยนไปใช้ firebase/database.rules.json ต้องใส่บัญชีนี้และเพิ่ม uid ลง /writers
// - รหัสผ่านห้ามมีเครื่องหมายอัญประกาศ (") หรือแบ็กสแลช (\) อยู่ข้างใน
#define DEVICE_EMAIL    ""
#define DEVICE_PASSWORD ""

// ---------- เซนเซอร์ที่ติดตั้ง (1 = มี, 0 = ยังไม่มี) ----------
// เปลี่ยนเป็น 1 หลังต่อสายและลงไลบรารีของตัวนั้นแล้ว (ดู README ข้อ 2)
// PMS5003 ตัวที่ 1–2 และ SD card ไม่ต้องตั้ง โปรแกรมตรวจหาเอง
#define USE_ENS160      0   // ต้องลงไลบรารี "SparkFun Indoor Air Quality Sensor - ENS160"
#define USE_BME280      0   // ต้องลงไลบรารี "Adafruit BME280 Library"
#define USE_MQ7         0   // ไม่ต้องลงไลบรารี (ถ้าเปิดโดยไม่ได้ต่อ จะได้ค่ามั่ว)

// ---------- อุปกรณ์ ----------
#define DEVICE_ID        "unit-01"   // ชื่อเครื่อง ใช้แยกข้อมูลเมื่อมีหลายเครื่อง
#define SEND_INTERVAL_MS 10000       // ส่งทุก 10 วินาที
