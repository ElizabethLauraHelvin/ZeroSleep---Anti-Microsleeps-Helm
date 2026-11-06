import { useEffect, useMemo, useState } from "react";
import {
  Area,
  AreaChart,
  Bar,
  BarChart,
  CartesianGrid,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";

const API = import.meta.env.VITE_API_URL || "http://localhost:8000";

function formatValue(value, suffix = "") {
  return value === null || value === undefined || value === ""
    ? "--"
    : `${value}${suffix}`;
}

function isOn(value) {
  return ["ON", "ACTIVE", "1", "TRUE", "YES"].includes(
    String(value ?? "").toUpperCase(),
  );
}

function StatusPill({ status }) {
  const type =
    status === "DROWSY"
      ? "danger"
      : status === "NORMAL"
        ? "success"
        : "neutral";
  const label = status === "DROWSY" ? "MENGANTUK" : status || "UNKNOWN";
  return <span className={`status-pill ${type}`}>{label}</span>;
}

function MetricCard({ label, value, unit }) {
  return (
    <article className="metric-card">
      <div className="metric-text">
        <p>{label}</p>
        <strong>{formatValue(value, unit)}</strong>
      </div>
    </article>
  );
}

function App() {
  const [overview, setOverview] = useState(null);
  const [history, setHistory] = useState([]);
  const [analytics, setAnalytics] = useState({
    weekly: [],
    hourly: [],
    weekly_total: 0,
  });
  const [database, setDatabase] = useState(null);
  const [error, setError] = useState("");
  const [updatedAt, setUpdatedAt] = useState(null);
  const [loading, setLoading] = useState(false);

  async function refreshDashboard() {
    setLoading(true);
    try {
      const [
        overviewResponse,
        historyResponse,
        analyticsResponse,
        healthResponse,
      ] = await Promise.all([
        fetch(`${API}/api/overview`),
        fetch(`${API}/api/history?limit=500`),
        fetch(`${API}/api/analytics`),
        fetch(`${API}/api/health`),
      ]);

      for (const response of [
        overviewResponse,
        historyResponse,
        analyticsResponse,
      ]) {
        if (!response.ok) throw new Error(`API error: ${response.status}`);
      }

      const [overviewData, historyData, analyticsData, healthData] =
        await Promise.all([
          overviewResponse.json(),
          historyResponse.json(),
          analyticsResponse.json(),
          healthResponse.json(),
        ]);

      setOverview(overviewData);
      setHistory(historyData.items || []);
      setAnalytics(analyticsData);
      setDatabase(healthData);
      setError("");
      setUpdatedAt(new Date());
    } catch (err) {
      setError(
        `${err.message}. Periksa apakah FastAPI berjalan dan DB_PATH sudah benar.`,
      );
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    refreshDashboard();
    const timer = setInterval(refreshDashboard, 2000);
    return () => clearInterval(timer);
  }, []);

  const isDrowsy = overview?.status === "DROWSY";
  const drowsyHistory = useMemo(
    () => history.filter((row) => row.status === "DROWSY"),
    [history],
  );
  const hourlyData = useMemo(() => analytics.hourly || [], [analytics]);
  const weeklyData = useMemo(() => analytics.weekly || [], [analytics]);

  return (
    <main className={`dashboard ${isDrowsy ? "drowsy-mode" : ""}`}>
      <div className="background-glow glow-one" />
      <div className="background-glow glow-two" />

      <div className="dashboard-content">
        <header className="header">
          <div className="brand">
            <h1>
              Zero<span>Sleep</span>
            </h1>
          </div>

          <div className="header-actions">
            <span
              className={`database-indicator ${database?.ok ? "online" : "offline"}`}
            >
              {database?.ok ? "Database connected" : "Database disconnected"}
            </span>
            <button
              className="refresh-button"
              onClick={refreshDashboard}
              disabled={loading}
            >
              {loading ? "Memuat..." : "Refresh"}
            </button>
          </div>
        </header>

        {isDrowsy && (
          <section className="critical-alert">
            <div>
              <strong>PERINGATAN: INDIKASI MENGANTUK</strong>
              <p>
                Sistem mendeteksi kondisi mengantuk. Segera berhenti berkendara
                di tempat aman dan beristirahat.
              </p>
            </div>
          </section>
        )}

        {error && <div className="error-message">{error}</div>}

        <section className="page-heading">
          <div>
            <h2>Monitoring Pengguna</h2>
          </div>
          <div className="last-updated">
            Update: {updatedAt ? updatedAt.toLocaleTimeString("id-ID") : "--"}
          </div>
        </section>

        <section className="overview-layout">
          <article className="current-status card">
            <div className="card-heading">
              <h3>Status Pengguna</h3>
              <div className="current-pill">
                <StatusPill status={overview?.status || "UNKNOWN"} />
              </div>
            </div>

            <div className="connection-list">
              <span>
                <i
                  className={
                    overview?.sensor_connected ? "green-dot" : "gray-dot"
                  }
                />
                Sensor
              </span>
              <span>
                <i
                  className={
                    overview?.camera_data_available ? "green-dot" : "gray-dot"
                  }
                />
                Kamera
              </span>
            </div>
          </article>

          <div className="metrics-grid">
            <MetricCard
              label="Heart Rate"
              value={overview?.heart_rate}
              unit=" BPM"
            />
            <MetricCard
              label="Sudut Kepala"
              value={overview?.head_angle}
              unit="°"
            />
            <MetricCard label="Nods / 7 detik" value={overview?.nods_7s} />
            <MetricCard label="Deteksi Kamera" value={overview?.eye_status} />
          </div>
        </section>

        <section className="section">
          <div className="section-heading">
            <h3>Output Perangkat</h3>
          </div>

          <div className="device-grid">
            {[
              ["Buzzer", overview?.buzzer_status],
              ["Vibrator", overview?.vibrator_status],
            ].map(([label, value]) => (
              <article className="device-card" key={label}>
                <div className="device-info">
                  <strong>{label}</strong>
                </div>
              </article>
            ))}
          </div>
        </section>

        <section className="section">
          <div className="section-heading">
            <h3>Statistik Mengantuk</h3>
            <span className="period-label">7 hari terakhir</span>
          </div>

          <div className="analytics-summary card">
            <strong>{analytics.weekly_total ?? 0}</strong>
            <div className="summary-text">
              <span>Total episode mengantuk</span>
              <small>Dalam 7 hari terakhir</small>
            </div>
          </div>

          <div className="charts-grid">
            <article className="card chart-card">
              <div className="chart-title">
                <h4>Jumlah Mengantuk per Hari</h4>
              </div>
              <div className="chart-area">
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart
                    data={weeklyData}
                    margin={{ top: 10, right: 10, left: -18, bottom: 0 }}
                  >
                    <CartesianGrid
                      strokeDasharray="3 3"
                      vertical={false}
                      stroke="#f1dce8"
                    />
                    <XAxis
                      dataKey="label"
                      tick={{ fontSize: 11, fill: "#9a8293" }}
                      axisLine={false}
                      tickLine={false}
                    />
                    <YAxis
                      allowDecimals={false}
                      tick={{ fontSize: 11, fill: "#9a8293" }}
                      axisLine={false}
                      tickLine={false}
                    />
                    <Tooltip />
                    <Bar
                      dataKey="count"
                      name="Episode mengantuk"
                      fill="#ec4899"
                      radius={[7, 7, 0, 0]}
                      maxBarSize={42}
                    />
                  </BarChart>
                </ResponsiveContainer>
              </div>
            </article>

            <article className="card chart-card hourly-card">
              <div className="chart-title">
                <h4>Jumlah Mengantuk per Jam</h4>
              </div>
              <div className="chart-area">
                <ResponsiveContainer width="100%" height="100%">
                  <AreaChart
                    data={hourlyData}
                    margin={{ top: 10, right: 12, left: -18, bottom: 0 }}
                  >
                    <defs>
                      <linearGradient
                        id="hourlyFill"
                        x1="0"
                        y1="0"
                        x2="0"
                        y2="1"
                      >
                        <stop
                          offset="0%"
                          stopColor="#ec4899"
                          stopOpacity={0.25}
                        />
                        <stop
                          offset="100%"
                          stopColor="#ec4899"
                          stopOpacity={0.02}
                        />
                      </linearGradient>
                    </defs>
                    <CartesianGrid
                      strokeDasharray="3 3"
                      vertical={false}
                      stroke="#f1dce8"
                    />
                    <XAxis
                      dataKey="hour"
                      interval={2}
                      tick={{ fontSize: 10, fill: "#9a8293" }}
                      axisLine={false}
                      tickLine={false}
                    />
                    <YAxis
                      allowDecimals={false}
                      tick={{ fontSize: 11, fill: "#9a8293" }}
                      axisLine={false}
                      tickLine={false}
                    />
                    <Tooltip />
                    <Area
                      type="monotone"
                      dataKey="count"
                      name="Episode mengantuk"
                      stroke="#ec4899"
                      strokeWidth={2.5}
                      fill="url(#hourlyFill)"
                    />
                  </AreaChart>
                </ResponsiveContainer>
              </div>
            </article>
          </div>
        </section>

        <section className="section card history-card drowsy-history-card">
          <div className="section-heading">
            <h3>Riwayat Mengantuk</h3>
            <span className="record-count drowsy-count">
              {drowsyHistory.length} kejadian
            </span>
          </div>

          <div className="table-scroll">
            <table>
              <thead>
                <tr>
                  <th>Waktu</th>
                  <th>Heart Rate</th>
                  <th>Sudut Kepala</th>
                  <th>Nods / 7s</th>
                  <th>Status</th>
                </tr>
              </thead>
              <tbody>
                {drowsyHistory.length ? (
                  drowsyHistory.map((row, index) => (
                    <tr key={`${row.timestamp}-${index}`}>
                      <td>{row.timestamp || "--"}</td>
                      <td>{formatValue(row.heart_rate, " BPM")}</td>
                      <td>{formatValue(row.head_angle, "°")}</td>
                      <td>{formatValue(row.nods_7s)}</td>
                      <td>
                        <StatusPill status={row.status} />
                      </td>
                    </tr>
                  ))
                ) : (
                  <tr>
                    <td colSpan="5" className="empty-row">
                      Belum ada riwayat mengantuk.
                    </td>
                  </tr>
                )}
              </tbody>
            </table>
          </div>
        </section>
      </div>
    </main>
  );
}

export default App;
