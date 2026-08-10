/**
 * @file CommandService.cpp
 * @brief 单槽覆盖式命令队列和 Controller HTTP 服务实现。
 */
#include "CommandService.h"

#include "BehaviorService.h"
#include "../config/ControllerConfig.h"
#include "../web/ControllerPage.h"

#include <Arduino.h>
#include <WebServer.h>
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

void executeCommand(Command command)
{
  if (command == Command::Stop)
  {
    BehaviorService::stop();
    return;
  }

  unsigned long startedAt = millis();
  bool reversePolarity = false;
  uint16_t completedPhases = 0;

  while (command != Command::Stop &&
         completedPhases < ControllerConfig::PHASE_COUNT)
  {
    if (!BehaviorService::applyPhase(command, reversePolarity))
    {
      break;
    }
    reversePolarity = !reversePolarity;
    ++completedPhases;

    Command replacement;
    if (xQueueReceive(commandQueue,
                      &replacement,
                      pdMS_TO_TICKS(ControllerConfig::PHASE_DURATION_MS)) == pdTRUE)
    {
      command = replacement;
      startedAt = millis();
      reversePolarity = false;
      completedPhases = 0;
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
  Command command;
  for (;;)
  {
    if (xQueueReceive(commandQueue, &command, portMAX_DELAY) == pdTRUE)
    {
      executeCommand(command);
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

  if (!CommandService::submit(command))
  {
    controlServer.send(503, "application/json", "{\"ok\":false,\"error\":\"command service unavailable\"}");
    return;
  }

  controlServer.sendHeader("Cache-Control", "no-store");
  controlServer.send(200, "application/json", "{\"ok\":true}");
}
} // namespace

bool CommandService::enable()
{
  if (serverEnabled)
  {
    return true;
  }

  BehaviorService::enable();
  commandQueue = xQueueCreate(1, sizeof(Command));
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
                     char response[80];
                     snprintf(response,
                              sizeof(response),
                              "{\"controlPort\":%u,\"videoPort\":%u}",
                              ControllerConfig::CONTROL_PORT,
                              ControllerConfig::VIDEO_PORT);
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

bool CommandService::submit(Command command)
{
  return commandQueue != nullptr &&
         xQueueOverwrite(commandQueue, &command) == pdPASS;
}
