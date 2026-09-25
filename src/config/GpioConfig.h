/**
 * @file GpioConfig.h
 * @brief 电极通道与状态灯引脚配置。
 */
#pragma once

#include <Arduino.h>

// GPIO settings
namespace GPIOConfig
{
    static constexpr int Chnl1 = D7; // Channel 1: grounding cable
    static constexpr int Chnl2 = D8; // Channel 2: left antenna
    static constexpr int Chnl3 = D9; // Channel 3: right antenna
    static constexpr int Chnl4 = D10; // Channel 4: cerci
    static constexpr int gpLed = D1; // Indicator light
}