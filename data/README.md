# Data

`data/raw/` และ `data/processed/` ถูก ignore ไว้ใน git เพราะไฟล์ใหญ่และสร้างใหม่ได้จากสคริปต์
เก็บไฟล์จริงไว้ใน Google Drive ของกลุ่ม แล้วใส่ลิงก์ไว้ท้ายไฟล์นี้ ส่วน `data/sample/` เก็บตัวอย่างเล็กๆ ที่ commit ได้

## ชุดข้อมูล

### A · ข้อมูลภายนอก

| รหัส | ข้อมูล | ความถี่ | แหล่ง / วิธีได้มา | ไฟล์ใน `data/raw/` |
|---|---|---|---|---|
| A1 | PM2.5 สถานี Air4Thai ขอนแก่น | รายชั่วโมง | ดาวน์โหลดจากเว็บ Air4Thai ครั้งละ 1 เดือน | `air4thai/<stationID>/*.csv` |
| A2 | PM2.5, O₃, NO₂, CO, SO₂ สถานี Air4Thai มหาสารคาม | รายชั่วโมง | ดาวน์โหลดจากเว็บ Air4Thai ครั้งละ 1 เดือน | `air4thai/<stationID>/*.csv` |
| A3 | อุณหภูมิ ความชื้น ลม ฝน ความกดอากาศ (ERA5) | รายชั่วโมง | `scripts/fetch_open_meteo.py` | `open_meteo_<name>_<start>_<end>.csv` |
| A4 | CO, NO₂, SO₂, O₃ จากแบบจำลอง CAMS (~45 กม.) | รายชั่วโมง | `scripts/fetch_open_meteo.py` (คอลัมน์ขึ้นต้น `cams_`) | ไฟล์เดียวกับ A3 |
| A5 | จุดความร้อน NASA FIRMS ภาคอีสาน | ต่อการโคจร | `scripts/fetch_firms.py` | `firms_<source>_<start>_<end>.csv` |

ช่วงเวลาที่ใช้: อย่างน้อย 2 ฤดูฝุ่น (พ.ย.–เม.ย.) ล่าสุด เพื่อแบ่ง train/test ตามเวลาได้

### B · ข้อมูลจากเครื่องของโปรเจกต์

| รหัส | ข้อมูล | ลักษณะการเก็บ | แหล่ง | ไฟล์ใน `data/raw/` |
|---|---|---|---|---|
| B1 | ตำแหน่งคงที่ PM, TVOC, eCO₂, CO index, T, RH | ต่อเนื่อง ทุก 10 วินาที | Firebase `/history` หรือ SD card | `firebase_export.json`, `sd/*.csv` |
| B2 | รอบวัดเคลื่อนที่ + GPS | 12 รอบ เส้นทาง 12–18 กม. เริ่มและจบที่สถานี | เหมือน B1 (มีฟิลด์ `lat`, `lng`) | `runs/<YYYYMMDD>_<รอบ>.csv` |

บันทึกของแต่ละรอบ (วันที่ เวลาเริ่ม/จบ สภาพอากาศ เหตุการณ์ผิดปกติ) ใน `runs/run_log.csv`

## ชื่อฟิลด์จากเครื่อง (ตรงกับ ESP32 และหน้าเว็บ)

| ฟิลด์ | หน่วย | หมายเหตุ |
|---|---|---|
| `timestamp` | ms (epoch, UTC) | เวลาฝั่งเซิร์ฟเวอร์ Firebase |
| `device` | – | `unit-01`, `unit-02` |
| `pm1_0`, `pm2_5`, `pm10` | µg/m³ | ค่าดิบจาก PMS5003 ระบุว่าเป็น CF=1 หรือ ATM |
| `tvoc` | ppb | ENS160 |
| `eco2` | ppm | ENS160 (ค่าประมาณจาก VOC ไม่ใช่ CO₂ จริง) |
| `aqi` | 1–5 | AQI-UBA จาก ENS160 |
| `co` | ADC (0–4095) | MQ-7 ใช้เป็นดัชนีเชิงสัมพัทธ์ |
| `temperature`, `humidity` | °C, %RH | BME280 |
| `lat`, `lng` | องศา | GPS จากสมาร์ตโฟน |

## ไฟล์ที่ประมวลผลแล้ว (`data/processed/`)

สร้างจาก `analysis/notebooks/01_data_cleaning.ipynb` และ `03_hotspot_map.ipynb`

| ไฟล์ | คอลัมน์หลัก |
|---|---|
| `hourly_khonkaen.csv` | `time`, `station_pm25`, `mk_pm25`, `mk_o3`, `mk_no2`, `mk_co`, `mk_so2`, ตัวแปร ERA5, `cams_*` |
| `device_readings.csv` | ฟิลด์จากเครื่อง + `pm25_corr` (แก้ความชื้นแบบ EPA) |
| `grid_cells.csv` | `lat`, `lon`, `ratio`, `n` (กริด 100 ม.) |
| `forecast_results.csv` | ผล backtest ของแต่ละโมเดลและระยะพยากรณ์ |

## ข้อตกลง

- เวลาในไฟล์ที่ประมวลผลแล้วเป็นเวลาไทย (UTC+7) ไม่มี timezone suffix
- ค่าที่หายเว้นว่าง (NaN) ไม่ใช้ 0 หรือ -999
- ชื่อไฟล์ใช้ภาษาอังกฤษ ตัวพิมพ์เล็ก คั่นด้วย `_`
- `data/sample/demo_grid_cells.csv` เป็นข้อมูลจำลองสำหรับทดสอบโค้ด ไม่ใช่ผลการวัด

## ลิงก์ข้อมูลจริง

- Google Drive: _(ใส่ลิงก์)_
