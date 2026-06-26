#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[1/3] Generating app icons"
swift -module-cache-path "$ROOT_DIR/build/swift-module-cache" \
  scripts/generate_app_icons.swift \
  assets/traffic_monitor.png \
  assets/AppIcon.iconset \
  frontend/public/app-icon.png
mkdir -p assets/Assets.xcassets/AppIcon.appiconset
cp assets/AppIcon.iconset/*.png assets/Assets.xcassets/AppIcon.appiconset/

echo "[2/3] Building React frontend"
npm --prefix frontend run build

echo "[3/3] Building C++ backend"
cmake -S backend -B build/backend
cmake --build build/backend --config Release

echo "Build complete"
