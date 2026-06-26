import AppKit
import Foundation

struct IconOutput {
  let filename: String
  let size: Int
}

func usage() -> Never {
  fputs("generate_app_icons.swift <source-png> <iconset-dir> <web-icon-png>\n", stderr)
  exit(2)
}

guard CommandLine.arguments.count == 4 else {
  usage()
}

let sourceURL = URL(fileURLWithPath: CommandLine.arguments[1])
let iconsetURL = URL(fileURLWithPath: CommandLine.arguments[2], isDirectory: true)
let webIconURL = URL(fileURLWithPath: CommandLine.arguments[3])

guard let image = NSImage(contentsOf: sourceURL) else {
  fputs("Unable to read source image: \(sourceURL.path)\n", stderr)
  exit(1)
}

try FileManager.default.createDirectory(at: iconsetURL, withIntermediateDirectories: true)
try FileManager.default.createDirectory(at: webIconURL.deletingLastPathComponent(), withIntermediateDirectories: true)

func writePNG(size: Int, to url: URL) throws {
  guard let rep = NSBitmapImageRep(
    bitmapDataPlanes: nil,
    pixelsWide: size,
    pixelsHigh: size,
    bitsPerSample: 8,
    samplesPerPixel: 4,
    hasAlpha: true,
    isPlanar: false,
    colorSpaceName: .deviceRGB,
    bytesPerRow: 0,
    bitsPerPixel: 0
  ) else {
    throw NSError(domain: "TrafficMonitorIcon", code: 1)
  }

  rep.size = NSSize(width: size, height: size)
  NSGraphicsContext.saveGraphicsState()
  NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
  NSColor.clear.setFill()
  NSRect(x: 0, y: 0, width: size, height: size).fill()
  image.draw(in: NSRect(x: 0, y: 0, width: size, height: size),
             from: .zero,
             operation: .sourceOver,
             fraction: 1.0)
  NSGraphicsContext.restoreGraphicsState()

  guard let data = rep.representation(using: .png, properties: [:]) else {
    throw NSError(domain: "TrafficMonitorIcon", code: 2)
  }
  try data.write(to: url)
}

let outputs = [
  IconOutput(filename: "icon_16x16.png", size: 16),
  IconOutput(filename: "icon_16x16@2x.png", size: 32),
  IconOutput(filename: "icon_32x32.png", size: 32),
  IconOutput(filename: "icon_32x32@2x.png", size: 64),
  IconOutput(filename: "icon_128x128.png", size: 128),
  IconOutput(filename: "icon_128x128@2x.png", size: 256),
  IconOutput(filename: "icon_256x256.png", size: 256),
  IconOutput(filename: "icon_256x256@2x.png", size: 512),
  IconOutput(filename: "icon_512x512.png", size: 512),
  IconOutput(filename: "icon_512x512@2x.png", size: 1024),
]

for output in outputs {
  try writePNG(size: output.size, to: iconsetURL.appendingPathComponent(output.filename))
}

try writePNG(size: 512, to: webIconURL)
