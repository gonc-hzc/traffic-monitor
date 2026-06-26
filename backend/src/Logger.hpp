#pragma once

#include "AppConfig.hpp"
#include "Models.hpp"

#include <mutex>
#include <set>
#include <vector>

namespace tmon {

class Logger {
public:
  explicit Logger(const AppConfig &config);

  void event(const std::string &level, const std::string &type, const std::string &message, const std::string &detailJson = "{}");
  void warningOnce(const std::string &key, const std::string &message, const std::string &detailJson = "{}");
  std::vector<Event> recent(size_t limit) const;
  std::string eventsJson(size_t limit) const;

private:
  const AppConfig &config_;
  mutable std::mutex mutex_;
  std::vector<Event> events_;
  std::set<std::string> warningKeys_;
};

} // namespace tmon
