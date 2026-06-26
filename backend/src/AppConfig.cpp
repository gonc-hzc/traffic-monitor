#include "AppConfig.hpp"
#include "JsonUtil.hpp"
#include "Util.hpp"

#include <fstream>
#include <regex>
#include <sstream>

namespace tmon {

AppConfig::AppConfig(AppPaths paths) : paths_(std::move(paths)) {}

const AppPaths &AppConfig::paths() const {
  return paths_;
}

void AppConfig::ensureDefaults() const {
  std::filesystem::create_directories(paths_.configDir);
  std::filesystem::create_directories(paths_.dataDir);

  const auto targets = paths_.configDir / "remote-targets.json";
  if (!std::filesystem::exists(targets)) {
    writeText(targets,
      "[\n"
      "  {\"id\":\"local-ssh\",\"name\":\"本机 SSH\",\"host\":\"127.0.0.1\",\"port\":22,\"protocol\":\"ssh\",\"enabled\":true},\n"
      "  {\"id\":\"example-server\",\"name\":\"示例服务器\",\"host\":\"example.com\",\"port\":443,\"protocol\":\"https\",\"enabled\":false}\n"
      "]\n");
  }

  const auto settings = paths_.configDir / "settings.json";
  if (!std::filesystem::exists(settings)) {
    writeText(settings,
      "{\n"
      "  \"highDownloadBps\": 8388608,\n"
      "  \"highUploadBps\": 4194304,\n"
      "  \"watchedPorts\": [22],\n"
      "  \"watchedHosts\": [],\n"
      "  \"blockedHosts\": [],\n"
      "  \"sampleHistoryEveryMs\": 60000,\n"
      "  \"allowPublicIpLookup\": false,\n"
      "  \"publicIpRefreshIntervalMs\": 300000\n"
      "}\n");
  }
}

Settings AppConfig::loadSettings() const {
  Settings settings;
  const std::string raw = readText(paths_.configDir / "settings.json");
  if (raw.empty()) return settings;
  settings.highDownloadBps = json::extractNumber(raw, "highDownloadBps", settings.highDownloadBps);
  settings.highUploadBps = json::extractNumber(raw, "highUploadBps", settings.highUploadBps);
  settings.sampleHistoryEveryMs = static_cast<long long>(json::extractNumber(raw, "sampleHistoryEveryMs", settings.sampleHistoryEveryMs));
  settings.allowPublicIpLookup = json::extractBool(raw, "allowPublicIpLookup", settings.allowPublicIpLookup);
  settings.publicIpRefreshIntervalMs = static_cast<long long>(json::extractNumber(raw, "publicIpRefreshIntervalMs", settings.publicIpRefreshIntervalMs));
  if (settings.publicIpRefreshIntervalMs < 60000) settings.publicIpRefreshIntervalMs = 60000;
  settings.watchedPorts = json::extractArray(raw, "watchedPorts");
  if (settings.watchedPorts.empty()) settings.watchedPorts = {"22"};
  settings.watchedHosts = json::extractArray(raw, "watchedHosts");
  settings.blockedHosts = json::extractArray(raw, "blockedHosts");
  for (auto &host : settings.watchedHosts) host = lower(host);
  for (auto &host : settings.blockedHosts) host = lower(host);
  return settings;
}

void AppConfig::saveSettings(const Settings &settings) const {
  std::ostringstream out;
  out << "{\n"
      << "  \"highDownloadBps\": " << settings.highDownloadBps << ",\n"
      << "  \"highUploadBps\": " << settings.highUploadBps << ",\n"
      << "  \"watchedPorts\": " << json::stringArray(settings.watchedPorts) << ",\n"
      << "  \"watchedHosts\": " << json::stringArray(settings.watchedHosts) << ",\n"
      << "  \"blockedHosts\": " << json::stringArray(settings.blockedHosts) << ",\n"
      << "  \"sampleHistoryEveryMs\": " << settings.sampleHistoryEveryMs << ",\n"
      << "  \"allowPublicIpLookup\": " << (settings.allowPublicIpLookup ? "true" : "false") << ",\n"
      << "  \"publicIpRefreshIntervalMs\": " << settings.publicIpRefreshIntervalMs << "\n"
      << "}\n";
  writeText(paths_.configDir / "settings.json", out.str());
}

Settings AppConfig::setPublicIpLookupEnabled(bool enabled) const {
  Settings settings = loadSettings();
  settings.allowPublicIpLookup = enabled;
  saveSettings(settings);
  return settings;
}

std::vector<RemoteTarget> AppConfig::loadTargets() const {
  std::vector<RemoteTarget> targets;
  const std::string raw = readText(paths_.configDir / "remote-targets.json");
  std::regex objectRe("\\{[^\\}]*\\}");
  auto begin = std::sregex_iterator(raw.begin(), raw.end(), objectRe);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    const std::string object = it->str();
    RemoteTarget target;
    target.id = sanitizeId(json::extractString(object, "id"));
    target.name = json::extractString(object, "name", target.id);
    target.host = json::extractString(object, "host");
    target.port = static_cast<int>(json::extractNumber(object, "port", 0));
    target.protocol = lower(json::extractString(object, "protocol", "tcp"));
    target.enabled = json::extractBool(object, "enabled", true);
    if (!target.host.empty() && target.port > 0 && target.port < 65536) targets.push_back(target);
  }
  return targets;
}

void AppConfig::saveTargets(const std::vector<RemoteTarget> &targets) const {
  std::ostringstream out;
  out << "[\n";
  for (size_t i = 0; i < targets.size(); ++i) {
    const auto &target = targets[i];
    out << "  {"
        << "\"id\":" << json::quote(target.id) << ","
        << "\"name\":" << json::quote(target.name) << ","
        << "\"host\":" << json::quote(target.host) << ","
        << "\"port\":" << target.port << ","
        << "\"protocol\":" << json::quote(target.protocol) << ","
        << "\"enabled\":" << (target.enabled ? "true" : "false")
        << "}";
    if (i + 1 < targets.size()) out << ",";
    out << "\n";
  }
  out << "]\n";
  writeText(paths_.configDir / "remote-targets.json", out.str());
}

std::string AppConfig::readText(const std::filesystem::path &file) const {
  std::ifstream in(file, std::ios::binary);
  if (!in) return "";
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void AppConfig::writeText(const std::filesystem::path &file, const std::string &content) const {
  std::filesystem::create_directories(file.parent_path());
  std::ofstream out(file, std::ios::binary);
  out << content;
}

std::string AppConfig::sanitizeId(std::string value) {
  for (char &c : value) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')) c = '-';
  }
  value = std::regex_replace(value, std::regex("-+"), "-");
  return value.empty() ? "target" : value;
}

} // namespace tmon
