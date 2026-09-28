/**
 * @file CameraComp.h
 * @brief 摄像头硬件初始化与基础帧管理组件。
 */
#pragma once

#include <esp_camera.h>
#include <stddef.h>

namespace CameraComp
{
struct Settings
{
  framesize_t frameSize;
  uint8_t fps;
  int8_t brightness;
  int8_t contrast;
  int8_t sharpness;
  bool hmirror;
  bool vflip;
};

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

/** @brief 读取当前传感器参数和流目标帧率。 */
bool getSettings(Settings &settings);

/** @brief 校验并应用整组参数；失败时恢复原传感器参数。 */
bool applySettings(const Settings &settings);

/** @brief 将 API 分辨率字符串转换为摄像头帧尺寸。 */
bool parseResolution(const char *value, framesize_t &frameSize);

/** @brief 返回 API 使用的分辨率字符串。 */
const char *resolutionName(framesize_t frameSize);

/** @brief 当前传感器是否支持该帧尺寸。 */
bool supportsFrameSize(framesize_t frameSize);

/** @brief 当前传感器和已分配帧缓冲共同支持的分辨率数量。 */
size_t supportedResolutionCount();

/** @brief 按索引返回支持的 API 分辨率字符串。 */
const char *supportedResolutionAt(size_t index);

/** @brief 返回指定分辨率允许的 MJPEG 目标帧率上限。 */
uint8_t maxFpsForFrameSize(framesize_t frameSize);

/** @brief 运行时探测到的传感器型号。 */
const char *sensorModel();

/** @brief 运行时探测到的传感器 PID。 */
uint16_t sensorPid();

/** @brief 是否检测并启用了 PSRAM。 */
bool hasPsram();

/** @brief 为帧缓冲预分配的最大分辨率。 */
framesize_t allocationFrameSize();

/** @brief 视频流目标帧率上限。 */
uint8_t targetFps();

/**
 * @brief 以当前传感器/缓冲区支持的最大分辨率拍摄一张 JPEG，随后恢复原设置。
 * @param data 返回位于堆（优先 PSRAM）中的 JPEG 数据，调用者必须使用
 *             releaseSnapshot() 释放。
 * @param length JPEG 字节数。
 * @param width 实际图像宽度。
 * @param height 实际图像高度。
 */
bool captureMaximumJpeg(uint8_t *&data,
                        size_t &length,
                        uint16_t &width,
                        uint16_t &height);

/** @brief 释放 captureMaximumJpeg() 返回的 JPEG 副本。 */
void releaseSnapshot(uint8_t *data);
}
