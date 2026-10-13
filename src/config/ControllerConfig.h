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
inline constexpr uint16_t DISCOVERY_PORT = 37020;

inline constexpr uint32_t MIN_DURATION_MS = 1;
inline constexpr uint32_t MAX_DURATION_MS = UINT32_MAX;
inline constexpr float MIN_FREQUENCY_HZ = 1.0F;
inline constexpr float MAX_FREQUENCY_HZ = 500.0F;

inline constexpr uint32_t STIMULATION_TASK_STACK_SIZE = 4096;
inline constexpr uint8_t STIMULATION_TASK_PRIORITY = 2;
}
