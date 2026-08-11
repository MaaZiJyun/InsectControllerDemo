/**
 * @file Command.h
 * @brief Controller 支持的行为命令。
 */
#pragma once

#include <stdint.h>

enum class Command : uint8_t
{
  Stop,
  Forward,
  Back,
  Left,
  Right
};

/** @brief A command and the waveform settings captured when it was submitted. */
struct CommandRequest
{
  Command command;
  uint16_t cycleCount;
  uint16_t phaseDurationMs;
};
