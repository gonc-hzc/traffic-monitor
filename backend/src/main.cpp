#include "AppConfig.hpp"
#include "HttpServer.hpp"
#include "Logger.hpp"
#include "SnapshotService.hpp"
#include "Util.hpp"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>

namespace {

void printUsage() {
  std::cout << "traffic-monitor [--port 3719] [--public-dir path] [--config-dir path] [--data-dir path]\n";
}

tmon::AppPaths defaultPaths() {
  tmon::AppPaths paths;
  paths.root = std::filesystem::current_path();
  paths.publicDir = paths.root / "frontend" / "dist";
  paths.configDir = paths.root / "config";
  paths.dataDir = paths.root / "data";
  return paths;
}

} // namespace

int main(int argc, char **argv) {
  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  int port = 3719;
  tmon::AppPaths paths = defaultPaths();

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) return "";
      return argv[++i];
    };
    if (arg == "--port") port = static_cast<int>(tmon::parseDouble(next()));
    else if (arg == "--public-dir") paths.publicDir = next();
    else if (arg == "--config-dir") paths.configDir = next();
    else if (arg == "--data-dir") paths.dataDir = next();
    else if (arg == "--help") {
      printUsage();
      return 0;
    }
  }

  tmon::AppConfig config(paths);
  config.ensureDefaults();
  tmon::Logger logger(config);
  logger.event("info", "服务", "C++ 流量监控服务启动", "{\"port\":" + std::to_string(port) + "}");

  tmon::SnapshotService snapshots(config, logger);
  tmon::HttpServer server(port, config, snapshots);
  const int result = server.run();
  logger.event("info", "服务", "C++ 流量监控服务停止");
  return result;
}
