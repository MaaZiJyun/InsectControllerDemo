/**
 * @file App.h
 * @brief 固件顶层启动与调度协调器。
 *
 * App 负责按安全顺序初始化各子系统，并在 Arduino 主循环中驱动非阻塞服务。
 * 该类拥有 IMU 调度服务的生命周期。
 */
#pragma once

/** @brief 组织设备初始化和周期性更新的应用门面。 */
class App
{
public:
  /** @brief 按电极、摄像头、网络、存储、Web、IMU 的顺序初始化系统。 */
  void begin();
  /** @brief 推进所有依赖主循环的非阻塞状态机。 */
  void update();
};
