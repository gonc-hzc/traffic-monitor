#pragma once

#include <string>

namespace tmon {

class CommandRunner {
public:
  std::string run(const std::string &command) const;
};

} // namespace tmon
