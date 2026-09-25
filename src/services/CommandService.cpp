/**
 * @file CommandService.cpp
 * @brief 单槽覆盖式命令队列和 Controller HTTP 服务实现。
 */
#include "CommandService.h"

#include "BehaviorService.h"
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

bool parseCycleCount(uint16_t &cycleCount)
{
  if (!controlServer.hasArg("cycleCount"))
  {
    cycleCount = ControllerConfig::DEFAULT_CYCLE_COUNT;
    return true;
  }

  const String value = controlServer.arg("cycleCount");
  char *end = nullptr;
  const unsigned long parsed = strtoul(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0' ||
      parsed < ControllerConfig::MIN_CYCLE_COUNT ||
      parsed > ControllerConfig::MAX_CYCLE_COUNT)
  {
    return false;
  }

  cycleCount = static_cast<uint16_t>(parsed);
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
  uint32_t completedPhases = 0;
  uint32_t phaseCount = static_cast<uint32_t>(request.cycleCount) * 2U;

  while (request.command != Command::Stop && completedPhases < phaseCount)
  {
    if (!BehaviorService::applyPhase(request.command,
                                     request.channelPins,
                                     reversePolarity))
    {
      break;
    }
    reversePolarity = !reversePolarity;
    ++completedPhases;

    CommandRequest replacement;
    if (xQueueReceive(commandQueue,
                      &replacement,
                      pdMS_TO_TICKS(request.phaseDurationMs)) == pdTRUE)
    {
      request = replacement;
      startedAt = millis();
      reversePolarity = false;
      completedPhases = 0;
      phaseCount = static_cast<uint32_t>(request.cycleCount) * 2U;
      continue;
    }

    if (millis() - startedAt >= ControllerConfig::COMMAND_TIMEOUT_MS)
    {
      break;
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

void handleCommandRequest()
{
  Command command;
  if (!controlServer.hasArg("name") ||
      !parseCommand(controlServer.arg("name"), command))
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid command\"}");
    return;
  }

  uint16_t cycleCount = ControllerConfig::DEFAULT_CYCLE_COUNT;
  uint16_t phaseDurationMs = ControllerConfig::DEFAULT_PHASE_DURATION_MS;
  uint8_t channelPins[GPIOConfig::CHANNEL_COUNT] = {};
  if (!parseChannelPins(channelPins))
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid or duplicate channel pins\"}");
    return;
  }

  if (command != Command::Stop &&
      (!parseCycleCount(cycleCount) || !parsePhaseDuration(phaseDurationMs)))
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid waveform settings\"}");
    return;
  }

  const uint32_t durationMs =
      static_cast<uint32_t>(cycleCount) * 2U * phaseDurationMs;
  if (command != Command::Stop &&
      durationMs > ControllerConfig::COMMAND_TIMEOUT_MS)
  {
    controlServer.send(400, "application/json", "{\"ok\":false,\"error\":\"waveform exceeds 500 ms safety timeout\"}");
    return;
  }

  CommandRequest request = {command, cycleCount, phaseDurationMs, {}};
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
  char response[96];
  snprintf(response,
           sizeof(response),
           "{\"ok\":true,\"cycleCount\":%u,\"phaseDurationMs\":%u}",
           cycleCount,
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

  BehaviorService::enable();
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

  controlServer.on("/", HTTP_GET, []()
                   {
                     controlServer.sendHeader("Cache-Control", "no-store");
                     controlServer.send_P(200, "text/html; charset=utf-8", ControllerPage::HTML);
                   });
  controlServer.on("/config", HTTP_GET, []()
                   {
                     char response[320];
                     snprintf(response,
                              sizeof(response),
                              "{\"controlPort\":%u,\"videoPort\":%u,\"cycleCount\":%u,\"frequency\":%.1f,\"minCycleCount\":%u,\"maxCycleCount\":%u,\"minFrequency\":%.1f,\"maxFrequency\":%.1f,\"maxDurationMs\":%lu,\"defaultPins\":[%u,%u,%u,%u]}",
                              ControllerConfig::CONTROL_PORT,
                              ControllerConfig::VIDEO_PORT,
                              ControllerConfig::DEFAULT_CYCLE_COUNT,
                              ControllerConfig::DEFAULT_FREQUENCY_HZ,
                              ControllerConfig::MIN_CYCLE_COUNT,
                              ControllerConfig::MAX_CYCLE_COUNT,
                              ControllerConfig::MIN_FREQUENCY_HZ,
                              ControllerConfig::MAX_FREQUENCY_HZ,
                              static_cast<unsigned long>(ControllerConfig::COMMAND_TIMEOUT_MS),
                              GPIOConfig::DEFAULT_CHANNEL_INDICES[0],
                              GPIOConfig::DEFAULT_CHANNEL_INDICES[1],
                              GPIOConfig::DEFAULT_CHANNEL_INDICES[2],
                              GPIOConfig::DEFAULT_CHANNEL_INDICES[3]);
                     controlServer.sendHeader("Cache-Control", "no-store");
                     controlServer.send(200, "application/json", response);
                   });
  controlServer.on("/command", HTTP_POST, handleCommandRequest);
  controlServer.onNotFound([]()
                           { controlServer.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}"); });
  controlServer.begin();
  serverEnabled = true;

  Serial.printf("Controller started on port %u\n", ControllerConfig::CONTROL_PORT);
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
  return commandQueue != nullptr &&
         xQueueOverwrite(commandQueue, &request) == pdPASS;
}
