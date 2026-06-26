#include "ConnectionSampler.hpp"
#include "Util.hpp"

#include <algorithm>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace tmon {

ConnectionSampler::ConnectionSampler(CommandRunner commandRunner, Logger &logger)
  : commandRunner_(std::move(commandRunner)), logger_(logger) {}

Endpoint ConnectionSampler::parseEndpoint(const std::string &text) {
  std::string clean = trim(text);
  if (clean.empty() || clean == "*:*") return {"*", "*"};
  if (clean.front() == '[') {
    const size_t close = clean.find(']');
    if (close != std::string::npos) {
      std::string address = clean.substr(1, close - 1);
      std::string port = "*";
      if (close + 2 <= clean.size() && clean[close + 1] == ':') port = clean.substr(close + 2);
      return {address, port};
    }
  }
  const size_t colon = clean.rfind(':');
  if (colon == std::string::npos) return {clean, ""};
  return {clean.substr(0, colon), clean.substr(colon + 1)};
}

std::string ConnectionSampler::decodeCommandName(std::string command) {
  command = std::regex_replace(command, std::regex("\\\\x20"), " ");
  command = std::regex_replace(command, std::regex("\\\\x2d"), "-");
  return command;
}

std::string ConnectionSampler::serviceForPort(const std::string &port, const std::string &protocol) {
  static const std::map<std::string, std::string> services = {
    {"20", "FTP-DATA"}, {"21", "FTP"}, {"22", "SSH"}, {"25", "SMTP"},
    {"53", "DNS"}, {"80", "HTTP"}, {"110", "POP3"}, {"123", "NTP"},
    {"143", "IMAP"}, {"389", "LDAP"}, {"443", "HTTPS"}, {"465", "SMTPS"},
    {"587", "SMTP"}, {"993", "IMAPS"}, {"995", "POP3S"}, {"3306", "MySQL"},
    {"5000", "Dev"}, {"5353", "mDNS"}, {"5432", "Postgres"}, {"6379", "Redis"},
    {"7000", "Dev"}, {"8000", "HTTP-ALT"}, {"8080", "HTTP-ALT"},
    {"8443", "HTTPS-ALT"}, {"10000", "Service"}, {"23119", "Zotero"},
    {"33331", "Proxy"}
  };
  auto it = services.find(port);
  if (it != services.end()) return it->second;
  return protocol == "UDP" ? "UDP" : "TCP";
}

std::string ConnectionSampler::preferredPort(const Connection &connection) {
  if (connection.state == "LISTEN") return connection.local.port != "*" ? connection.local.port : connection.remote.port;
  if (!connection.remote.port.empty() && connection.remote.port != "*") return connection.remote.port;
  return connection.local.port;
}

Classification ConnectionSampler::classify(Connection &connection) {
  Classification cls;
  cls.port = preferredPort(connection);
  cls.service = serviceForPort(cls.port, connection.protocol);
  const std::string command = lower(connection.command);
  if (cls.port == "22" || command.find("ssh") != std::string::npos ||
      command.find("scp") != std::string::npos || command.find("sftp") != std::string::npos) {
    cls.service = "SSH";
  }
  cls.isSsh = cls.service == "SSH";
  cls.isExternal = !isLocalAddress(connection.remote.address) || connection.state == "LISTEN";
  return cls;
}

std::vector<Connection> ConnectionSampler::readConnections() {
  const std::string output = commandRunner_.run("lsof -nP -iTCP -iUDP");
  std::vector<Connection> connections;
  if (output.find("COMMAND") == std::string::npos) {
    logger_.warningOnce("lsof", "连接列表不可用，协议/端口/进程页会降级显示", "{\"command\":\"lsof -nP -iTCP -iUDP\"}");
    return connections;
  }

  std::istringstream lines(output);
  std::string line;
  bool header = true;
  std::regex rowRe(R"(^(\S+)\s+(\d+)\s+(\S+)\s+\S+\s+\S+\s+\S+\s+\S+\s+(\S+)\s+(.+)$)");
  while (std::getline(lines, line)) {
    if (header) {
      header = false;
      continue;
    }
    std::smatch match;
    if (!std::regex_match(line, match, rowRe)) continue;
    Connection connection;
    connection.command = decodeCommandName(match[1].str());
    connection.pid = static_cast<int>(parseDouble(match[2].str()));
    connection.user = match[3].str();
    connection.protocol = match[4].str();
    connection.raw = trim(match[5].str());
    if (connection.protocol != "TCP" && connection.protocol != "UDP") continue;

    std::string endpointText = connection.raw;
    std::smatch stateMatch;
    std::regex stateRe(R"(\s*\(([^)]+)\)\s*$)");
    if (std::regex_search(endpointText, stateMatch, stateRe)) {
      connection.state = stateMatch[1].str();
      endpointText = std::regex_replace(endpointText, stateRe, "");
    } else {
      connection.state = connection.protocol == "UDP" ? "OPEN" : "UNKNOWN";
    }

    const size_t arrow = endpointText.find("->");
    if (arrow == std::string::npos) {
      connection.local = parseEndpoint(endpointText);
      connection.remote = {"*", "*"};
    } else {
      connection.local = parseEndpoint(endpointText.substr(0, arrow));
      connection.remote = parseEndpoint(endpointText.substr(arrow + 2));
    }
    connection.id = std::to_string(connection.pid) + "-" + connection.protocol + "-" + endpointText + "-" + connection.state;
    connection.classification = classify(connection);
    connections.push_back(connection);
  }
  return connections;
}

double ConnectionSampler::connectionWeight(const Connection &connection) {
  if (connection.state == "LISTEN") return 0.25;
  if (connection.state == "CLOSED" || connection.state == "CLOSE_WAIT") return 0.15;
  if (connection.state != "ESTABLISHED" && connection.protocol == "TCP") return 0.45;
  double weight = 1.0;
  if (connection.classification.isSsh) weight += 1.1;
  if (connection.classification.service == "HTTPS") weight += 0.35;
  if (isLocalAddress(connection.remote.address)) weight *= 0.25;
  return weight;
}

bool ConnectionSampler::targetMatchesConnection(const RemoteTarget &target, const Connection &connection) {
  const std::string port = std::to_string(target.port);
  const std::string host = lower(target.host);
  const bool portMatch = connection.local.port == port || connection.remote.port == port;
  if (!portMatch) return false;
  const std::string local = lower(connection.local.address);
  const std::string remote = lower(connection.remote.address);
  return local == host || remote == host || (host == "localhost" && local == "127.0.0.1");
}

template <typename T>
std::vector<std::string> firstValues(const std::set<T> &values, size_t limit) {
  std::vector<std::string> result;
  for (const auto &value : values) {
    if (result.size() >= limit) break;
    result.push_back(value);
  }
  return result;
}

ConnectionSummary ConnectionSampler::summarize(const std::vector<Connection> &connections, double downloadBps, double uploadBps) const {
  struct ProtocolAgg {
    ProtocolRow row;
    std::set<std::string> processes;
    std::set<std::string> remoteHosts;
    double weight = 0;
  };
  struct ProcessAgg {
    ProcessRow row;
    std::set<std::string> ports;
    std::set<std::string> remoteHosts;
    double weight = 0;
  };
  struct HostAgg {
    HostRow row;
    std::set<std::string> services;
    std::set<std::string> ports;
    std::set<std::string> processes;
    double weight = 0;
  };

  std::map<std::string, ProtocolAgg> protocolMap;
  std::map<std::string, ProcessAgg> processMap;
  std::map<std::string, HostAgg> hostMap;
  double totalWeight = 0;
  ConnectionSummary summary;
  summary.total = static_cast<int>(connections.size());

  for (const auto &connection : connections) {
    const double weight = connectionWeight(connection);
    totalWeight += weight;
    summary.established += connection.state == "ESTABLISHED" ? 1 : 0;
    summary.listening += connection.state == "LISTEN" ? 1 : 0;
    if (connection.classification.isSsh) {
      summary.ssh += 1;
      summary.sshConnections.push_back(connection);
    }

    const std::string protocolKey = connection.protocol + ":" + connection.classification.service + ":" + connection.classification.port;
    auto &protocol = protocolMap[protocolKey];
    protocol.row.key = protocolKey;
    protocol.row.protocol = connection.protocol;
    protocol.row.service = connection.classification.service;
    protocol.row.port = connection.classification.port.empty() ? "any" : connection.classification.port;
    protocol.row.ssh = connection.classification.isSsh;
    protocol.row.connections += 1;
    protocol.row.established += connection.state == "ESTABLISHED" ? 1 : 0;
    protocol.row.listening += connection.state == "LISTEN" ? 1 : 0;
    protocol.processes.insert(connection.command);
    if (!connection.remote.address.empty()) protocol.remoteHosts.insert(connection.remote.address);
    protocol.weight += weight;

    const std::string processKey = connection.command + ":" + std::to_string(connection.pid);
    auto &process = processMap[processKey];
    process.row.key = processKey;
    process.row.command = connection.command;
    process.row.pid = connection.pid;
    process.row.connections += 1;
    process.row.established += connection.state == "ESTABLISHED" ? 1 : 0;
    process.row.listening += connection.state == "LISTEN" ? 1 : 0;
    process.row.udp += connection.protocol == "UDP" ? 1 : 0;
    process.row.ssh += connection.classification.isSsh ? 1 : 0;
    if (!connection.classification.port.empty()) process.ports.insert(connection.classification.port);
    if (!connection.remote.address.empty()) process.remoteHosts.insert(connection.remote.address);
    process.weight += weight;

    const std::string hostAddress = connection.remote.address.empty() ? connection.local.address : connection.remote.address;
    if (!hostAddress.empty() && hostAddress != "*") {
      auto &host = hostMap[lower(hostAddress)];
      host.row.host = hostAddress;
      host.row.connections += 1;
      host.row.established += connection.state == "ESTABLISHED" ? 1 : 0;
      host.row.external = !isLocalAddress(hostAddress);
      host.services.insert(connection.classification.service);
      if (!connection.classification.port.empty()) host.ports.insert(connection.classification.port);
      host.processes.insert(connection.command);
      host.weight += weight;
    }
  }

  auto distribute = [&](double weight) {
    const double share = totalWeight <= 0 ? 0 : weight / totalWeight;
    return std::pair<double, double>{downloadBps * share, uploadBps * share};
  };

  for (auto &[_, agg] : protocolMap) {
    auto [down, up] = distribute(agg.weight);
    agg.row.downloadBps = down;
    agg.row.uploadBps = up;
    agg.row.processes = firstValues(agg.processes, 8);
    agg.row.remoteHosts = firstValues(agg.remoteHosts, 8);
    summary.protocols.push_back(agg.row);
  }
  std::sort(summary.protocols.begin(), summary.protocols.end(), [](const auto &a, const auto &b) {
    return (a.downloadBps + a.uploadBps) > (b.downloadBps + b.uploadBps);
  });
  if (summary.protocols.size() > 40) summary.protocols.resize(40);

  for (auto &[_, agg] : processMap) {
    auto [down, up] = distribute(agg.weight);
    agg.row.downloadBps = down;
    agg.row.uploadBps = up;
    agg.row.ports = firstValues(agg.ports, 10);
    agg.row.remoteHosts = firstValues(agg.remoteHosts, 8);
    summary.processes.push_back(agg.row);
  }
  std::sort(summary.processes.begin(), summary.processes.end(), [](const auto &a, const auto &b) {
    return (a.downloadBps + a.uploadBps) > (b.downloadBps + b.uploadBps);
  });
  if (summary.processes.size() > 60) summary.processes.resize(60);

  for (auto &[_, agg] : hostMap) {
    auto [down, up] = distribute(agg.weight);
    agg.row.downloadBps = down;
    agg.row.uploadBps = up;
    agg.row.services = firstValues(agg.services, 8);
    agg.row.ports = firstValues(agg.ports, 10);
    agg.row.processes = firstValues(agg.processes, 8);
    summary.hosts.push_back(agg.row);
  }
  std::sort(summary.hosts.begin(), summary.hosts.end(), [](const auto &a, const auto &b) {
    return (a.downloadBps + a.uploadBps) > (b.downloadBps + b.uploadBps);
  });
  if (summary.hosts.size() > 50) summary.hosts.resize(50);

  return summary;
}

} // namespace tmon
