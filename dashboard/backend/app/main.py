from datetime import datetime, timedelta
from collections import Counter
from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from .database import connect, tables as get_tables, columns as get_columns, find_table, rows as rows_as_dicts, DB_PATH
import os

app = FastAPI(title="Helm Anti-Ngantung API", version="1.1.0")
origins = [x.strip() for x in os.getenv("CORS_ORIGINS", "http://localhost:5173").split(",") if x.strip()]
app.add_middleware(
    CORSMiddleware, allow_origins=origins, allow_credentials=True,
    allow_methods=["GET"], allow_headers=["*"]
)

def val(row, aliases, default=None):
    keys = {str(k).lower(): k for k in row.keys()}
    for name in aliases:
        key = keys.get(name.lower())
        if key is not None and row.get(key) is not None:
            return row.get(key)
    return default

def normalize_status(value):
    if value is None:
        return "UNKNOWN"
    s = str(value).strip().upper()
    if s in {"OPENED", "OPEN", "AWAKE", "NORMAL", "TIDAK MENGANTUK"}:
        return "NORMAL"
    if s in {"DROWSY", "SLEEPY", "INDIKASI MENGANTUK", "MENGANTUK"}:
        return "DROWSY"
    if s in {"CLOSED"}:
        return "CLOSED"
    if s in {"1", "TRUE", "YES"}:
        return "DROWSY"
    return s

def load_db():
    try:
        conn = connect()
    except FileNotFoundError as exc:
        raise HTTPException(status_code=503, detail=str(exc))
    tables = get_tables(conn)
    sensor_table = find_table(tables, ["esp32_sensor_data", "sensor_data"])
    eye_table = find_table(tables, ["eye_detection", "camera_detection"])
    sensors = rows_as_dicts(conn, sensor_table, 5000)
    eyes = rows_as_dicts(conn, eye_table, 500)
    return conn, tables, sensor_table, eye_table, sensors, eyes

def sensor_status(row):
    return normalize_status(val(row, ["drowsiness_status", "status", "sleepy_status", "drowsiness", "condition"]))

def is_head_down(row):
    angle = val(row, [
        "head_angle",
        "angle",
        "sudut",
        "relative_angle"
    ])

    try:
        return abs(float(angle)) >= 25.0
    except (ValueError, TypeError):
        return False
    
def parse_dt(value):
    if not value:
        return None
    try:
        return datetime.fromisoformat(str(value).replace("Z", "+00:00")).replace(tzinfo=None)
    except (ValueError, TypeError):
        return None

def episode_events(rows):
    """Count transition into DROWSY, not every repeated sensor sample."""
    chronological = list(reversed(rows))
    events = []
    previous = None
    for row in chronological:
        status = sensor_status(row)
        timestamp = val(row, ["timestamp", "created_at", "datetime", "time"])
        dt = parse_dt(timestamp)
        if status == "DROWSY" and previous != "DROWSY" and dt:
            events.append(dt)
        previous = status
    return events

@app.get("/api/health")
def health():
    try:
        conn = connect()
        tables = get_tables(conn)
        conn.close()
        return {"ok": True, "database": str(DB_PATH), "tables": tables}
    except Exception as exc:
        return {"ok": False, "database": str(DB_PATH), "error": str(exc)}

@app.get("/api/schema")
def schema():
    conn = connect()
    try:
        return {"database": str(DB_PATH), "tables": {t: get_columns(conn, t) for t in get_tables(conn)}}
    finally:
        conn.close()

@app.get("/api/overview")
def overview():
    conn, tables, sensor_table, eye_table, sensors, eyes = load_db()
    try:
        sensor = sensors[0] if sensors else {}
        eye = eyes[0] if eyes else {}
        eye_raw = val(eye, ["eye_status", "status", "prediction", "result", "alert_triggered"])
        eye_status = normalize_status(eye_raw)
        return {
            "status": sensor_status(sensor),
            "status_raw": val(sensor, ["drowsiness_status", "status"]),
            "heart_rate": val(sensor, ["heart_rate_bpm", "heart_rate", "bpm", "hr"]),
            "heart_rate_status": val(sensor, ["heart_rate_status"]),
            "head_angle": val(sensor, ["head_angle", "angle", "sudut", "relative_angle"]),
            "nods_7s": val(sensor, ["nods_7s", "nod_count", "nods"]),
            "eye_status": eye_status,
            "eye_status_raw": eye_raw,
            "buzzer_status": val(sensor, ["buzzer_status", "buzzer"]),
            "vibrator_status": val(sensor, ["vibrator_status", "vibration_status", "vibrator"]),
            "timestamp": val(sensor, ["timestamp", "created_at", "datetime", "time"]),
            "sensor_connected": bool(sensor),
            "camera_data_available": bool(eyes),
            "database_tables": tables,
            "sensor_table": sensor_table,
            "eye_table": eye_table
        }
    finally:
        conn.close()

@app.get("/api/history")
def history(limit: int = Query(50, ge=1, le=500)):
    conn, tables, sensor_table, eye_table, sensors, eyes = load_db()
    try:
        items = []
        
        filtered_sensors = [
            row for row in sensors
            if is_head_down(row)
        ]
                
        for row in filtered_sensors[:limit]:
            items.append({
                "timestamp": val(row, ["timestamp", "created_at", "datetime", "time"]),
                "heart_rate": val(row, ["heart_rate_bpm", "heart_rate", "bpm", "hr"]),
                "head_angle": val(row, ["head_angle", "angle", "sudut", "relative_angle"]),
                "head_down": is_head_down(row),
                "nods_7s": val(row, ["nods_7s", "nod_count", "nods"]),
                "status": sensor_status(row)
            })
        return {"items": items, "count": len(items), "table": sensor_table}
    finally:
        conn.close()

@app.get("/api/analytics")
def analytics():
    conn, tables, sensor_table, eye_table, sensors, eyes = load_db()
    try:
        now = datetime.now()
        start_day = (now.date() - timedelta(days=6))
        daily = Counter()
        hourly = Counter()
        events = episode_events(sensors)
        for dt in events:
            if start_day <= dt.date() <= now.date():
                daily[dt.strftime("%Y-%m-%d")] += 1
            if now - timedelta(hours=24) <= dt <= now:
                hourly[dt.strftime("%Y-%m-%d %H:00")] += 1

        days = []
        for offset in range(6, -1, -1):
            day = now.date() - timedelta(days=offset)
            days.append({
                "date": day.strftime("%Y-%m-%d"),
                "label": day.strftime("%a"),
                "count": daily[day.strftime("%Y-%m-%d")]
            })

        hours = []
        for offset in range(23, -1, -1):
            hour = (now.replace(minute=0, second=0, microsecond=0) - timedelta(hours=offset))
            key = hour.strftime("%Y-%m-%d %H:00")
            hours.append({
                "hour": hour.strftime("%H:%M"),
                "label": hour.strftime("%d/%m %H:%M"),
                "count": hourly[key]
            })

        return {
            "weekly": days,
            "hourly": hours,
            "weekly_total": sum(x["count"] for x in days),
            "hourly_total": sum(x["count"] for x in hours),
            "counting_method": "Jumlah episode: transisi status NORMAL/selain MENGANTUK menjadi MENGANTUK, bukan jumlah baris sensor.",
            "source_table": sensor_table
        }
    finally:
        conn.close()
