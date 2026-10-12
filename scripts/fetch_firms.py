#!/usr/bin/env python3
"""
ดึงจุดความร้อน (active fire) จาก NASA FIRMS ครอบคลุมภาคตะวันออกเฉียงเหนือ
ใช้ในหลักฐานชุด "เพราะอะไร": นับจุดความร้อนต้นลมในช่วงที่ PM2.5 สูง

1) ขอ MAP_KEY ฟรีที่ https://firms.modaps.eosdis.nasa.gov/api/map_key/
2) ตั้งค่า:  export FIRMS_MAP_KEY=xxxxxxxx   (Windows: set FIRMS_MAP_KEY=xxxxxxxx)
3) รัน:
   python scripts/fetch_firms.py --start 2025-01-01 --end 2025-04-30 --source VIIRS_SNPP_SP
   python scripts/fetch_firms.py --days 7                       # 7 วันล่าสุด (NRT)

source ที่ใช้บ่อย:
  VIIRS_SNPP_NRT, VIIRS_NOAA20_NRT, MODIS_NRT   ข้อมูลใกล้เวลาจริง (ราว 2 เดือนล่าสุด)
  VIIRS_SNPP_SP, MODIS_SP                       ข้อมูล standard processing สำหรับย้อนหลัง
API คืนข้อมูลได้ครั้งละไม่เกิน 10 วัน สคริปต์นี้แบ่งช่วงให้เอง
ผลลัพธ์: data/raw/firms_<source>_<start>_<end>.csv
"""
import argparse
import os
import sys
import time
from datetime import date, timedelta

import requests

API = "https://firms.modaps.eosdis.nasa.gov/api/area/csv/{key}/{source}/{bbox}/{days}/{date}"
ISAN_BBOX = "100.5,14.0,106.0,18.6"     # west,south,east,north ครอบคลุมภาคอีสาน
MAX_DAYS = 10


def main():
    ap = argparse.ArgumentParser(description="NASA FIRMS hotspots -> CSV")
    ap.add_argument("--key", default=os.environ.get("FIRMS_MAP_KEY"))
    ap.add_argument("--source", default="VIIRS_SNPP_NRT")
    ap.add_argument("--bbox", default=ISAN_BBOX, help="west,south,east,north")
    ap.add_argument("--start", help="YYYY-MM-DD")
    ap.add_argument("--end", help="YYYY-MM-DD")
    ap.add_argument("--days", type=int, help="ใช้แทน --start/--end เพื่อดึง N วันล่าสุด")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    if not a.key:
        ap.error("ไม่พบ MAP_KEY ตั้ง env FIRMS_MAP_KEY หรือใส่ --key")

    if a.days:
        end = date.today()
        start = end - timedelta(days=a.days - 1)
    elif a.start and a.end:
        start, end = date.fromisoformat(a.start), date.fromisoformat(a.end)
    else:
        ap.error("ใส่ --start และ --end หรือ --days")

    header, lines = None, []
    s = start
    while s <= end:
        n = min(MAX_DAYS, (end - s).days + 1)
        url = API.format(key=a.key, source=a.source, bbox=a.bbox, days=n, date=s.isoformat())
        r = requests.get(url, timeout=120)
        text = r.text.strip()
        if r.status_code != 200 or text.lower().startswith(("invalid", "error")):
            raise RuntimeError(f"{s}: {r.status_code} {text[:200]}")
        rows = text.splitlines()
        if rows:
            header = header or rows[0]
            lines += rows[1:]
        print(f"  {s} +{n} วัน: {max(0, len(rows) - 1)} จุด")
        s += timedelta(days=n)
        time.sleep(1)    # FIRMS จำกัดจำนวนคำขอต่อ 10 นาที

    out = a.out or os.path.join("data", "raw", f"firms_{a.source}_{start}_{end}.csv")
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="") as f:
        if header:
            f.write(header + "\n")
            f.write("\n".join(dict.fromkeys(lines)) + ("\n" if lines else ""))   # ตัดแถวซ้ำ
    print(f"บันทึก {len(set(lines))} จุด -> {out}")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("ผิดพลาด:", e, file=sys.stderr)
        sys.exit(1)
