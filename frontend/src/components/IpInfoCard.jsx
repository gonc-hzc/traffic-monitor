import { useEffect, useMemo, useState } from "react";

function countryLabel(data) {
  const country = data?.country_name || data?.country || "未知";
  const code = data?.country_code ? ` · ${data.country_code}` : "";
  return `${country}${code}`;
}

function locationLabel(data) {
  const parts = [data?.city, data?.region, data?.country_name].filter(Boolean);
  return parts.length ? parts.join(", ") : "等待刷新";
}

function coordinateLabel(data) {
  const lat = Number(data?.latitude);
  const lon = Number(data?.longitude);
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return "-";
  return `${lon.toFixed(2)}, ${lat.toFixed(2)}`;
}

function maskedIp(ip, enabled = true) {
  if (!ip) return enabled ? "等待刷新" : "未授权";
  const parts = String(ip).split(".");
  if (parts.length === 4) return `${parts[0]}.${parts[1]}.•••.${parts[3]}`;
  return String(ip).replace(/[0-9a-f]/gi, "•").slice(0, 18);
}

function routeList(info) {
  if (Array.isArray(info?.routes) && info.routes.length) return info.routes;
  return [{
    id: "current",
    label: "当前出口",
    path: "默认网络路径",
    ok: Boolean(info?.ok),
    source: info?.source || "",
    fetchedAt: info?.fetchedAt || 0,
    nextRefreshAt: info?.nextRefreshAt || 0,
    error: info?.error || "",
    data: info?.data || {},
  }];
}

function secondsUntil(target, now) {
  if (!target) return 0;
  return Math.max(0, Math.ceil((target - now) / 1000));
}

function friendlyError(message) {
  if (!message) return "";
  if (message.includes("HTTP 429")) return "IP 查询服务限流，已启用备用源并会稍后重试";
  if (message.includes("HTTP 403")) return "IP 查询服务拒绝请求，稍后会自动切换或重试";
  if (message.includes("Could not resolve") || message.includes("Failed to connect")) return "当前网络无法连接 IP 查询服务";
  return message;
}

async function readIpInfo(force = false) {
  const response = await fetch(force ? "/api/ip-info/refresh" : "/api/ip-info", {
    method: force ? "POST" : "GET",
    cache: "no-store",
  });
  if (!response.ok) throw new Error(`ip-info ${response.status}`);
  return response.json();
}

async function setConsent(enabled) {
  const response = await fetch("/api/ip-info/consent", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ enabled }),
  });
  if (!response.ok) throw new Error(`ip-consent ${response.status}`);
  return response.json();
}

export function IpInfoCard() {
  const [info, setInfo] = useState(null);
  const [now, setNow] = useState(Date.now());
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");

  const enabled = Boolean(info?.enabled);
  const routes = useMemo(() => routeList(info), [info]);
  const countdown = secondsUntil(info?.nextRefreshAt, now);
  const errorText = friendlyError(error || info?.error || "");

  async function load(force = false) {
    setBusy(true);
    try {
      setInfo(await readIpInfo(force));
      setError("");
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  async function allowLookup() {
    setBusy(true);
    try {
      setInfo(await setConsent(true));
      setError("");
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  async function disableLookup() {
    setBusy(true);
    try {
      setInfo(await setConsent(false));
      setError("");
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  useEffect(() => {
    load(false);
    const clock = setInterval(() => setNow(Date.now()), 1000);
    const poll = setInterval(() => load(false), 60000);
    return () => {
      clearInterval(clock);
      clearInterval(poll);
    };
  }, []);

  return (
    <section className={`panel ip-card ${enabled ? "" : "is-locked"}`}>
      <div className="ip-card-head">
        <div className="ip-card-title">
          <span className="ip-card-mark">⌖</span>
          <div>
            <h2>IP 信息</h2>
            <p>{enabled ? `${info?.source || "双出口检测"} · 自动刷新` : "需要你允许后才访问公网查询服务"}</p>
          </div>
        </div>
        <button
          className="icon-button"
          type="button"
          title={enabled ? "刷新 IP 信息" : "允许刷新公网 IP 信息"}
          aria-label={enabled ? "刷新 IP 信息" : "允许刷新公网 IP 信息"}
          disabled={busy}
          onClick={enabled ? () => load(true) : allowLookup}
        >
          ↻
        </button>
      </div>

      {enabled ? (
        <>
          <div className="ip-route-grid">
            {routes.map((route) => {
              const data = route.data || {};
              const routeError = friendlyError(route.error || "");
              return (
                <article className={`ip-route-card ${route.ok ? "is-ok" : "is-muted"}`} key={route.id}>
                  <div className="ip-route-head">
                    <div>
                      <strong>{route.label || "出口"}</strong>
                      <span>{route.path || "默认路径"}</span>
                    </div>
                    <em>{route.ok ? (route.source || "ok") : "未就绪"}</em>
                  </div>
                  <div className="ip-route-main">
                    <div className="ip-primary">
                      <strong>{route.ok ? countryLabel(data) : route.id === "proxy" ? "未检测到代理出口" : "等待刷新"}</strong>
                      <span className="ip-address">IP: <b>{maskedIp(data.ip, enabled)}</b></span>
                      <span>自治域: {data.asn || "-"}</span>
                    </div>
                    <div className="ip-detail-list">
                      <span><b>服务商</b>{data.org || "-"}</span>
                      <span><b>位置</b>{route.ok ? locationLabel(data) : routeError || "等待刷新"}</span>
                      <span><b>时区</b>{data.timezone || "-"}</span>
                    </div>
                  </div>
                  <div className="ip-route-foot">
                    <span>{route.proxyUrl ? `代理 ${route.proxyUrl}` : coordinateLabel(data)}</span>
                    <span>{route.fetchedAt ? new Date(route.fetchedAt).toLocaleTimeString("zh-CN", { hour12: false }) : `${secondsUntil(route.nextRefreshAt, now)}s 后重试`}</span>
                  </div>
                </article>
              );
            })}
          </div>
          <div className="ip-card-footer">
            <span>自动刷新: {countdown}s</span>
            <span>{routes.filter((route) => route.ok).length}/{routes.length} 个出口可用</span>
          </div>
          <div className="ip-card-actions">
            <button type="button" onClick={disableLookup} disabled={busy}>关闭公网查询</button>
            {errorText ? <span>{errorText}</span> : <span>上次刷新 {info?.fetchedAt ? new Date(info.fetchedAt).toLocaleTimeString("zh-CN", { hour12: false }) : "-"}</span>}
          </div>
        </>
      ) : (
        <div className="ip-consent">
          <strong>公网 IP 查询未开启</strong>
          <p>允许后会每 5 分钟通过 HTTPS 查询一次公网 IP、服务商、位置和时区，并保存在本地缓存中。</p>
          <button type="button" onClick={allowLookup} disabled={busy}>{busy ? "正在开启..." : "允许实时刷新"}</button>
          {error ? <span>{error}</span> : null}
        </div>
      )}
    </section>
  );
}
