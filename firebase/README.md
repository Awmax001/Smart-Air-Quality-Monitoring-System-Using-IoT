# Firebase

โปรเจกต์ใช้ **Firebase Realtime Database** (`air-quality-project-7904c`, region asia-southeast1)

## โครงสร้างข้อมูล

| Path | ใครเขียน | ลักษณะ |
|---|---|---|
| `/sensor` | ESP32 | ค่าล่าสุด เขียนทับทุกครั้ง (`pm1_0`, `pm2_5`, `pm10`, ...) |
| `/history/<pushId>` | ESP32 | ประวัติ 1 รายการต่อการวัด 1 ครั้ง ต้องมี `timestamp` (ยังไม่ได้ทำ) |
| `/air4thai/<stationID>` | `scripts/air4thai_to_firebase.py` | ค่าล่าสุดของสถานี |
| `/air4thai_history/<stationID>/<YYYYMMDD_HH>` | `scripts/air4thai_to_firebase.py` | ประวัติรายชั่วโมงของสถานี |
| `/writers/<uid>` | ตั้งเองใน Console | รายชื่อบัญชีที่มีสิทธิ์เขียน (`true`) |

## Database Rules (`database.rules.json`)

- **อ่าน**: ทุกคนอ่านได้เฉพาะ 4 path ข้างบน (หน้าเว็บต้องอ่านได้โดยไม่ล็อกอิน)
- **เขียน**: เฉพาะบัญชีที่ล็อกอินแล้ว **และ** มี uid อยู่ใน `/writers`
- Web API key ในหน้าเว็บไม่ใช่ความลับ ใครก็เห็นได้ ความปลอดภัยมาจาก Rules เท่านั้น
- Rules แบบ test mode ที่ Firebase ตั้งให้ตอนสร้างจะหมดอายุเอง (ปกติ 30 วัน) หลังจากนั้นทั้งอ่านและเขียนจะถูกปฏิเสธ

## ขั้นตอนเปิดใช้ (ทำตามลำดับ ไม่งั้น ESP32 จะเขียนไม่ได้)

1. Console > **Authentication** > Sign-in method > เปิด **Email/Password** (ไม่ต้องเปิด Anonymous)
2. Authentication > Users > **Add user** สร้าง 2 บัญชี เช่น `device@...` สำหรับ ESP32 และ `script@...` สำหรับสคริปต์ Air4Thai แล้วจด **User UID** ของแต่ละบัญชี
3. Realtime Database > Data > เพิ่ม node `writers` แล้วใส่ `<uid ของ device>: true` และ `<uid ของ script>: true`
4. แก้โค้ด ESP32 ให้ล็อกอินด้วยบัญชี device ก่อนเขียน (ดู `firmware/esp32-air-monitor/README.md`) แล้วทดสอบว่ายังเขียนได้
5. ตั้ง Secrets `FIREBASE_EMAIL` / `FIREBASE_PASSWORD` ของบัญชี script ใน GitHub
6. สุดท้ายค่อยคัดลอก `database.rules.json` ไปวางที่ Realtime Database > **Rules** แล้วกด Publish
7. เปิด `docs/index.html` ตรวจว่ายังเห็นข้อมูลจริง

> ถ้าทำข้อ 6 ก่อนข้อ 4 ESP32 จะเขียนไม่ได้ทันที แก้ได้โดยกลับไปใช้ Rules เดิมชั่วคราว
