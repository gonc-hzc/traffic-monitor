#pragma once

#include "CommandRunner.hpp"
#include "Logger.hpp"
#include "Models.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace tmon {

class PublicIpService {
public:
  explicit PublicIpService(Logger &logger);

  std::string ipInfoJson(const Settings &settings, bool forceRefresh);

private:
  struct OutletCache {
    std::string id;
    std::string label;
    std::string path;
    std::string cachedBody = "{}";
    std::string cachedProvider;
    std::string proxyUrl;
    std::string lastError;
    long long fetchedAt = 0;
    long long lastAttemptAt = 0;
  };

  bool shouldRefresh(const OutletCache &cache, long long now, const Settings &settings, bool forceRefresh) const;
  bool looksLikeJsonObject(const std::string &value) const;
  void fetchIpInfo(long long now, const Settings &settings, bool forceRefresh);
  bool fetchOutlet(OutletCache &cache, const std::string &curlRouteArgs, long long now, bool forceRefresh);
  bool fetchProxyOutlet(long long now, bool forceRefresh);
  std::vector<std::string> proxyCandidates() const;
  bool localPortOpen(int port) const;
  std::string routeJson(const OutletCache &cache, long long now, const Settings &settings) const;
  std::string statusJson(long long now, const Settings &settings) const;

  Logger &logger_;
  CommandRunner commandRunner_;
  mutable std::mutex mutex_;
  OutletCache direct_ {"direct", "直连出口", "绕过系统代理", "{}", "", "", "", 0, 0};
  OutletCache proxy_ {"proxy", "代理/VPN 出口", "系统代理或本地代理", "{}", "", "", "", 0, 0};
  long long retryCooldownMs_ = 120000;
  long long forceCooldownMs_ = 15000;
};

} // namespace tmon
