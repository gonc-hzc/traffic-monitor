#include "HttpServer.hpp"
#include "JsonUtil.hpp"
#include "Util.hpp"

#include <arpa/inet.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace tmon {

HttpServer::HttpServer(int port, const AppConfig &config, SnapshotService &snapshots)
  : port_(port), config_(config), snapshots_(snapshots) {}

void HttpServer::stop() {
  running_ = false;
}

bool HttpServer::readRequest(int client, HttpRequest &request) const {
  std::string raw;
  char buffer[4096];
  while (raw.find("\r\n\r\n") == std::string::npos) {
    ssize_t received = recv(client, buffer, sizeof(buffer), 0);
    if (received <= 0) return false;
    raw.append(buffer, static_cast<size_t>(received));
    if (raw.size() > 1024 * 1024) return false;
  }

  const size_t headerEnd = raw.find("\r\n\r\n");
  std::string head = raw.substr(0, headerEnd);
  request.body = raw.substr(headerEnd + 4);
  std::istringstream lines(head);
  std::string requestLine;
  std::getline(lines, requestLine);
  std::istringstream first(trim(requestLine));
  first >> request.method >> request.path;

  std::string line;
  while (std::getline(lines, line)) {
    line = trim(line);
    const size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    request.headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
  }

  size_t contentLength = 0;
  if (request.headers.count("content-length")) {
    contentLength = static_cast<size_t>(parseDouble(request.headers["content-length"]));
  }
  while (request.body.size() < contentLength) {
    ssize_t received = recv(client, buffer, sizeof(buffer), 0);
    if (received <= 0) return false;
    request.body.append(buffer, static_cast<size_t>(received));
  }
  if (request.body.size() > contentLength) request.body.resize(contentLength);
  return !request.method.empty();
}

void HttpServer::sendResponse(int client, int status, const std::string &statusText, const std::string &contentType,
                              const std::string &body, const std::string &extraHeaders) const {
  std::ostringstream header;
  header << "HTTP/1.1 " << status << " " << statusText << "\r\n"
         << "Content-Type: " << contentType << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n"
         << extraHeaders
         << "\r\n";
  const std::string head = header.str();
  send(client, head.data(), head.size(), 0);
  send(client, body.data(), body.size(), 0);
}

std::string HttpServer::mimeType(const std::filesystem::path &file) const {
  const std::string ext = lower(file.extension().string());
  if (ext == ".html") return "text/html; charset=utf-8";
  if (ext == ".css") return "text/css; charset=utf-8";
  if (ext == ".js") return "application/javascript; charset=utf-8";
  if (ext == ".json") return "application/json; charset=utf-8";
  if (ext == ".svg") return "image/svg+xml";
  if (ext == ".png") return "image/png";
  if (ext == ".ico") return "image/x-icon";
  return "application/octet-stream";
}

void HttpServer::sendFile(int client, const std::filesystem::path &file) const {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    sendResponse(client, 404, "Not Found", "text/plain; charset=utf-8", "Not found");
    return;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  sendResponse(client, 200, "OK", mimeType(file), buffer.str());
}

void HttpServer::handleClient(int client) {
  HttpRequest request;
  if (!readRequest(client, request)) {
    close(client);
    return;
  }

  const size_t query = request.path.find('?');
  std::string path = query == std::string::npos ? request.path : request.path.substr(0, query);
  path = urlDecode(path);

  if (path == "/api/identity" && request.method == "GET") {
    const bool publicReady = std::filesystem::exists(config_.paths().publicDir / "index.html");
    sendResponse(client, 200, "OK", "application/json; charset=utf-8",
      "{\"app\":\"TrafficMonitor\","
      "\"version\":\"0.2.0\","
      "\"publicReady\":" + std::string(publicReady ? "true" : "false") + ","
      "\"publicDir\":" + json::quote(config_.paths().publicDir.string()) + "}");
  } else if (path == "/api/snapshot" && request.method == "GET") {
    sendResponse(client, 200, "OK", "application/json; charset=utf-8", snapshots_.snapshotJson());
  } else if (path == "/api/logs" && request.method == "GET") {
    sendResponse(client, 200, "OK", "application/json; charset=utf-8", snapshots_.logsJson(500));
  } else if (path == "/api/ip-info" && request.method == "GET") {
    sendResponse(client, 200, "OK", "application/json; charset=utf-8", snapshots_.publicIpJson(false));
  } else if (path == "/api/ip-info/refresh" && request.method == "POST") {
    sendResponse(client, 200, "OK", "application/json; charset=utf-8", snapshots_.publicIpJson(true));
  } else if (path == "/api/ip-info/consent" && request.method == "POST") {
    sendResponse(client, 200, "OK", "application/json; charset=utf-8", snapshots_.setPublicIpConsent(request.body));
  } else if (path == "/api/targets" && request.method == "GET") {
    sendResponse(client, 200, "OK", "application/json; charset=utf-8", snapshots_.targetsJson());
  } else if (path == "/api/targets" && request.method == "POST") {
    sendResponse(client, 200, "OK", "application/json; charset=utf-8", snapshots_.addTarget(request.body));
  } else if (path.rfind("/api/targets/", 0) == 0 && request.method == "DELETE") {
    sendResponse(client, 200, "OK", "application/json; charset=utf-8", snapshots_.removeTarget(path.substr(std::string("/api/targets/").size())));
  } else if (path == "/api/export/logs" && request.method == "GET") {
    const auto logFile = config_.paths().dataDir / "traffic-monitor.log";
    sendResponse(client, 200, "OK", "text/plain; charset=utf-8", config_.readText(logFile),
      "Content-Disposition: attachment; filename=\"traffic-monitor.log\"\r\n");
  } else {
    std::string relative = path == "/" ? "index.html" : path.substr(1);
    if (relative.find("..") != std::string::npos) {
      sendResponse(client, 403, "Forbidden", "text/plain; charset=utf-8", "Forbidden");
    } else {
      auto file = config_.paths().publicDir / relative;
      if (!std::filesystem::exists(file) || std::filesystem::is_directory(file)) file = config_.paths().publicDir / "index.html";
      sendFile(client, file);
    }
  }
  close(client);
}

int HttpServer::run() {
  int serverFd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd < 0) {
    std::cerr << "socket failed\n";
    return 1;
  }
  int opt = 1;
  setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in address {};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin_port = htons(static_cast<uint16_t>(port_));
  if (bind(serverFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
    std::cerr << "bind failed on 127.0.0.1:" << port_ << "\n";
    close(serverFd);
    return 1;
  }
  if (listen(serverFd, 64) < 0) {
    std::cerr << "listen failed\n";
    close(serverFd);
    return 1;
  }

  std::cout << "Traffic Monitor C++ backend: http://localhost:" << port_ << "\n";
  while (running_) {
    sockaddr_in clientAddress {};
    socklen_t length = sizeof(clientAddress);
    int client = accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddress), &length);
    if (client < 0) continue;
    std::thread(&HttpServer::handleClient, this, client).detach();
  }

  close(serverFd);
  return 0;
}

} // namespace tmon
