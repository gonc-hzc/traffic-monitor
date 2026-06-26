import Cocoa
import Darwin
import WebKit

final class TrafficMonitorAppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate, WKNavigationDelegate {
    private var window: NSWindow?
    private var webView: WKWebView?
    private var backendProcess: Process?
    private var logHandle: FileHandle?
    private var healthTimer: Timer?
    private var didLoadDashboard = false
    private var launchedBackend = false

    private var port = ProcessInfo.processInfo.environment["TRAFFIC_MONITOR_PORT"] ?? "3719"

    func applicationDidFinishLaunching(_ notification: Notification) {
        setupMenu()
        setupWindow()
        startBackendWhenNeeded()
        waitForBackend()
    }

    func applicationWillTerminate(_ notification: Notification) {
        healthTimer?.invalidate()
        if launchedBackend {
            backendProcess?.terminate()
        }
        try? logHandle?.close()
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        showMainWindow()
        return false
    }

    func applicationDidBecomeActive(_ notification: Notification) {
        if window?.isVisible == false {
            showMainWindow()
        }
    }

    private func setupMenu() {
        let mainMenu = NSMenu()
        let appItem = NSMenuItem()
        let appMenu = NSMenu()
        let showItem = NSMenuItem(title: "Show Traffic Monitor", action: #selector(showMainWindow), keyEquivalent: "0")
        showItem.target = self
        appMenu.addItem(showItem)
        appMenu.addItem(.separator())
        appMenu.addItem(NSMenuItem(title: "Quit Traffic Monitor", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))
        appItem.submenu = appMenu
        mainMenu.addItem(appItem)
        NSApp.mainMenu = mainMenu
    }

    private func setupWindow() {
        if window != nil, webView != nil {
            showMainWindow()
            return
        }

        let configuration = WKWebViewConfiguration()
        configuration.preferences.javaScriptCanOpenWindowsAutomatically = false

        let dashboardView = WKWebView(frame: .zero, configuration: configuration)
        dashboardView.navigationDelegate = self
        dashboardView.setValue(false, forKey: "drawsBackground")
        webView = dashboardView

        let dashboardWindow = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1320, height: 900),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        dashboardWindow.title = "Traffic Monitor"
        dashboardWindow.minSize = NSSize(width: 980, height: 680)
        dashboardWindow.isReleasedWhenClosed = false
        dashboardWindow.delegate = self
        dashboardWindow.center()
        dashboardWindow.contentView = dashboardView
        dashboardWindow.makeKeyAndOrderFront(nil)
        window = dashboardWindow
        NSApp.activate(ignoringOtherApps: true)

        showStatus("正在启动本地监控服务...")
    }

    @objc private func showMainWindow() {
        guard let window else {
            setupWindow()
            return
        }

        if didLoadDashboard, webView?.url == nil, let url = URL(string: "http://127.0.0.1:\(port)") {
            webView?.load(URLRequest(url: url))
        }

        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    private func startBackendWhenNeeded() {
        checkBackend { [weak self] isReady in
            guard let self else { return }
            if !isReady {
                self.port = self.chooseLaunchPort()
                self.launchBackend()
            }
        }
    }

    private func chooseLaunchPort() -> String {
        if let preferred = Int(port), isPortAvailable(preferred) {
            return String(preferred)
        }
        for candidate in 3720...3799 {
            if isPortAvailable(candidate) {
                return String(candidate)
            }
        }
        return port
    }

    private func isPortAvailable(_ candidate: Int) -> Bool {
        let fd = socket(AF_INET, SOCK_STREAM, 0)
        if fd < 0 { return false }
        defer { close(fd) }

        var address = sockaddr_in()
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        address.sin_family = sa_family_t(AF_INET)
        address.sin_port = UInt16(candidate).bigEndian
        address.sin_addr = in_addr(s_addr: inet_addr("127.0.0.1"))

        return withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockaddrPointer in
                Darwin.bind(fd, sockaddrPointer, socklen_t(MemoryLayout<sockaddr_in>.size)) == 0
            }
        }
    }

    private func launchBackend() {
        guard let resources = Bundle.main.resourceURL else {
            showStatus("无法定位应用资源目录")
            return
        }

        let backendURL = resources.appendingPathComponent("backend/traffic-monitor")
        let publicDir = resources.appendingPathComponent("public")
        let bundledConfigDir = resources.appendingPathComponent("config")

        let fm = FileManager.default
        let supportDir = fm.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Application Support/TrafficMonitor", isDirectory: true)
        let configDir = supportDir.appendingPathComponent("config", isDirectory: true)
        let dataDir = supportDir.appendingPathComponent("data", isDirectory: true)
        let logDir = fm.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Logs/TrafficMonitor", isDirectory: true)

        do {
            try fm.createDirectory(at: configDir, withIntermediateDirectories: true)
            try fm.createDirectory(at: dataDir, withIntermediateDirectories: true)
            try fm.createDirectory(at: logDir, withIntermediateDirectories: true)
            copyDefaultConfigIfNeeded(from: bundledConfigDir, to: configDir, name: "remote-targets.json")
            copyDefaultConfigIfNeeded(from: bundledConfigDir, to: configDir, name: "settings.json")

            let logFile = logDir.appendingPathComponent("backend.log")
            if !fm.fileExists(atPath: logFile.path) {
                fm.createFile(atPath: logFile.path, contents: nil)
            }
            logHandle = try FileHandle(forWritingTo: logFile)
            try logHandle?.seekToEnd()

            let process = Process()
            process.executableURL = backendURL
            process.arguments = [
                "--port", port,
                "--public-dir", publicDir.path,
                "--config-dir", configDir.path,
                "--data-dir", dataDir.path
            ]
            process.standardOutput = logHandle
            process.standardError = logHandle
            try process.run()
            backendProcess = process
            launchedBackend = true
        } catch {
            showStatus("启动 C++ 后端失败：\(error.localizedDescription)")
        }
    }

    private func copyDefaultConfigIfNeeded(from sourceDir: URL, to targetDir: URL, name: String) {
        let source = sourceDir.appendingPathComponent(name)
        let target = targetDir.appendingPathComponent(name)
        if !FileManager.default.fileExists(atPath: target.path) {
            try? FileManager.default.copyItem(at: source, to: target)
        }
    }

    private func waitForBackend() {
        healthTimer?.invalidate()
        healthTimer = Timer.scheduledTimer(withTimeInterval: 0.35, repeats: true) { [weak self] timer in
            guard let self else {
                timer.invalidate()
                return
            }
            self.checkBackend { [weak self] isReady in
                guard let self else { return }
                if isReady {
                    timer.invalidate()
                    self.loadDashboard()
                }
            }
        }
    }

    private func checkBackend(_ completion: @escaping (Bool) -> Void) {
        guard let url = URL(string: "http://127.0.0.1:\(port)/api/identity") else {
            completion(false)
            return
        }
        var request = URLRequest(url: url)
        request.timeoutInterval = 0.6
        URLSession.shared.dataTask(with: request) { data, response, error in
            let status = (response as? HTTPURLResponse)?.statusCode
            let body = data.flatMap { String(data: $0, encoding: .utf8) } ?? ""
            let isTrafficMonitor = body.contains("\"app\":\"TrafficMonitor\"") && body.contains("\"publicReady\":true")
            DispatchQueue.main.async {
                completion(error == nil && status == 200 && isTrafficMonitor)
            }
        }.resume()
    }

    private func loadDashboard() {
        guard !didLoadDashboard, let url = URL(string: "http://127.0.0.1:\(port)") else { return }
        didLoadDashboard = true
        webView?.load(URLRequest(url: url))
    }

    private func showStatus(_ message: String) {
        let escaped = message
            .replacingOccurrences(of: "&", with: "&amp;")
            .replacingOccurrences(of: "<", with: "&lt;")
            .replacingOccurrences(of: ">", with: "&gt;")
        let html = """
        <!doctype html>
        <html lang="zh-CN">
        <head>
          <meta charset="utf-8" />
          <style>
            body {
              margin: 0;
              min-height: 100vh;
              display: grid;
              place-items: center;
              background: #101113;
              color: #f1f5f2;
              font: 15px -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            }
            .box {
              border: 1px solid rgba(255,255,255,.1);
              border-radius: 8px;
              background: #17191d;
              padding: 24px 28px;
              box-shadow: 0 20px 80px rgba(0,0,0,.32);
            }
            strong { display:block; font-size: 22px; margin-bottom: 8px; color: #b9b8ff; }
            span { color: #9aa4a8; }
          </style>
        </head>
        <body><div class="box"><strong>Traffic Monitor</strong><span>\(escaped)</span></div></body>
        </html>
        """
        webView?.loadHTMLString(html, baseURL: nil)
    }
}

let app = NSApplication.shared
let delegate = TrafficMonitorAppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
