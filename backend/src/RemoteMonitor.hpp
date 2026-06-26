#pragma once

#include "AppConfig.hpp"
#include "ConnectionSampler.hpp"
#include "Logger.hpp"
#include "Models.hpp"

#include <map>

namespace tmon {

class RemoteMonitor {
public:
  RemoteMonitor(const AppConfig &config, Logger &logger);

  std::vector<RemoteRow> collect(const std::vector<Connection> &connections, double downloadBps, double uploadBps);

private:
  struct Probe {
    std::string status = "unknown";
    int latencyMs = 0;
    std::string error;
    long long checkedAt = 0;
  };

  Probe probeTarget(const RemoteTarget &target) const;

  const AppConfig &config_;
  Logger &logger_;
  std::map<std::string, Probe> cache_;
  std::map<std::string, std::string> previousStatus_;
};

} // namespace tmon
