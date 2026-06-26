#include "JsonUtil.hpp"
#include "Util.hpp"

#include <iomanip>
#include <regex>
#include <sstream>

namespace tmon::json {

std::string escape(const std::string &value) {
  std::ostringstream out;
  for (unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        } else {
          out << c;
        }
    }
  }
  return out.str();
}

std::string quote(const std::string &value) {
  return "\"" + escape(value) + "\"";
}

std::string stringArray(const std::vector<std::string> &values, size_t limit) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < values.size() && i < limit; ++i) {
    if (i) out << ",";
    out << quote(values[i]);
  }
  out << "]";
  return out.str();
}

std::string stringArray(const std::set<std::string> &values, size_t limit) {
  std::vector<std::string> ordered(values.begin(), values.end());
  return stringArray(ordered, limit);
}

std::string extractString(const std::string &object, const std::string &key, const std::string &fallback) {
  std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
  std::smatch match;
  return std::regex_search(object, match, re) ? match[1].str() : fallback;
}

double extractNumber(const std::string &object, const std::string &key, double fallback) {
  std::regex re("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
  std::smatch match;
  return std::regex_search(object, match, re) ? parseDouble(match[1].str()) : fallback;
}

bool extractBool(const std::string &object, const std::string &key, bool fallback) {
  std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
  std::smatch match;
  if (!std::regex_search(object, match, re)) return fallback;
  return match[1].str() == "true";
}

std::vector<std::string> extractArray(const std::string &object, const std::string &key) {
  std::vector<std::string> items;
  std::regex re("\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
  std::smatch match;
  if (!std::regex_search(object, match, re)) return items;
  std::string raw = match[1].str();
  std::regex itemRe("\"([^\"]*)\"|(-?[0-9]+)");
  auto begin = std::sregex_iterator(raw.begin(), raw.end(), itemRe);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    items.push_back((*it)[1].matched ? (*it)[1].str() : (*it)[2].str());
  }
  return items;
}

} // namespace tmon::json
