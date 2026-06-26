#include "Logger.hpp"
#include "JsonUtil.hpp"
#include "Util.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace tmon {

Logger::Logger(const AppConfig &config) : config_(config) {}

void Logger::event(const std::string &level, const std::string &type, const std::string &message, const std::string &detailJson) {
  std::lock_guard<std::mutex> lock(mutex_);
  Event event;
  event.ts = nowMs();
  event.time = isoTime(event.ts);
  event.id = std::to_string(event.ts) + "-" + std::to_string(std::rand());
  event.level = level;
  event.type = type;
  event.message = message;
  event.detailJson = detailJson;
  events_.insert(events_.begin(), event);
  if (events_.size() > 500) events_.resize(500);

  std::ofstream log(config_.paths().dataDir / "traffic-monitor.log", std::ios::app);
  log << "{"
      << "\"id\":" << json::quote(event.id) << ","
      << "\"ts\":" << event.ts << ","
      << "\"time\":" << json::quote(event.time) << ","
      << "\"level\":" << json::quote(event.level) << ","
      << "\"type\":" << json::quote(event.type) << ","
      << "\"message\":" << json::quote(event.message) << ","
      << "\"detail\":" << event.detailJson
      << "}\n";
}

void Logger::warningOnce(const std::string &key, const std::string &message, const std::string &detailJson) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (warningKeys_.count(key)) return;
    warningKeys_.insert(key);
  }
  event("warn", "采样", message, detailJson);
}

std::vector<Event> Logger::recent(size_t limit) const {
  std::lock_guard<std::mutex> lock(mutex_);
  limit = std::min(limit, events_.size());
  return std::vector<Event>(events_.begin(), events_.begin() + static_cast<long>(limit));
}

std::string Logger::eventsJson(size_t limit) const {
  const auto events = recent(limit);
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < events.size(); ++i) {
    const auto &event = events[i];
    if (i) out << ",";
    out << "{"
        << "\"id\":" << json::quote(event.id) << ","
        << "\"ts\":" << event.ts << ","
        << "\"time\":" << json::quote(event.time) << ","
        << "\"level\":" << json::quote(event.level) << ","
        << "\"type\":" << json::quote(event.type) << ","
        << "\"message\":" << json::quote(event.message) << ","
        << "\"detail\":" << event.detailJson
        << "}";
  }
  out << "]";
  return out.str();
}

} // namespace tmon
