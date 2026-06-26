#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace tmon {

long long nowMs();
std::string isoTime(long long ms);
std::string lower(std::string value);
std::string trim(const std::string &value);
std::vector<std::string> splitWhitespace(const std::string &line);
double parseDouble(const std::string &value);
bool isLocalAddress(const std::string &address);
std::string urlDecode(const std::string &value);

} // namespace tmon
