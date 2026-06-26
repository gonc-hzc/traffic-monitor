# 流量监控

当前这个是codex控制管理版本
https://github.com/gonc-hzc/Traffic_Monitor.git 这个是人工手动管理版本
React 前端 + C++ 后端的本机实时流量监控仪表盘。

## 功能

- 首屏总上行、总下行速率与动态曲线
- SSH / HTTPS / DNS / 代理等协议与端口统计
- 按进程聚合连接与速率估算
- Top 主机排行，辅助定位异常通信对象
- 远程目标监控，显示地址、协议、状态、延迟和实时速率
- 公网 IP 信息查询，支持直连和系统代理两条线路刷新位置、运营商和时区
- 日志采集、历史采样落盘、告警规则
- macOS `.app` 和 `.dmg` 打包脚本

## 工程结构

```text
backend/     C++17 HTTP 服务、系统采样、日志、远程探测
frontend/    React + Vite 仪表盘
config/      远程目标和告警规则
scripts/     构建与 DMG 打包脚本
data/        运行日志和历史采样
```

## 模块设计

后端保持高解耦：

- `AppConfig`：路径、配置文件、远程目标持久化
- `Logger`：事件日志、一次性告警、日志导出
- `NetworkSampler`：系统接口字节计数和速率计算
- `ConnectionSampler`：`lsof` 连接采样、协议/端口/进程/主机聚合
- `RemoteMonitor`：远程目标 TCP 探测与状态变化记录
- `SnapshotService`：组合各模块生成 API 快照
- `HttpServer`：HTTP 路由、静态资源和 API 分发

前端保持视图和数据分离：

- `hooks/useSnapshot.js`：轮询 C++ 后端快照
- `utils/format.js`：格式化、过滤匹配、速率条计算
- `components/`：指标卡、曲线图、速率条、空状态
- `views/`：总览、协议端口、进程、远程监控、日志页面

## 开发运行

```bash
cd ~/Desktop/流量监控
npm run install:frontend
npm run build
npm start
```

打开：

```text
http://localhost:3719
```

## 数据来源说明

- 总上行/下行优先读取系统接口字节计数：
  - macOS: `netstat -ibn`
  - Linux: `/proc/net/dev`
- 连接、协议、端口和进程归因优先读取：
  - `lsof -nP -iTCP -iUDP`
- per-process/per-port 真实字节数在 macOS 上需要更高权限或 packet capture。当前版本会清晰标注为估算速率，估算基于当前连接权重和总速率分配。
- 公网 IP 信息默认关闭外网查询；可在应用内授权开启。应用会优先直连查询，同时检测系统代理，展示直连 IP 与代理出口 IP 的差异。

## 打包 DMG

```bash
npm run package:dmg
```

输出：

```text
dist/TrafficMonitor.dmg
```

安装后运行 `.app` 会启动 C++ 本地服务，并在原生 macOS 应用窗口中显示 React 仪表盘，不会跳转到默认浏览器。应用运行数据写入：

```text
~/Library/Application Support/TrafficMonitor
~/Library/Logs/TrafficMonitor
```
