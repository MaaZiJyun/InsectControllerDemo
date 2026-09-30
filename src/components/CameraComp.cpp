/**
 * @file CameraComp.cpp
 * @brief 摄像头探测、最大帧缓冲预分配、参数切换与帧验证。
 */
#include "CameraComp.h"

#include "../config/CameraConfig.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

namespace
{
bool cameraEnabled = false;
bool psramEnabled = false;
volatile uint8_t streamTargetFps = 30;
framesize_t sensorMaximum = FRAMESIZE_VGA;
framesize_t allocatedMaximum = FRAMESIZE_VGA;
uint16_t detectedPid = 0;
char detectedModel[20] = "unknown";

struct ResolutionOption
{
  const char *name;
  framesize_t frameSize;
};

constexpr ResolutionOption RESOLUTION_OPTIONS[] = {
    {"2048x1536", FRAMESIZE_QXGA},
    {"1920x1080", FRAMESIZE_FHD},
    {"1280x720", FRAMESIZE_HD},
    {"1024x768", FRAMESIZE_XGA},
    {"640x480", FRAMESIZE_VGA},
    {"320x240", FRAMESIZE_QVGA},
};

bool applySensorSettings(sensor_t *sensor, const CameraComp::Settings &settings)
{
  if (sensor == nullptr || sensor->set_framesize(sensor, settings.frameSize) != 0)
  {
    return false;
  }

  return sensor->set_brightness(sensor, settings.brightness) == 0 &&
         sensor->set_contrast(sensor, settings.contrast) == 0 &&
         sensor->set_sharpness(sensor, settings.sharpness) == 0 &&
         sensor->set_hmirror(sensor, settings.hmirror ? 1 : 0) == 0 &&
         sensor->set_vflip(sensor, settings.vflip ? 1 : 0) == 0;
}

bool frameMatches(camera_fb_t *frame, framesize_t frameSize)
{
  if (frame == nullptr || frameSize >= FRAMESIZE_INVALID)
  {
    return false;
  }
  const resolution_info_t &expected = resolution[frameSize];
  return frame->format == PIXFORMAT_JPEG &&
         frame->width == expected.width &&
         frame->height == expected.height &&
         frame->len >= 4 &&
         frame->buf[0] == 0xFF && frame->buf[1] == 0xD8 &&
         frame->buf[frame->len - 2] == 0xFF && frame->buf[frame->len - 1] == 0xD9;
}

bool discardAndValidateFrames(framesize_t frameSize)
{
  // CAMERA_GRAB_LATEST with two buffers can retain frames captured before a
  // sensor change. Discard both buffers, then validate a newly captured JPEG.
  for (uint8_t index = 0; index < 3; ++index)
  {
    camera_fb_t *frame = esp_camera_fb_get();
    if (frame == nullptr)
    {
      Serial.println("Camera validation failed: frame unavailable");
      return false;
    }
    const bool valid = index == 2 && frameMatches(frame, frameSize);
    if (index == 2 && !valid)
    {
      Serial.printf("Camera validation failed: got %ux%u format=%d bytes=%u\n",
                    frame->width,
                    frame->height,
                    static_cast<int>(frame->format),
                    static_cast<unsigned int>(frame->len));
    }
    esp_camera_fb_return(frame);
    if (index == 2)
    {
      return valid;
    }
  }
  return false;
}

bool applyAndValidateSettings(sensor_t *sensor, const CameraComp::Settings &settings)
{
  return applySensorSettings(sensor, settings) &&
         discardAndValidateFrames(settings.frameSize);
}

bool initializeCamera(const CameraComp::Settings &settings)
{
  camera_config_t config = CameraConfig::create(psramEnabled, allocatedMaximum);
  const esp_err_t error = esp_camera_init(&config);
  if (error != ESP_OK)
  {
    Serial.printf("Camera initialization failed: 0x%04x\n",
                  static_cast<unsigned int>(error));
    cameraEnabled = false;
    return false;
  }

  cameraEnabled = true;
  sensor_t *sensor = esp_camera_sensor_get();
  if (!applyAndValidateSettings(sensor, settings))
  {
    esp_camera_deinit();
    cameraEnabled = false;
    return false;
  }

  streamTargetFps = settings.fps;
  return true;
}

bool probeSensor()
{
  camera_config_t probeConfig = CameraConfig::create(psramEnabled, FRAMESIZE_VGA);
  if (esp_camera_init(&probeConfig) != ESP_OK)
  {
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  camera_sensor_info_t *info = sensor == nullptr
                                   ? nullptr
                                   : esp_camera_sensor_get_info(&sensor->id);
  if (sensor == nullptr || info == nullptr)
  {
    esp_camera_deinit();
    return false;
  }

  detectedPid = sensor->id.PID;
  snprintf(detectedModel, sizeof(detectedModel), "%s", info->name);
  sensorMaximum = info->max_size;
  allocatedMaximum = psramEnabled ? sensorMaximum : FRAMESIZE_VGA;
  esp_camera_deinit();
  delay(50);
  return true;
}
} // namespace

bool CameraComp::enable()
{
  if (cameraEnabled)
  {
    return true;
  }

  pinMode(CameraConfig::LED_GPIO_NUM, OUTPUT);
  digitalWrite(CameraConfig::LED_GPIO_NUM, LOW);

  psramEnabled = psramFound();
  Serial.printf("PSRAM detected=%s total=%u free=%u\n",
                psramEnabled ? "true" : "false",
                static_cast<unsigned int>(ESP.getPsramSize()),
                static_cast<unsigned int>(ESP.getFreePsram()));

  if (!probeSensor())
  {
    Serial.println("Camera sensor probe failed");
    return false;
  }

  const Settings defaults = {
      psramEnabled ? FRAMESIZE_VGA : FRAMESIZE_QVGA,
      30,
      0,
      0,
      0,
      false,
      true};

  if (!initializeCamera(defaults))
  {
    Serial.println("Camera failed to initialize with validated defaults");
    return false;
  }

  Serial.printf("Camera initialized sensor=%s pid=0x%04X allocation=%s PSRAM=%u/%u free\n",
                detectedModel,
                detectedPid,
                resolutionName(allocatedMaximum),
                static_cast<unsigned int>(ESP.getFreePsram()),
                static_cast<unsigned int>(ESP.getPsramSize()));
  return true;
}

void CameraComp::disable()
{
  if (cameraEnabled)
  {
    esp_camera_deinit();
  }
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

bool CameraComp::getSettings(Settings &settings)
{
  sensor_t *sensor = cameraEnabled ? esp_camera_sensor_get() : nullptr;
  if (sensor == nullptr)
  {
    return false;
  }
  settings.frameSize = sensor->status.framesize;
  settings.fps = streamTargetFps;
  settings.brightness = sensor->status.brightness;
  settings.contrast = sensor->status.contrast;
  settings.sharpness = sensor->status.sharpness;
  settings.hmirror = sensor->status.hmirror != 0;
  settings.vflip = sensor->status.vflip != 0;
  return true;
}

bool CameraComp::applySettings(const Settings &settings)
{
  Settings previous = {};
  if (!getSettings(previous) || !supportsFrameSize(settings.frameSize) ||
      settings.fps < 1 || settings.fps > maxFpsForFrameSize(settings.frameSize) ||
      settings.brightness < -2 || settings.brightness > 2 ||
      settings.contrast < -2 || settings.contrast > 2 ||
      settings.sharpness < -2 || settings.sharpness > 2)
  {
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (applyAndValidateSettings(sensor, settings))
  {
    streamTargetFps = settings.fps;
    return true;
  }

  Serial.println("Camera setting validation failed; rebuilding previous configuration");
  esp_camera_deinit();
  cameraEnabled = false;
  delay(50);
  if (initializeCamera(previous))
  {
    return false;
  }

  Serial.println("Camera rollback failed; rebuilding safe VGA defaults");
  const Settings safeDefaults = {
      psramEnabled ? FRAMESIZE_VGA : FRAMESIZE_QVGA,
      15,
      0,
      0,
      0,
      false,
      true};
  initializeCamera(safeDefaults);
  return false;
}

bool CameraComp::parseResolution(const char *value, framesize_t &frameSize)
{
  for (const ResolutionOption &option : RESOLUTION_OPTIONS)
  {
    if (strcmp(value, option.name) == 0)
    {
      frameSize = option.frameSize;
      return true;
    }
  }
  return false;
}

const char *CameraComp::resolutionName(framesize_t frameSize)
{
  for (const ResolutionOption &option : RESOLUTION_OPTIONS)
  {
    if (option.frameSize == frameSize)
    {
      return option.name;
    }
  }
  return "unknown";
}

bool CameraComp::supportsFrameSize(framesize_t frameSize)
{
  return cameraEnabled && frameSize <= sensorMaximum && frameSize <= allocatedMaximum;
}

size_t CameraComp::supportedResolutionCount()
{
  size_t count = 0;
  for (const ResolutionOption &option : RESOLUTION_OPTIONS)
  {
    if (supportsFrameSize(option.frameSize))
    {
      ++count;
    }
  }
  return count;
}

const char *CameraComp::supportedResolutionAt(size_t index)
{
  size_t current = 0;
  for (const ResolutionOption &option : RESOLUTION_OPTIONS)
  {
    if (!supportsFrameSize(option.frameSize))
    {
      continue;
    }
    if (current++ == index)
    {
      return option.name;
    }
  }
  return nullptr;
}

uint8_t CameraComp::maxFpsForFrameSize(framesize_t frameSize)
{
  switch (frameSize)
  {
  case FRAMESIZE_QVGA:
    return 120;
  case FRAMESIZE_VGA:
    return 60;
  case FRAMESIZE_XGA:
  case FRAMESIZE_HD:
    return 45;
  case FRAMESIZE_FHD:
    return 20;
  case FRAMESIZE_QXGA:
    return 15;
  default:
    return 1;
  }
}

const char *CameraComp::sensorModel()
{
  return detectedModel;
}

uint16_t CameraComp::sensorPid()
{
  return detectedPid;
}

bool CameraComp::hasPsram()
{
  return psramEnabled;
}

framesize_t CameraComp::allocationFrameSize()
{
  return allocatedMaximum;
}

uint8_t CameraComp::targetFps()
{
  return streamTargetFps;
}

bool CameraComp::captureMaximumJpeg(uint8_t *&data,
                                    size_t &length,
                                    uint16_t &width,
                                    uint16_t &height)
{
  data = nullptr;
  length = 0;
  width = 0;
  height = 0;

  Settings previous = {};
  if (!getSettings(previous) || !supportsFrameSize(allocatedMaximum))
  {
    return false;
  }

  Settings captureSettings = previous;
  captureSettings.frameSize = allocatedMaximum;
  captureSettings.fps = maxFpsForFrameSize(allocatedMaximum);

  if (!applySettings(captureSettings))
  {
    Serial.println("Maximum-resolution capture failed while changing resolution");
    return false;
  }

  camera_fb_t *frame = captureFrame();
  bool captured = frameMatches(frame, allocatedMaximum);
  if (captured)
  {
    data = static_cast<uint8_t *>(heap_caps_malloc(
        frame->len,
        psramEnabled ? MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT : MALLOC_CAP_8BIT));
    if (data == nullptr && psramEnabled)
    {
      data = static_cast<uint8_t *>(heap_caps_malloc(frame->len, MALLOC_CAP_8BIT));
    }
    if (data != nullptr)
    {
      memcpy(data, frame->buf, frame->len);
      length = frame->len;
      width = frame->width;
      height = frame->height;
    }
    else
    {
      Serial.printf("Snapshot allocation failed: %u bytes\n",
                    static_cast<unsigned int>(frame->len));
      captured = false;
    }
  }
  else
  {
    Serial.println("Maximum-resolution capture returned an invalid frame");
  }
  releaseFrame(frame);

  // Always restore the stream's previous resolution and controls. applySettings()
  // validates fresh frames and rebuilds the camera if the sensor rejects a change.
  bool restored = applySettings(previous);
  if (!restored)
  {
    // applySettings() rolls back to the maximum-resolution capture state when
    // restoration is rejected. Make one explicit reinitialization attempt with
    // the real pre-capture settings so a failed photo cannot leave the stream
    // running at the still-image resolution.
    if (cameraEnabled)
    {
      esp_camera_deinit();
      cameraEnabled = false;
    }
    delay(50);
    restored = initializeCamera(previous);
  }
  if (!restored)
  {
    Serial.println("Snapshot completed but previous camera settings could not be restored");
    releaseSnapshot(data);
    data = nullptr;
    length = 0;
    width = 0;
    height = 0;
    return false;
  }

  if (captured)
  {
    Serial.printf("Captured maximum-resolution JPEG: %ux%u, %u bytes\n",
                  width,
                  height,
                  static_cast<unsigned int>(length));
  }
  return captured;
}

void CameraComp::releaseSnapshot(uint8_t *data)
{
  if (data != nullptr)
  {
    heap_caps_free(data);
  }
}
