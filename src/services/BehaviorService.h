/**
 * @file BehaviorService.h
 * @brief 电极行为与双极性交替输出服务。
 */
#pragma once

#include "Command.h"

namespace BehaviorService
{
/** @brief 初始化行为服务，并将四个通道置为 INPUT 高阻。 */
void enable();

/** @brief 停止输出，并将四个通道全部设为 INPUT 高阻。 */
void stop();

/** @brief 输出指定行为的一个极性相位。 */
bool applyPhase(Command command, const uint8_t channelPins[4], bool reversePolarity);
}
