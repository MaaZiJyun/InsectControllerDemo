/**
 * @file App.cpp
 * @brief 固件启动序列与 Arduino 主循环调度实现。
 *
 * 初始化顺序体现硬件安全依赖：先建立安全电极状态，再启用摄像头和网络服务。
 * IMU 首次失败属于可恢复故障，不会关闭已启动的 Web 功能。
 */
#include "App.h"

#include "../components/BoardComp.h"
#include "../components/CameraComp.h"
#include "../components/WifiComp.h"
#include "../services/BehaviorService.h"
#include "../services/CommandService.h"
#include "../services/DiscoveryService.h"
#include "../services/VideoService.h"

#include <Arduino.h>

// 主 Setup 函数
/** @brief 按安全依赖顺序初始化所有设备与网络子系统。 */
void App::begin(){

  // Arduino Serial 调试输出
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println();

  // 先将所有行为通道置于安全的 INPUT 高阻状态
  BoardComp::enable();

  // 初始化摄像头；失败不会阻止网络服务启动
  CameraComp::enable();

  // 启动 Wi-Fi 连接或 SoftAP 回退
  WifiComp::enable();

  // 启动行为、控制页面和独立端口的视频流服务
  BehaviorService::enable();
  VideoService::enable();
  CommandService::enable();
  DiscoveryService::enable();
}

// 主循环
/** @brief 推进刺激和 IMU 非阻塞任务，并向 Arduino 调度器让出执行时间。 */
void App::update()
{
  CommandService::update();
  DiscoveryService::update();

  // Give background Wi-Fi and camera tasks CPU time without blocking them.
  delay(1);
}
