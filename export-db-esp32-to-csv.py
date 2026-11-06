import sqlite3
import pandas as pd

DB_PATH = "helmet_data.db"
EXCEL_PATH = "helmet_data.xlsx"

with sqlite3.connect(DB_PATH) as conn:

    eye_df = pd.read_sql_query(
        "SELECT * FROM eye_detection",
        conn
    )

    sensor_df = pd.read_sql_query(
        "SELECT * FROM esp32_sensor_data",
        conn
    )

with pd.ExcelWriter(EXCEL_PATH, engine="openpyxl") as writer:

    eye_df.to_excel(
        writer,
        sheet_name="Eye Detection",
        index=False
    )

    sensor_df.to_excel(
        writer,
        sheet_name="ESP32 Sensor Data",
        index=False
    )

print("\n=== EXPORT DATABASE SUCCESS ===")
print(f"File       : {EXCEL_PATH}")

print(f"\nEye Detection Dataset")
print(f"Total Data : {len(eye_df)}")
print(f"Columns    : {list(eye_df.columns)}")

print(f"\nESP32 Sensor Dataset")
print(f"Total Data : {len(sensor_df)}")
print(f"Columns    : {list(sensor_df.columns)}")

print("\nSemua dataset berhasil diekspor!")