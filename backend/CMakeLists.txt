cmake_minimum_required(VERSION 3.18)

project(TrafficMonitorBackend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(traffic-monitor
  src/AppConfig.cpp
  src/CommandRunner.cpp
  src/ConnectionSampler.cpp
  src/HttpServer.cpp
  src/JsonUtil.cpp
  src/Logger.cpp
  src/main.cpp
  src/NetworkSampler.cpp
  src/PublicIpService.cpp
  src/RemoteMonitor.cpp
  src/SnapshotService.cpp
  src/Util.cpp
)

target_compile_options(traffic-monitor PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)
