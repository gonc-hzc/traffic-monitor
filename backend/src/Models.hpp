#pragma once

#include <map>
#include <string>
#include <vector>

namespace tmon {

struct InterfaceStats {
  std::string name;
  double rxBytes = 0;
  double txBytes = 0;
  double rxBps = 0;
  double txBps = 0;
};

struct Totals {
  std::string source = "demo";
  double rxBytes = 0;
  double txBytes = 0;
  std::vector<InterfaceStats> interfaces;
  std::vector<std::string> warnings;
};

struct Endpoint {
  std::string address = "*";
  std::string port = "*";
};

struct Classification {
  std::string port;
  std::string service;
  bool isSsh = false;
  bool isExternal = false;
};

struct Connection {
  std::string id;
  std::string command;
  int pid = 0;
  std::string user;
  std::string protocol;
  Endpoint local;
  Endpoint remote;
  std::string state;
  std::string raw;
  Classification classification;
};

struct RemoteTarget {
  std::string id;
  std::string name;
  std::string host;
  int port = 0;
  std::string protocol = "tcp";
  bool enabled = true;
};

struct RemoteRow : RemoteTarget {
  std::string status = "unknown";
  int latencyMs = 0;
  std::string error;
  long long checkedAt = 0;
  int connections = 0;
  double downloadBps = 0;
  double uploadBps = 0;
};

struct Event {
  std::string id;
  long long ts = 0;
  std::string time;
  std::string level;
  std::string type;
  std::string message;
  std::string detailJson = "{}";
};

struct Settings {
  double highDownloadBps = 8 * 1024 * 1024;
  double highUploadBps = 4 * 1024 * 1024;
  std::vector<std::string> watchedPorts = {"22"};
  std::vector<std::string> watchedHosts;
  std::vector<std::string> blockedHosts;
  long long sampleHistoryEveryMs = 60000;
  bool allowPublicIpLookup = false;
  long long publicIpRefreshIntervalMs = 300000;
};

struct ProtocolRow {
  std::string key;
  std::string protocol;
  std::string service;
  std::string port;
  int connections = 0;
  int established = 0;
  int listening = 0;
  bool ssh = false;
  std::vector<std::string> processes;
  std::vector<std::string> remoteHosts;
  double downloadBps = 0;
  double uploadBps = 0;
};

struct ProcessRow {
  std::string key;
  std::string command;
  int pid = 0;
  int connections = 0;
  int established = 0;
  int listening = 0;
  int udp = 0;
  int ssh = 0;
  std::vector<std::string> ports;
  std::vector<std::string> remoteHosts;
  double downloadBps = 0;
  double uploadBps = 0;
};

struct HostRow {
  std::string host;
  int connections = 0;
  int established = 0;
  std::vector<std::string> services;
  std::vector<std::string> ports;
  std::vector<std::string> processes;
  bool external = false;
  double downloadBps = 0;
  double uploadBps = 0;
};

struct ConnectionSummary {
  std::vector<ProtocolRow> protocols;
  std::vector<ProcessRow> processes;
  std::vector<HostRow> hosts;
  std::vector<Connection> sshConnections;
  int total = 0;
  int established = 0;
  int listening = 0;
  int ssh = 0;
};

} // namespace tmon
