#include "PublicIpService.hpp"
#include "JsonUtil.hpp"
#include "Util.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <regex>
#include <set>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace tmon {

namespace {

struct Provider {
  std::string name;
  std::string url;
  std::string kind;
};

struct CurlResult {
  int status = 0;
  std::string body;
  std::string error;
};

std::string numberOrNull(const std::string &body, const std::string &key) {
  std::regex re("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
  std::smatch match;
  return std::regex_search(body, match, re) ? match[1].str() : "null";
}

std::string nestedString(const std::string &body, const std::string &objectKey, const std::string &fieldKey, const std::string &fallback = "") {
  std::regex objectRe("\"" + objectKey + "\"\\s*:\\s*\\{([^\\}]*)\\}");
  std::smatch objectMatch;
  if (!std::regex_search(body, objectMatch, objectRe)) return fallback;
  return json::extractString(objectMatch[1].str(), fieldKey, fallback);
}

std::string nestedNumberAsString(const std::string &body, const std::string &objectKey, const std::string &fieldKey) {
  std::regex objectRe("\"" + objectKey + "\"\\s*:\\s*\\{([^\\}]*)\\}");
  std::smatch objectMatch;
  if (!std::regex_search(body, objectMatch, objectRe)) return "";
  const std::string objectBody = objectMatch[1].str();
  std::regex valueRe("\"" + fieldKey + "\"\\s*:\\s*(-?[0-9]+)");
  std::smatch valueMatch;
  return std::regex_search(objectBody, valueMatch, valueRe) ? valueMatch[1].str() : "";
}

std::string asnFromOrg(const std::string &org) {
  std::regex re("\\bAS([0-9]+)\\b");
  std::smatch match;
  return std::regex_search(org, match, re) ? "AS" + match[1].str() : "";
}

std::string normalize(const std::string &provider, const std::string &kind, const std::string &body) {
  const std::string ip = json::extractString(body, "ip");
  if (ip.empty()) return "";

  std::string countryName;
  std::string countryCode;
  std::string region = json::extractString(body, "region");
  std::string city = json::extractString(body, "city");
  std::string timezone;
  std::string asn;
  std::string org;
  std::string latitude = numberOrNull(body, "latitude");
  std::string longitude = numberOrNull(body, "longitude");

  if (kind == "ipapi") {
    countryName = json::extractString(body, "country_name", json::extractString(body, "country"));
    countryCode = json::extractString(body, "country_code");
    timezone = json::extractString(body, "timezone");
    asn = json::extractString(body, "asn");
    org = json::extractString(body, "org");
  } else if (kind == "ipwho") {
    if (body.find("\"success\":false") != std::string::npos) return "";
    countryName = json::extractString(body, "country");
    countryCode = json::extractString(body, "country_code");
    timezone = nestedString(body, "timezone", "id");
    const std::string asnNumber = nestedNumberAsString(body, "connection", "asn");
    asn = asnNumber.empty() ? "" : "AS" + asnNumber;
    org = nestedString(body, "connection", "org", nestedString(body, "connection", "isp"));
  } else if (kind == "ipinfo") {
    countryCode = json::extractString(body, "country");
    countryName = countryCode;
    timezone = json::extractString(body, "timezone");
    org = json::extractString(body, "org");
    asn = asnFromOrg(org);
    const std::string loc = json::extractString(body, "loc");
    const size_t comma = loc.find(',');
    if (comma != std::string::npos) {
      latitude = loc.substr(0, comma);
      longitude = loc.substr(comma + 1);
    }
  }

  if (countryName.empty()) countryName = countryCode;
  std::ostringstream out;
  out << "{"
      << "\"provider\":" << json::quote(provider) << ","
      << "\"ip\":" << json::quote(ip) << ","
      << "\"country_name\":" << json::quote(countryName) << ","
      << "\"country\":" << json::quote(countryName) << ","
      << "\"country_code\":" << json::quote(countryCode) << ","
      << "\"region\":" << json::quote(region) << ","
      << "\"city\":" << json::quote(city) << ","
      << "\"latitude\":" << latitude << ","
      << "\"longitude\":" << longitude << ","
      << "\"timezone\":" << json::quote(timezone) << ","
      << "\"asn\":" << json::quote(asn) << ","
      << "\"org\":" << json::quote(org)
      << "}";
  return out.str();
}

CurlResult parseCurlResult(const std::string &rawOutput) {
  CurlResult result;
  const std::string marker = "__TM_HTTP_STATUS:";
  const size_t markerPos = rawOutput.rfind(marker);
  if (markerPos == std::string::npos) {
    result.body = trim(rawOutput);
    result.error = result.body.empty() ? "curl request failed" : result.body.substr(0, 220);
    return result;
  }

  result.body = trim(rawOutput.substr(0, markerPos));
  result.status = static_cast<int>(parseDouble(trim(rawOutput.substr(markerPos + marker.size()))));
  if (result.status < 200 || result.status >= 300) {
    result.error = "HTTP " + std::to_string(result.status);
  }
  return result;
}

std::string compactError(const Provider &provider, const CurlResult &result) {
  if (!result.error.empty()) return provider.name + ": " + result.error;
  if (!result.body.empty()) return provider.name + ": " + result.body.substr(0, 120);
  return provider.name + ": no response";
}

std::string shellQuote(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') quoted += "'\\''";
    else quoted.push_back(c);
  }
  quoted += "'";
  return quoted;
}

std::string extractProxyValue(const std::string &raw, const std::string &key) {
  std::regex re(key + "\\s*:\\s*([^\\n]+)");
  std::smatch match;
  return std::regex_search(raw, match, re) ? trim(match[1].str()) : "";
}

bool proxyEnabled(const std::string &raw, const std::string &key) {
  return extractProxyValue(raw, key) == "1";
}

} // namespace

PublicIpService::PublicIpService(Logger &logger) : logger_(logger) {}

bool PublicIpService::shouldRefresh(const OutletCache &cache, long long now, const Settings &settings, bool forceRefresh) const {
  if (forceRefresh) return cache.lastAttemptAt == 0 || now - cache.lastAttemptAt >= forceCooldownMs_;
  if (cache.lastAttemptAt > 0 && now - cache.lastAttemptAt < retryCooldownMs_) return false;
  if (cache.fetchedAt == 0) return true;
  return now - cache.fetchedAt >= settings.publicIpRefreshIntervalMs;
}

bool PublicIpService::looksLikeJsonObject(const std::string &value) const {
  const std::string cleaned = trim(value);
  return cleaned.size() >= 2 && cleaned.front() == '{' && cleaned.back() == '}';
}

bool PublicIpService::fetchOutlet(OutletCache &cache, const std::string &curlRouteArgs, long long now, bool forceRefresh) {
  cache.lastAttemptAt = now;
  const std::vector<Provider> providers = {
    {"ipapi.co", "https://ipapi.co/json/", "ipapi"},
    {"ipwho.is", "https://ipwho.is/", "ipwho"},
    {"ipinfo.io", "https://ipinfo.io/json", "ipinfo"}
  };

  std::vector<std::string> errors;
  for (const auto &provider : providers) {
    const std::string command = "/usr/bin/curl -sS --connect-timeout 3 --max-time 7 "
      "-A 'TrafficMonitor/0.2' -H 'Accept: application/json' "
      "-w '\\n__TM_HTTP_STATUS:%{http_code}' " + curlRouteArgs + " " + shellQuote(provider.url);
    const CurlResult result = parseCurlResult(commandRunner_.run(command));
    const std::string normalized = looksLikeJsonObject(result.body) && result.status >= 200 && result.status < 300
      ? normalize(provider.name, provider.kind, result.body)
      : "";

    if (!normalized.empty()) {
      cache.cachedBody = normalized;
      cache.cachedProvider = provider.name;
      cache.fetchedAt = now;
      cache.lastError.clear();
      logger_.event("info", "公网 IP", (forceRefresh ? "已手动刷新 " : "已刷新 ") + cache.label,
        "{\"source\":" + json::quote(provider.name) + ",\"route\":" + json::quote(cache.id) + "}");
      return true;
    }

    errors.push_back(compactError(provider, result));
  }

  std::ostringstream errorText;
  for (size_t i = 0; i < errors.size(); ++i) {
    if (i) errorText << "; ";
    errorText << errors[i];
  }
  cache.lastError = errorText.str();
  logger_.event("warn", "公网 IP", cache.label + " 刷新失败，将稍后重试",
    "{\"error\":" + json::quote(cache.lastError) + ",\"route\":" + json::quote(cache.id) + "}");
  return false;
}

bool PublicIpService::localPortOpen(int port) const {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return false;
  sockaddr_in address {};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  const bool ok = connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
  close(fd);
  return ok;
}

std::vector<std::string> PublicIpService::proxyCandidates() const {
  std::vector<std::string> candidates;
  std::set<std::string> seen;
  auto add = [&](const std::string &url) {
    if (!url.empty() && !seen.count(url)) {
      seen.insert(url);
      candidates.push_back(url);
    }
  };

  const std::string systemProxy = commandRunner_.run("/usr/sbin/scutil --proxy");
  if (proxyEnabled(systemProxy, "HTTPEnable")) {
    const std::string host = extractProxyValue(systemProxy, "HTTPProxy");
    const std::string port = extractProxyValue(systemProxy, "HTTPPort");
    if (!host.empty() && !port.empty()) add("http://" + host + ":" + port);
  }
  if (proxyEnabled(systemProxy, "HTTPSEnable")) {
    const std::string host = extractProxyValue(systemProxy, "HTTPSProxy");
    const std::string port = extractProxyValue(systemProxy, "HTTPSPort");
    if (!host.empty() && !port.empty()) add("http://" + host + ":" + port);
  }
  if (proxyEnabled(systemProxy, "SOCKSEnable")) {
    const std::string host = extractProxyValue(systemProxy, "SOCKSProxy");
    const std::string port = extractProxyValue(systemProxy, "SOCKSPort");
    if (!host.empty() && !port.empty()) add("socks5h://" + host + ":" + port);
  }

  const std::vector<std::pair<int, std::string>> localPorts = {
    {7890, "http"}, {7897, "http"}, {7899, "http"}, {6152, "http"},
    {7891, "socks5h"}, {1080, "socks5h"}, {1086, "socks5h"}, {6153, "socks5h"}
  };
  for (const auto &[port, scheme] : localPorts) {
    if (localPortOpen(port)) add(scheme + "://127.0.0.1:" + std::to_string(port));
  }
  return candidates;
}

bool PublicIpService::fetchProxyOutlet(long long now, bool forceRefresh) {
  proxy_.lastAttemptAt = now;
  const auto candidates = proxyCandidates();
  if (candidates.empty()) {
    proxy_.proxyUrl.clear();
    proxy_.lastError = "未检测到系统代理或常见本地代理端口";
    logger_.event("info", "公网 IP", "未检测到代理出口", "{\"route\":\"proxy\"}");
    return false;
  }

  std::vector<std::string> errors;
  for (const auto &proxyUrl : candidates) {
    proxy_.proxyUrl = proxyUrl;
    if (fetchOutlet(proxy_, "-x " + shellQuote(proxyUrl), now, forceRefresh)) return true;
    errors.push_back(proxyUrl + ": " + proxy_.lastError);
  }
  std::ostringstream out;
  for (size_t i = 0; i < errors.size(); ++i) {
    if (i) out << "; ";
    out << errors[i];
  }
  proxy_.lastError = out.str();
  return false;
}

void PublicIpService::fetchIpInfo(long long now, const Settings &settings, bool forceRefresh) {
  if (shouldRefresh(direct_, now, settings, forceRefresh)) {
    fetchOutlet(direct_, "--noproxy '*'", now, forceRefresh);
  }
  if (shouldRefresh(proxy_, now, settings, forceRefresh)) {
    fetchProxyOutlet(now, forceRefresh);
  }
}

std::string PublicIpService::routeJson(const OutletCache &cache, long long now, const Settings &settings) const {
  const bool enabled = settings.allowPublicIpLookup;
  const long long ageMs = cache.fetchedAt > 0 ? now - cache.fetchedAt : 0;
  const long long regularNextRefreshAt = cache.fetchedAt > 0 ? cache.fetchedAt + settings.publicIpRefreshIntervalMs : now;
  const long long retryAt = cache.lastAttemptAt > 0 ? cache.lastAttemptAt + retryCooldownMs_ : now;
  const long long nextRefreshAt = cache.lastError.empty() && cache.fetchedAt > 0 ? regularNextRefreshAt : std::max(regularNextRefreshAt, retryAt);
  std::ostringstream out;
  out << "{"
      << "\"id\":" << json::quote(cache.id) << ","
      << "\"label\":" << json::quote(cache.label) << ","
      << "\"path\":" << json::quote(cache.path) << ","
      << "\"ok\":" << (enabled && cache.fetchedAt > 0 && cache.lastError.empty() ? "true" : "false") << ","
      << "\"source\":" << json::quote(enabled ? cache.cachedProvider : "") << ","
      << "\"proxyUrl\":" << json::quote(enabled ? cache.proxyUrl : "") << ","
      << "\"fetchedAt\":" << (enabled ? cache.fetchedAt : 0) << ","
      << "\"lastAttemptAt\":" << (enabled ? cache.lastAttemptAt : 0) << ","
      << "\"ageMs\":" << (enabled ? ageMs : 0) << ","
      << "\"nextRefreshAt\":" << (enabled ? nextRefreshAt : now) << ","
      << "\"error\":" << json::quote(enabled ? cache.lastError : "") << ","
      << "\"data\":" << (enabled ? cache.cachedBody : "{}")
      << "}";
  return out.str();
}

std::string PublicIpService::statusJson(long long now, const Settings &settings) const {
  const bool directOk = direct_.fetchedAt > 0 && direct_.lastError.empty();
  const bool proxyOk = proxy_.fetchedAt > 0 && proxy_.lastError.empty();
  const OutletCache &primary = direct_.fetchedAt > 0 ? direct_ : proxy_;
  const long long fetchedAt = std::max(direct_.fetchedAt, proxy_.fetchedAt);
  const long long lastAttemptAt = std::max(direct_.lastAttemptAt, proxy_.lastAttemptAt);
  const long long ageMs = fetchedAt > 0 ? now - fetchedAt : 0;
  auto nextFor = [&](const OutletCache &cache) {
    const long long regularNextRefreshAt = cache.fetchedAt > 0 ? cache.fetchedAt + settings.publicIpRefreshIntervalMs : now;
    const long long retryAt = cache.lastAttemptAt > 0 ? cache.lastAttemptAt + retryCooldownMs_ : now;
    return cache.lastError.empty() && cache.fetchedAt > 0 ? regularNextRefreshAt : std::max(regularNextRefreshAt, retryAt);
  };
  const long long nextDirectAt = nextFor(direct_);
  const long long nextProxyAt = nextFor(proxy_);
  const long long nextRefreshAt = std::min(nextDirectAt, nextProxyAt);
  const std::string error = (directOk || proxyOk)
    ? ""
    : (direct_.lastError.empty() ? proxy_.lastError : (proxy_.lastError.empty() ? direct_.lastError : direct_.lastError + "; " + proxy_.lastError));
  std::ostringstream out;
  out << "{"
      << "\"enabled\":" << (settings.allowPublicIpLookup ? "true" : "false") << ","
      << "\"ok\":" << (settings.allowPublicIpLookup && (directOk || proxyOk) ? "true" : "false") << ","
      << "\"source\":\"双出口检测\","
      << "\"fetchedAt\":" << fetchedAt << ","
      << "\"lastAttemptAt\":" << lastAttemptAt << ","
      << "\"ageMs\":" << ageMs << ","
      << "\"refreshIntervalMs\":" << settings.publicIpRefreshIntervalMs << ","
      << "\"retryCooldownMs\":" << retryCooldownMs_ << ","
      << "\"nextRefreshAt\":" << nextRefreshAt << ","
      << "\"error\":" << json::quote(settings.allowPublicIpLookup ? error : "") << ","
      << "\"data\":" << (settings.allowPublicIpLookup ? primary.cachedBody : "{}") << ","
      << "\"routes\":["
      << routeJson(direct_, now, settings) << ","
      << routeJson(proxy_, now, settings)
      << "]"
      << "}";
  return out.str();
}

std::string PublicIpService::ipInfoJson(const Settings &settings, bool forceRefresh) {
  std::lock_guard<std::mutex> lock(mutex_);
  const long long now = nowMs();
  if (!settings.allowPublicIpLookup) {
    return statusJson(now, settings);
  }
  if (shouldRefresh(direct_, now, settings, forceRefresh) || shouldRefresh(proxy_, now, settings, forceRefresh)) {
    fetchIpInfo(now, settings, forceRefresh);
  }
  return statusJson(now, settings);
}

} // namespace tmon
