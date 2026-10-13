/**
 * @file BoardComp.cpp
 * @brief 板载刺激引脚安全状态初始化。
 */
#include "BoardComp.h"

#include "../config/GpioConfig.h"

#include <Arduino.h>

void BoardComp::enable()
{
  // Keep every user-selectable stimulation pin disconnected until requested.
  for (uint8_t index = 0; index < GPIOConfig::PIN_OPTION_COUNT; ++index)
  {
    pinMode(GPIOConfig::CONFIGURABLE_PINS[index], INPUT);
  }
}
