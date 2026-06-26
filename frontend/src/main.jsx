import React, { useMemo, useState } from "react";
import { createRoot } from "react-dom/client";
import { Empty } from "./components/Empty.jsx";
import { useSnapshot } from "./hooks/useSnapshot.js";
import { Logs } from "./views/Logs.jsx";
import { Overview } from "./views/Overview.jsx";
import { Processes } from "./views/Processes.jsx";
import { Protocols } from "./views/Protocols.jsx";
import { Remote } from "./views/Remote.jsx";
import "./styles.css";

const NAV_ITEMS = [
  ["overview", "总览"],
  ["protocols", "协议 / 端口"],
  ["processes", "进程统计"],
  ["remote", "远程监控"],
  ["logs", "日志采集"],
];

function samplingText(snapshot, error) {
  if (error) return error;
  if (!snapshot) return "等待采样";
  if (snapshot.status.rateMode === "demo") return "采样受限 / 模拟曲线";
  const warnings = snapshot.status.warnings || [];
  if (warnings.length) return `有 ${warnings.length} 个采样提醒`;
  return `${snapshot.status.networkSource} 实时采样`;
}

function App() {
  const { snapshot, history, error, refresh } = useSnapshot();
  const [view, setView] = useState("overview");
  const [filter, setFilter] = useState("");

  const maxRate = useMemo(() => {
    const values = [
      snapshot?.totals?.uploadBps || 0,
      snapshot?.totals?.downloadBps || 0,
      ...(snapshot?.processes || []).flatMap((item) => [item.uploadBps || 0, item.downloadBps || 0]),
    ];
    return Math.max(1, ...values);
  }, [snapshot]);

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand">
          <div className="brand-mark"><img src="/app-icon.png" alt="" /></div>
          <div><strong>流量监控</strong><small>Traffic Monitor</small></div>
        </div>
        <nav className="nav-list" aria-label="主导航">
          {NAV_ITEMS.map(([key, label], index) => (
            <button className={`nav-item ${view === key ? "is-active" : ""}`} key={key} onClick={() => setView(key)}>
              <span className="nav-icon">{String(index + 1).padStart(2, "0")}</span>
              <span>{label}</span>
            </button>
          ))}
        </nav>
        <div className="sidebar-footer">
          <div className="tiny-label">采样状态</div>
          <div className="status-pill"><span className="pulse" /><span>{samplingText(snapshot, error)}</span></div>
        </div>
      </aside>

      <main className="workspace">
        <header className="topbar">
          <div>
            <p className="eyebrow">本机实时流量监控 · React + C++</p>
            <h1>Traffic Monitor</h1>
          </div>
          <div className="topbar-actions">
            <div className="search-box">
              <span>⌕</span>
              <input value={filter} onChange={(event) => setFilter(event.target.value.trim().toLowerCase())} type="search" placeholder="过滤进程、端口、地址" />
            </div>
            <button className="icon-button" type="button" title="立即刷新" aria-label="立即刷新" onClick={refresh}>↻</button>
          </div>
        </header>

        {!snapshot ? (
          <section className="panel"><Empty>{error || "正在连接 C++ 后端..."}</Empty></section>
        ) : (
          <>
            {view === "overview" && <Overview snapshot={snapshot} history={history} filter={filter} />}
            {view === "protocols" && <Protocols snapshot={snapshot} filter={filter} maxRate={maxRate} />}
            {view === "processes" && <Processes snapshot={snapshot} filter={filter} maxRate={maxRate} />}
            {view === "remote" && <Remote snapshot={snapshot} filter={filter} maxRate={maxRate} onRefresh={refresh} />}
            {view === "logs" && <Logs snapshot={snapshot} filter={filter} />}
          </>
        )}
      </main>
    </div>
  );
}

createRoot(document.getElementById("root")).render(<App />);
