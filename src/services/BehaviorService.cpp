/**
 * @file BehaviorService.cpp
 * @brief 电极行为与双极性交替输出服务实现。
 */
#include "BehaviorService.h"

#include "../config/ControllerConfig.h"
#include "../config/GpioConfig.h"

#include <Arduino.h>

namespace
{
  constexpr uint8_t CHANNEL_COUNT = 4;
  constexpr int CHANNEL_PINS[CHANNEL_COUNT] = {
      GPIOConfig::Chnl1,
      GPIOConfig::Chnl2,
      GPIOConfig::Chnl3,
      GPIOConfig::Chnl4};

  Command configuredCommand = Command::Stop;

  struct DrivePattern
  {
    uint8_t primary;
    uint8_t secondary;
    bool valid;
  };

  DrivePattern patternFor(Command command)
  {
    switch (command)
    {
    case Command::Forward:
      return {0, 3, true}; // Ch1 <-> Ch4; Ch2/Ch3 high impedance
    case Command::Back:
      return {0, 3, true}; // Ch1 <-> Ch4; Ch2/Ch3 high impedance
    case Command::Left:
      return {0, 2, true}; // Ch1 <-> Ch3; Ch2/Ch4 high impedance
    case Command::Right:
      return {0, 1, true}; // Ch1 <-> Ch2; Ch3/Ch4 high impedance
    case Command::Stop:
    default:
      return {0, 0, false};
    }
  }

  void setHighImpedance(uint8_t index)
  {
    pinMode(CHANNEL_PINS[index], INPUT);
  }

  void setActiveOutput(uint8_t index)
  {
    pinMode(CHANNEL_PINS[index], OUTPUT);
    digitalWrite(CHANNEL_PINS[index], LOW);
  }

  void writeSignal(uint8_t index, uint16_t signalCode)
  {
    digitalWrite(CHANNEL_PINS[index],
                 signalCode == ControllerConfig::SIGNAL_LOW ? LOW : HIGH);
  }

  bool configureFor(Command command)
  {
    const DrivePattern pattern = patternFor(command);
    if (!pattern.valid)
    {
      return false;
    }

    // Disconnect every channel before selecting a new electrode pattern.
    for (uint8_t index = 0; index < CHANNEL_COUNT; ++index)
    {
      setHighImpedance(index);
    }

    setActiveOutput(pattern.primary);
    setActiveOutput(pattern.secondary);

    configuredCommand = command;
    return true;
  }
} // namespace

void BehaviorService::enable()
{
  stop();
}

void BehaviorService::stop()
{
  for (uint8_t index = 0; index < CHANNEL_COUNT; ++index)
  {
    setHighImpedance(index);
  }
  configuredCommand = Command::Stop;
}

bool BehaviorService::applyPhase(Command command, bool reversePolarity)
{
  if (command == Command::Stop)
  {
    stop();
    return true;
  }

  if (configuredCommand != command && !configureFor(command))
  {
    stop();
    return false;
  }

  const DrivePattern pattern = patternFor(command);
  // Phase 1: primary=0, secondary=512. Phase 2 reverses the polarity.
  const uint16_t primaryValue = reversePolarity
                                    ? ControllerConfig::SIGNAL_HIGH
                                    : ControllerConfig::SIGNAL_LOW;
  const uint16_t secondaryValue = reversePolarity
                                      ? ControllerConfig::SIGNAL_LOW
                                      : ControllerConfig::SIGNAL_HIGH;

  writeSignal(pattern.primary, primaryValue);
  writeSignal(pattern.secondary, secondaryValue);
  return true;
}
