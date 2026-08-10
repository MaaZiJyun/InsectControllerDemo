/**
 * @file VideoService.h
 * @brief 摄像头 MJPEG HTTP 视频服务。
 */
#pragma once

namespace VideoService
{
/** @brief 在配置的视频端口启动实时流服务。 */
bool enable();

/** @brief 停止实时流服务。 */
void disable();

/** @brief 返回视频服务是否正在运行。 */
bool isEnabled();
}
