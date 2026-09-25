/**
 * @file CameraComp.cpp
 * @brief 摄像头硬件初始化与基础帧管理组件实现。
 */
#include "CameraComp.h"

#include "../config/CameraConfig.h"

#include <Arduino.h>

namespace
{
bool cameraEnabled = false;
}

bool CameraComp::enable()
{
  if (cameraEnabled)
  {
    return true;
  }

  pinMode(CameraConfig::LED_GPIO_NUM, OUTPUT);
  digitalWrite(CameraConfig::LED_GPIO_NUM, LOW);

  const bool hasPsram = psramFound();
  camera_config_t config = CameraConfig::create(hasPsram);
  const esp_err_t error = esp_camera_init(&config);

  if (error != ESP_OK)
  {
    Serial.printf("Camera initialization failed: 0x%04x\n",
                  static_cast<unsigned int>(error));
    return false;
  }

  // Keep the 180° orientation, then mirror it horizontally. The extra
  // horizontal mirror cancels the rotation's horizontal flip, leaving vflip.
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == nullptr ||
      sensor->set_hmirror(sensor, 0) != 0 ||
      sensor->set_vflip(sensor, 1) != 0)
  {
    Serial.println("Failed to rotate camera image");
    esp_camera_deinit();
    return false;
  }

  cameraEnabled = true;
  Serial.printf("Camera initialized (%s PSRAM)\n",
                hasPsram ? "with" : "without");
  return true;
}

void CameraComp::disable()
{
  if (!cameraEnabled)
  {
    return;
  }

  esp_camera_deinit();
  digitalWrite(CameraConfig::LED_GPIO_NUM, LOW);
  cameraEnabled = false;
}

bool CameraComp::isEnabled()
{
  return cameraEnabled;
}

camera_fb_t *CameraComp::captureFrame()
{
  if (!cameraEnabled)
  {
    Serial.println("Cannot capture frame: camera is not initialized");
    return nullptr;
  }

  camera_fb_t *frame = esp_camera_fb_get();
  if (frame == nullptr)
  {
    Serial.println("Camera frame capture failed");
  }

  return frame;
}

void CameraComp::releaseFrame(camera_fb_t *frame)
{
  if (frame != nullptr)
  {
    esp_camera_fb_return(frame);
  }
}
