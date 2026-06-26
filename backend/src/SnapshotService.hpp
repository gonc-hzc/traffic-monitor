#pragma once

#include "AppConfig.hpp"
#include "ConnectionSampler.hpp"
#include "Logger.hpp"
#include "NetworkSampler.hpp"
#include "PublicIpService.hpp"
#include "RemoteMonitor.hpp"

#include <mutex>
#include <set>

namespace tmon {

class SnapshotService {
public:
  SnapshotService(AppConfig &config, Logger &logger);

  std::string snapshotJson();
  std::string logsJson(size_t limit);
  std::string publicIpJson(bool forceRefresh);
  std::string setPublicIpConsent(const std::string &body);
  std::string targetsJson() const;
  std::string addTarget(const std::string &body);
  std::string removeTarget(const std::string &id);

private:
  void trackSshEvents(const std::vector<Connection> &sshConnections);
  void persistUsage(long long timestamp, const Totals &totals, double downloadBps, double uploadBps, const Settings &settings);
  std::string alertsJson(const Totals &totals, double downloadBps, double uploadBps, const Settings &settings,
                         const ConnectionSummary &summary, const std::vector<RemoteRow> &remoteRows) const;

  AppConfig &config_;
  Logger &logger_;
  NetworkSampler networkSampler_;
  ConnectionSampler connectionSampler_;
  RemoteMonitor remoteMonitor_;
  PublicIpService publicIpService_;
  mutable std::mutex mutex_;
  std::set<std::string> previousSsh_;
  long long lastHistoryPersistedAt_ = 0;
  long long highTrafficCooldownUntil_ = 0;
};

} // namespace tmon
