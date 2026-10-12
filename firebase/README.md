# Firebase

โปรเจกต์ใช้ **Firebase Realtime Database** (`air-quality-project-7904c`, region asia-southeast1) แพ็กเกจฟรี (Spark)

## โครงสร้างข้อมูล

| Path | ใครเขียน | ลักษณะ |
|---|---|---|
| `/sensor` | ESP32 | ค่าล่าสุด เขียนทับทุกครั้ง (`pm1_0`, `pm2_5`, `pm10`, ...) |
| `/history/<pushId>` | ESP32 | ประวัติ 1 รายการต่อรอบ (10 วินาที) พร้อม `timestamp` ของเซิร์ฟเวอร์ |
| `/air4thai/<stationID>` | `scripts/air4thai_to_firebase.py` | ค่าล่าสุดของสถานี |
| `/air4thai_history/<stationID>/<YYYYMMDD_HH>` | `scripts/air4thai_to_firebase.py` | ประวัติรายชั่วโมงของสถานี |
| `/writers/<uid>` | ตั้งเองใน Console | รายชื่อบัญชีที่มีสิทธิ์เขียน (`true`) |

## ไฟล์ Rules

| ไฟล์ | ใช้เมื่อ | อ่าน | เขียน |
|---|---|---|---|
| `database.rules.dev.json` | ช่วงพัฒนา ESP32 ยังไม่ล็อกอิน | ทุกคน (เฉพาะ 4 path) | ทุกคน (เฉพาะ 4 path) |
| `database.rules.json` | ใช้งานจริง | ทุกคน (เฉพาะ 4 path) | เฉพาะบัญชีใน `/writers` |

Web API key ในหน้าเว็บไม่ใช่ความลับ ใครก็เห็นได้ ความปลอดภัยมาจาก Rules เท่านั้น

---

## ระยะที่ 1 · ตอนนี้ (5 นาที)

1. Firebase Console → **Build → Realtime Database → แท็บ Rules**
2. ถ้าเห็นแบบนี้ แปลว่าเป็น *test mode* ซึ่ง**จะหมดอายุ**และหยุดรับข้อมูลเองตามวันที่ในตัวเลข

   ```json
   ".read": "now < 1762000000000",
   ".write": "now < 1762000000000"
   ```

3. แทนที่ทั้งหมดด้วยเนื้อหาของ `database.rules.dev.json` → **Publish**
   (ยังเปิดให้เขียนได้เหมือนเดิม แต่ไม่หมดอายุ และจำกัดเฉพาะ 4 path ที่ใช้)
4. อัปโหลดโค้ด ESP32 ใหม่ แล้วดูแท็บ **Data** ต้องเห็น `/sensor` เปลี่ยน และ `/history` เพิ่มรายการทุก 10 วินาที

## ระยะที่ 2 · ล็อกการเขียน (15 นาที) ทำเมื่อ ESP32 ส่งข้อมูลได้แล้ว

ทำตามลำดับ ถ้า Publish rules ก่อนข้อ 4 ESP32 จะเขียนไม่ได้ทันที

1. **เปิดการล็อกอินแบบอีเมล**
   Build → **Authentication** → Get started → แท็บ **Sign-in method** → **Email/Password** → เปิดสวิตช์แรก (ไม่ต้องเปิด Email link) → Save
   ไม่ต้องเปิด Anonymous
2. **สร้างบัญชี 2 บัญชี** แท็บ **Users** → **Add user**
   - บัญชีอุปกรณ์ เช่น `device01@airquality.test` สำหรับ ESP32
   - บัญชีสคริปต์ เช่น `script@airquality.test` สำหรับ GitHub Actions
   - อีเมลไม่ต้องมีอยู่จริง แค่รูปแบบถูก ตั้งรหัสผ่านยาว ๆ และห้ามมี `"` หรือ `\`
   - คัดลอกค่า **User UID** ของทั้งสองบัญชีเก็บไว้
3. **เพิ่มรายชื่อผู้มีสิทธิ์เขียน** Realtime Database → แท็บ **Data** → กด `+` ที่ root
   - Key `writers` แล้วกด `+` ใต้ `writers` เพิ่มลูก 2 ตัว: Key = UID ของแต่ละบัญชี, Value = `true` (ชนิด boolean)

   ```
   writers
   ├── 7Hk2...ของ device : true
   └── Qp9x...ของ script : true
   ```

4. **ให้ ESP32 ล็อกอิน** ใส่ `DEVICE_EMAIL` / `DEVICE_PASSWORD` ใน `firmware/esp32-air-monitor/config.h` → อัปโหลดใหม่
   Serial Monitor ต้องขึ้น `auth email/password`, `[auth] signed in` และ `Firebase OK`
5. **ให้ GitHub Actions ล็อกอิน** repo บน GitHub → Settings → **Secrets and variables → Actions** → New repository secret
   - `FIREBASE_EMAIL` = อีเมลบัญชีสคริปต์
   - `FIREBASE_PASSWORD` = รหัสผ่านบัญชีสคริปต์
6. **Publish rules จริง** แท็บ Rules → แทนที่ด้วยเนื้อหาของ `database.rules.json` → Publish
7. **ทดสอบ**
   - Serial Monitor ของ ESP32 ยังขึ้น `Firebase OK`
   - หน้าเว็บ `docs/index.html` ยังเห็นค่าล่าสุด
   - GitHub → Actions → Air4Thai to Firebase → **Run workflow** ต้องเป็นเครื่องหมายถูกสีเขียว
   - ลองเขียนโดยไม่ล็อกอิน ต้องถูกปฏิเสธ (Git Bash):
     ```bash
     curl -X PUT -d '"test"' "https://air-quality-project-7904c-default-rtdb.asia-southeast1.firebasedatabase.app/sensor/test.json"
     # ต้องได้ {"error" : "Permission denied"}
     ```

รันสคริปต์ Air4Thai บนเครื่องตัวเองหลังล็อกแล้ว ให้ตั้งรหัสก่อน:

```bash
export FIREBASE_EMAIL="script@airquality.test"
export FIREBASE_PASSWORD="..."
python scripts/air4thai_to_firebase.py --station <รหัส> --once
```

ถ้ามีปัญหาหลัง Publish ให้กลับไปใช้ `database.rules.dev.json` ชั่วคราว ข้อมูลไม่หาย

---

## ดึงข้อมูลออกไปวิเคราะห์

Realtime Database → แท็บ Data → เมนู ⋮ → **Export JSON** → บันทึกเป็น `data/raw/firebase_export.json` แล้วรัน `analysis/notebooks/01_data_cleaning.ipynb`
ควร export เก็บไว้ทุกสัปดาห์ เผื่อข้อมูลบนคลาวด์ถูกลบโดยไม่ตั้งใจ

## โควตาแพ็กเกจฟรี

- พื้นที่เก็บ 1 GB: `/history` ทุก 10 วินาทีใช้ราว 3–4 MB ต่อวันต่อเครื่อง อยู่ได้หลายเดือน
- ดาวน์โหลด 10 GB ต่อเดือน: หน้าเว็บอ่านแค่ `/sensor` จึงใช้น้อยมาก
- ดูการใช้งานจริงที่ Realtime Database → แท็บ **Usage**
