/**
 * @file CommandService.cpp
 * @brief 单槽覆盖式命令队列和 Controller HTTP 服务实现。
 */
#include "CommandService.h"

#include "StimulationService.h"
#include "VideoService.h"
#include "../components/CameraComp.h"
#include "../components/WifiComp.h"
#include "../config/ControllerConfig.h"
#include "../config/GpioConfig.h"
#include "../web/ControllerPage.h"

#include <Arduino.h>
#include <WebServer.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

namespace
{
WebServer controlServer(ControllerConfig::CONTROL_PORT);
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

bool extractJsonNumber(const String &json, const char *name, String &value)
{
  const String marker = String('"') + name + '"';
  const int keyAt = json.indexOf(marker);
  if (keyAt < 0 || json.indexOf(marker, keyAt + marker.length()) >= 0)
  {
    return false;
  }

  int cursor = keyAt + marker.length();
  while (cursor < json.length() && isspace(static_cast<unsigned char>(json[cursor])))
    ++cursor;
  if (cursor >= json.length() || json[cursor++] != ':')
    return false;
  while (cursor < json.length() && isspace(static_cast<unsigned char>(json[cursor])))
    ++cursor;

  const int numberAt = cursor;
  while (cursor < json.length())
  {
    const char character = json[cursor];
    if (!(isdigit(static_cast<unsigned char>(character)) ||
          character == '+' || character == '-' || character == '.' ||
          character == 'e' || character == 'E'))
    {
      break;
    }
    ++cursor;
  }
  if (cursor == numberAt)
    return false;

  value = json.substring(numberAt, cursor);
  return true;
}

bool parseUnsigned(const String &input, uint32_t maximum, uint32_t &value)
{
  if (input.isEmpty() || input[0] == '-' || input[0] == '+')
    return false;

  errno = 0;
  char *end = nullptr;
  const unsigned long long parsed = strtoull(input.c_str(), &end, 10);
  if (errno == ERANGE || end == input.c_str() || *end != '\0' || parsed > maximum)
    return false;

  value = static_cast<uint32_t>(parsed);
  return true;
}

bool parseFrequency(const String &input, float &value)
{
  errno = 0;
  char *end = nullptr;
  const float parsed = strtof(input.c_str(), &end);
  if (errno == ERANGE || end == input.c_str() || *end != '\0' || !isfinite(parsed))
    return false;
  value = parsed;
  return true;
}

void handleStimulationStartRequest()
{
  if (rejectControllerRequestInAccessPointMode())
    return;

  String json = controlServer.arg("plain");
  json.trim();
  String pin1Input;
  String pin2Input;
  String frequencyInput;
  String durationInput;
  if (json.length() < 2 || json[0] != '{' || json[json.length() - 1] != '}' ||
      !extractJsonNumber(json, "pin1", pin1Input) ||
      !extractJsonNumber(json, "pin2", pin2Input) ||
      !extractJsonNumber(json, "frequencyHz", frequencyInput) ||
      !extractJsonNumber(json, "durationMs", durationInput))
  {
    controlServer.send(400, "application/json",
                       "{\"ok\":false,\"error\":\"pin1, pin2, frequencyHz and durationMs are required JSON numbers\"}");
    return;
  }

  uint32_t pin1 = 0;
  uint32_t pin2 = 0;
  uint32_t durationMs = 0;
  float frequencyHz = 0.0F;
  if (!parseUnsigned(pin1Input, GPIOConfig::PIN_OPTION_COUNT - 1, pin1) ||
      !parseUnsigned(pin2Input, GPIOConfig::PIN_OPTION_COUNT - 1, pin2) ||
      pin1 == pin2 ||
      !parseUnsigned(durationInput, ControllerConfig::MAX_DURATION_MS, durationMs) ||
      durationMs < ControllerConfig::MIN_DURATION_MS ||
      !parseFrequency(frequencyInput, frequencyHz) ||
      frequencyHz < ControllerConfig::MIN_FREQUENCY_HZ ||
      frequencyHz > ControllerConfig::MAX_FREQUENCY_HZ)
  {
    controlServer.send(400, "application/json",
                       "{\"ok\":false,\"error\":\"invalid stimulation parameters\"}");
    return;
  }

  if (!StimulationService::startStimulation(static_cast<uint8_t>(pin1),
                                             static_cast<uint8_t>(pin2),
                                             frequencyHz,
                                             durationMs))
  {
    controlServer.send(503, "application/json",
                       "{\"ok\":false,\"error\":\"stimulation service unavailable\"}");
    return;
  }

  controlServer.sendHeader("Cache-Control", "no-store");
  char response[160] = {};
  snprintf(response,
           sizeof(response),
           "{\"ok\":true,\"pin1\":%lu,\"pin2\":%lu,\"frequencyHz\":%.3f,\"durationMs\":%lu}",
           static_cast<unsigned long>(pin1),
           static_cast<unsigned long>(pin2),
           frequencyHz,
           static_cast<unsigned long>(durationMs));
  controlServer.send(202, "application/json", response);
}

void handleStimulationStopRequest()
{
  if (rejectControllerRequestInAccessPointMode())
    return;

  if (!StimulationService::stopStimulation())
  {
    controlServer.send(503, "application/json",
                       "{\"ok\":false,\"error\":\"stimulation service unavailable\"}");
    return;
  }
  controlServer.sendHeader("Cache-Control", "no-store");
  controlServer.send(202, "application/json", "{\"ok\":true}");
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
                     char response[384];
                     snprintf(response,
                              sizeof(response),
                              "{\"model\":\"XIAO ESP32S3 Sense\",\"networkMode\":\"%s\",\"controlPort\":%u,\"videoPort\":%u,\"stimulation\":{\"startPath\":\"/stimulation/start\",\"stopPath\":\"/stimulation/stop\",\"supportedPins\":[0,1,2,3,4,5,6,7,8,9,10],\"minDurationMs\":%lu,\"maxDurationMs\":%lu,\"minFrequencyHz\":%.1f,\"maxFrequencyHz\":%.1f}}",
                              WifiComp::isAccessPointMode() ? "access-point" : "station",
                              ControllerConfig::CONTROL_PORT,
                              ControllerConfig::VIDEO_PORT,
                              static_cast<unsigned long>(ControllerConfig::MIN_DURATION_MS),
                              static_cast<unsigned long>(ControllerConfig::MAX_DURATION_MS),
                              ControllerConfig::MIN_FREQUENCY_HZ,
                              ControllerConfig::MAX_FREQUENCY_HZ);
                     controlServer.sendHeader("Cache-Control", "no-store");
                     controlServer.send(200, "application/json", response);
                   });
  controlServer.on("/stimulation/start", HTTP_POST, handleStimulationStartRequest);
  controlServer.on("/stimulation/start", HTTP_OPTIONS, []()
                   {
                     if (!rejectControllerRequestInAccessPointMode())
                       controlServer.send(204);
                   });
  controlServer.on("/stimulation/stop", HTTP_POST, handleStimulationStopRequest);
  controlServer.on("/stimulation/stop", HTTP_OPTIONS, []()
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
