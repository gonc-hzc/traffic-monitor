import { useEffect, useRef } from "react";
import { formatRate, formatTime } from "../utils/format.js";

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

export function TrafficChart({ history }) {
  const canvasRef = useRef(null);
  const maxRateRef = useRef(1);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const context = canvas.getContext("2d");
    const ratio = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    const width = Math.max(320, Math.floor(rect.width));
    const height = 320;
    if (canvas.width !== width * ratio || canvas.height !== height * ratio) {
      canvas.width = width * ratio;
      canvas.height = height * ratio;
    }
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, width, height);

    const padding = { top: 22, right: 18, bottom: 32, left: 72 };
    const plotWidth = width - padding.left - padding.right;
    const plotHeight = height - padding.top - padding.bottom;
    const maxPoint = history.reduce((max, point) => Math.max(max, point.uploadBps, point.downloadBps), 1);
    maxRateRef.current = Math.max(maxPoint, maxRateRef.current * 0.96, 1024);
    const maxRate = maxRateRef.current;

    const background = context.createLinearGradient(0, 0, width, height);
    background.addColorStop(0, "rgba(255,255,255,0.08)");
    background.addColorStop(0.45, "rgba(31,167,255,0.06)");
    background.addColorStop(1, "rgba(7,10,15,0.24)");
    context.fillStyle = background;
    context.fillRect(0, 0, width, height);
    context.strokeStyle = "rgba(255,255,255,0.11)";
    context.lineWidth = 1;
    context.fillStyle = "rgba(238,246,255,0.66)";
    context.font = "12px system-ui";
    context.textAlign = "right";

    for (let i = 0; i <= 5; i += 1) {
      const y = padding.top + (plotHeight / 5) * i;
      const value = maxRate * (1 - i / 5);
      context.beginPath();
      context.moveTo(padding.left, y);
      context.lineTo(width - padding.right, y);
      context.stroke();
      context.fillText(formatRate(value), padding.left - 10, y + 4);
    }

    if (history.length) {
      const average = history.reduce((sum, point) => sum + Math.max(point.uploadBps, point.downloadBps), 0) / history.length;
      const averageY = padding.top + plotHeight - (Math.min(average, maxRate) / maxRate) * plotHeight;
      context.save();
      context.setLineDash([8, 8]);
      context.strokeStyle = "rgba(255, 209, 102, 0.5)";
      context.beginPath();
      context.moveTo(padding.left, averageY);
      context.lineTo(width - padding.right, averageY);
      context.stroke();
      context.restore();
    }

    function drawSeries(key, color, fillColor) {
      if (!history.length) return;
      const points = history.map((point, index) => {
        const x = padding.left + (history.length === 1 ? plotWidth : (plotWidth * index) / (history.length - 1));
        const y = padding.top + plotHeight - (Math.min(point[key], maxRate) / maxRate) * plotHeight;
        return { x, y };
      });
      context.beginPath();
      smoothPath(context, points);
      context.lineTo(points[points.length - 1].x, padding.top + plotHeight);
      context.lineTo(points[0].x, padding.top + plotHeight);
      context.closePath();
      context.fillStyle = fillColor;
      context.fill();

      context.beginPath();
      smoothPath(context, points);
      context.strokeStyle = color;
      context.lineWidth = 2.8;
      context.lineJoin = "round";
      context.lineCap = "round";
      context.stroke();

      const last = points[points.length - 1];
      context.fillStyle = color;
      context.beginPath();
      context.arc(last.x, last.y, 4, 0, Math.PI * 2);
      context.fill();
    }

    drawSeries("downloadBps", "#5bc8ff", "rgba(91, 200, 255, 0.17)");
    drawSeries("uploadBps", "#54e68f", "rgba(84, 230, 143, 0.13)");

    if (history.length) {
      context.fillStyle = "rgba(220,232,240,0.55)";
      context.textAlign = "left";
      context.fillText(formatTime(history[0].ts), padding.left, height - 10);
      context.textAlign = "right";
      context.fillText(formatTime(history[history.length - 1].ts), width - padding.right, height - 10);
    }
  }, [history]);

  return <canvas ref={canvasRef} className="traffic-chart" height="320" />;
}
