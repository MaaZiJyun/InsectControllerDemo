/**
 * @file BehaviorService.h
 * @brief 电极行为与双极性交替输出服务。
 */
#pragma once

#include "Command.h"

namespace BehaviorService
{
/** @brief 初始化行为输出，并将四个通道置 LOW。 */
void enable();

/** @brief 将四个通道全部设为低电平输出。 */
void stop();

/** @brief 输出指定行为的一个极性相位。 */
bool applyPhase(Command command, bool reversePolarity);
}
