/**
 * @file ControllerConfig.h
 * @brief Web controller、命令超时和刺激波形参数。
 */
#pragma once

#include <stdint.h>

namespace ControllerConfig
{
inline constexpr uint16_t CONTROL_PORT = 80;
inline constexpr uint16_t VIDEO_PORT = 81;

inline constexpr uint32_t COMMAND_TIMEOUT_MS = 500;
inline constexpr uint32_t SIGNAL_HALF_PERIOD_MS = 10;

inline constexpr uint32_t PWM_FREQUENCY_HZ = 1000;
inline constexpr uint8_t PWM_RESOLUTION_BITS = 10;
inline constexpr uint16_t SIGNAL_LOW = 0;
inline constexpr uint16_t SIGNAL_HIGH = 512;

// Camera XCLK occupies LEDC channel 0. Keep behavior outputs on channels 2-5.
inline constexpr uint8_t PWM_CHANNELS[4] = {2, 3, 4, 5};

inline constexpr uint32_t COMMAND_TASK_STACK_SIZE = 4096;
inline constexpr uint8_t COMMAND_TASK_PRIORITY = 2;
}
