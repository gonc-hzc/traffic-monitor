export function formatBytes(bytes) {
  const { value, unit } = formatBytesParts(bytes);
  return `${value} ${unit}`;
}

export function formatBytesParts(bytes) {
  const units = ["B", "KB", "MB", "GB", "TB"];
  let value = Math.max(0, Number(bytes) || 0);
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }
  const digits = value >= 100 || unit === 0 ? 0 : value >= 10 ? 1 : 2;
  return { value: value.toFixed(digits), unit: units[unit] };
}

export function formatRate(bytesPerSecond) {
  const { value, unit } = formatRateParts(bytesPerSecond);
  return `${value} ${unit}`;
}

export function formatRateParts(bytesPerSecond) {
  const { value, unit } = formatBytesParts(bytesPerSecond);
  return { value, unit: `${unit}/s` };
}

export function formatTime(ts) {
  return new Date(ts).toLocaleTimeString("zh-CN", { hour12: false });
}

export function rateWidth(value, maxRate) {
  return `${Math.max(2, Math.min(100, (Number(value || 0) / Math.max(1, maxRate)) * 100))}%`;
}

export function matches(row, filter) {
  if (!filter) return true;
  return JSON.stringify(row).toLowerCase().includes(filter);
}
