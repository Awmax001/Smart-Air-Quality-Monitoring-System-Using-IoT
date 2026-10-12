// คัดลอกไฟล์นี้เป็น config.h แล้วใส่ค่าจริง
// config.h อยู่ใน .gitignore จึงไม่ถูก push ขึ้น GitHub (อย่าใส่รหัส Wi-Fi ใน .ino โดยตรง)
#pragma once

// ---------- Wi-Fi ----------
#define WIFI_SSID       "your-wifi-name"
#define WIFI_PASSWORD   "your-wifi-password"

// ---------- Firebase Realtime Database ----------
#define API_KEY         "AIzaSyBWNlV5O1PcqfnjyJk3RiMryhtGAVYal_I"
#define DATABASE_URL    "https://air-quality-project-7904c-default-rtdb.asia-southeast1.firebasedatabase.app"

// บัญชีของอุปกรณ์ (สร้างใน Firebase Console > Authentication แล้วเพิ่ม uid ลง /writers)
#define DEVICE_EMAIL    "device@example.com"
#define DEVICE_PASSWORD "change-me"

// ---------- อุปกรณ์ ----------
#define DEVICE_ID       "unit-01"     // ใช้แยกเครื่องเมื่อมี 2 ชุด (ทดสอบ inter-unit precision)
#define SEND_INTERVAL_MS 10000        // ส่งทุก 10 วินาที
