import { useEffect, useState } from "react";

export function useSnapshot() {
  const [snapshot, setSnapshot] = useState(null);
  const [history, setHistory] = useState([]);
  const [error, setError] = useState("");

  async function refresh() {
    const response = await fetch("/api/snapshot", { cache: "no-store" });
    if (!response.ok) throw new Error(`snapshot ${response.status}`);
    const next = await response.json();
    setSnapshot(next);
    setHistory((current) => {
      const point = {
        ts: next.timestamp,
        uploadBps: next.totals.uploadBps,
        downloadBps: next.totals.downloadBps,
      };
      return [...current, point].slice(-90);
    });
    setError("");
  }

  useEffect(() => {
    let mounted = true;
    async function tick() {
      try {
        if (mounted) await refresh();
      } catch (err) {
        if (mounted) setError(err.message);
      }
    }
    tick();
    const timer = setInterval(tick, 1000);
    return () => {
      mounted = false;
      clearInterval(timer);
    };
  }, []);

  return { snapshot, history, error, refresh };
}
