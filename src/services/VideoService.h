/**
 * @file VideoService.h
 * @brief 摄像头 MJPEG HTTP 视频服务。
 */
#pragma once

#include <stdint.h>

namespace VideoService
{
/** @brief 在配置的视频端口启动实时流服务。 */
bool enable();

/**
 * @brief 暂停取帧并等待当前视频帧发送完成，但保持 MJPEG TCP 连接。
 * @return 摄像头已安全交还给调用方时返回 true。
 */
bool pauseForCameraChange();

/** @brief 恢复被 pauseForCameraChange() 暂停的视频取帧。 */
void resumeAfterCameraChange();

/** @brief 停止实时流服务。 */
void disable();

/** @brief 返回视频服务是否正在运行。 */
bool isEnabled();

/** @brief 每次视频服务成功启动后递增，供前端强制刷新 stream URL。 */
uint32_t generation();
}
