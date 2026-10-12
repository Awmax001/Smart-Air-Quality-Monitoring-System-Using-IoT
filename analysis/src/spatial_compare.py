"""
เปรียบเทียบวิธีประมาณค่าเชิงพื้นที่ (IDW, Ordinary Kriging, Random Forest)
สำหรับแผนที่ฝุ่นจากการตรวจวัดแบบเคลื่อนที่

อินพุต: grid_cells.csv  (หนึ่งแถว = หนึ่งช่องกริด ~100 ม. ที่มีการวัดจริง)
  คอลัมน์ที่ต้องมี:
    lat, lon       จุดกึ่งกลางของช่องกริด
    ratio          ค่ามัธยฐานของ (PM2.5 เครื่องเรา / PM2.5 สถานีในชั่วโมงเดียวกัน)
  คอลัมน์เสริม (ใช้กับ Random Forest ถ้ามี):
    dist_road_m    ระยะถึงถนนสายหลัก (ม.)  เช่น คำนวณจาก OpenStreetMap
    building_frac  สัดส่วนพื้นที่อาคารในรัศมี 100 ม.
    ...            ตัวแปรเชิงพื้นที่อื่นๆ ที่ขึ้นต้นด้วย "cov_" หรือระบุใน COVARIATES

ติดตั้ง:  pip install pandas numpy scikit-learn pykrige pyproj
รัน:     python spatial_compare.py grid_cells.csv
"""
import sys
import numpy as np
import pandas as pd
from pyproj import Transformer
from pykrige.ok import OrdinaryKriging
from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import GroupKFold

COVARIATES = ["dist_road_m", "building_frac"]   # แก้ตามคอลัมน์ที่มีจริง
BLOCK_SIZE_M = 500      # ขนาดบล็อกสำหรับ spatial cross-validation
N_FOLDS = 5
IDW_POWER = 2
IDW_K = 8               # ใช้เพื่อนบ้านใกล้สุดกี่จุด


def to_utm(lat, lon):
    """แปลงพิกัดเป็นเมตร (UTM zone 48N ครอบคลุมขอนแก่น) เพื่อคำนวณระยะได้ถูกต้อง"""
    tr = Transformer.from_crs("EPSG:4326", "EPSG:32648", always_xy=True)
    x, y = tr.transform(lon, lat)
    return np.asarray(x), np.asarray(y)


# ---------- วิธีที่ 0: baseline = สมมติว่าค่าสถานีใช้แทนได้ทุกจุด (ratio = 1) ----------
def predict_station(train, test):
    return np.ones(len(test))


# ---------- วิธีที่ 1: IDW ----------
def predict_idw(train, test, power=IDW_POWER, k=IDW_K):
    preds = []
    for tx, ty in zip(test.x, test.y):
        d = np.hypot(train.x.values - tx, train.y.values - ty)
        idx = np.argsort(d)[:k]
        d_k = np.maximum(d[idx], 1.0)          # กันหารด้วยศูนย์
        w = 1.0 / d_k ** power
        preds.append(np.sum(w * train.ratio.values[idx]) / np.sum(w))
    return np.array(preds)


# ---------- วิธีที่ 2: Ordinary Kriging ----------
def predict_kriging(train, test, variogram="spherical"):
    ok = OrdinaryKriging(train.x.values, train.y.values, train.ratio.values,
                         variogram_model=variogram, verbose=False, enable_plotting=False)
    z, var = ok.execute("points", test.x.values, test.y.values)
    return np.asarray(z)   # var = ความไม่แน่นอน ใช้ทำแผนที่ความเชื่อมั่นได้


# ---------- วิธีที่ 3: Random Forest ----------
def predict_rf(train, test, covs):
    feats = ["x", "y"] + covs
    rf = RandomForestRegressor(n_estimators=300, min_samples_leaf=3, random_state=42)
    rf.fit(train[feats], train.ratio)
    return rf.predict(test[feats])


def metrics(y, p):
    err = p - y
    rmse = np.sqrt(np.mean(err ** 2))
    mae = np.mean(np.abs(err))
    ss_res = np.sum(err ** 2)
    ss_tot = np.sum((y - y.mean()) ** 2)
    r2 = 1 - ss_res / ss_tot if ss_tot > 0 else np.nan
    return rmse, mae, r2


def main(path):
    df = pd.read_csv(path).dropna(subset=["lat", "lon", "ratio"])
    df["x"], df["y"] = to_utm(df.lat.values, df.lon.values)
    covs = [c for c in COVARIATES if c in df.columns] + \
           [c for c in df.columns if c.startswith("cov_")]

    # Spatial block CV: แบ่งพื้นที่เป็นบล็อก แล้วกันทั้งบล็อกออกเป็นชุดทดสอบ
    # (ห้ามสุ่มทีละช่อง เพราะช่องติดกันมีค่าใกล้กัน ทำให้ผลดูดีเกินจริง)
    df["block"] = (df.x // BLOCK_SIZE_M).astype(int).astype(str) + "_" + \
                  (df.y // BLOCK_SIZE_M).astype(int).astype(str)
    n_blocks = df.block.nunique()
    folds = min(N_FOLDS, n_blocks)
    print(f"ช่องกริด {len(df)} ช่อง, {n_blocks} บล็อก, {folds} folds, covariates={covs or 'ไม่มี'}")

    methods = {
        "Station (ratio=1)": lambda tr, te: predict_station(tr, te),
        "IDW": lambda tr, te: predict_idw(tr, te),
        "Ordinary Kriging": lambda tr, te: predict_kriging(tr, te),
        "Random Forest (x,y)": lambda tr, te: predict_rf(tr, te, []),
    }
    if covs:
        methods["Random Forest (x,y+covariates)"] = lambda tr, te: predict_rf(tr, te, covs)

    preds = {m: np.full(len(df), np.nan) for m in methods}
    for train_idx, test_idx in GroupKFold(n_splits=folds).split(df, groups=df.block):
        tr, te = df.iloc[train_idx], df.iloc[test_idx]
        for name, fn in methods.items():
            try:
                preds[name][test_idx] = fn(tr, te)
            except Exception as e:      # เช่น kriging ฟิต variogram ไม่ได้เมื่อข้อมูลน้อย
                print(f"  [{name}] fold ล้มเหลว: {e}")

    rows = []
    for name, p in preds.items():
        ok = ~np.isnan(p)
        rmse, mae, r2 = metrics(df.ratio.values[ok], p[ok])
        rows.append({"method": name, "RMSE": rmse, "MAE": mae, "R2": r2})
    res = pd.DataFrame(rows).sort_values("RMSE")
    print(res.to_string(index=False, float_format=lambda v: f"{v:.3f}"))
    res.to_csv("spatial_cv_results.csv", index=False)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        main(sys.argv[1])
    else:
        # ข้อมูลจำลองไว้ทดสอบว่าโค้ดรันได้ (ไม่ใช่ข้อมูลจริง)
        rng = np.random.default_rng(0)
        n = 250
        lat = 16.43 + rng.uniform(-0.03, 0.03, n)
        lon = 102.83 + rng.uniform(-0.03, 0.03, n)
        dist_road = rng.uniform(0, 800, n)
        ratio = 1.0 + 0.4 * np.exp(-dist_road / 200) + rng.normal(0, 0.08, n)
        pd.DataFrame({"lat": lat, "lon": lon, "ratio": ratio,
                      "dist_road_m": dist_road}).to_csv("demo_grid_cells.csv", index=False)
        print("ไม่ได้ระบุไฟล์ ใช้ข้อมูลจำลอง demo_grid_cells.csv")
        main("demo_grid_cells.csv")
