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

/// 单个相位的持续时间，单位为毫秒。
inline constexpr uint16_t PHASE_DURATION_MS = 10;
/// 每次刺激命令执行的完整双相周期数。
inline constexpr uint8_t CYCLE_COUNT = 20;
inline constexpr uint16_t PHASE_COUNT = CYCLE_COUNT * 2;

static_assert(PHASE_DURATION_MS * PHASE_COUNT <= COMMAND_TIMEOUT_MS,
              "Stimulus cycle sequence must fit inside the command timeout");

// Logical signal codes. SIGNAL_HIGH maps to a stable 3.3 V GPIO HIGH,
// not to a PWM duty value.
inline constexpr uint16_t SIGNAL_LOW = 0;
inline constexpr uint16_t SIGNAL_HIGH = 512;

inline constexpr uint32_t COMMAND_TASK_STACK_SIZE = 4096;
inline constexpr uint8_t COMMAND_TASK_PRIORITY = 2;
}
