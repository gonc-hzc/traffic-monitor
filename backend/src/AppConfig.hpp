#pragma once

#include "Models.hpp"

#include <filesystem>

namespace tmon {

struct AppPaths {
  std::filesystem::path root = std::filesystem::current_path();
  std::filesystem::path publicDir = std::filesystem::current_path() / "frontend" / "dist";
  std::filesystem::path configDir = std::filesystem::current_path() / "config";
  std::filesystem::path dataDir = std::filesystem::current_path() / "data";
};

class AppConfig {
public:
  explicit AppConfig(AppPaths paths);

  const AppPaths &paths() const;
  void ensureDefaults() const;
  Settings loadSettings() const;
  void saveSettings(const Settings &settings) const;
  Settings setPublicIpLookupEnabled(bool enabled) const;
  std::vector<RemoteTarget> loadTargets() const;
  void saveTargets(const std::vector<RemoteTarget> &targets) const;
  std::string readText(const std::filesystem::path &file) const;
  void writeText(const std::filesystem::path &file, const std::string &content) const;

  static std::string sanitizeId(std::string value);

private:
  AppPaths paths_;
};

} // namespace tmon
