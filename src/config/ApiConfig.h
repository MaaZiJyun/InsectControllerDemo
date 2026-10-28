/**
 * @file ApiConfig.h
 * @brief 板端所有 HTTP/UDP API 的端口、路径和协议常量。
 *
 * 新增或修改 API 时先在此处维护，服务实现中不再写死路由字符串。
 */
#pragma once

#include <stdint.h>

namespace ApiConfig
{
// HTTP 控制 API 端口：刺激、摄像头、Wi-Fi 和状态接口。
inline constexpr uint16_t CONTROL_PORT = 80;

// MJPEG 视频服务独立端口。
inline constexpr uint16_t VIDEO_PORT = 81;

// Exp.-Recorder UDP 局域网发现端口。
inline constexpr uint16_t DISCOVERY_PORT = 37020;

// GET：读取板卡型号、端口和刺激参数范围。
inline constexpr char CONFIG_PATH[] = "/config";

// POST：以 pin1、pin2、frequencyHz、durationMs 启动刺激。
inline constexpr char STIMULATION_START_PATH[] = "/stimulation/start";

// POST：停止刺激并将所有刺激引脚恢复高阻。
inline constexpr char STIMULATION_STOP_PATH[] = "/stimulation/stop";

// GET/POST：读取或修改摄像头参数。
inline constexpr char CAMERA_SETTINGS_PATH[] = "/camera/settings";

// GET：拍摄一张最大分辨率 JPEG 照片。
inline constexpr char CAMERA_CAPTURE_PATH[] = "/camera/capture";

// GET：读取 MJPEG 传输统计。
inline constexpr char STREAM_STATS_PATH[] = "/stream/stats";

// GET：扫描附近的 Wi-Fi 网络。
inline constexpr char WIFI_NETWORKS_PATH[] = "/wifi/networks";

// GET/POST：读取 Wi-Fi 状态或提交新的 SSID/密码。
inline constexpr char WIFI_SETTINGS_PATH[] = "/wifi/settings";

// GET/POST：读取或修改板子 SoftAP 的 2.4 GHz 信道。
inline constexpr char WIFI_ACCESS_POINT_PATH[] = "/wifi/access-point";

// GET（VIDEO_PORT）：MJPEG 实时视频流。
inline constexpr char VIDEO_STREAM_PATH[] = "/stream";

// UDP 发现请求的固定文本。
inline constexpr char DISCOVERY_REQUEST[] = "EXP_RECORDER_DISCOVER_V1";

// UDP 发现响应中的协议版本。
inline constexpr char DISCOVERY_PROTOCOL[] = "EXP_RECORDER_BOARD_V1";
} // namespace ApiConfig
