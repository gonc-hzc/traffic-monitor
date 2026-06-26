#pragma once

#include "CommandRunner.hpp"
#include "Logger.hpp"
#include "Models.hpp"

#include <map>

namespace tmon {

class NetworkSampler {
public:
  NetworkSampler(CommandRunner commandRunner, Logger &logger);

  Totals readTotals();
  std::pair<double, double> calculateRates(Totals &totals, long long timestamp);

private:
  bool excludedInterface(const std::string &name) const;

  CommandRunner commandRunner_;
  Logger &logger_;
  bool previousValid_ = false;
  std::string previousSource_;
  double previousRx_ = 0;
  double previousTx_ = 0;
  long long previousTs_ = 0;
  std::map<std::string, InterfaceStats> previousInterfaces_;
  double demoRx_ = 42000000;
  double demoTx_ = 9000000;
  int demoTick_ = 0;
};

} // namespace tmon
