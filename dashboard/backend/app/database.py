import os, sqlite3
from pathlib import Path
from dotenv import load_dotenv
load_dotenv()
DEFAULT_DB = Path(__file__).resolve().parents[3] / "helmet_data.db"
DB_PATH = Path(os.getenv("DB_PATH", str(DEFAULT_DB))).expanduser().resolve()
def connect():
    if not DB_PATH.exists():
        raise FileNotFoundError(f"Database tidak ditemukan: {DB_PATH}. Atur DB_PATH di backend/.env")
    c = sqlite3.connect(str(DB_PATH), timeout=10)
    c.row_factory = sqlite3.Row
    c.execute("PRAGMA busy_timeout=10000")
    return c
def tables(c):
    return [r[0] for r in c.execute("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")]
def columns(c, t):
    return [r[1] for r in c.execute(f'PRAGMA table_info("{t}")')]
def find_table(ts, names):
    m = {x.lower(): x for x in ts}
    return next((m[n.lower()] for n in names if n.lower() in m), None)
def rows(c, t, limit=100):
    return [dict(r) for r in c.execute(f'SELECT * FROM "{t}" ORDER BY rowid DESC LIMIT ?', (limit,))] if t else []
