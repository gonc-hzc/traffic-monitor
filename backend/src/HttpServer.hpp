#pragma once

#include "AppConfig.hpp"
#include "SnapshotService.hpp"

#include <atomic>
#include <map>
#include <string>

namespace tmon {

struct HttpRequest {
  std::string method;
  std::string path;
  std::string body;
  std::map<std::string, std::string> headers;
};

class HttpServer {
public:
  HttpServer(int port, const AppConfig &config, SnapshotService &snapshots);

  int run();
  void stop();

private:
  bool readRequest(int client, HttpRequest &request) const;
  void handleClient(int client);
  void sendResponse(int client, int status, const std::string &statusText, const std::string &contentType,
                    const std::string &body, const std::string &extraHeaders = "") const;
  void sendFile(int client, const std::filesystem::path &file) const;
  std::string mimeType(const std::filesystem::path &file) const;

  int port_;
  const AppConfig &config_;
  SnapshotService &snapshots_;
  std::atomic<bool> running_ {true};
};

} // namespace tmon
