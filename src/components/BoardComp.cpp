/**
 * @file BoardComp.cpp
 * @brief 板载电极通道与指示灯初始化组件实现。
 */
#include "BoardComp.h"

#include "../config/GpioConfig.h"

#include <Arduino.h>

void BoardComp::enable()
{
  // Keep every behavior channel electrically disconnected until commanded.
  pinMode(GPIOConfig::Chnl1, INPUT);
  pinMode(GPIOConfig::Chnl2, INPUT);
  pinMode(GPIOConfig::Chnl3, INPUT);
  pinMode(GPIOConfig::Chnl4, INPUT);
  pinMode(GPIOConfig::gpLed, OUTPUT); // Light

  digitalWrite(GPIOConfig::gpLed, LOW);
}
