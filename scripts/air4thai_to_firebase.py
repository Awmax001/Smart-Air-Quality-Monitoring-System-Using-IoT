#!/usr/bin/env python3
"""
ดึงค่า PM2.5 (และก๊าซ ถ้าสถานีวัด) จาก Air4Thai แล้วเขียนลง Firebase Realtime Database
เพื่อให้หน้าเว็บ smart-air-live.html อ่านค่าสถานีได้ (เบราว์เซอร์ดึง Air4Thai ตรงๆ ไม่ได้เพราะ CORS)

เขียนลง:
  /air4thai/<stationID>          ค่าล่าสุดของสถานี (เขียนทับ)
  /air4thai_history/<stationID>  ประวัติรายชั่วโมง (เพิ่มเมื่อมีชั่วโมงใหม่)

ติดตั้ง:   pip install -r scripts/requirements.txt
ดูรหัสสถานี: python scripts/air4thai_to_firebase.py --list ขอนแก่น
รันค้างไว้:  python scripts/air4thai_to_firebase.py --station 46t
รันครั้งเดียว (ใช้กับ GitHub Actions / Task Scheduler): python scripts/air4thai_to_firebase.py --station 46t --once
ทดสอบโดยไม่เขียน Firebase: เพิ่ม --dry-run

สิทธิ์การเขียน (เมื่อใช้ firebase/database.rules.json ที่ล็อกการเขียนแล้ว):
  ตั้งตัวแปรสภาพแวดล้อม FIREBASE_EMAIL และ FIREBASE_PASSWORD ของบัญชีที่อยู่ใน /writers
  สคริปต์จะล็อกอินผ่าน Firebase Auth และขอ ID token ใหม่ทุกรอบเอง
  (บน GitHub Actions ให้เก็บสองค่านี้ไว้ใน Settings > Secrets and variables > Actions)
"""
import os
import argparse
import json
import sys
import time
from datetime import datetime, timezone, timedelta

import requests

DB_URL = "https://air-quality-project-7904c-default-rtdb.asia-southeast1.firebasedatabase.app"
API_KEY = "AIzaSyBWNlV5O1PcqfnjyJk3RiMryhtGAVYal_I"   # Web API key (ไม่ใช่ความลับ ความปลอดภัยมาจาก Database Rules)
AIR4THAI_URLS = [
    "https://air4thai.pcd.go.th/services/getNewAQI_JSON.php",
    "http://air4thai.pcd.go.th/services/getNewAQI_JSON.php",
]
TH = timezone(timedelta(hours=7))
PARAMS = {"PM25": "pm25", "PM10": "pm10", "O3": "o3", "CO": "co", "NO2": "no2", "SO2": "so2"}


def fetch_air4thai(source_file=None):
    if source_file:
        with open(source_file, encoding="utf-8") as f:
            return json.load(f)
    last_err = None
    for url in AIR4THAI_URLS:
        try:
            r = requests.get(url, timeout=30, headers={"User-Agent": "Mozilla/5.0 (student air-quality project)"})
            r.raise_for_status()
            return r.json()
        except Exception as e:  # ลอง URL ถัดไป
            last_err = e
    raise RuntimeError(f"ดึงข้อมูล Air4Thai ไม่ได้: {last_err}")


def num(v):
    try:
        x = float(v)
        return None if x < 0 else x   # Air4Thai ใช้ -1 / -999 แทนค่าที่ไม่มี
    except (TypeError, ValueError):
        return None


def parse_station(s):
    last = s.get("AQILast") or s.get("LastUpdate") or {}
    date, hhmm = last.get("date"), last.get("time")
    t = None
    if date and hhmm:
        try:
            t = datetime.strptime(f"{date} {hhmm[:5]}", "%Y-%m-%d %H:%M").replace(tzinfo=TH)
        except ValueError:
            t = None
    out = {
        "stationID": s.get("stationID"),
        "name": s.get("nameTH") or s.get("nameEN"),
        "area": s.get("areaTH") or s.get("areaEN"),
        "lat": num(s.get("lat")),
        "lng": num(s.get("long")),
        "time": t.isoformat() if t else None,
        "fetchedAt": datetime.now(TH).isoformat(timespec="seconds"),
    }
    aqi = last.get("AQI") or {}
    out["aqi"] = num(aqi.get("aqi")) if isinstance(aqi, dict) else None
    for key, name in PARAMS.items():
        p = last.get(key)
        out[name] = num(p.get("value")) if isinstance(p, dict) else None
    return out


def stations_of(data):
    if isinstance(data, dict) and "stations" in data:
        return data["stations"]
    if isinstance(data, list):
        return data
    return [data]


def sign_in(email, password):
    """ล็อกอิน Firebase Auth แบบอีเมล/รหัสผ่าน แล้วคืน ID token (อายุ 1 ชั่วโมง)"""
    r = requests.post(
        f"https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key={API_KEY}",
        json={"email": email, "password": password, "returnSecureToken": True}, timeout=30)
    if r.status_code != 200:
        raise RuntimeError(f"ล็อกอิน Firebase ไม่สำเร็จ: {r.text[:200]}")
    return r.json()["idToken"]


def fb(method, path, payload=None, auth=None):
    url = f"{DB_URL}/{path.strip('/')}.json"
    params = {"auth": auth} if auth else None
    r = requests.request(method, url, json=payload, params=params, timeout=30)
    r.raise_for_status()
    return r.json()


def run_once(args):
    if args.email and args.password and not args.dry_run and args.list is None:
        args.auth = sign_in(args.email, args.password)
    data = fetch_air4thai(args.source_file)
    sts = stations_of(data)
    if args.list is not None:
        kw = args.list
        for s in sts:
            txt = " ".join(str(s.get(k, "")) for k in ("stationID", "nameTH", "areaTH", "nameEN", "areaEN"))
            if kw.lower() in txt.lower():
                p = parse_station(s)
                print(f"{p['stationID']:>6}  {p['name']} | {p['area']} | PM2.5={p['pm25']} O3={p['o3']} CO={p['co']}")
        return
    for sid in args.station:
        s = next((x for x in sts if x.get("stationID") == sid), None)
        if not s:
            print(f"[{datetime.now(TH):%H:%M}] ไม่พบสถานี {sid} (ใช้ --list ขอนแก่น เพื่อดูรหัส)", file=sys.stderr)
            continue
        p = parse_station(s)
        print(f"[{datetime.now(TH):%H:%M}] {sid} {p['name']}: PM2.5={p['pm25']} เวลา {p['time']}")
        if args.dry_run:
            print(json.dumps(p, ensure_ascii=False, indent=2))
            continue
        prev = fb("GET", f"air4thai/{sid}", auth=args.auth) or {}
        fb("PUT", f"air4thai/{sid}", p, auth=args.auth)
        if p["time"] and prev.get("time") != p["time"]:
            key = p["time"][:13].replace("-", "").replace("T", "_")   # เช่น 20261012_14 = 1 ชั่วโมงต่อ 1 รายการ
            fb("PUT", f"air4thai_history/{sid}/{key}", p, auth=args.auth)
            print("   + เพิ่มประวัติชั่วโมงใหม่", key)


def main():
    ap = argparse.ArgumentParser(description="Air4Thai -> Firebase Realtime Database")
    ap.add_argument("--station", nargs="+", default=[], help="รหัสสถานี เช่น 46t (ใส่ได้หลายสถานี)")
    ap.add_argument("--list", nargs="?", const="", help="แสดงรายชื่อสถานีที่มีคำนี้ เช่น ขอนแก่น")
    ap.add_argument("--once", action="store_true", help="รันครั้งเดียวแล้วจบ")
    ap.add_argument("--interval", type=int, default=600, help="วินาทีระหว่างรอบ (ค่าเริ่มต้น 10 นาที)")
    ap.add_argument("--auth", default=os.environ.get("FIREBASE_AUTH"), help="ID token หรือ Database secret (หรือตั้ง env FIREBASE_AUTH)")
    ap.add_argument("--email", default=os.environ.get("FIREBASE_EMAIL"), help="อีเมลบัญชีที่มีสิทธิ์เขียน (หรือตั้ง env FIREBASE_EMAIL)")
    ap.add_argument("--password", default=os.environ.get("FIREBASE_PASSWORD"), help="รหัสผ่าน (แนะนำให้ใช้ env FIREBASE_PASSWORD แทนการพิมพ์ในคำสั่ง)")
    ap.add_argument("--dry-run", action="store_true", help="แสดงผลอย่างเดียว ไม่เขียน Firebase")
    ap.add_argument("--source-file", default=None, help="อ่าน JSON จากไฟล์แทน (ใช้ทดสอบ)")
    args = ap.parse_args()
    if args.list is None and not args.station:
        ap.error("ใส่ --station <รหัส> หรือ --list <คำค้น>")
    while True:
        try:
            run_once(args)
        except Exception as e:
            print(f"[{datetime.now(TH):%H:%M}] ผิดพลาด: {e}", file=sys.stderr)
            if args.once:
                sys.exit(1)
        if args.once or args.list is not None:
            break
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
