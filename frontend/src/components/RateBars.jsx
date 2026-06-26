import { formatRate, rateWidth } from "../utils/format.js";

export function RateBars({ item, maxRate }) {
  return (
    <div className="rate-bars">
      <div className="rate-row">
        <span>下行</span>
        <div className="bar download"><span style={{ width: rateWidth(item.downloadBps, maxRate) }} /></div>
        <strong className="mono">{formatRate(item.downloadBps)}</strong>
      </div>
      <div className="rate-row">
        <span>上行</span>
        <div className="bar upload"><span style={{ width: rateWidth(item.uploadBps, maxRate) }} /></div>
        <strong className="mono">{formatRate(item.uploadBps)}</strong>
      </div>
    </div>
  );
}
