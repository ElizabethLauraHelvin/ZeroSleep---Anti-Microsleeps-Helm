
import sqlite3
import pandas as pd

DB_PATH = "helmet_data.db"
EXCEL_PATH = "helmet_data.xlsx"

EYE_CSV_PATH = "eye_detection.csv"
SENSOR_CSV_PATH = "esp32_sensor_data.csv"

# READ DATA FROM SQLITE DATABASE

with sqlite3.connect(DB_PATH) as conn:

    eye_df = pd.read_sql_query(
        "SELECT * FROM eye_detection",
        conn
    )

    sensor_df = pd.read_sql_query(
        "SELECT * FROM esp32_sensor_data",
        conn
    )


# EXPORT TO EXCEL

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


# EXPORT TO CSV

eye_df.to_csv(
    EYE_CSV_PATH,
    index=False
)

sensor_df.to_csv(
    SENSOR_CSV_PATH,
    index=False
)


# DISPLAY EXPORT RESULTS

print("\n=== EXPORT DATABASE SUCCESS ===")

print("\n[EXCEL]")
print(f"File       : {EXCEL_PATH}")

print("\n[CSV - EYE DETECTION]")
print(f"File       : {EYE_CSV_PATH}")
print(f"Total Data : {len(eye_df)}")
print(f"Columns    : {list(eye_df.columns)}")

print("\n[CSV - ESP32 SENSOR DATA]")
print(f"File       : {SENSOR_CSV_PATH}")
print(f"Total Data : {len(sensor_df)}")
print(f"Columns    : {list(sensor_df.columns)}")

print("\nSemua dataset berhasil diekspor!")
