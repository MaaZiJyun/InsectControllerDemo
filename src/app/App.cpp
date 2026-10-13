/**
 * @file App.cpp
 * @brief 固件启动序列与 Arduino 主循环调度实现。
 *
 * 初始化顺序体现硬件安全依赖：先建立安全电极状态，再启动网络。
 * SoftAP 阶段仅提供配网；加入外部网络后才启动摄像头和控制服务。
 */
#include "App.h"

#include "../components/BoardComp.h"
#include "../components/CameraComp.h"
#include "../components/WifiComp.h"
#include "../services/CommandService.h"
#include "../services/DiscoveryService.h"
#include "../services/StimulationService.h"
#include "../services/VideoService.h"

#include <Arduino.h>

namespace
{
bool stationServicesStarted = false;

void startStationServices()
{
  if (stationServicesStarted || WifiComp::isAccessPointMode())
  {
    return;
  }

  CameraComp::enable();
  VideoService::enable();
  DiscoveryService::enable();
  stationServicesStarted = true;
}
} // namespace

// 主 Setup 函数
/** @brief 按安全依赖顺序初始化所有设备与网络子系统。 */
void App::begin(){

  // Arduino Serial 调试输出
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println();

  // 先将所有刺激引脚置于安全的 INPUT 高阻状态
  BoardComp::enable();

  // 启动 Wi-Fi 连接或 SoftAP 回退
  WifiComp::enable();

  // SoftAP 只提供 Wi-Fi 配网；控制、摄像头和发现服务仅在 STA 模式启动。
  StimulationService::enable();
  CommandService::enable();
  startStationServices();
}

// 主循环
/** @brief 推进刺激和 IMU 非阻塞任务，并向 Arduino 调度器让出执行时间。 */
void App::update()
{
  WifiComp::update();
  startStationServices();
  CommandService::update();
  DiscoveryService::update();

  // Give background Wi-Fi and camera tasks CPU time without blocking them.
  delay(1);
}
