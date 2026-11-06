# Helm Anti-Ngantung Dashboard

Dashboard React + FastAPI bertema pink yang membaca `helmet_data.db` secara read-only. ESP32 dan `camera-detection.py` tetap melakukan pemrosesan dan penulisan database.

## Alur
1. ESP32 membaca MPU6050/MAX30102, menjalankan rule-based detection, dan mengendalikan LED/buzzer/vibrator.
2. Laptop menjalankan webcam, CNN/MediaPipe, dan `camera-detection.py`.
3. Python menerima data ESP32 melalui Serial, memproses hasil kamera, lalu menyimpan ke SQLite.
4. FastAPI membaca SQLite melalui endpoint GET.
5. React polling API setiap 1,5 detik dan menampilkan metrik, grafik, serta riwayat.
6. Jika kolom status deteksi akhir berisi `INDIKASI MENGANTUK`, `DROWSY`, atau `SLEEPY`, UI berubah merah berkedip.

## Backend
```bash
cd backend
python -m venv .venv
# Windows PowerShell: .venv\\Scripts\\Activate.ps1
# Linux/macOS: source .venv/bin/activate
pip install -r requirements.txt
```
Buat `backend/.env` dari `.env.example`, lalu atur `DB_PATH` ke lokasi absolut file `helmet_data.db` milik project.
```bash
python run.py
```
API: `http://localhost:8000/docs`, health: `/api/health`, schema: `/api/schema`.

## Frontend
Terminal kedua:
```bash
cd frontend
npm install
npm run dev
```
Buka `http://localhost:5173`. Jika API bukan localhost:8000, buat `frontend/.env` dari `.env.example` dan atur `VITE_API_URL`.

## Integrasi SQLite
- Jalankan `camera-detection.py` seperti biasa.
- Jalankan FastAPI dan React di terminal terpisah.
- Pastikan proses Python melakukan `conn.commit()` setelah INSERT/UPDATE.
- FastAPI hanya membaca database; jangan buka port COM dari FastAPI/React. COM tetap dimiliki proses Python.
- Pertimbangkan mengaktifkan WAL sekali pada database: `PRAGMA journal_mode=WAL;`. Pertahankan satu proses writer utama.

## Penting tentang schema
API mencari tabel `esp32_sensor_data` dan `eye_detection`, lalu memetakan beberapa nama kolom umum. Screenshot database menunjukkan nama-nama seperti `head_angle`, `heart_rate`, `nods_7s`, `drowsiness_status`, `led_status`, dan `raw_message`, tetapi nama kolom lengkap perlu diverifikasi di `/api/schema`. Jika metrik atau status tampil `--`/`UNKNOWN`, sesuaikan daftar alias di `backend/app/main.py` dengan schema aktual.

Status dashboard harus berasal dari keputusan akhir sistem, bukan hanya eye detection, jika logika mengantuk memang menggabungkan kamera dan sensor. Output buzzer/vibrator hanya muncul jika statusnya disimpan ke SQLite; jika belum, dashboard akan menampilkan UNKNOWN.

Backend ini ditujukan untuk prototipe lokal. Sebelum dibuka ke internet, tambahkan autentikasi, HTTPS, dan pembatasan akses. Efek berkedip hanya peringatan visual tambahan, bukan pengganti alarm perangkat/prosedur keselamatan.
