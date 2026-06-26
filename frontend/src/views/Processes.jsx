import { Empty } from "../components/Empty.jsx";
import { RateBars } from "../components/RateBars.jsx";
import { matches } from "../utils/format.js";

export function Processes({ snapshot, filter, maxRate }) {
  const processes = (snapshot.processes || []).filter((item) => matches(item, filter));

  return (
    <>
      <div className="section-head">
        <div>
          <h2>按进程统计</h2>
          <p>Chrome、VS Code、终端、代理工具等进程连接归因</p>
        </div>
        <div className="summary-strip">
          <span>{processes.length} 进程</span>
          <span>连接真实</span>
          <span>速率估算</span>
        </div>
      </div>
      <div className="process-grid">
        {processes.length ? processes.map((item) => (
          <article className="process-card" key={item.key}>
            <div className="process-title">
              <div>
                <strong>{item.command}</strong>
                <div className="process-meta">
                  <span>PID {item.pid}</span>
                  <span>{item.connections} 连接</span>
                  <span>{item.established} established</span>
                </div>
              </div>
              <span className={`service-chip ${item.ssh ? "ssh" : ""}`}>{item.ssh ? "SSH" : item.udp ? "TCP/UDP" : "TCP"}</span>
            </div>
            <RateBars item={item} maxRate={maxRate} />
            <div className="process-meta">
              <span>端口 {(item.ports || []).join(", ") || "-"}</span>
              <span>远程 {(item.remoteHosts || []).slice(0, 3).join(", ") || "-"}</span>
            </div>
          </article>
        )) : <Empty>没有匹配的进程</Empty>}
      </div>
    </>
  );
}
