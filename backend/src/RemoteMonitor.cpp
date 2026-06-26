#include "RemoteMonitor.hpp"
#include "JsonUtil.hpp"
#include "Util.hpp"

#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace tmon {

RemoteMonitor::RemoteMonitor(const AppConfig &config, Logger &logger) : config_(config), logger_(logger) {}

RemoteMonitor::Probe RemoteMonitor::probeTarget(const RemoteTarget &target) const {
  Probe status;
  const long long started = nowMs();
  status.checkedAt = started;

  addrinfo hints {};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  addrinfo *result = nullptr;
  const int gai = getaddrinfo(target.host.c_str(), std::to_string(target.port).c_str(), &hints, &result);
  if (gai != 0) {
    status.status = "down";
    status.error = gai_strerror(gai);
    status.latencyMs = static_cast<int>(nowMs() - started);
    return status;
  }

  for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
    int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0) continue;
    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 900000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      close(fd);
      freeaddrinfo(result);
      status.status = "up";
      status.latencyMs = static_cast<int>(nowMs() - started);
      return status;
    }
    status.error = std::strerror(errno);
    close(fd);
  }

  freeaddrinfo(result);
  status.status = "down";
  status.latencyMs = static_cast<int>(nowMs() - started);
  return status;
}

std::vector<RemoteRow> RemoteMonitor::collect(const std::vector<Connection> &connections, double downloadBps, double uploadBps) {
  std::vector<RemoteRow> rows;
  const auto targets = config_.loadTargets();
  double allWeight = 1.0;
  for (const auto &connection : connections) allWeight += ConnectionSampler::connectionWeight(connection);

  for (const auto &target : targets) {
    Probe probe;
    const long long current = nowMs();
    if (!target.enabled) {
      probe.status = "disabled";
      probe.checkedAt = current;
    } else if (!cache_.count(target.id) || current - cache_[target.id].checkedAt > 5000) {
      probe = probeTarget(target);
      cache_[target.id] = probe;
    } else {
      probe = cache_[target.id];
    }

    if (target.enabled) {
      const auto previous = previousStatus_[target.id];
      if (!previous.empty() && previous != probe.status) {
        logger_.event(probe.status == "up" ? "info" : "warn", "远程监控", target.name + " 状态变为 " + probe.status,
          "{\"host\":" + json::quote(target.host) + ",\"port\":" + std::to_string(target.port) + ",\"error\":" + json::quote(probe.error) + "}");
      }
      previousStatus_[target.id] = probe.status;
    }

    int matched = 0;
    double weight = 0;
    for (const auto &connection : connections) {
      if (ConnectionSampler::targetMatchesConnection(target, connection)) {
        matched += 1;
        weight += ConnectionSampler::connectionWeight(connection);
      }
    }

    RemoteRow row;
    static_cast<RemoteTarget &>(row) = target;
    row.status = probe.status;
    row.latencyMs = probe.latencyMs;
    row.error = probe.error;
    row.checkedAt = probe.checkedAt;
    row.connections = matched;
    row.downloadBps = downloadBps * (weight / allWeight);
    row.uploadBps = uploadBps * (weight / allWeight);
    rows.push_back(row);
  }

  return rows;
}

} // namespace tmon
