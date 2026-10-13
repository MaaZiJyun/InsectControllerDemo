/**
 * @file StimulationService.cpp
 * @brief 通用差分双相刺激服务实现。
 */
#include "StimulationService.h"

#include "../config/ControllerConfig.h"
#include "../config/GpioConfig.h"

#include <Arduino.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace
{
struct StimulationRequest
{
  bool stop;
  uint8_t pin1;
  uint8_t pin2;
  uint16_t phaseDurationMs;
  uint32_t durationMs;
};

QueueHandle_t stimulationQueue = nullptr;
TaskHandle_t stimulationTaskHandle = nullptr;

void setAllHighImpedance()
{
  for (uint8_t index = 0; index < GPIOConfig::PIN_OPTION_COUNT; ++index)
  {
    pinMode(GPIOConfig::CONFIGURABLE_PINS[index], INPUT);
  }
}

void configurePair(uint8_t pin1, uint8_t pin2)
{
  setAllHighImpedance();
  pinMode(pin1, OUTPUT);
  pinMode(pin2, OUTPUT);
  digitalWrite(pin1, LOW);
  digitalWrite(pin2, LOW);
}

void writePhase(uint8_t pin1, uint8_t pin2, bool reversePolarity)
{
  // 第一相：pin1=LOW, pin2=HIGH；第二相反转。
  digitalWrite(pin1, reversePolarity ? HIGH : LOW);
  digitalWrite(pin2, reversePolarity ? LOW : HIGH);
}

void execute(StimulationRequest request)
{
  for (;;)
  {
    // 新命令开始前总是先断开上一对引脚。
    setAllHighImpedance();
    if (request.stop)
    {
      return;
    }

    configurePair(request.pin1, request.pin2);
    const uint32_t startedAt = millis();
    bool reversePolarity = false;
    bool replaced = false;

    while (millis() - startedAt < request.durationMs)
    {
      writePhase(request.pin1, request.pin2, reversePolarity);
      reversePolarity = !reversePolarity;

      const uint32_t elapsedMs = millis() - startedAt;
      const uint32_t remainingMs = elapsedMs < request.durationMs
                                       ? request.durationMs - elapsedMs
                                       : 0;
      const uint32_t waitMs = min(static_cast<uint32_t>(request.phaseDurationMs),
                                  remainingMs);

      StimulationRequest replacement;
      if (xQueueReceive(stimulationQueue,
                        &replacement,
                        pdMS_TO_TICKS(waitMs)) == pdTRUE)
      {
        request = replacement;
        replaced = true;
        break;
      }
    }

    if (!replaced)
    {
      setAllHighImpedance();
      return;
    }
  }
}

void stimulationTask(void *)
{
  StimulationRequest request;
  for (;;)
  {
    if (xQueueReceive(stimulationQueue, &request, portMAX_DELAY) == pdTRUE)
    {
      execute(request);
    }
  }
}

bool ensureExecutor()
{
  if (stimulationQueue != nullptr && stimulationTaskHandle != nullptr)
  {
    return true;
  }

  stimulationQueue = xQueueCreate(1, sizeof(StimulationRequest));
  if (stimulationQueue == nullptr)
  {
    Serial.println("Failed to create stimulation queue");
    return false;
  }

  if (xTaskCreate(stimulationTask,
                  "stimulation",
                  ControllerConfig::STIMULATION_TASK_STACK_SIZE,
                  nullptr,
                  ControllerConfig::STIMULATION_TASK_PRIORITY,
                  &stimulationTaskHandle) != pdPASS)
  {
    vQueueDelete(stimulationQueue);
    stimulationQueue = nullptr;
    Serial.println("Failed to create stimulation task");
    return false;
  }
  return true;
}
} // namespace

void StimulationService::enable()
{
  setAllHighImpedance();
}

bool StimulationService::startStimulation(uint8_t pin1,
                                          uint8_t pin2,
                                          float frequencyHz,
                                          uint32_t durationMs)
{
  if (pin1 >= GPIOConfig::PIN_OPTION_COUNT ||
      pin2 >= GPIOConfig::PIN_OPTION_COUNT ||
      pin1 == pin2 ||
      !isfinite(frequencyHz) ||
      frequencyHz < ControllerConfig::MIN_FREQUENCY_HZ ||
      frequencyHz > ControllerConfig::MAX_FREQUENCY_HZ ||
      durationMs < ControllerConfig::MIN_DURATION_MS)
  {
    return false;
  }

  uint16_t phaseDurationMs = static_cast<uint16_t>(lroundf(500.0F / frequencyHz));
  if (phaseDurationMs == 0)
  {
    phaseDurationMs = 1;
  }

  const StimulationRequest request = {
      false,
      static_cast<uint8_t>(GPIOConfig::pinForIndex(pin1)),
      static_cast<uint8_t>(GPIOConfig::pinForIndex(pin2)),
      phaseDurationMs,
      durationMs};
  return ensureExecutor() &&
         xQueueOverwrite(stimulationQueue, &request) == pdPASS;
}

bool StimulationService::stopStimulation()
{
  const StimulationRequest request = {true, 0, 0, 0, 0};
  return ensureExecutor() &&
         xQueueOverwrite(stimulationQueue, &request) == pdPASS;
}
