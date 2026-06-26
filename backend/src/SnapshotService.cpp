#include "SnapshotService.hpp"
#include "JsonUtil.hpp"
#include "Util.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <unistd.h>

namespace tmon {

namespace {

std::string interfacesJson(const std::vector<InterfaceStats> &interfaces) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < interfaces.size() && i < 12; ++i) {
    const auto &iface = interfaces[i];
    if (i) out << ",";
    out << "{"
        << "\"name\":" << json::quote(iface.name) << ","
        << "\"rxBytes\":" << iface.rxBytes << ","
        << "\"txBytes\":" << iface.txBytes << ","
        << "\"rxBps\":" << iface.rxBps << ","
        << "\"txBps\":" << iface.txBps
        << "}";
  }
  out << "]";
  return out.str();
}

std::string connectionsJson(const std::vector<Connection> &connections, size_t limit = 30) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < connections.size() && i < limit; ++i) {
    const auto &connection = connections[i];
    if (i) out << ",";
    out << "{"
        << "\"id\":" << json::quote(connection.id) << ","
        << "\"command\":" << json::quote(connection.command) << ","
        << "\"pid\":" << connection.pid << ","
        << "\"protocol\":" << json::quote(connection.protocol) << ","
        << "\"state\":" << json::quote(connection.state) << ","
        << "\"local\":{\"address\":" << json::quote(connection.local.address) << ",\"port\":" << json::quote(connection.local.port) << "},"
        << "\"remote\":{\"address\":" << json::quote(connection.remote.address) << ",\"port\":" << json::quote(connection.remote.port) << "}"
        << "}";
  }
  out << "]";
  return out.str();
}

std::string protocolsJson(const std::vector<ProtocolRow> &rows) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto &row = rows[i];
    if (i) out << ",";
    out << "{"
        << "\"key\":" << json::quote(row.key) << ","
        << "\"protocol\":" << json::quote(row.protocol) << ","
        << "\"service\":" << json::quote(row.service) << ","
        << "\"port\":" << json::quote(row.port) << ","
        << "\"connections\":" << row.connections << ","
        << "\"established\":" << row.established << ","
        << "\"listening\":" << row.listening << ","
        << "\"ssh\":" << (row.ssh ? "true" : "false") << ","
        << "\"processes\":" << json::stringArray(row.processes) << ","
        << "\"remoteHosts\":" << json::stringArray(row.remoteHosts) << ","
        << "\"downloadBps\":" << row.downloadBps << ","
        << "\"uploadBps\":" << row.uploadBps << ","
        << "\"rateSource\":\"estimated\""
        << "}";
  }
  out << "]";
  return out.str();
}

std::string processesJson(const std::vector<ProcessRow> &rows) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto &row = rows[i];
    if (i) out << ",";
    out << "{"
        << "\"key\":" << json::quote(row.key) << ","
        << "\"command\":" << json::quote(row.command) << ","
        << "\"pid\":" << row.pid << ","
        << "\"connections\":" << row.connections << ","
        << "\"established\":" << row.established << ","
        << "\"listening\":" << row.listening << ","
        << "\"udp\":" << row.udp << ","
        << "\"ssh\":" << row.ssh << ","
        << "\"ports\":" << json::stringArray(row.ports) << ","
        << "\"remoteHosts\":" << json::stringArray(row.remoteHosts) << ","
        << "\"downloadBps\":" << row.downloadBps << ","
        << "\"uploadBps\":" << row.uploadBps << ","
        << "\"rateSource\":\"estimated\""
        << "}";
  }
  out << "]";
  return out.str();
}

std::string hostsJson(const std::vector<HostRow> &rows) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto &row = rows[i];
    if (i) out << ",";
    out << "{"
        << "\"host\":" << json::quote(row.host) << ","
        << "\"connections\":" << row.connections << ","
        << "\"established\":" << row.established << ","
        << "\"services\":" << json::stringArray(row.services) << ","
        << "\"ports\":" << json::stringArray(row.ports) << ","
        << "\"processes\":" << json::stringArray(row.processes) << ","
        << "\"external\":" << (row.external ? "true" : "false") << ","
        << "\"downloadBps\":" << row.downloadBps << ","
        << "\"uploadBps\":" << row.uploadBps << ","
        << "\"rateSource\":\"estimated\""
        << "}";
  }
  out << "]";
  return out.str();
}

std::string remoteJson(const std::vector<RemoteRow> &rows) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto &row = rows[i];
    if (i) out << ",";
    out << "{"
        << "\"id\":" << json::quote(row.id) << ","
        << "\"name\":" << json::quote(row.name) << ","
        << "\"host\":" << json::quote(row.host) << ","
        << "\"port\":" << row.port << ","
        << "\"protocol\":" << json::quote(row.protocol) << ","
        << "\"enabled\":" << (row.enabled ? "true" : "false") << ","
        << "\"status\":" << json::quote(row.status) << ","
        << "\"latencyMs\":" << row.latencyMs << ","
        << "\"error\":" << json::quote(row.error) << ","
        << "\"checkedAt\":" << row.checkedAt << ","
        << "\"connections\":" << row.connections << ","
        << "\"downloadBps\":" << row.downloadBps << ","
        << "\"uploadBps\":" << row.uploadBps << ","
        << "\"rateSource\":\"estimated\""
        << "}";
  }
  out << "]";
  return out.str();
}

std::string platformOs() {
#if defined(__APPLE__)
  return "darwin";
#elif defined(__linux__)
  return "linux";
#else
  return "unknown";
#endif
}

std::string hostname() {
  char name[256] {};
  return gethostname(name, sizeof(name)) == 0 ? std::string(name) : std::string("localhost");
}

} // namespace

SnapshotService::SnapshotService(AppConfig &config, Logger &logger)
  : config_(config),
    logger_(logger),
    networkSampler_(CommandRunner(), logger),
    connectionSampler_(CommandRunner(), logger),
    remoteMonitor_(config, logger),
    publicIpService_(logger) {}

void SnapshotService::trackSshEvents(const std::vector<Connection> &sshConnections) {
  std::set<std::string> current;
  for (const auto &connection : sshConnections) current.insert(connection.id);
  for (const auto &key : current) {
    if (!previousSsh_.count(key)) {
      auto it = std::find_if(sshConnections.begin(), sshConnections.end(), [&](const Connection &item) {
        return item.id == key;
      });
      if (it != sshConnections.end()) {
        logger_.event("info", "SSH", "检测到 SSH 连接",
          "{\"process\":" + json::quote(it->command) + ",\"pid\":" + std::to_string(it->pid) +
          ",\"remote\":" + json::quote(it->remote.address + ":" + it->remote.port) + "}");
      }
    }
  }
  for (const auto &key : previousSsh_) {
    if (!current.count(key)) logger_.event("info", "SSH", "SSH 连接已结束", "{\"key\":" + json::quote(key) + "}");
  }
  previousSsh_ = current;
}

void SnapshotService::persistUsage(long long timestamp, const Totals &totals, double downloadBps, double uploadBps, const Settings &settings) {
  if (timestamp - lastHistoryPersistedAt_ < settings.sampleHistoryEveryMs) return;
  lastHistoryPersistedAt_ = timestamp;
  std::ofstream out(config_.paths().dataDir / "usage-history.jsonl", std::ios::app);
  out << "{"
      << "\"time\":" << json::quote(isoTime(timestamp)) << ","
      << "\"uploadBps\":" << uploadBps << ","
      << "\"downloadBps\":" << downloadBps << ","
      << "\"uploadedBytes\":" << totals.txBytes << ","
      << "\"downloadedBytes\":" << totals.rxBytes << ","
      << "\"source\":" << json::quote(totals.source)
      << "}\n";
}

std::string SnapshotService::alertsJson(const Totals &totals, double downloadBps, double uploadBps, const Settings &settings,
                                        const ConnectionSummary &summary, const std::vector<RemoteRow> &remoteRows) const {
  struct Alert { std::string level, title, message; };
  std::vector<Alert> alerts;
  if (totals.source == "demo") alerts.push_back({"warn", "采样受限", "系统接口字节计数不可用，当前曲线为模拟数据。"});
  if (downloadBps > settings.highDownloadBps) alerts.push_back({"warn", "下行峰值", "下行超过阈值"});
  if (uploadBps > settings.highUploadBps) alerts.push_back({"warn", "上行峰值", "上行超过阈值"});

  std::set<std::string> watchedPorts(settings.watchedPorts.begin(), settings.watchedPorts.end());
  for (const auto &row : summary.protocols) {
    if (watchedPorts.count(row.port) && row.connections > 0) {
      alerts.push_back({row.port == "22" ? "info" : "warn", "重点端口 " + row.port, "当前 " + std::to_string(row.connections) + " 个连接"});
    }
  }

  std::set<std::string> activeHosts;
  for (const auto &host : summary.hosts) activeHosts.insert(lower(host.host));
  for (const auto &host : settings.blockedHosts) {
    if (activeHosts.count(host)) alerts.push_back({"error", "命中拦截主机", host + " 当前仍有连接"});
  }
  for (const auto &host : settings.watchedHosts) {
    if (activeHosts.count(host)) alerts.push_back({"info", "关注主机活动", host + " 当前有连接"});
  }
  for (const auto &row : remoteRows) {
    if (row.enabled && row.status != "up") {
      alerts.push_back({"warn", "远程目标异常", row.name + " " + row.status + (row.error.empty() ? "" : " (" + row.error + ")")});
    }
  }

  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < alerts.size() && i < 12; ++i) {
    if (i) out << ",";
    out << "{\"level\":" << json::quote(alerts[i].level)
        << ",\"title\":" << json::quote(alerts[i].title)
        << ",\"message\":" << json::quote(alerts[i].message) << "}";
  }
  out << "]";
  return out.str();
}

std::string SnapshotService::snapshotJson() {
  std::lock_guard<std::mutex> lock(mutex_);
  const long long timestamp = nowMs();
  const Settings settings = config_.loadSettings();
  Totals totals = networkSampler_.readTotals();
  auto [downloadBps, uploadBps] = networkSampler_.calculateRates(totals, timestamp);
  auto connections = connectionSampler_.readConnections();
  auto summary = connectionSampler_.summarize(connections, downloadBps, uploadBps);
  auto remoteRows = remoteMonitor_.collect(connections, downloadBps, uploadBps);

  trackSshEvents(summary.sshConnections);
  persistUsage(timestamp, totals, downloadBps, uploadBps, settings);
  if (timestamp > highTrafficCooldownUntil_ &&
      (downloadBps > settings.highDownloadBps || uploadBps > settings.highUploadBps)) {
    highTrafficCooldownUntil_ = timestamp + 30000;
    logger_.event("warn", "流量", "检测到高流量峰值",
      "{\"downloadBps\":" + std::to_string(downloadBps) + ",\"uploadBps\":" + std::to_string(uploadBps) + "}");
  }

  std::sort(totals.interfaces.begin(), totals.interfaces.end(), [](const auto &a, const auto &b) {
    return a.rxBps + a.txBps > b.rxBps + b.txBps;
  });

  std::ostringstream out;
  out << "{"
      << "\"timestamp\":" << timestamp << ","
      << "\"time\":" << json::quote(isoTime(timestamp)) << ","
      << "\"platform\":{\"os\":" << json::quote(platformOs()) << ",\"hostname\":" << json::quote(hostname()) << "},"
      << "\"status\":{\"networkSource\":" << json::quote(totals.source)
      << ",\"connectionSource\":\"lsof\","
      << "\"rateMode\":" << json::quote(totals.source == "demo" ? "demo" : "system")
      << ",\"processRateMode\":\"estimated\","
      << "\"warnings\":" << json::stringArray(totals.warnings) << "},"
      << "\"totals\":{\"uploadBps\":" << uploadBps
      << ",\"downloadBps\":" << downloadBps
      << ",\"uploadedBytes\":" << totals.txBytes
      << ",\"downloadedBytes\":" << totals.rxBytes << "},"
      << "\"interfaces\":" << interfacesJson(totals.interfaces) << ","
      << "\"connections\":{\"total\":" << summary.total
      << ",\"established\":" << summary.established
      << ",\"listening\":" << summary.listening
      << ",\"ssh\":" << summary.ssh << "},"
      << "\"ssh\":{\"active\":" << summary.ssh << ",\"connections\":" << connectionsJson(summary.sshConnections) << "},"
      << "\"protocols\":" << protocolsJson(summary.protocols) << ","
      << "\"processes\":" << processesJson(summary.processes) << ","
      << "\"hosts\":" << hostsJson(summary.hosts) << ","
      << "\"remoteTargets\":" << remoteJson(remoteRows) << ","
      << "\"alerts\":" << alertsJson(totals, downloadBps, uploadBps, settings, summary, remoteRows) << ","
      << "\"events\":" << logger_.eventsJson(20) << ","
      << "\"logFile\":" << json::quote((config_.paths().dataDir / "traffic-monitor.log").string()) << ","
      << "\"historyFile\":" << json::quote((config_.paths().dataDir / "usage-history.jsonl").string()) << ","
      << "\"settings\":{\"highDownloadBps\":" << settings.highDownloadBps
      << ",\"highUploadBps\":" << settings.highUploadBps
      << ",\"watchedPorts\":" << json::stringArray(settings.watchedPorts)
      << ",\"watchedHosts\":" << json::stringArray(settings.watchedHosts)
      << ",\"blockedHosts\":" << json::stringArray(settings.blockedHosts)
      << "}"
      << "}";
  return out.str();
}

std::string SnapshotService::logsJson(size_t limit) {
  return "{\"logFile\":" + json::quote((config_.paths().dataDir / "traffic-monitor.log").string()) + ",\"events\":" + logger_.eventsJson(limit) + "}";
}

std::string SnapshotService::publicIpJson(bool forceRefresh) {
  return publicIpService_.ipInfoJson(config_.loadSettings(), forceRefresh);
}

std::string SnapshotService::setPublicIpConsent(const std::string &body) {
  const bool enabled = json::extractBool(body, "enabled", false);
  const Settings settings = config_.setPublicIpLookupEnabled(enabled);
  logger_.event("info", "公网 IP", enabled ? "已允许公网 IP 信息刷新" : "已关闭公网 IP 信息刷新",
    "{\"enabled\":" + std::string(enabled ? "true" : "false") + "}");
  return publicIpService_.ipInfoJson(settings, enabled);
}

std::string SnapshotService::targetsJson() const {
  const auto targets = config_.loadTargets();
  std::ostringstream out;
  out << "{\"targets\":[";
  for (size_t i = 0; i < targets.size(); ++i) {
    const auto &target = targets[i];
    if (i) out << ",";
    out << "{"
        << "\"id\":" << json::quote(target.id) << ","
        << "\"name\":" << json::quote(target.name) << ","
        << "\"host\":" << json::quote(target.host) << ","
        << "\"port\":" << target.port << ","
        << "\"protocol\":" << json::quote(target.protocol) << ","
        << "\"enabled\":" << (target.enabled ? "true" : "false")
        << "}";
  }
  out << "]}";
  return out.str();
}

std::string SnapshotService::addTarget(const std::string &body) {
  RemoteTarget target;
  target.name = json::extractString(body, "name", "未命名目标");
  target.host = json::extractString(body, "host");
  target.port = static_cast<int>(json::extractNumber(body, "port", 0));
  target.protocol = lower(json::extractString(body, "protocol", "tcp"));
  target.enabled = json::extractBool(body, "enabled", true);
  target.id = AppConfig::sanitizeId(json::extractString(body, "id", target.host + "-" + std::to_string(target.port) + "-" + std::to_string(nowMs())));
  if (target.host.empty() || target.port <= 0 || target.port >= 65536) return "{\"error\":\"invalid target\"}";

  auto targets = config_.loadTargets();
  targets.erase(std::remove_if(targets.begin(), targets.end(), [&](const auto &item) { return item.id == target.id; }), targets.end());
  targets.push_back(target);
  config_.saveTargets(targets);
  logger_.event("info", "远程监控", "新增远程目标 " + target.name,
    "{\"host\":" + json::quote(target.host) + ",\"port\":" + std::to_string(target.port) + "}");
  return "{\"ok\":true,\"target\":{\"id\":" + json::quote(target.id) + "}}";
}

std::string SnapshotService::removeTarget(const std::string &id) {
  auto targets = config_.loadTargets();
  targets.erase(std::remove_if(targets.begin(), targets.end(), [&](const auto &item) { return item.id == id; }), targets.end());
  config_.saveTargets(targets);
  logger_.event("info", "远程监控", "删除远程目标 " + id, "{\"id\":" + json::quote(id) + "}");
  return "{\"ok\":true}";
}

} // namespace tmon
