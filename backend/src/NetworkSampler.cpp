#include "NetworkSampler.hpp"
#include "Util.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>

namespace tmon {

NetworkSampler::NetworkSampler(CommandRunner commandRunner, Logger &logger)
  : commandRunner_(std::move(commandRunner)), logger_(logger) {}

bool NetworkSampler::excludedInterface(const std::string &name) const {
  static const std::regex re("^(lo|awdl|llw|bridge|gif|stf|ap)[0-9]*", std::regex_constants::icase);
  return std::regex_search(name, re);
}

Totals NetworkSampler::readTotals() {
#if defined(__linux__)
  std::ifstream proc("/proc/net/dev");
  if (proc) {
    std::string line;
    int index = 0;
    Totals totals;
    totals.source = "procfs";
    while (std::getline(proc, line)) {
      if (index++ < 2) continue;
      const size_t colon = line.find(':');
      if (colon == std::string::npos) continue;
      const std::string name = trim(line.substr(0, colon));
      if (excludedInterface(name)) continue;
      auto fields = splitWhitespace(line.substr(colon + 1));
      if (fields.size() < 16) continue;
      InterfaceStats iface;
      iface.name = name;
      iface.rxBytes = parseDouble(fields[0]);
      iface.txBytes = parseDouble(fields[8]);
      totals.rxBytes += iface.rxBytes;
      totals.txBytes += iface.txBytes;
      totals.interfaces.push_back(iface);
    }
    if (!totals.interfaces.empty()) return totals;
  }
#endif

  std::map<std::string, InterfaceStats> byName;
  std::istringstream lines(commandRunner_.run("netstat -ibn"));
  std::string line;
  while (std::getline(lines, line)) {
    if (line.empty() || line.rfind("Name", 0) == 0 || line.rfind("netstat:", 0) == 0) continue;
    auto parts = splitWhitespace(line);
    if (parts.size() < 10) continue;
    const std::string name = parts[0];
    if (excludedInterface(name)) continue;
    const double rxBytes = parseDouble(parts[6]);
    const double txBytes = parseDouble(parts[9]);
    if (rxBytes <= 0 && txBytes <= 0) continue;
    auto &iface = byName[name];
    iface.name = name;
    iface.rxBytes = std::max(iface.rxBytes, rxBytes);
    iface.txBytes = std::max(iface.txBytes, txBytes);
  }

  Totals totals;
  if (!byName.empty()) {
    totals.source = "netstat";
    for (const auto &entry : byName) {
      totals.rxBytes += entry.second.rxBytes;
      totals.txBytes += entry.second.txBytes;
      totals.interfaces.push_back(entry.second);
    }
    return totals;
  }

  logger_.warningOnce("netstat", "系统接口字节计数不可用，已启用模拟采样", "{\"command\":\"netstat -ibn\"}");
  demoTick_ += 1;
  const double phase = demoTick_ / 5.0;
  const double rxRate = 850000 + std::max(0.0, std::sin(phase) * 1900000) + (std::rand() % 520000);
  const double txRate = 70000 + std::max(0.0, std::cos(phase * 0.7) * 260000) + (std::rand() % 80000);
  demoRx_ += rxRate;
  demoTx_ += txRate;
  InterfaceStats iface;
  iface.name = "demo0";
  iface.rxBytes = demoRx_;
  iface.txBytes = demoTx_;
  totals.source = "demo";
  totals.rxBytes = demoRx_;
  totals.txBytes = demoTx_;
  totals.interfaces.push_back(iface);
  totals.warnings.push_back("system-network-counters-unavailable");
  return totals;
}

std::pair<double, double> NetworkSampler::calculateRates(Totals &totals, long long timestamp) {
  double downloadBps = 0;
  double uploadBps = 0;
  double interval = 1.0;
  if (previousValid_ && previousSource_ == totals.source) {
    interval = std::max(0.001, (timestamp - previousTs_) / 1000.0);
    downloadBps = std::max(0.0, (totals.rxBytes - previousRx_) / interval);
    uploadBps = std::max(0.0, (totals.txBytes - previousTx_) / interval);
  }
  for (auto &iface : totals.interfaces) {
    auto previous = previousInterfaces_.find(iface.name);
    if (previous != previousInterfaces_.end()) {
      iface.rxBps = std::max(0.0, (iface.rxBytes - previous->second.rxBytes) / interval);
      iface.txBps = std::max(0.0, (iface.txBytes - previous->second.txBytes) / interval);
    }
  }
  previousValid_ = true;
  previousSource_ = totals.source;
  previousRx_ = totals.rxBytes;
  previousTx_ = totals.txBytes;
  previousTs_ = timestamp;
  previousInterfaces_.clear();
  for (const auto &iface : totals.interfaces) previousInterfaces_[iface.name] = iface;
  return {downloadBps, uploadBps};
}

} // namespace tmon
