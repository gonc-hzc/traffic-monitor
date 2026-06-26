#include "Util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace tmon {

long long nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string isoTime(long long ms) {
  std::time_t seconds = static_cast<std::time_t>(ms / 1000);
  std::tm tm {};
  gmtime_r(&seconds, &tm);
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string trim(const std::string &value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::vector<std::string> splitWhitespace(const std::string &line) {
  std::istringstream input(line);
  std::vector<std::string> parts;
  std::string item;
  while (input >> item) parts.push_back(item);
  return parts;
}

double parseDouble(const std::string &value) {
  try {
    size_t index = 0;
    double parsed = std::stod(value, &index);
    return index > 0 ? parsed : 0;
  } catch (...) {
    return 0;
  }
}

bool isLocalAddress(const std::string &address) {
  return address.empty() || address == "*" || address == "localhost" ||
         address.rfind("127.", 0) == 0 || address == "::1" ||
         address.rfind("::ffff:127.", 0) == 0;
}

std::string urlDecode(const std::string &value) {
  std::string result;
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const std::string hex = value.substr(i + 1, 2);
      char *end = nullptr;
      long parsed = std::strtol(hex.c_str(), &end, 16);
      if (end && *end == '\0') {
        result.push_back(static_cast<char>(parsed));
        i += 2;
      } else {
        result.push_back(value[i]);
      }
    } else if (value[i] == '+') {
      result.push_back(' ');
    } else {
      result.push_back(value[i]);
    }
  }
  return result;
}

} // namespace tmon
