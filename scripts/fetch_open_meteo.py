#!/usr/bin/env python3
"""
ดึงข้อมูลอุตุนิยมวิทยาย้อนหลัง (ERA5) และก๊าซจากแบบจำลอง CAMS ผ่าน Open-Meteo (ฟรี ไม่ต้องใช้ key)
ใช้เป็น feature ของแบบจำลองพยากรณ์ PM2.5/AQI และใช้ตรวจความสมเหตุสมผลของค่าก๊าซ

ตัวอย่าง:
  python scripts/fetch_open_meteo.py --start 2023-01-01 --end 2025-12-31
  python scripts/fetch_open_meteo.py --lat 16.18 --lon 103.30 --name mahasarakham --start 2024-01-01 --end 2024-12-31
  python scripts/fetch_open_meteo.py --only air --start 2024-01-01 --end 2024-03-31

ผลลัพธ์: data/raw/open_meteo_<name>_<start>_<end>.csv  (รายชั่วโมง เวลาไทย)
หมายเหตุ:
  - ERA5 มีหน่วงประมาณ 5 วัน ข้อมูลล่าสุดอาจยังไม่มี
  - CAMS (ก๊าซ) มีความละเอียดราว 45 กม. ใช้เป็นภาพรวมระดับพื้นที่ ไม่ใช่ค่าจุดเดียว
  - ตัวแปรที่ใช้ได้ดูที่ https://open-meteo.com/en/docs/historical-weather-api และ /en/docs/air-quality-api
"""
import argparse
import csv
import os
import sys
from datetime import date, timedelta

import requests

WEATHER_URL = "https://archive-api.open-meteo.com/v1/archive"
AIR_URL = "https://air-quality-api.open-meteo.com/v1/air-quality"
WEATHER_VARS = "temperature_2m,relative_humidity_2m,precipitation,surface_pressure,wind_speed_10m,wind_direction_10m"
AIR_VARS = "pm2_5,pm10,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone"
CHUNK_DAYS = 92


def chunks(start, end, days=CHUNK_DAYS):
    s = start
    while s <= end:
        e = min(end, s + timedelta(days=days - 1))
        yield s, e
        s = e + timedelta(days=1)


def get_hourly(url, lat, lon, variables, start, end, prefix):
    rows = {}
    for s, e in chunks(start, end):
        r = requests.get(url, params={
            "latitude": lat, "longitude": lon, "hourly": variables,
            "start_date": s.isoformat(), "end_date": e.isoformat(), "timezone": "Asia/Bangkok",
        }, timeout=60)
        if r.status_code != 200:
            raise RuntimeError(f"{url} {s}..{e}: {r.status_code} {r.text[:200]}")
        h = r.json().get("hourly", {})
        times = h.get("time", [])
        for k, vals in h.items():
            if k == "time":
                continue
            for t, v in zip(times, vals):
                rows.setdefault(t, {})[f"{prefix}{k}"] = v
        print(f"  {prefix or 'weather_'}{s}..{e}: {len(times)} ชั่วโมง")
    return rows


def main():
    ap = argparse.ArgumentParser(description="Open-Meteo ERA5 + CAMS -> CSV")
    ap.add_argument("--lat", type=float, default=16.4419, help="ค่าเริ่มต้น = เมืองขอนแก่น")
    ap.add_argument("--lon", type=float, default=102.8360)
    ap.add_argument("--name", default="khonkaen")
    ap.add_argument("--start", required=True, help="YYYY-MM-DD")
    ap.add_argument("--end", required=True, help="YYYY-MM-DD")
    ap.add_argument("--only", choices=["weather", "air"], help="ดึงอย่างเดียว")
    ap.add_argument("--weather-vars", default=WEATHER_VARS)
    ap.add_argument("--air-vars", default=AIR_VARS)
    ap.add_argument("--out", default=None)
    a = ap.parse_args()

    start, end = date.fromisoformat(a.start), date.fromisoformat(a.end)
    merged = {}
    if a.only != "air":
        print("ERA5 weather")
        for t, d in get_hourly(WEATHER_URL, a.lat, a.lon, a.weather_vars, start, end, "").items():
            merged.setdefault(t, {}).update(d)
    if a.only != "weather":
        print("CAMS air quality")
        for t, d in get_hourly(AIR_URL, a.lat, a.lon, a.air_vars, start, end, "cams_").items():
            merged.setdefault(t, {}).update(d)

    out = a.out or os.path.join("data", "raw", f"open_meteo_{a.name}_{a.start}_{a.end}.csv")
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    cols = sorted({k for d in merged.values() for k in d})
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["time"] + cols)
        for t in sorted(merged):
            w.writerow([t] + [merged[t].get(c) for c in cols])
    print(f"บันทึก {len(merged)} แถว -> {out}")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("ผิดพลาด:", e, file=sys.stderr)
        sys.exit(1)
