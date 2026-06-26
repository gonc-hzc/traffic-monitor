#pragma once

#include <set>
#include <string>
#include <vector>

namespace tmon::json {

std::string escape(const std::string &value);
std::string quote(const std::string &value);
std::string stringArray(const std::vector<std::string> &values, size_t limit = 1000);
std::string stringArray(const std::set<std::string> &values, size_t limit = 1000);

std::string extractString(const std::string &object, const std::string &key, const std::string &fallback = "");
double extractNumber(const std::string &object, const std::string &key, double fallback = 0);
bool extractBool(const std::string &object, const std::string &key, bool fallback = false);
std::vector<std::string> extractArray(const std::string &object, const std::string &key);

} // namespace tmon::json
