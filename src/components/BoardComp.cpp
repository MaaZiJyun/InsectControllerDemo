/**
 * @file BoardComp.cpp
 * @brief 板载电极通道与指示灯初始化组件实现。
 */
#include "BoardComp.h"

#include "../config/GpioConfig.h"

#include <Arduino.h>

void BoardComp::enable()
{
  pinMode(GPIOConfig::Chnl1, OUTPUT); // Left Backward
  pinMode(GPIOConfig::Chnl2, OUTPUT); // Left Forward
  pinMode(GPIOConfig::Chnl3, OUTPUT); // Right Forward
  pinMode(GPIOConfig::Chnl4, OUTPUT); // Right Backward
  pinMode(GPIOConfig::gpLed, OUTPUT); // Light

  // Initialize every output to a safe inactive state.
  digitalWrite(GPIOConfig::Chnl1, LOW);
  digitalWrite(GPIOConfig::Chnl2, LOW);
  digitalWrite(GPIOConfig::Chnl3, LOW);
  digitalWrite(GPIOConfig::Chnl4, LOW);
  digitalWrite(GPIOConfig::gpLed, LOW);
}
