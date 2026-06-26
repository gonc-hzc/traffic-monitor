import { MiniRateChart } from "./MiniRateChart.jsx";

export function MetricCard({ type, label, badge, value, note, history, trendKey, trendColor }) {
  const valueParts = typeof value === "object" && value !== null
    ? value
    : { value, unit: "" };
  const title = [valueParts.value, valueParts.unit].filter(Boolean).join(" ");

  return (
    <article className={`metric-card ${type || ""}`}>
      <div className="metric-card-header">
        <span>{label}</span>
        <span className="metric-badge">{badge}</span>
      </div>
      <strong className="metric-value" title={title}>
        <span className="metric-number">{valueParts.value}</span>
        {valueParts.unit ? <span className="metric-unit">{valueParts.unit}</span> : null}
      </strong>
      <small>{note}</small>
      {history && trendKey ? <MiniRateChart history={history} dataKey={trendKey} color={trendColor} /> : null}
    </article>
  );
}
