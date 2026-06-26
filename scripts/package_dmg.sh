#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="TrafficMonitor"
APP_BUNDLE="$ROOT_DIR/dist/$APP_NAME.app"
DMG_STAGE="$ROOT_DIR/dist/dmg-stage"
DMG_FILE="$ROOT_DIR/dist/$APP_NAME.dmg"

cd "$ROOT_DIR"

bash scripts/build_all.sh

rm -rf "$ROOT_DIR/build/asset-catalog"
mkdir -p "$ROOT_DIR/build/asset-catalog"
ACTOOL="$(xcrun --find actool 2>/dev/null || true)"
if [[ -n "$ACTOOL" ]]; then
  if ! "$ACTOOL" \
    --compile "$ROOT_DIR/build/asset-catalog" \
    --platform macosx \
    --minimum-deployment-target 12.0 \
    --app-icon AppIcon \
    --output-partial-info-plist "$ROOT_DIR/build/asset-catalog/asset-info.plist" \
    assets/Assets.xcassets \
    >"$ROOT_DIR/build/asset-catalog/actool.log" 2>&1; then
    cat "$ROOT_DIR/build/asset-catalog/actool.log"
    exit 1
  fi
else
  echo "warning: actool not found; app icon will be unavailable"
fi

rm -rf "$APP_BUNDLE" "$DMG_STAGE" "$DMG_FILE"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources/backend"
mkdir -p "$APP_BUNDLE/Contents/Resources/public"
mkdir -p "$APP_BUNDLE/Contents/Resources/config"

cp build/backend/traffic-monitor "$APP_BUNDLE/Contents/Resources/backend/traffic-monitor"
cp -R frontend/dist/. "$APP_BUNDLE/Contents/Resources/public/"
cp config/remote-targets.json "$APP_BUNDLE/Contents/Resources/config/remote-targets.json"
cp config/settings.json "$APP_BUNDLE/Contents/Resources/config/settings.json"
if [[ -f "$ROOT_DIR/build/asset-catalog/Assets.car" ]]; then
  cp "$ROOT_DIR/build/asset-catalog/Assets.car" "$APP_BUNDLE/Contents/Resources/Assets.car"
fi
if [[ -f "$ROOT_DIR/build/asset-catalog/AppIcon.icns" ]]; then
  cp "$ROOT_DIR/build/asset-catalog/AppIcon.icns" "$APP_BUNDLE/Contents/Resources/AppIcon.icns"
fi

cat > "$APP_BUNDLE/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>
  <string>Traffic Monitor</string>
  <key>CFBundleDisplayName</key>
  <string>Traffic Monitor</string>
  <key>CFBundleIdentifier</key>
  <string>local.traffic.monitor</string>
  <key>CFBundleVersion</key>
  <string>0.2.0</string>
  <key>CFBundleShortVersionString</key>
  <string>0.2.0</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleExecutable</key>
  <string>TrafficMonitor</string>
  <key>CFBundleIconFile</key>
  <string>AppIcon</string>
  <key>CFBundleIconName</key>
  <string>AppIcon</string>
  <key>LSMinimumSystemVersion</key>
  <string>12.0</string>
  <key>LSUIElement</key>
  <false/>
</dict>
</plist>
PLIST

swiftc \
  macos/TrafficMonitorApp.swift \
  -o "$APP_BUNDLE/Contents/MacOS/TrafficMonitor" \
  -module-cache-path "$ROOT_DIR/build/swift-module-cache" \
  -framework Cocoa \
  -framework WebKit

chmod +x "$APP_BUNDLE/Contents/Resources/backend/traffic-monitor"

codesign --force --deep --sign - "$APP_BUNDLE" >/dev/null 2>&1 || true

mkdir -p "$DMG_STAGE"
cp -R "$APP_BUNDLE" "$DMG_STAGE/"
ln -s /Applications "$DMG_STAGE/Applications"

hdiutil create \
  -volname "Traffic Monitor" \
  -srcfolder "$DMG_STAGE" \
  -ov \
  -format UDZO \
  "$DMG_FILE"

rm -rf "$DMG_STAGE" "$APP_BUNDLE"

echo "DMG created: $DMG_FILE"
