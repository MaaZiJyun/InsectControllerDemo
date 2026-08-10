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

bool pwmAttached[CHANNEL_COUNT] = {false, false, false, false};
Command configuredCommand = Command::Stop;

struct ChannelPair
{
  uint8_t first;
  uint8_t second;
  bool valid;
};

ChannelPair pairFor(Command command)
{
  switch (command)
  {
  case Command::Forward:
    return {0, 3, true}; // Channels 1 and 4
  case Command::Back:
    return {0, 3, true}; // Channels 1 and 4
  case Command::Left:
    return {0, 2, true}; // Channels 1 and 3, 
  case Command::Right:
    return {0, 1, true}; // Channels 1 and 2
  case Command::Stop:
  default:
    return {0, 0, false};
  }
}

void detachPwm(uint8_t index)
{
  if (pwmAttached[index])
  {
    ledcWrite(CHANNEL_PINS[index], ControllerConfig::SIGNAL_LOW);
    ledcDetach(CHANNEL_PINS[index]);
    pwmAttached[index] = false;
  }
}

void setLowOutput(uint8_t index)
{
  detachPwm(index);
  pinMode(CHANNEL_PINS[index], OUTPUT);
  digitalWrite(CHANNEL_PINS[index], LOW);
}

void setHighImpedance(uint8_t index)
{
  detachPwm(index);
  pinMode(CHANNEL_PINS[index], INPUT);
}

bool attachPwm(uint8_t index)
{
  if (pwmAttached[index])
  {
    return true;
  }

  pwmAttached[index] = ledcAttachChannel(
      CHANNEL_PINS[index],
      ControllerConfig::PWM_FREQUENCY_HZ,
      ControllerConfig::PWM_RESOLUTION_BITS,
      ControllerConfig::PWM_CHANNELS[index]);

  if (!pwmAttached[index])
  {
    Serial.printf("Failed to attach PWM to behavior channel %u\n", index + 1);
  }
  return pwmAttached[index];
}

bool configureFor(Command command)
{
  const ChannelPair pair = pairFor(command);
  if (!pair.valid)
  {
    return false;
  }

  // Remove the previous drive state before selecting a new electrode pair.
  for (uint8_t index = 0; index < CHANNEL_COUNT; ++index)
  {
    setLowOutput(index);
  }

  for (uint8_t index = 0; index < CHANNEL_COUNT; ++index)
  {
    if (index != pair.first && index != pair.second)
    {
      setHighImpedance(index);
    }
  }

  if (!attachPwm(pair.first) || !attachPwm(pair.second))
  {
    return false;
  }

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
    setLowOutput(index);
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

  const ChannelPair pair = pairFor(command);
  const uint16_t firstValue = reversePolarity
                                  ? ControllerConfig::SIGNAL_LOW
                                  : ControllerConfig::SIGNAL_HIGH;
  const uint16_t secondValue = reversePolarity
                                   ? ControllerConfig::SIGNAL_HIGH
                                   : ControllerConfig::SIGNAL_LOW;

  ledcWrite(CHANNEL_PINS[pair.first], firstValue);
  ledcWrite(CHANNEL_PINS[pair.second], secondValue);
  return true;
}
