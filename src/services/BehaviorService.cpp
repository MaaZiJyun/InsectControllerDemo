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
  Command configuredCommand = Command::Stop;
  uint8_t configuredPins[GPIOConfig::CHANNEL_COUNT] = {};

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

  void setAllHighImpedance()
  {
    for (uint8_t index = 0; index < GPIOConfig::PIN_OPTION_COUNT; ++index)
    {
      pinMode(GPIOConfig::CONFIGURABLE_PINS[index], INPUT);
    }
  }

  void setActiveOutput(uint8_t pin)
  {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  void writeSignal(uint8_t pin, uint16_t signalCode)
  {
    digitalWrite(pin,
                 signalCode == ControllerConfig::SIGNAL_LOW ? LOW : HIGH);
  }

  bool pinsMatch(const uint8_t channelPins[GPIOConfig::CHANNEL_COUNT])
  {
    for (uint8_t index = 0; index < GPIOConfig::CHANNEL_COUNT; ++index)
    {
      if (configuredPins[index] != channelPins[index])
      {
        return false;
      }
    }
    return true;
  }

  bool configureFor(Command command, const uint8_t channelPins[GPIOConfig::CHANNEL_COUNT])
  {
    const DrivePattern pattern = patternFor(command);
    if (!pattern.valid)
    {
      return false;
    }

    setAllHighImpedance();
    for (uint8_t index = 0; index < GPIOConfig::CHANNEL_COUNT; ++index)
    {
      configuredPins[index] = channelPins[index];
    }

    setActiveOutput(configuredPins[pattern.primary]);
    setActiveOutput(configuredPins[pattern.secondary]);

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
  setAllHighImpedance();
  configuredCommand = Command::Stop;
}

bool BehaviorService::applyPhase(Command command,
                                 const uint8_t channelPins[GPIOConfig::CHANNEL_COUNT],
                                 bool reversePolarity)
{
  if (command == Command::Stop)
  {
    stop();
    return true;
  }

  if ((configuredCommand != command || !pinsMatch(channelPins)) &&
      !configureFor(command, channelPins))
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

  writeSignal(configuredPins[pattern.primary], primaryValue);
  writeSignal(configuredPins[pattern.secondary], secondaryValue);
  return true;
}
