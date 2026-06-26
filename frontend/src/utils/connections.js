function normalizeAddress(address) {
  return String(address || "").replace(/^\[|\]$/g, "");
}

export function isLoopbackAddress(address) {
  const value = normalizeAddress(address).toLowerCase();
  return value === "127.0.0.1" || value === "::1" || value === "localhost";
}

export function detectedSshSessions(snapshot) {
  const connections = snapshot?.ssh?.connections || [];
  const hostsByAddress = new Map((snapshot?.hosts || []).map((host) => [normalizeAddress(host.host), host]));
  const processesByPid = new Map((snapshot?.processes || []).map((process) => [Number(process.pid), process]));
  const seen = new Set();

  return connections
    .filter((connection) => connection.state === "ESTABLISHED")
    .filter((connection) => {
      const address = normalizeAddress(connection.remote?.address);
      return address && address !== "*" && !isLoopbackAddress(address);
    })
    .map((connection) => {
      const address = normalizeAddress(connection.remote.address);
      const port = connection.remote?.port || "22";
      const key = `${address}:${port}:${connection.pid}`;
      if (seen.has(key)) return null;
      seen.add(key);

      const hostStats = hostsByAddress.get(address);
      const processStats = processesByPid.get(Number(connection.pid));
      const downloadBps = hostStats?.downloadBps ?? processStats?.downloadBps ?? 0;
      const uploadBps = hostStats?.uploadBps ?? processStats?.uploadBps ?? 0;

      return {
        id: key,
        name: `Remote SSH · PID ${connection.pid}`,
        command: connection.command,
        host: address,
        port,
        protocol: "ssh",
        status: "connected",
        local: `${connection.local?.address || "*"}:${connection.local?.port || "*"}`,
        connections: hostStats?.connections || processStats?.connections || 1,
        downloadBps,
        uploadBps,
        rateSource: "estimated",
      };
    })
    .filter(Boolean);
}
