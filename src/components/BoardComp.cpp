/**
 * @file BoardComp.cpp
 * @brief 板载电极通道与指示灯初始化组件实现。
 */
#include "BoardComp.h"

#include "../config/GpioConfig.h"

#include <Arduino.h>

void BoardComp::enable()
{
  // Keep every user-selectable behavior pin disconnected until commanded.
  for (uint8_t index = 0; index < GPIOConfig::PIN_OPTION_COUNT; ++index)
  {
    pinMode(GPIOConfig::CONFIGURABLE_PINS[index], INPUT);
  }
}
