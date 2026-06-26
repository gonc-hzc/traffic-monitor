import { Empty } from "../components/Empty.jsx";
import { formatTime, matches } from "../utils/format.js";

export function Logs({ snapshot, filter }) {
  const events = (snapshot.events || []).filter((item) => matches(item, filter));

  return (
    <>
      <div className="section-head">
        <div>
          <h2>日志采集</h2>
          <p>采样异常、SSH 变化、远程状态和高流量事件</p>
        </div>
        <a className="download-link" href="/api/export/logs">导出日志</a>
      </div>
      <section className="panel">
        <div className="log-path">{snapshot.logFile || "日志文件等待初始化"} | 历史采样 {snapshot.historyFile || "-"}</div>
        <div className="log-stream">
          {events.length ? events.map((item) => (
            <div className={`log-entry ${item.level}`} key={item.id}>
              <time>{formatTime(item.ts)}</time>
              <span>{item.type} / {item.level}</span>
              <div>
                <strong>{item.message}</strong>
                <span>{JSON.stringify(item.detail || {})}</span>
              </div>
            </div>
          )) : <Empty>暂无日志事件</Empty>}
        </div>
      </section>
    </>
  );
}
