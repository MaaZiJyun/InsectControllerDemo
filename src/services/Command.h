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
