#include "CommandRunner.hpp"

#include <array>
#include <cstdio>

namespace tmon {

std::string CommandRunner::run(const std::string &command) const {
  std::array<char, 4096> buffer {};
  std::string result;
  FILE *pipe = popen((command + " 2>&1").c_str(), "r");
  if (!pipe) return "";
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
    result += buffer.data();
  }
  pclose(pipe);
  return result;
}

} // namespace tmon
