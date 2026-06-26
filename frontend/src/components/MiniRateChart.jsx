import { useEffect, useRef } from "react";

function smoothPath(context, points) {
  if (!points.length) return;
  context.moveTo(points[0].x, points[0].y);
  for (let index = 1; index < points.length - 1; index += 1) {
    const current = points[index];
    const next = points[index + 1];
    context.quadraticCurveTo(current.x, current.y, (current.x + next.x) / 2, (current.y + next.y) / 2);
  }
  if (points.length > 1) {
    const last = points[points.length - 1];
    context.lineTo(last.x, last.y);
  }
}

export function MiniRateChart({ history, dataKey, color = "#5bc8ff" }) {
  const canvasRef = useRef(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const context = canvas.getContext("2d");
    const ratio = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    const width = Math.max(160, Math.floor(rect.width));
    const height = 88;

    if (canvas.width !== width * ratio || canvas.height !== height * ratio) {
      canvas.width = width * ratio;
      canvas.height = height * ratio;
    }

    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, width, height);

    const padding = { top: 10, right: 10, bottom: 10, left: 10 };
    const plotWidth = width - padding.left - padding.right;
    const plotHeight = height - padding.top - padding.bottom;
    const samples = history.length ? history.slice(-42) : [{ [dataKey]: 0 }];
    const values = samples.map((point) => Number(point[dataKey] || 0));
    const max = Math.max(1024, ...values);
    const average = values.reduce((sum, value) => sum + value, 0) / Math.max(1, values.length);

    const panelGradient = context.createLinearGradient(0, 0, width, height);
    panelGradient.addColorStop(0, "rgba(255,255,255,0.11)");
    panelGradient.addColorStop(1, "rgba(255,255,255,0.035)");
    context.fillStyle = panelGradient;
    context.fillRect(0, 0, width, height);

    context.strokeStyle = "rgba(255,255,255,0.11)";
    context.lineWidth = 1;
    for (let index = 1; index <= 2; index += 1) {
      const y = padding.top + (plotHeight / 3) * index;
      context.beginPath();
      context.moveTo(0, y);
      context.lineTo(width, y);
      context.stroke();
    }

    const averageY = padding.top + plotHeight - (Math.min(average, max) / max) * plotHeight;
    context.save();
    context.setLineDash([7, 7]);
    context.strokeStyle = "rgba(255, 209, 102, 0.58)";
    context.beginPath();
    context.moveTo(0, averageY);
    context.lineTo(width, averageY);
    context.stroke();
    context.restore();

    const points = samples.map((point, index) => {
      const value = Math.min(Number(point[dataKey] || 0), max);
      return {
        x: padding.left + (samples.length === 1 ? plotWidth : (plotWidth * index) / (samples.length - 1)),
        y: padding.top + plotHeight - (value / max) * plotHeight,
      };
    });

    if (points.length) {
      context.beginPath();
      smoothPath(context, points);
      context.lineTo(points[points.length - 1].x, padding.top + plotHeight);
      context.lineTo(points[0].x, padding.top + plotHeight);
      context.closePath();
      const fillGradient = context.createLinearGradient(0, padding.top, 0, padding.top + plotHeight);
      fillGradient.addColorStop(0, `${color}66`);
      fillGradient.addColorStop(1, `${color}12`);
      context.fillStyle = fillGradient;
      context.fill();

      context.beginPath();
      smoothPath(context, points);
      context.strokeStyle = color;
      context.lineWidth = 2.5;
      context.lineJoin = "round";
      context.lineCap = "round";
      context.stroke();

      const last = points[points.length - 1];
      context.fillStyle = color;
      context.beginPath();
      context.arc(last.x, last.y, 3.4, 0, Math.PI * 2);
      context.fill();
    }
  }, [history, dataKey, color]);

  return <canvas className="mini-rate-chart" ref={canvasRef} height="88" aria-hidden="true" />;
}
