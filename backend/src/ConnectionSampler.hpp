#pragma once

#include "CommandRunner.hpp"
#include "Logger.hpp"
#include "Models.hpp"

namespace tmon {

class ConnectionSampler {
public:
  ConnectionSampler(CommandRunner commandRunner, Logger &logger);

  std::vector<Connection> readConnections();
  ConnectionSummary summarize(const std::vector<Connection> &connections, double downloadBps, double uploadBps) const;

  static double connectionWeight(const Connection &connection);
  static bool targetMatchesConnection(const RemoteTarget &target, const Connection &connection);

private:
  static Endpoint parseEndpoint(const std::string &text);
  static std::string decodeCommandName(std::string command);
  static std::string serviceForPort(const std::string &port, const std::string &protocol);
  static std::string preferredPort(const Connection &connection);
  static Classification classify(Connection &connection);

  CommandRunner commandRunner_;
  Logger &logger_;
};

} // namespace tmon
