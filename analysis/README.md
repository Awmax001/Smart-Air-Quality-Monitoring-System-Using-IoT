# Analysis

```bash
pip install -r analysis/requirements.txt
jupyter lab analysis/notebooks
```

| Notebook | หลักฐาน | อินพุต → ผลลัพธ์ |
|---|---|---|
| `01_data_cleaning` | ประเมินเซนเซอร์ | `data/raw/*` → `hourly_khonkaen.csv`, `device_readings.csv` |
| `02_pm25_forecast` | เมื่อไร | `hourly_khonkaen.csv` → `forecast_results.csv` (MAE, RMSE, hit rate, false alarm) |
| `03_hotspot_map` | ที่ไหน | `device_readings.csv` + สถานี → `grid_cells.csv` → `src/spatial_compare.py` |
| `04_source_signature` | เพราะอะไร | `device_readings.csv` + FIRMS → กลุ่มเหตุการณ์ + จุดความร้อนต้นลม |

Notebook เป็นโครงเริ่มต้นที่รันได้กับข้อมูลตามรูปแบบใน `data/README.md` แก้ `COLMAP` และรหัสสถานีใน 01 ให้ตรงกับไฟล์จริงก่อน

ทดสอบการเปรียบเทียบวิธีประมาณค่าเชิงพื้นที่ด้วยข้อมูลจำลอง:

```bash
python analysis/src/spatial_compare.py data/sample/demo_grid_cells.csv
```

ล้าง output ของ notebook ก่อน commit (Kernel > Restart & Clear Output) เพื่อให้ diff อ่านง่ายและไฟล์ไม่ใหญ่
