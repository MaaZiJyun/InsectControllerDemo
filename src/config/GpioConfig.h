/**
 * @file GpioConfig.h
 * @brief 电极通道与状态灯引脚配置。
 */
#pragma once

#include <Arduino.h>

// GPIO settings
namespace GPIOConfig
{
    inline constexpr uint8_t CHANNEL_COUNT = 4;
    inline constexpr uint8_t PIN_OPTION_COUNT = 11;
    inline constexpr uint8_t DEFAULT_CHANNEL_INDICES[CHANNEL_COUNT] = {1, 2, 3, 4};
    inline constexpr int DEFAULT_CHANNEL_PINS[CHANNEL_COUNT] = {D1, D2, D3, D4};
    inline constexpr int CONFIGURABLE_PINS[PIN_OPTION_COUNT] = {
        D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10};

    inline constexpr int pinForIndex(uint8_t index)
    {
        return index < PIN_OPTION_COUNT ? CONFIGURABLE_PINS[index] : -1;
    }
}
