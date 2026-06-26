import { Empty } from "../components/Empty.jsx";
import { RateBars } from "../components/RateBars.jsx";
import { formatRate, matches } from "../utils/format.js";

export function Protocols({ snapshot, filter, maxRate }) {
  const protocols = (snapshot.protocols || []).filter((item) => matches(item, filter));
  const hosts = (snapshot.hosts || []).filter((item) => matches(item, filter)).slice(0, 12);

  return (
    <>
      <div className="section-head">
        <div>
          <h2>协议 / 端口统计</h2>
          <p>用于发现 SSH、HTTPS、DNS、代理和本地服务连接分布</p>
        </div>
        <div className="summary-strip">
          <span>{protocols.length} 项</span>
          <span>连接真实</span>
          <span>估算速率</span>
        </div>
      </div>
      <section className="panel">
        <div className="table-wrap">
          <table>
            <thead>
              <tr><th>协议</th><th>端口</th><th>服务</th><th>连接</th><th>下行</th><th>上行</th><th>进程</th><th>远程地址</th></tr>
            </thead>
            <tbody>
              {protocols.length ? protocols.map((item) => (
                <tr key={item.key}>
                  <td>{item.protocol}</td>
                  <td><span className="port-chip">{item.port}</span></td>
                  <td><span className={`service-chip ${item.ssh ? "ssh" : ""}`}>{item.service}</span></td>
                  <td className="mono">{item.connections} / {item.established}</td>
                  <td className="mono">{formatRate(item.downloadBps)}</td>
                  <td className="mono">{formatRate(item.uploadBps)}</td>
                  <td>{(item.processes || []).join(", ") || "-"}</td>
                  <td>{(item.remoteHosts || []).join(", ") || "-"}</td>
                </tr>
              )) : <tr><td colSpan="8"><Empty>没有匹配的协议或端口</Empty></td></tr>}
            </tbody>
          </table>
        </div>
      </section>

      <section className="panel host-panel">
        <div className="panel-header">
          <div>
            <h2>Top 主机排行</h2>
            <p>按当前连接权重和估算速率排序</p>
          </div>
        </div>
        <div className="host-grid">
          {hosts.length ? hosts.map((item) => (
            <article className="host-card" key={item.host}>
              <div className="process-title">
                <div>
                  <strong>{item.host}</strong>
                  <div className="process-meta">
                    <span>{item.external ? "外部" : "本地"}</span>
                    <span>{item.connections} 连接</span>
                    <span>{item.established} established</span>
                  </div>
                </div>
                <span className="service-chip">{item.services?.[0] || "TCP"}</span>
              </div>
              <RateBars item={item} maxRate={maxRate} />
              <div className="process-meta">
                <span>端口 {(item.ports || []).join(", ") || "-"}</span>
                <span>进程 {(item.processes || []).slice(0, 3).join(", ") || "-"}</span>
              </div>
            </article>
          )) : <Empty>没有匹配的主机</Empty>}
        </div>
      </section>
    </>
  );
}
