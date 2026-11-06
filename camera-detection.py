
import cv2
import numpy as np
import tensorflow as tf
import mediapipe as mp
import time
import sqlite3
from datetime import datetime
from PIL import Image
import serial
import threading
import re

# ESP32 SERIAL CONFIGURATION
SERIAL_PORT = "COM11"  # Ganti sesuai port ESP32 di komputer
SERIAL_BAUD = 115200

esp32 = serial.Serial(
    SERIAL_PORT,
    SERIAL_BAUD,
    timeout=0.1
)

time.sleep(2)  # Tunggu ESP32 selesai restart

last_serial_send = 0.0
SERIAL_SEND_INTERVAL = 0.1  # Kirim setiap 100 ms

# ==========================================
# CONFIGURATION
# ==========================================
MODEL_PATH = "Drowsiness_Detection_model_CNN.keras"
DB_PATH = "helmet_data.db"

IMG_SIZE = 64
CAMERA_INDEX = 1

CLOSED_THRESHOLD = 1.0
CONFIDENCE_THRESHOLD = 0.70

DB_SAVE_INTERVAL = 1.0  # Simpan setiap 1 detik

# Training notebook:
# 0 = Opened
# 1 = Closed

EYE_LANDMARKS = [
    [33, 133, 160, 159, 158, 144, 145, 153],
    [362, 263, 387, 386, 385, 373, 374, 380]
]

EYE_NAMES = ["Left Eye Input", "Right Eye Input"]


def read_esp32_serial():
    while True:
        try:
            if esp32.in_waiting > 0:
                message = esp32.readline().decode(
                    "utf-8", errors="replace"
                ).strip()

                if message:
                    print(f"[ESP32] {message}")
                    save_esp32_record(message)

        except (serial.SerialException, OSError) as e:
            print(f"Serial read error: {e}")
            break


# ==========================================
# SQLITE DATABASE
# ==========================================
def init_database():
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS eye_detection (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                eye_status TEXT NOT NULL,
                opened_prob REAL NOT NULL,
                closed_prob REAL NOT NULL,
                closed_duration REAL NOT NULL,
                alert_triggered INTEGER NOT NULL
            )
        """)
        
        # Tabel kedua: data sensor dan keputusan yang dicetak ESP32.
        conn.execute("""
            CREATE TABLE IF NOT EXISTS esp32_sensor_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                head_angle REAL,
                heart_rate_bpm REAL,
                heart_rate_status TEXT,
                nods_7s INTEGER,
                drowsiness_status TEXT,
                raw_message TEXT NOT NULL
            )
        """)

def save_detection(
    eye_status,
    opened_prob,
    closed_prob,
    closed_duration,
    alert_triggered
):
    with sqlite3.connect(DB_PATH, timeout=5) as conn:
        conn.execute("""
            INSERT INTO eye_detection (
                timestamp,
                eye_status,
                opened_prob,
                closed_prob,
                closed_duration,
                alert_triggered
            )
            VALUES (?, ?, ?, ?, ?, ?)
        """, (
            datetime.now().isoformat(timespec="milliseconds"),
            eye_status,
            opened_prob,
            closed_prob,
            closed_duration,
            int(alert_triggered)
        ))


def save_esp32_record(message):
    """Simpan data sensor ESP32 ke SQLite."""
    if "Sudut:" not in message or "Nods/7s:" not in message:
        return

    try:
        parts = [part.strip() for part in message.split("|")]

        timestamp = parts[0] if parts else datetime.now().isoformat()

        angle_match = re.search(
            r"Sudut:\s*(-?\d+(?:\.\d+)?)\s*deg",
            message
        )
        nods_match = re.search(
            r"Nods/7s:\s*(\d+)",
            message
        )
        hr_match = re.search(
            r"HR:\s*([0-9]+(?:\.\d+)?)\s*BPM",
            message
        )
        status_match = re.search(
            r"Status:\s*([^|]+)",
            message
        )


        # Tampilkan pesan yang tidak cocok agar formatnya bisa diperiksa.
        if not all([angle_match, nods_match, status_match]):
            print(f"[ESP32 PARSE SKIPPED] Format tidak cocok: {message}")
            return

        if hr_match:
            hr_bpm = float(hr_match.group(1))
            hr_status = "VALID"
        elif "Jari tidak terdeteksi" in message:
            hr_bpm = None
            hr_status = "NO_FINGER"
        elif "Mengukur" in message:
            hr_bpm = None
            hr_status = "MEASURING"
        else:
            hr_bpm = None
            hr_status = "UNAVAILABLE"

        with sqlite3.connect(DB_PATH, timeout=5) as conn:
            conn.execute("""
                INSERT INTO esp32_sensor_data (
                    timestamp, head_angle, heart_rate_bpm,
                    heart_rate_status, nods_7s,
                    drowsiness_status, raw_message
                ) VALUES (?, ?, ?, ?, ?, ?, ?)
            """, (
                timestamp,
                float(angle_match.group(1)),
                hr_bpm,
                hr_status,
                int(nods_match.group(1)),
                status_match.group(1).strip(),
                message
            ))

        print(
            f"[DATABASE SAVED - ESP32] "
            f"Sudut={angle_match.group(1)} | "
            f"HR={hr_bpm if hr_bpm is not None else hr_status} | "
            f"Nods={nods_match.group(1)} | "
            f"Status={status_match.group(1).strip()}"
        )

    except (sqlite3.Error, ValueError) as e:
        print(f"[DATABASE ESP32 ERROR] {e}")


init_database()
print(f"SQLite database ready: {DB_PATH}")

serial_thread = threading.Thread(
    target=read_esp32_serial,
    daemon=True
)
serial_thread.start()

# ==========================================
# LOAD MODEL
# ==========================================
print("Loading model...")

model = tf.keras.models.load_model(MODEL_PATH)

print("Model loaded.")
print("Input shape:", model.input_shape)
print("Output shape:", model.output_shape)
print("Class mapping: 0 = Opened, 1 = Closed")

# ==========================================
# OPEN CAMERA
# ==========================================
cap = cv2.VideoCapture(CAMERA_INDEX)

if not cap.isOpened():
    raise RuntimeError(
        "Camera tidak dapat dibuka. "
        "Coba ubah CAMERA_INDEX menjadi 1."
    )

mp_face_mesh = mp.solutions.face_mesh
closed_start = None
last_db_save = 0.0

# ==========================================
# CROP EYE
# ==========================================
def crop_eye(frame, landmarks, indices):
    h, w = frame.shape[:2]

    points = np.array([
        [
            int(landmarks[i].x * w),
            int(landmarks[i].y * h)
        ]
        for i in indices
    ])

    x1, y1 = points.min(axis=0)
    x2, y2 = points.max(axis=0)

    eye_w = x2 - x1
    eye_h = y2 - y1

    if eye_w <= 0 or eye_h <= 0:
        return None

    pad_x = max(5, int(eye_w * 0.45))
    pad_y = max(5, int(eye_h * 1.0))

    x1 = max(0, x1 - pad_x)
    y1 = max(0, y1 - pad_y)
    x2 = min(w, x2 + pad_x)
    y2 = min(h, y2 + pad_y)

    if x2 <= x1 or y2 <= y1:
        return None

    return frame[y1:y2, x1:x2]

# ==========================================
# PREPROCESSING
# ==========================================
def preprocess_eye(eye):
    eye_rgb = cv2.cvtColor(eye, cv2.COLOR_BGR2RGB)

    pil_image = Image.fromarray(eye_rgb)

    pil_image = pil_image.resize(
        (IMG_SIZE, IMG_SIZE),
        resample=Image.Resampling.BOX
    )

    eye_resized = np.array(pil_image, dtype=np.uint8)

    x = eye_resized.astype(np.float32) / 255.0
    x = np.expand_dims(x, axis=0)

    return eye_resized, x

# ==========================================
# MAIN LOOP
# ==========================================
try:
    with mp_face_mesh.FaceMesh(
        max_num_faces=1,
        refine_landmarks=True,
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5
    ) as face_mesh:

        while True:
            ret, frame = cap.read()

            if not ret:
                print("Tidak dapat membaca kamera.")
                break

            frame = cv2.flip(frame, 1)

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            result = face_mesh.process(rgb)

            eye_predictions = []
            current_time = time.monotonic()
            alert_for_esp32 = 0
            
            

            if result.multi_face_landmarks:
                landmarks = (
                    result.multi_face_landmarks[0].landmark
                )

                for eye_idx, eye_indices in enumerate(
                    EYE_LANDMARKS
                ):
                    eye = crop_eye(
                        frame,
                        landmarks,
                        eye_indices
                    )

                    if eye is None or eye.size == 0:
                        continue

                    eye_resized, x = preprocess_eye(eye)

                    cv2.imshow(
                        EYE_NAMES[eye_idx],
                        cv2.cvtColor(
                            eye_resized,
                            cv2.COLOR_RGB2BGR
                        )
                    )

                    probs = model.predict(x, verbose=0)[0]

                    opened_eye_prob = float(probs[0])
                    closed_eye_prob = float(probs[1])

                    eye_predictions.append(probs)

                    cv2.putText(
                        frame,
                        f"Eye {eye_idx + 1}: "
                        f"O {opened_eye_prob:.0%} "
                        f"C {closed_eye_prob:.0%}",
                        (20, 215 + eye_idx * 30),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.6,
                        (255, 255, 0),
                        2
                    )

                # ==================================
                # COMBINE BOTH EYES
                # ==================================
                if len(eye_predictions) == 2:
                    avg_probs = np.mean(eye_predictions, axis=0)

                    opened_prob = float(avg_probs[0])
                    closed_prob = float(avg_probs[1])

                    if closed_prob >= CONFIDENCE_THRESHOLD:
                        label = "Closed"
                    elif opened_prob >= CONFIDENCE_THRESHOLD:
                        label = "Opened"
                    else:
                        label = "Closed"  # Uncertain dianggap Closed

                    # ==================================
                    # CLOSED DURATION
                    # ==================================
                    if label == "Closed":
                        if closed_start is None:
                            closed_start = current_time

                        duration = current_time - closed_start
                    else:
                        closed_start = None
                        duration = 0.0

                    # ==================================
                    # STATUS
                    # ==================================
                    alert_triggered = (
                        duration >= CLOSED_THRESHOLD
                    )
                    
                    alert_for_esp32 = int(alert_triggered)

                    if alert_triggered:
                        status = "WARNING: EYES CLOSED TOO LONG"
                        color = (0, 0, 255)
                    elif label == "Closed":
                        status = "Eyes Closed"
                        color = (0, 165, 255)
                    elif label == "Opened":
                        status = "Eyes Opened"
                        color = (0, 255, 0)
                    else:
                        status = "Uncertain"
                        color = (0, 255, 255)

                    # ==================================
                    # SAVE TO SQLITE EVERY 1 SECOND
                    # ==================================
                    if (
                        current_time - last_db_save
                        >= DB_SAVE_INTERVAL
                    ):
                        try:
                            save_detection(
                                eye_status=label,
                                opened_prob=opened_prob,
                                closed_prob=closed_prob,
                                closed_duration=duration,
                                alert_triggered=alert_triggered
                            )

                            last_db_save = current_time

                            print(
                                f"[DATABASE SAVED] "
                                f"Status={label} | "
                                f"Opened={opened_prob:.3f} | "
                                f"Closed={closed_prob:.3f} | "
                                f"Duration={duration:.1f}s | "
                                f"Alert={int(alert_triggered)}"
                            )

                        except sqlite3.Error as e:
                            print(f"Database error: {e}")

                    # ==================================
                    # DISPLAY RESULTS
                    # ==================================
                    cv2.putText(
                        frame,
                        f"Prediction: {label}",
                        (20, 35),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.8,
                        color,
                        2
                    )

                    cv2.putText(
                        frame,
                        f"Opened: {opened_prob:.1%}",
                        (20, 70),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.7,
                        (255, 255, 0),
                        2
                    )

                    cv2.putText(
                        frame,
                        f"Closed: {closed_prob:.1%}",
                        (20, 105),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.7,
                        (255, 255, 0),
                        2
                    )

                    cv2.putText(
                        frame,
                        f"Closed duration: {duration:.1f}s",
                        (20, 140),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.65,
                        (255, 255, 255),
                        2
                    )

                    cv2.putText(
                        frame,
                        status,
                        (20, 175),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.65,
                        color,
                        2
                    )

                else:
                    closed_start = None

                    cv2.putText(
                        frame,
                        "Both eyes not detected",
                        (20, 35),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.7,
                        (0, 0, 255),
                        2
                    )

            else:
                closed_start = None

                cv2.putText(
                    frame,
                    "Face not detected",
                    (20, 35),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.7,
                    (0, 0, 255),
                    2
                )

            # ==========================================
            # SEND EYE ALERT TO ESP32
            # ==========================================
            if current_time - last_serial_send >= SERIAL_SEND_INTERVAL:
                timestamp = datetime.now().isoformat(
                    timespec="milliseconds"
                )

                message = f"EYE,{alert_for_esp32},{timestamp}\n"

                try:
                    esp32.write(message.encode("utf-8"))

                    print(
                        f"[ESP32 TX] Alert={alert_for_esp32} | "
                        f"Timestamp={timestamp}"
                    )

                    last_serial_send = current_time

                except serial.SerialException as e:
                    print(f"Serial communication error: {e}")

            cv2.imshow("Eye Detection", frame)

            key = cv2.waitKey(1) & 0xFF

            if key == ord("c"):
                esp32.write(b"CALIBRATE\n")
                print("[ESP32 TX] Perintah kalibrasi dikirim.")

            elif key == ord("q"):
                break

finally:
    cap.release()
    cv2.destroyAllWindows()

    if esp32.is_open:
        esp32.close()

    print(f"Database saved at: {DB_PATH}")
