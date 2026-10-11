/**
 * @file CommandService.cpp
 * @brief 单槽覆盖式命令队列和 Controller HTTP 服务实现。
 */
#include "CommandService.h"

#include "BehaviorService.h"
#include "VideoService.h"
#include "../components/CameraComp.h"
#include "../components/WifiComp.h"
#include "../config/ControllerConfig.h"
#include "../config/GpioConfig.h"
#include "../web/ControllerPage.h"

#include <Arduino.h>
#include <WebServer.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace
{
WebServer controlServer(ControllerConfig::CONTROL_PORT);
QueueHandle_t commandQueue = nullptr;
TaskHandle_t commandTaskHandle = nullptr;
bool serverEnabled = false;

bool rejectControllerRequestInAccessPointMode()
{
  if (!WifiComp::isAccessPointMode())
  {
    return false;
  }
  controlServer.send(403,
                     "application/json",
                     "{\"ok\":false,\"error\":\"only Wi-Fi setup is available in access-point mode\"}");
  return true;
}

void handleWifiNetworksRequest()
{
  if (!WifiComp::isAccessPointMode())
  {
    controlServer.send(403,
                       "application/json",
                       "{\"ok\":false,\"error\":\"Wi-Fi setup is only available in access-point mode\"}");
    return;
  }

  String response;
  response.reserve(1024);
  if (!WifiComp::scanNetworks(response))
  {
    controlServer.send(500,
                       "application/json",
                       "{\"ok\":false,\"error\":\"Wi-Fi scan failed\"}");
    return;
  }

  controlServer.sendHeader("Cache-Control", "no-store");
  controlServer.send(200, "application/json", response);
}

void handleWifiConnectRequest()
{
  if (!WifiComp::isAccessPointMode())
  {
    controlServer.send(403,
                       "application/json",
                       "{\"ok\":false,\"error\":\"Wi-Fi setup is only available in access-point mode\"}");
    return;
  }
  if (!controlServer.hasArg("ssid"))
  {
    controlServer.send(400,
                       "application/json",
                       "{\"ok\":false,\"error\":\"ssid is required\"}");
    return;
  }

  const String ssid = controlServer.arg("ssid");
  const String password = controlServer.hasArg("password")
                              ? controlServer.arg("password")
                              : String();
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63)
  {
    controlServer.send(400,
                       "application/json",
                       "{\"ok\":false,\"error\":\"invalid SSID or password length\"}");
    return;
  }

  IPAddress stationAddress;
  if (!WifiComp::connectToExternalWifi(ssid, password, stationAddress))
  {
    controlServer.send(504,
                       "application/json",
                       "{\"ok\":false,\"error\":\"unable to join Wi-Fi; access point remains active\"}");
    return;
  }

  String response = "{\"ok\":true,\"ip\":\"";
  response += stationAddress.toString();
  response += "\",\"controlUrl\":\"http://";
  response += stationAddress.toString();
  response += "/\",\"message\":\"joined; access point will close\"}";
  controlServer.sendHeader("Cache-Control", "no-store");
  controlServer.send(200, "application/json", response);
}

void handleStreamStatsRequest()
{
  if (rejectControllerRequestInAccessPointMode())
  {
    return;
  }

  const VideoService::StreamStats stats = VideoService::stats();
  const uint64_t averageFrameBytes = stats.framesSent == 0
                                         ? 0
                                         : stats.jpegBytesSent / stats.framesSent;
  char response[256] = {};
  snprintf(response,
           sizeof(response),
           "{\"active\":%s,\"stream_id\":%lu,\"frames_sent\":%lu,"
           "\"jpeg_bytes_sent\":%llu,\"average_frame_bytes\":%llu,"
           "\"capture_time_ms\":%llu,\"send_time_ms\":%llu}",
           stats.active ? "true" : "false",
           static_cast<unsigned long>(stats.streamId),
           static_cast<unsigned long>(stats.framesSent),
           static_cast<unsigned long long>(stats.jpegBytesSent),
           static_cast<unsigned long long>(averageFrameBytes),
           static_cast<unsigned long long>(stats.captureTimeMs),
           static_cast<unsigned long long>(stats.sendTimeMs));
  controlServer.sendHeader("Cache-Control", "no-store");
  controlServer.send(200, "application/json", response);
}

bool parseSignedSetting(const char *name, int8_t &value)
{
  if (!controlServer.hasArg(name))
  {
    return true;
  }
  const String input = controlServer.arg(name);
  char *end = nullptr;
  const long parsed = strtol(input.c_str(), &end, 10);
  if (end == input.c_str() || *end != '\0' || parsed < -2 || parsed > 2)
  {
    return false;
  }
  value = static_cast<int8_t>(parsed);
  return true;
}

bool parseBooleanSetting(const char *name, bool &value)
{
  if (!controlServer.hasArg(name))
  {
    return true;
  }
  const String input = controlServer.arg(name);
  if (input == "1" || input == "true")
  {
    value = true;
    return true;
  }
  if (input == "0" || input == "false")
  {
    value = false;
    return true;
  }
  return false;
}

void sendCameraSettings()
{
  if (rejectControllerRequestInAccessPointMode())
  {
    return;
  }

  CameraComp::Settings settings = {};
  if (!CameraComp::getSettings(settings))
  {
    controlServer.send(503, "application/json", "{\"ok\":false,\"error\":\"camera unavailable\"}");
    return;
  }

  char pid[12] = {};
  snprintf(pid, sizeof(pid), "0x%04X", CameraComp::sensorPid());

  String response;
  response.reserve(1800);
  response += "{\"ok\":true,\"sensorModel\":\"";
  response += CameraComp::sensorModel();
  response += "\",\"sensorPid\":\"";
  response += pid;
  response += "\",\"psram\":{\"enabled\":";
  response += CameraComp::hasPsram() ? "true" : "false";
  response += ",\"totalBytes\":";
  response += static_cast<unsigned long>(ESP.getPsramSize());
  response += ",\"freeBytes\":";
  response += static_cast<unsigned long>(ESP.getFreePsram());
  response += "},\"allocationResolution\":\"";
  response += CameraComp::resolutionName(CameraComp::allocationFrameSize());
  response += "\",\"resolution\":\"";
  response += CameraComp::resolutionName(settings.frameSize);
  response += "\",\"fps\":";
  response += static_cast<unsigned int>(settings.fps);
  response += ",\"brightness\":";
  response += static_cast<int>(settings.brightness);
  response += ",\"contrast\":";
  response += static_cast<int>(settings.contrast);
  response += ",\"sharpness\":";
  response += static_cast<int>(settings.sharpness);
  response += ",\"hmirror\":";
  response += settings.hmirror ? "true" : "false";
  response += ",\"vflip\":";
  response += settings.vflip ? "true" : "false";
  response += ",\"streamGeneration\":";
  response += static_cast<unsigned long>(VideoService::generation());
  response += ",\"supportedResolutions\":[";

  const size_t resolutionCount = CameraComp::supportedResolutionCount();
  for (size_t index = 0; index < resolutionCount; ++index)
  {
    if (index > 0)
      response += ',';
    response += '\"';
    response += CameraComp::supportedResolutionAt(index);
    response += '\"';
  }

  response += "],\"maxFpsByResolution\":{";
  for (size_t index = 0; index < resolutionCount; ++index)
  {
    const char *resolutionName = CameraComp::supportedResolutionAt(index);
    framesize_t frameSize = FRAMESIZE_INVALID;
    CameraComp::parseResolution(resolutionName, frameSize);
    if (index > 0)
      response += ',';
    response += '\"';
    response += resolutionName;
    response += "\":";
    response += static_cast<unsigned int>(CameraComp::maxFpsForFrameSize(frameSize));
  }
  response += "},\"limits\":{\"fps\":[1,";
  response += static_cast<unsigned int>(CameraComp::maxFpsForFrameSize(settings.frameSize));
  response += "],\"brightness\":[-2,2],\"contrast\":[-2,2],\"sharpness\":[-2,2]}}";

  controlServer.sendHeader("Cache-Control", "no-store");
  controlServer.send(200, "application/json", response);
}

void handleCameraSettingsRequest()
{
  if (rejectControllerRequestInAccessPointMode())
  {
    return;
  }

  CameraComp::Settings settings = {};
  if (!CameraComp::getSettings(settings))
  {
    controlServer.send(503, "application/json", "{\"ok\":false,\"error\":\"camera unavailable\"}");
    return;
  }

  const char *settingNames[] = {
      "resolution", "fps", "brightness", "contrast", "sharpness", "hmirror", "vflip"};
  bool hasAnySetting = false;
  for (const char *name : settingNames)
  {
    hasAnySetting = hasAnySetting || controlServer.hasArg(name);
  }
  if (!hasAnySetting)
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"no camera settings supplied\"}");
    return;
  }

  const bool changesResolution = controlServer.hasArg("resolution");
  const bool suppliesFps = controlServer.hasArg("fps");
  if (changesResolution &&
      (!CameraComp::parseResolution(controlServer.arg("resolution").c_str(), settings.frameSize) ||
       !CameraComp::supportsFrameSize(settings.frameSize)))
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"unsupported resolution\"}");
    return;
  }

  if (suppliesFps)
  {
    const String input = controlServer.arg("fps");
    char *end = nullptr;
    const unsigned long parsed = strtoul(input.c_str(), &end, 10);
    if (end == input.c_str() || *end != '\0' || parsed < 1 || parsed > 120)
    {
      controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"fps must be between 1 and 120\"}");
      return;
    }
    settings.fps = static_cast<uint8_t>(parsed);
  }

  const uint8_t maximumFps = CameraComp::maxFpsForFrameSize(settings.frameSize);
  if (suppliesFps && settings.fps > maximumFps)
  {
    char response[112] = {};
    snprintf(response,
             sizeof(response),
             "{\"ok\":false,\"error\":\"fps exceeds resolution limit\",\"maxFps\":%u}",
             maximumFps);
    controlServer.send(400, "application/json", response);
    return;
  }
  if (!suppliesFps && settings.fps > maximumFps)
  {
    settings.fps = maximumFps;
  }

  if (!parseSignedSetting("brightness", settings.brightness) ||
      !parseSignedSetting("contrast", settings.contrast) ||
      !parseSignedSetting("sharpness", settings.sharpness) ||
      !parseBooleanSetting("hmirror", settings.hmirror) ||
      !parseBooleanSetting("vflip", settings.vflip))
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid camera setting\"}");
    return;
  }

  const bool videoWasEnabled = VideoService::isEnabled();
  bool streamNeedsRestart = false;
  if (videoWasEnabled && !VideoService::pauseForCameraChange())
  {
    VideoService::disable();
    streamNeedsRestart = true;
    delay(25);
  }

  const bool settingsApplied = CameraComp::applySettings(settings);
  bool videoAvailable = CameraComp::isEnabled();
  if (videoWasEnabled && videoAvailable)
  {
    if (streamNeedsRestart)
    {
      videoAvailable = VideoService::enable();
    }
    else
    {
      VideoService::resumeAfterCameraChange();
    }
  }
  if (!settingsApplied)
  {
    controlServer.send(500,
                       "application/json",
                       videoAvailable
                           ? "{\"ok\":false,\"error\":\"camera rejected settings; previous settings restored\"}"
                           : "{\"ok\":false,\"error\":\"camera rejected settings and video restart failed\"}");
    return;
  }
  if (!videoAvailable)
  {
    controlServer.send(503, "application/json", "{\"ok\":false,\"error\":\"settings applied but video restart failed\"}");
    return;
  }
  sendCameraSettings();
}

void handleCameraCaptureRequest()
{
  if (rejectControllerRequestInAccessPointMode())
  {
    return;
  }

  if (!CameraComp::isEnabled())
  {
    controlServer.send(503, "application/json", "{\"ok\":false,\"error\":\"camera unavailable\"}");
    return;
  }

  // Pause frame acquisition but keep the MJPEG socket alive. The browser then
  // continues on the same <img> request after the camera settings are restored.
  const bool videoWasEnabled = VideoService::isEnabled();
  bool streamNeedsRestart = false;
  if (videoWasEnabled && !VideoService::pauseForCameraChange())
  {
    // A broken client may hold a socket send until its timeout. Fall back to a
    // full restart rather than racing the camera while a frame is still owned.
    VideoService::disable();
    streamNeedsRestart = true;
    delay(25);
  }

  uint8_t *jpeg = nullptr;
  size_t jpegLength = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  const bool captured = CameraComp::captureMaximumJpeg(
      jpeg, jpegLength, width, height);
  bool videoAvailable = true;
  if (videoWasEnabled)
  {
    if (streamNeedsRestart)
    {
      videoAvailable = VideoService::enable();
    }
    else
    {
      VideoService::resumeAfterCameraChange();
    }
  }

  if (!captured)
  {
    controlServer.send(500,
                       "application/json",
                       videoAvailable
                           ? "{\"ok\":false,\"error\":\"maximum-resolution capture failed\"}"
                           : "{\"ok\":false,\"error\":\"capture failed and video restart failed\"}");
    return;
  }

  if (!videoAvailable)
  {
    CameraComp::releaseSnapshot(jpeg);
    controlServer.send(503,
                       "application/json",
                       "{\"ok\":false,\"error\":\"photo captured but video restart failed\"}");
    return;
  }

  char resolution[24] = {};
  snprintf(resolution, sizeof(resolution), "%ux%u", width, height);
  controlServer.sendHeader("Cache-Control", "no-store");
  controlServer.sendHeader("Content-Disposition", "inline; filename=controller-capture.jpg");
  controlServer.sendHeader("X-Camera-Resolution", resolution);
  controlServer.sendHeader("X-Stream-Reconnect-Required", streamNeedsRestart ? "true" : "false");
  controlServer.sendHeader("X-Stream-Generation", String(VideoService::generation()));
  controlServer.send_P(200,
                       PSTR("image/jpeg"),
                       reinterpret_cast<PGM_P>(jpeg),
                       jpegLength);
  CameraComp::releaseSnapshot(jpeg);
}

bool parseCommand(const String &name, Command &command)
{
  if (name == "stop")
    command = Command::Stop;
  else if (name == "forward")
    command = Command::Forward;
  else if (name == "back")
    command = Command::Back;
  else if (name == "left")
    command = Command::Left;
  else if (name == "right")
    command = Command::Right;
  else
    return false;
  return true;
}

uint32_t defaultDurationFor(Command command)
{
  return command == Command::Forward
             ? ControllerConfig::DEFAULT_FORWARD_DURATION_MS
             : ControllerConfig::DEFAULT_DURATION_MS;
}

bool parseDuration(Command command, uint32_t &durationMs)
{
  if (!controlServer.hasArg("durationMs"))
  {
    durationMs = defaultDurationFor(command);
    return true;
  }

  const String value = controlServer.arg("durationMs");
  char *end = nullptr;
  const unsigned long long parsed = strtoull(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0' ||
      parsed < ControllerConfig::MIN_DURATION_MS ||
      parsed > ControllerConfig::MAX_DURATION_MS)
  {
    return false;
  }

  durationMs = static_cast<uint32_t>(parsed);
  return true;
}

bool parsePhaseDuration(uint16_t &phaseDurationMs)
{
  if (!controlServer.hasArg("frequency"))
  {
    phaseDurationMs = ControllerConfig::DEFAULT_PHASE_DURATION_MS;
    return true;
  }

  const String value = controlServer.arg("frequency");
  char *end = nullptr;
  const float frequency = strtof(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0' || !isfinite(frequency) ||
      frequency < ControllerConfig::MIN_FREQUENCY_HZ ||
      frequency > ControllerConfig::MAX_FREQUENCY_HZ)
  {
    return false;
  }

  phaseDurationMs = static_cast<uint16_t>(lroundf(500.0F / frequency));
  if (phaseDurationMs == 0)
  {
    phaseDurationMs = 1;
  }
  return true;
}

bool parseChannelPins(uint8_t channelPins[GPIOConfig::CHANNEL_COUNT])
{
  static const char *ARG_NAMES[GPIOConfig::CHANNEL_COUNT] = {
      "chnl1", "chnl2", "chnl3", "chnl4"};

  bool hasAnyPin = false;
  for (uint8_t index = 0; index < GPIOConfig::CHANNEL_COUNT; ++index)
  {
    hasAnyPin = hasAnyPin || controlServer.hasArg(ARG_NAMES[index]);
  }

  for (uint8_t index = 0; index < GPIOConfig::CHANNEL_COUNT; ++index)
  {
    if (!hasAnyPin)
    {
      channelPins[index] = static_cast<uint8_t>(GPIOConfig::DEFAULT_CHANNEL_PINS[index]);
      continue;
    }
    if (!controlServer.hasArg(ARG_NAMES[index]))
    {
      return false;
    }

    const String value = controlServer.arg(ARG_NAMES[index]);
    char *end = nullptr;
    const unsigned long pinIndex = strtoul(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || pinIndex >= GPIOConfig::PIN_OPTION_COUNT)
    {
      return false;
    }

    const uint8_t pin = static_cast<uint8_t>(
        GPIOConfig::pinForIndex(static_cast<uint8_t>(pinIndex)));
    for (uint8_t previous = 0; previous < index; ++previous)
    {
      if (channelPins[previous] == pin)
      {
        return false;
      }
    }
    channelPins[index] = pin;
  }
  return true;
}

void executeCommand(CommandRequest request)
{
  if (request.command == Command::Stop)
  {
    BehaviorService::stop();
    return;
  }

  unsigned long startedAt = millis();
  bool reversePolarity = false;

  while (request.command != Command::Stop &&
         millis() - startedAt < request.durationMs)
  {
    if (!BehaviorService::applyPhase(request.command,
                                     request.channelPins,
                                     reversePolarity))
    {
      break;
    }
    reversePolarity = !reversePolarity;

    const uint32_t elapsedMs = millis() - startedAt;
    const uint32_t remainingMs = elapsedMs < request.durationMs
                                     ? request.durationMs - elapsedMs
                                     : 0;
    const uint32_t waitMs = min(static_cast<uint32_t>(request.phaseDurationMs),
                                remainingMs);

    CommandRequest replacement;
    if (xQueueReceive(commandQueue,
                      &replacement,
                      pdMS_TO_TICKS(waitMs)) == pdTRUE)
    {
      request = replacement;
      startedAt = millis();
      reversePolarity = false;
      continue;
    }
  }

  BehaviorService::stop();
}

void commandTask(void *)
{
  CommandRequest request;
  for (;;)
  {
    if (xQueueReceive(commandQueue, &request, portMAX_DELAY) == pdTRUE)
    {
      executeCommand(request);
    }
  }
}

bool ensureCommandExecutor()
{
  if (commandQueue != nullptr && commandTaskHandle != nullptr)
  {
    return true;
  }

  commandQueue = xQueueCreate(1, sizeof(CommandRequest));
  if (commandQueue == nullptr)
  {
    Serial.println("Failed to create command queue");
    return false;
  }

  if (xTaskCreate(commandTask,
                  "command-service",
                  ControllerConfig::COMMAND_TASK_STACK_SIZE,
                  nullptr,
                  ControllerConfig::COMMAND_TASK_PRIORITY,
                  &commandTaskHandle) != pdPASS)
  {
    vQueueDelete(commandQueue);
    commandQueue = nullptr;
    Serial.println("Failed to create command task");
    return false;
  }
  return true;
}

void handleCommandRequest()
{
  if (rejectControllerRequestInAccessPointMode())
  {
    return;
  }

  Command command;
  if (!controlServer.hasArg("name") ||
      !parseCommand(controlServer.arg("name"), command))
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid command\"}");
    return;
  }

  uint32_t durationMs = defaultDurationFor(command);
  uint16_t phaseDurationMs = ControllerConfig::DEFAULT_PHASE_DURATION_MS;
  uint8_t channelPins[GPIOConfig::CHANNEL_COUNT] = {};
  if (!parseChannelPins(channelPins))
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid or duplicate channel pins\"}");
    return;
  }

  if (command != Command::Stop &&
      (!parseDuration(command, durationMs) || !parsePhaseDuration(phaseDurationMs)))
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid waveform settings\"}");
    return;
  }

  CommandRequest request = {command, durationMs, phaseDurationMs, {}};
  for (uint8_t index = 0; index < GPIOConfig::CHANNEL_COUNT; ++index)
  {
    request.channelPins[index] = channelPins[index];
  }
  if (!CommandService::submit(request))
  {
    controlServer.send(503, "application/json", "{\"ok\":false,\"error\":\"command service unavailable\"}");
    return;
  }

  controlServer.sendHeader("Cache-Control", "no-store");
  char response[112];
  snprintf(response,
           sizeof(response),
           "{\"ok\":true,\"durationMs\":%lu,\"phaseDurationMs\":%u}",
           static_cast<unsigned long>(durationMs),
           phaseDurationMs);
  controlServer.send(200, "application/json", response);
}
} // namespace

bool CommandService::enable()
{
  if (serverEnabled)
  {
    return true;
  }

  controlServer.on("/", HTTP_GET, []()
                   {
                     if (!WifiComp::isAccessPointMode())
                     {
                       controlServer.send(404,
                                          "application/json",
                                          "{\"ok\":false,\"error\":\"Wi-Fi setup page is only available in access-point mode\"}");
                       return;
                     }
                     controlServer.sendHeader("Cache-Control", "no-store");
                     controlServer.send_P(200, "text/html; charset=utf-8", ControllerPage::HTML);
                   });
  controlServer.on("/config", HTTP_GET, []()
                   {
                     if (rejectControllerRequestInAccessPointMode())
                     {
                       return;
                     }
                     char response[512];
                     snprintf(response,
                              sizeof(response),
                              "{\"model\":\"XIAO ESP32S3 Sense\",\"networkMode\":\"%s\",\"controlPort\":%u,\"videoPort\":%u,\"durationMs\":%lu,\"forwardDurationMs\":%lu,\"defaultDurationsMs\":{\"forward\":%lu,\"back\":%lu,\"left\":%lu,\"right\":%lu},\"frequency\":%.1f,\"minDurationMs\":%lu,\"maxDurationMs\":%lu,\"minFrequency\":%.1f,\"maxFrequency\":%.1f,\"defaultPins\":[%u,%u,%u,%u]}",
                              WifiComp::isAccessPointMode() ? "access-point" : "station",
                              ControllerConfig::CONTROL_PORT,
                              ControllerConfig::VIDEO_PORT,
                              static_cast<unsigned long>(ControllerConfig::DEFAULT_DURATION_MS),
                              static_cast<unsigned long>(ControllerConfig::DEFAULT_FORWARD_DURATION_MS),
                              static_cast<unsigned long>(ControllerConfig::DEFAULT_FORWARD_DURATION_MS),
                              static_cast<unsigned long>(ControllerConfig::DEFAULT_DURATION_MS),
                              static_cast<unsigned long>(ControllerConfig::DEFAULT_DURATION_MS),
                              static_cast<unsigned long>(ControllerConfig::DEFAULT_DURATION_MS),
                              ControllerConfig::DEFAULT_FREQUENCY_HZ,
                              static_cast<unsigned long>(ControllerConfig::MIN_DURATION_MS),
                              static_cast<unsigned long>(ControllerConfig::MAX_DURATION_MS),
                              ControllerConfig::MIN_FREQUENCY_HZ,
                              ControllerConfig::MAX_FREQUENCY_HZ,
                              GPIOConfig::DEFAULT_CHANNEL_INDICES[0],
                              GPIOConfig::DEFAULT_CHANNEL_INDICES[1],
                              GPIOConfig::DEFAULT_CHANNEL_INDICES[2],
                              GPIOConfig::DEFAULT_CHANNEL_INDICES[3]);
                     controlServer.sendHeader("Cache-Control", "no-store");
                     controlServer.send(200, "application/json", response);
                   });
  controlServer.on("/command", HTTP_POST, handleCommandRequest);
  controlServer.on("/command", HTTP_OPTIONS, []()
                   {
                     if (!rejectControllerRequestInAccessPointMode())
                       controlServer.send(204);
                   });
  controlServer.on("/camera/settings", HTTP_GET, sendCameraSettings);
  controlServer.on("/camera/settings", HTTP_POST, handleCameraSettingsRequest);
  controlServer.on("/camera/settings", HTTP_OPTIONS, []()
                   {
                     if (!rejectControllerRequestInAccessPointMode())
                       controlServer.send(204);
                   });
  controlServer.on("/camera/capture", HTTP_GET, handleCameraCaptureRequest);
  controlServer.on("/camera/capture", HTTP_OPTIONS, []()
                   {
                     if (!rejectControllerRequestInAccessPointMode())
                       controlServer.send(204);
                   });
  controlServer.on("/stream/stats", HTTP_GET, handleStreamStatsRequest);
  controlServer.on("/stream/stats", HTTP_OPTIONS, []()
                   {
                     if (!rejectControllerRequestInAccessPointMode())
                       controlServer.send(204);
                   });
  controlServer.on("/wifi/networks", HTTP_GET, handleWifiNetworksRequest);
  controlServer.on("/wifi/networks", HTTP_OPTIONS, []()
                   { controlServer.send(204); });
  controlServer.on("/wifi/connect", HTTP_POST, handleWifiConnectRequest);
  controlServer.on("/wifi/connect", HTTP_OPTIONS, []()
                   { controlServer.send(204); });
  controlServer.onNotFound([]()
                           { controlServer.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}"); });
  controlServer.enableCORS(true);
  controlServer.begin();
  serverEnabled = true;

  Serial.printf("HTTP service started on port %u\n", ControllerConfig::CONTROL_PORT);
  return true;
}

void CommandService::update()
{
  if (serverEnabled)
  {
    controlServer.handleClient();
  }
}

bool CommandService::submit(const CommandRequest &request)
{
  return ensureCommandExecutor() &&
         xQueueOverwrite(commandQueue, &request) == pdPASS;
}
