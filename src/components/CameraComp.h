/**
 * @file CameraComp.h
 * @brief 摄像头硬件初始化与基础帧管理组件。
 */
#pragma once

#include <esp_camera.h>

namespace CameraComp
{
/** @brief 根据 CameraConfig 初始化摄像头硬件。 */
bool enable();

/** @brief 释放摄像头驱动资源。 */
void disable();

/** @brief 返回摄像头是否已成功初始化。 */
bool isEnabled();

/**
 * @brief 获取一帧 JPEG 图像。
 * @return 成功时返回帧指针；失败或未初始化时返回 nullptr。
 * @note 使用完毕后必须调用 releaseFrame()。
 */
camera_fb_t *captureFrame();

/** @brief 归还由 captureFrame() 获取的帧缓冲区。 */
void releaseFrame(camera_fb_t *frame);
}
