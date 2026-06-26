import { Empty } from "../components/Empty.jsx";
import { RateBars } from "../components/RateBars.jsx";
import { detectedSshSessions } from "../utils/connections.js";
import { matches } from "../utils/format.js";

export function Remote({ snapshot, filter, maxRate, onRefresh }) {
  const targets = (snapshot.remoteTargets || []).filter((item) => matches(item, filter));
  const sshSessions = detectedSshSessions(snapshot).filter((item) => matches(item, filter));

  async function submit(event) {
    event.preventDefault();
    const form = event.currentTarget;
    const payload = Object.fromEntries(new FormData(form).entries());
    payload.port = Number(payload.port);
    const response = await fetch("/api/targets", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    if (!response.ok) throw new Error("添加目标失败");
    form.reset();
    form.port.value = 22;
    await onRefresh();
  }

  async function remove(id) {
    await fetch(`/api/targets/${encodeURIComponent(id)}`, { method: "DELETE" });
    await onRefresh();
  }

  return (
    <>
      <div className="section-head">
        <div>
          <h2>远程监控</h2>
          <p>自动识别 VS Code Remote-SSH，并监控手动添加的远程目标</p>
        </div>
        <div className="summary-strip">
          <span>{sshSessions.length} SSH 会话</span>
          <span>{targets.length} 手动目标</span>
          <span>TCP 探测</span>
          <span>速率估算</span>
        </div>
      </div>

      <section className="panel detected-panel">
        <div className="panel-header">
          <div>
            <h2>自动检测的 SSH 会话</h2>
            <p>包含 VS Code Remote-SSH 的真实远端连接与本地转发归因</p>
          </div>
        </div>
        <div className="remote-grid">
          {sshSessions.length ? sshSessions.map((item) => (
            <article className="remote-card" key={item.id}>
              <div className="remote-title">
                <div>
                  <strong>{item.host}:{item.port}</strong>
                  <div className="remote-address">{item.name} · {item.command}</div>
                </div>
                <span className={`state-chip ${item.status}`}>{item.status}</span>
              </div>
              <div className="remote-meta">
                <span>{item.protocol}</span>
                <span>{item.connections} 连接</span>
                <span>本地 {item.local}</span>
              </div>
              <RateBars item={item} maxRate={maxRate} />
            </article>
          )) : <Empty>当前没有检测到远端 SSH 会话</Empty>}
        </div>
      </section>

      <form className="target-form" onSubmit={submit}>
        <label><span>名称</span><input name="name" placeholder="生产 SSH" required /></label>
        <label><span>地址</span><input name="host" placeholder="192.168.1.20" required /></label>
        <label><span>端口</span><input name="port" type="number" min="1" max="65535" defaultValue="22" required /></label>
        <label>
          <span>协议</span>
          <select name="protocol" defaultValue="ssh">
            <option value="ssh">ssh</option>
            <option value="https">https</option>
            <option value="http">http</option>
            <option value="tcp">tcp</option>
          </select>
        </label>
        <button type="submit">添加目标</button>
      </form>
      <div className="panel-header target-header">
        <div>
          <h2>手动远程目标</h2>
          <p>用于持续探测固定服务器地址、端口和连通状态</p>
        </div>
      </div>
      <div className="remote-grid">
        {targets.length ? targets.map((item) => (
          <article className="remote-card" key={item.id}>
            <div className="remote-title">
              <div>
                <strong>{item.name}</strong>
                <div className="remote-address">{item.host}:{item.port}</div>
              </div>
              <span className={`state-chip ${item.status}`}>{item.status}</span>
            </div>
            <div className="remote-meta">
              <span>{item.protocol}</span>
              <span>{item.latencyMs || 0} ms</span>
              <span>{item.connections} 连接</span>
              {item.error ? <span>{item.error}</span> : null}
            </div>
            <RateBars item={item} maxRate={maxRate} />
            <button className="delete-target" type="button" onClick={() => remove(item.id)}>删除</button>
          </article>
        )) : <Empty>还没有启用的远程目标</Empty>}
      </div>
    </>
  );
}
