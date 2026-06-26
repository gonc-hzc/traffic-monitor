import { Empty } from "../components/Empty.jsx";
import { IpInfoCard } from "../components/IpInfoCard.jsx";
import { MetricCard } from "../components/MetricCard.jsx";
import { TrafficChart } from "../components/TrafficChart.jsx";
import { detectedSshSessions } from "../utils/connections.js";
import { formatBytes, formatRate, formatRateParts, matches } from "../utils/format.js";

export function Overview({ snapshot, history, filter }) {
  const interfaces = (snapshot.interfaces || []).filter((item) => matches(item, filter));
  const sshConnections = (snapshot.ssh?.connections || []).filter((item) => matches(item, filter));
  const sshSessions = detectedSshSessions(snapshot).filter((item) => matches(item, filter));
  const status = snapshot.status || {};
  const isDemo = status.rateMode === "demo";

  return (
    <>
      <div className="metric-grid">
        <MetricCard type="upload" label="Upload Speed" badge="上行" value={formatRateParts(snapshot.totals.uploadBps)} note={`累计 ${formatBytes(snapshot.totals.uploadedBytes)}`} history={history} trendKey="uploadBps" trendColor="#54e68f" />
        <MetricCard type="download" label="Download Speed" badge="下行" value={formatRateParts(snapshot.totals.downloadBps)} note={`累计 ${formatBytes(snapshot.totals.downloadedBytes)}`} history={history} trendKey="downloadBps" trendColor="#5bc8ff" />
        <MetricCard type="compact" label="SSH" badge="连接" value={snapshot.ssh?.active || 0} note="活跃 / 监听连接" />
        <MetricCard type="compact" label="Connections" badge="全部" value={snapshot.connections.total} note={`${snapshot.connections.established} established / ${snapshot.connections.listening} listen`} />
      </div>

      <div className="alert-strip">
        {(snapshot.alerts || []).length ? (
          snapshot.alerts.map((item, index) => (
            <div className={`alert-item ${item.level}`} key={`${item.title}-${index}`}>
              <span className="alert-dot" />
              <strong>{item.title}</strong>
              <span>{item.message}</span>
            </div>
          ))
        ) : (
          <div className="alert-item">
            <span className="alert-dot" />
            <strong>状态正常</strong>
            <span>没有触发当前告警规则</span>
          </div>
        )}
      </div>

      <div className="source-grid">
        <div className={`source-card ${isDemo ? "warn" : ""}`}>
          <strong>总流量速率</strong>
          <span>{isDemo ? "采样受限，当前为模拟曲线" : `真实采样 · ${status.networkSource || "system"}`}</span>
        </div>
        <div className="source-card">
          <strong>连接与 SSH</strong>
          <span>真实采样 · {status.connectionSource || "lsof"}</span>
        </div>
        <div className="source-card estimate">
          <strong>进程 / 协议速率</strong>
          <span>{status.processRateMode === "estimated" ? "按连接权重估算" : "系统精确采样"}</span>
        </div>
      </div>

      <section className="panel chart-panel">
        <div className="panel-header">
          <div>
            <h2>Bandwidth Usage</h2>
            <p>上行与下行实时曲线</p>
          </div>
          <div className="legend">
            <span><i className="legend-upload" />Upload</span>
            <span><i className="legend-download" />Download</span>
          </div>
        </div>
        <TrafficChart history={history} />
      </section>

      <div className="overview-lower">
        <section className="panel">
          <div className="panel-header">
            <div>
              <h2>网络接口</h2>
              <p>按当前速率排序</p>
            </div>
          </div>
          <div className="table-wrap">
            <table>
              <thead>
                <tr><th>接口</th><th>下行</th><th>上行</th><th>累计接收</th><th>累计发送</th></tr>
              </thead>
              <tbody>
                {interfaces.length ? interfaces.map((item) => (
                  <tr key={item.name}>
                    <td><span className="port-chip">{item.name}</span></td>
                    <td className="mono">{formatRate(item.rxBps)}</td>
                    <td className="mono">{formatRate(item.txBps)}</td>
                    <td className="mono">{formatBytes(item.rxBytes)}</td>
                    <td className="mono">{formatBytes(item.txBytes)}</td>
                  </tr>
                )) : <tr><td colSpan="5"><Empty>没有匹配的接口</Empty></td></tr>}
              </tbody>
            </table>
          </div>
        </section>

        <div className="overview-side-stack">
          <IpInfoCard />

          <section className="panel">
            <div className="panel-header">
              <div>
                <h2>SSH 会话</h2>
                <p>优先显示 VS Code Remote-SSH 等真实远端连接</p>
              </div>
            </div>
            <div className="connection-list">
              {sshSessions.length ? sshSessions.map((item) => (
                <div className="connection-item is-primary" key={item.id}>
                  <strong>{item.host}:{item.port} <span className="service-chip ssh">SSH</span></strong>
                  <span>{item.name} · {item.local}</span>
                  <span>估算 {formatRate(item.downloadBps)} 下行 / {formatRate(item.uploadBps)} 上行</span>
                </div>
              )) : sshConnections.length ? sshConnections.map((item) => (
                <div className="connection-item" key={item.id}>
                  <strong>{item.command} <span className="mono">PID {item.pid}</span></strong>
                  <span>{item.local.address}:{item.local.port} → {item.remote.address || "*"}:{item.remote.port || "*"}</span>
                  <span>{item.protocol} / {item.state}</span>
                </div>
              )) : <Empty>当前没有 SSH 连接</Empty>}
            </div>
          </section>
        </div>
      </div>
    </>
  );
}
